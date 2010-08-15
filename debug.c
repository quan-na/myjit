#include <stdlib.h>
#include <stdio.h>

#include "myjit/jitlib.h"

typedef double (* pdfdd)(int, float, short, double);

double foofn(float a, int b, float c)
{
	printf("Test: %f x %i = %f\n", a, b, c);
	return a + b + c;
}

void foobar()
{
	printf("foo-bar\n");
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

	printf("rRR:%i\n", R(0));

	pdfdd foo;
	jit_prolog(p, &foo);
	int a = jit_allocai(p, 10);
	jit_movi(p, R(0), 127);
	jit_movi(p, R(1), 127);
	jit_movi(p, R(2), 127);
	jit_movi(p, R(3), 127);
	jit_movi(p, R(4), 127);
	jit_movi(p, R(5), 127);
	jit_movi(p, R(6), 127);
	jit_movi(p, R(7), 127);
	jit_movi(p, R(8), 127);
	jit_movi(p, R(9), 127);
	jit_movi(p, R(10), 127);
	jit_movi(p, R(11), 127);
	jit_movi(p, R(12), 127);
	jit_movi(p, R(13), 127);
	jit_movi(p, R(14), 127);
	jit_movi(p, R(15), 127);
	jit_movi(p, R(16), 127);

	jit_addr(p, R(0), R(1), R(2));
	jit_addr(p, R(0), R(3), R(4));
	jit_addr(p, R(0), R(5), R(6));
	jit_addr(p, R(0), R(7), R(8));
	jit_addr(p, R(0), R(9), R(10));
	jit_addr(p, R(0), R(11), R(12));
	jit_addr(p, R(0), R(13), R(14));
	jit_addr(p, R(0), R(15), R(16));


	/*
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(int));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));

	jit_getarg(p, R(0), 0);
	jit_getarg(p, FR(0), 1);
	jit_getarg(p, R(1), 2);
	jit_getarg(p, FR(1), 3);
*/
	/*
	jit_fmovi(p, FR(0), 1.0);
	jit_fmovi(p, FR(1), -2.2);
	jit_roundr(p, R(0), FR(1));
	jit_extr(p, FR(1), R(0));
	*/
	//jit_fsubr(p, FR(1), FR(0), FR(1));
//	jit_floorr(p, R(0), FR(0));
//	jit_extr(p, FR(0), R(0));

	jit_fretr(p, FR(0));

	jit_generate_code(p);

	jit_dump_ops(p, 0);

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
