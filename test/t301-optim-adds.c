#include "tests.h"

DEFINE_TEST(test10)
{
	plfv f1;
	jit_prolog(p, &f1);
	//jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_muli(p, R(2), R(1), 2);
	jit_addi(p, R(2), R(2), 123);
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(15 * 2 + 123, f1());
	return 0;
}

DEFINE_TEST(test20)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_muli(p, R(2), R(1), 2);
	jit_addr(p, R(2), R(2), R(0));
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(15 * 2 + 7, f1());
	return 0;
}

DEFINE_TEST(test21)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_muli(p, R(2), R(1), 2);
	jit_addr(p, R(2), R(0), R(2));
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(15 * 2 + 7, f1());
	return 0;
}

DEFINE_TEST(test22)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_muli(p, R(1), R(0), 4);
	jit_ori(p, R(1), R(1), 2);
	jit_retr(p, R(1));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(7 * 4 + 2, f1());
	return 0;
}

DEFINE_TEST(test23)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_lshi(p, R(1), R(0), 2);
	jit_ori(p, R(1), R(1), 2);
	jit_retr(p, R(1));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(7 * 4 + 2, f1());
	return 0;
}

DEFINE_TEST(test30)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_addi(p, R(2), R(1), 2);
	jit_addr(p, R(2), R(0), R(2));
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(15 + 2 + 7, f1());
	return 0;
}

DEFINE_TEST(test31)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_addi(p, R(2), R(1), 2);
	jit_addr(p, R(2), R(2), R(0));
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(15 + 2 + 7, f1());
	return 0;
}

DEFINE_TEST(test33)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_addi(p, R(3), R(1), 2);
	jit_addr(p, R(2), R(3), R(0));
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(15 + 2 + 7, f1());
	return 0;
}


DEFINE_TEST(test34)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_addi(p, R(3), R(1), 2);
	jit_addr(p, R(2), R(0), R(3));
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(15 + 2 + 7, f1());
	return 0;
}

DEFINE_TEST(test35)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_addr(p, R(3), R(0), R(1));
	jit_addi(p, R(2), R(3), 20);
	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(15 + 7 + 20, f1());
	return 0;
}

void test_setup() 
{
	test_filename = __FILE__;
	SETUP_TEST(test10);
	SETUP_TEST(test20);
	SETUP_TEST(test21);
	SETUP_TEST(test22);
	SETUP_TEST(test23);
	SETUP_TEST(test30);
	SETUP_TEST(test31);
	SETUP_TEST(test33);
	SETUP_TEST(test34);
	SETUP_TEST(test35);
}
