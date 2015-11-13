CFLAGS= -W -Wall -O3 -g -std=gnu99 -pedantic -ggdb
CPPFLAGS= -I src

EXECUTABLES= gc_content genFasta validate noop

LOGFILE= test.log

.PHONY: all clean check format
all: $(EXECUTABLES)

gc_content: examples/gc_content.o src/pfasta.o
	$(CC) $(CFLAGS) -o $@ $^

validate: examples/validate.o src/pfasta.o
	$(CC) $(CFLAGS) -o $@ $^

genFasta: test/genFasta.o test/pcg_basic.o
	$(CC) $(CFLAGS) -o $@ $^

noop: examples/noop.o src/pfasta.o
	$(CC) $(CFLAGS) -o $@ $^

format:
	clang-format -i src/*.c src/*.h

.PHONY: asan
asan: CC=clang
asan: CFLAGS= -W -Wall -O1 -g -ggdb -std=gnu99 -fsanitize=address
asan: CPPFLAGS= -I src
asan: all

fuzzer: test/fuzz.cxx asan
	clang++ -fsanitize=address -fsanitize-coverage=edge test/fuzz.cxx Fuzzer*.o src/pfasta.o -I src -o $@

clean:
	rm -f $(EXECUTABLES) src/*.o examples/*.o test/*.o $(LOGFILE) fuzzer


XFAIL= $(wildcard test/xfail*)
PASS= $(wildcard test/pass*)

.PHONY: $(PASS) $(XFAIL)

check: genFasta validate $(PASS) $(XFAIL)
	@ for LENGTH in 1 2 3 10 100 1000 10000; do \
		echo -n "testing with generated sequence of length $${LENGTH} … "; \
		(./genFasta -l "$${LENGTH}" | ./validate 2> $(LOGFILE) ) || \
		(echo " Unexpected error: $@\n See $(LOGFILE) for details." && exit 1); \
		echo "pass."; \
	done
	rm -f $(LOGFILE)

$(PASS): noop
	@echo -n "testing $@ … "
	@./noop $@ | diff -bB - $@ 2> $(LOGFILE) || \
		(echo " Unexpected error: $@\n See $(LOGFILE) for details." && exit 1)
	@echo "pass."

$(XFAIL): validate
	@echo -n "testing $@ … "
	@! ./validate $@ 2> $(LOGFILE) || \
		(echo " Unexpected pass: $@\n See $(LOGFILE) for details." && exit 1)
	@echo "expected fail."
