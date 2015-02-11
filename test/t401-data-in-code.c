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
	//jit_dump_ops(p, JIT_DEBUG_COMPILABLE);

	r = f1();

	if (r == (7 + 15)) SUCCESS(10);
	else FAIL(10);

	jit_free(p);
}

int check_func(char *str) {
	if (!strcmp(str, "ABCDEF!")) return 42;
	return -1;
}

void test11()
{
	jit_value r;
	struct jit * p = jit_init();
	jit_disable_optimization(p, JIT_OPT_ALL);

	plfv f1;
	jit_prolog(p, &f1);
	jit_op *skip_data = jit_jmpi(p, JIT_FORWARD);

	// data
	jit_data_byte(p, 0xaa);
	jit_data_byte(p, 0xbb);
	
	jit_label *label_string = jit_get_label(p);
	jit_data_str(p, "ABCDEF!");

	jit_code_align(p, 32);


	// code
	jit_patch(p, skip_data);
	
	jit_code_addr(p, R(0), label_string); 

	jit_prepare(p);
	jit_putargr(p, R(0));
	jit_call(p, check_func);
	jit_retval(p, R(0));

	jit_retr(p, R(0));

	jit_generate_code(p);
//	jit_dump_ops(p, JIT_DEBUG_ASSOC);
//	jit_dump_code(p, 0);

	r = f1();

	if (r == 42) SUCCESS(11);
	else FAIL(11);

	jit_free(p);
}

void test12()
{
	jit_value r;
	struct jit * p = jit_init();
	jit_disable_optimization(p, JIT_OPT_ALL);

	plfv f1;
	jit_prolog(p, &f1);

	jit_op * label_string = jit_code_addr(p, R(0), JIT_FORWARD);
	jit_prepare(p);
	jit_putargr(p, R(0));
	jit_call(p, check_func);
	jit_retval(p, R(0));
	jit_retr(p, R(0));

	// data
	jit_code_align(p,32);
	jit_data_byte(p, 0xaa);
	jit_data_byte(p, 0xbb);

	jit_patch(p, label_string);
	jit_data_str(p, "ABCDEF!");

//	jit_dump_ops(p, 0);
	jit_generate_code(p);
//	jit_dump_code(p, 0);

	r = f1();

	if (r == 42) SUCCESS(12);
	else FAIL(12);

	jit_free(p);
}


int main() 
{
	test10();
	test11();
	test12();
}
