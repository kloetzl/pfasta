# A pedantic FASTA Parser

Even though [FASTA](https://en.wikipedia.org/wiki/FASTA_format) is probably
one of the simplest data formats ever created, a surprising amount of things
can do wrong when trying to parse a FASTA file. `pfasta` was designed with pedantic error handling: It detects a lot of errors and also makes it easy to print useful warnings.

## Grammar

A FASTA file contains one or more sequences. Each sequence consists of a header line with a name and an optional comment. Then follow multiple lines with the data. Here is a regex explaining what sequences are considered valid.

    >[:graph:]+([^\n]*)\n
    ([:graph:]+\n)+
    \n*

`[:graph:]` refers to the character class defined by the respective C function `isgraph(3)`.

## API Usage

Copy the `pfasta.h` and `pfasta.c` files into your project. You then have access
to a number of simple functions to setup a parser and use it on files. For a complete example on their usage see [examples/gc_content.c](examples/gc_content.c).

```c
typedef struct pfasta_file {
	/* internal data */
} pfasta_file;
```

This structure holds a number of members to represent the state of the FASTA parser. Please make sure, that it is properly initialized before usage. Always free this structure when the parser is done.


```c
typedef struct pfasta_seq {
	char *name, *comment, *seq;
} pfasta_seq;
```

There is no magic to this structure. Its just a container of three strings. Feel free to duplicate or move them. But don't forget to free the structure after usage!

```c
int pfasta_parse( pfasta_file *, FILE *);
```

This function initializes a pfasta_file struct with a parser bound to a specific file. Iff successful, 0 is returned and the first parameter is properly initialized. On error a nonzero value is returned. A human-readable description of the problem can be obtained via `pfasta_sterror()`. In both cases the parser should be freed after usage.

```c
int pfasta_read( pfasta_file *, pfasta_seq *);
```

Using a properly initialized parser, this function can read FASTA sequences. These are stored in the simple structure passed via the second parameter. A nonzero return value indicates an error (`1` means `EOF`). In that case both the sequence and the parser are left in an undetermined state and should no longer be used, but freed.

```c
const char *pfasta_strerror( const pfasta_file *);
```

Like `strerror(3)` this functions returns a human readable description to the occurred error. This includes low-level IO errors as well as notifications about broken FASTA files.

```c
void pfasta_free( pfasta_file *);
void pfasta_seq_free( pfasta_seq *);
```

These two functions free the resources allocated by the structures above. After freeing the structure itself can be reused.

## License

This code is *open source* software released under the permissive ISC license. See the LICENSE file for details.

## Links

- A good alternative to pfasta that also parses FASTQ files is [kseq.h](https://github.com/lh3/seqtk).
- Please submit bugs at [GitHub](https://github.com/kloetzl/pfasta/issues)
