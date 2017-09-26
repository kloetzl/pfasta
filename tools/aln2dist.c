#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "pfasta.h"

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
		// TODO: check for multiplication overflow
		size_t new_cap = (sv.capacity / 2) * 3;
		sv.data = realloc(sv.data, new_cap * sizeof(pfasta_seq));
		if (!sv.data) err(errno, "realloc failed");
		sv.capacity = new_cap;
		sv.data[sv.size++] = ps;
	}
}

void sv_free() {
	for (size_t i = 0; i < sv.size; i++) {
		pfasta_seq_free(&sv.data[i]);
	}
	free(sv.data);
}

void sv_swap(size_t i, size_t j) {
	pfasta_seq temp = sv.data[i];
	sv.data[i] = sv.data[j];
	sv.data[j] = temp;
}

double compare(const pfasta_seq* subject, const pfasta_seq *query) {
	size_t mutations = 0;
	const char *s = subject->seq;
	const char *q = query->seq;
	size_t length = strlen(s);

	for (size_t i = 0; i < length; i++) {
		if (s[i] != q[i]) {
			mutations++;
		}
	}

	return (double)mutations / length;
}

int main(int argc, const char *argv[]) {

	if (argc == 2 && argv[1][0] == '-' && argv[1][1] == 'h') {
		fprintf(stderr, "Usage: %s [FASTA...]\n", argv[0]);
		return 1;
	}

	argv += 1;

	sv_init();

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

		pfasta_seq ps;
		while ((l = pfasta_read(&pf, &ps)) == 0) {
			sv_emplace(ps);
			// pfasta_seq_free(&ps);
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

	double *DD = malloc(sv.size * sv.size * sizeof(*DD));
	#define D(X,Y) (DD[(X) * sv.size + (Y)])

	for (size_t i = 0; i < sv.size; i++) {
		D(i,i) = 0.0;
		for (size_t j = 0; j < i; j++) {
			D(i,j) = D(j,i) = compare(&sv.data[i], &sv.data[j]);
		}
	}

	printf("%zu\n", sv.size);
	for (size_t i = 0; i < sv.size; i++) {
		printf("%-10s", sv.data[i].name);
		for (size_t j = 0; j < sv.size; j++) {
			printf(" %1.6e", D(i,j));
		}
		printf("\n");
	}

	free(DD);
	sv_free();

	return exit_code;
}
