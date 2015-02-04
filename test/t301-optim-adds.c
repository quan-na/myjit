#include <limits.h>
#include "../myjit/jitlib.h"
#include "tests.h"

void test10()
{
	jit_value r;
	struct jit * p = jit_init();
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_muli(p, R(2), R(1), 2);
	jit_addi(p, R(2), R(2), 123);
	jit_retr(p, R(2));
	jit_generate_code(p);

	r = f1();

	if (r == (15 * 2 + 123)) SUCCESS(10);
	else FAIL(10);

	jit_free(p);
}

void test20()
{
	jit_value r;
	struct jit * p = jit_init();
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_muli(p, R(2), R(1), 2);
	jit_addr(p, R(2), R(2), R(0));
	jit_retr(p, R(2));
	jit_generate_code(p);

	r = f1();
	if (r == (15 * 2 + 7)) SUCCESS(20);
	else FAIL(20);

	jit_free(p);
}

void test21()
{
	jit_value r;
	struct jit * p = jit_init();
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_muli(p, R(2), R(1), 2);
	jit_addr(p, R(2), R(0), R(2));
	jit_retr(p, R(2));
	jit_generate_code(p);

	r = f1();
	if (r == (15 * 2 + 7)) SUCCESS(21);
	else FAIL(21);

	jit_free(p);
}

void test30()
{
	jit_value r;
	struct jit * p = jit_init();
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_addi(p, R(2), R(1), 2);
	jit_addr(p, R(2), R(0), R(2));
	jit_retr(p, R(2));
	jit_generate_code(p);
	jit_dump_code(p, 0);

	r = f1();
	if (r == (15 + 7 + 2)) SUCCESS(30);
	else FAIL(30);

	jit_free(p);
}

void test31()
{
	jit_value r;
	struct jit * p = jit_init();
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_addi(p, R(2), R(1), 2);
	jit_addr(p, R(2), R(2), R(0));
	jit_retr(p, R(2));
	jit_generate_code(p);
	jit_dump_code(p, 0);

	r = f1();
	if (r == (15 + 7 + 2)) SUCCESS(31);
	else FAIL(31);

	jit_free(p);
}

void test33()
{
	jit_value r;
	struct jit * p = jit_init();
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_addi(p, R(3), R(1), 2);
	jit_addr(p, R(2), R(3), R(0));
	jit_retr(p, R(2));
	jit_generate_code(p);
	jit_dump_code(p, 0);

	r = f1();
	if (r == (15 + 7 + 2)) SUCCESS(33);
	else FAIL(33);

	jit_free(p);
}


void test34()
{
	jit_value r;
	struct jit * p = jit_init();
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_addi(p, R(3), R(1), 2);
	jit_addr(p, R(2), R(0), R(3));
	jit_retr(p, R(2));
	jit_generate_code(p);
	jit_dump_code(p, 0);

	r = f1();
	if (r == (15 + 7 + 2)) SUCCESS(34);
	else FAIL(34);

	jit_free(p);
}

void test35()
{
	jit_value r;
	struct jit * p = jit_init();
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_addr(p, R(3), R(0), R(1));
	jit_addi(p, R(2), R(3), 20);
	jit_retr(p, R(2));
	jit_generate_code(p);
	jit_dump_code(p, 0);

	r = f1();
	if (r == (15 + 7 + 20)) SUCCESS(34);
	else FAIL(34);

	jit_free(p);
}

int main() 
{
	test10();
	test20();
	test21();
	test30();
	test31();
	test33();
	test34();
	test35();
}
