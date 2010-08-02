#include <limits.h>
#include "../myjit/jitlib.h"
#include "tests.h"

// imm -- same reg
#define TEST_IMM_SAME_REG(__name, __n, __op, __arg1, __arg2, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(); \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FPR(0), __arg1); \
	__op(p, FPR(0), FPR(0), __arg2); \
	jit_fretr(p, FPR(0)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (equal(r, __expected_value, 0.001)) SUCCESS(__n); \
	else FAILD(__n, r, __expected_value); \
	jit_free(p); \
} 

#define TEST_IMM_OTHER_REG(__name, __n, __op, __arg1, __arg2, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(); \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FPR(0), __arg1); \
	__op(p, FPR(1), FPR(0), __arg2); \
	jit_fretr(p, FPR(1)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (equal(r, __expected_value, 0.001)) SUCCESS(__n); \
	else FAILD(__n, r, __expected_value); \
	jit_free(p); \
}

#define TEST_REG_SAME_REG(__name, __n, __op, __arg1, __arg2, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(); \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FPR(0), __arg1); \
	jit_fmovi(p, FPR(1), __arg2); \
	__op(p, FPR(1), FPR(0), FPR(1)); \
	jit_fretr(p, FPR(1)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (equal(r, __expected_value, 0.001)) SUCCESS(__n); \
	else FAILD(__n, r, __expected_value); \
	jit_free(p); \
}

#define TEST_REG_OTHER_REG(__name, __n, __op, __arg1, __arg2, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(); \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FPR(0), __arg1); \
	jit_fmovi(p, FPR(1), __arg2); \
	__op(p, FPR(2), FPR(0), FPR(1)); \
	jit_fretr(p, FPR(2)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (equal(r, __expected_value, 0.001)) SUCCESS(__n); \
	else FAILD(__n, r, __expected_value); \
	jit_free(p); \
}

#define TEST_UNARY_REG_OTHER_REG(__name, __n, __op, __arg1, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(); \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FPR(0), __arg1); \
	__op(p, FPR(2), FPR(0)); \
	jit_fretr(p, FPR(2)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (equal(r, __expected_value, 0.001)) SUCCESS(__n); \
	else FAILD(__n, r, __expected_value); \
	jit_free(p); \
}

#define TEST_UNARY_REG_SAME_REG(__name, __n, __op, __arg1, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(); \
	pdfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FPR(0), __arg1); \
	__op(p, FPR(0), FPR(0)); \
	jit_fretr(p, FPR(0)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (equal(r, __expected_value, 0.001)) SUCCESS(__n); \
	else FAILD(__n, r, __expected_value); \
	jit_free(p); \
}


TEST_IMM_SAME_REG(test10, 10, jit_faddi, 42, 28, 70) 
TEST_IMM_OTHER_REG(test11, 11, jit_faddi, 42, 28, 70) 
TEST_REG_SAME_REG(test12, 12, jit_faddr, 42, 28, 70) 
TEST_REG_OTHER_REG(test13, 13, jit_faddr, 42, 28, 70) 

TEST_IMM_SAME_REG(test20, 20, jit_fsubi, 42, 28, 14) 
TEST_IMM_OTHER_REG(test21, 21, jit_fsubi, 42, 28, 14) 
TEST_REG_SAME_REG(test22, 22, jit_fsubr, 42, 28, 14) 
TEST_REG_OTHER_REG(test23, 23, jit_fsubr, 42, 28, 14) 

TEST_IMM_SAME_REG(test30, 30, jit_frsbi, 42, 28, -14) 
TEST_IMM_OTHER_REG(test31, 31, jit_frsbi, 42, 28, -14) 
TEST_REG_SAME_REG(test32, 32, jit_frsbr, 42, 28, -14) 
TEST_REG_OTHER_REG(test33, 33, jit_frsbr, 42, 28, -14) 

TEST_IMM_SAME_REG(test40, 40, jit_fmuli, 42, 28, 1176) 
TEST_IMM_OTHER_REG(test41, 41, jit_fmuli, 42, 28, 1176) 
TEST_REG_SAME_REG(test42, 42, jit_fmulr, 42, 28, 1176) 
TEST_REG_OTHER_REG(test43, 43, jit_fmulr, 42, 28, 1176) 

TEST_IMM_SAME_REG(test44, 44, jit_fmuli, 42, 4, 168) 
TEST_IMM_OTHER_REG(test45, 45, jit_fmuli, 42, 4, 168) 
TEST_REG_SAME_REG(test46, 46, jit_fmulr, 42, 4, 168) 
TEST_REG_OTHER_REG(test47, 47, jit_fmulr, 42, 4, 168) 

TEST_IMM_SAME_REG(test48, 48, jit_fmuli, 42, 5, 210) 
TEST_IMM_OTHER_REG(test49, 49, jit_fmuli, 42, 5, 210) 
TEST_REG_SAME_REG(test50, 50, jit_fmulr, 42, 5, 210) 
TEST_REG_OTHER_REG(test51, 51, jit_fmulr, 42, 5, 210) 

TEST_IMM_SAME_REG(test52, 52, jit_fmuli, 42, 2,84) 
TEST_IMM_OTHER_REG(test53, 53, jit_fmuli, 42, 2, 84) 
TEST_REG_SAME_REG(test54, 54, jit_fmulr, 42, 2, 84) 
TEST_REG_OTHER_REG(test55, 55, jit_fmulr, 42, 2, 84) 

TEST_IMM_SAME_REG(test56, 56, jit_fmuli, 42, 9, 378) 
TEST_IMM_OTHER_REG(test57, 57, jit_fmuli, 42, 9, 378) 
TEST_REG_SAME_REG(test58, 58, jit_fmulr, 42, 9, 378) 
TEST_REG_OTHER_REG(test59, 59, jit_fmulr, 42, 9, 378) 

TEST_IMM_SAME_REG(test60, 60, jit_fdivi, 42, 4, 10) 
TEST_IMM_OTHER_REG(test61, 61, jit_fdivi, 42, 4, 10) 
TEST_REG_SAME_REG(test62, 62, jit_fdivr, 42, 4, 10) 
TEST_REG_OTHER_REG(test63, 63, jit_fdivr, 42, 4, 10) 

TEST_IMM_SAME_REG(test64, 64, jit_fdivi, 42, 9, 4) 
TEST_IMM_OTHER_REG(test65, 65, jit_fdivi, 42, 9, 4) 
TEST_REG_SAME_REG(test66, 66, jit_fdivr, 42, 9, 4) 
TEST_REG_OTHER_REG(test67, 67, jit_fdivr, 42, 9, 4) 

TEST_UNARY_REG_SAME_REG(test160, 160, jit_fnegr, 42, -42);
TEST_UNARY_REG_OTHER_REG(test161, 161, jit_fnegr, 42, -42);



int main() 
{
	test10();
	test11();
	test12();
	test13();
	test20();
	test21();
	test22();
	test23();
	test30();
	test31();
	test32();
	test33();
	test40();
	test41();
	test42();
	test43();
	test44();

	test45();
	test46();
	test47();
	test48();
	test49();
	test50();
	test51();
	test52();
	test53();
	test54();
	test55();
	test56();
	test57();
	test58();
	test59();

	test60();
	test61();
	test62();
	test63();
	test64();
	test65();
	test66();
	test67();
	
	test160();
	test161();
}
