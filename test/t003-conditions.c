#include <limits.h>
#include "../myjit/jitlib.h"
#include "tests.h"

#define CREATE_TEST_SUITE_REG(testname, _data_type, _jit_op, _operator) \
	CREATE_TEST_CASE_REG(testname ## 0, _data_type, _jit_op, _operator, 0, 0, 0, 0) \
	CREATE_TEST_CASE_REG(testname ## 1, _data_type, _jit_op, _operator, 0, 0, 0, 1) \
	CREATE_TEST_CASE_REG(testname ## 2, _data_type, _jit_op, _operator, 0, 0, 1, 0) \
	CREATE_TEST_CASE_REG(testname ## 3, _data_type, _jit_op, _operator, 0, 0, 1, 1) \
	CREATE_TEST_CASE_REG(testname ## 4, _data_type, _jit_op, _operator, 0, 1, 0, 0) \
	CREATE_TEST_CASE_REG(testname ## 5, _data_type, _jit_op, _operator, 0, 1, 0, 1) \
	CREATE_TEST_CASE_REG(testname ## 6, _data_type, _jit_op, _operator, 0, 1, 1, 0) \
	CREATE_TEST_CASE_REG(testname ## 7, _data_type, _jit_op, _operator, 0, 1, 1, 1) \
	CREATE_TEST_CASE_REG(testname ## 8, _data_type, _jit_op, _operator, 1, 0, 0, 0) \
	CREATE_TEST_CASE_REG(testname ## 9, _data_type, _jit_op, _operator, 1, 0, 0, 1) \
	CREATE_TEST_CASE_REG(testname ## 10, _data_type, _jit_op, _operator, 1, 0, 1, 0) \
	CREATE_TEST_CASE_REG(testname ## 11, _data_type, _jit_op, _operator, 1, 0, 1, 1) \
	CREATE_TEST_CASE_REG(testname ## 12, _data_type, _jit_op, _operator, 1, 1, 0, 0) \
	CREATE_TEST_CASE_REG(testname ## 13, _data_type, _jit_op, _operator, 1, 1, 0, 1) \
	CREATE_TEST_CASE_REG(testname ## 14, _data_type, _jit_op, _operator, 1, 1, 1, 0) \
	CREATE_TEST_CASE_REG(testname ## 15, _data_type, _jit_op, _operator, 1, 1, 1, 1) \

#define CREATE_TEST_SUITE_IMM(testname, _data_type, _jit_op, _operator) \
	CREATE_TEST_CASE_IMM(testname ## 0, _data_type, _jit_op, _operator, 0, 0, 0, 0) \
	CREATE_TEST_CASE_IMM(testname ## 1, _data_type, _jit_op, _operator, 0, 0, 0, 1) \
	CREATE_TEST_CASE_IMM(testname ## 2, _data_type, _jit_op, _operator, 0, 0, 1, 0) \
	CREATE_TEST_CASE_IMM(testname ## 3, _data_type, _jit_op, _operator, 0, 0, 1, 1) \
	CREATE_TEST_CASE_IMM(testname ## 4, _data_type, _jit_op, _operator, 0, 1, 0, 0) \
	CREATE_TEST_CASE_IMM(testname ## 5, _data_type, _jit_op, _operator, 0, 1, 0, 1) \
	CREATE_TEST_CASE_IMM(testname ## 6, _data_type, _jit_op, _operator, 0, 1, 1, 0) \
	CREATE_TEST_CASE_IMM(testname ## 7, _data_type, _jit_op, _operator, 0, 1, 1, 1) \
	CREATE_TEST_CASE_IMM(testname ## 8, _data_type, _jit_op, _operator, 1, 0, 0, 0) \
	CREATE_TEST_CASE_IMM(testname ## 9, _data_type, _jit_op, _operator, 1, 0, 0, 1) \
	CREATE_TEST_CASE_IMM(testname ## 10, _data_type, _jit_op, _operator, 1, 0, 1, 0) \
	CREATE_TEST_CASE_IMM(testname ## 11, _data_type, _jit_op, _operator, 1, 0, 1, 1) \
	CREATE_TEST_CASE_IMM(testname ## 12, _data_type, _jit_op, _operator, 1, 1, 0, 0) \
	CREATE_TEST_CASE_IMM(testname ## 13, _data_type, _jit_op, _operator, 1, 1, 0, 1) \
	CREATE_TEST_CASE_IMM(testname ## 14, _data_type, _jit_op, _operator, 1, 1, 1, 0) \
	CREATE_TEST_CASE_IMM(testname ## 15, _data_type, _jit_op, _operator, 1, 1, 1, 1) \


#define SETUP_TESTS(testname) \
	SETUP_TEST(testname ## 0); \
	SETUP_TEST(testname ## 1); \
	SETUP_TEST(testname ## 2); \
	SETUP_TEST(testname ## 3); \
	SETUP_TEST(testname ## 4); \
	SETUP_TEST(testname ## 5); \
	SETUP_TEST(testname ## 6); \
	SETUP_TEST(testname ## 7); \
	SETUP_TEST(testname ## 8); \
	SETUP_TEST(testname ## 9); \
	SETUP_TEST(testname ## 10); \
	SETUP_TEST(testname ## 11); \
	SETUP_TEST(testname ## 12); \
	SETUP_TEST(testname ## 13); \
	SETUP_TEST(testname ## 14); \
	SETUP_TEST(testname ## 15);


#define CREATE_TEST_CASE_REG(testname, _data_type, _jit_op, _operator, _samereg, _equal, _negative, _greater) \
DEFINE_TEST(testname)\
{\
	plfv f1;\
	jit_prolog(p, &f1);\
\
	_data_type firstval = 42;\
	_data_type secval = 28;\
	if (_equal) secval = firstval;\
	if (_greater) secval = 60;\
	if (_negative) secval *= -1;\
\
	jit_movi(p, R(1), firstval);\
	jit_movi(p, R(2), secval);\
\
	if (_samereg) {\
		_jit_op(p, R(2), R(1), R(2));\
		jit_retr(p, R(2));\
	} else {\
		_jit_op(p, R(3), R(1), R(2));\
		jit_retr(p, R(3));\
	}\
	JIT_GENERATE_CODE(p);\
\
	if (firstval _operator secval) ASSERT_EQ(1, f1()); \
	if (!(firstval _operator secval)) ASSERT_EQ(0, f1()); \
	return 0; \
}

#define CREATE_TEST_CASE_IMM(testname, _data_type, _jit_op, _operator, _samereg, _equal, _negative, _greater) \
DEFINE_TEST(testname)\
{\
	plfv f1;\
	jit_prolog(p, &f1);\
\
	_data_type firstval = 42;\
	_data_type secval = 28;\
	if (_equal) secval = firstval;\
	if (_greater) secval = 60;\
	if (_negative) secval *= -1;\
\
	jit_movi(p, R(1), firstval);\
\
	if (_samereg) {\
		_jit_op(p, R(2), R(1), secval);\
		jit_retr(p, R(2));\
	} else {\
		_jit_op(p, R(3), R(1), secval);\
		jit_retr(p, R(3));\
	}\
\
	JIT_GENERATE_CODE(p);\
\
	if (firstval _operator secval) ASSERT_EQ(1, f1()); \
	if (!(firstval _operator secval)) ASSERT_EQ(0, f1()); \
	return 0; \
}

CREATE_TEST_SUITE_REG(test01reg, jit_value, jit_ltr, <)
CREATE_TEST_SUITE_REG(test02reg, jit_value, jit_ler, <=)
CREATE_TEST_SUITE_REG(test03reg, jit_value, jit_ger, >=)
CREATE_TEST_SUITE_REG(test04reg, jit_value, jit_gtr, >)
CREATE_TEST_SUITE_REG(test05reg, jit_value, jit_eqr, ==)
CREATE_TEST_SUITE_REG(test06reg, jit_value, jit_ner, !=)

CREATE_TEST_SUITE_REG(test11reg, jit_unsigned_value, jit_ltr_u, <)
CREATE_TEST_SUITE_REG(test12reg, jit_unsigned_value, jit_ler_u, <=)
CREATE_TEST_SUITE_REG(test13reg, jit_unsigned_value, jit_ger_u, >=)
CREATE_TEST_SUITE_REG(test14reg, jit_unsigned_value, jit_gtr_u, >)
CREATE_TEST_SUITE_REG(test15reg, jit_unsigned_value, jit_eqr, ==)
CREATE_TEST_SUITE_REG(test16reg, jit_unsigned_value, jit_ner, !=)

CREATE_TEST_SUITE_IMM(test21imm, jit_value, jit_lti, <)
CREATE_TEST_SUITE_IMM(test22imm, jit_value, jit_lei, <=)
CREATE_TEST_SUITE_IMM(test23imm, jit_value, jit_gei, >=)
CREATE_TEST_SUITE_IMM(test24imm, jit_value, jit_gti, >)
CREATE_TEST_SUITE_IMM(test25imm, jit_value, jit_eqi, ==)
CREATE_TEST_SUITE_IMM(test26imm, jit_value, jit_nei, !=)

CREATE_TEST_SUITE_IMM(test31imm, jit_unsigned_value, jit_lti_u, <)
CREATE_TEST_SUITE_IMM(test32imm, jit_unsigned_value, jit_lei_u, <=)
CREATE_TEST_SUITE_IMM(test33imm, jit_unsigned_value, jit_gei_u, >=)
CREATE_TEST_SUITE_IMM(test34imm, jit_unsigned_value, jit_gti_u, >)
CREATE_TEST_SUITE_IMM(test35imm, jit_unsigned_value, jit_eqi, ==)
CREATE_TEST_SUITE_IMM(test36imm, jit_unsigned_value, jit_nei, !=)

void test_setup()
{
	test_filename = __FILE__;

	SETUP_TESTS(test01reg);
	SETUP_TESTS(test02reg);
	SETUP_TESTS(test03reg);
	SETUP_TESTS(test04reg);
	SETUP_TESTS(test05reg);
	SETUP_TESTS(test06reg);

	SETUP_TESTS(test11reg);
	SETUP_TESTS(test12reg);
	SETUP_TESTS(test13reg);
	SETUP_TESTS(test14reg);
	SETUP_TESTS(test15reg);
	SETUP_TESTS(test16reg);

	SETUP_TESTS(test01reg);
	SETUP_TESTS(test02reg);
	SETUP_TESTS(test03reg);
	SETUP_TESTS(test04reg);
	SETUP_TESTS(test05reg);
	SETUP_TESTS(test06reg);

	SETUP_TESTS(test11reg);
	SETUP_TESTS(test12reg);
	SETUP_TESTS(test13reg);
	SETUP_TESTS(test14reg);
	SETUP_TESTS(test15reg);
	SETUP_TESTS(test16reg);

	SETUP_TESTS(test21imm);
	SETUP_TESTS(test22imm);
	SETUP_TESTS(test23imm);
	SETUP_TESTS(test24imm);
	SETUP_TESTS(test25imm);
	SETUP_TESTS(test26imm);

	SETUP_TESTS(test31imm);
	SETUP_TESTS(test32imm);
	SETUP_TESTS(test33imm);
	SETUP_TESTS(test34imm);
	SETUP_TESTS(test35imm);
	SETUP_TESTS(test36imm);

}
