all: gp fp

gp: t001 t002 t003 t004 t005 t006 t007 t008 t009 t010
	
fp: t101 t102 t103 t104 t105 t107 t108 t109

t001: t001-basics.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t001 t001-basics.c jitlib-core.o -lopcodes

t002: t002-arithmetic.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t002 t002-arithmetic.c jitlib-core.o -lopcodes

t003: t003-conditions.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t003 t003-conditions.c jitlib-core.o -lopcodes

t004: t004-branches.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t004 t004-branches.c jitlib-core.o -lopcodes

t005: t005-functions.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t005 t005-functions.c jitlib-core.o -lopcodes

t006: t006-misc.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t006 t006-misc.c jitlib-core.o -lopcodes

t007: t007-args.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t007 t007-args.c jitlib-core.o -lopcodes

t008: t008-pushargs.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t008 t008-pushargs.c jitlib-core.o -lopcodes

t009: t009-allocaireg.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t009 t009-allocaireg.c jitlib-core.o -lopcodes

t010: t010-debug.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t010 t010-debug.c jitlib-core.o -lopcodes

t101: t101-fp-basics.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t101 t101-fp-basics.c jitlib-core.o -lopcodes

t102: t102-fp-arithmetic.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t102 t102-fp-arithmetic.c jitlib-core.o -lopcodes

t103: t103-fp-rounding.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t103 -lm t103-fp-rounding.c jitlib-core.o -lopcodes

t104: t104-fp-branches.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t104 t104-fp-branches.c jitlib-core.o -lopcodes

t105: t105-fp-functions.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t105 t105-fp-functions.c jitlib-core.o -lopcodes

t107: t107-fp-args.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t107 t107-fp-args.c jitlib-core.o -lopcodes

t108: t108-fp-pushargs.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t108 t108-fp-pushargs.c jitlib-core.o -lopcodes

t109: t109-fp-allocaireg.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t109 t109-fp-allocaireg.c jitlib-core.o -lopcodes



jitlib-core.o: ../myjit/jitlib.h ../myjit/jitlib-core.h ../myjit/jitlib-core.c ../myjit/jitlib-debug.c ../myjit/x86-codegen.h ../myjit/x86-specific.h ../myjit/reg-allocator.h ../myjit/flow-analysis.h ../myjit/set.h ../myjit/amd64-specific.h ../myjit/amd64-codegen.h ../myjit/llrb.c ../myjit/reg-allocator.h ../myjit/rmap.h 
	        gcc -c -g -Winline -Wall -std=c99 -pedantic -D_XOPEN_SOURCE=600 ../myjit/jitlib-core.c


clean:
	rm -f *.o
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
