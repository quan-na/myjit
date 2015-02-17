#include "tests.h"

typedef jit_value (*plfsss)(short, short, short);
typedef jit_value (*plfiui)(int, unsigned int);
typedef jit_value (*plfuc)(unsigned char);
typedef jit_value (*plfus)(unsigned short);
typedef jit_value (*plfpcus)(char *, unsigned short);

// function which computes an average of three numbers each occupying 2 bytes 
DEFINE_TEST(test1)
{
	plfsss f1;
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(short));
	jit_getarg(p, R(0), 0);
	jit_getarg(p, R(1), 1);
	jit_addr(p, R(0), R(0), R(1));
	jit_getarg(p, R(1), 2);
	jit_addr(p, R(0), R(0), R(1));
	jit_divi(p, R(0), R(0), 3);
	jit_retr(p, R(0));

	// TEST 2
	// function which computes n^m

	plfiui f2;
	jit_prolog(p, &f2);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(int));
	jit_declare_arg(p, JIT_UNSIGNED_NUM, sizeof(int));
	jit_getarg(p, R(0), 0);
	jit_getarg(p, R(1), 1);

	jit_movi(p, R(2), 1);

	jit_label * loop = jit_get_label(p);
	jit_op * end = jit_beqi(p, JIT_FORWARD, R(1), 0);
	jit_mulr(p, R(2), R(2), R(0));
	jit_subi(p, R(1), R(1), 1);
	jit_jmpi(p, loop);

	jit_patch(p, end);	
	jit_retr(p, R(2));

	// TEST 3
	// function which computes factorial
	plfuc f3;

	jit_prolog(p, &f3);
	jit_declare_arg(p, JIT_UNSIGNED_NUM, sizeof(char));
	jit_getarg(p, R(0), 0);

	jit_movi(p, R(1), 1);

	jit_label * loop2 = jit_get_label(p);

	jit_op * end2 = jit_beqi(p, JIT_FORWARD, R(0), 0);

	jit_mulr(p, R(1), R(1), R(0));
	jit_subi(p, R(0), R(0), 1);

	jit_jmpi(p, loop2);
	jit_patch(p, end2);
	jit_retr(p, R(1));

	// TEST 4
	// function which computes fibonacci's number
	plfus f4;

	jit_label * fib = jit_get_label(p);
	jit_prolog(p, &f4);
	jit_declare_arg(p, JIT_UNSIGNED_NUM, sizeof(short));

	jit_getarg(p, R(0), 0);

	jit_op * br = jit_blti_u(p, JIT_FORWARD, R(0), 3);

	jit_subi(p, R(0), R(0), 1);

	jit_prepare(p);
	jit_putargr(p, R(0));
	jit_call(p, fib);

	jit_retval(p, R(1));

	jit_getarg(p, R(0), 0);
	jit_subi(p, R(0), R(0), 2);
	jit_prepare(p);
	jit_putargr(p, R(0));
	jit_call(p, fib);

	jit_retval(p, R(2));

	jit_addr(p, R(0), R(1), R(2));

	jit_retr(p, R(0));

	jit_patch(p, br);
	jit_reti(p, 1);

	// TEST 5

	plfpcus f5; // string, radix -> long 

	jit_prolog(p, &f5);
	jit_declare_arg(p, JIT_UNSIGNED_NUM, sizeof(long));
	jit_declare_arg(p, JIT_UNSIGNED_NUM, sizeof(short));

	// R(0): string
	// R(1): radix
	// R(2): position in string
	// R(3): current char
	// R(4): result
	jit_getarg(p, R(0), 0);
	jit_getarg(p, R(1), 1);
	jit_movi(p, R(2), 0);
	jit_movi(p, R(4), 0);

	jit_label * loop3 = jit_get_label(p);

	jit_ldxr_u(p, R(3), R(0), R(2), 1);
	jit_op * end3 = jit_beqi(p, JIT_FORWARD, R(3), 0);
	jit_op * lowercase = jit_bgti_u(p, JIT_FORWARD, R(3), 'Z');
	jit_op * uppercase = jit_bgti_u(p, JIT_FORWARD, R(3), '9');
	jit_subi(p, R(3), R(3), '0');
	jit_op * comput = jit_jmpi(p, JIT_FORWARD);

	// is upper case char
	jit_patch(p, uppercase);
	jit_subi(p, R(3), R(3), 'A' - 10);
	jit_op * comput0 = jit_jmpi(p, JIT_FORWARD);

	jit_patch(p, lowercase);
	jit_subi(p, R(3), R(3), 'a' - 10);
	jit_op * comput1 = jit_jmpi(p, JIT_FORWARD);

	// computations
	jit_patch(p, comput);
	jit_patch(p, comput0);
	jit_patch(p, comput1);
	jit_mulr(p, R(4), R(4), R(1));
	jit_addr(p, R(4), R(4), R(3));
	jit_addi(p, R(2), R(2), 1);
	jit_jmpi(p, loop3);

	jit_patch(p, end3);
	jit_retr(p, R(4));

	JIT_GENERATE_CODE(p);

	ASSERT_EQ(10, f1(20, 20, -10));
	ASSERT_EQ(81, f2(3, 4));
	ASSERT_EQ(-27, f2(-3, 3));
	ASSERT_EQ(120, f3(5));
	ASSERT_EQ(832040, f4(30));
	ASSERT_EQ(31, f5("1f", 16));
	return 0;
}

void test_setup() 
{
	test_filename = __FILE__;
	SETUP_TEST(test1);
}
