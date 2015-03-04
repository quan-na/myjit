#define JIT_REGISTER_TEST
#include "tests.h"
#include "simple-buffer.c"

#define BLOCK_SIZE	(16)

int correct_result;

static int foobar(double a1, double a2, double a3, double a4, double a5, double a6, double a7, double a8, double a9, double a10)
{
        correct_result = 1;
        if (!equals(a1, -1, 0.0001)) correct_result = 0;
        if (!equals(a2, 1, 0.0001)) correct_result = 0;
        if (!equals(a3, 2, 0.0001)) correct_result = 0;
        if (!equals(a4, 4, 0.0001)) correct_result = 0;
        if (!equals(a5, 8, 0.0001)) correct_result = 0;
        if (!equals(a6, 16, 0.0001)) correct_result = 0;
        if (!equals(a7, 32, 0.0001)) correct_result = 0;
        if (!equals(a8, 64, 0.0001)) correct_result = 0;
        if (!equals(a9, 128, 0.0001)) correct_result = 0;
        if (!equals(a10, 256, 0.0001)) correct_result = 0;
	return 666;
}

DEFINE_TEST(test1)
{
	correct_result = 2;
	static char * msg2 = "%u ";

	plfv foo;

	jit_prolog(p, &foo);
	int i = jit_allocai(p, BLOCK_SIZE);

	// loads into the allocated memory multiples of 3
	jit_movi(p, R(0), 0);	// index
	jit_addi(p, R(1), R_FP, i); // pointer to the allocated memory
	jit_label * lab = jit_get_label(p);
	jit_muli(p, R(2), R(0), 3);
	jit_stxr(p, R(1), R(0), R(2), 1);
	jit_addi(p, R(0), R(0), 1);
	jit_blti(p, lab, R(0), BLOCK_SIZE);

	// does something with registers
	jit_fmovi(p, FR(0), -1);
	jit_fmovi(p, FR(1), 1);
	jit_fmovi(p, FR(2), 2);
	jit_fmovi(p, FR(3), 4);
	jit_fmovi(p, FR(4), 8);
	jit_fmovi(p, FR(5), 16);
	jit_fmovi(p, FR(6), 32);
	jit_fmovi(p, FR(7), 64);
	jit_fmovi(p, FR(8), 128);
	jit_fmovi(p, FR(9), 256);

	jit_prepare(p);
	jit_fputargr(p, FR(0), sizeof(double));
	jit_fputargr(p, FR(1), sizeof(double));
	jit_fputargr(p, FR(2), sizeof(double));
	jit_fputargr(p, FR(3), sizeof(double));
	jit_fputargr(p, FR(4), sizeof(double));
	jit_fputargr(p, FR(5), sizeof(double));
	jit_fputargr(p, FR(6), sizeof(double));
	jit_fputargr(p, FR(7), sizeof(double));
	jit_fputargr(p, FR(8), sizeof(double));
	jit_fputargr(p, FR(9), sizeof(double));
	jit_call(p, foobar);

	// print outs allocated memory

	jit_movi(p, R(0), 0);	// index
	jit_addi(p, R(1), R_FP, i); // pointer to the allocated memory
	jit_label * lab2 = jit_get_label(p);
	
	jit_ldxr(p, R(2), R(1), R(0), 1);

	jit_prepare(p);
	jit_putargi(p, BUFFER_PUT);
	jit_putargi(p, msg2);
	jit_putargr(p, R(2));
	jit_call(p, simple_buffer);

	jit_addi(p, R(0), R(0), 1);
	jit_blti(p, lab2, R(0), BLOCK_SIZE);

	jit_retr(p, R(0));


	JIT_GENERATE_CODE(p);
	ASSERT_EQ(16, foo());
	ASSERT_EQ(1, correct_result);
	ASSERT_EQ_STR("0 3 6 9 12 15 18 21 24 27 30 33 36 39 42 45 ", simple_buffer(BUFFER_GET, NULL));
	return 0;
}

void test_setup()
{
	test_filename = __FILE__;
	SETUP_TEST(test1);
}
