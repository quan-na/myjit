#define _POSIX_C_SOURCE 200809L
#define JIT_REGISTER_TEST
#include "tests.h"

#define BUF_SIZE	(4096)

DEFINE_TEST(test1)
{
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

	jit_generate_code(p);

	jit_value r =  factorial(6);

	fclose(stdout);
	stdout = old_stdout;

	ASSERT_EQ(720, r);
	ASSERT_EQ_STR("Check 1.\nCheck R(1): 1\nCheck R(1): 6\nCheck R(1): 30\nCheck R(1): 120\nCheck R(1): 360\nCheck R(1): 720\nCheck X.\n", buf);


	//printf("Check #3: 6! = %li\n", factorial(6));
	return 0;
}

void test_setup()
{
        test_filename = __FILE__;
        SETUP_TEST(test1);
}
