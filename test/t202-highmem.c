#undef _XOPEN_SOURCE
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/mman.h>

#include "tests.h"

#ifndef MAP_ANONYMOUS
	#define MAP_ANONYMOUS MAP_ANON
#endif

static unsigned char *xxx_alloc_addr = (void *)0x100000000000UL;

static inline jit_value jit_value_align(jit_value value, jit_value alignment)
{
        return (value + (alignment - 1)) & (- alignment);
}

void *xxx_alloc(size_t size) 
{
	size = jit_value_align(size, sysconf(_SC_PAGE_SIZE));
	if (size == 0) size = sysconf(_SC_PAGE_SIZE);
	void *result = mmap(xxx_alloc_addr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE , -1, 0);
	xxx_alloc_addr += size;
	return result;
}

void xxx_free(void *obj)
{
}

DEFINE_TEST(test1)
{
	pdfv f1;
	jit_prolog(p, &f1);
	jit_fmovi(p, FR(0), 12.3);
	jit_faddi(p, FR(0), FR(0), 11.1);
	jit_fretr(p, FR(0), sizeof(double));
	JIT_GENERATE_CODE(p);

	ASSERT_EQ_DOUBLE(23.4, f1());

	return 0;
}



void test_setup() 
{
	test_filename = __FILE__;
#ifdef JIT_ARCH_AMD64
	SETUP_TEST(test1);
#endif
}
