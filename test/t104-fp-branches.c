#include "tests.h"

#define CREATE_TEST_SUITE_REG(testname, _data_type, _jit_op, _operator) \
        CREATE_TEST_CASE_REG(testname ## 0, _data_type, _jit_op, _operator, 0, 0, 0) \
        CREATE_TEST_CASE_REG(testname ## 1, _data_type, _jit_op, _operator, 0, 0, 1) \
        CREATE_TEST_CASE_REG(testname ## 2, _data_type, _jit_op, _operator, 0, 1, 0) \
        CREATE_TEST_CASE_REG(testname ## 3, _data_type, _jit_op, _operator, 0, 1, 1) \
        CREATE_TEST_CASE_REG(testname ## 4, _data_type, _jit_op, _operator, 1, 0, 0) \
        CREATE_TEST_CASE_REG(testname ## 5, _data_type, _jit_op, _operator, 1, 0, 1) \
        CREATE_TEST_CASE_REG(testname ## 6, _data_type, _jit_op, _operator, 1, 1, 0) \
        CREATE_TEST_CASE_REG(testname ## 7, _data_type, _jit_op, _operator, 1, 1, 1) \

#define CREATE_TEST_SUITE_IMM(testname, _data_type, _jit_op, _operator) \
        CREATE_TEST_CASE_IMM(testname ## 0, _data_type, _jit_op, _operator, 0, 0, 0) \
        CREATE_TEST_CASE_IMM(testname ## 1, _data_type, _jit_op, _operator, 0, 0, 1) \
        CREATE_TEST_CASE_IMM(testname ## 2, _data_type, _jit_op, _operator, 0, 1, 0) \
        CREATE_TEST_CASE_IMM(testname ## 3, _data_type, _jit_op, _operator, 0, 1, 1) \
        CREATE_TEST_CASE_IMM(testname ## 4, _data_type, _jit_op, _operator, 1, 0, 0) \
        CREATE_TEST_CASE_IMM(testname ## 5, _data_type, _jit_op, _operator, 1, 0, 1) \
        CREATE_TEST_CASE_IMM(testname ## 6, _data_type, _jit_op, _operator, 1, 1, 0) \
        CREATE_TEST_CASE_IMM(testname ## 7, _data_type, _jit_op, _operator, 1, 1, 1) \

#define SETUP_TESTS(testname) \
        SETUP_TEST(testname ## 0); \
        SETUP_TEST(testname ## 1); \
        SETUP_TEST(testname ## 2); \
        SETUP_TEST(testname ## 3); \
        SETUP_TEST(testname ## 4); \
        SETUP_TEST(testname ## 5); \
        SETUP_TEST(testname ## 6); \
        SETUP_TEST(testname ## 7);


#define CREATE_TEST_CASE_REG(testname, _data_type, _jit_op, _operator, _equal, _negative, _greater) \
DEFINE_TEST(testname)\
{\
	pdfv f1;\
	jit_prolog(p, &f1);\
\
	_data_type firstval = 42;\
	_data_type secval = 28;\
	if (_equal) secval = firstval;\
	if (_greater) secval = 60;\
	if (_negative) secval *= -1;\
\
	jit_fmovi(p, FR(1), firstval);\
	jit_fmovi(p, FR(2), secval);\
\
	jit_op * br; \
	br = _jit_op(p, 0, FR(1), FR(2));\
	jit_fmovi(p, FR(3), -10); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_fmovi(p, FR(3), 10); \
	jit_patch(p, e); \
	jit_fretr(p, FR(3), sizeof(double)); \
\
        JIT_GENERATE_CODE(p);\
\
        if (firstval _operator secval) ASSERT_EQ_DOUBLE(10.0, f1()); \
        if (!(firstval _operator secval)) ASSERT_EQ_DOUBLE(-10.0, f1()); \
        return 0; \
}

#define CREATE_TEST_CASE_IMM(testname, _data_type, _jit_op, _operator, _equal, _negative, _greater) \
DEFINE_TEST(testname)\
{\
	pdfv f1;\
	jit_prolog(p, &f1);\
\
	_data_type firstval = 42;\
	_data_type secval = 28;\
	if (_equal) secval = firstval;\
	if (_greater) secval = 60;\
	if (_negative) secval *= -1;\
	\
	jit_fmovi(p, FR(1), firstval);\
\
	jit_op * br; \
	br = _jit_op(p, 0, FR(1), secval);\
	jit_fmovi(p, FR(3), -10); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_fmovi(p, FR(3), 10); \
	jit_patch(p, e); \
	jit_fretr(p, FR(3), sizeof(double)); \
\
        JIT_GENERATE_CODE(p);\
\
        if (firstval _operator secval) ASSERT_EQ_DOUBLE(10.0, f1()); \
        if (!(firstval _operator secval)) ASSERT_EQ_DOUBLE(-10.0, f1()); \
        return 0; \
}

CREATE_TEST_SUITE_REG(test01reg, double, jit_fbltr, <)
CREATE_TEST_SUITE_REG(test02reg, double, jit_fbler, <=)
CREATE_TEST_SUITE_REG(test03reg, double, jit_fbger, >=)
CREATE_TEST_SUITE_REG(test04reg, double, jit_fbgtr, >)
CREATE_TEST_SUITE_REG(test05reg, double, jit_fbeqr, ==)
CREATE_TEST_SUITE_REG(test06reg, double, jit_fbner, !=)

CREATE_TEST_SUITE_IMM(test21imm, double, jit_fblti, <)
CREATE_TEST_SUITE_IMM(test22imm, double, jit_fblei, <=)
CREATE_TEST_SUITE_IMM(test23imm, double, jit_fbgei, >=)
CREATE_TEST_SUITE_IMM(test24imm, double, jit_fbgti, >)
CREATE_TEST_SUITE_IMM(test25imm, double, jit_fbeqi, ==)
CREATE_TEST_SUITE_IMM(test26imm, double, jit_fbnei, !=)

void test_setup()
{
        test_filename = __FILE__;
        SETUP_TESTS(test01reg);
        SETUP_TESTS(test02reg);
        SETUP_TESTS(test03reg);
        SETUP_TESTS(test04reg);
        SETUP_TESTS(test05reg);
        SETUP_TESTS(test06reg);

        SETUP_TESTS(test21imm);
        SETUP_TESTS(test22imm);
        SETUP_TESTS(test23imm);
        SETUP_TESTS(test24imm);
        SETUP_TESTS(test25imm);
        SETUP_TESTS(test26imm);
}

