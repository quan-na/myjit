all: sparc-dis.o 

#CFLAGS = -g -std=c99 -Wall -pedantic -D_XOPEN_SOURCE=600

sparc-dis.o: sparc-dis.c sparc-dis.h
	$(CC) $(CFLAGS) -c sparc-dis.c 

clean:
	rm -f *.o
