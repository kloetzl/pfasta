#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "pfasta.h"

#define BOLD "\033[1m"
#define FAINT "\033[2m"
#define NORMAL "\033[0m"
#define GREY "\033[90m"

void count(const struct pfasta_record *);
void print_counts(const char *name, const char *comment, const size_t *counts);
void usage(int exit_code);
void process(const char *file_name);

// const int CHARS = 128;
#define CHARS 128
size_t counts_total[CHARS];
size_t counts_local[CHARS];

enum { NONE = 0, SPLIT = 1, CASE_INSENSITIVE = 2 } FLAGS = 0;

int main(int argc, char **argv) {
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

static const char blocks_rev[] = "▉▊▋▌▍▎▏\0\0\0\0";
static const char blocks_fwd[] = " ▏▎▍▌▋▊▉";
static const char all_black[] = "████████";

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
			print_counts(pr.name, pr.comment, counts_local);
			bzero(counts_local, sizeof(counts_local));
		}
		pfasta_record_free(&pr);
		for (size_t i = 0; i < CHARS; i++) {
			counts_total[i] += counts_local[i];
		}
	}

	if (!(FLAGS & SPLIT)) {
		print_counts(file_name, NULL, counts_total);
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

void print_counts(const char *name, const char *comment, const size_t *counts) {
	size_t local_length = 0;
	size_t local_max = 0;
	for (size_t i = 0; i < CHARS; i++) {
		local_length += counts[i];
		if (counts[i] > local_max) {
			local_max = counts[i];
		}
	}

	double scale = local_length / (double)local_max;

	printf("\n>" BOLD "%s\n" NORMAL, name);
	if (comment) printf(FAINT " %s\n" NORMAL, comment);

	printf("\n");

	for (size_t i = 0; i < CHARS; i++) {
		if (counts[i]) {
			size_t block_bytes = strlen("▉"); // length of one block
			double rel_count = (double)counts[i] / local_length;
			double count_scaled = scale * rel_count;
			size_t full_blocks = (size_t)(count_scaled * 8);
			size_t part_blocks = (size_t)(count_scaled * 8 * 8) & 7;
			size_t offset = full_blocks * block_bytes;

			char blocks[block_bytes * 10];
			memcpy(blocks, all_black, block_bytes * 8 + 1);
			size_t offset_inner = 7 - part_blocks;
			memcpy(blocks + offset, blocks_rev + block_bytes * offset_inner,
			       block_bytes);
			blocks[offset + block_bytes] = '\0';

			printf(" %c %10zu  %5.1lf%% " GREY "%s" NORMAL "\n", (char)i,
			       counts[i], rel_count * 100, blocks);
		}
	}

	printf(GREY " Σ %10zu  100.0%%\n\n" NORMAL, local_length);
}

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: info [OPTIONS...] [FILE...]\n"
	    "Count the residues. When FILE is '-' read from standard input.\n\n"
	    "Options:\n"
	    "  -h         Display help and exit\n"
	    "  -i         Ignore case\n"
	    "  -s         Print output per individual sequence\n" //
	};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
