include Makefile.conf

ARCH:=$(shell uname -s)-$(shell uname -m)
STABLE=88855bc35225a388b52f1574488df3486543f510
all: minigrace gracepm stdlib $(OTHER_MODULES)

REALSOURCEFILES = compiler.grace errormessages.grace util.grace ast.grace lexer.grace parser.grace genjs.grace genc.grace mgcollections.grace interactive.grace xmodule.grace identifierresolution.grace genjson.grace
SOURCEFILES = $(REALSOURCEFILES) buildinfo.grace
JSSOURCEFILES = js/compiler.js js/errormessages.js js/ast.js js/lexer.js js/parser.js js/genjs.js js/genc.js js/mgcollections.js js/xmodule.js js/identifierresolution.js js/buildinfo.js js/genjson.js
CGRACELIB = gracelib.c gracelib_gc.c gracelib_threads.c gracelib_msg.c gracelib.h gracelib_gc.h gracelib_threads.h gracelib_msg.h gracelib_types.h

ifeq ($(MINIGRACE_BUILD_SUBPROCESSES),)
MINIGRACE_BUILD_SUBPROCESSES = 2
endif

echo:
	echo $(MINIGRACE_BUILD_SUBPROCESSES)

buildinfo.grace: $(REALSOURCEFILES) StandardPrelude.grace gracelib.c gracelib_gc.c gracelib_threads.c gracelib_msg.c gracelib_types.h
	echo "#pragma DefaultVisibility=public" > buildinfo.grace
	echo "method gitrevision { \"$(shell [ -e .git ] && git rev-parse HEAD || echo unknown )\" }" >> buildinfo.grace
	echo "method gitgeneration { \"$(shell [ -e .git ] && tools/git-calculate-generation || echo unknown )\" }" >> buildinfo.grace
	echo "method prefix { \"$(PREFIX)\" }" >> buildinfo.grace
	echo "method includepath { \"$(INCLUDE_PATH)\" }" >> buildinfo.grace
	echo "method modulepath { \"$(MODULE_PATH)\" }" >> buildinfo.grace
	echo "method objectpath { \"$(OBJECT_PATH)\" }" >> buildinfo.grace

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

gracelib-basic.o: gracelib.o gracelib_gc.o gracelib_threads.o gracelib_msg.o $(CGRACELIB)
	ld -r -o gracelib-basic.o gracelib.o gracelib_gc.o gracelib_threads.o gracelib_msg.o

gracelib-final.o: gracelib-basic.o debugger.o l1/minigrace StandardPrelude.grace
	l1/minigrace --make --noexec -XNoMain -XNativePrelude StandardPrelude.grace
	ld -o gracelib-final.o -r gracelib-basic.o StandardPrelude.gcn debugger.o

curl.gso: curl.c gracelib.h gracelib_types.h
	gcc $(CFLAGS) $(UNICODE_LDFLAGS) -o curl.gso -shared -fPIC curl.c -lcurl

mirrors.gso: mirrors.c gracelib.h gracelib_types.h
	gcc $(CFLAGS) $(UNICODE_LDFLAGS) -o mirrors.gso -shared -fPIC mirrors.c

actors_prim.gso: actors_prim.c gracelib.h gracelib_types.h gracelib_threads.h gracelib_msg.h gracelib_gc.h
	gcc $(CFLAGS) $(UNICODE_LDFLAGS) -o actors_prim.gso -shared -fPIC actors_prim.c

math.gso: math.c gracelib.h gracelib_types.h
	gcc $(CFLAGS) $(UNICODE_LDFLAGS) -o math.gso -shared -fPIC math.c

repl.gso: repl.c gracelib.h gracelib_types.h
	gcc $(CFLAGS) $(UNICODE_LDFLAGS) -o repl.gso -shared -fPIC repl.c

unicode.gso: unicode.c unicodedata.h gracelib.h gracelib_types.h
	gcc $(CFLAGS) $(UNICODE_LDFLAGS) -fPIC -shared -o unicode.gso unicode.c

unicode.gcn: unicode.c unicodedata.h gracelib.h gracelib_types.h
	gcc $(CFLAGS) -fPIC -c -o unicode.gcn unicode.c

l1/minigrace: known-good/$(ARCH)/$(STABLE)/minigrace $(SOURCEFILES) $(UNICODE_MODULE) $(CGRACELIB)
	( mkdir -p l1 ; cd l1 ; for f in $(SOURCEFILES) gracelib-final.o gracelib.h gracelib_gc.h gracelib_threads.h gracelib_msg.h gracelib_types.h ; do ln -sf ../$$f . ; done ; ln -sf ../known-good/$(ARCH)/$(STABLE)/$(UNICODE_MODULE) . ; for x in $(OTHER_MODULES) ; do ln -sf ../known-good/$(ARCH)/$(STABLE)/$$x . ; done ; ../known-good/$(ARCH)/$(STABLE)/minigrace --verbose --make --native --module minigrace --gracelib ../known-good/$(ARCH)/$(STABLE) --vtag kg -j $(MINIGRACE_BUILD_SUBPROCESSES) compiler.grace )

l2/minigrace: l1/minigrace $(SOURCEFILES) $(UNICODE_MODULE) gracelib-final.o gracelib.h gracelib_gc.h gracelib_threads.h gracelib_msg.h gracelib_types.h $(OTHER_MODULES)
	( mkdir -p l2 ; cd l2 ; for f in $(SOURCEFILES) gracelib-final.o gracelib.h gracelib_gc.h gracelib_threads.h gracelib_msg.h gracelib_types.h $(UNICODE_MODULE) $(OTHER_MODULES) ; do ln -sf ../$$f . ; done ; ../l1/minigrace --verbose --make --native --module minigrace --vtag l1 -j $(MINIGRACE_BUILD_SUBPROCESSES) compiler.grace )

js: js/index.html

js/StandardPrelude.js: StandardPrelude.grace minigrace
	./minigrace --verbose --target js -XNativePrelude -o js/StandardPrelude.js StandardPrelude.grace
	echo "Grace_prelude = do_import('StandardPrelude', gracecode_StandardPrelude);" >> js/StandardPrelude.js

js/minigrace.js: js/minigrace.in.js $(JSSOURCEFILES) js/StandardPrelude.js js/gracelib.js js/dom.js
	@echo Generating minigrace.js from minigrace.in.js...
	cat js/minigrace.in.js > js/minigrace.js
	echo "MiniGrace.version = '$$(tools/calculate-version HEAD)';" >> js/minigrace.js
	echo "MiniGrace.revision = '$$(git rev-parse HEAD|cut -b1-7)';" >> js/minigrace.js
	cat js/dom.js >> js/minigrace.js
	cat js/gracelib.js >> js/minigrace.js
	cat js/StandardPrelude.js >> js/minigrace.js
	for f in $(JSSOURCEFILES) ; do cat $$f >> js/minigrace.js ; done
	cat js/unicodedata.js >> js/minigrace.js

js/%.js: %.grace minigrace
	./minigrace --verbose --target js -o $@ $<

js/index.html: js/index.in.html js/ace.in.html js/minigrace.js
	@echo Generating index.html from index.in.html...
	@awk '!/<!--\[!SH\[/ { print } /<!--\[!SH\[/ { gsub(/<!--\[!SH\[/, "") ; gsub(/\]!\]-->/, "") ; system($$0) }' < $< > $@

c: minigrace $(CGRACELIB) unicode.c unicodedata.h Makefile c/Makefile mirrors.c actors_prim.c definitions.h curl.c repl.c math.c
	for f in $(CGRACELIB) unicode.c unicodedata.h $(SOURCEFILES) StandardPrelude.grace $(UNICODE_MODULE) mirrors.c actors_prim.c math.c definitions.h debugger.c curl.c repl.c ; do cp $$f c ; done && cd c && ../minigrace --make --noexec -XNoMain -XNativePrelude StandardPrelude.grace && ../minigrace --target c --make --verbose --module minigrace --noexec compiler.grace && sed -i 's!#include "../gracelib\(.*\).h"!#include "gracelib\1.h"!' *.c && rm -f *.gcn $(UNICODE_MODULE)

tarball: minigrace
	touch c/Makefile.conf
	make -C c fullclean
	make c
	sed -e 's/DISTRIB=tree/DISTRIB=tarball/' < configure > c/configure
	chmod 755 c/configure
	VER=$$(tools/calculate-version) ; mkdir minigrace-$$VER ; cp -R c/* minigrace-$$VER ; mkdir minigrace-$$VER/tests ; cp tests/*.grace tests/*.out tests/harness minigrace-$$VER/tests ; mkdir -p minigrace-$$VER/sample/dialects ; cp sample/dialects/*.grace sample/dialects/README sample/dialects/Makefile minigrace-$$VER/sample/dialects ; cp -R README doc minigrace-$$VER ; tar cjvf ../minigrace-$$VER.tar.bz2 minigrace-$$VER ; rm -rf minigrace-$$VER

selfhost-stats: minigrace
	cat compiler.grace util.grace ast.grace parser.grace genc.grace > tmp.grace
	GRACE_STATS=1 ./minigrace -XIgnoreShadowing < tmp.grace >/dev/null
	rm -f tmp.grace

selftest: minigrace
	rm -rf selftest
	mkdir -p selftest
	for f in $(SOURCEFILES) unicode.gso gracelib-final.o gracelib.h gracelib_gc.h gracelib_threads.h gracelib_msg.h gracelib_types.h ; do ln -sf ../$$f selftest ; done
	( cd selftest ; ../minigrace --verbose --make --native --module minigrace --vtag selftest -j $(MINIGRACE_BUILD_SUBPROCESSES) compiler.grace )
	rm -rf selftest

minigrace: l2/minigrace $(SOURCEFILES) $(UNICODE_MODULE) gracelib-final.o
	[ -e .git/hooks/commit-msg ] || ln -s ../../tools/validate-commit-message .git/hooks/commit-msg
	./l2/minigrace --vtag l2 -j $(MINIGRACE_BUILD_SUBPROCESSES) --make --native --module minigrace --verbose compiler.grace

# Giant hack! Not suitable for use.
minigrace-dynamic: l2/minigrace $(SOURCEFILES)
	l1/minigrace --make --noexec --import-dynamic -XNoMain -XNativePrelude StandardPrelude.grace
	ld -o gracelib-final.o -r gracelib-basic.o StandardPrelude.gcn debugger.o
	l2/minigrace --make --import-dynamic --verbose --module minigrace-dynamic compiler.grace

actors.gso: minigrace actors.grace
	./minigrace --make --noexec --dynamic-module -XNoMain actors.grace

actors_lib.gso: minigrace actors_lib.grace
	./minigrace --make --noexec --dynamic-module -XNoMain actors_lib.grace

stdlib: actors.gso actors_lib.gso

gencheck:
	( X=$$(tools/git-calculate-generation) ; mv .git-generation-cache .git-generation-cache.$$$$ ; Y=$$(tools/git-calculate-generation) ; [ "$$X" = "$$Y" ] || exit 1 ; rm -rf .git-generation-cache ; mv .git-generation-cache.$$$$ .git-generation-cache )
regrtest: minigrace
	./tests/harness "../../minigrace" tests/regression ""
test: minigrace
	./tests/harness "../minigrace" tests ""
fulltest: gencheck clean selftest test
togracetest: minigrace
	./tests/harness "../minigrace" tests tograce
repltest: minigrace
	./tests/harness "../minigrace" tests repl
backendtests: test

alltests: test regrtest

samples-%: minigrace
	$(MAKE) -C sample/$*

samples: samples-dialects samples-graphics samples-js

clean:
	rm -f gracelib-final.o gracelib-basic.o
	rm -f gracelib.bc gracelib.o
	rm -f gracelib_gc.bc gracelib_gc.o
	rm -f gracelib_threads.bc gracelib_threads.o
	rm -f gracelib_msg.bc gracelib_msg.o
	rm -f unicode.gco unicode.gso unicode.gcn
	rm -f mirrors.gso math.gso actors_prim.gso
	rm -f debugger.o
	rm -f repl.gso
	rm -rf l1 l2 buildinfo.grace
	rm -f $(SOURCEFILES:.grace=.c) minigrace.c
	rm -f $(SOURCEFILES:.grace=.gco)
	rm -f $(SOURCEFILES:.grace=.gcn) minigrace.gcn
	rm -f $(SOURCEFILES:.grace=.gso) minigrace.gso
	rm -f $(SOURCEFILES:.grace=.gct) minigrace.gct
	rm -f minigrace-dynamic
	rm -f $(SOURCEFILES:.grace=)
	( cd js ; for sf in $(SOURCEFILES:.grace=.js) ; do rm -f $$sf ; done )
	( cd js ; for sf in $(SOURCEFILES) ; do rm -f $$sf ; done )
	rm -f js/minigrace.js
	( cd c ; rm -f *.gcn *.gct *.c *.h *.grace minigrace unicode.gso gracelib.o gracelib_gc.o gracelib_threads.o gracelib_msg.o gracelib-final.o )
	rm -f minigrace.gco minigrace

known-good/%:
	rm -rf l1 l2 # We must regenerate files so #include updated
	cd known-good && $(MAKE) $*
	rm -f known-good/*out

gracepm: gracepm.grace minigrace
	./minigrace --verbose --make gracepm.grace

install: minigrace gracepm
	install -d $(PREFIX)/bin $(MODULE_PATH) $(OBJECT_PATH) $(INCLUDE_PATH)
	install -m 755 minigrace $(PREFIX)/bin/minigrace
	install -m 755 gracepm $(PREFIX)/bin/gracepm
	install -m 755 unicode.gso $(OTHER_MODULES) $(MODULE_PATH)
	install -m 755 actors.gso $(MODULE_PATH)
	install -m 755 gracelib-final.o $(OBJECT_PATH)
	install -m 644 gracelib.h $(INCLUDE_PATH)
	install -m 644 gracelib_gc.h $(INCLUDE_PATH)
	install -m 644 gracelib_threads.h $(INCLUDE_PATH)
	install -m 644 gracelib_msg.h $(INCLUDE_PATH)
	install -m 644 gracelib_types.h $(INCLUDE_PATH)
	install -m 644 mgcollections.grace $(MODULE_PATH)

Makefile.conf: configure
	./configure

ctags:
	ctags -R --exclude=c/* --exclude=js/* --exclude=known-good/* --exclude=l1/* --exclude=l2/*

.PHONY: all clean selfhost-stats test js c selftest install samples sample-%
