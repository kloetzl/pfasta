#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfasta.h"

void count(const struct pfasta_record *);
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

	struct pfasta_parser pp = pfasta_init(file_descriptor);
	if (pp.errstr) errx(1, "%s: %s", file_name, pp.errstr);

	while (!pp.done) {
		struct pfasta_record pr = pfasta_read(&pp);
		if (pp.errstr) errx(2, "%s: %s", file_name, pp.errstr);

		count(&pr);
		if (FLAGS & SPLIT) {
			print_counts(pr.name, counts_local);
			bzero(counts_local, sizeof(counts_local));
		}
		pfasta_record_free(&pr);
		for (size_t i = 0; i < CHARS; i++) {
			counts_total[i] += counts_local[i];
		}
	}

	if (!(FLAGS & SPLIT)) {
		print_counts(file_name, counts_total);
		bzero(counts_total, sizeof(counts_total));
	}

	pfasta_free(&pp);
	close(file_descriptor);
}

void count(const struct pfasta_record *pr) {
	bzero(counts_local, sizeof(counts_local));
	const char *ptr = pr->sequence;

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
	    "  -h         Display help and exit\n"
	    "  -i         Ignore case\n"
	    "  -s         Print output per individual sequence\n" //
	};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
