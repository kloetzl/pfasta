#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "pfasta.h"

void usage(int exit_code);
void process(const char *file_name);
size_t count_muts(const struct pfasta_record *subject,
                  const struct pfasta_record *query, size_t length);
void print_mutations(size_t *DD);
void print_jc(size_t *DD, size_t length);
void print_ani(size_t *DD, size_t length);

struct seq_vector {
	struct pfasta_record *data;
	size_t size;
	size_t capacity;
} sv;

void sv_init() {
	sv.data = malloc(4 * sizeof(struct pfasta_record));
	sv.size = 0;
	sv.capacity = 4;
	if (!sv.data) err(errno, "malloc failed");
}

void sv_emplace(struct pfasta_record ps) {
	if (sv.size < sv.capacity) {
		sv.data[sv.size++] = ps;
	} else {
		sv.data = my_reallocarray(sv.data, sv.capacity / 2,
		                          3 * sizeof(struct pfasta_record));
		if (!sv.data) err(errno, "realloc failed");
		// reallocarray would return NULL, if mult would overflow
		sv.capacity = (sv.capacity / 2) * 3;
		sv.data[sv.size++] = ps;
	}
}

void sv_free() {
	for (size_t i = 0; i < sv.size; i++) {
		pfasta_record_free(&sv.data[i]);
	}
	free(sv.data);
}

enum { F_JC, F_MUTATIONS, F_ANI };

int main(int argc, char *argv[]) {
	sv_init();

	int c;
	int format = F_JC;
	while ((c = getopt(argc, argv, "f:h")) != -1) {
		switch (c) {
		case 'f': {
			// available formats: mutations, JC, ANI
			if (strcasecmp(optarg, "mutations") == 0) {
				format = F_MUTATIONS;
			} else if (strcasecmp(optarg, "JC") == 0) {
				format = F_JC;
			} else if (strcasecmp(optarg, "ANI") == 0) {
				format = F_ANI;
			} else {
				errx(1, "unknown output format '%s'", optarg);
			}
			break;
		}
		case 'h':
			usage(EXIT_SUCCESS);
		default:
			usage(EXIT_FAILURE);
		}
	}

	argc -= optind, argv += optind;
	if (argc == 0) {
		if (!isatty(STDIN_FILENO)) {
			process("-");
		} else {
			usage(EXIT_FAILURE);
		}
	}

	for (int i = 0; i < argc; i++) {
		process(argv[i]);
	}

	// check lengths
	if (sv.size < 2) errx(1, "less than two sequences read");
	size_t length = strlen(sv.data[0].sequence);
	for (size_t i = 1; i < sv.size; i++) {
		if (strlen(sv.data[i].sequence) != length) {
			errx(1, "alignments of unequal length");
		}
	}

	size_t *DD = malloc(sv.size * sv.size * sizeof(*DD));
	if (!DD) err(errno, "out of memory");

#define D(X, Y) (DD[(X)*sv.size + (Y)])

	for (size_t i = 0; i < sv.size; i++) {
		D(i, i) = 0.0;
		for (size_t j = 0; j < i; j++) {
			D(i, j) = D(j, i) = count_muts(&sv.data[i], &sv.data[j], length);
		}
	}

	if (format == F_JC) {
		print_jc(DD, length);
	} else if (format == F_MUTATIONS) {
		print_mutations(DD);
	} else if (format == F_ANI) {
		print_ani(DD, length);
	}

	free(DD);
	sv_free();

	return EXIT_SUCCESS;
}

void process(const char *file_name) {
	int file_descriptor =
	    strcmp(file_name, "-") == 0 ? STDIN_FILENO : open(file_name, O_RDONLY);
	if (file_descriptor < 0) err(1, "%s", file_name);

	struct pfasta_parser pp = pfasta_init(file_descriptor);
	if (pp.errstr) errx(1, "%s: %s", file_name, pp.errstr);

	while (!pp.done) {
		struct pfasta_record pr = pfasta_read(&pp);
		if (pp.errstr) errx(2, "%s: %s", file_name, pp.errstr);

		sv_emplace(pr);
	}

	pfasta_free(&pp);
	close(file_descriptor);
}

size_t count_muts(const struct pfasta_record *subject,
                  const struct pfasta_record *query, size_t length) {
	size_t mutations = 0;
	const char *s = subject->sequence;
	const char *q = query->sequence;

	for (size_t i = 0; i < length; i++) {
		if (s[i] != q[i]) {
			mutations++;
		}
	}

	return mutations;
}

void print_mutations(size_t *DD) {
	printf("%zu\n", sv.size);
	for (size_t i = 0; i < sv.size; i++) {
		printf("%-10s", sv.data[i].name);
		for (size_t j = 0; j < sv.size; j++) {
			printf(" %4zu", D(i, j));
		}
		printf("\n");
	}
}

void print_jc(size_t *DD, size_t length) {
	printf("%zu\n", sv.size);
	for (size_t i = 0; i < sv.size; i++) {
		printf("%-10s", sv.data[i].name);
		for (size_t j = 0; j < sv.size; j++) {
			double muts = D(i, j) / (double)length;
			// apply Jukes-Cantor Correction
			if (muts > 0.0) {
				muts = -0.75 * log(1.0 - (4.0 / 3.0) * muts);
			}
			printf(" %1.6e", muts);
		}
		printf("\n");
	}
}

void print_ani(size_t *DD, size_t length) {
	printf("%zu\n", sv.size);
	for (size_t i = 0; i < sv.size; i++) {
		printf("%-10s", sv.data[i].name);
		for (size_t j = 0; j < sv.size; j++) {
			double ani = (length - D(i, j)) / (double)length;
			printf(" %lf", ani);
		}
		printf("\n");
	}
}

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: aln2dist [OPTIONS...] [FILE...]\n"
	    "Compute sequence distances from an alignment. Output is a "
	    "PHYLIP-style distance matrix.\n"
	    "When FILE is '-' read from standard input.\n\n"
	    "Options:\n"
	    "  -f FORMAT  Set output format to one of 'JC', 'ANI', or 'mutations'\n"
	    "  -h         Display help and exit\n" //
	};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
