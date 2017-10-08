#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfasta.h"

void count(const pfasta_seq *);
void print_counts(const char *name, const size_t *counts);
void usage(int exit_code);
void process(const char *file_name);

// const int CHARS = 128;
#define CHARS 128
size_t counts_total[CHARS];
size_t counts_local[CHARS];

enum { NONE = 0, SPLIT = 1, CASE_INSENSITIVE = 2 } FLAGS = 0;

int main(int argc, char *argv[]) {
	int c;
	while ((c = getopt(argc, argv, "ihs")) != -1) {
		switch (c) {
		case 'i':
			FLAGS |= CASE_INSENSITIVE;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		case 's':
			FLAGS |= SPLIT;
			break;
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
	bzero(counts_total, sizeof(counts_total));

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
		count(&ps);
		if (FLAGS & SPLIT) {
			print_counts(ps.name, counts_local);
			bzero(counts_local, sizeof(counts_local));
		}
		pfasta_seq_free(&ps);
		for (size_t i = 0; i < CHARS; i++) {
			counts_total[i] += counts_local[i];
		}
	}

	if (l < 0) {
		errx(1, "%s: %s", file_name, pfasta_strerror(&pf));
	}

	if (!(FLAGS & SPLIT)) {
		print_counts(file_name, counts_total);
		bzero(counts_total, sizeof(counts_total));
	}

	pfasta_free(&pf);
	close(file_descriptor);
}

void count(const pfasta_seq *ps) {
	bzero(counts_local, sizeof(counts_local));
	const char *ptr = ps->seq;

	while (*ptr) {
		unsigned char c = *ptr;
		if (FLAGS & CASE_INSENSITIVE) {
			c = toupper(c);
		}
		if (c < CHARS) {
			counts_local[c]++;
		}
		ptr++;
	}
}

void print_counts(const char *name, const size_t *counts) {
	printf(">%s\n", name);

	size_t sum = 0;
	for (int i = 1; i < CHARS; ++i) {
		if (counts[i]) {
			printf("%c:\t%8zu\n", i, counts[i]);
			sum += counts[i];
		}
	}
	printf("# sum:\t%8zu\n", sum);
}

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: cchar [OPTIONS...] [FILE...]\n"
	    "Count the residues. When FILE is '-' read from standard input.\n\n"
	    "Options:\n"
	    "  -i         Ignore case\n"
	    "  -h         Display help and exit\n"
	    "  -L num     Set the maximum line length (0 to disable)\n"
	    "  -s         Print output per individual sequence\n" //
	};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
