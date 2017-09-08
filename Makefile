CFLAGS= -Wall -Wextra -O3 -g -std=gnu99 -pedantic -ggdb
CPPFLAGS= -I src

EXAMPLES= acgt concat gc_content noop revcomp shuffle validate
LOGFILE= test.log

.PHONY: all clean check format
all: $(EXAMPLES) genFasta

$(EXAMPLES): %: src/pfasta.o examples/%.o
	$(CC) $(CFLAGS) -o $@ $^

genFasta: test/pcg_basic.o test/genFasta.o
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
