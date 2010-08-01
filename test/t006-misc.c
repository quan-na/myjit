#include <limits.h>
#include <stddef.h>
#include "../myjit/jitlib.h"
#include "tests.h"
typedef long (*plfsss)(short, short, short);
typedef long (*plfiui)(int, unsigned int);
typedef long (*plfuc)(unsigned char);
typedef long (*plfus)(unsigned short);
typedef long (*plfpcus)(char *, unsigned short);

#define LOOP_CNT	(10000000)
// implements ternary operator (old-fashioned way); end tests efficiency
void test1(long cond, long value1, long value2)
{
	long r;
	struct jit * p = jit_init();
	plfl f1;
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(long));

	jit_getarg(p, R(0), 0);
	jit_movi(p, R(1), LOOP_CNT);

	jit_label * loop = jit_get_label(p);

	jit_op * eq = jit_beqi(p, JIT_FORWARD, R(0), cond);
	jit_movi(p, R(2), value2);
	jit_op * end = jit_jmpi(p, JIT_FORWARD);
	jit_patch(p, eq);
	jit_movi(p, R(2), value1);
	jit_patch(p, end);

	jit_subi(p, R(1), R(1), 1);
	jit_bgti(p, loop, R(1), 0);

	jit_retr(p, R(2));
	jit_generate_code(p);

	r = f1(10);
	if (r == 666) SUCCESS(10);
	else FAIL(10);

	jit_free(p);
}

// implements ternary operator (tricky); end tests efficiency
void test2(long cond, long value1, long value2)
{
	long r;
	struct jit * p = jit_init();
	plfl f1;
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(long));

	jit_getarg(p, R(0), 0);
	jit_movi(p, R(1), LOOP_CNT);

	jit_label * loop = jit_get_label(p);

	jit_eqi(p, R(2), R(0), cond);

	jit_subi(p, R(2), R(2), 1);
	jit_andi(p, R(2), R(2), value2 - value1);
	jit_addi(p, R(2), R(2), value1);


	jit_subi(p, R(1), R(1), 1);
	jit_bgti(p, loop, R(1), 0);

	jit_retr(p, R(2));
	jit_generate_code(p);
//	jit_dump_code(p, 0);

	r = f1(10);
	printf(":::%i\n", r);
	if (r == 666) SUCCESS(11);
	else FAIL(11);

	jit_free(p);
}


int main() 
{
	test1(10, 666, 777);
	test2(10, 666, 777);
}
