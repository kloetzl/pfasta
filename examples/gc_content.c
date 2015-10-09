#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pfasta.h"

double gc(const pfasta_seq *ps);

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
			if (!firsttime) exit(0);

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
			warnx("%s: Parser initialization failed: %s", file_name,
			      pfasta_strerror(&pf));
			exit_code = EXIT_FAILURE;
			goto fail;
		}

		pfasta_seq ps;
		while ((l = pfasta_read(&pf, &ps)) == 0) {
			printf("%s\t%lf\n", ps.name, gc(&ps));
			pfasta_seq_free(&ps);
		}

		if (l < 0) {
			warnx("%s: Input parsing failed: %s", file_name,
			      pfasta_strerror(&pf));
			exit_code = EXIT_FAILURE;
			pfasta_seq_free(&ps);
		}

	fail:
		pfasta_free(&pf);
		close(file_descriptor);
	}

	return exit_code;
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
