all: myjit-disasm

export CFLAGS = -g -std=c99 -Wall -pedantic -D_XOPEN_SOURCE=600 -O2

myjit-disasm: myjit-disasm.o io.o udis86/decode.o udis86/itab.o udis86/syn.o udis86/syn-att.o udis86/syn-intel.o udis86/udis86.o sparc/sparc-dis.o
	$(MAKE) -C udis86
	$(MAKE) -C sparc
	$(CC) $(CFLAGS) -o myjit-disasm udis86/decode.o udis86/itab.o udis86/syn.o udis86/syn-att.o udis86/syn-intel.o udis86/udis86.o sparc/sparc-dis.o myjit-disasm.o io.o

myjit-disasm.o: myjit-disasm.c
	$(CC) $(CFLAGS) -c myjit-disasm.c

io.o: io.c io.h
	$(CC) $(CFLAGS) -c io.c

clean:
	rm -f myjit-disasm
	rm -f *.o
	rm -f udis86/*.o
	rm -f sparc/*.o
