#include <limits.h>
#include "../myjit/jitlib.h"
#include "tests.h"

// imm -- same reg
#define TEST_IMM_SAME_REG(__name, __n, __op, __arg1, __arg2, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(16); \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), __arg1); \
	__op(p, R(0), R(0), __arg2); \
	jit_retr(p, R(0)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (r == __expected_value) SUCCESS(__n); \
	else FAILX(__n, r, __expected_value); \
	jit_free(p); \
} 

#define TEST_IMM_OTHER_REG(__name, __n, __op, __arg1, __arg2, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(16); \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), __arg1); \
	__op(p, R(1), R(0), __arg2); \
	jit_retr(p, R(1)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (r == __expected_value) SUCCESS(__n); \
	else FAILX(__n, r, __expected_value); \
	jit_free(p); \
}

#define TEST_REG_SAME_REG(__name, __n, __op, __arg1, __arg2, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(16); \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), __arg1); \
	jit_movi(p, R(1), __arg2); \
	__op(p, R(1), R(0), R(1)); \
	jit_retr(p, R(1)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (r == __expected_value) SUCCESS(__n); \
	else FAILX(__n, r, __expected_value); \
	jit_free(p); \
}

#define TEST_REG_OTHER_REG(__name, __n, __op, __arg1, __arg2, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(16); \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), __arg1); \
	jit_movi(p, R(1), __arg2); \
	__op(p, R(2), R(0), R(1)); \
	jit_retr(p, R(2)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (r == __expected_value) SUCCESS(__n); \
	else FAILX(__n, r, __expected_value); \
	jit_free(p); \
}

#define TEST_UNARY_REG_OTHER_REG(__name, __n, __op, __arg1, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(16); \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), __arg1); \
	__op(p, R(2), R(0)); \
	jit_retr(p, R(2)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (r == __expected_value) SUCCESS(__n); \
	else FAILX(__n, r, __expected_value); \
	jit_free(p); \
}

#define TEST_UNARY_REG_SAME_REG(__name, __n, __op, __arg1, __expected_value) \
void __name() \
{ \
	long r; \
	struct jit * p = jit_init(16); \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), __arg1); \
	__op(p, R(0), R(0)); \
	jit_retr(p, R(0)); \
	jit_generate_code(p); \
\
	r = f1(); \
	if (r == __expected_value) SUCCESS(__n); \
	else FAILX(__n, r, __expected_value); \
	jit_free(p); \
}


TEST_IMM_SAME_REG(test10, 10, jit_addi, 42, 28, 70) 
TEST_IMM_OTHER_REG(test11, 11, jit_addi, 42, 28, 70) 
TEST_REG_SAME_REG(test12, 12, jit_addr, 42, 28, 70) 
TEST_REG_OTHER_REG(test13, 13, jit_addr, 42, 28, 70) 

TEST_IMM_SAME_REG(test20, 20, jit_subi, 42, 28, 14) 
TEST_IMM_OTHER_REG(test21, 21, jit_subi, 42, 28, 14) 
TEST_REG_SAME_REG(test22, 22, jit_subr, 42, 28, 14) 
TEST_REG_OTHER_REG(test23, 23, jit_subr, 42, 28, 14) 

TEST_IMM_SAME_REG(test30, 30, jit_rsbi, 42, 28, -14) 
TEST_IMM_OTHER_REG(test31, 31, jit_rsbi, 42, 28, -14) 
TEST_REG_SAME_REG(test32, 32, jit_rsbr, 42, 28, -14) 
TEST_REG_OTHER_REG(test33, 33, jit_rsbr, 42, 28, -14) 

TEST_IMM_SAME_REG(test40, 40, jit_muli, 42, 28, 1176) 
TEST_IMM_OTHER_REG(test41, 41, jit_muli, 42, 28, 1176) 
TEST_REG_SAME_REG(test42, 42, jit_mulr, 42, 28, 1176) 
TEST_REG_OTHER_REG(test43, 43, jit_mulr, 42, 28, 1176) 

TEST_IMM_SAME_REG(test44, 44, jit_muli, 42, 4, 168) 
TEST_IMM_OTHER_REG(test45, 45, jit_muli, 42, 4, 168) 
TEST_REG_SAME_REG(test46, 46, jit_mulr, 42, 4, 168) 
TEST_REG_OTHER_REG(test47, 47, jit_mulr, 42, 4, 168) 

TEST_IMM_SAME_REG(test48, 48, jit_muli, 42, 5, 210) 
TEST_IMM_OTHER_REG(test49, 49, jit_muli, 42, 5, 210) 
TEST_REG_SAME_REG(test50, 50, jit_mulr, 42, 5, 210) 
TEST_REG_OTHER_REG(test51, 51, jit_mulr, 42, 5, 210) 

TEST_IMM_SAME_REG(test52, 52, jit_muli, 42, 2,84) 
TEST_IMM_OTHER_REG(test53, 53, jit_muli, 42, 2, 84) 
TEST_REG_SAME_REG(test54, 54, jit_mulr, 42, 2, 84) 
TEST_REG_OTHER_REG(test55, 55, jit_mulr, 42, 2, 84) 

TEST_IMM_SAME_REG(test56, 56, jit_muli, 42, 9, 378) 
TEST_IMM_OTHER_REG(test57, 57, jit_muli, 42, 9, 378) 
TEST_REG_SAME_REG(test58, 58, jit_mulr, 42, 9, 378) 
TEST_REG_OTHER_REG(test59, 59, jit_mulr, 42, 9, 378) 

TEST_IMM_SAME_REG(test60, 60, jit_divi, 42, 4, 10) 
TEST_IMM_OTHER_REG(test61, 61, jit_divi, 42, 4, 10) 
TEST_REG_SAME_REG(test62, 62, jit_divr, 42, 4, 10) 
TEST_REG_OTHER_REG(test63, 63, jit_divr, 42, 4, 10) 

TEST_IMM_SAME_REG(test64, 64, jit_divi, 42, 9, 4) 
TEST_IMM_OTHER_REG(test65, 65, jit_divi, 42, 9, 4) 
TEST_REG_SAME_REG(test66, 66, jit_divr, 42, 9, 4) 
TEST_REG_OTHER_REG(test67, 67, jit_divr, 42, 9, 4) 

TEST_IMM_SAME_REG(test70, 70, jit_modi, 42, 4, 2) 
TEST_IMM_OTHER_REG(test71, 71, jit_modi, 42, 4, 2) 
TEST_REG_SAME_REG(test72, 72, jit_modr, 42, 4, 2) 
TEST_REG_OTHER_REG(test73, 73, jit_modr, 42, 4, 2) 

TEST_IMM_SAME_REG(test74, 74, jit_modi, 42, 9, 6) 
TEST_IMM_OTHER_REG(test75, 75, jit_modi, 42, 9, 6) 
TEST_REG_SAME_REG(test76, 76, jit_modr, 42, 9, 6) 
TEST_REG_OTHER_REG(test77, 77, jit_modr, 42, 9, 6) 


TEST_IMM_SAME_REG(test80, 80, jit_addxi, 42, 28, 70) 
TEST_IMM_OTHER_REG(test81, 81, jit_addxi, 42, 28, 70) 
TEST_REG_SAME_REG(test82, 82, jit_addxr, 42, 28, 70) 
TEST_REG_OTHER_REG(test83, 83, jit_addxr, 42, 28, 70) 

TEST_IMM_SAME_REG(test84, 84, jit_addci, 42, 28, 70) 
TEST_IMM_OTHER_REG(test85, 85, jit_addci, 42, 28, 70) 
TEST_REG_SAME_REG(test86, 86, jit_addcr, 42, 28, 70) 
TEST_REG_OTHER_REG(test87, 87, jit_addcr, 42, 28, 70) 

TEST_IMM_SAME_REG(test90, 90, jit_subxi, 42, 28, 14) 
TEST_IMM_OTHER_REG(test91, 91, jit_subxi, 42, 28, 14) 
TEST_REG_SAME_REG(test92, 92, jit_subxr, 42, 28, 14) 
TEST_REG_OTHER_REG(test93, 93, jit_subxr, 42, 28, 14) 

TEST_IMM_SAME_REG(test94, 94, jit_subci, 42, 28, 14) 
TEST_IMM_OTHER_REG(test95, 95, jit_subci, 42, 28, 14) 
TEST_REG_SAME_REG(test96, 96, jit_subcr, 42, 28, 14) 
TEST_REG_OTHER_REG(test97, 97, jit_subcr, 42, 28, 14) 

TEST_IMM_SAME_REG(test100, 100, jit_ori, 42, 28, 42 | 28) 
TEST_IMM_OTHER_REG(test101, 101, jit_ori, 42, 28, 42 | 28) 
TEST_REG_SAME_REG(test102, 102, jit_orr, 42, 28, 42 | 28) 
TEST_REG_OTHER_REG(test103, 103, jit_orr, 42, 28, 42 | 28) 

TEST_IMM_SAME_REG(test110, 110, jit_andi, 42, 28, 8) 
TEST_IMM_OTHER_REG(test111, 111, jit_andi, 42, 28, 8) 
TEST_REG_SAME_REG(test112, 112, jit_andr, 42, 28, 8) 
TEST_REG_OTHER_REG(test113, 113, jit_andr, 42, 28, 8) 

TEST_IMM_SAME_REG(test120, 120, jit_xori, 42, 28, 42 ^ 28) 
TEST_IMM_OTHER_REG(test121, 121, jit_xori, 42, 28, 42 ^ 28) 
TEST_REG_SAME_REG(test122, 122, jit_xorr, 42, 28, 42 ^ 28) 
TEST_REG_OTHER_REG(test123, 123, jit_xorr, 42, 28, 42 ^ 28) 

TEST_IMM_SAME_REG(test130, 130, jit_lshi, 42, 8, 42 << 8) 
TEST_IMM_OTHER_REG(test131, 131, jit_lshi, 42, 8, 42 << 8) 
TEST_REG_SAME_REG(test132, 132, jit_lshr, 42, 8, 42 << 8) 
TEST_REG_OTHER_REG(test133, 133, jit_lshr, 42, 8, 42 << 8) 

TEST_IMM_SAME_REG(test140, 140, jit_rshi, 42UL, 2, 42L >> 2) 
TEST_IMM_OTHER_REG(test141, 141, jit_rshi, 42UL, 2, 42L >> 2) 
TEST_REG_SAME_REG(test142, 142, jit_rshr, 42UL, 2, 42L >> 2) 
TEST_REG_OTHER_REG(test143, 143, jit_rshr, 42UL, 2, 42L >> 2) 

TEST_IMM_SAME_REG(test144, 144, jit_rshi, -42UL, 2, -42L >> 2) 
TEST_IMM_OTHER_REG(test145, 145, jit_rshi, -42UL, 2, -42L >> 2) 
TEST_REG_SAME_REG(test146, 146, jit_rshr, -42UL, 2, -42L >> 2) 
TEST_REG_OTHER_REG(test147, 147, jit_rshr, -42UL, 2, -42L >> 2) 

TEST_IMM_SAME_REG(test150, 150, jit_rshi_u, 42UL, 2, 42UL >> 2) 
TEST_IMM_OTHER_REG(test151, 151, jit_rshi_u, 42UL, 2, 42UL >> 2) 
TEST_REG_SAME_REG(test152, 152, jit_rshr_u, 42UL, 2, 42UL >> 2) 
TEST_REG_OTHER_REG(test153, 153, jit_rshr_u, 42UL, 2, 42UL >> 2) 

TEST_IMM_SAME_REG(test154, 154, jit_rshi_u, -42UL, 2, -42UL >> 2) 
TEST_IMM_OTHER_REG(test155, 155, jit_rshi_u, -42UL, 2, -42UL >> 2) 
TEST_REG_SAME_REG(test156, 156, jit_rshr_u, -42UL, 2, -42UL >> 2) 
TEST_REG_OTHER_REG(test157, 157, jit_rshr_u, -42UL, 2, -42UL >> 2) 


TEST_UNARY_REG_SAME_REG(test160, 160, jit_negr, 42, -42);
TEST_UNARY_REG_OTHER_REG(test161, 161, jit_negr, 42, -42);


TEST_UNARY_REG_SAME_REG(test162, 162, jit_notr, 42, ~42);
TEST_UNARY_REG_OTHER_REG(test163, 163, jit_notr, 42, ~42);



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

	test70();
	test71();
	test72();
	test73();
	test74();
	test75();
	test76();
	test77();

	test80();
	test81();
	test82();
	test83();
	test84();
	test85();
	test86();
	test87();

	test90();
	test91();
	test92();
	test93();
	test94();
	test95();
	test96();
	test97();

	test100();
	test101();
	test102();
	test103();

	test110();
	test111();
	test112();
	test113();

	test120();
	test121();
	test122();
	test123();

	test130();
	test131();
	test132();
	test133();

	test140();
	test141();
	test142();
	test143();

	test144();
	test145();
	test146();
	test147();

	test150();
	test151();
	test152();
	test153();
	test154();
	test155();
	test156();
	test157();

	test160();
	test161();
	test162();
	test163();
}
