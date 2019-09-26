#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "pfasta.h"

int pfasta_print(int file_descriptor, const struct pfasta_record *pr,
                 int line_length) {
	if (file_descriptor < 0 || !pr) {
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

extern __attribute__((weak)) // may be supplied by libc
long long
strtonum(const char *numstr, long long minval, long long maxval,
         const char **errstrp);

long long my_strtonum(const char *numstr, long long minval, long long maxval,
                      const char **errstrp) {
	if (strtonum != NULL) {
		return strtonum(numstr, minval, maxval, errstrp);
	}

	errno = 0;
	if (errstrp) *errstrp = NULL;

	long long ll = strtoll(numstr, NULL, 10);
	if (errno || ll < minval || ll > maxval) {
		if (errstrp != NULL) {
			*errstrp = "invalid";
		}
	}

	return ll;
}

extern __attribute__((weak)) // may be supplied by libc
void *
reallocarray(void *ptr, size_t nmemb, size_t size);

void *my_reallocarray(void *ptr, size_t nmemb, size_t size) {
	if (reallocarray != NULL) {
		return reallocarray(ptr, nmemb, size);
	}

	// unsafe fallback
	return realloc(ptr, nmemb * size);
}
