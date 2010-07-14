#include <stdlib.h>
#include <stdio.h>

#include "myjit/jitlib.h"

typedef double (* pdfdd)(double, double);

int main()
{
	struct jit * p = jit_init(4, 4);

	pdfdd foo;
	jit_prolog(p, &foo);

	int ar1 = jit_arg(p);

//	jit_getarg(p, R(0), ar1, sizeof(long));

	jit_fmovi(p, FPR(0), 5.0);
	jit_fmovi(p, FPR(1), 2.1);
	jit_fdivr(p, FPR(1), FPR(0), FPR(1));

	jit_fretr(p, FPR(0));

	jit_generate_code(p);

	jit_dump_code(p, 0);

	// check
	printf("Check #1: %f\n", foo(1.2, 2.5));
	//printf("Check #2: %li\n", foo(100));
	//printf("Check #3: %li\n", foo(255));

	// cleanup
	jit_free(p);
	return 0;
}
