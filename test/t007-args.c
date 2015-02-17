#include "tests.h"

typedef long (* plf10s)(short, short, short, short, short, short, short, short, short, short);

DEFINE_TEST(test1)
{
	plf10s foo;

	jit_prolog(p, &foo);

	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	
	jit_getarg(p, R(0), 0);
	jit_getarg(p, R(1), 1);
	jit_getarg(p, R(2), 2);
	jit_getarg(p, R(3), 3);
	jit_getarg(p, R(4), 4);
	jit_getarg(p, R(5), 5);
	jit_getarg(p, R(6), 6);
	jit_getarg(p, R(7), 7);
	jit_getarg(p, R(8), 8);
	jit_getarg(p, R(9), 9);

	
	jit_movi(p, R(10), 0);
	jit_addr(p, R(10), R(10), R(9));
	jit_addr(p, R(11), R(10), R(8));
	jit_addr(p, R(12), R(11), R(7));
	jit_addr(p, R(13), R(12), R(6));
	jit_addr(p, R(14), R(13), R(5));
	jit_addr(p, R(15), R(14), R(4));
	jit_addr(p, R(16), R(15), R(3));
	jit_addr(p, R(17), R(16), R(2));
	jit_addr(p, R(18), R(17), R(1));

	jit_addr(p, R(8), R(8), R(9));
	jit_addr(p, R(7), R(7), R(8));
	jit_addr(p, R(6), R(6), R(7));
	jit_addr(p, R(5), R(5), R(6));
	jit_addr(p, R(4), R(4), R(5));
	jit_addr(p, R(3), R(3), R(4));
	jit_addr(p, R(2), R(2), R(3));
	jit_addr(p, R(1), R(1), R(2));
	jit_addr(p, R(0), R(0), R(1));

	jit_addr(p, R(0), R(0), R(18));

	jit_retr(p, R(0));

	JIT_GENERATE_CODE(p);

	ASSERT_EQ(109, foo(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
	return 0;
}

void test_setup()
{
	test_filename = __FILE__;
	SETUP_TEST(test1);
}
