#include <limits.h>
#include <stddef.h>
#include "../myjit/jitlib.h"
#include "tests.h"
typedef long (*plfsss)(short, short, short);
typedef long (*plfiui)(int, unsigned int);
typedef long (*plfuc)(unsigned char);
typedef long (*plfus)(unsigned short);
typedef long (*plfpcus)(char *, unsigned short);
//typedef long (*plfv)(void);

// function which computes an average of three numbers each occupying 2 bytes 
void test1()
{
	long r;
	struct jit * p = jit_init();

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

	jit_generate_code(p);
	jit_dump_code(p, 0);


	r = f1(20, 20, -10);
	if (r == 10) SUCCESS(10);
	else FAIL(10);

	r = f2(3, 4);
	if (r == 81) SUCCESS(21);
	else FAIL(21);
	
	r = f2(-3, 3);
	if (r == -27) SUCCESS(22);
	else FAIL(22);

	r = f3(5);
	if (r == 120) SUCCESS(31);
	else FAIL(31);

	r = f4(30);
	if (r == 832040) SUCCESS(41);
	else FAIL(41);

	r = f5("1f", 16);
	if (r == 31) SUCCESS(51);
	else FAIL(51);

	jit_free(p);
}

int main() 
{
	test1();
}
