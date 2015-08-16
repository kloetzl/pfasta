CFLAGS= -W -Wall -O3 -g -std=gnu99 -pedantic -ggdb # -fsanitize=address
CPPFLAGS= -I src

EXECUTABLES= gc_content genFasta validate

LOGFILE= test.log

.PHONY: all clean check
all: $(EXECUTABLES)

gc_content: examples/gc_content.o src/pfasta.o
	$(CC) $(CFLAGS) -o $@ $^

validate: examples/validate.o src/pfasta.o
	$(CC) $(CFLAGS) -o $@ $^

genFasta: test/genFasta.o test/pcg_basic.o
	$(CC) $(CFLAGS) -o $@ $^


clean:
	rm -f $(EXECUTABLES) src/*.o examples/*.o test/*.o $(LOGFILE)


XFAIL= $(wildcard test/xfail*)
PASS= $(wildcard test/pass*)

.PHONY: $(PASS) $(XFAIL)

check: genFasta validate $(PASS) $(XFAIL)
	@ for LENGTH in 1 2 3 10 100 1000 10000; do \
		echo -n "testing with generated sequence of length $${LENGTH} … "; \
		(./genFasta -l "$${LENGTH}" | ./validate) || \
		(echo " Unexpected error: $@\n See $(LOGFILE) for details." && exit 1); \
		echo "pass."; \
	done
	rm -f $(LOGFILE)

$(PASS): validate
	@echo -n "testing $@ … "
	@./validate $@ &> $(LOGFILE) || \
		(echo " Unexpected error: $@\n See $(LOGFILE) for details." && exit 1)
	@echo "pass."

$(XFAIL): validate
	@echo -n "testing $@ … "
	@! ./validate $@ &> $(LOGFILE) || \
		(echo " Unexpected pass: $@\n See $(LOGFILE) for details." && exit 1)
	@echo "expected fail."
