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
	
	jit_data_addr(p, R(0), label_string); 

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

	jit_op * label_string = jit_data_addr(p, R(0), JIT_FORWARD);
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

void test13()
{
	jit_value r;
	struct jit * p = jit_init();
	jit_disable_optimization(p, JIT_OPT_ALL);

	plfv f1;
	jit_prolog(p, &f1);

	jit_movi(p, R(0), 0xaa);

	jit_op *target_addr = jit_code_addr(p, R(1), JIT_FORWARD);

	jit_jmpr(p, R(1));
	jit_movi(p, R(0), 0xbb);

	jit_patch(p, target_addr);
	jit_retr(p, R(0));

	jit_generate_code(p);
//	jit_dump_ops(p, JIT_DEBUG_ASSOC);
//	jit_dump_code(p, 0);

	r = f1();

	if (r == 0xaa) SUCCESS(13);
	else FAIL(13);

	jit_free(p);
}

void test14() 
{
	jit_value r;
	struct jit * p = jit_init();
	jit_disable_optimization(p, JIT_OPT_ALL);

	plfl f1;
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(int));

	jit_movi(p, R(0), 10);

	jit_op *target1_addr = jit_code_addr(p, R(1), JIT_FORWARD);
	jit_op *target2_addr = jit_code_addr(p, R(2), JIT_FORWARD);

	jit_movr(p, R(3), R(1));
	jit_movi(p, R(4), 20);
	jit_getarg(p, R(5), 0);

	jit_op *skip = jit_bgti(p, JIT_FORWARD, R(5), 0);

	jit_movr(p, R(3), R(2));
	jit_movi(p, R(4), 40);

	jit_patch(p, skip);



	jit_jmpr(p, R(3));

	jit_code_align(p, 16);
	jit_patch(p, target1_addr);
	jit_addr(p, R(0), R(0), R(4));
	jit_addr(p, R(0), R(0), R(5));
	
	jit_retr(p, R(0));


	jit_code_align(p, 16);
	jit_patch(p, target2_addr);
	jit_subr(p, R(0), R(0), R(4));
	jit_subr(p, R(0), R(0), R(5));
	
	jit_retr(p, R(0));



	jit_generate_code(p);
//	jit_dump_ops(p, JIT_DEBUG_LIVENESS | JIT_DEBUG_ASSOC);
//	jit_dump_code(p, 0);

	r = f1(1);

	if (r == (10 + 1 + 20)) SUCCESS(14);
	else FAIL(14);


	r = f1(-1);
	if (r == (10 + 1 - 40)) SUCCESS(14);
	else FAIL(14);

	jit_free(p);
}


int main() 
{
	test10();
	test11();
	test12();
	test13();
	test14();
}
