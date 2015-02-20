#include "tests.h"

static int foo(int a, int b)
{
	return a * a + b * b;
}

DEFINE_TEST(test1)
{
	plfv f1;
	jit_prolog(p, &f1);
	jit_movi(p, R(0), 10);
	jit_addi(p, R(1), R(0), 1);
	jit_addr(p, R(2), R(0), R(1));
	jit_addr(p, R(3), R(2), R(1));
	jit_addr(p, R(4), R(3), R(0));
	jit_addr(p, R(5), R(0), R(1));
	//jit_addr(p, R(6), R(2), R(3));
	//jit_addr(p, R(7), R(4), R(5));
	//jit_addr(p, R(8), R(0), R(1));
	//jit_addr(p, R(9), R(2), R(3));
	jit_addr(p, R(10), R(4), R(5));
	jit_movi(p, R(11), foo);

	jit_prepare(p);
	jit_putargi(p, 2);
	jit_putargi(p, 3);
	jit_callr(p, R(11));
	jit_retval(p, R(0));
	jit_addr(p, R(0), R(0), R(10));
	jit_retr(p, R(0));
	JIT_GENERATE_CODE(p);
	
	ASSERT_EQ(76, f1());
	return 0;
}



void test_setup() 
{
	test_filename = __FILE__;
	SETUP_TEST(test1);
}
