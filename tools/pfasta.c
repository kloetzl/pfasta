#include "pfasta.h"
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>

char *get_self_path_mac();
#endif

struct description {
	char *name, *text;
};

enum { F_NONE, F_VERBOSE = 1 };
int FLAGS = 0;

static const char *default_path = DEFAULT_PATH;

static const struct description descriptions[] = {
    {"acgt", "Filter the nucleotides."},
    {"aln2dist", "Compute sequence distances from an alignment."},
    {"aln2maf", "Convert an alignment to the Multiple Alignment Format (MAF)."},
    {"cchar", "Count the residues."},
    {"concat", "Concatenate multiple Fasta files into one sequence."},
    {"fancy_info", "Print a fancy report."},
    {"format", "Format the input sequence."},
    {"gc_content", "Compute the GC content of each sequence."},
    {"n50", "Compute the N50."},
    {"revcomp", "Print the reverse complement of each sequence."},
    {"shuffle", "Shuffle a set of sequences."},
    {"sim", "Simulate a set of genomic sequences."},
    {"split", "Split a FASTA file into one per contained sequence."},
    {"validate", "Verify that the input is a valid FASTA file."},
    {0, 0}};

char *tool_is_exec(const char *path, const char *tool);
char *get_self_path();
void usage(int exit_status);
void list();
void version();

int main(int argc, char *const *argv) {
	int main_args = 1;
	while (main_args < argc && argv[main_args][0] == '-')
		main_args++;

	while (1) {
		int c = getopt(main_args, argv, "hlvV");
		if (c == -1) {
			break;
		} else if (c == 'h') {
			usage(EXIT_SUCCESS);
		} else if (c == 'l') {
			list();
		} else if (c == 'v') {
			version();
		} else if (c == 'V') {
			FLAGS |= F_VERBOSE;
		} else {
			usage(EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) usage(EXIT_FAILURE);
	const char *tool = argv[0];

	const struct description *desc = descriptions;
	while (desc->name && strcmp(desc->name, tool) != 0)
		desc++;

	if (!desc->name) {
		errx(1, "'%s' is not a tool", tool);
	}

	char *path = tool_is_exec(default_path, tool);
	if (!path) {
		const char *dir = get_self_path();
		path = tool_is_exec(dir, tool);
	}

#ifdef __APPLE__
	if (!path) {
		const char *dir = get_self_path_mac();
		path = tool_is_exec(dir, tool);
	}
#endif

	int check = execv(path, argv);
	if (check == -1) err(errno, "%s: executing failed", tool);

	return 0;
}

char *tool_is_exec(const char *dir, const char *tool) {
	char *path = NULL;

	int check = asprintf(&path, "%s/%s", dir, tool);
	if (check == -1) {
		if (FLAGS & F_VERBOSE) warn("could not create path");
		return NULL;
	}

	check = access(path, X_OK);
	if (check == -1) {
		if (FLAGS & F_VERBOSE) warn("%s: cannot be executed", path);
		return NULL;
	}

	return path;
}

char *get_self_path() {
	static const char self_link[] = "/proc/self/exe";

	static char buffer[1024];
	ssize_t check = readlink(self_link, buffer, sizeof(buffer));

	if (check == -1) {
		if (FLAGS & F_VERBOSE) warn("getting own path failed");
		return NULL;
	}

	// append null byte
	if (check == 1024) check = 1023;
	buffer[check] = '\0';

	dirname(buffer);

	return strdup(buffer);
}

#ifdef __APPLE__
char *get_self_path_mac(void) {
	char buffer[1024];
	uint32_t size = sizeof(buffer);
	int check = _NSGetExecutablePath(buffer, &size);

	if (check) {
		if (FLAGS & F_VERBOSE) warn("getting own path failed (mac)");
		return NULL;
	}

	char *str = malloc(size);
	realpath(buffer, str);
	str = dirname(str); // memory leak

	return strdup(str);
}
#endif

void list() {
	const struct description *desc = descriptions;
	while (desc->name) {
		fprintf(stderr, "%-10.10s -- %s\n", desc->name, desc->text);
		desc++;
	}
	exit(0);
}

void usage(int exit_code) {
	static const char str[] = {
	    "Usage: pfasta [OPTIONS] tool [OPTIONS...] [FILE...]\n"
	    "The PFASTA suite of tools.\n\n"
	    "Options:\n"
	    "  -h         Display help and exit\n"
	    "  -l         List available tools\n"
	    "  -v         Output version information\n"
	    "  -V         Be verbose\n"
	    "\nThe default tool path is " DEFAULT_PATH ".\n"};

	fprintf(exit_code == EXIT_SUCCESS ? stdout : stderr, str);
	exit(exit_code);
}

void version() {
	static const char str[] = {
	    "pfasta %s\n"
	    "Copyright (c) 2015 - 2018, Fabian Klötzl "
	    "<fabian-pfasta@kloetzl.info>\n"
	    "ISC License\n\n"
	    "library version: %s\n" //
	};

	printf(str, VERSION, pfasta_version());
	exit(EXIT_SUCCESS);
}
