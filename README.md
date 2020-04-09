# A Pedantic FASTA Parser and Tool Set

`pfasta` is a tool set for handling FASTA files and also comes with a pedantic parser.

## Compilation and Installation

Compilation is simple.

    git clone https://github.com/kloetzl/pfasta.git
    cd pfasta
    make
    sudo make install

For increased error handling compile with [libbsd](https://libbsd.freedesktop.org/wiki/) support `make WITH_LIBBSD=1`. To change the installation directory use `make DESTDIR=/usr/local install`.

## Tool Set

After compilation the main directory will contain a set of tools. They are designed to behave well on the commandline.

 * `acgt`: Reduce residues to the four canonical bases.
 * `aln2dist`: Convert an alignment to a distance matrix.
 * `aln2maf`: Convert an alignment to MAF.
 * `cchar`: Count the number of nucleotides.
 * `concat`: Concatenate sequences.
 * `fancy_info`: Print a fancy report.
 * `format`: Format sequences.
 * `gc_content`: Determine the GC content.
 * `n50`: Compute the N50.
 * `revcomp`: Compute the reverse complement.
 * `shuffle`: Shuffle a set of sequences.
 * `sim`: Simulate a set of genetic sequences.
 * `split`: Split a FASTA file into multiple files on a sequence basis.
 * `validate`: Check if a file conforms to the grammar given below.

Therein is also a wrapper program `pfasta` which bundles all of the tools at installation. Use the following command

    pfasta format file.fasta

to format a file.

## Parser

Even though [FASTA](https://en.wikipedia.org/wiki/FASTA_format) is probably
one of the simplest data formats ever created, a surprising amount of things
can do wrong when trying to parse a FASTA file in C. `pfasta` was designed with pedantic error handling: It detects a lot of errors and also makes it easy to print useful warnings.

### Grammar

A FASTA file contains one or more sequences. Each sequence consists of a header line with a name and an optional comment. Then follow multiple lines with the data. Here is a regex explaining what sequences are considered valid.

    >[^[:space:]]+([^\n]*)\n
    ([-*a-zA-Z][^[:space:]]*[[:space:]]*)+

`[:space:]` refers to the character class defined by the respective C function `isspace(3)`.

### API Usage

To use the parser in your own tools, install the library and `#include <pfasta.h>`. You then have access to a number of simple functions to setup a parser and use it on files. For a complete example on their usage see [tools/gc_content.c](tools/gc_content.c). Don't forget to link with `-lpfasta`.

```c
struct pfasta_parser {
    const char *errstr;
    int done;
    /* internal data */
};
```

This structure holds a number of members to represent the state of the FASTA parser. Please make sure, that it is properly initialized before usage. Always free this structure when the parser is done.

```c
struct pfasta_record {
    char *name, *comment, *sequence;
    size_t name_length, comment_length, sequence_length;
};
```

There is no magic to this structure. Its just a container of three strings. Feel free to duplicate or move them. But don't forget to free the data after usage!

```c
struct pfasta_parser pfasta_parse(int);
```

This function initializes a `pfasta_parser` struct with a parser bound to a specific file descriptor. Iff an error occurred `errstr` is set to contain a suitable message. Otherwise you can read data from it as long as `done` isn't set. The parser should be freed after usage.

Please note that the user is responsible for opening the file descriptor as readable and closing after usage.

```c
struct pfasta_record pfasta_read( struct pfasta_parser *);
```

Using a properly initialized parser, this function can read FASTA sequences. These are stored in the simple structure and returned. On error, the `errstr` property of the parser is set.

```c
void pfasta_free( struct pfasta_parser *);
void pfasta_record_free( struct pfasta_record *);
```

These two functions free the resources allocated by the structures above.

If the preprocessor macro `PFASTA_NO_THREADS` is defined, the parser is not fully thread safe. It probably also is not thread safe with older compilers.

```c
const char *pfasta_version(void);
```

Get a string defining the version of the pfasta library.

## Releases

The tool set is not released as tarballs. Instead, versioning is done via git tags.

## License

This code is *open source* software released under the permissive ISC license. See the LICENSE file for details.

## Links

- A good alternative to pfasta that also parses FASTQ files is [kseq.h](https://github.com/lh3/seqtk).
- Another tool set written in Go is [seqkit](http://bioinf.shenwei.me/seqkit/).
- Please submit bugs at [GitHub](https://github.com/kloetzl/pfasta/issues)
