#include <stdlib.h>
#include <stdio.h>

#include "../myjit/jitlib.h"

typedef long (* plf10s)(short, short, short, short, short, short, short, short, short, short);

int main()
{
	struct jit * p = jit_init();

	plf10s foo;

	jit_prolog(p, &foo);

	int ar0 = jit_arg(p);
	int ar1 = jit_arg(p);
	int ar2 = jit_arg(p);
	int ar3 = jit_arg(p);
	int ar4 = jit_arg(p);
	int ar5 = jit_arg(p);
	int ar6 = jit_arg(p);
	int ar7 = jit_arg(p);
	int ar8 = jit_arg(p);
	int ar9 = jit_arg(p);

	jit_getarg(p, R(0), ar0, sizeof(short));
	jit_getarg(p, R(1), ar1, sizeof(short));
	jit_getarg(p, R(2), ar2, sizeof(short));
	jit_getarg(p, R(3), ar3, sizeof(short));
	jit_getarg(p, R(4), ar4, sizeof(short));
	jit_getarg(p, R(5), ar5, sizeof(short));
	jit_getarg(p, R(6), ar6, sizeof(short));
	jit_getarg(p, R(7), ar7, sizeof(short));
	jit_getarg(p, R(8), ar8, sizeof(short));
	jit_getarg(p, R(9), ar9, sizeof(short));

	
	jit_movi(p, R(10), 0);
	jit_addr(p, R(10), R(10), R(9));
	jit_addr(p, R(11), R(10), R(8));
	jit_addr(p, R(12), R(11), R(7));
	jit_addr(p, R(13), R(12), R(6));
	jit_addr(p, R(14), R(13), R(5));
	jit_addr(p, R(15), R(14), R(4));
	jit_addr(p, R(16), R(15), R(3));
	jit_addr(p, R(17), R(16), R(2));
	jit_addr(p, R(18), R(17), R(1));

	jit_addr(p, R(8), R(8), R(9));
	jit_addr(p, R(7), R(7), R(8));
	jit_addr(p, R(6), R(6), R(7));
	jit_addr(p, R(5), R(5), R(6));
	jit_addr(p, R(4), R(4), R(5));
	jit_addr(p, R(3), R(3), R(4));
	jit_addr(p, R(2), R(2), R(3));
	jit_addr(p, R(1), R(1), R(2));
	jit_addr(p, R(0), R(0), R(1));

	jit_addr(p, R(0), R(0), R(18));

	jit_retr(p, R(0));

	jit_generate_code(p);

	jit_dump_code(p, 0);

	// check
	printf("Check #1: %li\n", foo(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));

	// cleanup
	jit_free(p);
	return 0;
}
