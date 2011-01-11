#include <limits.h>
#include "../myjit/jitlib.h"
#include "tests.h"

#define CREATE_TEST_REG(testname, __data_type, __ji_op, __operator) \
void testname(int id, int equal, int negative, int greater)\
{\
	jit_value r;\
	struct jit * p = jit_init();\
	plfv f1;\
	jit_prolog(p, &f1);\
\
	__data_type firstval = 42;\
	__data_type secval = 28;\
	if (equal) secval = firstval;\
	if (greater) secval = 60;\
	if (negative) secval *= -1;\
\
	jit_movi(p, R(1), firstval);\
	jit_movi(p, R(2), secval);\
\
	jit_op * br; \
	br = __ji_op(p, 0, R(1), R(2));\
	jit_movi(p, R(3), -10); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_movi(p, R(3), 10); \
	jit_patch(p, e); \
	jit_retr(p, R(3)); \
\
	jit_generate_code(p);\
	r = f1();\
	int testid = id * 10000 + equal * 100 + negative * 10 + greater;\
	if ((r == -10) && !(firstval __operator secval)) SUCCESS(testid);\
	else if ((r == 10) && (firstval __operator secval)) SUCCESS(testid);\
	else FAIL(testid);\
	jit_free(p);\
}

#define CREATE_TEST_IMM(testname, __data_type, __ji_op, __operator) \
void testname(int id, int equal, int negative, int greater)\
{\
	jit_value r;\
	struct jit * p = jit_init();\
	plfv f1;\
	jit_prolog(p, &f1);\
\
	__data_type firstval = 42;\
	__data_type secval = 28;\
	if (equal) secval = firstval;\
	if (greater) secval = 60;\
	if (negative) secval *= -1;\
	\
	jit_movi(p, R(1), firstval);\
	jit_movi(p, R(2), secval);\
\
	jit_op * br; \
	br = __ji_op(p, 0, R(1), secval);\
	jit_movi(p, R(3), -10); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_movi(p, R(3), 10); \
	jit_patch(p, e); \
	jit_retr(p, R(3)); \
\
	jit_generate_code(p);\
	r = f1();\
	int testid = id * 10000 + equal * 100 + negative * 10 + greater;\
	if ((r == -10) && !(firstval __operator secval)) SUCCESS(testid);\
	else if ((r == 10) && (firstval __operator secval)) SUCCESS(testid);\
	else FAIL(testid);\
	jit_free(p);\
}

#define TEST_SET(testname, id) \
	testname(id, 0, 0, 0);\
	testname(id, 0, 0, 1);\
	testname(id, 0, 1, 0);\
	testname(id, 0, 1, 1);\
	testname(id, 1, 0, 0);\
	testname(id, 1, 0, 1);\
	testname(id, 1, 1, 0);\
	testname(id, 1, 1, 1);


CREATE_TEST_REG(test01reg, jit_value, jit_bltr, <)
CREATE_TEST_REG(test02reg, jit_value, jit_bler, <=)
CREATE_TEST_REG(test03reg, jit_value, jit_bger, >=)
CREATE_TEST_REG(test04reg, jit_value, jit_bgtr, >)
CREATE_TEST_REG(test05reg, jit_value, jit_beqr, ==)
CREATE_TEST_REG(test06reg, jit_value, jit_bner, !=)

CREATE_TEST_REG(test11reg, jit_unsigned_value, jit_bltr_u, <)
CREATE_TEST_REG(test12reg, jit_unsigned_value, jit_bler_u, <=)
CREATE_TEST_REG(test13reg, jit_unsigned_value, jit_bger_u, >=)
CREATE_TEST_REG(test14reg, jit_unsigned_value, jit_bgtr_u, >)
CREATE_TEST_REG(test15reg, jit_unsigned_value, jit_beqr, ==)
CREATE_TEST_REG(test16reg, jit_unsigned_value, jit_bner, !=)

CREATE_TEST_IMM(test21imm, jit_value, jit_blti, <)
CREATE_TEST_IMM(test22imm, jit_value, jit_blei, <=)
CREATE_TEST_IMM(test23imm, jit_value, jit_bgei, >=)
CREATE_TEST_IMM(test24imm, jit_value, jit_bgti, >)
CREATE_TEST_IMM(test25imm, jit_value, jit_beqi, ==)
CREATE_TEST_IMM(test26imm, jit_value, jit_bnei, !=)

CREATE_TEST_IMM(test31imm, jit_unsigned_value, jit_blti_u, <)
CREATE_TEST_IMM(test32imm, jit_unsigned_value, jit_blei_u, <=)
CREATE_TEST_IMM(test33imm, jit_unsigned_value, jit_bgei_u, >=)
CREATE_TEST_IMM(test34imm, jit_unsigned_value, jit_bgti_u, >)
CREATE_TEST_IMM(test35imm, jit_unsigned_value, jit_beqi, ==)
CREATE_TEST_IMM(test36imm, jit_unsigned_value, jit_bnei, !=)

int main()
{
	TEST_SET(test01reg, 1);
	TEST_SET(test02reg, 2);
	TEST_SET(test03reg, 3);
	TEST_SET(test04reg, 4);
	TEST_SET(test05reg, 5);
	TEST_SET(test06reg, 6);

	TEST_SET(test11reg, 11);
	TEST_SET(test12reg, 12);
	TEST_SET(test13reg, 13);
	TEST_SET(test14reg, 14);
	TEST_SET(test15reg, 15);
	TEST_SET(test16reg, 16);

	TEST_SET(test21imm, 21);
	TEST_SET(test22imm, 22);
	TEST_SET(test23imm, 23);
	TEST_SET(test24imm, 24);
	TEST_SET(test25imm, 25);
	TEST_SET(test26imm, 26);

	TEST_SET(test31imm, 31);
	TEST_SET(test32imm, 32);
	TEST_SET(test33imm, 33);
	TEST_SET(test34imm, 34);
	TEST_SET(test35imm, 35);
	TEST_SET(test36imm, 36);
}
