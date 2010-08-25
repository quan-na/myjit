#include <stdlib.h>
#include <stdio.h>

#include "../myjit/jitlib.h"

typedef long (* plfv)();

int foobar(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8, double a9, double a10)
{
	double a11 = 666;
	printf("A1:%f\n", a1);
	printf("1:\t%f\n2:\t%f\n3:\t%f\n4:\t%f\n5:\t%f\n6:\t%f\n7:\t%f\n8:\t%f\n9:\t%f\n10:\t%f\n11:\t%f\n",
		a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
	return 666;
}

int barbaz(float a1, float a2, float a3, float a4, float a5, float a6, float a7, float a8, float a9, float a10, float a11)
{
	//double a11 = 666;
	printf("A1:%f\n", a1);
	printf("1:\t%f\n2:\t%f\n3:\t%f\n4:\t%f\n5:\t%f\n6:\t%f\n7:\t%f\n8:\t%f\n9:\t%f\n10:\t%f\n11:\t%f\n",
		a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11);
	return 666;
}

int test1()
{
	struct jit * p = jit_init();

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
	jit_movi(p, R(10), 123);

	jit_prepare(p, 11);
	jit_fputargr(p, FR(0), sizeof(double));
	jit_fputargr(p, FR(1), sizeof(double));
	jit_fputargr(p, FR(2), sizeof(double));
	jit_fputargr(p, FR(3), sizeof(double));
	jit_fputargr(p, FR(4), sizeof(double));
	jit_fputargr(p, FR(5), sizeof(double));
	jit_fputargr(p, FR(6), sizeof(double));
	jit_fputargr(p, FR(7), sizeof(double));
	jit_fputargr(p, FR(8), sizeof(double));
	jit_fputargi(p, 222.222, sizeof(double));
	//jit_fputargr(p, FR(9), sizeof(double));
	jit_call(p, foobar);

	jit_reti(p, 0);

	jit_generate_code(p);
//	jit_dump_ops(p, JIT_DEBUG_ASSOC);

	jit_dump_code(p, 0);

	// check
	printf("Check #1: %li\n", foo());

	// cleanup
	jit_free(p);
	return 0;
}

int test2()
{
	struct jit * p = jit_init();

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
	jit_movi(p, R(10), 123);

	jit_prepare(p, 11);
	jit_fputargr(p, FR(0), sizeof(float));
	jit_fputargr(p, FR(1), sizeof(float));
	jit_fputargr(p, FR(2), sizeof(float));
	jit_fputargr(p, FR(3), sizeof(float));
	jit_fputargr(p, FR(4), sizeof(float));
	jit_fputargr(p, FR(5), sizeof(float));
	jit_fputargr(p, FR(6), sizeof(float));
	jit_fputargr(p, FR(7), sizeof(float));
	jit_fputargr(p, FR(8), sizeof(float));
	jit_fputargi(p, 222.222, sizeof(float));
	jit_fputargr(p, FR(9), sizeof(float));
	jit_call(p, barbaz);

	jit_reti(p, 0);

	jit_generate_code(p);
//	jit_dump_ops(p, JIT_DEBUG_ASSOC);

	jit_dump_code(p, 0);

	// check
	printf("Check #2: %li\n", foo());

	// cleanup
	jit_free(p);
	return 0;
}


int main()
{
	test1();
	test2();
}
