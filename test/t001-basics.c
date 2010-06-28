#include <limits.h>
#include "../myjit/jitlib.h"
#include "tests.h"

void test1()
{
	long r;
	struct jit * p = jit_init(16);
	plfv f1;
	jit_prolog(p, &f1);
	jit_reti(p, 123);
	jit_generate_code(p);

	r = f1();
	if (r == 123) SUCCESS(1);
	else FAIL(1);

	jit_free(p);
}

void test2()
{
	long r;
	struct jit * p = jit_init(16);
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 100);
	jit_addi(p, R(0), R(0), 200);
	jit_addi(p, R(1), R(0), 200);
	jit_retr(p, R(1));
	jit_generate_code(p);

	r = f1();
	if (r == 500) SUCCESS(2);
	else FAIL(2);

	jit_free(p);
}

void test3()
{
	long r;
	struct jit * p = jit_init(16);
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 100);
	jit_addr(p, R(0), R(0), R(0));
	jit_subi(p, R(1), R(0), 50);
	jit_subr(p, R(2), R(1), R(0));
	jit_retr(p, R(2));
	jit_generate_code(p);

	r = f1();
	if (r == -50) SUCCESS(3);
	else FAIL(3);

	jit_free(p);
}

void test4()
{
	long r;
	struct jit * p = jit_init(16);
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 100);
	jit_movi(p, R(1), ULONG_MAX);
	jit_addcr(p, R(2), R(0), R(1));
	jit_addxr(p, R(2), R(0), R(0));
	jit_retr(p, R(2));
	jit_generate_code(p);

	r = f1();
	if (r == 201) SUCCESS(4);
	else FAIL(4);

	jit_free(p);
}

void test5()
{
	long r;
	struct jit * p = jit_init(16);
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), -100);
	jit_movi(p, R(1), ULONG_MAX);
	jit_subcr(p, R(2), R(0), R(1));
	jit_subxr(p, R(2), R(0), R(0));
	jit_retr(p, R(2));
	jit_generate_code(p);

	r = f1();
	if (r == -1) SUCCESS(4);
	else FAIL(4);

	jit_free(p);
}

int main() 
{
	test1();
	test2();
	test3();
	test4();
	test5();
}
