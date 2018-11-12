/*
 * Copyright (c) 2015-2018, Fabian Klötzl <fabian-pfasta@kloetzl.info>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <emmintrin.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <immintrin.h>
#include <inttypes.h>
#include <malloc.h>
#include <nmmintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>
#include <unistd.h>

void *reallocarray(void *ptr, size_t nmemb, size_t size);

#define PF_ERROR_STRING_LENGTH 100
#define BUFFER_SIZE 16384

#define LIKELY(X) __builtin_expect((intptr_t)(X), 1)
#define UNLIKELY(X) __builtin_expect((intptr_t)(X), 0)

struct pfasta_record {
	char *name, *comment, *sequence;
	size_t name_length, comment_length, sequence_length;
	const char *errstr;
};

struct pfasta_parser {
	int file_descriptor;
	int done;
	char *buffer;
	char *read_ptr, *fill_ptr;
	size_t line_number;
	const char *errstr;
};

typedef struct dynstr {
	char *str;
	size_t capacity, count;
} dynstr;

thread_local char errstr_buffer[PF_ERROR_STRING_LENGTH];

#define PF_FAIL_ERRNO(PP)                                                      \
	do {                                                                       \
		strerror_r(errno, errstr_buffer, PF_ERROR_STRING_LENGTH);              \
		(PP)->errstr = errstr_buffer;                                          \
		return_code = -2;                                                      \
		goto cleanup;                                                          \
	} while (0)

#define PF_FAIL_BUBBLE(PP)                                                     \
	do {                                                                       \
		if (UNLIKELY((PP)->errstr)) {                                          \
			return_code = -3;                                                  \
			goto cleanup;                                                      \
		}                                                                      \
	} while (0)

#define PF_FAIL_STR_CONST(PP, STR)                                             \
	do {                                                                       \
		(PP)->errstr = (STR);                                                  \
		return_code = -1;                                                      \
		goto cleanup;                                                          \
	} while (0)

#define PF_FAIL_STR(PP, ...)                                                   \
	do {                                                                       \
		(void)snprintf(errstr_buffer, PF_ERROR_STRING_LENGTH, __VA_ARGS__);    \
		(PP)->errstr = errstr_buffer;                                          \
		return_code = -1;                                                      \
		goto cleanup;                                                          \
	} while (0)

int pfasta_read_name(struct pfasta_parser *pp, struct pfasta_record *pr);
int pfasta_read_comment(struct pfasta_parser *pp, struct pfasta_record *pr);
int pfasta_read_sequence(struct pfasta_parser *pp, struct pfasta_record *pr);
void pfasta_record_free(struct pfasta_record *pr);
void pfasta_free(struct pfasta_parser *pp);

char *buffer_begin(struct pfasta_parser *pp);
int buffer_advance(struct pfasta_parser *pp, size_t steps);
char *buffer_end(struct pfasta_parser *pp);
int buffer_is_eof(const struct pfasta_parser *pp);
int buffer_is_empty(const struct pfasta_parser *pp);
int buffer_read(struct pfasta_parser *pp);

char *find_if_is_graph(const char *begin, const char *end);
char *find_if_is_not_graph(const char *begin, const char *end);

static inline int dynstr_init(dynstr *ds, struct pfasta_parser *pp);
static inline void dynstr_free(dynstr *ds);
static inline char *dynstr_move(dynstr *ds);
static inline size_t dynstr_len(const dynstr *ds);
static inline int dynstr_append(dynstr *ds, const char *str, size_t length,
                                struct pfasta_parser *pp);

int my_isgraph(unsigned char c) { return (c >= '!') && (c <= '~'); }
int my_isspace(unsigned char c) {
	return (c >= '\t' && c <= '\r') || (c == ' ');
}

int buffer_init(struct pfasta_parser *pp) {
	int return_code = 0;

	pp->buffer = malloc(BUFFER_SIZE);
	if (!pp->buffer) PF_FAIL_ERRNO(pp);

	buffer_read(pp);
	PF_FAIL_BUBBLE(pp);

cleanup:
	return return_code;
}

int buffer_read(struct pfasta_parser *pp) {
	int return_code = 0;
	ssize_t count = read(pp->file_descriptor, pp->buffer, BUFFER_SIZE);

	if (UNLIKELY(count < 0)) PF_FAIL_ERRNO(pp);
	if (UNLIKELY(count == 0)) { // EOF
		pp->fill_ptr = pp->buffer;
		pp->read_ptr = pp->buffer + 1;
		return 1;
	}

	pp->read_ptr = pp->buffer;
	pp->fill_ptr = pp->buffer + count;

cleanup:
	return return_code;
}

int buffer_peek(struct pfasta_parser *pp) {
	return LIKELY(pp->read_ptr < pp->fill_ptr) ? *pp->read_ptr : EOF;
}

char *buffer_begin(struct pfasta_parser *pp) { return pp->read_ptr; }

char *buffer_end(struct pfasta_parser *pp) { return pp->fill_ptr; }

inline int buffer_advance(struct pfasta_parser *pp, size_t steps) {
	int return_code = 0;

	pp->read_ptr += steps;
	if (UNLIKELY(pp->read_ptr >= pp->fill_ptr)) {
		assert(pp->read_ptr == pp->fill_ptr);
		int check = buffer_read(pp); // resets pointers
		if (UNLIKELY(check)) PF_FAIL_BUBBLE(pp);
	}

cleanup:
	return return_code;
}

int buffer_is_empty(const struct pfasta_parser *pp) {
	return pp->read_ptr == pp->fill_ptr;
}

int buffer_is_eof(const struct pfasta_parser *pp) {
	return pp->read_ptr > pp->fill_ptr;
}

char *find_if_is_graph(const char *begin, const char *end) {
	size_t offset = 0;
	size_t length = end - begin;

	for (; offset < length; offset++) {
		if (LIKELY(my_isgraph(begin[offset]))) break;
	}
	return (char *)begin + offset;
}

size_t count_newlines(const char *begin, const char *end) {
	size_t offset = 0;
	size_t length = end - begin;
	size_t newlines = 0;

	for (; offset < length; offset++) {
		if (begin[offset] == '\n') newlines++;
	}

	return newlines;
}

__attribute__((target_clones("avx2", "avx", "sse2", "default"))) char *
find_if_is_not_graph(const char *begin, const char *end) {
	size_t length = end - begin;
	size_t offset = 0;

#ifdef __AVX2__

	const __m256i avx2bang = _mm256_set1_epi8('!' - 1);
	const __m256i avx2del = _mm256_set1_epi8(127); // DEL

	size_t avx2_offset = 0;
	size_t avx2_length = length / sizeof(__m256i);

	for (; avx2_offset < avx2_length; avx2_offset++) {
		__m256i b;
		memcpy(&b, begin + avx2_offset * sizeof(__m256i), sizeof(b));

		__m256i v1 = _mm256_cmpgt_epi8(avx2bang, b);
		__m256i v2 = _mm256_cmpeq_epi8(b, avx2del);

		unsigned int vmask =
		    _mm256_movemask_epi8(v1) | _mm256_movemask_epi8(v2);
		if (UNLIKELY(~vmask)) {
			// rightmost 1 is pos of first invalid byte
			offset += __builtin_ctz(vmask);
			offset += avx2_offset * sizeof(__m256i);
			return (char *)begin + offset;
		}
	}

	offset += avx2_offset * sizeof(__m256i);
#endif

	size_t i = offset;
	for (; LIKELY(i < length); i++) {
		char c = begin[i];
		if (UNLIKELY(c < '!' || c == 127)) return (char *)&begin[i];
	}
	return (char *)end;
}

char *find_if_is_not_graph_indexed_sse2(const char *begin, const char *end) {
	size_t offset = 0;
	size_t length = end - begin;

	// There is a great explanation at [1] how one can quickly scan for a
	// certain character in a string using SIMD instructions. Here I have
	// settled for an SSE2 implementation (and a manual fallback). Using
	// pcmpistri I could not measure any improvement. Same for AVX2.

	// [1]:
	// http://natsys-lab.blogspot.de/2016/10/http-strings-processing-using-c-sse42.html

#ifdef __AVX2__

	const __m256i avx2bang = _mm256_set1_epi8('!' - 1);
	const __m256i avx2del = _mm256_set1_epi8(127); // DEL

	size_t avx2_offset = 0;
	size_t avx2_length = length / sizeof(__m256i);

	for (; avx2_offset < avx2_length; avx2_offset++) {
		__m256i b;
		memcpy(&b, begin + avx2_offset * sizeof(__m256i), sizeof(b));

		__m256i v1 = _mm256_cmpgt_epi8(avx2bang, b);
		__m256i v2 = _mm256_cmpeq_epi8(b, avx2del);

		unsigned int vmask =
		    _mm256_movemask_epi8(v1) | _mm256_movemask_epi8(v2);
		if (UNLIKELY(vmask)) {
			// rightmost 1 is pos of first invalid byte
			offset += __builtin_ctz(vmask);
			offset += avx2_offset * sizeof(__m256i);
			return (char *)begin + offset;
		}
	}

	offset += avx2_offset * sizeof(__m256i);
#else
#ifdef __SSE2__

	const __m128i allbang = _mm_set1_epi8('!');
	const __m128i alldel = _mm_set1_epi8(127); // DEL

	size_t long_offset = 0;
	size_t long_length = (end - /*offset -*/ begin) / sizeof(__m128i);

	for (; long_offset < long_length; long_offset++) {
		__m128i b;
		memcpy(&b, begin + /*offset +*/ long_offset * sizeof(__m128i),
		       sizeof(b));

		__m128i v1 = _mm_cmplt_epi8(b, allbang);
		__m128i v2 = _mm_cmpeq_epi8(b, alldel);

		unsigned int vmask = _mm_movemask_epi8(v1) | _mm_movemask_epi8(v2);
		if (UNLIKELY(vmask)) {
			// rightmost 1 is pos of first invalid byte
			offset += __builtin_ctz(vmask);
			offset += long_offset * sizeof(__m128i);
			return (char *)begin + offset;
		}
	}

	offset += long_offset * sizeof(__m128i);

#else

	// Fallback method using bit twiddling hacks. Almost as fast as SSE2.
	// http://graphics.stanford.edu/~seander/bithacks.html

#define hasless(x, n) (((x) - ~0ULL / 255 * (n)) & ~(x) & ~0ULL / 255 * 128)
#define hasvalue(x, n) (hasless((x) ^ (~0ULL / 255 * (n)), 1))

	size_t long_offset = 0;
	size_t long_length = (end - begin - offset) / sizeof(long);

	for (; long_offset < long_length; long_offset++) {
		long buffer;
		memcpy(&buffer, begin + offset + long_offset * sizeof(buffer),
		       sizeof(buffer));
		if (hasless(buffer, '!')) break;
		if (hasvalue(buffer, '~' + 1)) break;
	}

	offset += long_offset * sizeof(long);

#endif
#endif

	for (; offset < length; offset++) {
		if (LIKELY(!my_isgraph(begin[offset]))) break;
	}
	return (char *)begin + offset;
}

static int copy_word(struct pfasta_parser *pp, dynstr *target) {
	int return_code = 0;

	while (LIKELY(my_isgraph(buffer_peek(pp)))) {
		char *end_of_word =
		    find_if_is_not_graph_indexed_sse2(buffer_begin(pp), buffer_end(pp));
		size_t word_length = end_of_word - buffer_begin(pp);

		int check = dynstr_append(target, buffer_begin(pp), word_length, pp);
		if (UNLIKELY(check)) PF_FAIL_BUBBLE(pp);

		check = buffer_advance(pp, word_length);
		if (UNLIKELY(check)) PF_FAIL_BUBBLE(pp);
	}

cleanup:
	return return_code;
}

static int skip_whitespace(struct pfasta_parser *pp) {
	int return_code = 0;

	// optimise for common case
	if (LIKELY(buffer_end(pp) - buffer_begin(pp) >= 2 &&
	           my_isspace(buffer_peek(pp)) &&
	           !my_isspace(buffer_begin(pp)[1]))) {
		int newlines = buffer_peek(pp) == '\n' ? 1 : 0;
		buffer_advance(pp, 1);
		PF_FAIL_BUBBLE(pp);
		pp->line_number += newlines;
		return 0;
	}

	while (my_isspace(buffer_peek(pp))) {
		char *split = find_if_is_graph(buffer_begin(pp), buffer_end(pp));

		// advance may clear the buffer. So count first …
		size_t newlines = count_newlines(buffer_begin(pp), split);
		int check = buffer_advance(pp, split - buffer_begin(pp));
		if (UNLIKELY(check)) PF_FAIL_BUBBLE(pp);

		// … and then increase the counter.
		pp->line_number += newlines;
	}

cleanup:
	return return_code;
}

struct pfasta_parser pfasta_init(int file_descriptor) {
	int return_code = 0;
	struct pfasta_parser pp = {0};
	pp.line_number = 1;

	pp.file_descriptor = file_descriptor;
	buffer_init(&pp);
	PF_FAIL_BUBBLE(&pp);

	if (buffer_is_empty(&pp) || buffer_is_eof(&pp)) {
		PF_FAIL_STR(&pp, "File is empty.");
	}

	if (buffer_peek(&pp) != '>') {
		PF_FAIL_STR(&pp, "File must start with '>'.");
	}

cleanup:
	// free buffer if necessary
	if (return_code) {
		pfasta_free(&pp);
	}
	pp.done = return_code || buffer_is_eof(&pp);
	return pp;
}

struct pfasta_record pfasta_read(struct pfasta_parser *pp) {
	int return_code = 0;
	struct pfasta_record pr = {0};

	pfasta_read_name(pp, &pr);
	PF_FAIL_BUBBLE(pp);

	pfasta_read_comment(pp, &pr);
	PF_FAIL_BUBBLE(pp);

	pfasta_read_sequence(pp, &pr);
	PF_FAIL_BUBBLE(pp);

cleanup:
	if (return_code) {
		pfasta_record_free(&pr);
		pfasta_free(pp);
	}
	pp->done = return_code || buffer_is_eof(pp);
	return pr;
}

int pfasta_read_name(struct pfasta_parser *pp, struct pfasta_record *pr) {
	int return_code = 0;

	dynstr name;
	dynstr_init(&name, pp);
	PF_FAIL_BUBBLE(pp);

	assert(!buffer_is_empty(pp));
	assert(buffer_peek(pp) == '>');

	buffer_advance(pp, 1); // skip >
	PF_FAIL_BUBBLE(pp);

	copy_word(pp, &name);
	PF_FAIL_BUBBLE(pp);

	if (dynstr_len(&name) == 0)
		PF_FAIL_STR(pp, "Empty name on line %zu.", pp->line_number);

	pr->name_length = dynstr_len(&name);
	pr->name = dynstr_move(&name);

cleanup:
	if (return_code) {
		dynstr_free(&name);
	}
	return return_code;
}

int pfasta_read_comment(struct pfasta_parser *pp, struct pfasta_record *pr) {
	int return_code = 0;

	if (buffer_peek(pp) == '\n') {
		pr->comment_length = 0;
		pr->comment = NULL;
		return 0;
	}

	dynstr comment;
	dynstr_init(&comment, pp);
	PF_FAIL_BUBBLE(pp);

	assert(!buffer_is_empty(pp));

	buffer_advance(pp, 1); // skip first whitespace
	PF_FAIL_BUBBLE(pp);

	assert(!buffer_is_empty(pp));

	// get comment
	while (buffer_peek(pp) != '\n') {
		int check = copy_word(pp, &comment);
		PF_FAIL_BUBBLE(pp);

		if (buffer_is_eof(pp))
			PF_FAIL_STR(pp, "Unexpected EOF in comment on line %zu.",
			            pp->line_number);

		// iterate non-linebreak whitespace
		while (isblank(buffer_peek(pp))) {
			dynstr_append(&comment, buffer_begin(pp), 1, pp);
			PF_FAIL_BUBBLE(pp);

			buffer_advance(pp, 1);
			PF_FAIL_BUBBLE(pp);
		}
	}

	pr->comment_length = dynstr_len(&comment);
	pr->comment = dynstr_move(&comment);

cleanup:
	if (return_code) {
		dynstr_free(&comment);
	}
	return return_code;
}

int pfasta_read_sequence(struct pfasta_parser *pp, struct pfasta_record *pr) {
	int return_code = 0;

	dynstr sequence;
	dynstr_init(&sequence, pp);
	PF_FAIL_BUBBLE(pp);

	assert(!buffer_is_empty(pp));
	assert(!buffer_is_eof(pp));
	assert(buffer_peek(pp) == '\n');

	skip_whitespace(pp);
	PF_FAIL_BUBBLE(pp);

	while (LIKELY(isalpha(buffer_peek(pp)))) {
		int check = copy_word(pp, &sequence);
		if (UNLIKELY(check)) PF_FAIL_BUBBLE(pp);

		check = skip_whitespace(pp);
		if (UNLIKELY(check)) PF_FAIL_BUBBLE(pp);
	}

	if (dynstr_len(&sequence) == 0)
		PF_FAIL_STR(pp, "Empty sequence on line %zu.", pp->line_number);

	pr->sequence_length = dynstr_len(&sequence);
	pr->sequence = dynstr_move(&sequence);

cleanup:
	if (return_code) {
		dynstr_free(&sequence);
	}
	return return_code;
}

void pfasta_record_free(struct pfasta_record *pr) {
	if (!pr) return;
	free(pr->name);
	free(pr->comment);
	free(pr->sequence);
	pr->name = pr->comment = pr->sequence = NULL;
}

void pfasta_free(struct pfasta_parser *pp) {
	if (!pp) return;
	free(pp->buffer);
	pp->buffer = NULL;
}

int main(int argc, char *argv[]) {
	argc--, argv++;
	for (; *argv; argv++) {
		int file_descriptor =
		    strcmp(*argv, "-") == 0 ? STDIN_FILENO : open(*argv, O_RDONLY);
		struct pfasta_parser pp = pfasta_init(file_descriptor);
		if (pp.errstr) errx(1, "%s", pp.errstr);

		while (!pp.done) {
			struct pfasta_record pr = pfasta_read(&pp);
			if (pp.errstr) errx(2, "%s", pp.errstr);

			if (pr.comment) {
				printf(">%s %s\n%zu\n", pr.name, pr.comment,
				       pr.sequence_length);
			} else {
				printf(">%s\n%zu\n", pr.name, pr.sequence_length);
			}
			pfasta_record_free(&pr);
		}

		pfasta_free(&pp);
		close(file_descriptor);
	}
}

/** @brief Creates a new string that can grow dynamically.
 *
 * @param ds - A reference to the dynstr container.
 *
 * @returns 0 iff successful.
 */
static inline int dynstr_init(dynstr *ds, struct pfasta_parser *pp) {
	int return_code = 0;

	*ds = (dynstr){NULL, 0, 0};
	ds->str = malloc(61);
	if (!ds->str) PF_FAIL_ERRNO(pp);

	ds->str[0] = '\0';
	// ds->capacity = 61;
	ds->capacity = malloc_usable_size(ds->str);
	ds->count = 0;

cleanup:
	return return_code;
}

/** @brief A append a character to a string.
 *
 * @param ds - A reference to the dynstr container.
 * @param c - The new character.
 *
 * @returns 0 iff successful.
 */
static inline int dynstr_put(dynstr *ds, char c) {
	if (ds->count >= ds->capacity - 1) {
		char *neu = reallocarray(ds->str, ds->capacity / 2, 3);
		if (!neu) {
			dynstr_free(ds);
			return -1;
		}
		ds->str = neu;
		ds->capacity = (ds->capacity / 2) * 3;
	}

	ds->str[ds->count++] = c;
	return 0;
}

/** @brief A append more than one character to a string.
 *
 * @param ds - A reference to the dynstr container.
 * @param str - The new characters.
 * @param length - number of new characters to append
 *
 * @returns 0 iff successful.
 */
static inline int dynstr_append(dynstr *ds, const char *str, size_t length,
                                struct pfasta_parser *pp) {
	int return_code = 0;
	size_t required = ds->count + length;

	if (UNLIKELY(required >= ds->capacity)) {
		char *neu = reallocarray(ds->str, required / 2, 3);
		if (UNLIKELY(!neu)) {
			dynstr_free(ds);
			PF_FAIL_ERRNO(pp);
		}
		ds->str = neu;
		// ds->capacity = (required / 2) * 3;
		ds->capacity = malloc_usable_size(ds->str);
	}

	memcpy(ds->str + ds->count, str, length);
	ds->count = required;

cleanup:
	return return_code;
}

/** @brief Frees a dynamic string. */
static inline void dynstr_free(dynstr *ds) {
	if (!ds) return;
	free(ds->str);
	*ds = (dynstr){NULL, 0, 0};
}

/** @brief Returns the string as a standard `char*`. The internal reference is
 * then deleted. Hence the name *move* as in *move semantics*.
 *
 * @param ds - The dynamic string to move from.
 *
 * @returns a `char*` to a standard null-terminated string.
 */
static inline char *dynstr_move(dynstr *ds) {
	char *out = reallocarray(ds->str, ds->count + 1, 1);
	if (!out) {
		out = ds->str;
	}
	out[ds->count] = '\0';
	*ds = (dynstr){NULL, 0, 0};
	return out;
}

/** @brief Returns the current length of the dynamic string. */
static inline size_t dynstr_len(const dynstr *ds) { return ds->count; }

/**
 * @brief Unsafe fallback in case reallocarray isn't provided by the stdlib.
 */
__attribute__((weak)) void *reallocarray(void *ptr, size_t nmemb, size_t size) {
	return realloc(ptr, nmemb * size);
}
