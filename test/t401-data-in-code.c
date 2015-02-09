#include <limits.h>
#include "../myjit/jitlib.h"
#include "tests.h"

void test10()
{
	jit_value r;
	struct jit * p = jit_init();
	plfv f1;
	jit_prolog(p, &f1);
	jit_op *skip_data = jit_jmpi(p, JIT_FORWARD);

	jit_data_byte(p, 0xaa);
	jit_data_byte(p, 0xbb);
	jit_data_str(p, "Hello!");

	jit_data_emptyarea(p, 10);
	jit_data_qword(p, 0xaabbccdd00112233);
	jit_code_align(p, 16);

	jit_patch(p, skip_data);
	jit_movi(p, R(0), 7);
	jit_movi(p, R(1), 15);
	jit_addr(p, R(2), R(0), R(1));
	jit_retr(p, R(2));
	jit_generate_code(p);
	jit_dump_ops(p, JIT_DEBUG_COMPILABLE);

	r = f1();

	if (r == (7 + 15)) SUCCESS(10);
	else FAIL(10);

	jit_free(p);
}
int main() 
{
	test10();
}
