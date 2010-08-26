#include <stdlib.h>
#include <stdio.h>

#define JIT_REGISTER_TEST
#include "../myjit/jitlib.h"

#define BLOCK_SIZE	(16)

typedef long (* plfv)();

int foobar(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8, double a9, double a10)
{
	printf("A1:%f\n", a1);
	printf("1:\t%f\n2:\t%f\n3:\t%f\n4:\t%f\n5:\t%f\n6:\t%f\n7:\t%f\n8:\t%f\n9:\t%f\n10:\t%f\n",
		a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
	return 666;
}
int main()
{
	struct jit * p = jit_init();
	static char * msg2 = "%u\n";

	plfv foo;

	jit_prolog(p, &foo);
	int i = jit_allocai(p, BLOCK_SIZE);

	// loads into the allocated memory multiples of 3
	jit_movi(p, R(0), 0);	// index
	jit_addi(p, R(1), R_FP, i); // pointer to the allocated memory
	jit_label * lab = jit_get_label(p);
	jit_muli(p, R(2), R(0), 3);
	jit_stxr(p, R(1), R(0), R(2), 1);
	jit_addi(p, R(0), R(0), 1);
	jit_blti(p, lab, R(0), BLOCK_SIZE);

	// does something with registers
	jit_fmovi(p, FR(0), -1);
	jit_fmovi(p, FR(1), 1);
	jit_fmovi(p, FR(2), 2);
	jit_fmovi(p, FR(3), 4);
	jit_fmovi(p, FR(4), 8);
	jit_fmovi(p, FR(5), 16);
	jit_fmovi(p, FR(6), 32);
	jit_fmovi(p, FR(7), 64);
	jit_fmovi(p, FR(8), 128);
	jit_fmovi(p, FR(9), 256);

	jit_prepare(p);
	jit_fputargr(p, FR(0), sizeof(double));
	jit_fputargr(p, FR(1), sizeof(double));
	jit_fputargr(p, FR(2), sizeof(double));
	jit_fputargr(p, FR(3), sizeof(double));
	jit_fputargr(p, FR(4), sizeof(double));
	jit_fputargr(p, FR(5), sizeof(double));
	jit_fputargr(p, FR(6), sizeof(double));
	jit_fputargr(p, FR(7), sizeof(double));
	jit_fputargr(p, FR(8), sizeof(double));
	jit_fputargr(p, FR(9), sizeof(double));
	jit_call(p, foobar);

	// print outs allocated memory

	jit_movi(p, R(0), 0);	// index
	jit_addi(p, R(1), R_FP, i); // pointer to the allocated memory
	jit_label * lab2 = jit_get_label(p);
	
	jit_ldxr(p, R(2), R(1), R(0), 1);

	jit_prepare(p);
	jit_putargi(p, msg2);
	jit_putargr(p, R(2));
	jit_call(p, printf);

	jit_addi(p, R(0), R(0), 1);
	jit_blti(p, lab2, R(0), BLOCK_SIZE);

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
