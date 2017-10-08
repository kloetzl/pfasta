#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "pfasta.h"

static int line_length = 70;
static char *suffix = ".fasta";
static char *outdir = "./";

void usage(int exit_code);
void process(const char *file_name);

int main(int argc, char **argv) {

	int c;
	while ((c = getopt(argc, argv, "hd:L:s:")) != -1) {
		switch (c) {
		case 'h':
			usage(EXIT_SUCCESS);
		case 'd':
			outdir = optarg;
			break;
		case 'L': {
			const char *errstr;

			line_length = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr) errx(1, "line length is %s: %s", errstr, optarg);

			if (!line_length) line_length = INT_MAX;
			break;
		}
		case 's':
			suffix = optarg;
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
		char *out_file_name;

		int check =
		    asprintf(&out_file_name, "%s/%s%s", outdir, ps.name, suffix);
		if (check < 0) {
			err(errno, "building output path failed");
		}

		FILE *output = fopen(out_file_name, "a+");
		if (output == NULL) {
			err(errno, "couldn't open file %s", out_file_name);
		}

		int file_descriptor = fileno(output);
		pfasta_print(file_descriptor, &ps, line_length);

		fclose(output);
		free(out_file_name);
		pfasta_seq_free(&ps);
	}

	if (l < 0) {
		errx(1, "%s: %s", file_name, pfasta_strerror(&pf));
	}

	pfasta_free(&pf);
	close(file_descriptor);
}

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: split [OPTIONS...] [FILE...]\n"
	    "Split a FASTA file into one per contained sequence. \n\n"
	    "Options:\n"
	    "  -h         Display help and exit\n"
	    "  -d DIR     Set the directory to put the new files in\n"
	    "  -s SUFFIX  Set the file suffix (default: '.fasta')\n"
	    "  -L num     Set the maximum line length (0 to disable)\n"};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
