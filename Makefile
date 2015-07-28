CFLAGS= -W -Wall -O3 -g -std=gnu99 -pedantic -ggdb
CPPFLAGS= -I src

EXECUTABLES= gc_content genFasta

all: $(EXECUTABLES)

gc_content: examples/gc_content.o src/pfasta.o
	$(CC) $(CFLAGS) -o $@ $^

genFasta: test/genFasta.o test/pcg_basic.o
	$(CC) -o $@ $^

.PHONY: clean
clean:
	rm -f $(EXECUTABLES) src/*.o examples/*.o test/*.o
