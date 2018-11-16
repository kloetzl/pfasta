#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <err.h>
#include <stdint.h>

#include "pfasta.h"

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
	int filedes[2];
	int check = pipe(filedes);
	if (check == -1) err(errno, "pipe");

	ssize_t bytes = write(filedes[1], Data, Size);
	if (bytes == -1) err(errno, "write failed");
	close(filedes[1]);

	struct pfasta_parser pp = pfasta_init(filedes[0]);
	// if (pp.errstr) warn("%s", pp.errstr);

	while (!pp.done) {
		struct pfasta_record pr = pfasta_read(&pp);
		// if (pp.errstr) warn("%s", pp.errstr);

		// if (pr.comment) {
		// 	printf(">%s %s\n%zu\n", pr.name, pr.comment, pr.sequence_length);
		// } else {
		// 	printf(">%s\n%zu\n", pr.name, pr.sequence_length);
		// }
		pfasta_record_free(&pr);
	}

	pfasta_free(&pp);
	close(filedes[0]);

	return 0; // Non-zero return values are reserved for future use.
}
