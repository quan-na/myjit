#include "tests.h"

DEFINE_TEST(test10)
{
	static unsigned char data;
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(1), &data);
	jit_movi(p, R(2), 10);
	jit_str(p, R(1), R(2), 1);

	jit_reti(p, 0);
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(0, f1());
	ASSERT_EQ(10, data);
	return 0;
}

DEFINE_TEST(test11)
{
	static unsigned int data;
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(2), 10);
	jit_sti(p, (unsigned char *)&data, R(2), 4);

	jit_reti(p, 0);
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(0, f1());
	ASSERT_EQ(10, data);
	return 0;
}

DEFINE_TEST(test12)
{
	static unsigned char data[10];
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(1), data);
	jit_movi(p, R(2), 3);
	jit_movi(p, R(3), 10);
	jit_stxr(p, R(1), R(2), R(3), 1);

	jit_reti(p, 0);
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(0, f1());
	ASSERT_EQ(10, data[3]);
	return 0;
}

void test_setup() 
{
	test_filename = __FILE__;
	SETUP_TEST(test10);
	SETUP_TEST(test11);
	SETUP_TEST(test12);
}
