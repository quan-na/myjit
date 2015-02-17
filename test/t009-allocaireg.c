#define JIT_REGISTER_TEST
#include "tests.h"
#include "simple-buffer.c"

#define BLOCK_SIZE	(16)

int correct_result;

static int foobar(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
	correct_result = 1;
	if (a1 != -1) correct_result = 0;
	if (a2 != 1) correct_result = 0;
	if (a3 != 2) correct_result = 0;
	if (a4 != 4) correct_result = 0;
	if (a5 != 8) correct_result = 0;
	if (a6 != 16) correct_result = 0;
	if (a7 != 32) correct_result = 0;
	if (a8 != 64) correct_result = 0;
	if (a9 != 128) correct_result = 0;
	if (a10 != 256) correct_result = 0;
	return 666;
}

DEFINE_TEST(test1)
{
	static char *msg2 = "%u ";

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
	jit_movi(p, R(0), -1);
	jit_movi(p, R(1), 1);
	jit_movi(p, R(2), 2);
	jit_movi(p, R(3), 4);
	jit_movi(p, R(4), 8);
	jit_movi(p, R(5), 16);
	jit_movi(p, R(6), 32);
	jit_movi(p, R(7), 64);
	jit_movi(p, R(8), 128);
	jit_movi(p, R(9), 256);

	jit_prepare(p);
	jit_putargr(p, R(0));
	jit_putargr(p, R(1));
	jit_putargr(p, R(2));
	jit_putargr(p, R(3));
	jit_putargr(p, R(4));
	jit_putargr(p, R(5));
	jit_putargr(p, R(6));
	jit_putargr(p, R(7));
	jit_putargr(p, R(8));
	jit_putargr(p, R(9));
	jit_call(p, foobar);

	// prints out allocated memory

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
