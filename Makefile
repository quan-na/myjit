all: demo1 demo2 demo3 test7-args

demo1: demo1.c jitlib-core.o
	gcc -c -g -Winline -Wall -std=c99 -pedantic -D_XOPEN_SOURCE=600 demo1.c
	gcc -o demo1 -g -Wall -std=c99 -pedantic demo1.o jitlib-core.o

demo2: demo2.c jitlib-core.o
	gcc -c -g -Winline -Wall -std=c99 -pedantic -D_XOPEN_SOURCE=600 demo2.c
	gcc -o demo2 -g -Wall -std=c99 -pedantic demo2.o jitlib-core.o

demo3: demo3.c jitlib-core.o
	gcc -c -g -Winline -Wall -std=c99 -pedantic -D_XOPEN_SOURCE=600 demo3.c
	gcc -o demo3 -g -Wall -std=c99 -pedantic demo3.o jitlib-core.o

test7-args: test7-args.c jitlib-core.o
	gcc -c -g -Winline -Wall -std=c99 -pedantic -D_XOPEN_SOURCE=600 test7-args.c
	gcc -o test7-args -g -Wall -std=c99 -pedantic test7-args.o jitlib-core.o


jitlib-core.o: myjit/jitlib.h myjit/jitlib-core.h myjit/jitlib-core.c myjit/jitlib-debug.c myjit/x86-codegen.h myjit/x86-specific.h myjit/x86-reg-allocator.h myjit/flow-analysis.h myjit/set.h myjit/amd64-specific.h myjit/amd64-codegen.h myjit/x86-reg-allocator.h myjit/amd64-reg-allocator.h
	gcc -c -g -Winline -Wall -std=c99 -pedantic -D_XOPEN_SOURCE=600 myjit/jitlib-core.c

clean:
	rm -f demo1
	rm -f demo2
	rm -f demo3
	rm -f *.o
