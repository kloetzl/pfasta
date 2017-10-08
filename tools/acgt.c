#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "pfasta.h"

static int line_length = 70;

void usage(int exit_code);
void process(const char *file_name);
void filter_acgt(char *seq);

int main(int argc, char *argv[]) {
	int c;
	while ((c = getopt(argc, argv, "hL:")) != -1) {
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
		filter_acgt(ps.seq);
		pfasta_print(STDOUT_FILENO, &ps, line_length);
	}

	if (l < 0) {
		errx(1, "%s: %s", file_name, pfasta_strerror(&pf));
	}

	pfasta_free(&pf);
	close(file_descriptor);
}

void filter_acgt(char *seq) {
	char const *p = seq;
	char *q = seq;
	for (; *p; p++) {
		char c = *p;
		switch (c) {
		case 'A':
		case 'C':
		case 'G':
		case 'T':
		case 'a':
		case 'c':
		case 'g':
		case 't':
			*q++ = c;
			break;
		default:
			break;
		}
	}
	*q++ = 0;
}

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: acgt [OPTIONS...] [FILE...]\n"
	    "Filter the nucleotides. Set FILE to '-' to read from standard "
	    "input.\n\n"
	    "Options:\n"
	    "  -h         Display help and exit\n"
	    "  -L num     Set the maximum line length (0 to disable)\n"};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
