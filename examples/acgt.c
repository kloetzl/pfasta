#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfasta.h"

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
			char const *p = ps.seq;
			char *q = ps.seq;
			for(; *p; p++){
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
