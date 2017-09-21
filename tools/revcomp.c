#define _GNU_SOURCE
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfasta.h"

char *revcomp(const char *seq, size_t len);

static size_t line_length = 70;

size_t strncpy_n(char *dest, const char *src, size_t n) {
	size_t i = 0;
	for (; i < n && *src; ++i) {
		*dest++ = *src++;
	}
	return i;
}

void print_seq(const pfasta_seq *ps) {
	printf(">%s", ps->name);
	if (ps->comment) {
		printf(" %s\n", ps->comment);
	} else {
		printf("\n");
	}

	char line[line_length + 1];
	const char *seq = ps->seq;

	for (size_t j; *seq; seq += j) {
		j = strncpy_n(line, seq, line_length);
		line[j] = '\0';
		puts(line);
	}
}

int main(int argc, const char *argv[]) {

	if (argc == 2 && argv[1][0] == '-' && argv[1][1] == 'h') {
		fprintf(stderr, "Usage: %s [FASTA...]\n", argv[0]);
		return 1;
	}

	argv += 1;

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
			size_t len = strlen(ps.seq);

			char *rev = revcomp(ps.seq, len);
			char *name = NULL;
			int check = asprintf(&name, "%s revcomp", ps.name);
			if (check == -1) {
				errx(errno, "critical error.");
			}

			free(ps.name);
			free(ps.seq);
			ps.seq = rev;
			ps.name = name;
			print_seq(&ps);
			pfasta_seq_free(&ps);
		}

		if (l < 0) {
			warnx("%s: %s", file_name, pfasta_strerror(&pf));
			exit_code = EXIT_FAILURE;
			pfasta_seq_free(&ps);
		}

	fail:
		pfasta_free(&pf);
		close(file_descriptor);
	}

	return exit_code;
}

char *revcomp(const char *seq, size_t len) {
	if (!seq || !len) return NULL;
	char *rev = malloc(len + 1);
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
