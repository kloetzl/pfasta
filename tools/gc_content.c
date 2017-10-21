#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfasta.h"

double gc(const pfasta_seq *ps);
void process(const char *file_name);
void usage(int exit_code);

int main(int argc, char *argv[]) {
	int c = getopt(argc, argv, "hL:");
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
		printf("%s\t%lf\n", ps.name, gc(&ps));
		pfasta_seq_free(&ps);
	}

	if (l < 0) {
		errx(1, "%s: %s", file_name, pfasta_strerror(&pf));
	}

	pfasta_free(&pf);
	close(file_descriptor);
}

// calculate the GC content
double gc(const pfasta_seq *ps) {
	size_t gc = 0;
	const char *ptr = ps->seq;

	for (; *ptr; ptr++) {
		if (*ptr == 'g' || *ptr == 'G' || *ptr == 'c' || *ptr == 'C') {
			gc++;
		}
	}

	return (double)gc / (ptr - ps->seq);
}

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: gc_content [FILE...]\n"
	    "Compute the GC content of each sequence.\n"
	    "When FILE is '-' read from standard input.\n\n"
	    "Options:\n"
	    "  -h         Display help and exit\n" //
	};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
