#ifndef PFASTA_H
#define PFASTA_H

#include <threads.h>

/** The following is the maximum length of an error string. It has to be
 * carefully chosen, so that all calls to PF_FAIL_STR succeed. For instance,
 * the line number can account for up to 20 characters.
 */
#define PF_ERROR_STRING_LENGTH 100

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

typedef struct dynstr {
	char *str;
	size_t capacity, count;
} dynstr;

thread_local char errstr_buffer[PF_ERROR_STRING_LENGTH];

void pfasta_record_free(struct pfasta_record *pr);
void pfasta_free(struct pfasta_parser *pp);
struct pfasta_parser pfasta_init(int file_descriptor);
struct pfasta_record pfasta_read(struct pfasta_parser *pp);


#endif /* PFASTA_H */
