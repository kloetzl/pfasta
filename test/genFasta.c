/**
 * This program can create genome sequences with a specific distance.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

#include "pcg_basic.h"

void usage(void);
void print_seq(double distance);

static size_t length = 1000;
static size_t line_length = 70;

static pcg32_random_t pcg32_base = PCG32_INITIALIZER;
static pcg32_random_t pcg32_mut = PCG32_INITIALIZER;

int main(int argc, char *argv[]) {
	// in the worst case half of the arguments are divergences
	double seqs[argc / 2];
	size_t seq_n = 1;
	seqs[0] = 0.0;

	int check;
	while ((check = getopt(argc, argv, "l:L:d:")) != -1) {
		switch (check) {
		case 'l':
			length = atoi(optarg);
			break;
		case 'L':
			line_length = atoi(optarg);
			break;
		case 'd':
			seqs[seq_n++] = atof(optarg);
			break;
		case '?': // intentional fallthrough
		default:
			usage();
		}
	}

	if (seq_n < 2) {
		seqs[seq_n++] = 0.1;
	}

	uint64_t seed = time(NULL);
	// getrandom(&seed, sizeof(seed), 0);

	for (size_t i = 0; i < seq_n; i++) {
		pcg32_srandom_r(&pcg32_base, seed, 1729);
		printf(">S%zu\n", i);
		print_seq(seqs[i]);
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

void usage() {
	static const char *str = {
	    "genFasta [-l length] [-L line length] [-d dist]...\n"
	};
	errx(1, "%s", str);
}
