#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "myjit/jitlib.h"

int foobar(int x, int y)
{
	fprintf(stderr, "Test:%i:%i\n", x, y);
	return x - y;
}

typedef int (*pifv)(void);
typedef int (*pifi)(int);
typedef int (*pifii)(int, int);
// TODO: jit_flush -- to flush the cache

int main() 
{
	static char * foo = "FOO: %i\n";
	struct jit * p = jit_init(1000, 8);
	int xy = 777;

	pifii f1;
	jit_prolog(p, &f1);
	int a1 = jit_arg(p);
	int a2 = jit_arg(p);
	int hoo = jit_allocai(p, 8);

	jit_movi(p, R(0), 12345);
	jit_stxi(p, hoo, R_FP, R(0), REG_SIZE);
	jit_stxi(p, hoo + 4, R_FP, R(0), REG_SIZE);

	jit_movi(p, R(0), 0);
	jit_movi(p, R(1), 1);
	jit_movi(p, R(2), 2);
	jit_movi(p, R(3), 3);
	jit_movi(p, R(4), 4);
	jit_movi(p, R(5), 5);
	jit_movi(p, R(6), 6);
	jit_movi(p, R(7), 7);

	jit_addr(p, R(0), R(0), R(1));
	jit_addr(p, R(0), R(0), R(2));
	jit_addr(p, R(0), R(0), R(3));
	jit_addr(p, R(0), R(0), R(4));
	jit_addr(p, R(0), R(0), R(5));
	jit_addr(p, R(0), R(0), R(6));
	jit_addr(p, R(0), R(0), R(7));

	jit_prepare(p, 2);
	jit_pushargr(p, R(0));
	jit_pushargi(p, foo);
	jit_finish(p, printf);	

	jit_addr(p, R(0), R(0), R(1));
	jit_addr(p, R(0), R(0), R(2));
	jit_addr(p, R(0), R(0), R(3));
	jit_addr(p, R(0), R(0), R(4));
	jit_addr(p, R(0), R(0), R(5));
	jit_addr(p, R(0), R(0), R(6));
	jit_addr(p, R(0), R(0), R(7));

	
	jit_retr(p, R(0));

//	jit_reti(p, 256);
	jit_generate_code(p);
//	jit_print_ops(p);
	jit_dump(p);
	int x;
	x = f1(15, 20);
	fprintf(stderr, "#%i (0x%x)#\n", x, x);
	jit_free(p);
	return 0;
}
