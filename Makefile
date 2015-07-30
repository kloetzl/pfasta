CFLAGS= -W -Wall -O3 -g -std=gnu99 -pedantic -ggdb # -fsanitize=address
CPPFLAGS= -I src

EXECUTABLES= gc_content genFasta validate

all: $(EXECUTABLES)

gc_content: examples/gc_content.o src/pfasta.o
	$(CC) $(CFLAGS) -o $@ $^

validate: examples/validate.o src/pfasta.o
	$(CC) $(CFLAGS) -o $@ $^

genFasta: test/genFasta.o test/pcg_basic.o
	$(CC) -o $@ $^

.PHONY: all clean check
clean:
	rm -f $(EXECUTABLES) src/*.o examples/*.o test/*.o

check: genFasta validate
	bash test/driver.sh
