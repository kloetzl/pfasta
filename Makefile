VERSION=$(shell git describe)

SONAME=libpfasta.so.12
CFLAGS?= -O3 -g -std=gnu11 -ggdb -fPIC
CPPFLAGS?= -Wall -Wextra -D_FORTIFY_SOURCE=2
CPPFLAGS+= -Isrc -DVERSION=$(VERSION) -D_GNU_SOURCE
LIBS+=-lm
PREFIX?="/usr"
BINDIR?=$(PREFIX)/bin
LIBDIR?=$(PREFIX)/lib
INCLUDEDIR?=$(PREFIX)/include

ifeq "$(WITH_LIBBSD)" "1"
CPPFLAGS+=-isystem $(INCLUDEDIR)/bsd -DLIBBSD_OVERLAY
LIBS+=-lbsd
endif

TOOLS= acgt concat gc_content format revcomp shuffle validate cchar aln2dist split sim
LOGFILE= test.log

.PHONY: all clean check dist distcheck clang-format install install-lib install-tools
all: $(TOOLS) $(SONAME)

sim: tools/pcg_basic.o tools/sim.o

$(TOOLS): %: src/pfasta.o tools/common.o tools/%.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)


clang-format:
	clang-format -i tools/*.c src/*.c src/*.h

TARBALL=pfasta-$(VERSION).tar.gz
PROJECT_VERSION=pfasta-$(VERSION)
dist: $(TARBALL)

distcheck: dist
	tar -xzf $(TARBALL)
	$(MAKE) -C $(PROJECT_VERSION) WITH_LIBBSD=$(WITH_LIBBSD)
	$(MAKE) -C $(PROJECT_VERSION) WITH_LIBBSD=$(WITH_LIBBSD) check
	rm -rf $(PROJECT_VERSION)

install: install-tools install-lib

install-tools: $(TOOLS)
	mkdir -p "${BINDIR}"
	install pfasta "${BINDIR}"
	mkdir -p "${LIBDIR}/pfasta/bin"
	install -t "${LIBDIR}/pfasta/bin" $(TOOLS)

install-lib: $(SONAME)
	install src/pfasta.h $(INCLUDEDIR)
	install $(SONAME) $(LIBDIR)
	ln -s $(SONAME) libpfasta.so
	install libpfasta.so $(LIBDIR)

src/pfasta.o: src/pfasta.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $^ -o $@

$(SONAME): src/pfasta.o
	gcc -shared -Wl,-soname,$@ -o $@ $^

$(TARBALL):
	mkdir -p "$(PROJECT_VERSION)"/{src,test,tools}
	cp Makefile LICENSE README.md pfasta "$(PROJECT_VERSION)"
	cp src/*.c src/*.h "$(PROJECT_VERSION)/src"
	cp test/*.cxx test/*.c test/*.fa test/*.h "$(PROJECT_VERSION)/test"
	cp tools/*.c "$(PROJECT_VERSION)/tools"
	tar -ca -f $@ $(PROJECT_VERSION)
	rm -rf $(PROJECT_VERSION)

clean:
	rm -f $(TOOLS) fuzzer
	rm -f src/*.o tools/*.o test/*.o *.o $(LOGFILE)
	rm -f *.tar.gz
	rm -f libpfasta.*



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

check: sim validate $(PASS) $(XFAIL)
	@ for LENGTH in 1 2 3 10 100 1000 10000; do \
		echo -n "testing with generated sequence of length $${LENGTH} … "; \
		(./sim -l "$${LENGTH}" | ./validate 2> $(LOGFILE) ) || \
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
