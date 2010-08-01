#include <stdlib.h>
#include <stdio.h>

#define JIT_REGISTER_TEST
#include "../myjit/jitlib.h"

typedef long (* plfl)(long);

int main()
{
	struct jit * p = jit_init();
	plfl factorial;
	jit_prolog(p, &factorial);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(long));
	jit_getarg(p, R(0), 0);
	jit_movi(p, R(1), 1);

	jit_msgi(p, "Check 1.\n");
	jit_label * loop = jit_get_label(p);
	jit_op * o = jit_blei(p, JIT_FORWARD, R(0), 0);
	jit_msgr(p, "Check R(1): %i\n", R(1));
	jit_mulr(p, R(1), R(1), R(0));
	jit_subi(p, R(0), R(0), 1);
	jit_jmpi(p, loop);

	jit_patch(p, o);
	
	jit_msgi(p, "Check X.\n");
	jit_retr(p, R(1));

	jit_generate_code(p);
	jit_dump_ops(p, 0);
	jit_dump_code(p, 0);

	printf("Check #3: 6! = %li\n", factorial(6));

	// cleanup
	jit_free(p);
	return 0;
}
