#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfasta.h"

void usage(int exit_code);
void process(const char *file_name);

int main(int argc, char *argv[]) {
	int c = getopt(argc, argv, "h");
	if (c == 'h') {
		usage(EXIT_SUCCESS);
	} else if (c != -1) {
		usage(EXIT_FAILURE);
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

	return EXIT_SUCCESS;
}

int size_t_cmp(const void *pa, const void *pb) {
	const size_t *a = pa;
	const size_t *b = pb;
	if (*a == *b) return 0;
	return *a < *b ? -1 : 1;
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

	size_t *array = reallocarray(NULL, 61, sizeof(*array));
	size_t capacity = 61;
	size_t used = 0;

	pfasta_seq ps;
	while ((l = pfasta_read(&pf, &ps)) == 0) {
		if (used >= capacity) {
			size_t *neu = reallocarray(array, capacity / 2, sizeof(*array) * 3);
			if (!neu) err(errno, "oom");
			array = neu;
			capacity = (capacity / 2) * 3;
		}
		array[used++] = strlen(ps.seq);
		pfasta_seq_free(&ps);
	}

	if (l < 0) {
		errx(1, "%s: %s", file_name, pfasta_strerror(&pf));
	}

	qsort(array, used, sizeof(*array), size_t_cmp);

	size_t total = 0;
	for (size_t i = 0; i < used; i++) {
		total += array[i];
	}

	size_t partial = 0;
	size_t i = 0;
	while (partial < total / 2) {
		partial += array[i];
		i++;
	}

	printf("%zu\n", array[i - 1]);

	free(array);
	pfasta_free(&pf);
	close(file_descriptor);
}

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: n50 [OPTIONS...] [FILE...]\n"
	    "Compute N50. When FILE is '-' read from standard input.\n\n"
	    "Options:\n"
	    "  -h         Display help and exit\n" //
	};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
