#include <stdlib.h>
#include <stdio.h>

#include "../myjit/jitlib.h"

typedef long (* plfv)();

int foobar(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
	printf("A1:%i\n", a1);
	printf("1:\t%i\n2:\t%i\n3:\t%i\n4:\t%i\n5:\t%i\n6:\t%i\n7:\t%i\n8:\t%i\n9:\t%i\n10:\t%i\n",
		a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
	return 666;
}
int main()
{
	struct jit * p = jit_init();
	//static char * msg = "1:\t%i\n2:\t%i\n3:\t%i\n4:\t%i\n5:\t%i\n6:\t%i\n7:\t%i\n8:\t%i\n9:\t%i\n10:\t%i\n";
	static char * msg = "1:\t%i\n2:\t%i\n3:\t%i\n4:\t%i\n5:\t%i\n6:\t%i\n7:\t%i\n8:\t%i\n9:\t%i\n";

	plfv foo;

	jit_prolog(p, &foo);

	jit_movi(p, R(0), -1);
	jit_movi(p, R(1), 1);
	jit_movi(p, R(2), 2);
	jit_movi(p, R(3), 4);
	jit_movi(p, R(4), 8);
	jit_movi(p, R(5), 16);
	jit_movi(p, R(6), 32);
	jit_movi(p, R(7), 64);
	jit_movi(p, R(8), 128);
	jit_movi(p, R(9), 256);
	jit_movi(p, R(10), msg);

	jit_prepare(p);
	jit_putargr(p, R(0));
	jit_putargr(p, R(1));
	jit_putargr(p, R(2));
	jit_putargr(p, R(3));
	jit_putargr(p, R(4));
	jit_putargr(p, R(5));
	jit_putargr(p, R(6));
	jit_putargr(p, R(7));
	jit_putargr(p, R(8));
	jit_putargr(p, R(9));
	jit_call(p, foobar);

	jit_retr(p, R(0));

	jit_generate_code(p);
//	jit_dump_ops(p, 0);

	jit_dump_code(p, 0);

	// check
	printf("Check #1: %li\n", foo());

	// cleanup
	jit_free(p);
	return 0;
}
