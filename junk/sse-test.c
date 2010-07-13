#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "../myjit/x86-codegen.h"

void jit_dump_code(unsigned char * buf, int buf_size)
{
	int i;
	FILE * f = stdout;

	char * source_file_name = tempnam(NULL, "myjit");
	char * obj_file_name = tempnam(NULL, "myjit");

	char * cmd1_fmt = "gcc -x assembler -c -o %s %s";
	char cmd1[strlen(cmd1_fmt) + strlen(source_file_name) + strlen(obj_file_name)];

	char * cmd2_fmt = "objdump -d -M intel %s";
	char cmd2[strlen(cmd2_fmt) + strlen(obj_file_name)];

	f = fopen(source_file_name, "w");

	fprintf (f, ".text\n.align 4\n.globl main\n.type main,@function\nmain:\n");
	for (i = 0; i < buf_size; i++)
		fprintf(f, ".byte %d\n", (unsigned int)buf[i]);
	fclose(f);

	sprintf(cmd1, cmd1_fmt, obj_file_name, source_file_name);
	system(cmd1);

	sprintf(cmd2, cmd2_fmt, obj_file_name);
	system(cmd2);

	unlink(source_file_name);
	unlink(obj_file_name);

	free(source_file_name);
	free(obj_file_name);
}

typedef double (*pdfdd)(double, double);

int main()
{
	pdfdd foo;
	unsigned char * buffer;

	posix_memalign((void **)&buffer, sysconf(_SC_PAGE_SIZE), 4096);
	mprotect(buffer, 4096, PROT_READ | PROT_EXEC | PROT_WRITE);

	unsigned char * buf = buffer;
	static const double b = 2.2;
	static const double c = 2.3;

	foo = (pdfdd)buf;
	x86_push_reg(buf, X86_EBP);
	x86_mov_reg_reg(buf, X86_EBP, X86_ESP, 4);

//	x86_movsd_reg_membase(buf, X86_XMM0, X86_ESP, 0);
	x86_movsd_reg_mem(buf, X86_XMM0, &b);
	x86_movsd_reg_mem(buf, X86_XMM1, &c);

	//x86_movsd_reg_reg(buf, X86_XMM0, X86_XMM1);
	//
	//
	x86_movlpd_xreg_membase(buf, X86_XMM1, X86_EBP, 16);
	x86_sse_alu_pd_reg_reg(buf, X86_SSE_DIV, X86_XMM0, X86_XMM1);


	// pushes FP number on the top of the stack 
	// x86_push_imm(buf, *((unsigned long long *)&b) >> 32);
	// x86_push_imm(buf, *((unsigned long long *)&b) && 0xffffffff);

	x86_alu_reg_imm(buf, X86_SUB, X86_ESP, 8);	// creates extra space on the stack
	x86_movlpd_membase_xreg(buf, X86_XMM0, X86_ESP, 0); // pushes the value on the top of the stack
	x86_fld_membase(buf, X86_ESP, 0, 1);		// transfers the value from the stack to the ST(0)

	x86_mov_reg_reg(buf, X86_ESP, X86_EBP, 4);
	x86_pop_reg(buf, X86_EBP);
	x86_ret(buf);

	int buf_size = (int)buf - (int)buffer;
	jit_dump_code(buffer, buf_size);

	double x = foo(1.2, 2.5);
	printf(":XX:%f\n", x);

	return 0;
}
