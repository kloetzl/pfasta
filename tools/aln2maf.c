#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfasta.h"

void usage(int exit_code);
void process(const char *file_name);
size_t nongaps(const char *str, size_t length);

struct seq_vector {
	pfasta_seq *data;
	size_t size;
	size_t capacity;
} sv;

void sv_init() {
	sv.data = malloc(4 * sizeof(pfasta_seq));
	sv.size = 0;
	sv.capacity = 4;
	if (!sv.data) err(errno, "malloc failed");
}

void sv_emplace(pfasta_seq ps) {
	if (sv.size < sv.capacity) {
		sv.data[sv.size++] = ps;
	} else {
		sv.data =
		    reallocarray(sv.data, sv.capacity / 2, 3 * sizeof(pfasta_seq));
		if (!sv.data) err(errno, "realloc failed");
		// reallocarray would return NULL, if mult would overflow
		sv.capacity = (sv.capacity / 2) * 3;
		sv.data[sv.size++] = ps;
	}
}

void sv_free() {
	for (size_t i = 0; i < sv.size; i++) {
		pfasta_seq_free(&sv.data[i]);
	}
	free(sv.data);
}

int main(int argc, char *argv[]) {
	sv_init();

	int c = getopt(argc, argv, "h");
	if (c != -1) {
		usage(c == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
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
	size_t length = strlen(sv.data[0].seq);
	for (size_t i = 1; i < sv.size; i++) {
		if (strlen(sv.data[i].seq) != length) {
			errx(1, "alignments of unequal length");
		}
	}

	printf("##maf version=1 program=aln2maf\n\na\n");
	for (size_t i = 0; i < sv.size; ++i) {
		size_t ng = nongaps(sv.data[i].seq, length);
		printf("s %s %i %zu %c %zu %s\n", sv.data[i].name, 0, ng, '+', length,
		       sv.data[i].seq);
	}

	sv_free();
	return EXIT_SUCCESS;
}

void process(const char *file_name) {
	int file_descriptor =
	    strcmp(file_name, "-") == 0 ? STDIN_FILENO : open(file_name, O_RDONLY);
	if (file_descriptor < 0) err(1, "%s", file_name);

	pfasta_file pf;
	int l = pfasta_parse(&pf, file_descriptor);
	if (l != 0) {
		errx(1, "%s: %s", file_name, pfasta_strerror(&pf));
	}

	pfasta_seq ps;
	while ((l = pfasta_read(&pf, &ps)) == 0) {
		sv_emplace(ps);
	}

	if (l < 0) {
		errx(1, "%s: %s", file_name, pfasta_strerror(&pf));
	}

	pfasta_free(&pf);
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
