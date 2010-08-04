#include <limits.h>
#include <math.h>
#include "../myjit/jitlib.h"
#include "tests.h"

#define CREATE_TEST(__test_name,__jit_func,__fn) \
void __test_name(double val, int id) \
{ \
	long r;\
	struct jit * p = jit_init(); \
	plfv f1; \
	jit_prolog(p, &f1); \
	jit_fmovi(p, FR(0), val); \
	__jit_func(p, R(0), FR(0)); \
	jit_retr(p, R(0)); \
	jit_generate_code(p); \
\
	r = f1();\
	double x = __fn (val);\
	if (equal(r, x, 0.0001)) SUCCESS(id); \
	else FAIL(id);\
\
	jit_free(p);\
}

CREATE_TEST(trunc_test,jit_truncr,trunc)
CREATE_TEST(round_test,jit_roundr,round)
CREATE_TEST(ceil_test,jit_ceilr,ceil)
CREATE_TEST(floor_test,jit_floorr,floor)

int main() 
{
	int count = 8;
	static double test[] = { 2.0, 2.1, 2.5, 2.7, -2.0, -2.1, -2.5, -2.7 };

	for (int i = 0; i < count; i++) trunc_test(test[i], 10 + i);
	for (int i = 0; i < count; i++) round_test(test[i], 20 + i);
	for (int i = 0; i < count; i++) ceil_test(test[i], 30 + i);
	for (int i = 0; i < count; i++) floor_test(test[i], 40 + i);
}
