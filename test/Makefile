all: gp fp misc optim

gp: t001 t002 t003 t004 t005 t006 t007 t008 t009 t010
	
fp: t101 t102 t103 t104 t105 t107 t108 t109

optim: t301 t302

misc: t200 t201 t202 t301 t401


CFLAGS = -g -std=c99 -Wall -pedantic -D_XOPEN_SOURCE=600

t001: t001-basics.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t001 t001-basics.c jitlib-core.o

t002: t002-arithmetic.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t002 t002-arithmetic.c jitlib-core.o

t003: t003-conditions.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t003 t003-conditions.c jitlib-core.o

t004: t004-branches.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t004 t004-branches.c jitlib-core.o

t005: t005-functions.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t005 t005-functions.c jitlib-core.o

t006: t006-misc.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t006 t006-misc.c jitlib-core.o

t007: t007-args.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t007 t007-args.c jitlib-core.o

t008: t008-pushargs.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t008 t008-pushargs.c jitlib-core.o

t009: t009-allocaireg.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t009 t009-allocaireg.c jitlib-core.o

t010: t010-debug.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t010 t010-debug.c jitlib-core.o

t101: t101-fp-basics.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t101 t101-fp-basics.c jitlib-core.o

t102: t102-fp-arithmetic.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t102 t102-fp-arithmetic.c jitlib-core.o

t103: t103-fp-rounding.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t103 -lm t103-fp-rounding.c jitlib-core.o

t104: t104-fp-branches.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t104 t104-fp-branches.c jitlib-core.o

t105: t105-fp-functions.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t105 t105-fp-functions.c jitlib-core.o

t107: t107-fp-args.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t107 t107-fp-args.c jitlib-core.o

t108: t108-fp-pushargs.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t108 t108-fp-pushargs.c jitlib-core.o

t109: t109-fp-allocaireg.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t109 t109-fp-allocaireg.c jitlib-core.o

t200: t200-callr.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t200 t200-callr.c jitlib-core.o


t201: t201-functions.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t201 t201-functions.c jitlib-core.o


t202: t202-highmem.c jitlib-core.o tests.h
	$(CC) -c $(CFLAGS) -DJIT_MALLOC=xxx_alloc -DJIT_FREE=xxx_free ../myjit/jitlib-core.c -o jitlib-core-highmem.o
	$(CC) $(CFLAGS) -DJIT_MALLOC=xxx_alloc -DJIT_FREE=xxx_free -o t202 t202-highmem.c jitlib-core-highmem.o

t301: t301-optim-adds.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t301 t301-optim-adds.c jitlib-core.o


t302: t302-optim-stores.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t302 t302-optim-stores.c jitlib-core.o

t401: t401-data-in-code.c jitlib-core.o tests.h
	$(CC) $(CFLAGS) -o t401 t401-data-in-code.c jitlib-core.o


jitlib-core.o: ../myjit/jitlib.h ../myjit/jitlib-core.h ../myjit/jitlib-core.c ../myjit/jitlib-debug.c ../myjit/x86-codegen.h ../myjit/x86-specific.h ../myjit/reg-allocator.h ../myjit/flow-analysis.h ../myjit/set.h ../myjit/amd64-specific.h ../myjit/amd64-codegen.h ../myjit/llrb.c ../myjit/reg-allocator.h ../myjit/rmap.h 
	        $(CC) -c $(CFLAGS) ../myjit/jitlib-core.c


clean:
	rm -f *.o
	rm -f *.gcda
	rm -f *.gcno
	rm -f coverage.info
	rm -fr coverage
	rm -f t001
	rm -f t002
	rm -f t003
	rm -f t004
	rm -f t005
	rm -f t006
	rm -f t007
	rm -f t008
	rm -f t009
	rm -f t010
	rm -f t101
	rm -f t102
	rm -f t103
	rm -f t104
	rm -f t105
	rm -f t107
	rm -f t108
	rm -f t109
	rm -f t200
	rm -f t201
	rm -f t202
	rm -f t301
	rm -f t302
	rm -f t401

coverage: CFLAGS+=--coverage
coverage: clean coverage-run gen-coverage

gen-coverage: 
	lcov --capture --directory ../ --output-file coverage.info
	genhtml coverage.info --output-directory coverage

coverage-run: all
	./run-tests.sh
	./t401 -l > /dev/null
	./t401 --all --code > /dev/null
	./t401 --all --compilable > /dev/null
	./t401 --all --compilable > /dev/null
	./t401 --all --liveness --assoc > /dev/null
	./t001 --all --liveness --assoc > /dev/null
	./t002 --all --liveness --assoc > /dev/null
	./t003 --all --liveness --assoc > /dev/null
	./t004 --all --liveness --assoc > /dev/null
	./t005 --all --liveness --assoc > /dev/null
	./t101 --all --liveness --assoc > /dev/null
	./t102 --all --liveness --assoc > /dev/null
	./t103 --all --liveness --assoc > /dev/null
	./t104 --all --liveness --assoc > /dev/null
	./t105 --all --liveness --assoc > /dev/null
	./t401 test11 test12 x

