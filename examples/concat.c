#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfasta.h"

static size_t line_length = 70;

size_t strncpy_n(char *dest, const char *src, size_t n)
{
	size_t i = 0;
	for (; i < n && *src; ++i) {
		*dest++ = *src++;
	}
	return i;
}

void print_seq(const pfasta_seq *ps)
{
	char line[line_length + 1];
	const char *seq = ps->seq;

	for (size_t j; *seq; seq += j) {
		j = strncpy_n(line, seq, line_length);
		line[j] = '\0';
		puts(line);
	}
}

int main(int argc, const char *argv[])
{

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

		// print header
		const char *file_name_sep = strrchr(file_name, '/');
		if (!file_name_sep) {
			file_name_sep = file_name;
		} else {
			file_name_sep += 1; // skip /
		}
		const char *file_name_dot = strchr(file_name_sep, '.');
		if (!file_name_dot) {
			file_name_dot = strchr(file_name_sep, '\0');
		}

		printf(">%.*s\n", (int)(file_name_dot - file_name_sep), file_name_sep);

		// concat sequences
		pfasta_seq ps;
		while ((l = pfasta_read(&pf, &ps)) == 0) {
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
