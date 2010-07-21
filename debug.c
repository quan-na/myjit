#include <stdlib.h>
#include <stdio.h>

#include "myjit/jitlib.h"

typedef double (* pdfdd)(double, double);

void foofn(double a, int b, double c)
{
	printf("Test: %f x %i = %f\n", a, b, c);
}

int main()
{
	struct jit * p = jit_init(4, 6);

	static double g[4];
	g[0] = 1.2;
	g[1] = 2.3;
	g[2] = 3.4;
	g[3] = 4.5;
	printf(":TTTTTTTTT:%lx\n", (long)g);

	pdfdd foo;
	jit_prolog(p, &foo);

	int ar1 = jit_arg(p);


//	jit_movi(p, R(0), 2 * sizeof(double));
//	jit_movi(p, R(1), 10);
//	jit_movi(p, R(2), g);
	jit_fmovi(p, FPR(0), 1.0);
	jit_fmovi(p, FPR(1), 2.0);
	jit_fmovi(p, FPR(2), 3.0);
	jit_fmovi(p, FPR(3), 4.0);
	jit_fmovi(p, FPR(4), 5.0);
	jit_fmovi(p, FPR(5), 6.0);

	jit_faddr(p, FPR(0), FPR(0), FPR(1));
	jit_faddi(p, FPR(0), FPR(0), 1.1);
	jit_fsubi(p, FPR(0), FPR(0), 2.5);

	/*
	jit_prepare(p, 3);
//	jit_fputargi(p, 1.2, sizeof(double));
	jit_fputargr(p, FPR(1), sizeof(double));
	jit_putargi(p, 666);
//	jit_fputargi(p, 2.2, sizeof(double));
	jit_fputargr(p, FPR(5), sizeof(double));
	jit_call(p, foofn);

	jit_fmovi(p, FPR(0), 123.456);
*/
	jit_fretr(p, FPR(0));

	jit_generate_code(p);

	jit_dump_code(p, 0);

//	foo(1.2, 2.5);
//	for (int i = 0; i < 4; i++)
//		printf("X:%f\n", g[i]);


	// check
	printf("Check #1: %f\n", foo(1.2, 2.5));
	//printf("Check #2: %li\n", foo(100));
	//printf("Check #3: %li\n", foo(255));

	// cleanup
	jit_free(p);
	return 0;
}
