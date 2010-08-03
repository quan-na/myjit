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

#include "cpu-detect.h"

#include "jitlib-core.h"
#include "jitlib-debug.c"
#include "flow-analysis.h"

#define BUF_SIZE		(4096)
#define MINIMAL_BUF_SPACE	(1024)

struct jit_op * jit_add_op(struct jit * jit, unsigned short code, unsigned char spec, long arg1, long arg2, long arg3, unsigned char arg_size)
{
	struct jit_op * r = __new_op(code, spec, arg1, arg2, arg3, arg_size);
	jit_op_append(jit->last_op, r);
	jit->last_op = r;
	if (code == JIT_PROLOG) jit->current_func = r;

	return r;
}

struct jit_op * jit_add_fop(struct jit * jit, unsigned short code, unsigned char spec, long arg1, long arg2, long arg3, double flt_imm, unsigned char arg_size)
{
	struct jit_op * r = jit_add_op(jit, code, spec, arg1, arg2, arg3, arg_size);
	r->fp = 1;
	r->flt_imm = flt_imm;

	return r;
}

struct jit * jit_init()
{
	struct jit * r = JIT_MALLOC(sizeof(struct jit));

	r->ops = __new_op(JIT_CODESTART, SPEC(NO, NO, NO), 0, 0, 0, 0);
	r->last_op = r->ops;

	r->labels = NULL;
	r->reg_al = jit_reg_allocator_create();
	r->argpos = 0;

	return r;
}

#if JIT_IMM_BITS > 0
/**
 * returns 1 if the immediate value have to be transformed into register
 */
static inline int jit_imm_overflow(struct jit * jit, int signed_op, long value)
{
	unsigned long mask = ~((1UL << JIT_IMM_BITS) - 1);
	unsigned long high_bits = value & mask;

	if (signed_op) {
		if ((high_bits != 0) && (high_bits != mask)) return 1;
	} else {
		if (high_bits != 0) return 1;
	}
	return 0;
}

/**
 * converts long immediates to MOV operation
 */
static inline void jit_correct_long_imms(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if (!IS_IMM(op)) continue;
		if (GET_OP(op) == JIT_JMP) continue;
		if (GET_OP(op) == JIT_CALL) continue;
		if (GET_OP(op) == JIT_PATCH) continue;
		if (GET_OP(op) == JIT_MOV) continue;
		if (GET_OP(op) == JIT_PUTARG) continue;
		if (GET_OP(op) == JIT_MSG) continue;
		if (GET_OP(op) == JIT_PROLOG) continue;
		int imm_arg;
		for (int i = 1; i < 4; i++)
			if (ARG_TYPE(op, i) == IMM) imm_arg = i - 1;
		long value = op->arg[imm_arg];

		if (jit_imm_overflow(jit, IS_SIGNED(op), value)) {
			jit_op * newop = __new_op(JIT_MOV | IMM, SPEC(TREG, IMM, NO), R_IMM, value, 0, REG_SIZE);
			jit_op_prepend(op, newop);

			op->code &= ~(0x3);
			op->code |= REG;

			op->spec &= ~(0x3 << (2 * imm_arg));
			op->spec |=  (REG << (2 * imm_arg));
			op->arg[imm_arg] = R_IMM;
		}
	}
}
#endif

static inline void jit_correct_float_imms(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if (!IS_IMM(op)) continue;
		if (!op->fp) continue;
		if (GET_OP(op) == JIT_FMOV) continue;
		if (GET_OP(op) == JIT_FPUTARG) continue;
		if (GET_OP(op) == JIT_FLD) continue;
		if (GET_OP(op) == JIT_FLDX) continue;
		if (GET_OP(op) == JIT_FST) continue;
		if (GET_OP(op) == JIT_FSTX) continue;
// FIXME: TODO		if (GET_OP(op) == JIT_FMSG) continue;
		int imm_arg;
		for (int i = 1; i < 4; i++)
			if (ARG_TYPE(op, i) == IMM) imm_arg = i - 1;

		jit_op * newop = __new_op(JIT_FMOV | IMM, SPEC(TREG, IMM, NO), FR_IMM, 0, 0, 0);
		newop->fp = 1;
		newop->flt_imm = op->flt_imm;
		jit_op_prepend(op, newop);

		op->code &= ~(0x3);
		op->code |= REG;

		op->spec &= ~(0x3 << (2 * imm_arg));
		op->spec |=  (REG << (2 * imm_arg));
		op->arg[imm_arg] = FR_IMM;
	}
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

static inline void __buf_expand(struct jit * jit)
{
	long pos = jit->ip - jit->buf;
	jit->buf_capacity *= 2;
	jit->buf = JIT_REALLOC(jit->buf, jit->buf_capacity);
	jit->ip = jit->buf + pos;
}

void jit_generate_code(struct jit * jit)
{
	__expand_patches_and_labels(jit);
#if JIT_IMM_BITS > 0
	jit_correct_long_imms(jit);
#endif
	jit_correct_float_imms(jit);
	jit_flw_analysis(jit);

#if defined(JIT_ARCH_I386) || defined(JIT_ARCH_AMD64)
	jit_optimize_st_ops(jit);
#endif
	jit_assign_regs(jit);

	jit->buf_capacity = BUF_SIZE;
	jit->buf = JIT_MALLOC(jit->buf_capacity);
	jit->ip = jit->buf;

	for (struct jit_op * op = jit->ops; op != NULL; op = op->next) {
		if (jit->buf_capacity - (jit->ip - jit->buf) < MINIMAL_BUF_SPACE) __buf_expand(jit);
		jit_gen_op(jit, op);
	}

	/* moves the code to its final destination */
	int code_size = jit->ip - jit->buf;  
	void * mem;
	posix_memalign(&mem, sysconf(_SC_PAGE_SIZE), code_size);
	mprotect(mem, code_size, PROT_READ | PROT_EXEC | PROT_WRITE);
	memcpy(mem, jit->buf, code_size);
	JIT_FREE(jit->buf);

	long pos = jit->ip - jit->buf;
	jit->buf = mem;
	jit->ip = jit->buf + pos;

	jit_patch_external_calls(jit);

	/* assigns functions */
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if (GET_OP(op) == JIT_PROLOG)
			*(void **)(op->arg[0]) = jit->buf + (long)op->patch_addr;
	}
}

static void __free_ops(struct jit_op * op)
{
	if (op == NULL) return;
	__free_ops(op->next);

	if (op->live_in) jitset_free(op->live_in);
	if (op->live_out) jitset_free(op->live_out);
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
	jit_reg_allocator_free(jit->reg_al);
	__free_ops(jit->ops);
	__free_labels(jit->labels);
	JIT_FREE(jit->buf);
	JIT_FREE(jit);
}
