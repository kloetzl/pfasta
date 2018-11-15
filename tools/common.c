#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "common.h"
#include "pfasta.h"

int pfasta_print(int file_descriptor, const struct pfasta_record *pr, int line_length) {
	if (file_descriptor < 0 || !pr || line_length <= 0) {
		errno = EINVAL;
		return -EINVAL;
	}

	int check;
	if (pr->comment) {
		check = dprintf(file_descriptor, ">%s %s\n", pr->name, pr->comment);
	} else {
		check = dprintf(file_descriptor, ">%s\n", pr->name);
	}

	if (check < 0) {
		return check;
	}

	const char *seq = pr->sequence;

	for (ssize_t j; *seq; seq += j) {
		j = dprintf(file_descriptor, "%.*s\n", line_length, seq) - 1;
		if (j < 0) {
			return j;
		}
	}

	return 0;
}
