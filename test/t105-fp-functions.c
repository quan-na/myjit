#include <limits.h>
#include <stddef.h>
#include "../myjit/jitlib.h"
#include "tests.h"
typedef double (*pdfdfd)(double, float, double);
typedef float (*pffdui)(double, unsigned int);
typedef double (*pdfuc)(unsigned char);
typedef double (*pdfd)(double);
typedef double (*pdfpcus)(char *, unsigned short);
//typedef long (*plfv)(void);

// function which computes an average of three numbers  
void test1()
{
	double r;
	struct jit * p = jit_init();
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
	jit_fretr(p, FR(0));
	jit_generate_code(p);

	jit_dump_code(p, 0);

	r = f1(20, 20, -10);
	printf("DD:%f\n", r);
	if (equal(r, 10, 0.001)) SUCCESS(10);
	else FAIL(10);

	jit_free(p);
}

// function which computes n^m
void test2()
{
	float r;
	struct jit * p = jit_init();
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
	jit_fretr(p, FR(2));
	jit_generate_code(p);
	jit_dump_code(p, 0);

	r = f1(3, 4);
	printf("LL:%f\n", r);
	if (equal(r, 81, 0.001)) SUCCESS(21);
	else FAIL(21);
	
	r = f1(-3, 3);
	if (equal(r, -27, 0.001)) SUCCESS(22);
	else FAIL(22);

	jit_free(p);
}

// function which computes factorial
void test3()
{
	double r;
	struct jit * p = jit_init();
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

	jit_fretr(p, FR(1));

	jit_generate_code(p);

	r = f1(5);
	if (equal(r, 120, 0.001)) SUCCESS(31);
	else FAIL(31);

	jit_free(p);
}

// function which computes fibonacci's number
void test4()
{
	/*
	double r;
	struct jit * p = jit_init();
	pdfd f1;

	jit_label * fib = jit_get_label(p);
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));

	jit_getarg(p, FR(0), 0);

	jit_op * br1 = jit_fblti(p, JIT_FORWARD, FR(0), 3);
	jit_op * gr = jit_jmpi(p, JIT_FORWARD);

	jit_patch(p, br1);
	jit_op * br = jit_jmpi(p, JIT_FORWARD);

	jit_patch(p, gr);
	jit_fsubi(p, FR(0), FR(0), 1);

	jit_prepare(p, 1);
	jit_fputargr(p, FR(0), sizeof(double));
	jit_call(p, fib);

	jit_fretval(p, FR(1));
//	jit_fmovi(p, FR(1), 4);

	jit_getarg(p, FR(0), 0);
	jit_fsubi(p, FR(0), FR(0), 2);
	jit_prepare(p, 1);
	jit_fputargr(p, FR(0), sizeof(double));
	jit_call(p, fib);

	jit_fretval(p, FR(2));

	jit_faddr(p, FR(0), FR(1), FR(2));

	jit_fretr(p, FR(0));

	jit_patch(p, br);
	jit_freti(p, 1.2);

	jit_generate_code(p);

//	jit_dump_ops(p, 0); return;
	jit_dump_code(p, 0); return;

	r = f1(3);
	printf("::%f\n", r);
	if (r == 832040) SUCCESS(41);
	else FAIL(41);

	jit_free(p);
	*/
	// always fails, since it requires long conditional jumps
	FAIL(41);
}

// function which converts string to number
void test5()
{
	double r;
	struct jit * p = jit_init();
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
	jit_fretr(p, FR(4));
	jit_generate_code(p);

//	jit_dump_ops(p, 0); return;
//	jit_dump_code(p, 0); return;
	r = f1("1f", 16);
	printf("::%f\n", r);
	if (equal(r, 31, 0.0001)) SUCCESS(51);
	else FAIL(51);


	jit_free(p);
}

// prints ``hello, world!''
void test6()
{
	long r;
	static char * str = "Hello, World! Number of the day is %f!!!\n";
	struct jit * p = jit_init();
	plfv f1; 

	jit_prolog(p, &f1);
	jit_movi(p, R(0), str);
	
	jit_fmovi(p, FR(1), 12345.678);

	jit_movi(p, R(2), printf);

	jit_prepare(p, 2);
	jit_putargr(p, R(0));
	jit_fputargr(p, FR(1), sizeof(double));
	jit_callr(p, R(2));

	jit_retval(p, R(3));

	jit_retr(p, R(2));
	jit_generate_code(p);
//	jit_dump_ops(p, 0); return;
//	jit_dump_code(p, 0); return;
	r = f1();
//	printf("::%i\n", r);


	jit_free(p);
}

// prints numbers 1, 2, 4, 8, 16, 32, 64, 128, 256, 512
void test7()
{
	static int ARR_SIZE = 10;
	long r;
	static char * formatstr = "%f\n";
	struct jit * p = jit_init();
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

	//jit_addi(p, R(0), R(0), 2 * REG_SIZE);

	jit_fldr(p, FR(1), R(0), sizeof(double));
	jit_prepare(p, 2);
	jit_putargi(p, formatstr);
	jit_fputargr(p, FR(1), sizeof(double));
	jit_call(p, printf);

	jit_addi(p, R(0), R(0), sizeof(double));
	jit_subi(p, R(2), R(2), 1);
	jit_bgei(p, loop2, R(2), 0);

	jit_reti(p, 0);
	jit_generate_code(p);
//	jit_dump_ops(p, 0); return;
//	jit_dump_code(p, 0); return;
	r = f1();
//	printf("::%i\n", r);
	
	jit_free(p);
}

struct mystruct {
	float * items;
	long count;
	float sum;
	double avg;
};

// sums integers in a an array and computes their sum and average (uses structs)
void test8()
{
	long r;
	static char * formatstr = "sum: %f\navg: %f\n";
	struct jit * p = jit_init();

	static struct mystruct s = { NULL, 0, 0, 0};
	s.items = (float []){1, 2, 3, 4, 5};
	s.count = 5;
	s.sum = 111;
	s.avg = 123;

	plfv f1; 

	jit_prolog(p, &f1);

	jit_ldi(p, R(0), &s + offsetof(struct mystruct, count), sizeof(long));	// count

	jit_movi(p, R(1), &s); // struct

	jit_ldi(p, R(2), &s + offsetof(struct mystruct, items), sizeof(void *)); // array
	jit_movi(p, R(3), 0); // index

	jit_fmovi(p, FR(5), 0);		// sum
	jit_label * loop = jit_get_label(p);

	jit_fldxr(p, FR(4), R(2), R(3), sizeof(float));
	jit_faddr(p, FR(5), FR(5), FR(4));
	
	jit_addi(p, R(3), R(3), sizeof(float));
	jit_subi(p, R(0), R(0), 1);

	jit_bgti(p, loop, R(0), 0);
	jit_fsti(p, &s + offsetof(struct mystruct, sum), FR(5), sizeof(float));
	jit_ldi(p, R(0), &s + offsetof(struct mystruct, count), sizeof(long));	// count

	jit_extr(p, FR(0), R(0));
	jit_fdivr(p, FR(5), FR(5), FR(0));

	jit_movi(p, R(0), &s);
	jit_movi(p, R(1), offsetof(struct mystruct, avg));
	jit_fstxi(p, &s, R(1), FR(5), sizeof(double));
	//jit_fstxr(p, R(0), R(1), FR(5), sizeof(double));

	jit_fldi(p, FR(0), &s + offsetof(struct mystruct, avg), sizeof(double));
	jit_fldi(p, FR(1), &s + offsetof(struct mystruct, sum), sizeof(float));

	jit_prepare(p, 3);
	jit_putargi(p, formatstr);
	jit_fputargr(p, FR(0), sizeof(double));
	jit_fputargr(p, FR(1), sizeof(double));

	jit_call(p, printf);

	jit_retr(p, R(1));

	jit_generate_code(p);
//	jit_dump_ops(p, 0); return;
//	jit_dump_code(p, 0); return;
	r = f1();

	jit_free(p);
}

int main() 
{
	test1();
	test2(); 
	test3();
	test4(); // XXX
  	test5(); // XXX
	test6(); // XXX
	test7(); // XXX
	test8(); 
}
