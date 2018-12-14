#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "pfasta.h"

static size_t line_length = 70;

void usage(int exit_code);
void process(const char *file_name);

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

void sv_emplace(struct pfasta_record pr) {
	if (sv.size < sv.capacity) {
		sv.data[sv.size++] = pr;
	} else {
		sv.data = my_reallocarray(sv.data, sv.capacity / 2,
		                          3 * sizeof(struct pfasta_record));
		if (!sv.data) err(errno, "realloc failed");
		// reallocarray would return NULL, if mult would overflow
		sv.capacity = (sv.capacity / 2) * 3;
		sv.data[sv.size++] = pr;
	}
}

void sv_free() {
	for (size_t i = 0; i < sv.size; i++) {
		pfasta_record_free(&sv.data[i]);
	}
	free(sv.data);
}

void sv_swap(size_t i, size_t j) {
	struct pfasta_record temp = sv.data[i];
	sv.data[i] = sv.data[j];
	sv.data[j] = temp;
}

int main(int argc, char *argv[]) {
	sv_init();

	unsigned int seed = 0;
	int c;
	while ((c = getopt(argc, argv, "hL:s:")) != -1) {
		switch (c) {
		case 'h':
			usage(EXIT_SUCCESS);
		case 'L': {
			const char *errstr;

			line_length = my_strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr) errx(1, "line length is %s: %s", errstr, optarg);

			if (!line_length) line_length = INT_MAX;
			break;
		}
		case 's': {
			const char *errstr;

			seed = my_strtonum(optarg, 0, UINT_MAX, &errstr);
			if (errstr) errx(1, "seed is %s: %s", errstr, optarg);

			break;
		}
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

	if (seed == 0) {
		seed = time(NULL) + getpid();
	}

	srand(seed);

	for (size_t i = sv.size; i > 0; i--) {
		size_t j = rand() % i;
		sv_swap(i - 1, j);
	}

	for (size_t i = 0; i < sv.size; i++) {
		pfasta_print(STDOUT_FILENO, &sv.data[i], line_length);
	}

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

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: shuffle [OPTIONS...] [FILE...]\n"
	    "Shuffle a set of sequences.\n"
	    "When FILE is '-' read from standard input.\n\n"
	    "Options:\n"
	    "  -h         Display help and exit\n"
	    "  -L num     Set the maximum line length (0 to disable)\n"
	    "  -s seed    Seed the PRNG\n" //
	};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
