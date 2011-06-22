ARCH=$(shell uname -s)-$(shell uname -m)
STABLE=f26b7f1f999067bfa44608cb282e26c28b49eaf8
all: minigrace

SOURCEFILES = compiler.gc util.gc ast.gc genllvm.gc lexer.gc parser.gc

gracelib.o: gracelib.c
	clang -emit-llvm -c gracelib.c

unicode.gso: unicode.c unicodedata.h
	gcc -fPIC -shared -o unicode.gso unicode.c

l1/minigrace: known-good/$(ARCH)/minigrace-$(STABLE) $(SOURCEFILES) unicode.gso gracelib.o
	( mkdir -p l1 ; cd l1 ; for f in $(SOURCEFILES) unicode.gso gracelib.o ; do ln -sf ../$$f . ; done ; ../known-good/$(ARCH)/minigrace-$(STABLE) --verbose --make --native --module minigrace --gracelib ../known-good/$(ARCH)/gracelib-$(STABLE).o compiler.gc )

l2/minigrace: l1/minigrace $(SOURCEFILES) unicode.gso gracelib.o
	( mkdir -p l2 ; cd l2 ; for f in $(SOURCEFILES) unicode.gso gracelib.o ; do ln -sf ../$$f . ; done ; ../l1/minigrace --verbose --make --native --module minigrace --vtag l1 compiler.gc )

selfhost-stats: minigrace
	cat compiler.gc util.gc ast.gc parser.gc genllvm.gc > tmp.gc
	GRACE_STATS=1 ./minigrace < tmp.gc >/dev/null
	rm -f tmp.gc

selfhost-rec: minigrace
	@( cd l2 ; llvm-dis -o l2.ll minigrace.bc )
	@llvm-dis -o l3.ll minigrace.bc
	@diff -q l2/l2.ll l3.ll
	@rm -f l2/l2.ll l3.ll

minigrace: l2/minigrace $(SOURCEFILES) unicode.gso gracelib.o
	./l2/minigrace --vtag l2 --make --native --module minigrace --verbose compiler.gc

unicode.gco: unicode.c unicodedata.h
	clang -emit-llvm -c -o unicode.gco -DNO_FLAGS= unicode.c

test: minigrace
	./tests/harness "$(shell pwd)/minigrace --gracelib $(shell pwd)/gracelib.o" tests
clean:
	rm -f gracelib.o
	rm -f unicode.gco unicode.gso
	rm -rf l1 l2
	rm -f $(patsubst %.gc,%.ll,$(SOURCEFILES))
	rm -f $(patsubst %.gc,%.s,$(SOURCEFILES))
	rm -f $(patsubst %.gc,%.gco,$(SOURCEFILES))
	rm -f $(patsubst %.gc,%.bc,$(SOURCEFILES))
	rm -f $(patsubst %.gc,%,$(SOURCEFILES))
	rm -f minigrace.gco minigrace.ll minigrace.s minigrace

semiclean:
	rm -f lexer.gco parser.gco util.gco ast.gco genllvm.gco

known-good/%:
	cd known-good && $(MAKE) $*

.PHONY: all clean selfhost-stats selfhost-rec test
