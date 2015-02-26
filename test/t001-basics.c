#include <limits.h>
#include "tests.h"

DEFINE_TEST(test1)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_reti(p, 123);
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(123, f1());
	return 0;
}

DEFINE_TEST(test2)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 100);
	jit_addi(p, R(0), R(0), 200);
	jit_addi(p, R(1), R(0), 200);
	jit_retr(p, R(1));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(500, f1());
	return 0;
}

DEFINE_TEST(test3)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 100);
	jit_addr(p, R(0), R(0), R(0));
	jit_subi(p, R(1), R(0), 50);
	jit_subr(p, R(2), R(1), R(0));
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(-50, f1());
	return 0;
}

DEFINE_TEST(test4)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 100);
	jit_movi(p, R(1), ULONG_MAX);
	jit_addcr(p, R(2), R(0), R(1));
	jit_addxr(p, R(2), R(0), R(0));
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(201, f1());
	return 0;
}

DEFINE_TEST(test5)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), -100);
	jit_movi(p, R(1), ULONG_MAX);
	jit_subcr(p, R(2), R(0), R(1));
	jit_subxr(p, R(2), R(0), R(0));
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(-1, f1());
	return 0;
}

DEFINE_TEST(test6)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), -100);
	jit_movi(p, R(1), ULONG_MAX);
	jit_movi(p, R(2), 10);
	jit_subcr(p, R(1), R(0), R(1));
	
	jit_subxr(p, R(2), R(2), R(0));
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(109, f1());
	return 0;
}

DEFINE_TEST(test7)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 25);
	jit_movi(p, R(1), 30);
	jit_rsbr(p, R(1), R(1), R(0));
	
	jit_retr(p, R(1));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(-5, f1());
	return 0;
}

void test_setup()
{
	test_filename = __FILE__;
	SETUP_TEST(test1);
	SETUP_TEST(test2);
	SETUP_TEST(test3);
	SETUP_TEST(test4);
	SETUP_TEST(test5);
	SETUP_TEST(test6);
	SETUP_TEST(test7);
}
