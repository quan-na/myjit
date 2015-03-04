#include "tests.h"

int correct_result;

static int foobar(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8, double a9, double a10)
{
	correct_result = 1;
        if (!equals(a1, -1, 0.0001)) correct_result = 0;
        if (!equals(a2, 1, 0.0001)) correct_result = 0;
        if (!equals(a3, 2, 0.0001)) correct_result = 0;
        if (!equals(a4, 4, 0.0001)) correct_result = 0;
        if (!equals(a5, 8, 0.0001)) correct_result = 0;
        if (!equals(a6, 16, 0.0001)) correct_result = 0;
        if (!equals(a7, 32, 0.0001)) correct_result = 0;
        if (!equals(a8, 64, 0.0001)) correct_result = 0;
        if (!equals(a9, 128, 0.0001)) correct_result = 0;
        if (!equals(a10, 222.222, 0.0001)) correct_result = 0;
        return 666;
}

static int barbaz(float a1, float a2, float a3, float a4, float a5, float a6, float a7, float a8, float a9, float a10, float a11)
{
	correct_result = 1;
        if (!equals(a1, -1, 0.0001)) correct_result = 0;
        if (!equals(a2, 1, 0.0001)) correct_result = 0;
        if (!equals(a3, 2, 0.0001)) correct_result = 0;
        if (!equals(a4, 4, 0.0001)) correct_result = 0;
        if (!equals(a5, 8, 0.0001)) correct_result = 0;
        if (!equals(a6, 16, 0.0001)) correct_result = 0;
        if (!equals(a7, 32, 0.0001)) correct_result = 0;
        if (!equals(a8, 64, 0.0001)) correct_result = 0;
        if (!equals(a9, 128, 0.0001)) correct_result = 0;
        if (!equals(a10, 222.222, 0.0001)) correct_result = 0;
        if (!equals(a11, 256, 0.0001)) correct_result = 0;
        return 666;
}

DEFINE_TEST(test1)
{
	correct_result = 2;
	plfv foo;

	jit_prolog(p, &foo);

	jit_fmovi(p, FR(0), -1);
	jit_fmovi(p, FR(1), 1);
	jit_fmovi(p, FR(2), 2);
	jit_fmovi(p, FR(3), 4);
	jit_fmovi(p, FR(4), 8);
	jit_fmovi(p, FR(5), 16);
	jit_fmovi(p, FR(6), 32);
	jit_fmovi(p, FR(7), 64);
	jit_fmovi(p, FR(8), 128);
	//jit_fmovi(p, FR(9), 256);
	//jit_movi(p, R(10), 123);

	jit_prepare(p);
	jit_fputargr(p, FR(0), sizeof(double));
	jit_fputargr(p, FR(1), sizeof(double));
	jit_fputargr(p, FR(2), sizeof(double));
	jit_fputargr(p, FR(3), sizeof(double));
	jit_fputargr(p, FR(4), sizeof(double));
	jit_fputargr(p, FR(5), sizeof(double));
	jit_fputargr(p, FR(6), sizeof(double));
	jit_fputargr(p, FR(7), sizeof(double));
	jit_fputargr(p, FR(8), sizeof(double));
	jit_fputargi(p, 222.222, sizeof(double));
	//jit_fputargr(p, FR(9), sizeof(double));
	jit_call(p, foobar);

	jit_reti(p, 0);

	JIT_GENERATE_CODE(p);
	ASSERT_EQ(0, foo());
	ASSERT_EQ(1, correct_result);
	return 0;

}

DEFINE_TEST(test2)
{
	correct_result = 2;
	plfv foo;

	jit_prolog(p, &foo);

	jit_fmovi(p, FR(0), -1);
	jit_fmovi(p, FR(1), 1);
	jit_fmovi(p, FR(2), 2);
	jit_fmovi(p, FR(3), 4);
	jit_fmovi(p, FR(4), 8);
	jit_fmovi(p, FR(5), 16);
	jit_fmovi(p, FR(6), 32);
	jit_fmovi(p, FR(7), 64);
	jit_fmovi(p, FR(8), 128);
	jit_fmovi(p, FR(9), 256);
	//jit_movi(p, R(10), 123);

	jit_prepare(p);
	jit_fputargr(p, FR(0), sizeof(float));
	jit_fputargr(p, FR(1), sizeof(float));
	jit_fputargr(p, FR(2), sizeof(float));
	jit_fputargr(p, FR(3), sizeof(float));
	jit_fputargr(p, FR(4), sizeof(float));
	jit_fputargr(p, FR(5), sizeof(float));
	jit_fputargr(p, FR(6), sizeof(float));
	jit_fputargr(p, FR(7), sizeof(float));
	jit_fputargr(p, FR(8), sizeof(float));
	jit_fputargi(p, 222.222, sizeof(float));
	jit_fputargr(p, FR(9), sizeof(float));
	jit_call(p, barbaz);

	jit_reti(p, 0);

	
	JIT_GENERATE_CODE(p);
	ASSERT_EQ(0, foo());
	ASSERT_EQ(1, correct_result);
	return 0;
}

DEFINE_TEST(test3)
{
	correct_result = 2;
	plfv foo;

	jit_prolog(p, &foo);

	jit_prepare(p);
	jit_fputargi(p, -1.0, sizeof(double));
	jit_fputargi(p, 1.0, sizeof(double));
	jit_fputargi(p, 2.0, sizeof(double));
	jit_fputargi(p, 4.0, sizeof(double));
	jit_fputargi(p, 8.0, sizeof(double));
	jit_fputargi(p, 16.0, sizeof(double));
	jit_fputargi(p, 32.0, sizeof(double));
	jit_fputargi(p, 64.0, sizeof(double));
	jit_fputargi(p, 128.0, sizeof(double));
	jit_fputargi(p, 222.222, sizeof(double));
	jit_call(p, foobar);

	jit_reti(p, 0);

	JIT_GENERATE_CODE(p);
	ASSERT_EQ(0, foo());
	ASSERT_EQ(1, correct_result);
	return 0;
}

DEFINE_TEST(test4)
{
	correct_result = 2;
	plfv foo;

	jit_prolog(p, &foo);

	jit_prepare(p);
	jit_fputargi(p, -1.0, sizeof(float));
	jit_fputargi(p, 1.0, sizeof(float));
	jit_fputargi(p, 2.0, sizeof(float));
	jit_fputargi(p, 4.0, sizeof(float));
	jit_fputargi(p, 8.0, sizeof(float));
	jit_fputargi(p, 16.0, sizeof(float));
	jit_fputargi(p, 32.0, sizeof(float));
	jit_fputargi(p, 64.0, sizeof(float));
	jit_fputargi(p, 128.0, sizeof(float));
	jit_fputargi(p, 222.222, sizeof(float));
	jit_fputargi(p, 256.0, sizeof(float));
	jit_call(p, barbaz);

	jit_reti(p, 0);

	JIT_GENERATE_CODE(p);
	ASSERT_EQ(0, foo());
	ASSERT_EQ(1, correct_result);
	return 0;
}


DEFINE_TEST(test5)
{
	correct_result = 2;
	plfv foo;

	jit_prolog(p, &foo);

	jit_fmovi(p, FR(9), 222.222);
	jit_fmovi(p, FR(0), -1);
	jit_fmovi(p, FR(1), 1);
	jit_fmovi(p, FR(2), 2);
	jit_fmovi(p, FR(3), 4);
	jit_fmovi(p, FR(4), 8);
	jit_fmovi(p, FR(5), 16);
	jit_fmovi(p, FR(6), 32);
	jit_fmovi(p, FR(7), 64);
	jit_fmovi(p, FR(8), 128);

	jit_prepare(p);
	jit_fputargr(p, FR(0), sizeof(double));
	jit_fputargr(p, FR(1), sizeof(double));
	jit_fputargr(p, FR(2), sizeof(double));
	jit_fputargr(p, FR(3), sizeof(double));
	jit_fputargr(p, FR(4), sizeof(double));
	jit_fputargr(p, FR(5), sizeof(double));
	jit_fputargr(p, FR(6), sizeof(double));
	jit_fputargr(p, FR(7), sizeof(double));
	jit_fputargr(p, FR(8), sizeof(double));
	jit_fputargr(p, FR(9), sizeof(double));
	jit_call(p, foobar);

	jit_reti(p, 0);

	JIT_GENERATE_CODE(p);
	ASSERT_EQ(0, foo());
	ASSERT_EQ(1, correct_result);
	return 0;
}

DEFINE_TEST(test6)
{
	correct_result = 2;
	plfv foo;

	jit_prolog(p, &foo);

	jit_fmovi(p, FR(9), 256);
	jit_fmovi(p, FR(0), -1);
	jit_fmovi(p, FR(1), 1);
	jit_fmovi(p, FR(2), 2);
	jit_fmovi(p, FR(3), 4);
	jit_fmovi(p, FR(4), 8);
	jit_fmovi(p, FR(5), 16);
	jit_fmovi(p, FR(6), 32);
	jit_fmovi(p, FR(7), 64);
	jit_fmovi(p, FR(8), 128);
	//jit_movi(p, R(10), 123);

	jit_prepare(p);
	jit_fputargr(p, FR(0), sizeof(float));
	jit_fputargr(p, FR(1), sizeof(float));
	jit_fputargr(p, FR(2), sizeof(float));
	jit_fputargr(p, FR(3), sizeof(float));
	jit_fputargr(p, FR(4), sizeof(float));
	jit_fputargr(p, FR(5), sizeof(float));
	jit_fputargr(p, FR(6), sizeof(float));
	jit_fputargr(p, FR(7), sizeof(float));
	jit_fputargr(p, FR(8), sizeof(float));
	jit_fputargi(p, 222.222, sizeof(float));
	jit_fputargr(p, FR(9), sizeof(float));
	jit_call(p, barbaz);

	jit_reti(p, 0);

	
	JIT_GENERATE_CODE(p);
	ASSERT_EQ(0, foo());
	ASSERT_EQ(1, correct_result);
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
}
