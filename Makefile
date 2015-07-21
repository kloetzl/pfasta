CFLAGS= -W -Wall -O0 -g -std=gnu99 -pedantic

pfasta: src/main.o src/pfasta.o
	$(CC) -o $@ $^

.PHONY: clean
clean:
	rm -f src/*.o pfasta
