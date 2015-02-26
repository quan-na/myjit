#include "tests.h"

// imm -- same reg
#define TEST_IMM_SAME_REG(_name, _op, _arg1, _arg2, _expected_value) \
DEFINE_TEST(_name) \
{ \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), _arg1); \
	jit_addci(p, R(0), R(0), 0); /* sets carry flag to 0 */ \
	_op(p, R(0), R(0), _arg2); \
	jit_retr(p, R(0)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ(_expected_value, f1());\
	return 0;\
} 

#define TEST_IMM_OTHER_REG(_name, _op, _arg1, _arg2, _expected_value) \
DEFINE_TEST(_name) \
{ \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), _arg1); \
	jit_addci(p, R(0), R(0), 0); /* sets carry flag to 0 */ \
	_op(p, R(1), R(0), _arg2); \
	jit_retr(p, R(1)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ(_expected_value, f1());\
	return 0;\
}

#define TEST_REG_SAME_REG(_name, _op, _arg1, _arg2, _expected_value) \
DEFINE_TEST(_name) \
{ \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), _arg1); \
	jit_movi(p, R(1), _arg2); \
	jit_addci(p, R(0), R(0), 0); /* sets carry flag to 0 */ \
	_op(p, R(1), R(0), R(1)); \
	jit_retr(p, R(1)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ(_expected_value, f1());\
	return 0;\
}

#define TEST_REG_OTHER_REG(_name, _op, _arg1, _arg2, _expected_value) \
DEFINE_TEST(_name) \
{ \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), _arg1); \
	jit_movi(p, R(1), _arg2); \
	jit_addci(p, R(0), R(0), 0); /* sets carry flag to 0 */ \
	_op(p, R(2), R(0), R(1)); \
	jit_retr(p, R(2)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ(_expected_value, f1());\
	return 0;\
}

#define TEST_UNARY_REG_OTHER_REG(_name, _op, _arg1, _expected_value) \
DEFINE_TEST(_name) \
{ \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), _arg1); \
	_op(p, R(2), R(0)); \
	jit_retr(p, R(2)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ(_expected_value, f1());\
	return 0;\
}

#define TEST_UNARY_REG_SAME_REG(_name, _op, _arg1, _expected_value) \
DEFINE_TEST(_name) \
{ \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_movi(p, R(0), _arg1); \
	_op(p, R(0), R(0)); \
	jit_retr(p, R(0)); \
	JIT_GENERATE_CODE(p); \
\
	ASSERT_EQ(_expected_value, f1());\
	return 0;\
}


TEST_IMM_SAME_REG(test10, jit_addi, 42, 28, 70) 
TEST_IMM_OTHER_REG(test11, jit_addi, 42, 28, 70) 
TEST_REG_SAME_REG(test12, jit_addr, 42, 28, 70) 
TEST_REG_OTHER_REG(test13, jit_addr, 42, 28, 70) 

TEST_IMM_SAME_REG(test20, jit_subi, 42, 28, 14) 
TEST_IMM_OTHER_REG(test21, jit_subi, 42, 28, 14) 
TEST_REG_SAME_REG(test22, jit_subr, 42, 28, 14) 
TEST_REG_OTHER_REG(test23, jit_subr, 42, 28, 14) 

TEST_IMM_SAME_REG(test30, jit_rsbi, 42, 28, -14) 
TEST_IMM_OTHER_REG(test31, jit_rsbi, 42, 28, -14) 
TEST_REG_SAME_REG(test32, jit_rsbr, 42, 28, -14) 
TEST_REG_OTHER_REG(test33, jit_rsbr, 42, 28, -14) 

TEST_IMM_SAME_REG(test40, jit_muli, 42, 28, 1176) 
TEST_IMM_OTHER_REG(test41, jit_muli, 42, 28, 1176) 
TEST_REG_SAME_REG(test42, jit_mulr, 42, 28, 1176) 
TEST_REG_OTHER_REG(test43, jit_mulr, 42, 28, 1176) 

TEST_IMM_SAME_REG(test44, jit_muli, 42, 4, 168) 
TEST_IMM_OTHER_REG(test45, jit_muli, 42, 4, 168) 
TEST_REG_SAME_REG(test46, jit_mulr, 42, 4, 168) 
TEST_REG_OTHER_REG(test47, jit_mulr, 42, 4, 168) 

TEST_IMM_SAME_REG(test48, jit_muli, 42, 5, 210) 
TEST_IMM_OTHER_REG(test49, jit_muli, 42, 5, 210) 
TEST_REG_SAME_REG(test50, jit_mulr, 42, 5, 210) 
TEST_REG_OTHER_REG(test51, jit_mulr, 42, 5, 210) 

TEST_IMM_SAME_REG(test52, jit_muli, 42, 2,84) 
TEST_IMM_OTHER_REG(test53, jit_muli, 42, 2, 84) 
TEST_REG_SAME_REG(test54, jit_mulr, 42, 2, 84) 
TEST_REG_OTHER_REG(test55, jit_mulr, 42, 2, 84) 

TEST_IMM_SAME_REG(test56, jit_muli, 42, 9, 378) 
TEST_IMM_OTHER_REG(test57, jit_muli, 42, 9, 378) 
TEST_REG_SAME_REG(test58, jit_mulr, 42, 9, 378) 
TEST_REG_OTHER_REG(test59, jit_mulr, 42, 9, 378) 

TEST_IMM_SAME_REG(test60, jit_divi, 42, 4, 10) 
TEST_IMM_OTHER_REG(test61, jit_divi, 42, 4, 10) 
TEST_REG_SAME_REG(test62, jit_divr, 42, 4, 10) 
TEST_REG_OTHER_REG(test63, jit_divr, 42, 4, 10) 

TEST_IMM_SAME_REG(test64, jit_divi, 42, 9, 4) 
TEST_IMM_OTHER_REG(test65, jit_divi, 42, 9, 4) 
TEST_REG_SAME_REG(test66, jit_divr, 42, 9, 4) 
TEST_REG_OTHER_REG(test67, jit_divr, 42, 9, 4) 

TEST_IMM_SAME_REG(test70, jit_modi, 42, 4, 2) 
TEST_IMM_OTHER_REG(test71, jit_modi, 42, 4, 2) 
TEST_REG_SAME_REG(test72, jit_modr, 42, 4, 2) 
TEST_REG_OTHER_REG(test73, jit_modr, 42, 4, 2) 

TEST_IMM_SAME_REG(test74, jit_modi, 42, 9, 6) 
TEST_IMM_OTHER_REG(test75, jit_modi, 42, 9, 6) 
TEST_REG_SAME_REG(test76, jit_modr, 42, 9, 6) 
TEST_REG_OTHER_REG(test77, jit_modr, 42, 9, 6) 


TEST_IMM_SAME_REG(test80, jit_addxi, 42, 28, 70) 
TEST_IMM_OTHER_REG(test81, jit_addxi, 42, 28, 70) 
TEST_REG_SAME_REG(test82, jit_addxr, 42, 28, 70) 
TEST_REG_OTHER_REG(test83, jit_addxr, 42, 28, 70) 

TEST_IMM_SAME_REG(test84, jit_addci, 42, 28, 70) 
TEST_IMM_OTHER_REG(test85, jit_addci, 42, 28, 70) 
TEST_REG_SAME_REG(test86, jit_addcr, 42, 28, 70) 
TEST_REG_OTHER_REG(test87, jit_addcr, 42, 28, 70) 

TEST_IMM_SAME_REG(test90, jit_subxi, 42, 28, 14) 
TEST_IMM_OTHER_REG(test91, jit_subxi, 42, 28, 14) 
TEST_REG_SAME_REG(test92, jit_subxr, 42, 28, 14) 
TEST_REG_OTHER_REG(test93, jit_subxr, 42, 28, 14) 

TEST_IMM_SAME_REG(test94, jit_subci, 42, 28, 14) 
TEST_IMM_OTHER_REG(test95, jit_subci, 42, 28, 14) 
TEST_REG_SAME_REG(test96, jit_subcr, 42, 28, 14) 
TEST_REG_OTHER_REG(test97, jit_subcr, 42, 28, 14) 

TEST_IMM_SAME_REG(test100, jit_ori, 42, 28, (42 | 28)) 
TEST_IMM_OTHER_REG(test101, jit_ori, 42, 28, (42 | 28)) 
TEST_REG_SAME_REG(test102, jit_orr, 42, 28, (42 | 28))
TEST_REG_OTHER_REG(test103, jit_orr, 42, 28, (42 | 28)) 

TEST_IMM_SAME_REG(test110, jit_andi, 42, 28, 8) 
TEST_IMM_OTHER_REG(test111, jit_andi, 42, 28, 8) 
TEST_REG_SAME_REG(test112, jit_andr, 42, 28, 8) 
TEST_REG_OTHER_REG(test113, jit_andr, 42, 28, 8) 

TEST_IMM_SAME_REG(test120, jit_xori, 42, 28, (42 ^ 28)) 
TEST_IMM_OTHER_REG(test121, jit_xori, 42, 28, (42 ^ 28)) 
TEST_REG_SAME_REG(test122, jit_xorr, 42, 28, (42 ^ 28)) 
TEST_REG_OTHER_REG(test123, jit_xorr, 42, 28, (42 ^ 28)) 

TEST_IMM_SAME_REG(test130, jit_lshi, 42, 8, 42 << 8) 
TEST_IMM_OTHER_REG(test131, jit_lshi, 42, 8, 42 << 8) 
TEST_REG_SAME_REG(test132, jit_lshr, 42, 8, 42 << 8) 
TEST_REG_OTHER_REG(test133, jit_lshr, 42, 8, 42 << 8) 

TEST_IMM_SAME_REG(test140, jit_rshi, 42UL, 2, 42L >> 2) 
TEST_IMM_OTHER_REG(test141, jit_rshi, 42UL, 2, 42L >> 2) 
TEST_REG_SAME_REG(test142, jit_rshr, 42UL, 2, 42L >> 2) 
TEST_REG_OTHER_REG(test143, jit_rshr, 42UL, 2, 42L >> 2) 

TEST_IMM_SAME_REG(test144, jit_rshi, -42UL, 2, -42L >> 2) 
TEST_IMM_OTHER_REG(test145, jit_rshi, -42UL, 2, -42L >> 2) 
TEST_REG_SAME_REG(test146, jit_rshr, -42UL, 2, -42L >> 2) 
TEST_REG_OTHER_REG(test147, jit_rshr, -42UL, 2, -42L >> 2) 

TEST_IMM_SAME_REG(test150, jit_rshi_u, 42UL, 2, 42UL >> 2) 
TEST_IMM_OTHER_REG(test151, jit_rshi_u, 42UL, 2, 42UL >> 2) 
TEST_REG_SAME_REG(test152, jit_rshr_u, 42UL, 2, 42UL >> 2) 
TEST_REG_OTHER_REG(test153, jit_rshr_u, 42UL, 2, 42UL >> 2) 

TEST_IMM_SAME_REG(test154, jit_rshi_u, -42UL, 2, -42UL >> 2) 
TEST_IMM_OTHER_REG(test155, jit_rshi_u, -42UL, 2, -42UL >> 2) 
TEST_REG_SAME_REG(test156, jit_rshr_u, -42UL, 2, -42UL >> 2) 
TEST_REG_OTHER_REG(test157, jit_rshr_u, -42UL, 2, -42UL >> 2) 


TEST_UNARY_REG_SAME_REG(test160, jit_negr, 42, -42)
TEST_UNARY_REG_OTHER_REG(test161, jit_negr, 42, -42)


TEST_UNARY_REG_SAME_REG(test162, jit_notr, 42, ~42)
TEST_UNARY_REG_OTHER_REG(test163, jit_notr, 42, ~42)


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

	SETUP_TEST(test70);
	SETUP_TEST(test71);
	SETUP_TEST(test72);
	SETUP_TEST(test73);
	SETUP_TEST(test74);
	SETUP_TEST(test75);
	SETUP_TEST(test76);
	SETUP_TEST(test77);

	SETUP_TEST(test80);
	SETUP_TEST(test81);
	SETUP_TEST(test82);
	SETUP_TEST(test83);
	SETUP_TEST(test84);
	SETUP_TEST(test85);
	SETUP_TEST(test86);
	SETUP_TEST(test87);

	SETUP_TEST(test90);
	SETUP_TEST(test91);
	SETUP_TEST(test92);
	SETUP_TEST(test93);
	SETUP_TEST(test94);
	SETUP_TEST(test95);
	SETUP_TEST(test96);
	SETUP_TEST(test97);

	SETUP_TEST(test100);
	SETUP_TEST(test101);
	SETUP_TEST(test102);
	SETUP_TEST(test103);

	SETUP_TEST(test110);
	SETUP_TEST(test111);
	SETUP_TEST(test112);
	SETUP_TEST(test113);

	SETUP_TEST(test120);
	SETUP_TEST(test121);
	SETUP_TEST(test122);
	SETUP_TEST(test123);

	SETUP_TEST(test130);
	SETUP_TEST(test131);
	SETUP_TEST(test132);
	SETUP_TEST(test133);

	SETUP_TEST(test140);
	SETUP_TEST(test141);
	SETUP_TEST(test142);
	SETUP_TEST(test143);

	SETUP_TEST(test144);
	SETUP_TEST(test145);
	SETUP_TEST(test146);
	SETUP_TEST(test147);

	SETUP_TEST(test150);
	SETUP_TEST(test151);
	SETUP_TEST(test152);
	SETUP_TEST(test153);
	SETUP_TEST(test154);
	SETUP_TEST(test155);
	SETUP_TEST(test156);
	SETUP_TEST(test157);

	SETUP_TEST(test160);
	SETUP_TEST(test161);
	SETUP_TEST(test162);
	SETUP_TEST(test163);
}
