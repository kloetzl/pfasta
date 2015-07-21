# A pedantic FASTA Parser

Even though [FASTA](https://en.wikipedia.org/wiki/FASTA_format) is probably
one of the simplest data formats ever created, a surprising amount of things
can do wrong when trying to parse a FASTA file. `pfasta` was designed with pedantic error handling: It detects a lot of errors and also makes it easy to print useful warnings.

## Usage

Copy the `pfasta.h` and `pfasta.c` files into your project. You then have access
to a number of simple functions to setup a parser and use it on files. An example says more than a thousand words:

```c
#include <stdio.h>
#include "pfasta.h"

int main(int argc, char const *argv[])
{
	if (argc == 1) {
		fprintf(stderr, "Usage: %s FASTA...\n", argv[0]);
		return 1;
	}

	argc -= 1, argv += 1;

	while (argc--){
		FILE* in = fopen(*argv++, "r");

		pfasta_file pf;
		pfasta_parse( &pf, in);

		int l;
		pfasta_seq ps;
		while ((l = pfasta_read( &pf, &ps)) == 0) {
			printf(">%s\n%9.9s\n", ps.name, ps.seq);
		}

		if( l < 0){
			fprintf(stderr, "Input parsing failed: %s\n", pfasta_strerror(&pf));
			return 1;
		}

		pfasta_seq_free(&ps);
		pfasta_free(&pf);
		fclose(in);
	}

	return 0;
}
```

## License

This code is *open source* software released under the permissive ISC license. See the LICENSE file for details.
