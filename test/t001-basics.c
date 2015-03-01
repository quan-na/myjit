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

DEFINE_TEST(test10)
{
	static short y = -2;

	plfv f1;
	jit_prolog(p, &f1);
	jit_ldi(p, R(0), &y, sizeof(short));
	jit_retr(p, R(0));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(-2, f1());
	return 0;
}

DEFINE_TEST(test11)
{
	static short y = -2;

	plfv f1;
	jit_prolog(p, &f1);
	jit_ldi_u(p, R(0), &y, sizeof(short));
	jit_retr(p, R(0));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(65534, f1());
	return 0;
}

DEFINE_TEST(test12)
{
	static short y = -2;

	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(1), &y);
	jit_ldr(p, R(0), R(1), sizeof(short));
	jit_retr(p, R(0));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(-2, f1());
	return 0;
}

DEFINE_TEST(test13)
{
	static short y = -2;

	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(1), &y);
	jit_ldr_u(p, R(0), R(1), sizeof(short));
	jit_retr(p, R(0));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(65534, f1());
	return 0;
}


DEFINE_TEST(test14)
{
	static short y[] = { -2, -3, -5 };

	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(1), &y);
	jit_ldxi(p, R(0), R(1), sizeof(short) * 1, sizeof(short));
	jit_retr(p, R(0));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(-3, f1());
	return 0;
}

DEFINE_TEST(test15)
{
	static short y[] = { -2, -3, -4 };

	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(1), &y);
	jit_ldxi_u(p, R(0), R(1), sizeof(short) * 2, sizeof(short));
	jit_retr(p, R(0));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(65532, f1());
	return 0;
}

DEFINE_TEST(test16)
{
	static short y[] = { -2, -3, -4, -5, -6 };

	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(2), 10);
	jit_movi(p, R(1), sizeof(short));
	jit_addi(p, R(1), R(1),  sizeof(short));
	jit_stxi(p, &y, R(1), R(2), sizeof(short));
	jit_reti(p, 42);
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(42, f1());
	ASSERT_EQ(10, y[2]);
	return 0;
}

DEFINE_TEST(test20)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 0xcafe);
	jit_hmuli(p, R(0), R(0), 1L << (REG_SIZE * 8 - 8));
	
	jit_retr(p, R(0));
	JIT_GENERATE_CODE(p);
	ASSERT_EQ(0xca, f1());
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

	SETUP_TEST(test10);
	SETUP_TEST(test11);
	SETUP_TEST(test12);
	SETUP_TEST(test13);
	SETUP_TEST(test14);
	SETUP_TEST(test15);
	SETUP_TEST(test16);

	SETUP_TEST(test20);
}
