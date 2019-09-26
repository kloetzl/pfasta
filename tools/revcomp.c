#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "pfasta.h"

static int line_length = 70;

char *revcomp(const char *seq, size_t len);
void usage(int exit_code);
void process(const char *file_name);

int main(int argc, char *argv[]) {
	int c;
	while ((c = getopt(argc, argv, "hL:")) != -1) {
		switch (c) {
		case 'h':
			usage(EXIT_SUCCESS);
		case 'L': {
			const char *errstr;

			line_length = my_strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr) errx(1, "line length is %s: %s", errstr, optarg);

			if (!line_length) line_length = -1;
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

	struct pfasta_parser pp = pfasta_init(file_descriptor);
	if (pp.errstr) errx(1, "%s: %s", file_name, pp.errstr);

	while (!pp.done) {
		struct pfasta_record pr = pfasta_read(&pp);
		if (pp.errstr) errx(2, "%s: %s", file_name, pp.errstr);

		struct pfasta_record rc;
		size_t len = pr.sequence_length;

		rc.name = strdup(pr.name);
		if (!rc.name) err(errno, "out of memory");
		rc.comment = NULL;
		rc.sequence = revcomp(pr.sequence, len);

		if (pr.comment) {
			int check = asprintf(&rc.comment, "%s revcomp", pr.comment);
			if (check < 0) err(errno, "out of memory");
		} else {
			rc.comment = strdup("revcomp");
			if (!rc.comment) err(errno, "out of memory");
		}

		pfasta_print(STDOUT_FILENO, &rc, line_length);
		pfasta_record_free(&pr);
		pfasta_record_free(&rc);
	}

	pfasta_free(&pp);
	close(file_descriptor);
}

char *revcomp(const char *seq, size_t len) {
	if (!seq || !len) return NULL;
	char *rev = malloc(len + 1);
	if (!rev) {
		err(errno, "out of memory.");
	}
	rev[len] = '\0';

	for (size_t k = 0; k < len; k++) {
		unsigned char d = '!', c = seq[len - k - 1];

		if (c >= 'A') {
			d = c ^= c & 2 ? 4 : 21;
		}

		rev[k] = d;
	}

	return rev;
}

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: revcomp [OPTIONS...] [FILE...]\n"
	    "Print the reverse complement of each sequence.\n"
	    "When FILE is '-' read from standard input.\n\n"
	    "Options:\n"
	    "  -h         Display help and exit\n"
	    "  -L num     Set the maximum line length (0 to disable)\n" //
	};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
