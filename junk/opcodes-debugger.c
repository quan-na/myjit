#include "../dis-asm/dis-asm.h"
#include "../dis-asm/bfd.h"

#define OUTPUT_BUF_SIZE		(8192)


static void override_printf(char * buf, const char * format, ...)
{
	va_list args;

	buf += strlen(buf);

	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);
}

static void override_print_address(bfd_vma addr, struct disassemble_info *info)
{
	char * buf = info->stream;
	buf += strlen(buf);

	sprintf(buf, "0x%lx", (long)addr);
}

static void * get_libocodes_handle()
{
	char buf[1024];

	void * handle;
	handle = dlopen("libopcodes.so", RTLD_LAZY);
	if (handle) return handle;

	FILE * f = popen("/sbin/ldconfig -p", "r");
	while (f && !feof(f)) {
		char * b;
		fgets(buf, 1024, f);
		b = strstr(buf, "libopcodes");
		if (b) {
			fclose(f);
			for (int i = 0; i < strlen(b); i++) {
				if (isspace(b[i])) {
					b[i] = '\0';
					break;
				}
			}
			return dlopen(b, RTLD_LAZY);
		}
	}
	fclose(f);
	return NULL;
}

int opcodes_based_debugger(struct jit * jit)
{
	struct disassemble_info info;
	unsigned long count, pc, buf_size;
	char output_buf[OUTPUT_BUF_SIZE];
	char * error;

	void (* __init_disassemble_info) (struct disassemble_info *info, void *stream, fprintf_ftype fprintf_func);
	int (* __print_insn) (bfd_vma, disassemble_info *);

	void * handle = get_libocodes_handle();

	if (!handle) {
		fprintf(stderr, "Unable to find libopcodes library.\n");
		return -1;
	}

	dlerror();    // Clear any existing error

	*(void **) (&__init_disassemble_info) = dlsym(handle, "init_disassemble_info");

	if ((error = dlerror()) != NULL)  {
		fprintf(stderr, "%s\n", error);
		return -1;
	}

	fprintf_ftype print_fn = (fprintf_ftype) override_printf;
	__init_disassemble_info(&info, output_buf, print_fn);
	info.print_address_func = override_print_address;

#ifdef JIT_ARCH_I386
	info.mach = bfd_mach_i386_i386_intel_syntax;
	info.endian = BFD_ENDIAN_LITTLE;

	*(void **) (&__print_insn) = dlsym(handle, "print_insn_i386_intel");
#endif

#ifdef JIT_ARCH_AMD64
	info.mach = bfd_mach_x86_64_intel_syntax;;
	info.endian = BFD_ENDIAN_LITTLE;

	*(void **) (&__print_insn) = dlsym(handle, "print_insn_x86_64_intel_syntax");
#endif

#ifdef JIT_ARCH_SPARC
	info.mach = bfd_mach_sparc_sparclite;
	info.endian = BFD_ENDIAN_BIG;

	*(void **) (&__print_insn) = dlsym(handle, "print_insn_sparclite");
#endif

	if ((error = dlerror()) != NULL)  {
		fprintf(stderr, "%s\n", error);
		return -1;
	}

	buf_size = (unsigned long)(jit->ip - jit->buf);
	info.buffer = (void *)jit->buf;
	info.buffer_length = buf_size;

	for (pc = 0; pc < buf_size; ) {
		sprintf(output_buf, "%4lx:  ", pc);
		count = __print_insn(pc, &info);
		printf("%s\n", output_buf);
		pc += count;
	}
	return 0;
}
