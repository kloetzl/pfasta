#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "pfasta.h"

static size_t line_length = 70;

size_t strncpy_n(char *dest, const char *src, size_t n) {
	size_t i = 0;
	for (; i < n && *src; ++i) {
		*dest++ = *src++;
	}
	return i;
}

void print_seq(const pfasta_seq *ps) {
	printf(">%s", ps->name);
	if (ps->comment) {
		printf(" %s\n", ps->comment);
	} else {
		printf("\n");
	}

	char line[line_length + 1];
	const char *seq = ps->seq;

	for (size_t j; *seq; seq += j) {
		j = strncpy_n(line, seq, line_length);
		line[j] = '\0';
		puts(line);
	}
}

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

	srand(time(NULL) + getpid());

	for (size_t i = sv.size; i > 0; i--) {
		size_t j = rand() % i;
		sv_swap(i - 1, j);
	}

	for (size_t i = 0; i < sv.size; i++) {
		print_seq(&sv.data[i]);
	}

	sv_free();

	return exit_code;
}
