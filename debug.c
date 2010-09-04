#include <stdlib.h>
#include <stdio.h>

#include "myjit/jitlib.h"

//typedef double (* pdfdd)(int, float, short, double);

typedef double (* pdfdd)(/*float, float, float, float, float, float, float, float,float, float, float, float*/);

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

float foobar(float x)
{
//	double r = 123.4;
	printf("foo-bar:%f\n", x);
	return 88.99;
//	return eee(x, x + 1, x + 2);
	//return (int)(x * r);
}

int aaa()
{
	return 4567;
}

int main()
{
	struct jit * p = jit_init();
	static char * str = "Hello, World! Number of the day is %f!!!\n";

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
	/*
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(int));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
*/
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(int));
	
	/*
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	*/
//	int a = jit_allocai(p, 10);
//	jit_fmovr(p, FR(0), FR(1));
	jit_fmovi(p, FR(0), 12.2);
	jit_fmovi(p, FR(1), 16.4);
	jit_fmovr(p, FR(0), FR(1));
	jit_movi(p, R(0), 45);
	jit_extr(p, FR(0), R(0));

	jit_fretr(p, FR(0), sizeof(double));
	//jit_retr(p, R(0));
	jit_generate_code(p);

	jit_dump_ops(p, JIT_DEBUG_ASSOC | JIT_DEBUG_LIVENESS);
	jit_dump_code(p, 0);

//	foo(1.2, 2.5);
//	for (int i = 0; i < 4; i++)
//		printf("X:%f\n", g[i]);


	// check
	//printf("Check #1: %f\n", foo(1, 1.2, 2, 2.5));
	printf("Check #1: %f\n", foo(/*1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2*/));

//	for (int i = 0; i < 4; i++)
//		printf("%f\t%f\n", g[i], (double)h[i]);
	//printf("Check #2: %li\n", foo(100));
	//printf("Check #3: %li\n", foo(255));

	// cleanup
	jit_free(p);
	return 0;
}
