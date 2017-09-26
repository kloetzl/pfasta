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
void print_counts(const size_t *counts);

//const int CHARS = 128;
#define CHARS 128
size_t counts_total[CHARS];
size_t counts_local[CHARS];

enum {
	NONE = 0,
	SPLIT = 1,
	CASE_INSENSITIVE = 2
} FLAGS = 0;

int main(int argc, char *argv[])
{
	int c;

	while ((c = getopt(argc, argv, "his")) != -1) {
		switch (c) {
			case 'i':
				FLAGS |= CASE_INSENSITIVE;
				break;
			case 's':
				FLAGS |= SPLIT;
				break;
			case 'h':
			case '?':
			default:
				fprintf(stderr, "Usage: %s [-his] [FASTA...]\n", argv[0]);
				return 1;
		}
	}

	bzero(counts_total, sizeof(counts_total));


	argv += optind;

	int firsttime = 1;
	int exit_code = EXIT_SUCCESS;

	for (;; firsttime = 0) {
		int file_descriptor;
		const char *file_name;
		if (!*argv) {
			if (!firsttime) break;

			file_descriptor = STDIN_FILENO;
			file_name = "stdin";
		} else {
			file_name = *argv++;
			file_descriptor = open(file_name, O_RDONLY);
			if (file_descriptor < 0) err(1, "%s", file_name);
		}

		int l;
		pfasta_file pf;
		if ((l = pfasta_parse(&pf, file_descriptor)) != 0) {
			warnx("%s: %s", file_name, pfasta_strerror(&pf));
			exit_code = EXIT_FAILURE;
			goto fail;
		}

		pfasta_seq ps;
		while ((l = pfasta_read(&pf, &ps)) == 0) {
			count(&ps);
			if (FLAGS & SPLIT) {
				printf(">%s\n", ps.name);
				print_counts(counts_local);
				bzero(counts_local, sizeof(counts_local));
			}
			pfasta_seq_free(&ps);
			for (size_t i = 0; i< CHARS; i++){
				counts_total[i] += counts_local[i];
			}
		}

		if (l < 0) {
			warnx("%s: %s", file_name, pfasta_strerror(&pf));
			exit_code = EXIT_FAILURE;
			pfasta_seq_free(&ps);
		}

		if (!(FLAGS & SPLIT)) {
			printf(">%s\n", file_name);
			print_counts(counts_total);
			bzero(counts_total, sizeof(counts_total));
		}

	fail:
		pfasta_free(&pf);
		close(file_descriptor);
	}

	return exit_code;
}

void count(const pfasta_seq *ps)
{
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

void print_counts(const size_t *counts)
{
	size_t sum = 0;
	for (int i = 1; i < CHARS; ++i) {
		if (counts[i]) {
			printf("%c:\t%8zu\n", i, counts[i]);
			sum += counts[i];
		}
	}
	printf("# sum:\t%8zu\n", sum);
}
