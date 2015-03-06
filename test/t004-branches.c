#include <limits.h>
#include "tests.h"
#pragma GCC diagnostic ignored "-Woverflow"
#pragma clang diagnostic ignored "-Winteger-overflow"

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
	jit_op * br; \
	br = _jit_op(p, 0, R(1), R(2));\
	jit_movi(p, R(3), -10); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_movi(p, R(3), 10); \
	jit_patch(p, e); \
	jit_retr(p, R(3)); \
\
	JIT_GENERATE_CODE(p);\
\
        if (firstval _operator secval) ASSERT_EQ(10, f1()); \
        if (!(firstval _operator secval)) ASSERT_EQ(-10, f1()); \
	return 0; \
}


#define CREATE_TEST_CASE_IMM(testname, _data_type, _jit_op, _operator, _equal, _negative, _greater) \
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
	jit_op * br; \
	br = _jit_op(p, 0, R(1), secval);\
	jit_movi(p, R(3), -10); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_movi(p, R(3), 10); \
	jit_patch(p, e); \
	jit_retr(p, R(3)); \
\
	JIT_GENERATE_CODE(p);\
\
        if (firstval _operator secval) ASSERT_EQ(10, f1()); \
        if (!(firstval _operator secval)) ASSERT_EQ(-10, f1()); \
	return 0; \
}

#define CREATE_TEST_MASK_REG(testname, _jit_op, value, mask, expected_result) \
DEFINE_TEST(testname) \
{ \
	plfv f1;\
	jit_prolog(p, &f1);\
\
	jit_movi(p, R(1), value);\
	jit_movi(p, R(2), mask);\
\
	jit_op * br; \
	br = _jit_op(p, 0, R(1), R(2));\
	jit_movi(p, R(3), -10); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_movi(p, R(3), 10); \
	jit_patch(p, e); \
	jit_retr(p, R(3)); \
\
	JIT_GENERATE_CODE(p);\
\
        if (expected_result) ASSERT_EQ(10, f1()); \
        if (!expected_result) ASSERT_EQ(-10, f1()); \
	return 0; \
}

#define CREATE_TEST_MASK_IMM(testname, _jit_op, value, mask, expected_result) \
DEFINE_TEST(testname) \
{ \
	plfv f1;\
	jit_prolog(p, &f1);\
\
	jit_movi(p, R(1), value);\
\
	jit_op * br; \
	br = _jit_op(p, 0, R(1), mask);\
	jit_movi(p, R(3), -10); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_movi(p, R(3), 10); \
	jit_patch(p, e); \
	jit_retr(p, R(3)); \
\
	JIT_GENERATE_CODE(p);\
\
        if (expected_result) ASSERT_EQ(10, f1()); \
        if (!expected_result) ASSERT_EQ(-10, f1()); \
	return 0; \
}



#define CREATE_TEST_OVERFLOW_REG(testname, _jit_op, _c_op, value1, value2, expected_overflow) \
DEFINE_TEST(testname) \
{ \
	plfv f1;\
	char overflow_flag = -1;\
	jit_prolog(p, &f1);\
\
	jit_movi(p, R(1), value1);\
	jit_movi(p, R(2), value2);\
\
	jit_op * br; \
	br = _jit_op(p, 0, R(1), R(2));\
	jit_movi(p, R(3), 0); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_movi(p, R(3), 1); \
	jit_patch(p, e); \
	jit_sti(p, &overflow_flag, R(3), 1); \
	jit_retr(p, R(1)); \
\
	JIT_GENERATE_CODE(p);\
\
	ASSERT_EQ(((value1) _c_op (value2)), f1()); \
        if (expected_overflow) ASSERT_EQ(1, overflow_flag); \
        if (!expected_overflow) ASSERT_EQ(0, overflow_flag); \
	return 0; \
}

#define CREATE_TEST_OVERFLOW_IMM(testname, _jit_op, _c_op, value1, value2, expected_overflow) \
DEFINE_TEST(testname) \
{ \
	plfv f1;\
	char overflow_flag = -1;\
	jit_prolog(p, &f1);\
\
	jit_movi(p, R(1), value1);\
\
	jit_op * br; \
	br = _jit_op(p, 0, R(1), value2);\
	jit_movi(p, R(3), 0); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_movi(p, R(3), 1); \
	jit_patch(p, e); \
	jit_sti(p, &overflow_flag, R(3), 1); \
	jit_retr(p, R(1)); \
\
	JIT_GENERATE_CODE(p);\
\
	ASSERT_EQ(((value1) _c_op (value2)), f1()); \
        if (expected_overflow) ASSERT_EQ(1, overflow_flag); \
        if (!expected_overflow) ASSERT_EQ(0, overflow_flag); \
	return 0; \
}


CREATE_TEST_SUITE_REG(test01reg, jit_value, jit_bltr, <)
CREATE_TEST_SUITE_REG(test02reg, jit_value, jit_bler, <=)
CREATE_TEST_SUITE_REG(test03reg, jit_value, jit_bger, >=)
CREATE_TEST_SUITE_REG(test04reg, jit_value, jit_bgtr, >)
CREATE_TEST_SUITE_REG(test05reg, jit_value, jit_beqr, ==)
CREATE_TEST_SUITE_REG(test06reg, jit_value, jit_bner, !=)

CREATE_TEST_SUITE_REG(test11reg, jit_unsigned_value, jit_bltr_u, <)
CREATE_TEST_SUITE_REG(test12reg, jit_unsigned_value, jit_bler_u, <=)
CREATE_TEST_SUITE_REG(test13reg, jit_unsigned_value, jit_bger_u, >=)
CREATE_TEST_SUITE_REG(test14reg, jit_unsigned_value, jit_bgtr_u, >)
CREATE_TEST_SUITE_REG(test15reg, jit_unsigned_value, jit_beqr, ==)
CREATE_TEST_SUITE_REG(test16reg, jit_unsigned_value, jit_bner, !=)

CREATE_TEST_SUITE_IMM(test21imm, jit_value, jit_blti, <)
CREATE_TEST_SUITE_IMM(test22imm, jit_value, jit_blei, <=)
CREATE_TEST_SUITE_IMM(test23imm, jit_value, jit_bgei, >=)
CREATE_TEST_SUITE_IMM(test24imm, jit_value, jit_bgti, >)
CREATE_TEST_SUITE_IMM(test25imm, jit_value, jit_beqi, ==)
CREATE_TEST_SUITE_IMM(test26imm, jit_value, jit_bnei, !=)

CREATE_TEST_SUITE_IMM(test31imm, jit_unsigned_value, jit_blti_u, <)
CREATE_TEST_SUITE_IMM(test32imm, jit_unsigned_value, jit_blei_u, <=)
CREATE_TEST_SUITE_IMM(test33imm, jit_unsigned_value, jit_bgei_u, >=)
CREATE_TEST_SUITE_IMM(test34imm, jit_unsigned_value, jit_bgti_u, >)
CREATE_TEST_SUITE_IMM(test35imm, jit_unsigned_value, jit_beqi, ==)
CREATE_TEST_SUITE_IMM(test36imm, jit_unsigned_value, jit_bnei, !=)

CREATE_TEST_MASK_REG(testmask40reg, jit_bmsr, 0x7f, 0x04, 1)
CREATE_TEST_MASK_REG(testmask41reg, jit_bmsr, 0x7f, 0x80, 0)
CREATE_TEST_MASK_REG(testmask42reg, jit_bmcr, 0x7f, 0x04, 0)
CREATE_TEST_MASK_REG(testmask43reg, jit_bmcr, 0x7f, 0x80, 1)

CREATE_TEST_MASK_IMM(testmask44imm, jit_bmsi, 0x7f, 0x04, 1)
CREATE_TEST_MASK_IMM(testmask45imm, jit_bmsi, 0x7f, 0x80, 0)
CREATE_TEST_MASK_IMM(testmask46imm, jit_bmci, 0x7f, 0x04, 0)
CREATE_TEST_MASK_IMM(testmask47imm, jit_bmci, 0x7f, 0x80, 1)

CREATE_TEST_OVERFLOW_REG(testaddoverflow50reg, jit_boaddr, +, 10, 20, 0)
CREATE_TEST_OVERFLOW_REG(testaddoverflow51reg, jit_boaddr, +, LONG_MAX, 20, 1)
CREATE_TEST_OVERFLOW_REG(testsuboverflow52reg, jit_bosubr, -, 10, 20, 0)
CREATE_TEST_OVERFLOW_REG(testsuboverflow53reg, jit_bosubr, -, -20, LONG_MAX, 1)
CREATE_TEST_OVERFLOW_REG(testaddoverflow54reg, jit_bnoaddr, +, 10, 20, 1)
CREATE_TEST_OVERFLOW_REG(testaddoverflow55reg, jit_bnoaddr, +, LONG_MAX, 20, 0)
CREATE_TEST_OVERFLOW_REG(testsuboverflow56reg, jit_bnosubr, -, 10, 20, 1)
CREATE_TEST_OVERFLOW_REG(testsuboverflow57reg, jit_bnosubr, -, -20, LONG_MAX, 0)

CREATE_TEST_OVERFLOW_IMM(testaddoverflow50imm, jit_boaddi, +, 10, 20, 0)
CREATE_TEST_OVERFLOW_IMM(testaddoverflow51imm, jit_boaddi, +, LONG_MAX, 20, 1)
CREATE_TEST_OVERFLOW_IMM(testsuboverflow52imm, jit_bosubi, -, 10, 20, 0)
CREATE_TEST_OVERFLOW_IMM(testsuboverflow53imm, jit_bosubi, -, -20, LONG_MAX, 1)
CREATE_TEST_OVERFLOW_IMM(testaddoverflow54imm, jit_bnoaddi, +, 10, 20, 1)
CREATE_TEST_OVERFLOW_IMM(testaddoverflow55imm, jit_bnoaddi, +, LONG_MAX, 20, 0)
CREATE_TEST_OVERFLOW_IMM(testsuboverflow56imm, jit_bnosubi, -, 10, 20, 1)
CREATE_TEST_OVERFLOW_IMM(testsuboverflow57imm, jit_bnosubi, -, -20, LONG_MAX, 0)



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

	SETUP_TEST(testmask40reg);
	SETUP_TEST(testmask41reg);
	SETUP_TEST(testmask42reg);
	SETUP_TEST(testmask43reg);

	SETUP_TEST(testmask44imm);
	SETUP_TEST(testmask45imm);
	SETUP_TEST(testmask46imm);
	SETUP_TEST(testmask47imm);

	SETUP_TEST(testaddoverflow50reg);
	SETUP_TEST(testaddoverflow51reg);
	SETUP_TEST(testsuboverflow52reg);
	SETUP_TEST(testsuboverflow53reg);
	SETUP_TEST(testaddoverflow54reg);
	SETUP_TEST(testaddoverflow55reg);
	SETUP_TEST(testsuboverflow56reg);
	SETUP_TEST(testsuboverflow57reg);

	SETUP_TEST(testaddoverflow50imm);
	SETUP_TEST(testaddoverflow51imm);
	SETUP_TEST(testsuboverflow52imm);
	SETUP_TEST(testsuboverflow53imm);
	SETUP_TEST(testaddoverflow54imm);
	SETUP_TEST(testaddoverflow55imm);
	SETUP_TEST(testsuboverflow56imm);
	SETUP_TEST(testsuboverflow57imm);
}
