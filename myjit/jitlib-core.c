/*
 * MyJIT 
 * Copyright (C) 2010 Petr Krajca, <krajcap@inf.upol.cz>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>


// pretty lousy processor detection
// FIXME: the same code is also in jitlib.h
#ifdef __arch32__
#define JIT_ARCH_I386
#else
#define JIT_ARCH_AMD64
#warning "AMD64 processors are supported only partially!"
#endif


#include "jitlib-core.h"
#include "jitlib-debug.c"
#include "flow-analysis.h"

struct jit_op * jit_add_op(struct jit * jit, unsigned short code, unsigned char spec, long arg1, long arg2, long arg3, unsigned char arg_size)
{
	struct jit_op * r = __new_op(code, spec, arg1, arg2, arg3, arg_size);
	jit_op_append(jit->last_op, r);
	jit->last_op = r;
	jit->argpos = 0;
	if (code == JIT_PROLOG) jit->current_func = r;

	return r;
}

struct jit * jit_init(size_t buffer_size, unsigned int reg_count)
{
	void * mem;
	struct jit * r = JIT_MALLOC(sizeof(struct jit));
// FIXME: should be platform specific
#ifdef JIT_ARCH_I386
	reg_count += 2; // two lower registers remain reserved for JIT_FP, JIT_RETREG
#endif
#ifdef JIT_ARCH_AMD64
	reg_count += 3; // three lower registers are reserved for JIT_FP, JIT_RETREG, JIT_IMM
	reg_count += 6; // these registers server are used to store passed arguments
#endif


	posix_memalign(&mem, sysconf(_SC_PAGE_SIZE), buffer_size * 5);
	int x = mprotect(mem, buffer_size, PROT_READ | PROT_EXEC | PROT_WRITE);

	r->ops = __new_op(JIT_CODESTART, SPEC(NO, NO, NO), 0, 0, 0, 0);
	r->last_op = r->ops;
	r->buf = mem;
	r->ip = mem;
	r->buffer_size = buffer_size;
	r->allocai_mem = reg_count * REG_SIZE;
	r->labels = NULL;

	r->reg_count = reg_count;
	r->reg_al = jit_reg_allocator_create(reg_count);

	return r;
}

static inline void __expand_patches_and_labels(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if (GET_OP(op) == JIT_PATCH) {
			((jit_op *)(op->arg[0]))->jmp_addr = op;
		}
		if (GET_OP(op) == JIT_LABEL) {
			((jit_label *)(op->arg[0]))->op = op;
		}

		if ((GET_OP(op) != JIT_LABEL) && (jit_is_label(jit, (void *)op->arg[0]))) {
			op->jmp_addr = ((jit_label *)(op->arg[0]))->op;
		}
	}
}

void jit_generate_code(struct jit * jit)
{
	__expand_patches_and_labels(jit);
	jit_flw_analysis(jit);
	jit_optimize(jit);
	jit_assign_regs(jit);

	for (struct jit_op * op = jit->ops; op != NULL; op = op->next)
		jit_gen_op(jit, op);
}

static void __free_ops(struct jit_op * op)
{
	if (op == NULL) return;
	__free_ops(op->next);

	jitset_free(op->live_in);
	jitset_free(op->live_out);
	JIT_FREE(op->regmap);

	JIT_FREE(op);
}

static void __free_labels(jit_label * lab)
{
	if (lab == NULL) return;
	__free_labels(lab->next);
	JIT_FREE(lab);
}

void jit_free(struct jit * jit)
{
	__free_ops(jit->ops);
	__free_labels(jit->labels);
	jit_reg_allocator_free(jit->reg_al);
	JIT_FREE(jit->buf);
	JIT_FREE(jit);
}

