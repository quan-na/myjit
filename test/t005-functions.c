#include <limits.h>
#include <stddef.h>
#include "../myjit/jitlib.h"
#include "tests.h"
typedef long (*plfsss)(short, short, short);
typedef long (*plfiui)(int, unsigned int);
typedef long (*plfuc)(unsigned char);
typedef long (*plfus)(unsigned short);
typedef long (*plfpcus)(char *, unsigned short);
//typedef long (*plfv)(void);

// function which computes an average of three numbers each occupying 2 bytes 
void test1()
{
	long r;
	struct jit * p = jit_init();
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
	jit_generate_code(p);

//	jit_dump_code(p, 0);

	r = f1(20, 20, -10);
	printf("DD:%i\n", r);
	if (r == 10) SUCCESS(10);
	else FAIL(10);

	jit_free(p);
}

// function which computes n^m
void test2()
{
	long r;
	struct jit * p = jit_init();
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
	jit_generate_code(p);
	jit_dump_code(p, 0);


	r = f1(3, 4);
	if (r == 81) SUCCESS(21);
	else FAIL(21);
	
	r = f1(-3, 3);
	if (r == -27) SUCCESS(22);
	else FAIL(22);

	jit_free(p);
}

// function which computes factorial
void test3()
{
	long r;
	struct jit * p = jit_init();
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

	jit_generate_code(p);

	r = f1(5);
	if (r == 120) SUCCESS(31);
	else FAIL(31);


	jit_free(p);
}

// function which computes fibonacci's number
void test4()
{
	long r;
	struct jit * p = jit_init();
	plfus f1;

	jit_label * fib = jit_get_label(p);
	jit_prolog(p, &f1);
	jit_declare_arg(p, JIT_UNSIGNED_NUM, sizeof(short));

	jit_getarg(p, R(0), 0);

	jit_op * br = jit_blti_u(p, JIT_FORWARD, R(0), 3);

	jit_subi(p, R(0), R(0), 1);

	jit_prepare(p, 1);
	jit_putargr(p, R(0));
	jit_call(p, fib);

	jit_retval(p, R(1));

	jit_getarg(p, R(0), 0);
	jit_subi(p, R(0), R(0), 2);
	jit_prepare(p, 1);
	jit_putargr(p, R(0));
	jit_call(p, fib);

	jit_retval(p, R(2));

	jit_addr(p, R(0), R(1), R(2));

	jit_retr(p, R(0));

	jit_patch(p, br);
	jit_reti(p, 1);

	jit_generate_code(p);

//	jit_dump_ops(p, 0); return;
	jit_dump_code(p, 0); return;


	r = f1(30);
	printf("::%i\n", r);
	if (r == 832040) SUCCESS(41);
	else FAIL(41);

	jit_free(p);
}

// function which converts string to number
void test5()
{
	long r;
	struct jit * p = jit_init();
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
	jit_generate_code(p);

//	jit_dump_ops(p, 0); return;
//	jit_dump_code(p, 0); return;
	r = f1("1f", 16);
	printf("::%i\n", r);
	if (r == 31) SUCCESS(51);
	else FAIL(51);


	jit_free(p);
}

// prints ``hello, world!''
void test6()
{
	long r;
	static char * str = "Hello, World! Number of the day is %i!!!\n";
	struct jit * p = jit_init();
	plfv f1; 

	jit_prolog(p, &f1);
	jit_movi(p, R(0), str);
	
	jit_movi(p, R(1), 12345);
	jit_movi(p, R(2), 100);
	jit_movi(p, R(3), 200);
	jit_movi(p, R(4), 300);
	jit_movi(p, R(5), 400);

	jit_movi(p, R(2), printf);

	jit_prepare(p, 2);
	jit_putargr(p, R(0));
	jit_putargr(p, R(1));
	jit_callr(p, R(2));

	jit_retval(p, R(3));

	/*
	jit_movi(p, R(2), 100);
	jit_movi(p, R(3), 100);
	jit_movi(p, R(4), 100);
*/

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
	static char * formatstr = "%i\n";
	struct jit * p = jit_init();
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

	//jit_addi(p, R(0), R(0), 2 * REG_SIZE);

	jit_ldr(p, R(1), R(0), REG_SIZE);
	jit_prepare(p, 2);
	jit_putargi(p, formatstr);
	jit_putargr(p, R(1));
	jit_call(p, printf);

	jit_addi(p, R(0), R(0), REG_SIZE);
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
	short * items;
	long count;
	short sum;
	int avg;
};

// sums integers in a an array and computes their sum and average (uses structs)
void test8()
{
	long r;
	static char * formatstr = "sum: %i\navg: %i\n";
	struct jit * p = jit_init();

	static struct mystruct s = { NULL, 0, 0, 0};
	s.items = (short []){1, 2, 3, 4, 5};
	s.count = 5;
	s.sum = 111;
	s.avg = 123;

	plfv f1; 

	jit_prolog(p, &f1);

	jit_ldi(p, R(0), &s + offsetof(struct mystruct, count), sizeof(long));	// count

	jit_movi(p, R(1), &s); // struct

	jit_ldi(p, R(2), &s + offsetof(struct mystruct, items), sizeof(void *)); // array
	jit_movi(p, R(3), 0); // index

	jit_movi(p, R(5), 0);		// sum
	jit_label * loop = jit_get_label(p);


	jit_ldxr(p, R(4), R(2), R(3), sizeof(short));
	jit_addr(p, R(5), R(5), R(4));
	
	jit_addi(p, R(3), R(3), sizeof(short));
	jit_subi(p, R(0), R(0), 1);

	jit_bgti(p, loop, R(0), 0);
	jit_sti(p, &s + offsetof(struct mystruct, sum), R(5), sizeof(short));
	jit_ldi(p, R(0), &s + offsetof(struct mystruct, count), sizeof(long));	// count

	jit_divr(p, R(5), R(5), R(0));

	jit_movi(p, R(0), &s);
	jit_movi(p, R(1), offsetof(struct mystruct, avg));
	jit_stxr(p, R(0), R(1), R(5), sizeof(int));

	jit_ldi(p, R(0), &s + offsetof(struct mystruct, avg), sizeof(int));
	jit_ldi(p, R(1), &s + offsetof(struct mystruct, sum), sizeof(short));

	jit_prepare(p, 3);
	jit_putargi(p, formatstr);
	jit_putargr(p, R(0));
	jit_putargr(p, R(1));

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
	test4(); 
	/*
	test1();
	test2(); 
	test3();
	test4(); // XXX
  	test5(); // XXX
	test6(); // XXX
	test7(); // XXX
	test8(); 
	*/
}
