#include "tests.h"

int correct_result;

static int foobar(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
	correct_result = 1;
	if (a1 != -1) correct_result = 0;
	if (a2 != 1) correct_result = 0;
	if (a3 != 2) correct_result = 0;
	if (a4 != 4) correct_result = 0;
	if (a5 != 8) correct_result = 0;
	if (a6 != 16) correct_result = 0;
	if (a7 != 32) correct_result = 0;
	if (a8 != 64) correct_result = 0;
	if (a9 != 128) correct_result = 0;
	if (a10 != 256) correct_result = 0;
	return 666;
}

DEFINE_TEST(test1)
{
	plfv foo;

	jit_prolog(p, &foo);

	jit_movi(p, R(0), -1);
	jit_movi(p, R(1), 1);
	jit_movi(p, R(2), 2);
	jit_movi(p, R(3), 4);
	jit_movi(p, R(4), 8);
	jit_movi(p, R(5), 16);
	jit_movi(p, R(6), 32);
	jit_movi(p, R(7), 64);
	jit_movi(p, R(8), 128);
	jit_movi(p, R(9), 256);

	jit_prepare(p);
	jit_putargr(p, R(0));
	jit_putargr(p, R(1));
	jit_putargr(p, R(2));
	jit_putargr(p, R(3));
	jit_putargr(p, R(4));
	jit_putargr(p, R(5));
	jit_putargr(p, R(6));
	jit_putargr(p, R(7));
	jit_putargr(p, R(8));
	jit_putargr(p, R(9));
	jit_call(p, foobar);

	jit_retr(p, R(0));

	JIT_GENERATE_CODE(p);
	ASSERT_EQ(-1, foo());
	ASSERT_EQ(1, correct_result);
	return 0;
}

DEFINE_TEST(test2)
{
	plfv foo;

	jit_prolog(p, &foo);

	jit_movi(p, R(0), -1);
	jit_prepare(p);
	jit_putargi(p, -1);
	jit_putargi(p, 1);
	jit_putargi(p, 2);
	jit_putargi(p, 4);
	jit_putargi(p, 8);
	jit_putargi(p, 16);
	jit_putargi(p, 32);
	jit_putargi(p, 64);
	jit_putargi(p, 128);
	jit_putargi(p, 256);
	jit_call(p, foobar);

	jit_retr(p, R(0));

	JIT_GENERATE_CODE(p);
	ASSERT_EQ(-1, foo());
	ASSERT_EQ(1, correct_result);
	return 0;
}

DEFINE_TEST(test3)
{
	plfv foo;

	jit_prolog(p, &foo);

	jit_movi(p, R(0), -1);
	jit_movi(p, R(1), 1);
	jit_movi(p, R(2), 2);
	jit_movi(p, R(3), 4);
	jit_movi(p, R(4), 8);
	jit_movi(p, R(5), 16);
	jit_movi(p, R(6), 32);
	jit_movi(p, R(7), 64);
	jit_movi(p, R(8), 128);
	jit_movi(p, R(9), 256);

	jit_force_spill(p, R(5));
	jit_force_spill(p, R(6));
	jit_force_spill(p, R(7));
	jit_force_spill(p, R(8));
	jit_force_spill(p, R(9));

	jit_prepare(p);
	jit_putargr(p, R(0));
	jit_putargr(p, R(1));
	jit_putargr(p, R(2));
	jit_putargr(p, R(3));
	jit_putargr(p, R(4));
	jit_putargr(p, R(5));
	jit_putargr(p, R(6));
	jit_putargr(p, R(7));
	jit_putargr(p, R(8));
	jit_putargr(p, R(9));
	jit_call(p, foobar);

	jit_retr(p, R(0));

	JIT_GENERATE_CODE(p);
	ASSERT_EQ(-1, foo());
	ASSERT_EQ(1, correct_result);
	return 0;
}

void test_setup()
{
	test_filename = __FILE__;
	SETUP_TEST(test1);
	SETUP_TEST(test2);
	SETUP_TEST(test3);
}
