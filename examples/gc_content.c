#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include "pfasta.h"

double gc(const pfasta_seq *ps);

int main(int argc, const char *argv[]) {

	if (argc == 2 && argv[1][0] == '-' && argv[1][1] == 'h') {
		fprintf(stderr, "Usage: %s [FASTA...]\n", argv[0]);
		return 1;
	}

	argv += 1;

	int firsttime = 1;

	for (;; firsttime = 0, argv++) {
		FILE *in;
		if (!*argv) {
			if (!firsttime)
				exit(0);

			in = stdin;
		} else {
			in = fopen(*argv, "r");
			if (!in)
				err(1, "%s", *argv);
		}

		int l;
		pfasta_file pf;
		if ((l = pfasta_parse(&pf, in)) != 0) {
			errx(1, "Parser initialization failed: %s\n", pfasta_strerror(&pf));
		}

		pfasta_seq ps;
		while ((l = pfasta_read(&pf, &ps)) == 0) {
			printf("%s\t%lf\n", ps.name, gc(&ps));
		}

		if (l < 0) {
			errx(1, "Input parsing failed: %s\n", pfasta_strerror(&pf));
		}

		pfasta_seq_free(&ps);
		pfasta_free(&pf);
		fclose(in);
	}

	return 0;
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
