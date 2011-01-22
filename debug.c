#include <stdlib.h>
#include <stdio.h>

#include "myjit/jitlib.h"

//typedef double (* pdfdd)(int, float, short, double);

typedef double (* pdfdd)(double, double);

double foofn(float a, int b, float c)
{
	printf("Test: %f x %i = %f\n", a, b, c);
	return a + b + c;
}

#include <stdarg.h>
int eee(double a, double b, double c)
{
	return a * b * c;
}

double foobaz(double x)
{
//	double r = 123.4;
	printf("foo-bazzz:%f\n", x);
	return 88.99;
//	return eee(x, x + 1, x + 2);
	//return (int)(x * r);
}

float foobar(float x)
{
//	double r = 123.4;
	printf("foo-bar:%f\n", x);
	return 88.99;
//	return eee(x, x + 1, x + 2);
	//return (int)(x * r);
}

int aaa(char * f, int x)
{
	printf("R%i:%i\n", 123, 45);
	return x;
}

int main()
{
	struct jit * p = jit_init();
	static char * str = "Hello, World! Number of the day is %f!!!\n";
	static char * str2 = "Hello, World! Number of the day is %u!!!\n";

	static double g[4];
	g[0] = 11.2;
	g[1] = 12.3;
	g[2] = 13.4;
	g[3] = 14.5;

	static float h[4];
	h[0] = 21.2f;
	h[1] = 22.3f;
	h[2] = 23.4f;
	h[3] = 24.5f;



	pdfdd foo;
	jit_prolog(p, &foo);
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));

	jit_movi(p, R(0), 2);
	jit_lshi(p, R(1), R(0), 3);
	jit_movi(p, R(2), 3);
	jit_ori(p, R(2), R(1), 7);
	jit_msgr(p, "Foooo:%i\n", R(2));
	jit_addr(p, R(2), R(2), R(0));
//	jit_lshr(p, R(2), R(0), R(1));
	jit_extr(p, FR(0), R(2));
/*
	jit_prepare(p);
	jit_putargi(p, str2);
	jit_putargi(p, 1234);
	jit_call(p, printf);

	jit_fmovi(p, FR(0), 3.14); 

*/
	jit_fretr(p, FR(0), sizeof(double));
	jit_generate_code(p);

	jit_dump_ops(p, JIT_DEBUG_ASSOC | JIT_DEBUG_LIVENESS);
	jit_dump_code(p, 0);

//	foo(1.2, 2.5);


	// check
	//printf("Check #1: %f\n", foo(1, 1.2, 2, 2.5));
	printf("Check #1: %f\n", foo(1.1, 1.2));
	for (int i = 0; i < 4; i++)
		printf("X:%f\t%f\n", g[i], h[i]);

//	for (int i = 0; i < 4; i++)
//		printf("%f\t%f\n", g[i], (double)h[i]);
	//printf("Check #2: %li\n", foo(100));
	//printf("Check #3: %li\n", foo(255));

	// cleanup
	jit_free(p);
	return 0;
}
