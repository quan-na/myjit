#include <stdlib.h>
#include <stdio.h>

#define JIT_REGISTER_TEST
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
	struct jit * p = jit_init(4);
	//static char * msg = "1:\t%i\n2:\t%i\n3:\t%i\n4:\t%i\n5:\t%i\n6:\t%i\n7:\t%i\n8:\t%i\n9:\t%i\n10:\t%i\n";
	static char * msg = "1:\t%i\n2:\t%i\n3:\t%i\n4:\t%i\n5:\t%i\n6:\t%i\n7:\t%i\n8:\t%i\n9:\t%i\n";
	static char * msg2 = "%i\n";

	plfv foo;

	jit_prolog(p, &foo);
	int i = jit_allocai(p, 16);

	// loads into the allocated memory multiples of 5
	jit_movi(p, R(0), 0);	// index
	jit_addi(p, R(1), JIT_FP, i); // pointer to the allocated memory
	jit_label * lab = jit_get_label(p);
	jit_muli(p, R(2), R(0), 5);
	jit_stxr(p, R(1), R(0), R(2), 1);
	jit_addi(p, R(0), R(0), 1);
	jit_blti(p, lab, R(0), 16);

	// does something with registers
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

	jit_prepare(p, 10);
	jit_pushargr(p, R(9));
	jit_pushargr(p, R(8));
	jit_pushargr(p, R(7));
	jit_pushargr(p, R(6));
	jit_pushargr(p, R(5));
	jit_pushargr(p, R(4));
	jit_pushargr(p, R(3));
	jit_pushargr(p, R(2));
	jit_pushargr(p, R(1));
	jit_pushargr(p, R(0));
	jit_call(p, foobar);

	// print outs allocated memory
	jit_movi(p, R(0), 0);	// index
	jit_addi(p, R(1), JIT_FP, i); // pointer to the allocated memory
	jit_label * lab2 = jit_get_label(p);

	jit_ldxr(p, R(2), R(1), R(0), 1);
	
	jit_prepare(p, 2);
	jit_pushargr(p, R(2));
	jit_pushargi(p, msg2);
	jit_call(p, printf);

	jit_addi(p, R(0), R(0), 1);
	jit_blti(p, lab2, R(0), 16);
	
	jit_retr(p, R(0));

	jit_generate_code(p);
//	jit_print_ops(p);

	jit_dump(p);

	// check
	printf("Check #1: %li\n", foo());

	// cleanup
	jit_free(p);
	return 0;
}
