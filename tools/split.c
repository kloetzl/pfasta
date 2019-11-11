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
static int append = 0;

void usage(int exit_code);
void process(const char *file_name);

int main(int argc, char **argv) {

	int c;
	while ((c = getopt(argc, argv, "ad:hL:s:")) != -1) {
		switch (c) {
		case 'a':
			append = 1;
			break;
		case 'd':
			outdir = optarg;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		case 'L': {
			const char *errstr;

			line_length = my_strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr) errx(1, "line length is %s: %s", errstr, optarg);

			if (!line_length) line_length = -1;
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

	struct pfasta_parser pp = pfasta_init(file_descriptor);
	if (pp.errstr) errx(1, "%s: %s", file_name, pp.errstr);

	while (!pp.done) {
		struct pfasta_record pr = pfasta_read(&pp);
		if (pp.errstr) errx(2, "%s: %s", file_name, pp.errstr);

		char *out_file_name;

		int check =
		    asprintf(&out_file_name, "%s/%s%s", outdir, pr.name, suffix);
		if (check < 0) {
			err(errno, "building output path failed");
		}

		FILE *output = fopen(out_file_name, append ? "a" : "w");
		if (output == NULL) {
			err(errno, "couldn't open file %s", out_file_name);
		}

		int file_descriptor = fileno(output);
		pfasta_print(file_descriptor, &pr, line_length);

		fclose(output);
		free(out_file_name);
		pfasta_record_free(&pr);
	}

	pfasta_free(&pp);
	close(file_descriptor);
}

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: split [OPTIONS...] [FILE...]\n"
	    "Split a FASTA file into one per contained sequence. \n\n"
	    "Options:\n"
	    "  -a         Append to existing files\n"
	    "  -d DIR     Set the directory to put the new files in\n"
	    "  -h         Display help and exit\n"
	    "  -s SUFFIX  Set the file suffix (default: '.fasta')\n"
	    "  -L num     Set the maximum line length (0 to disable)\n"};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
