#include <stddef.h>
#include "tests.h"
#include "simple-buffer.c"

typedef double (*pdfdfd)(double, float, double);
typedef float (*pffdui)(double, unsigned int);
typedef double (*pdfuc)(unsigned char);
typedef double (*pdfpcus)(char *, unsigned short);

// function which computes an average of three numbers  
DEFINE_TEST(test1)
{
	pdfdfd f1;
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(float));
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_getarg(p, FR(0), 0);
	jit_getarg(p, FR(1), 1);
	jit_faddr(p, FR(0), FR(0), FR(1));
	jit_getarg(p, FR(1), 2);
	jit_faddr(p, FR(0), FR(0), FR(1));
	jit_fdivi(p, FR(0), FR(0), 3);
	jit_fretr(p, FR(0), sizeof(double));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ_DOUBLE(10.0, f1(20, 20, -10));
	return 0;
}

// function which computes n^m
DEFINE_TEST(test2)
{
	pffdui f1;

	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));
	jit_declare_arg(p, JIT_UNSIGNED_NUM, sizeof(int));
	jit_getarg(p, FR(0), 0);
	jit_getarg(p, R(1), 1);

	jit_fmovi(p, FR(2), 1);

	jit_label * loop = jit_get_label(p);
	jit_op * end = jit_beqi(p, JIT_FORWARD, R(1), 0);
	jit_fmulr(p, FR(2), FR(2), FR(0));
	jit_subi(p, R(1), R(1), 1);
	jit_jmpi(p, loop);

	jit_patch(p, end);	
	jit_fretr(p, FR(2), sizeof(float));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ_DOUBLE(81.0, f1(3, 4));
        ASSERT_EQ_DOUBLE(-27.0, f1(-3, 3));
        return 0;
}

// function which computes factorial
DEFINE_TEST(test3)
{
	pdfuc f1;

	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_UNSIGNED_NUM, sizeof(char));
	jit_getarg(p, R(0), 0);

	jit_fmovi(p, FR(1), 1);

	jit_label * loop = jit_get_label(p);

	jit_op * end = jit_beqi(p, JIT_FORWARD, R(0), 0);

	jit_extr(p, FR(0), R(0));
	jit_fmulr(p, FR(1), FR(1), FR(0));
	jit_subi(p, R(0), R(0), 1);

	jit_jmpi(p, loop);
	jit_patch(p, end);

	jit_fretr(p, FR(1), sizeof(double));

	JIT_GENERATE_CODE(p);
	ASSERT_EQ_DOUBLE(120.0, f1(5));
	return 0;
}

// function which computes fibonacci's number
DEFINE_TEST(test4)
{
	pdfd f1;

	jit_label * fib = jit_get_label(p);
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));

	jit_getarg(p, FR(0), 0);

	jit_op * br = jit_fblti(p, JIT_FORWARD, FR(0), 3);

	jit_fsubi(p, FR(0), FR(0), 1);

	jit_prepare(p);
	jit_fputargr(p, FR(0), sizeof(double));
	jit_call(p, fib);

	jit_fretval(p, FR(1), sizeof(double));

	jit_getarg(p, FR(0), 0);
	jit_fsubi(p, FR(0), FR(0), 2);
	jit_prepare(p);
	jit_fputargr(p, FR(0), sizeof(double));
	jit_call(p, fib);

	jit_fretval(p, FR(2), sizeof(double));

	jit_faddr(p, FR(0), FR(1), FR(2));

	jit_fretr(p, FR(0), sizeof(double));

	jit_patch(p, br);

	jit_freti(p, 1.0, sizeof(double));

	JIT_GENERATE_CODE(p);

        JIT_GENERATE_CODE(p);
        ASSERT_EQ_DOUBLE(832040.0, f1(30));
        return 0;
}

// function which converts string to number
DEFINE_TEST(test5)
{
	pdfpcus f1; // string, radix -> long 

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
	jit_fmovi(p, FR(4), 0);
	jit_extr(p, FR(1), R(1));

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
	jit_fmulr(p, FR(4), FR(4), FR(1));
	jit_extr(p, FR(3), R(3));
	jit_faddr(p, FR(4), FR(4), FR(3));
	jit_addi(p, R(2), R(2), 1);
	jit_jmpi(p, loop);

	jit_patch(p, end);
	jit_fretr(p, FR(4), sizeof(double));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ_DOUBLE(31.0, f1("1f", 16));
        return 0;
}

// prints ``hello, world!''
DEFINE_TEST(test6)
{
	static char *str = "Hello, World! Lucky number for today is %.3f!!!\n";
        static char buf[120];
	plfv f1; 

	jit_prolog(p, &f1);
	jit_movi(p, R(0), str);
	
	jit_fmovi(p, FR(1), 12345.678);

	jit_movi(p, R(2), sprintf);

	jit_prepare(p);
	jit_putargi(p, buf);
	jit_putargr(p, R(0));
	jit_fputargr(p, FR(1), sizeof(double));
	jit_callr(p, R(2));

	//jit_retval(p, R(3));

	jit_retr(p, R(2));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ((jit_value)sprintf, f1());
	ASSERT_EQ_STR("Hello, World! Lucky number for today is 12345.678!!!\n", buf);
	return 0;
}

// prints numbers 1, 2, 4, 8, 16, 32, 64, 128, 256, 512
DEFINE_TEST(test7)
{
	static int ARR_SIZE = 10;
	static char * formatstr = "%.1f ";

	simple_buffer(BUFFER_CLEAR, NULL);

	plfv f1; 

	jit_prolog(p, &f1);
	int arr = jit_allocai(p, ARR_SIZE  * sizeof(double)); // allocates an array

	jit_fmovi(p, FR(0), 1);			// sets up the first element
	jit_fstxi(p, arr, R_FP, FR(0), sizeof(double));	
	
	jit_addi(p, R(0), R_FP, arr);			// pointer to the array
	jit_addi(p, R(1), R(0), sizeof(double));	// position in the array (2nd element)
	jit_movi(p, R(2), ARR_SIZE - 1);		// counter

	jit_label * loop = jit_get_label(p);

	jit_fldxi(p, FR(3), R(1), -sizeof(double), sizeof(double));
	jit_faddr(p, FR(3), FR(3), FR(3));
	jit_fstr(p, R(1), FR(3), sizeof(double));

	jit_addi(p, R(1), R(1), sizeof(double));
	jit_subi(p, R(2), R(2), 1);
	jit_bgti(p, loop, R(2), 0);

	jit_movi(p, R(2), ARR_SIZE - 1);		

	jit_label * loop2 = jit_get_label(p);

	jit_fldr(p, FR(1), R(0), sizeof(double));
	jit_prepare(p);
	jit_putargi(p, BUFFER_PUT);
	jit_putargi(p, formatstr);
	jit_fputargr(p, FR(1), sizeof(double));
	jit_call(p, simple_buffer);

	jit_addi(p, R(0), R(0), sizeof(double));
	jit_subi(p, R(2), R(2), 1);
	jit_bgei(p, loop2, R(2), 0);

	jit_reti(p, 0);
	JIT_GENERATE_CODE(p);

	ASSERT_EQ(0, f1());
	ASSERT_EQ_STR("1.0 2.0 4.0 8.0 16.0 32.0 64.0 128.0 256.0 512.0 ", simple_buffer(BUFFER_GET, NULL));
	return 0;
}

struct mystruct {
	float * items;
	long count;
	float sum;
	double avg;
};

// sums integers in a an array and computes their sum and average (uses structs)
DEFINE_TEST(test8)
{
	static char * formatstr = "avg: %.1f\nsum: %.1f\n";
	simple_buffer(BUFFER_CLEAR, NULL);

	static struct mystruct s = { NULL, 0, 0, 0};
	s.items = (float []){1, 2, 3, 4, 5};
	s.count = 5;
	s.sum = 111;
	s.avg = 123;

	plfv f1; 

	jit_prolog(p, &f1);

	jit_ldi(p, R(0), (unsigned char *)&s + offsetof(struct mystruct, count), sizeof(long));	// count

	//jit_movi(p, R(1), &s); // struct

	jit_ldi(p, R(2), (unsigned char *)&s + offsetof(struct mystruct, items), sizeof(void *)); // array
	jit_movi(p, R(3), 0); // index

	jit_fmovi(p, FR(5), 0);		// sum
	jit_label * loop = jit_get_label(p);

	jit_fldxr(p, FR(4), R(2), R(3), sizeof(float));
	jit_faddr(p, FR(5), FR(5), FR(4));
	
	jit_addi(p, R(3), R(3), sizeof(float));
	jit_subi(p, R(0), R(0), 1);

	jit_bgti(p, loop, R(0), 0);
	jit_fsti(p, (unsigned char *)&s + offsetof(struct mystruct, sum), FR(5), sizeof(float));
	jit_ldi(p, R(0), (unsigned char *)&s + offsetof(struct mystruct, count), sizeof(long));	// count

	jit_extr(p, FR(0), R(0));
	jit_fdivr(p, FR(5), FR(5), FR(0));

	//jit_movi(p, R(0), &s);
	jit_movi(p, R(1), offsetof(struct mystruct, avg));
	jit_fstxi(p, &s, R(1), FR(5), sizeof(double));

	jit_fldi(p, FR(0), (unsigned char *)&s + offsetof(struct mystruct, avg), sizeof(double));
	jit_fldi(p, FR(1), (unsigned char *)&s + offsetof(struct mystruct, sum), sizeof(float));

	jit_prepare(p);
	jit_putargi(p, BUFFER_PUT);
	jit_putargi(p, formatstr);
	jit_fputargr(p, FR(0), sizeof(double));
	jit_fputargr(p, FR(1), sizeof(double));

	jit_call(p, simple_buffer);

	jit_reti(p, 0);

	JIT_GENERATE_CODE(p);

        ASSERT_EQ(0, f1());
        ASSERT_EQ_STR("avg: 3.0\nsum: 15.0\n", simple_buffer(BUFFER_GET, NULL));
        ASSERT_EQ_DOUBLE(15.0, s.sum);
        ASSERT_EQ_DOUBLE(3.0, s.avg);
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
