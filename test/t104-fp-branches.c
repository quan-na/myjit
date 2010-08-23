#include <limits.h>
#include "../myjit/jitlib.h"
#include "tests.h"

#define CREATE_TEST_REG(testname, __data_type, __ji_op, __operator) \
void testname(int id, int eql, int negative, int greater)\
{\
	double r;\
	struct jit * p = jit_init();\
	pdfv f1;\
	jit_prolog(p, &f1);\
\
	__data_type firstval = 42;\
	__data_type secval = 28;\
	if (eql) secval = firstval;\
	if (greater) secval = 60;\
	if (negative) secval *= -1;\
\
	jit_fmovi(p, FR(1), firstval);\
	jit_fmovi(p, FR(2), secval);\
\
	jit_op * br; \
	br = __ji_op(p, 0, FR(1), FR(2));\
	jit_fmovi(p, FR(3), -10); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_fmovi(p, FR(3), 10); \
	jit_patch(p, e); \
	jit_fretr(p, FR(3), sizeof(double)); \
\
	jit_generate_code(p);\
	r = f1();\
	int testid = id * 10000 + eql * 100 + negative * 10 + greater;\
	if ((equal(r, -10.0, 0.001)) && !(firstval __operator secval)) SUCCESS(testid);\
	else if ((equal(r, 10.0, 0.001)) && (firstval __operator secval)) SUCCESS(testid);\
	else FAIL(testid);\
	jit_free(p);\
}

#define CREATE_TEST_IMM(testname, __data_type, __ji_op, __operator) \
void testname(int id, int eql, int negative, int greater)\
{\
	double r;\
	struct jit * p = jit_init();\
	pdfv f1;\
	jit_prolog(p, &f1);\
\
	__data_type firstval = 42;\
	__data_type secval = 28;\
	if (eql) secval = firstval;\
	if (greater) secval = 60;\
	if (negative) secval *= -1;\
	\
	jit_fmovi(p, FR(1), firstval);\
	jit_fmovi(p, FR(2), secval);\
\
	jit_op * br; \
	br = __ji_op(p, 0, FR(1), secval);\
	jit_fmovi(p, FR(3), -10); \
	jit_op * e = jit_jmpi(p, 0); \
	\
	jit_patch(p, br); \
	jit_fmovi(p, FR(3), 10); \
	jit_patch(p, e); \
	jit_fretr(p, FR(3), sizeof(double)); \
\
	jit_generate_code(p);\
	r = f1();\
	int testid = id * 10000 + eql * 100 + negative * 10 + greater;\
	if (equal(r, -10.0, 0.001) && !(firstval __operator secval)) SUCCESS(testid);\
	else if (equal(r, 10.0, 0.001) && (firstval __operator secval)) SUCCESS(testid);\
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


CREATE_TEST_REG(test01reg, double, jit_fbltr, <)
CREATE_TEST_REG(test02reg, double, jit_fbler, <=)
CREATE_TEST_REG(test03reg, double, jit_fbger, >=)
CREATE_TEST_REG(test04reg, double, jit_fbgtr, >)
CREATE_TEST_REG(test05reg, double, jit_fbeqr, ==)
CREATE_TEST_REG(test06reg, double, jit_fbner, !=)

CREATE_TEST_IMM(test21imm, double, jit_fblti, <)
CREATE_TEST_IMM(test22imm, double, jit_fblei, <=)
CREATE_TEST_IMM(test23imm, double, jit_fbgei, >=)
CREATE_TEST_IMM(test24imm, double, jit_fbgti, >)
CREATE_TEST_IMM(test25imm, double, jit_fbeqi, ==)
CREATE_TEST_IMM(test26imm, double, jit_fbnei, !=)

int main()
{
	TEST_SET(test01reg, 1);
	TEST_SET(test02reg, 2);
	TEST_SET(test03reg, 3);
	TEST_SET(test04reg, 4);
	TEST_SET(test05reg, 5);
	TEST_SET(test06reg, 6);

	TEST_SET(test21imm, 21);
	TEST_SET(test22imm, 22);
	TEST_SET(test23imm, 23);
	TEST_SET(test24imm, 24);
	TEST_SET(test25imm, 25);
	TEST_SET(test26imm, 26);
}
