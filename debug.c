#include <stdlib.h>
#include <stdio.h>

#include "myjit/jitlib.h"

typedef double (* pdfdd)(int, float, short, double);

double foofn(float a, int b, float c)
{
	printf("Test: %f x %i = %f\n", a, b, c);
	return a + b + c;
}

int main()
{
	struct jit * p = jit_init();
	static char * str = "Hello, World! Number of the day is %f!!!\n";

	static double g[4];
	g[0] = 1.2;
	g[1] = 2.3;
	g[2] = 3.4;
	g[3] = 4.5;

	pdfdd foo;
	jit_prolog(p, &foo);

	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(int));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));

	/*
	jit_getarg(p, R(0), 0);
	jit_getarg(p, FR(0), 1);
	jit_getarg(p, R(1), 2);
	jit_getarg(p, FR(1), 3);*/

	jit_fmovi(p, FR(0), 123.1);
	jit_floorr(p, R(0), FR(0));
	jit_extr(p, FR(0), R(0));

	jit_fretr(p, FR(0));

	jit_generate_code(p);

	jit_dump_code(p, 0);

//	foo(1.2, 2.5);
//	for (int i = 0; i < 4; i++)
//		printf("X:%f\n", g[i]);


	// check
	printf("Check #1: %f\n", foo(1, 1.2, 2, 2.5));
	//printf("Check #2: %li\n", foo(100));
	//printf("Check #3: %li\n", foo(255));

	// cleanup
	jit_free(p);
	return 0;
}
