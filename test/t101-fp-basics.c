#include "tests.h"

DEFINE_TEST(test1)
{
	pdfv f1;
	jit_prolog(p, &f1);
	jit_freti(p, 123, sizeof(double));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ_DOUBLE(123.0, f1());
	return 0;
}

DEFINE_TEST(test2)
{
	pdfv f1;
	jit_prolog(p, &f1);
	jit_fmovi(p, FR(0), 100);
	jit_faddi(p, FR(0), FR(0), 200);
	jit_faddi(p, FR(1), FR(0), 200);
	jit_fretr(p, FR(1), sizeof(double));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ_DOUBLE(500.0, f1());
	return 0;
}

DEFINE_TEST(test3)
{
	pdfv f1;
	jit_prolog(p, &f1);
	jit_fmovi(p, FR(0), 100);
	jit_faddr(p, FR(0), FR(0), FR(0));
	jit_fsubi(p, FR(1), FR(0), 50);
	jit_fsubr(p, FR(2), FR(1), FR(0));
	jit_fretr(p, FR(2), sizeof(double));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ_DOUBLE(-50.0, f1());
	return 0;
}

DEFINE_TEST(test4)
{
	pdfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 100);
	jit_movi(p, R(1), 50);

	jit_extr(p, FR(0), R(0));
	jit_extr(p, FR(1), R(1));

	jit_faddr(p, FR(0), FR(0), FR(0));
	jit_fsubr(p, FR(1), FR(0), FR(1));
	jit_fsubr(p, FR(2), FR(1), FR(0));
	jit_fretr(p, FR(2), sizeof(double));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ_DOUBLE(-50.0, f1());
	return 0;
}


void test_setup() 
{
	test_filename = __FILE__;
	SETUP_TEST(test1);
	SETUP_TEST(test2);
	SETUP_TEST(test3);
	SETUP_TEST(test4);
}
