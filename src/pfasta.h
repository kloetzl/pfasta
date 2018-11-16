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

#ifndef PFASTA_H
#define PFASTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct pfasta_record {
	char *name, *comment, *sequence;
	size_t name_length, comment_length, sequence_length;
};

struct pfasta_parser {
	int file_descriptor;
	int done;
	char *buffer;
	char *read_ptr, *fill_ptr;
	size_t line_number;
	const char *errstr;
};

void pfasta_record_free(struct pfasta_record *pr);
void pfasta_free(struct pfasta_parser *pp);
struct pfasta_parser pfasta_init(int file_descriptor);
struct pfasta_record pfasta_read(struct pfasta_parser *pp);

#ifdef __STDC_NO_THREADS__
#define PFASTA_NO_THREADS
#endif

#ifdef __cplusplus
}
#endif

#endif /* PFASTA_H */
