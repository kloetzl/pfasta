# A Pedantic FASTA Parser and Tool Set

`pfasta` is a tool set for handling FASTA files and also comes with a pedantic parser.

## Compilation and Installation

On Linux, compilation requires [libbsd](https://libbsd.freedesktop.org/wiki/). It may not be required on other systems.

    git clone https://github.com/kloetzl/pfasta.git
    make WITH_LIBBSD=1
    sudo make install

## Tool Set

After compilation the main directory will contain a set of tools. They are designed to behave well on the commandline.

 * `acgt`: Reduce residues to the four canonical bases.
 * `aln2dist`: Convert an alignment to a distance matrix.
 * `cchar`: Count the number of nucleotides.
 * `concat`: Concatenate sequences.
 * `format`: Format sequences.
 * `gc_content`: Determine the GC content.
 * `genFasta`: Simulate a set of genomic sequences.
 * `revcomp`: Compute the reverse complement.
 * `shuffle`: Shuffle a set of sequences.
 * `split`: Split a FASTA file into multiple files on a sequence basis.
 * `validate`: Check if a file conforms to the grammar given below.

Therein is also a wrapper script `pfasta` which bundles all of the tools at installation. Use the following command

    pfasta format file.fa

to format a file.

## Parser

Even though [FASTA](https://en.wikipedia.org/wiki/FASTA_format) is probably
one of the simplest data formats ever created, a surprising amount of things
can do wrong when trying to parse a FASTA file in C. `pfasta` was designed with pedantic error handling: It detects a lot of errors and also makes it easy to print useful warnings.

### Grammar

A FASTA file contains one or more sequences. Each sequence consists of a header line with a name and an optional comment. Then follow multiple lines with the data. Here is a regex explaining what sequences are considered valid.

    >[:graph:]+([^\n]*)\n
    ([:graph:]+\n)+
    \n*

`[:graph:]` refers to the character class defined by the respective C function `isgraph(3)`.

### API Usage

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
int pfasta_parse( pfasta_file *, int);
```

This function initializes a pfasta_file struct with a parser bound to a specific file descriptor. Iff successful, 0 is returned and the first parameter is properly initialized. On error a nonzero value is returned. A human-readable description of the problem can be obtained via `pfasta_sterror()`. In both cases the parser should be freed after usage.

Please note that the user is responsible for opening the file descriptor as readable and closing after usage.

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

## Releases

The tool set is not released as tarballs. Instead, versioning is done via git tags. To integrate the library in your project, just copying the `pfasta.*` files is the preferred way, at the moment.

## License

This code is *open source* software released under the permissive ISC license. See the LICENSE file for details.

## Links

- A good alternative to pfasta that also parses FASTQ files is [kseq.h](https://github.com/lh3/seqtk).
- Another tool set written in Go is [seqkit](http://bioinf.shenwei.me/seqkit/).
- Please submit bugs at [GitHub](https://github.com/kloetzl/pfasta/issues)
