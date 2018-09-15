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

void count(const pfasta_seq *);
void print_counts(const char *name, const size_t *counts);
void usage(int exit_code);
void process(const char *file_name);

struct seq_vector {
	pfasta_seq *data;
	size_t size;
	size_t capacity;
} sv;

void sv_init() {
	sv.data = malloc(4 * sizeof(pfasta_seq));
	sv.size = 0;
	sv.capacity = 4;
	if (!sv.data) err(errno, "malloc failed");
}

void sv_emplace(pfasta_seq ps) {
	if (sv.size < sv.capacity) {
		sv.data[sv.size++] = ps;
	} else {
		sv.data =
		    reallocarray(sv.data, sv.capacity / 2, 3 * sizeof(pfasta_seq));
		if (!sv.data) err(errno, "realloc failed");
		// reallocarray would return NULL, if mult would overflow
		sv.capacity = (sv.capacity / 2) * 3;
		sv.data[sv.size++] = ps;
	}
}

void sv_free() {
	for (size_t i = 0; i < sv.size; i++) {
		pfasta_seq_free(&sv.data[i]);
	}
	free(sv.data);
}

// const int CHARS = 128;
#define CHARS 128
size_t counts_total[CHARS];
size_t counts_local[CHARS];

enum { NONE = 0, SPLIT = 1, CASE_INSENSITIVE = 2 } FLAGS = 0;

int main(int argc, char **argv) {
	int c;
	while ((c = getopt(argc, argv, "h")) != -1) {
		if (c == 'h')
			usage(EXIT_SUCCESS);
		else
			usage(EXIT_FAILURE);
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

	pfasta_file pf;
	int l = pfasta_parse(&pf, file_descriptor);
	if (l != 0) {
		errx(1, "%s: %s", file_name, pfasta_strerror(&pf));
	}

	pfasta_seq ps;
	while ((l = pfasta_read(&pf, &ps)) == 0) {
		count(&ps);

		size_t local_length = 0;
		size_t local_max = 0;
		for (size_t i = 0; i < CHARS; i++) {
			local_length += counts_local[i];
			counts_total[i] += counts_local[i];
			if (counts_local[i] > local_max) {
				local_max = counts_local[i];
			}
		}

		double scale = local_length / (double)local_max;

		printf("\n");
		printf(">" BOLD "%s\n" NORMAL, ps.name);
		if (ps.comment) printf(FAINT " %s\n" NORMAL, ps.comment);

		printf("\n");

		for (size_t i = 0; i < CHARS; i++) {
			if (counts_local[i]) {
				size_t block_bytes = strlen("▉"); // length of one block
				double rel_count = (double)counts_local[i] / local_length;
				double count_scaled = scale * rel_count;
				size_t full_blocks = (size_t)(count_scaled * 8);
				size_t part_blocks = (size_t)(count_scaled * 8 * 8) & 7;
				size_t offset = full_blocks * block_bytes;

				char blocks[block_bytes * 10];
				memcpy(blocks, all_black, block_bytes * 8 + 1);
				size_t offset_inner = 7 - part_blocks;
				// printf("%zu %zu %zu\n", offset, part_blocks, offset_inner);
				memcpy(blocks + offset, blocks_rev + block_bytes * offset_inner,
				       block_bytes);
				blocks[offset + block_bytes] = '\0';

				printf(" %c %10zu  %5.1lf%% " GREY "%s" NORMAL "\n", (char)i,
				       counts_local[i], rel_count * 100, blocks);
			}
		}
		printf(GREY " Σ %10zu  100.0%%\n" NORMAL, local_length);

		printf("\n");
		pfasta_seq_free(&ps);
	}

	if (l < 0) {
		errx(1, "%s: %s", file_name, pfasta_strerror(&pf));
	}

	pfasta_free(&pf);
	close(file_descriptor);
}

void count(const pfasta_seq *ps) {
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

void print_counts(const char *name, const size_t *counts) {
	printf(">%s\n", name);

	size_t sum = 0;
	for (int i = 1; i < CHARS; ++i) {
		if (counts[i]) {
			printf("%c:\t%8zu\n", i, counts[i]);
			sum += counts[i];
		}
	}
	printf("# sum:\t%8zu\n", sum);
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
