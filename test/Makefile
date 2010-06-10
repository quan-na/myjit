all: t001 t002 t003 t004 t005 t006

t001: t001-basics.c jitlib-core.o tests.h
	gcc -std=c99 -o t001 t001-basics.c jitlib-core.o 

t002: t002-arithmetic.c jitlib-core.o tests.h
	gcc -std=c99 -o t002 t002-arithmetic.c jitlib-core.o 


t003: t003-conditions.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t003 t003-conditions.c jitlib-core.o 

t004: t004-branches.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t004 t004-branches.c jitlib-core.o 

t005: t005-functions.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t005 t005-functions.c jitlib-core.o 

t006: t006-misc.c jitlib-core.o tests.h
	gcc -g -std=c99 -o t006 t006-misc.c jitlib-core.o 

jitlib-core.o: ../myjit/jitlib.h ../myjit/jitlib-core.h ../myjit/jitlib-core.c ../myjit/jitlib-debug.c ../myjit/x86-codegen.h ../myjit/x86-specific.h ../myjit/x86-reg-allocator.h ../myjit/flow-analysis.h ../myjit/set.h
	        gcc -c -g -Winline -Wall -std=c99 -pedantic -D_XOPEN_SOURCE=600 ../myjit/jitlib-core.c


clean:
	rm -f *.o
	rm -f t001
	rm -f t002
	rm -f t003
	rm -f t004
	rm -f t005
	rm -f t006
