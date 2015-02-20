#include <stddef.h>
#include "tests.h"
#include "simple-buffer.c"

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
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(10, f1(20, 20, -10));
	return 0;
}

// function which computes n^m
DEFINE_TEST(test2)
{
	plfiui f1;
	jit_prolog(p, &f1);
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
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(81, f1(3, 4));
	ASSERT_EQ(-27, f1(-3, 3));
	return 0;
}

// function which computes factorial
DEFINE_TEST(test3)
{
	plfuc f1;

	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_UNSIGNED_NUM, sizeof(char));
	jit_getarg(p, R(0), 0);

	jit_movi(p, R(1), 1);

	jit_label * loop = jit_get_label(p);

	jit_op * end = jit_beqi(p, JIT_FORWARD, R(0), 0);

	jit_mulr(p, R(1), R(1), R(0));
	jit_subi(p, R(0), R(0), 1);

	jit_jmpi(p, loop);
	jit_patch(p, end);
	jit_retr(p, R(1));

	JIT_GENERATE_CODE(p);

	ASSERT_EQ(120, f1(5));
	return 0;
}

// function which computes fibonacci's number
DEFINE_TEST(test4)
{
	plfus f1;

	jit_label * fib = jit_get_label(p);
	jit_prolog(p, &f1);
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

	JIT_GENERATE_CODE(p);
	ASSERT_EQ(832040, f1(30));
	return 0;
}

// function which converts string to number
DEFINE_TEST(test5)
{
	plfpcus f1; // string, radix -> long 

	jit_prolog(p, &f1);
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

	jit_label * loop = jit_get_label(p);

	jit_ldxr_u(p, R(3), R(0), R(2), 1);
	jit_op * end = jit_beqi(p, JIT_FORWARD, R(3), 0);
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
	jit_jmpi(p, loop);

	jit_patch(p, end);
	jit_retr(p, R(4));

	JIT_GENERATE_CODE(p);
	ASSERT_EQ(31, f1("1f", 16));
	return 0;
}

// prints ``hello, world!''
DEFINE_TEST(test6)
{
	static char *str = "Hello, World! Lucky number for today is %i!!!\n";
	static char buf[120];
	plfv f1; 

	jit_prolog(p, &f1);
	jit_movi(p, R(0), str);
	
	jit_movi(p, R(1), 100);
	//jit_movi(p, R(2), 200);
	//jit_movi(p, R(3), 300);
	jit_movi(p, R(4), 400);
	//jit_movi(p, R(5), 500);

	jit_movi(p, R(2), sprintf);

	jit_prepare(p);
	jit_putargi(p, buf);
	jit_putargr(p, R(0));
	jit_putargr(p, R(1));
	jit_callr(p, R(2));

	//jit_retval(p, R(3));

	jit_retr(p, R(4));

	JIT_GENERATE_CODE(p);
	ASSERT_EQ(400, f1());
	ASSERT_EQ_STR("Hello, World! Lucky number for today is 100!!!\n", buf);
	return 0;
}

// prints numbers 1, 2, 4, 8, 16, 32, 64, 128, 256, 512
DEFINE_TEST(test7)
{
	static int ARR_SIZE = 10;
	static char * formatstr = "%i ";

	simple_buffer(BUFFER_CLEAR, NULL);

	plfv f1; 

	jit_prolog(p, &f1);
	int arr = jit_allocai(p, ARR_SIZE  * REG_SIZE); // allocates an array

	jit_movi(p, R(0), 1);				// sets up the first element
	jit_stxi(p, arr, R_FP, R(0), REG_SIZE);	
	
	jit_addi(p, R(0), R_FP, arr);			// pointer to the array
	jit_addi(p, R(1), R(0), REG_SIZE);		// position in the array (2nd element)
	jit_movi(p, R(2), ARR_SIZE - 1);		// counter

	jit_label * loop = jit_get_label(p);

	jit_ldxi(p, R(3), R(1), -REG_SIZE, REG_SIZE);
	jit_lshi(p, R(3), R(3), 1);
	jit_str(p, R(1), R(3), REG_SIZE);

	jit_addi(p, R(1), R(1), REG_SIZE);
	jit_subi(p, R(2), R(2), 1);
	jit_bgti(p, loop, R(2), 0);

	jit_movi(p, R(2), ARR_SIZE - 1);		

	jit_label * loop2 = jit_get_label(p);

	jit_ldr(p, R(1), R(0), REG_SIZE);
	jit_prepare(p);
	jit_putargi(p, BUFFER_PUT);
	jit_putargi(p, formatstr);
	jit_putargr(p, R(1));
	jit_call(p, simple_buffer);

	jit_addi(p, R(0), R(0), REG_SIZE);
	jit_subi(p, R(2), R(2), 1);
	jit_bgei(p, loop2, R(2), 0);

	jit_reti(p, 0);
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(0, f1());
	ASSERT_EQ_STR("1 2 4 8 16 32 64 128 256 512 ", simple_buffer(BUFFER_GET, NULL));
	return 0;
}

struct mystruct {
	short * items;
	long count;
	short sum;
	int avg;
};

// sums integers in a an array and computes their sum and average (uses structs)
DEFINE_TEST(test8)
{
	static char * formatstr = "avg: %i\nsum: %i\n";
	simple_buffer(BUFFER_CLEAR, NULL);

	static struct mystruct s = { NULL, 0, 0, 0};
	s.items = (short []){1, 2, 3, 4, 5};
	s.count = 5;
	s.sum = 111;
	s.avg = 123;

	plfv f1; 

	jit_prolog(p, &f1);

	jit_ldi(p, R(0), (unsigned char *)&s + offsetof(struct mystruct, count), sizeof(long));	// count

	//jit_movi(p, R(1), &s); // struct

	jit_ldi(p, R(2), (unsigned char *)&s + offsetof(struct mystruct, items), sizeof(void *)); // array
	jit_movi(p, R(3), 0); // index

	jit_movi(p, R(5), 0);		// sum
	jit_label * loop = jit_get_label(p);


	jit_ldxr(p, R(4), R(2), R(3), sizeof(short));
	jit_addr(p, R(5), R(5), R(4));
	
	jit_addi(p, R(3), R(3), sizeof(short));
	jit_subi(p, R(0), R(0), 1);

	jit_bgti(p, loop, R(0), 0);
	jit_sti(p, (unsigned char *)&s + offsetof(struct mystruct, sum), R(5), sizeof(short));
	jit_ldi(p, R(0), (unsigned char *)&s + offsetof(struct mystruct, count), sizeof(long));	// count

	jit_divr(p, R(5), R(5), R(0));

	jit_movi(p, R(0), &s);
	jit_movi(p, R(1), offsetof(struct mystruct, avg));
	jit_stxr(p, R(0), R(1), R(5), sizeof(int));

	jit_ldi(p, R(0), (unsigned char *)&s + offsetof(struct mystruct, avg), sizeof(int));
	jit_ldi(p, R(1), (unsigned char *)&s + offsetof(struct mystruct, sum), sizeof(short));

	jit_prepare(p);
	jit_putargi(p, BUFFER_PUT);
	jit_putargi(p, formatstr);
	jit_putargr(p, R(0));
	jit_putargr(p, R(1));

	jit_call(p, simple_buffer);

	jit_retr(p, R(1));

	JIT_GENERATE_CODE(p);

	ASSERT_EQ(15, f1());
	ASSERT_EQ_STR("avg: 3\nsum: 15\n", simple_buffer(BUFFER_GET, NULL));
	ASSERT_EQ(15, s.sum);
	ASSERT_EQ(3, s.avg);
	return 0;

}

void test_setup()
{
	test_filename = __FILE__;
	SETUP_TEST(test1);
	SETUP_TEST(test2);
	SETUP_TEST(test3);
	SETUP_TEST(test4);
	SETUP_TEST(test5);
	SETUP_TEST(test6);
	SETUP_TEST(test7);
	SETUP_TEST(test8);
}
