#include "tests.h"

typedef double (* pdf10f)(float, float, float, float, float, float, float, float, float, float);
typedef double (* pdf10d)(double, double, double, double, double, double, double, double, double, double);

DEFINE_TEST(test1)
{
	pdf10f foo;
	jit_prolog(p, &foo);

	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	
	jit_getarg(p, FR(0), 0);
	jit_getarg(p, FR(1), 1);
	jit_getarg(p, FR(2), 2);
	jit_getarg(p, FR(3), 3);
	jit_getarg(p, FR(4), 4);
	jit_getarg(p, FR(5), 5);
	jit_getarg(p, FR(6), 6);
	jit_getarg(p, FR(7), 7);
	jit_getarg(p, FR(8), 8);
	jit_getarg(p, FR(9), 9);

	
	jit_fmovi(p, FR(10), 0);
	jit_faddr(p, FR(10), FR(10), FR(9));
	jit_faddr(p, FR(11), FR(10), FR(8));
	jit_faddr(p, FR(12), FR(11), FR(7));
	jit_faddr(p, FR(13), FR(12), FR(6));
	jit_faddr(p, FR(14), FR(13), FR(5));
	jit_faddr(p, FR(15), FR(14), FR(4));
	jit_faddr(p, FR(16), FR(15), FR(3));
	jit_faddr(p, FR(17), FR(16), FR(2));
	jit_faddr(p, FR(18), FR(17), FR(1));

	jit_faddr(p, FR(8), FR(8), FR(9));
	jit_faddr(p, FR(7), FR(7), FR(8));
	jit_faddr(p, FR(6), FR(6), FR(7));
	jit_faddr(p, FR(5), FR(5), FR(6));
	jit_faddr(p, FR(4), FR(4), FR(5));
	jit_faddr(p, FR(3), FR(3), FR(4));
	jit_faddr(p, FR(2), FR(2), FR(3));
	jit_faddr(p, FR(1), FR(1), FR(2));
	jit_faddr(p, FR(0), FR(0), FR(1));

	jit_faddr(p, FR(0), FR(0), FR(18));

	jit_fretr(p, FR(0), sizeof(double));

	JIT_GENERATE_CODE(p);
	ASSERT_EQ_DOUBLE(109.0, foo(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
	return 0;
}

DEFINE_TEST(test2)
{
	pdf10d foo;

	jit_prolog(p, &foo);

	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	
	jit_getarg(p, FR(0), 0);
	jit_getarg(p, FR(1), 1);
	jit_getarg(p, FR(2), 2);
	jit_getarg(p, FR(3), 3);
	jit_getarg(p, FR(4), 4);
	jit_getarg(p, FR(5), 5);
	jit_getarg(p, FR(6), 6);
	jit_getarg(p, FR(7), 7);
	jit_getarg(p, FR(8), 8);
	jit_getarg(p, FR(9), 9);

	jit_fmovi(p, FR(10), 0);
	jit_faddr(p, FR(10), FR(10), FR(9));
	jit_faddr(p, FR(11), FR(10), FR(8));
	jit_faddr(p, FR(12), FR(11), FR(7));
	jit_faddr(p, FR(13), FR(12), FR(6));
	jit_faddr(p, FR(14), FR(13), FR(5));
	jit_faddr(p, FR(15), FR(14), FR(4));
	jit_faddr(p, FR(16), FR(15), FR(3));
	jit_faddr(p, FR(17), FR(16), FR(2));
	jit_faddr(p, FR(18), FR(17), FR(1));

	jit_faddr(p, FR(8), FR(8), FR(9));
	jit_faddr(p, FR(7), FR(7), FR(8));
	jit_faddr(p, FR(6), FR(6), FR(7));
	jit_faddr(p, FR(5), FR(5), FR(6));
	jit_faddr(p, FR(4), FR(4), FR(5));
	jit_faddr(p, FR(3), FR(3), FR(4));
	jit_faddr(p, FR(2), FR(2), FR(3));
	jit_faddr(p, FR(1), FR(1), FR(2));
	jit_faddr(p, FR(0), FR(0), FR(1));

	jit_faddr(p, FR(0), FR(0), FR(18));

	jit_fretr(p, FR(0), sizeof(double));

	JIT_GENERATE_CODE(p);
	
	ASSERT_EQ_DOUBLE(109.0, foo(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
	return 0;
}

void test_setup()
{
	test_filename = __FILE__;
	SETUP_TEST(test1);
	SETUP_TEST(test2);
}
