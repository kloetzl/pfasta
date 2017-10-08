#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "common.h"
#include "pfasta.h"

int pfasta_print(int file_descriptor, const pfasta_seq *ps, int line_length) {
	if (file_descriptor < 0 || !ps || line_length <= 0) {
		errno = EINVAL;
		return -EINVAL;
	}

	int check;
	if (ps->comment) {
		check = dprintf(file_descriptor, ">%s %s\n", ps->name, ps->comment);
	} else {
		check = dprintf(file_descriptor, ">%s\n", ps->name);
	}

	if (check < 0) {
		return check;
	}

	const char *seq = ps->seq;

	for (ssize_t j; *seq; seq += j) {
		j = dprintf(file_descriptor, "%.*s\n", line_length, seq) - 1;
		if (j < 0) {
			return j;
		}
	}

	return 0;
}
