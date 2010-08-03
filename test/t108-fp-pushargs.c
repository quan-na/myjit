#include <stdlib.h>
#include <stdio.h>

#include "../myjit/jitlib.h"

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
	//static char * msg = "1:\t%i\n2:\t%i\n3:\t%i\n4:\t%i\n5:\t%i\n6:\t%i\n7:\t%i\n8:\t%i\n9:\t%i\n10:\t%i\n";
	static char * msg = "1:\t%i\n2:\t%i\n3:\t%i\n4:\t%i\n5:\t%i\n6:\t%i\n7:\t%i\n8:\t%i\n9:\t%i\n";

	plfv foo;

	jit_prolog(p, &foo);

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
	jit_movi(p, R(10), msg);

	jit_prepare(p, 10);
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
