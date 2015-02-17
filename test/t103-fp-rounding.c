#include <math.h>
#include "tests.h"

int count = 8;
double test_data[] = { 2.0, 2.1, 2.5, 2.7, -2.0, -2.1, -2.5, -2.7 };

#define CREATE_TEST(_test_name, _jit_func, _fn) \
DEFINE_TEST(_test_name) \
{ \
	plfd f1; \
	jit_disable_optimization(p, JIT_OPT_ALL); \
	jit_prolog(p, &f1); \
	jit_declare_arg(p, JIT_FLOAT_NUM, sizeof(double));\
	jit_getarg(p, FR(0), 0); \
	_jit_func(p, R(0), FR(0)); \
	jit_retr(p, R(0)); \
	JIT_GENERATE_CODE(p); \
\
	for (int i = 0; i < count; i++) \
		ASSERT_EQ_DOUBLE(_fn(test_data[i]), f1(test_data[i])); \
\
	return 0;\
}

CREATE_TEST(trunc_test, jit_truncr, trunc)
CREATE_TEST(round_test, jit_roundr, round)
CREATE_TEST(ceil_test, jit_ceilr, ceil)
CREATE_TEST(floor_test, jit_floorr, floor)

void test_setup()
{
	test_filename = __FILE__;
	SETUP_TEST(trunc_test);
	SETUP_TEST(round_test);
	SETUP_TEST(ceil_test);
	SETUP_TEST(floor_test);
}
