#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfasta.h"

struct seq_vector {
	struct pfasta_record *data;
	size_t size;
	size_t capacity;
};

void sv_init(struct seq_vector *sv) {
	sv->data = malloc(4 * sizeof(struct pfasta_record));
	sv->size = 0;
	sv->capacity = 4;
	if (!sv->data) err(errno, "malloc failed");
}

void sv_emplace(struct seq_vector *sv, struct pfasta_record ps) {
	if (sv->size < sv->capacity) {
		sv->data[sv->size++] = ps;
	} else {
		sv->data = reallocarray(sv->data, sv->capacity / 2,
		                        3 * sizeof(struct pfasta_record));
		if (!sv->data) err(errno, "realloc failed");
		// reallocarray would return NULL, if mult would overflow
		sv->capacity = (sv->capacity / 2) * 3;
		sv->data[sv->size++] = ps;
	}
}

void sv_free(struct seq_vector *sv) {
	for (size_t i = 0; i < sv->size; i++) {
		pfasta_record_free(&sv->data[i]);
	}
	free(sv->data);
}

void usage(int exit_code);
void process(const char *file_name, struct seq_vector *sv);
size_t nongaps(const char *str, size_t length);

int main(int argc, char *argv[]) {

	int c = getopt(argc, argv, "h");
	if (c != -1) {
		usage(c == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	argc -= optind, argv += optind;

	struct seq_vector blocks[argc + 1];
	size_t num_blocks = argc;

	if (argc == 0) {
		if (!isatty(STDIN_FILENO)) {
			process("-", &blocks[0]);
			num_blocks = 1;
		} else {
			usage(EXIT_FAILURE);
		}
	}

	size_t total_length = 0;
	for (int i = 0; i < argc; i++) {
		process(argv[i], &blocks[i]);
		total_length += blocks[i].data[0].sequence_length;
	}

	size_t starting_pos = 0;

	printf("##maf version=1 program=aln2maf\n");
	for (size_t j = 0; j < num_blocks; j++) {
		struct seq_vector *sv = &blocks[j];

		printf("\na\n");
		for (size_t i = 0; i < sv->size; ++i) {
			struct pfasta_record pr = sv->data[i];
			size_t ng = nongaps(pr.sequence, pr.sequence_length);
			printf("s %s %zu %zu %c %zu %s\n", pr.name, starting_pos, ng, '+',
			       total_length, pr.sequence);
		}

		starting_pos += sv->data[0].sequence_length;
		sv_free(sv);
	}

	return EXIT_SUCCESS;
}

void process(const char *file_name, struct seq_vector *sv) {
	int file_descriptor =
	    strcmp(file_name, "-") == 0 ? STDIN_FILENO : open(file_name, O_RDONLY);
	if (file_descriptor < 0) err(1, "%s", file_name);

	struct pfasta_parser pp = pfasta_init(file_descriptor);
	if (pp.errstr) errx(1, "%s: %s", file_name, pp.errstr);

	sv_init(sv);

	size_t length = 0;
	while (!pp.done) {
		struct pfasta_record pr = pfasta_read(&pp);
		if (pp.errstr) errx(2, "%s: %s", file_name, pp.errstr);
		if (length == 0) length = pr.sequence_length;
		if (length != pr.sequence_length) {
			errx(3, "File %s contains sequences of unequal length", file_name);
		}

		sv_emplace(sv, pr);
	}

	if (sv->size < 2) errx(1, "%s: less than two sequences read", file_name);

	pfasta_free(&pp);
	close(file_descriptor);
}

size_t nongaps(const char *str, size_t length) {
	size_t count = 0;
	for (size_t i = 0; i < length; i++) {
		if (str[i] != '-') {
			count++;
		}
	}
	return count;
}

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: aln2maf [FILE...]\n"
	    "Convert an alignment to the Multiple Alignment Format (MAF).\n"
	    "When FILE is '-' read from standard input.\n\n"
	    "Options:\n"
	    "  -h         Display help and exit\n" //
	};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
