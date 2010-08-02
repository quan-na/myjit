#include <limits.h>
#include "../myjit/jitlib.h"
#include "tests.h"

void test1()
{
	double r;
	struct jit * p = jit_init();
	pdfv f1;
	jit_prolog(p, &f1);
	jit_freti(p, 123);
	jit_generate_code(p);

	r = f1();
	if (equal(r, 123, 0.001)) SUCCESS(1);
	else FAIL(1);

	jit_free(p);
}

void test2()
{
	double r;
	struct jit * p = jit_init();
	pdfv f1;
	jit_prolog(p, &f1);
	jit_fmovi(p, FPR(0), 100);
	jit_faddi(p, FPR(0), FPR(0), 200);
	jit_faddi(p, FPR(1), FPR(0), 200);
	jit_fretr(p, FPR(1));
	jit_generate_code(p);

	r = f1();
	if (equal(r, 500, 0.001)) SUCCESS(2);
	else FAIL(2);

	jit_free(p);
}

void test3()
{
	double r;
	struct jit * p = jit_init();
	pdfv f1;
	jit_prolog(p, &f1);
	jit_fmovi(p, FPR(0), 100);
	jit_faddr(p, FPR(0), FPR(0), FPR(0));
	jit_fsubi(p, FPR(1), FPR(0), 50);
	jit_fsubr(p, FPR(2), FPR(1), FPR(0));
	jit_fretr(p, FPR(2));
	jit_generate_code(p);

	r = f1();
	if (equal(r, -50, 0.001)) SUCCESS(3);
	else FAIL(3);

	jit_free(p);
}

void test4()
{
	double r;
	struct jit * p = jit_init();
	pdfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 100);
	jit_movi(p, R(1), 50);

	jit_extr(p, FPR(0), R(0));
	jit_extr(p, FPR(1), R(1));

	jit_faddr(p, FPR(0), FPR(0), FPR(0));
	jit_fsubr(p, FPR(1), FPR(0), FPR(1));
	jit_fsubr(p, FPR(2), FPR(1), FPR(0));
	jit_fretr(p, FPR(2));
	jit_generate_code(p);
	jit_dump_code(p, 0);

	r = f1();
	if (equal(r, -50, 0.001)) SUCCESS(4);
	else FAIL(4);

	jit_free(p);
}


int main() 
{
	test1();
	test2();
	test3();
	test4();
}
