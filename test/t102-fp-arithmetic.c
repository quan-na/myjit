#include "tests.h"

// imm -- same reg
#define TEST_IMM_SAME_REG(_name, _op, _arg1, _arg2, _expected_value) \
DEFINE_TEST(_name) \
{ \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FR(0), _arg1); \
	_op(p, FR(0), FR(0), _arg2); \
	jit_fretr(p, FR(0), sizeof(double)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ_DOUBLE(_expected_value, f1()); \
	return 0;\
} 

#define TEST_IMM_OTHER_REG(_name, _op, _arg1, _arg2, _expected_value) \
DEFINE_TEST(_name) \
{ \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FR(0), _arg1); \
	_op(p, FR(1), FR(0), _arg2); \
	jit_fretr(p, FR(1), sizeof(double)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ_DOUBLE(_expected_value, f1()); \
	return 0;\
}

#define TEST_REG_SAME_REG(_name, _op, _arg1, _arg2, _expected_value) \
DEFINE_TEST(_name) \
{ \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FR(0), _arg1); \
	jit_fmovi(p, FR(1), _arg2); \
	_op(p, FR(1), FR(0), FR(1)); \
	jit_fretr(p, FR(1), sizeof(double)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ_DOUBLE(_expected_value, f1()); \
	return 0;\
}

#define TEST_REG_OTHER_REG(_name, _op, _arg1, _arg2, _expected_value) \
DEFINE_TEST(_name) \
{ \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FR(0), _arg1); \
	jit_fmovi(p, FR(1), _arg2); \
	_op(p, FR(2), FR(0), FR(1)); \
	jit_fretr(p, FR(2), sizeof(double)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ_DOUBLE(_expected_value, f1()); \
	return 0;\
}

#define TEST_UNARY_REG_OTHER_REG(_name, _op, _arg1, _expected_value) \
DEFINE_TEST(_name) \
{ \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FR(0), _arg1); \
	_op(p, FR(2), FR(0)); \
	jit_fretr(p, FR(2), sizeof(double)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ_DOUBLE(_expected_value, f1()); \
	return 0;\
}

#define TEST_UNARY_REG_SAME_REG(_name, _op, _arg1, _expected_value) \
DEFINE_TEST(_name) \
{ \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FR(0), _arg1); \
	_op(p, FR(0), FR(0)); \
	jit_fretr(p, FR(0), sizeof(double)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ_DOUBLE(_expected_value, f1()); \
	return 0;\
}


TEST_IMM_SAME_REG(test10, jit_faddi, 42, 28, 70.0) 
TEST_IMM_OTHER_REG(test11, jit_faddi, 42, 28, 70.0) 
TEST_REG_SAME_REG(test12, jit_faddr, 42, 28, 70.0) 
TEST_REG_OTHER_REG(test13, jit_faddr, 42, 28, 70.0) 

TEST_IMM_SAME_REG(test20, jit_fsubi, 42, 28, 14.0) 
TEST_IMM_OTHER_REG(test21, jit_fsubi, 42, 28, 14.0) 
TEST_REG_SAME_REG(test22, jit_fsubr, 42, 28, 14.0) 
TEST_REG_OTHER_REG(test23, jit_fsubr, 42, 28, 14.0) 

TEST_IMM_SAME_REG(test30, jit_frsbi, 42, 28, -14.0) 
TEST_IMM_OTHER_REG(test31, jit_frsbi, 42, 28, -14.0) 
TEST_REG_SAME_REG(test32, jit_frsbr, 42, 28, -14.0) 
TEST_REG_OTHER_REG(test33, jit_frsbr, 42, 28, -14.0) 

TEST_IMM_SAME_REG(test40, jit_fmuli, 42, 28, 1176.0) 
TEST_IMM_OTHER_REG(test41, jit_fmuli, 42, 28, 1176.0) 
TEST_REG_SAME_REG(test42, jit_fmulr, 42, 28, 1176.0) 
TEST_REG_OTHER_REG(test43, jit_fmulr, 42, 28, 1176.0) 

TEST_IMM_SAME_REG(test44, jit_fmuli, 42, 4, 168.0) 
TEST_IMM_OTHER_REG(test45, jit_fmuli, 42, 4, 168.0) 
TEST_REG_SAME_REG(test46, jit_fmulr, 42, 4, 168.0) 
TEST_REG_OTHER_REG(test47, jit_fmulr, 42, 4, 168.0) 

TEST_IMM_SAME_REG(test48, jit_fmuli, 42, 5, 210.0) 
TEST_IMM_OTHER_REG(test49, jit_fmuli, 42, 5, 210.0) 
TEST_REG_SAME_REG(test50, jit_fmulr, 42, 5, 210.0) 
TEST_REG_OTHER_REG(test51, jit_fmulr, 42, 5, 210.0) 

TEST_IMM_SAME_REG(test52, jit_fmuli, 42, 2,84.0) 
TEST_IMM_OTHER_REG(test53, jit_fmuli, 42, 2, 84.0) 
TEST_REG_SAME_REG(test54, jit_fmulr, 42, 2, 84.0) 
TEST_REG_OTHER_REG(test55, jit_fmulr, 42, 2, 84.0) 

TEST_IMM_SAME_REG(test56, jit_fmuli, 42, 9, 378.0) 
TEST_IMM_OTHER_REG(test57, jit_fmuli, 42, 9, 378.0) 
TEST_REG_SAME_REG(test58, jit_fmulr, 42, 9, 378.0) 
TEST_REG_OTHER_REG(test59, jit_fmulr, 42, 9, 378.0) 

TEST_IMM_SAME_REG(test60, jit_fdivi, 42, 4, 42.0 / 4.0) 
TEST_IMM_OTHER_REG(test61, jit_fdivi, 42, 4, 42.0 / 4.0) 
TEST_REG_SAME_REG(test62, jit_fdivr, 42, 4, 42.0 / 4.0) 
TEST_REG_OTHER_REG(test63, jit_fdivr, 42, 4, 42.0 / 4.0) 

TEST_IMM_SAME_REG(test64, jit_fdivi, 42, 9, 42.0 / 9.0) 
TEST_IMM_OTHER_REG(test65, jit_fdivi, 42, 9, 42.0 / 9.0) 
TEST_REG_SAME_REG(test66, jit_fdivr, 42, 9, 42.0 / 9.0) 
TEST_REG_OTHER_REG(test67, jit_fdivr, 42, 9, 42.0 / 9.0) 

TEST_UNARY_REG_SAME_REG(test160, jit_fnegr, 42, -42.0)
TEST_UNARY_REG_OTHER_REG(test161, jit_fnegr, 42, -42.0)



void test_setup() 
{
	test_filename = __FILE__;
	SETUP_TEST(test10);
	SETUP_TEST(test11);
	SETUP_TEST(test12);
	SETUP_TEST(test13);
	SETUP_TEST(test20);
	SETUP_TEST(test21);
	SETUP_TEST(test22);
	SETUP_TEST(test23);
	SETUP_TEST(test30);
	SETUP_TEST(test31);
	SETUP_TEST(test32);
	SETUP_TEST(test33);
	SETUP_TEST(test40);
	SETUP_TEST(test41);
	SETUP_TEST(test42);
	SETUP_TEST(test43);
	SETUP_TEST(test44);

	SETUP_TEST(test45);
	SETUP_TEST(test46);
	SETUP_TEST(test47);
	SETUP_TEST(test48);
	SETUP_TEST(test49);
	SETUP_TEST(test50);
	SETUP_TEST(test51);
	SETUP_TEST(test52);
	SETUP_TEST(test53);
	SETUP_TEST(test54);
	SETUP_TEST(test55);
	SETUP_TEST(test56);
	SETUP_TEST(test57);
	SETUP_TEST(test58);
	SETUP_TEST(test59);

	SETUP_TEST(test60);
	SETUP_TEST(test61);
	SETUP_TEST(test62);
	SETUP_TEST(test63);
	SETUP_TEST(test64);
	SETUP_TEST(test65);
	SETUP_TEST(test66);
	SETUP_TEST(test67);
	
	SETUP_TEST(test160);
	SETUP_TEST(test161);
}
