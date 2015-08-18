#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include "pfasta.h"

int main(int argc, const char *argv[]) {

	if (argc == 2 && argv[1][0] == '-' && argv[1][1] == 'h') {
		fprintf(stderr, "Usage: %s [FASTA...]\n", argv[0]);
		return 1;
	}

	argv += 1;

	int firsttime = 1;
	int exit_code = EXIT_SUCCESS;

	for (;; firsttime = 0) {
		FILE *in;
		const char *filename;
		if (!*argv) {
			if (!firsttime) exit(exit_code);

			in = stdin;
			filename = "stdin";
		} else {
			filename = *argv++;
			in = fopen(filename, "r");
			if (!in) err(1, "%s", filename);
		}

		int l;
		pfasta_file pf;
		if ((l = pfasta_parse(&pf, in)) != 0) {
			warnx("%s: Parser initialization failed: %s", filename,
			      pfasta_strerror(&pf));
			exit_code = EXIT_FAILURE;
			goto fail;
		}

		pfasta_seq ps;
		while ((l = pfasta_read(&pf, &ps)) == 0) {
			pfasta_seq_free(&ps);
		}

		if (l < 0) {
			warnx("%s: Input parsing failed: %s", filename,
			      pfasta_strerror(&pf));
			exit_code = EXIT_FAILURE;
			pfasta_seq_free(&ps);
		}

	fail:
		pfasta_free(&pf);
		fclose(in);
	}

	return exit_code;
}
