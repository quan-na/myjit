#include "tests.h"

DEFINE_TEST(test10)
{
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
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(7 + 15, f1());
	return 0;
}

int check_func(char *str) {
	if (!strcmp(str, "ABCDEF!")) return 42;
	return -1;
}

DEFINE_TEST(test11)
{
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
	
	jit_ref_data(p, R(0), label_string); 

	jit_prepare(p);
	jit_putargr(p, R(0));
	jit_call(p, check_func);
	jit_retval(p, R(0));

	jit_retr(p, R(0));

	JIT_GENERATE_CODE(p);

	ASSERT_EQ(42, f1());
	return 0;
}

DEFINE_TEST(test12)
{
	plfv f1;
	jit_prolog(p, &f1);

	jit_op * label_string = jit_ref_data(p, R(0), JIT_FORWARD);
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

	JIT_GENERATE_CODE(p);

	ASSERT_EQ(42, f1());
	return 0;
}

DEFINE_TEST(test13)
{
	plfv f1;
	jit_prolog(p, &f1);

	jit_movi(p, R(0), 0xaa);

	jit_op *target_addr = jit_ref_code(p, R(1), JIT_FORWARD);

	jit_jmpr(p, R(1));

	jit_patch(p, target_addr);
	jit_retr(p, R(0));

	JIT_GENERATE_CODE(p);
	ASSERT_EQ(0xaa, f1());
	return 0;
}

DEFINE_TEST(test14)
{
	plfl f1;
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(int));

	jit_movi(p, R(0), 10);

	jit_op *target1_addr = jit_ref_code(p, R(1), JIT_FORWARD);
	jit_op *target2_addr = jit_ref_code(p, R(2), JIT_FORWARD);

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

	JIT_GENERATE_CODE(p);

	ASSERT_EQ(10 + 1 + 20, f1(1));
	ASSERT_EQ(10 + 1 - 40, f1(-1));
	return 0;
}


DEFINE_TEST(test15)
{
	plfl f1;
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(int));

	jit_op *skip = jit_jmpi(p, JIT_FORWARD);
	jit_code_align(p, 16);

	// data
	jit_comment(p, "\ndata\n");
	jit_label *data_addr = jit_get_label(p);
	jit_op *target1_addr = jit_data_ref_code(p, JIT_FORWARD);
	jit_op *target2_addr = jit_data_ref_code(p, JIT_FORWARD);
	jit_code_align(p, 16);

	
	// code
	jit_comment(p, "code");
	jit_patch(p, skip);
	jit_movi(p, R(0), 10);
	jit_getarg(p, R(1), 0);
	jit_ref_data(p, R(2), data_addr);

	jit_muli(p, R(3), R(1), PTR_SIZE);
	jit_ldxr(p, R(2), R(2), R(3), PTR_SIZE);

	jit_jmpr(p, R(2));

	jit_patch(p, target1_addr);
	jit_addr(p, R(0), R(0), R(1));
	jit_retr(p, R(0));

	jit_patch(p, target2_addr);
	jit_subr(p, R(0), R(0), R(1));
	jit_retr(p, R(0));

	JIT_GENERATE_CODE(p);

	ASSERT_EQ(10, f1(0));
	ASSERT_EQ(9, f1(1));
	return 0;
}

DEFINE_TEST(test16)
{
	ppfl f1;
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(int));

	jit_op *skip = jit_jmpi(p, JIT_FORWARD);
	jit_code_align(p, 16);

	// data
	jit_comment(p, "data: days of week");
	jit_label *lb_mon = jit_get_label(p);
	jit_data_str(p, "Monday");	
	jit_label *lb_tue = jit_get_label(p);
	jit_data_str(p, "Tuesday");	
	jit_label *lb_wed = jit_get_label(p);
	jit_data_str(p, "Wednesday");	
	jit_label *lb_thr = jit_get_label(p);
	jit_data_str(p, "Thursday");	
	jit_label *lb_fri = jit_get_label(p);
	jit_data_str(p, "Friday");	
	jit_code_align(p, 16);


	jit_comment(p, "data: branch table");
	jit_label *data_addr = jit_get_label(p);
	jit_data_ref_data(p, lb_mon);
	jit_data_ref_data(p, lb_tue);
	jit_data_ref_data(p, lb_wed);
	jit_data_ref_data(p, lb_thr);
	jit_data_ref_data(p, lb_fri);
	jit_code_align(p, 16);

	
	// code
	jit_patch(p, skip);
	jit_getarg(p, R(1), 0);
	jit_ref_data(p, R(2), data_addr);

	jit_muli(p, R(3), R(1), PTR_SIZE);
	jit_ldxr(p, R(0), R(2), R(3), PTR_SIZE);

	jit_retr(p, R(0));

	JIT_GENERATE_CODE(p);

	ASSERT_EQ_STR("Wednesday", f1(2));
	return 0;
}

DEFINE_TEST(test17)
{
	plfl f1;
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_SIGNED_NUM, sizeof(int));

	jit_op *branch_table = jit_ref_data(p, R(0), JIT_FORWARD);
	jit_getarg(p, R(1), 0);
	jit_muli(p, R(1), R(1), PTR_SIZE);
	jit_ldxr(p, R(2), R(1), R(0), PTR_SIZE);
	jit_jmpr(p, R(2));

	// branches
	jit_label *branch1 = jit_get_label(p);
	jit_reti(p, 10);

	jit_label *branch2 = jit_get_label(p);
	jit_reti(p, 20);

	jit_label *branch3 = jit_get_label(p);
	jit_reti(p, 30);
	
	// branch table
	jit_code_align(p, 16);
	jit_patch(p, branch_table);
	jit_data_ref_code(p, branch1);
	jit_data_ref_code(p, branch2);
	jit_data_ref_code(p, branch3);

	JIT_GENERATE_CODE(p);

	ASSERT_EQ(10, f1(0));
	ASSERT_EQ(20, f1(1));
	ASSERT_EQ(30, f1(2));
	return 0;
}



void test_setup() 
{
	test_filename = __FILE__;
	SETUP_TEST(test10);
	SETUP_TEST(test11);
	SETUP_TEST(test12);
	SETUP_TEST(test13);
	SETUP_TEST(test14);
	SETUP_TEST(test15);
	SETUP_TEST(test16);
	SETUP_TEST(test17);
}
