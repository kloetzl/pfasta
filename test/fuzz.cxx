#include <unistd.h>
#include <stdlib.h>

extern "C" {
#include "pfasta.h"
}

extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, unsigned long size) {
	int fds[2];
	int check = pipe(fds);
	if(!check) return 0;

	int fd_write = fds[1];
	int fd_read = fds[0];

	int exit_code = 0;

	// write all the data.
	write(fd_write, data, size);
	close(fd_write);

	int l;
	pfasta_file pf;
	if ((l = pfasta_parse(&pf, fd_read)) != 0) {
		exit_code = EXIT_FAILURE;
		goto fail;
	}

	pfasta_seq ps;
	while ((l = pfasta_read(&pf, &ps)) == 0) {
		pfasta_seq_free(&ps);
	}

	if (l < 0) {
		exit_code = EXIT_FAILURE;
		pfasta_seq_free(&ps);
	}

fail:
	pfasta_free(&pf);
	close(fd_read);
	return 0;
}
