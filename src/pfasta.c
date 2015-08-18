#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "pfasta.h"

#define BUFFERSIZE 4096

#define PF_EXIT_ERRNO()                                                        \
	do {                                                                       \
		pf->errno__ = errno;                                                   \
		pf->errstr = NULL;                                                     \
		return errno;                                                          \
	} while (0)
#define PF_EXIT_FORWARD() return -1

#define PF_FAIL_FORWARD()                                                      \
	do {                                                                       \
		return_code = -1;                                                      \
		goto fail;                                                             \
	} while (0)
#define PF_FAIL_ERRNO()                                                        \
	do {                                                                       \
		pf->errno__ = errno;                                                   \
		pf->errstr = NULL;                                                     \
		return_code = errno;                                                   \
		goto fail;                                                             \
	} while (0)
#define PF_FAIL_STR(str)                                                       \
	do {                                                                       \
		pf->errno__ = 0;                                                       \
		pf->errstr = str;                                                      \
		return_code = -1;                                                      \
		goto fail;                                                             \
	} while (0)

static int binit(pfasta_file *pf);
static inline int bpeek(const pfasta_file *pf);
static inline int badv(pfasta_file *pf);
static ssize_t bread(pfasta_file *pf);

typedef struct dynstr {
	char *str;
	size_t capacity, count;
} dynstr;

static inline void *reallocarray(void *optr, size_t nmemb, size_t size);
static inline int dynstr_init(dynstr *ds);
static inline int dynstr_put(dynstr *ds, char c);
static inline void dynstr_free(dynstr *ds);
static inline char *dynstr_move(dynstr *ds);
static inline size_t dynstr_len(const dynstr *ds);

ssize_t pfasta_read_name(pfasta_file *pf, pfasta_seq *ps);
ssize_t pfasta_read_comment(pfasta_file *pf, pfasta_seq *ps);
ssize_t pfasta_read_seq(pfasta_file *pf, pfasta_seq *ps);

static int binit(pfasta_file *pf) {
	char *buffer = malloc(BUFFERSIZE);
	if (!buffer) PF_EXIT_ERRNO();

	pf->buffer = pf->readptr = pf->fillptr = buffer;
	if (bread(pf) < 0) PF_EXIT_FORWARD();
	return 0;
}

static inline int bpeek(const pfasta_file *pf) {
	if (pf->readptr < pf->fillptr) {
		return (int)*(pf->readptr);
	}
	return EOF;
}

static inline int badv(pfasta_file *pf) {
	if (pf->readptr < pf->fillptr - 1) {
		pf->readptr++;
		return 0;
	}

	if (bread(pf) < 0) PF_EXIT_FORWARD();

	return 0;
}

static ssize_t bread(pfasta_file *pf) {
	ssize_t count = read(pf->fd, pf->buffer, BUFFERSIZE);
	if (count < 0) PF_EXIT_ERRNO();
	if (count == 0) { // EOF
		pf->fillptr = pf->buffer;
		pf->readptr = pf->buffer + 1;
		return 0;
	}

	pf->readptr = pf->buffer;
	pf->fillptr = pf->buffer + count;

	return count;
}

void pfasta_free(pfasta_file *pf) {
	if (!pf) return;
	free(pf->buffer);
	pf->buffer = pf->readptr = pf->fillptr = pf->errstr = NULL;
	pf->errno__ = pf->fd = 0;
}

int pfasta_parse(pfasta_file *pf, FILE *in) {
	assert(pf && in);
	int return_code = 0;

	pf->errno__ = 0;
	pf->buffer = pf->readptr = pf->fillptr = pf->errstr = NULL;

	pf->fd = fileno(in);
	if (pf->fd == -1) PF_FAIL_ERRNO();

	if (binit(pf) != 0) PF_FAIL_FORWARD();

	int c = bpeek(pf);
	if (c == EOF) PF_FAIL_STR("Empty file");
	if (c != '>') PF_FAIL_STR("File does not start with '>'");

fail:
	return return_code;
}

void pfasta_seq_free(pfasta_seq *ps) {
	if (!ps) return;
	free(ps->name);
	free(ps->comment);
	free(ps->seq);
	ps->name = ps->comment = ps->seq = NULL;
}

ssize_t pfasta_read(pfasta_file *pf, pfasta_seq *ps) {
	assert(pf && ps && pf->buffer);
	*ps = (pfasta_seq){NULL, NULL, NULL};
	int return_code = 0;

	if (bpeek(pf) == EOF) return 1;
	if (bpeek(pf) != '>') PF_FAIL_STR("Expected '>'");

	if (pfasta_read_name(pf, ps) < 0) PF_FAIL_FORWARD();
	if (isblank(bpeek(pf))) {
		if (pfasta_read_comment(pf, ps) < 0) PF_FAIL_FORWARD();
	}
	if (pfasta_read_seq(pf, ps) < 0) PF_FAIL_FORWARD();

	// Skip blank lines
	while (bpeek(pf) == '\n') {
		if (badv(pf) != 0) PF_FAIL_FORWARD();
	}

fail:
	return return_code;
}

ssize_t pfasta_read_name(pfasta_file *pf, pfasta_seq *ps) {
	ssize_t return_code = 0;
	int c;
	dynstr name;
	if (dynstr_init(&name) != 0) PF_FAIL_ERRNO();

	while (1) {
		if (badv(pf) != 0) PF_FAIL_FORWARD();

		c = bpeek(pf);
		if (c == EOF) PF_FAIL_STR("Unexpected EOF in sequence name");
		if (!isgraph(c)) break;

		if (dynstr_put(&name, c) != 0) PF_FAIL_ERRNO();
	}

	ssize_t count = dynstr_len(&name);

	if (count == 0) PF_FAIL_STR("Empty name");

	ps->name = dynstr_move(&name);
	return count;

fail: /* cleanup */
	dynstr_free(&name);
	return return_code;
}

ssize_t pfasta_read_comment(pfasta_file *pf, pfasta_seq *ps) {
	ssize_t return_code = 0;
	int c;
	dynstr comment;
	if (dynstr_init(&comment) != 0) PF_FAIL_ERRNO();

	while (1) {
		if (badv(pf) != 0) PF_FAIL_FORWARD();

		c = bpeek(pf);
		if (c == EOF) PF_FAIL_STR("Unexpected EOF in sequence comment");
		if (c == '\n') break;

		if (dynstr_put(&comment, c) != 0) PF_FAIL_ERRNO();
	}

	ssize_t count = dynstr_len(&comment);
	ps->comment = dynstr_move(&comment);
	return count;

fail:
	dynstr_free(&comment);
	return return_code;
}

ssize_t pfasta_read_seq(pfasta_file *pf, pfasta_seq *ps) {
	ssize_t return_code = 0;
	int c;
	dynstr seq;
	if (dynstr_init(&seq) != 0) PF_FAIL_ERRNO();

	while (1) {
		assert(bpeek(pf) == '\n');

		// deal with the first character explicitly
		if (badv(pf) != 0) PF_FAIL_FORWARD();

		c = bpeek(pf);
		if (c == EOF || c == '>' || c == '\n') break;

		goto regular;

		// read line
		while (1) {
			if (badv(pf) != 0) PF_FAIL_FORWARD();

			c = bpeek(pf);
			if (c == '\n') break;

		regular:
			if (!isgraph(c)) PF_FAIL_STR("Unexpected character in sequence");
			if (dynstr_put(&seq, c) != 0) PF_FAIL_ERRNO();
		}
	}

	ssize_t count = dynstr_len(&seq);
	if (count == 0) PF_FAIL_STR("Empty sequence");
	ps->seq = dynstr_move(&seq);
	return count;

fail:
	dynstr_free(&seq);
	return return_code;
}

const char *pfasta_strerror(const pfasta_file *pf) {
	if (pf->errno__ == 0) {
		return pf->errstr;
	} else {
		return strerror(pf->errno__);
	}
}

static inline int dynstr_init(dynstr *ds) {
	*ds = (dynstr){NULL, 0, 0};
	ds->str = malloc(61);
	if (!ds->str) return -1;
	ds->str[0] = '\0';
	ds->capacity = 61;
	return 0;
}

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

static inline void dynstr_free(dynstr *ds) {
	free(ds->str);
	*ds = (dynstr){NULL, 0, 0};
}

static inline char *dynstr_move(dynstr *ds) {
	char *out = ds->str;
	out[ds->count] = '\0';
	*ds = (dynstr){NULL, 0, 0};
	return out;
}

static inline size_t dynstr_len(const dynstr *ds) { return ds->count; }

/*
 * This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
 * if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW
 */
#define MUL_NO_OVERFLOW ((size_t)1 << (sizeof(size_t) * 4))

static inline void *reallocarray(void *optr, size_t nmemb, size_t size) {
	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) && nmemb > 0 &&
	    SIZE_MAX / nmemb < size) {
		errno = ENOMEM;
		return NULL;
	}
	return realloc(optr, size * nmemb);
}
