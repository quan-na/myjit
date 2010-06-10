#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#ifndef JIT_ARCH
#define JIT_ARCH x86
#endif

#ifndef JIT_MALLOC
#define JIT_MALLOC(x)	(malloc(x))
#endif

#ifndef JIT_FREE
#define JIT_FREE(x)	(free(x))
#endif

#define INT_SIZE (sizeof(int))
#define PTR_SIZE (sizeof(void *))
#define REG_SIZE (sizeof(void *))

#define JIT_FORCE_REG(r) (r | 1 << 31)


#define NO  0x00
#define REG 0x01
#define IMM 0x02
#define TREG 0x03 /* target register */
#define SPEC(ARG1, ARG2, ARG3) (((ARG3) << 4) | ((ARG2) << 2) | (ARG1))
#define SPEC_SIGN(SIGN, ARG1, ARG2, ARG3) ((SIGN << 6) | ((ARG3) << 4) | ((ARG2) << 2) | (ARG1))
struct jit_op {
	unsigned char op_code;
	unsigned char op_spec;
	unsigned char op_variant;
	long arg1;
	long arg2;
	long arg3;
	long r_arg1;
	long r_arg2;
	long r_arg3;
};

#define jit_movr(jit, a, b, c) jit_add_op(jit, JIT_MOV | REG, SPEC(TREG, REG, IMM), a, b, c)
#define jit_movi(jit, a, b, c) jit_add_op(jit, JIT_MOV | IMM, SPEC(TREG, IMM, IMM), a, b, c)
#define jit_addr(jit, a, b, c) jit_add_op(jit, JIT_ADD | REG, SPEC(TREG, REG, REG), a, b, c)
#define jit_addi(jit, a, b, c) jit_add_op(jit, JIT_ADD | IMM, SPEC(TREG, REG, IMM), a, b, c)
#define jit_bltr(jit, a, b, c) jit_add_op(jit, JIT_BLT | REG | SIGN, SPEC(IMM, REG, REG), a, b, c) 
#define jit_blti(jit, a, b, c) jit_add_op(jit, JIT_BLT | IMM | SIGN, SPEC(IMM, REG, IMM), a, b, c) 
#define jit_bltr_u(jit, a, b, c) jit_add_op(jit, JIT_BLT | REG | UNSIGNED, SPEC(IMM, REG, REG), a, b, c) 
#define jit_blti_u(jit, a, b, c) jit_add_op(jit, JIT_BLT | REG | UNSIGNED, SPEC(IMM, REG, IMM), a, b, c) 


void jit_gen_op(struct jit * jit, struct jit_op * op)
{
	//load_effective_values(jit, op);
	
	switch (op->opcode) {
		case JIT_MOV | REG: x86_mov_reg_reg(jit->ip, op->r_arg1, op->r_arg2, op->r_arg3); break;
		case JIT_MOV | IMM: x86_mov_reg_imm(jit->ip, op->r_arg1, op->r_arg2, op->r_arg3); break;
		case JIT_ADD | REG:  break;
		case JIT_ADD | IMM: x86_alu_reg_imm(jit->ip, X86_ADD, op->r_arg1, op->r_arg2); break;
		default: fprintf(stderr, "Unknown operation");
	}
	
}



typedef int reg_t;

struct jit {
	unsigned char * buf;
	unsigned char * ip;
	size_t buffer_size;
	int argpos;
	unsigned int prepare_args;
	unsigned int reg_count;
	long * reg;
	struct jit_register_info * reg_info;
	struct jit_native_reg_info ** nat_reg_info;
};

struct jit_native_reg_info {
	reg_t virtual_reg;
	int reg_id;
	unsigned char in_use;
	unsigned char preserved;
	char * name;
};

struct jit_register_info {
	unsigned char preserve;
	unsigned char in_use;
	unsigned char in_register;
	reg_t physical_reg;
};


struct jit_native_reg_info * jit_create_reg_info(int reg, int preserved, char * name);
static inline void jit_touch_reg(struct jit * jit, int id);
static inline void jit_touch_reg2(struct jit * jit, reg_t id1, reg_t id2);
static inline void jit_touch_reg3(struct jit * jit, reg_t id1, reg_t id2, reg_t id3);
static inline reg_t jit_get_reg_ex(struct jit * jit, reg_t id, int ignorecontent);

#if JIT_ARCH == x86
#include "x86-specific.h"
#endif

struct jit_native_reg_info * jit_create_reg_info(int reg, int preserved, char * name)
{
	struct jit_native_reg_info * r = (struct jit_native_reg_info *) JIT_MALLOC(sizeof(struct jit_native_reg_info));
	r->reg_id = reg;
	r->in_use = 0;
	r->preserved = preserved;
	r->virtual_reg = -1;
	r->name = name;
	return r;
}


struct jit * jit_init(size_t buffer_size, unsigned int reg_count)
{
	struct jit * r = JIT_MALLOC(sizeof(struct jit));
	void * mem;

	posix_memalign(&mem, sysconf(_SC_PAGE_SIZE), buffer_size);
	mprotect(mem, buffer_size, PROT_READ | PROT_EXEC | PROT_WRITE);

	r->buf = mem;
	r->ip = mem;
	r->buffer_size = buffer_size;
	r->reg_count = reg_count;

	r->nat_reg_info = jit_init_native_regs();
	r->reg = JIT_MALLOC(sizeof(long) * reg_count);
	r->reg_info = JIT_MALLOC(sizeof(struct jit_register_info) * reg_count);
	memset(r->reg_info, 0x00, sizeof(struct jit_register_info) * reg_count);


	return r;
}

static inline void jit_touch_reg(struct jit * jit, int id)
{
	struct jit_native_reg_info * last = jit->nat_reg_info[id];
	for (int i = id - 1; i >= 0; i--) 
		jit->nat_reg_info[i + 1] = jit->nat_reg_info[i];
	jit->nat_reg_info[0] = last;
}

static inline void jit_touch_reg2(struct jit * jit, reg_t id1, reg_t id2)
{
	while (1) {
		reg_t last = jit->nat_reg_info[JIT_GP_REG_COUNT - 1]->virtual_reg;
		if ((last != id1) && (last != id2)) break;
		jit_touch_reg(jit, JIT_GP_REG_COUNT - 1);
	}
}

static inline void jit_touch_reg3(struct jit * jit, reg_t id1, reg_t id2, reg_t id3)
{
	while (1) {
		reg_t last = jit->nat_reg_info[JIT_GP_REG_COUNT - 1]->virtual_reg;
		if ((last != id1) && (last != id2) && (last != id3)) break;
		jit_touch_reg(jit, JIT_GP_REG_COUNT - 1);
	}
}

// vraci id fyzickeho registru ve kterem je aktualne ulozena hodnota virtualniho registru id
// pokud jeho hodnota neni ve fyzickem registru, presune ji tam
static inline reg_t jit_get_native_reg(struct jit * jit, reg_t id, int ignorecontent)
{
	// checks for unused registers
	for (int i = 0; i < JIT_GP_REG_COUNT; i++) {
		if (!jit->nat_reg_info[i]->in_use) {
			jit->nat_reg_info[i]->in_use = 1;
			jit_touch_reg(jit, i);
			jit->nat_reg_info[0]->virtual_reg = id;
			return jit->nat_reg_info[0]->reg_id;
		}
	}


	/* free least recently used register */

	reg_t freed_virt_reg = jit->nat_reg_info[JIT_GP_REG_COUNT - 1]->virtual_reg;
	jit_lru_reg_to_mem(jit,  jit->nat_reg_info[JIT_GP_REG_COUNT - 1]->reg_id, freed_virt_reg);	
	jit->reg_info[freed_virt_reg].in_register = 0;

	jit_touch_reg(jit, JIT_GP_REG_COUNT - 1);

	/* load content */
	if (!ignorecontent && (jit->reg_info[id].in_use)) {
		jit_lru_mem_to_reg(jit, id); 
		jit->reg_info[id].in_register = 1;
		jit->nat_reg_info[0]->virtual_reg = id;
	}

	return jit->nat_reg_info[0]->reg_id;
}

static inline reg_t jit_get_reg_ex(struct jit * jit, reg_t id, int ignorecontent)
{

	/* forced use of register */
	if (id & (1 << 31)) {
		id &= ((unsigned int)(1 << 31)) - 1;
		/* demanded register is not in use */
		for (int i = 0; i < JIT_GP_REG_COUNT; i++) {
			if ((jit->nat_reg_info[i]->reg_id == id) &&
				(!jit->nat_reg_info[i]->in_use))
				return id;
		}

		/* requested register is in use, but there are still some unused registers
		 * and thus it is not possible assign register using LRU mechanism,
		 * requested register must be freed explicitly
		 */
		int unused = 0;
		for (int i = 0; i < JIT_GP_REG_COUNT; i++) {
			if (!jit->nat_reg_info[i]->in_use) {
				unused = 1;
				break;
			}
		}
		if (unused) {
			for (int i = 0; i < JIT_GP_REG_COUNT; i++) {
				if (jit->nat_reg_info[i]->reg_id == id) {
					reg_t freed_virt_reg = jit->nat_reg_info[i]->virtual_reg;
					jit_lru_reg_to_mem(jit, id, freed_virt_reg);	
					jit->reg_info[freed_virt_reg].in_register = 0;
					jit->nat_reg_info[i]->virtual_reg = 0;
					jit->nat_reg_info[i]->in_use = 0;
					return id;
				}
			}


//			jit_touch_reg(jit, JIT_GP_REG_COUNT - 1);

		}

		while (jit->nat_reg_info[JIT_GP_REG_COUNT - 1]->reg_id != id) {
			fprintf(stderr, "::%s\n", jit->nat_reg_info[JIT_GP_REG_COUNT - 1]->name);
			jit_touch_reg(jit, JIT_GP_REG_COUNT - 1);
		}
		goto xxx;
	}
	struct jit_register_info * reg = &jit->reg_info[id];
		
	if (reg->in_register) {
		for (int i = 0; i < JIT_GP_REG_COUNT; i++) {
			if (jit->nat_reg_info[i]->reg_id == reg->physical_reg) {
				jit_touch_reg(jit, i);
				break;
			}
		}
		return reg->physical_reg;
	}
xxx:
	{}
	reg_t r = jit_get_native_reg(jit, id, ignorecontent);
	jit->reg_info[id].in_use = 1;
	reg->in_register = 1;
	reg->physical_reg = r;

	return reg->physical_reg;
}


/* common */
#define jit_finish(jit,fn) \
	jit_calli(jit,fn); \
	jit_cleanup_regs(jit);

#define jit_finishr(jit,reg) \
	do { \
		reg_t r = jit_get_reg_ex(reg, 0); \
		jit_callr(jit,r); \
		jit_cleanup_regs(jit); \
	} while (0);


void jit_free(struct jit * jit)
{
	free(jit->buf);
	free(jit);
}

void jit_dump(struct jit * jit)
{
	int size = jit->ip - jit->buf;
	printf (".text\n.align 4\n.globl main\n.type main,@function\nmain:\n");
        for (int i = 0; i < size; i++)
		printf(".byte %d\n", (unsigned int) jit->buf[i]);
}
