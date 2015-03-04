#define _POSIX_C_SOURCE 200809L
#define JIT_REGISTER_TEST
#include "tests.h"

#define BUF_SIZE	(65535)

DEFINE_TEST(test1)
{
#ifndef __APPLE__
	char buf[BUF_SIZE];
	FILE *old_stdout = stdout;
	stdout = fmemopen(buf, BUF_SIZE, "w");

	plfl factorial;
	jit_prolog(p, &factorial);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(long));
	jit_getarg(p, R(0), 0);
	jit_movi(p, R(1), 1);

	jit_msg(p, "Check 1.\n");
	jit_label * loop = jit_get_label(p);
	jit_op * o = jit_blei(p, JIT_FORWARD, R(0), 0);
	jit_msgr(p, "Check R(1): %i\n", R(1));
	jit_mulr(p, R(1), R(1), R(0));
	jit_subi(p, R(0), R(0), 1);
	jit_jmpi(p, loop);

	jit_patch(p, o);
	
	jit_msg(p, "Check X.\n");
	jit_retr(p, R(1));

	JIT_GENERATE_CODE(p);

	jit_value r =  factorial(6);

	fclose(stdout);
	stdout = old_stdout;

	ASSERT_EQ(720, r);
	ASSERT_EQ_STR("Check 1.\nCheck R(1): 1\nCheck R(1): 6\nCheck R(1): 30\nCheck R(1): 120\nCheck R(1): 360\nCheck R(1): 720\nCheck X.\n", buf);

	return 0;
#else
	IGNORE_TEST
#endif
}

struct jit_debug_info {
        const char *filename;
        const char *function;
        int lineno;
        int warnings;
};

#define ASSERT_WARNING(op, warning) ASSERT_EQ((warning), (op)->debug_info->warnings);

DEFINE_TEST(test2)
{
#ifndef __APPLE__
	char buf[BUF_SIZE];
	FILE *old_stdout = stdout;
	stdout = fmemopen(buf, BUF_SIZE, "w");

	plfl x;
	jit_op *op01 = jit_prolog(p, &x);
	jit_op *op01a = jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(long));
	jit_op *op01b = jit_declare_arg(p, JIT_PTR, 10);
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_op *op02 = jit_movr(p, R(0), R(1));
	jit_op *op03 = jit_fmovr(p, FR(0), FR(1));

	jit_op *op04 = jit_movr(p, FR(1), R(0));
	jit_label *label1 = jit_get_label(p);
	jit_op *op05 = jit_blti(p, JIT_FORWARD, R(0), 10);
	jit_op *op06 = jit_jmpi(p, JIT_FORWARD);
	jit_op *op07 = jit_retr(p, R(0));

	jit_op *op08 = jit_patch(p, op05);
	jit_op *op09 = jit_ldi(p, R(2), NULL, 3); 
	jit_op *op10 = jit_fldi(p, FR(2), NULL, 2); 

	
	jit_label *label2 = jit_get_label(p);
	jit_data_dword(p, 0xcaca0);

	jit_get_label(p);
	jit_op *op11 = jit_movi(p, R(0), 1);
	jit_op *op12 = jit_ref_code(p, R(0), label2);
	jit_op *op13 = jit_ref_data(p, R(0), label1);

	jit_op *op14 = jit_fsti(p, NULL, R(0), 2);
	jit_op *op15 = jit_fstxi(p, 0, R(0), R(0), 2);

	jit_op *op16 = jit_getarg(p, R(0), 2);
	jit_op *op17 = jit_getarg(p, FR(1), 1);

	jit_op *op18 = jit_fldi(p, R(0), NULL, 4);
	jit_op *op19 = jit_fldxi(p, R(0), R(1), NULL, 4);

	jit_check_code(p, JIT_WARN_ALL);

	fclose(stdout);
	stdout = old_stdout;

	ASSERT_WARNING(op01, JIT_WARN_UNINITIALIZED_REG);
	ASSERT_WARNING(op01a, 0);
	ASSERT_WARNING(op01b, JIT_WARN_INVALID_DATA_SIZE);
	ASSERT_WARNING(op02, 0);
	ASSERT_WARNING(op03, JIT_WARN_OP_WITHOUT_EFFECT);
	ASSERT_WARNING(op04, JIT_WARN_REGISTER_TYPE_MISMATCH | JIT_WARN_OP_WITHOUT_EFFECT);
	ASSERT_WARNING(op05, 0);
	ASSERT_WARNING(op06, JIT_WARN_MISSING_PATCH);
	ASSERT_WARNING(op07, JIT_WARN_DEAD_CODE);
	ASSERT_WARNING(op08, 0);
	ASSERT_WARNING(op09, JIT_WARN_INVALID_DATA_SIZE | JIT_WARN_OP_WITHOUT_EFFECT);
	ASSERT_WARNING(op10, JIT_WARN_INVALID_DATA_SIZE | JIT_WARN_OP_WITHOUT_EFFECT);
	ASSERT_WARNING(op11, JIT_WARN_UNALIGNED_CODE | JIT_WARN_OP_WITHOUT_EFFECT);
	ASSERT_WARNING(op12, JIT_WARN_INVALID_CODE_REFERENCE | JIT_WARN_OP_WITHOUT_EFFECT);
	ASSERT_WARNING(op13, JIT_WARN_INVALID_DATA_REFERENCE);
	ASSERT_WARNING(op14, JIT_WARN_INVALID_DATA_SIZE | JIT_WARN_REGISTER_TYPE_MISMATCH);
	ASSERT_WARNING(op15, JIT_WARN_INVALID_DATA_SIZE | JIT_WARN_REGISTER_TYPE_MISMATCH);
	ASSERT_WARNING(op16, JIT_WARN_REGISTER_TYPE_MISMATCH | JIT_WARN_OP_WITHOUT_EFFECT);
	ASSERT_WARNING(op17, JIT_WARN_REGISTER_TYPE_MISMATCH | JIT_WARN_OP_WITHOUT_EFFECT);
	ASSERT_WARNING(op18, JIT_WARN_REGISTER_TYPE_MISMATCH | JIT_WARN_OP_WITHOUT_EFFECT);
	ASSERT_WARNING(op19, JIT_WARN_REGISTER_TYPE_MISMATCH | JIT_WARN_OP_WITHOUT_EFFECT);
	
	return 0;
#else
	IGNORE_TEST
#endif
}

DEFINE_TEST(test3)
{
#ifndef __APPLE__
	char buf[BUF_SIZE];
	FILE *old_stdout = stdout;
	stdout = fmemopen(buf, BUF_SIZE, "w");

	jit_enable_optimization(p, JIT_OPT_OMIT_UNUSED_ASSIGNEMENTS);
	plfv x;
	jit_prolog(p, &x);
	jit_op *op03 = jit_movi(p, R(0), 10);
	jit_movi(p, R(0), 20);
	jit_addi(p, R(0), R(0), 30);
	jit_retr(p, R(0));

	JIT_GENERATE_CODE(p);
	
	ASSERT_EQ(50, x());
	ASSERT_EQ(JIT_NOP, op03->code);

	fclose(stdout);
	stdout = old_stdout;
		
	return 0;
#else
	IGNORE_TEST
#endif
}

void test_setup()
{
        test_filename = __FILE__;
        SETUP_TEST(test1);
        SETUP_TEST(test2);
        SETUP_TEST(test3);
}
