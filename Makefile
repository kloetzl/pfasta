CFLAGS= -W -Wall -O3 -g -std=gnu99 -pedantic -ggdb
CPPFLAGS= -I src

all: gc_content

gc_content: examples/gc_content.o src/pfasta.o
	$(CC) $(CFLAGS) -o $@ $^

pfasta: src/main.o src/pfasta.o
	$(CC) -o $@ $^

.PHONY: clean
clean:
	rm -f src/*.o pfasta gc_content examples/*.o
