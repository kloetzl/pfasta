#include <err.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "pcg_basic.h"

void usage(int exit_code);
void print_seq(double distance);

static size_t length = 1000;
static size_t line_length = 70;

static pcg32_random_t pcg32_base = PCG32_INITIALIZER;
static pcg32_random_t pcg32_mut = PCG32_INITIALIZER;

int main(int argc, char *argv[]) {
	double seqs[argc + 2];
	seqs[0] = 0.0; // base sequences
	size_t seq_n = 1;

	uint64_t seed = 0;

	int raw = 0;
	int check;
	while ((check = getopt(argc, argv, "d:hl:L:rs:")) != -1) {
		switch (check) {
		case 'd':
			seqs[seq_n++] = atof(optarg);
			break;
		case 'l': {
			const char *errstr;

			length = my_strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr) errx(1, "length is %s: %s", errstr, optarg);

			break;
		}
		case 'L': {
			const char *errstr;

			line_length = my_strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr) errx(1, "line length is %s: %s", errstr, optarg);

			if (!line_length) line_length = -1;
			break;
		}
		case 'r':
			raw = 1;
			break;
		case 's': {
			const char *errstr;

			seed = my_strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr) errx(1, "seed is %s: %s", errstr, optarg);

			break;
		}
		case 'h':
			usage(EXIT_SUCCESS);
		case '?': // intentional fallthrough
		default:
			usage(EXIT_FAILURE);
		}
	}

	if (line_length > length) {
		line_length = length;
	}

	if (seed == 0) {
		seed = time(NULL);
	}

	if (seq_n < 2) {
		seqs[seq_n++] = 0.1;
	}

	// getrandom(&seed, sizeof(seed), 0);
	pcg32_srandom_r(&pcg32_mut, seed, 1729);

	for (size_t i = 0; i < seq_n; i++) {
		pcg32_srandom_r(&pcg32_base, seed, 1729);

		printf(">S%zu\n", i);

		double dist = seqs[i];
		if (!raw && dist > 0) {
			dist = 0.75 - 0.75 * exp(-(4.0 / 3.0) * dist);
		}
		print_seq(dist);
	}

	return 0;
}

static const char *ACGT = "ACGT";
static const char *NO_A = "CGT";
static const char *NO_C = "AGT";
static const char *NO_G = "ACT";
static const char *NO_T = "ACG";

char base_acgt() {
	static unsigned state = 0;
	static unsigned counter = 0;
	unsigned idx;
	if (counter == 0) {
		state = pcg32_random_r(&pcg32_base);
		counter = sizeof(unsigned int) / 4;
	}
	idx = state & 3;
	state >>= 2;
	counter--;
	return ACGT[idx];
}

char mutate(char old) {
	unsigned idx = pcg32_boundedrand_r(&pcg32_mut, 3);
	switch (old) {
	case 'A':
		return NO_A[idx];
	case 'C':
		return NO_C[idx];
	case 'G':
		return NO_G[idx];
	case 'T':
		return NO_T[idx];
	}
	return 'X'; // cannot happen
}

void print_seq(double divergence) {
	char line[line_length + 1];
	line[line_length] = '\0';

	size_t nucleotides = length;
	size_t mutations = nucleotides * divergence;
	for (size_t i = length, j; i > 0; i -= j) {
		j = line_length;
		if (i < line_length) j = i;

		for (size_t k = 0; k < j; k++) {
			char c = base_acgt();

			if (pcg32_boundedrand_r(&pcg32_mut, nucleotides) < mutations) {
				c = mutate(c);
				mutations--;
			}

			line[k] = c;
			nucleotides--;
		}

		line[j] = '\0';
		puts(line);
	}
}

void usage(int exit_code) {
	static const char str[] = {
	    "sim [-l length] [-L line length] [-d dist] [-s seed]\n"
	    "Simulate a set of genomic sequences.\n\n"
	    "Options:\n"
	    "  -d dist     Simulate an additional sequence with given distance\n"
	    "  -h          Display help and exit\n"
	    "  -l length   Sequence length\n"
	    "  -L num      Set the maximum line length (0 to disable)\n"
	    "  -r          Raw distances, do not apply JC correction\n"
	    "  -s seed     Seed the PRNG\n" //
	};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}
