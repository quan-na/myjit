all: decode.o itab.o syn.o syn-att.o syn-intel.o udis86.o

#CFLAGS = -g -std=c99 -Wall -pedantic -D_XOPEN_SOURCE=600

decode.o: decode.c udint.h types.h extern.h decode.h
	$(CC) $(CFLAGS) -c decode.c 

itab.o: itab.c decode.h
	$(CC) $(CFLAGS) -c itab.c 

syn.o: syn.c types.h decode.h syn.h udint.h
	$(CC) $(CFLAGS) -c syn.c 

syn-att.o: syn-att.c types.h extern.h decode.h itab.h syn.h udint.h
	$(CC) $(CFLAGS) -c syn-att.c 

syn-intel.o: syn-intel.c types.h extern.h decode.h itab.h syn.h udint.h
	$(CC) $(CFLAGS) -c syn-intel.c 

udis86.o: udis86.c udint.h extern.h decode.h
	$(CC) $(CFLAGS) -c udis86.c 

clean:
	rm -f *.o
