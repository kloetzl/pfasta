VERSION=$(shell git describe)

CFLAGS?= -O3 -g -std=gnu11 -ggdb -fPIC
CPPFLAGS?= -Wall -Wextra -D_FORTIFY_SOURCE=2
CPPFLAGS+= -Isrc -DVERSION=$(VERSION)
PREFIX?=""
BINDIR?=$(PREFIX)/bin
LIBDIR?=$(PREFIX)/lib

TOOLS= acgt concat gc_content format revcomp shuffle validate cchar aln2dist
LOGFILE= test.log

.PHONY: all clean check dist distcheck format install install-lib install-tools
all: $(TOOLS) genFasta

$(TOOLS): %: src/pfasta.o tools/%.o
	$(CC) $(CFLAGS) -o $@ $^

genFasta: test/pcg_basic.o test/genFasta.o
	$(CC) $(CFLAGS) -o $@ $^

clang-format:
	clang-format -i tools/*.c src/*.c src/*.h

TARBALL=pfasta-$(VERSION).tar.gz
PROJECT_VERSION=pfasta-$(VERSION)
dist: $(TARBALL)

distcheck: dist
	tar -xzf $(TARBALL)
	$(MAKE) -C $(PROJECT_VERSION)
	$(MAKE) -C $(PROJECT_VERSION) check
	rm -rf $(PROJECT_VERSION)

install: install-tools install-lib

install-tools: $(TOOLS)
	mkdir -p "${BINDIR}"
	install pfasta "${BINDIR}"
	mkdir -p "${LIBDIR}/pfasta/bin"
	install -t "${LIBDIR}/pfasta/bin" $(TOOLS)

install-lib: libpfasta.so

libpfasta.so:
	libtool --mode=compile --tag=RELEASE $(CC) $(CPPFLAGS) $(CFLAGS) -c src/pfasta.c

$(TARBALL):
	mkdir -p "$(PROJECT_VERSION)"/{src,test,tools}
	cp Makefile LICENSE README.md pfasta "$(PROJECT_VERSION)"
	cp src/*.c src/*.h "$(PROJECT_VERSION)/src"
	cp test/*.cxx test/*.c test/*.fa test/*.h "$(PROJECT_VERSION)/test"
	cp tools/*.c "$(PROJECT_VERSION)/tools"
	tar -ca -f $@ $(PROJECT_VERSION)
	rm -rf $(PROJECT_VERSION)

clean:
	rm -f $(TOOLS) genFasta fuzzer
	rm -f src/*.o tools/*.o test/*.o $(LOGFILE)
	rm -f *.tar.gz



.PHONY: asan
asan: CC=clang
asan: CFLAGS= -W -Wall -O1 -g -ggdb -std=gnu99 -fsanitize=address
asan: CPPFLAGS= -I src
asan: all

fuzzer: test/fuzz.cxx asan
	clang++ -fsanitize=address -fsanitize-coverage=edge test/fuzz.cxx Fuzzer*.o src/pfasta.o -I src -o $@


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

$(PASS): format
	@echo -n "testing $@ … "
	@./format $@ | diff -bB - $@ 2> $(LOGFILE) || \
		(echo " Unexpected error: $@\n See $(LOGFILE) for details." && exit 1)
	@echo "pass."

$(XFAIL): validate
	@echo -n "testing $@ … "
	@! ./validate $@ 2> $(LOGFILE) || \
		(echo " Unexpected pass: $@\n See $(LOGFILE) for details." && exit 1)
	@echo "expected fail."
