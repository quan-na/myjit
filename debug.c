#include <stdlib.h>
#include <stdio.h>

#include "myjit/jitlib.h"

typedef double (* pdfdd)(double, double);

int main()
{
	struct jit * p = jit_init(4, 4);

	static double g[4];
	g[0] = 1.2;
	g[1] = 2.3;
	g[2] = 3.4;
	g[3] = 4.5;
	printf(":TTTTTTTTT:%lx\n", (long)g);

	pdfdd foo;
	jit_prolog(p, &foo);

	int ar1 = jit_arg(p);


	jit_movi(p, R(0), 2 * sizeof(double));
	jit_movi(p, R(1), 10);
	jit_movi(p, R(2), g);
	jit_fmovi(p, FPR(0), -5.1);

	jit_fldxr(p, FPR(0), R(2), R(0), sizeof(double));

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
