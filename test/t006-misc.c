#include "tests.h"

#define LOOP_CNT	(10000000)
int value1 = 666;
int value2 = 777;

int cond_value = 10;

// implements ternary operator (old-fashioned way); end tests efficiency
DEFINE_TEST(test1)
{
	plfl f1;
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(long));

	jit_getarg(p, R(0), 0);
	jit_movi(p, R(1), LOOP_CNT);

	jit_label * loop = jit_get_label(p);

	jit_op * eq = jit_beqi(p, JIT_FORWARD, R(0), cond_value);
	jit_movi(p, R(2), value2);
	jit_op * end = jit_jmpi(p, JIT_FORWARD);
	jit_patch(p, eq);
	jit_movi(p, R(2), value1);
	jit_patch(p, end);

	jit_subi(p, R(1), R(1), 1);
	jit_bgti(p, loop, R(1), 0);

	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(666, f1(10));
	ASSERT_EQ(777, f1(20));
	return 0;
}

// implements ternary operator (tricky); end tests efficiency
DEFINE_TEST(test2)
{
	plfl f1;
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(long));

	jit_getarg(p, R(0), 0);
	jit_movi(p, R(1), LOOP_CNT);

	jit_label * loop = jit_get_label(p);

	jit_eqi(p, R(2), R(0), cond_value);

	jit_subi(p, R(2), R(2), 1);
	jit_andi(p, R(2), R(2), value2 - value1);
	jit_addi(p, R(2), R(2), value1);


	jit_subi(p, R(1), R(1), 1);
	jit_bgti(p, loop, R(1), 0);

	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(666, f1(10));
	ASSERT_EQ(777, f1(20));

	return 0;
}

void test_setup()
{
	test_filename = __FILE__;
	SETUP_TEST(test1);
	SETUP_TEST(test2);
}
