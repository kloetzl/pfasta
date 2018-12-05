VERSION=$(shell git describe)
SOVERSION=12

PREFIX?=/usr
BINDIR?=$(PREFIX)/bin
LIBDIR?=$(PREFIX)/lib
INCLUDEDIR?=$(PREFIX)/include
TOOLDIR?=$(LIBDIR)/pfasta/bin
VALIDATE?=./validate
FORMAT?=./format

SONAME=libpfasta.so.$(SOVERSION)
CFLAGS?= -O2 -g -std=gnu11 -ggdb -fPIC -finline-functions
CPPFLAGS?= -Wall -Wextra -D_FORTIFY_SOURCE=2
CPPFLAGS+= -Isrc -DVERSION="\"$(VERSION)\"" -D_GNU_SOURCE -DNDEBUG -DDEFAULT_PATH="\"$(TOOLDIR)\"" -Werror=implicit-function-declaration
LIBS+=-lm

ifeq "$(WITH_LIBBSD)" "1"
# path may require patching
LIBBSDINCLUDEDIR?=/usr/include/bsd
CPPFLAGS+=-isystem $(LIBBSDINCLUDEDIR) -DLIBBSD_OVERLAY
LIBS+=-lbsd
endif

TOOLS= acgt aln2dist aln2maf cchar concat fancy_info format gc_content n50 revcomp shuffle sim split validate pfasta
LOGFILE= test.log

.PHONY: all clean check dist distcheck clang-format
.PHONY: install install-dev install-lib install-tools uninstall
all: $(TOOLS) $(SONAME)

sim: tools/pcg_basic.o tools/sim.o

$(TOOLS): %: tools/common.o tools/%.o libpfasta.a
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) -L. -lpfasta

libpfasta.a: libpfasta.o
	$(AR) -crvs $@ $^
	ranlib $@

libpfasta.o: src/pfasta.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $^ -o $@

$(SONAME): libpfasta.o
	$(CC) -shared -Wl,-soname,$@ -o $@ $^

TARBALL=pfasta-$(VERSION).tar.gz
PROJECT_VERSION=pfasta-$(VERSION)
dist: $(TARBALL)

distcheck: dist
	tar -xzf $(TARBALL)
	$(MAKE) -C $(PROJECT_VERSION) WITH_LIBBSD=$(WITH_LIBBSD)
	$(MAKE) -C $(PROJECT_VERSION) WITH_LIBBSD=$(WITH_LIBBSD) check
	rm -rf $(PROJECT_VERSION)

install: install-tools install-lib install-dev

install-dev:
	install -D -t "$(INCLUDEDIR)" src/pfasta.h

install-lib: $(SONAME)
	install -D -t "$(LIBDIR)" $(SONAME)
	install -D -t "$(LIBDIR)" libpfasta.a
	ln -f -r -s "$(LIBDIR)/$(SONAME)" "$(LIBDIR)/libpfasta.so"

install-tools: $(TOOLS)
	install -D -t "${BINDIR}" pfasta
	install -D -t "${TOOLDIR}" $(TOOLS)

uninstall:
	$(RM) "${BINDIR}/pfasta"
	$(RM) "${LIBDIR}/libpfasta.so"
	$(RM) "${LIBDIR}/$(SONAME)"
	$(RM) -r "${TOOLDIR}"
	$(RM) -r "${LIBDIR}/pfasta" # may fail iff TOOLDIR was set
	$(RM) "${INCLUDEDIR}/pfasta.h"

$(TARBALL):
	mkdir -p "$(PROJECT_VERSION)"/{src,test,tools}
	cp Makefile LICENSE README.md "$(PROJECT_VERSION)"
	cp src/*.c src/*.h "$(PROJECT_VERSION)/src"
	cp test/*.c test/*.fa "$(PROJECT_VERSION)/test"
	cp tools/*.c tools/*.h "$(PROJECT_VERSION)/tools"
	tar -ca -f $@ $(PROJECT_VERSION)
	rm -rf $(PROJECT_VERSION)

clean:
	$(RM) $(TOOLS) fuzzer
	$(RM) src/*.o tools/*.o test/*.o *.o *.a $(LOGFILE)
	$(RM) *.tar.gz
	$(RM) libpfasta.*

fuzzer: test/fuzz.c src/pfasta.c
	clang -fsanitize=fuzzer -I src -o $@ $^

clang-format:
	clang-format -i tools/*.c src/*.c src/*.h


XFAIL= $(wildcard test/xfail*)
PASS= $(wildcard test/pass*)

.PHONY: $(PASS) $(XFAIL)

check: sim $(VALIDATE) $(PASS) $(XFAIL)
	@ for LENGTH in 1 2 3 10 100 1000 10000 16383 16384 16385; do \
		echo -n "testing with generated sequence of length $${LENGTH} … "; \
		(./sim -l "$${LENGTH}" | $(VALIDATE) 2> $(LOGFILE) ) || \
		(echo -e " Unexpected error: $@\n See $(LOGFILE) for details." && exit 1); \
		echo "pass."; \
	done
	rm -f $(LOGFILE)

$(PASS): $(FORMAT)
	@echo -n "testing $@ … "
	@$(FORMAT) $@ | diff -bB - $@ 2> $(LOGFILE) || \
		(echo -e " Unexpected error: $@\n See $(LOGFILE) for details." && exit 1)
	@echo "pass."

$(XFAIL): $(VALIDATE)
	@echo -n "testing $@ … "
	@! $(VALIDATE) $@ 2> $(LOGFILE) || \
		(echo -e " Unexpected pass: $@\n See $(LOGFILE) for details." && exit 1)
	@echo "expected fail."
