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

void sv_swap(size_t i, size_t j) {
	pfasta_seq temp = sv.data[i];
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

			line_length = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr) errx(1, "line length is %s: %s", errstr, optarg);

			if (!line_length) line_length = INT_MAX;
			break;
		}
		case 's': {
			const char *errstr;

			seed = strtonum(optarg, 0, UINT_MAX, &errstr);
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
