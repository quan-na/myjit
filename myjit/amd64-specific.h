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

#include "amd64-codegen.h"

/* Stack frame organization:
 *
 * RBP      +--------------+
 *          | allocai mem  |
 * RBP - n  +--------------+
 *          | GP registers |
 * RPB - m  +--------------+
 *          | FP registers |
 * RPB - k  +--------------+
 *          | shadow space |
 *          | for arg.regs |
 * RPB - l  +--------------+
 */


#define __JIT_GET_ADDR(jit, imm) (!jit_is_label(jit, (void *)(imm)) ? (imm) :  \
		(((long)jit->buf + ((jit_label *)(imm))->pos - (long)jit->ip)))
		//(((long)jit->buf + ((jit_label *)(imm))->pos - (long)jit->ip)))
//#define __GET_REG_POS(jit, r) ((- (__REG(r)) * REG_SIZE) - jit_current_func_info(jit)->allocai_mem)

#define __GET_GPREG_POS(jit, r) (- ((JIT_REG(r).id + 1) * REG_SIZE) - jit_current_func_info(jit)->allocai_mem)
#define __GET_FPREG_POS(jit, r) (- jit_current_func_info(jit)->gp_reg_count * REG_SIZE - (JIT_REG(r).id + 1) * sizeof(jit_float) - jit_current_func_info(jit)->allocai_mem)

//#define __GET_ARG_SPILL_POS(jit, info, arg) ((- (JIT_REG(arg).id + info->gp_reg_count + info->fp_reg_count) * REG_SIZE) - jit_current_func_info(jit)->allocai_mem)
#define __GET_ARG_SPILL_POS(jit, info, arg) ((- (arg + info->gp_reg_count + info->fp_reg_count) * REG_SIZE) - jit_current_func_info(jit)->allocai_mem)

//#define __GET_REG_POS(jit,r) __GET_GPREG_POS(jit,r)
static inline int __GET_REG_POS(struct jit * jit, int r)
{
	if (JIT_REG(r).spec == JIT_RTYPE_REG) {
		if (JIT_REG(r). type == JIT_RTYPE_INT) return __GET_GPREG_POS(jit, r);
		else return __GET_FPREG_POS(jit, r);
	} else return __GET_ARG_SPILL_POS(jit, jit_current_func_info(jit), JIT_REG(r).id);
}

#define __PATCH_ADDR(jit)       ((long)jit->ip - (long)jit->buf)

#include "x86-common-stuff.c"

static inline int jit_allocai(struct jit * jit, int size)
{
	int real_size = (size + 15) & 0xfffffff0; // 16-bytes aligned
	jit_add_op(jit, JIT_ALLOCA | IMM, SPEC(IMM, NO, NO), (long)real_size, 0, 0, 0);
	jit_current_func_info(jit)->allocai_mem += real_size;
	return -(jit_current_func_info(jit)->allocai_mem);
}

static inline void __push_reg(struct jit * jit, struct __hw_reg * r)
{
	if (!r->fp) amd64_push_reg(jit->ip, r->id);
	else {
		amd64_alu_reg_imm(jit->ip, X86_SUB, AMD64_RSP, 8);
		amd64_sse_movlpd_membase_xreg(jit->ip, r->id, AMD64_RSP, 0);
	}
}

static inline void __pop_reg(struct jit * jit, struct __hw_reg * r)
{
	if (!r->fp) amd64_pop_reg(jit->ip, r->id);
	else {
		amd64_sse_movlpd_xreg_membase(jit->ip, r->id, AMD64_RSP, 0);
		amd64_alu_reg_imm(jit->ip, X86_ADD, AMD64_RSP, 8);
	}
}

void jit_init_arg_params(struct jit * jit, int p)
{
	struct jit_func_info * info = jit_current_func_info(jit);
	struct jit_inp_arg * a = &(info->args[p]);
	if (a->type != JIT_FLOAT_NUM) { // normal argument
		int pos = a->gp_pos;
		if (pos < jit->reg_al->gp_arg_reg_cnt) {
			a->passed_by_reg = 1;
			a->location.reg = jit->reg_al->gp_arg_regs[pos];
			a->spill_pos = __GET_ARG_SPILL_POS(jit, info, p);
		} else {
			int stack_pos = (pos - jit->reg_al->gp_arg_reg_cnt) + MAX(0, (a->fp_pos - jit->reg_al->fp_arg_reg_cnt));

			a->location.stack_pos = 16 + stack_pos * 8;
			a->spill_pos = 16 + stack_pos * 8;
			a->passed_by_reg = 0;
		}
		return;
	}

	// FP argument
	int pos = a->fp_pos;
	if (pos < jit->reg_al->fp_arg_reg_cnt) {
		a->passed_by_reg = 1;
		a->location.reg = jit->reg_al->fp_arg_regs[pos];
		a->spill_pos = __GET_ARG_SPILL_POS(jit, info, p);
	} else {

		int stack_pos = (pos - jit->reg_al->fp_arg_reg_cnt) + MAX(0, (a->gp_pos - jit->reg_al->gp_arg_reg_cnt));

		a->location.stack_pos = 16 + stack_pos * 8;
		a->spill_pos = 16 + stack_pos * 8;
		a->passed_by_reg = 0;
	}
}

static inline void __alu_op(struct jit * jit, struct jit_op * op, int amd64_op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) amd64_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		amd64_alu_reg_imm(jit->ip, amd64_op, op->r_arg[0], op->r_arg[2]);

	}  else {
		if (op->r_arg[0] == op->r_arg[1]) {
			amd64_alu_reg_reg(jit->ip, amd64_op, op->r_arg[0], op->r_arg[2]);
		} else if (op->r_arg[0] == op->r_arg[2]) {
			amd64_alu_reg_reg(jit->ip, amd64_op, op->r_arg[0], op->r_arg[1]);
		} else {
			amd64_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
			amd64_alu_reg_reg(jit->ip, amd64_op, op->r_arg[0], op->r_arg[2]);
		}	
	}
}

static inline void __sub_op(struct jit * jit, struct jit_op * op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) amd64_lea_membase(jit->ip, op->r_arg[0], op->r_arg[1], -op->r_arg[2]);
		else amd64_alu_reg_imm(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
		return;

	}
	if (op->r_arg[0] == op->r_arg[1]) {
		amd64_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
	} else if (op->r_arg[0] == op->r_arg[2]) {
		amd64_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
		amd64_neg_reg(jit->ip, op->r_arg[0]);
	} else {
		amd64_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		amd64_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
	}	
}

static inline void __subx_op(struct jit * jit, struct jit_op * op, int amd64_op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) amd64_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		amd64_alu_reg_imm(jit->ip, amd64_op, op->r_arg[0], op->r_arg[2]);
		return;

	}
	if (op->r_arg[0] == op->r_arg[1]) {
		amd64_alu_reg_reg(jit->ip, amd64_op, op->r_arg[0], op->r_arg[2]);
	} else if (op->r_arg[0] == op->r_arg[2]) {
		amd64_push_reg(jit->ip, op->r_arg[2]);
		amd64_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		amd64_alu_reg_membase(jit->ip, amd64_op, op->r_arg[0], AMD64_RSP, 0);
		amd64_alu_reg_imm(jit->ip, X86_ADD, AMD64_RSP, 8);
	} else {
		amd64_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		amd64_alu_reg_reg(jit->ip, amd64_op, op->r_arg[0], op->r_arg[2]);
	}	
}

static inline void __rsb_op(struct jit * jit, struct jit_op * op, int imm)
{
	if (imm) {
		if (op->r_arg[0] == op->r_arg[1]) amd64_alu_reg_imm(jit->ip, X86_ADD, op->r_arg[0], -op->r_arg[2]);
		else amd64_lea_membase(jit->ip, op->r_arg[0], op->r_arg[1], -op->r_arg[2]);
		amd64_neg_reg(jit->ip, op->r_arg[0]);
		return;
	}

	if (op->r_arg[0] == op->r_arg[1]) { // O1 = O3 - O1
		amd64_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
		amd64_neg_reg(jit->ip, op->r_arg[0]);
	} else if (op->r_arg[0] == op->r_arg[2]) { // O1 = O1 - O2
		amd64_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
	} else {
		amd64_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[2], REG_SIZE);
		amd64_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
	}
}

static inline void __shift_op(struct jit * jit, struct jit_op * op, int shift_op, int imm)
{
	if (imm) { 
		if (op->r_arg[0] != op->r_arg[1]) amd64_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		amd64_shift_reg_imm(jit->ip, shift_op, op->r_arg[0], op->r_arg[2]);
	} else {
		int destreg = op->r_arg[0];
		int valreg = op->r_arg[1];
		int shiftreg = op->r_arg[2];
		int ecx_pathology = 0; // shifting content in the ECX register

		int rcx_in_use = jit_reg_in_use(op, AMD64_RCX, 0);
		int rdx_in_use = jit_reg_in_use(op, AMD64_RDX, 0);

		if (destreg == AMD64_RCX) {
			ecx_pathology = 1;
			if (rdx_in_use) amd64_push_reg(jit->ip, AMD64_RDX); 
			destreg = AMD64_RDI;
		}

		if (shiftreg != AMD64_RCX) {
			if (rcx_in_use) amd64_push_reg(jit->ip, AMD64_RCX); 
			amd64_mov_reg_reg(jit->ip, AMD64_RCX, shiftreg, REG_SIZE);
		}
		if (destreg != valreg) amd64_mov_reg_reg(jit->ip, destreg, valreg, REG_SIZE); 
		amd64_shift_reg(jit->ip, shift_op, destreg);
		if (ecx_pathology) {
			amd64_mov_reg_reg(jit->ip, AMD64_RCX, AMD64_RDX, REG_SIZE);
			if ((shiftreg != AMD64_RCX) && rcx_in_use) amd64_alu_reg_imm(jit->ip, X86_ADD, AMD64_RSP, 8);
			if (rdx_in_use) amd64_pop_reg(jit->ip, AMD64_RDX);
		} else {
			if ((shiftreg != AMD64_RCX) && rcx_in_use) amd64_pop_reg(jit->ip, AMD64_RCX);
		}
	}
}

static inline void __cond_op(struct jit * jit, struct jit_op * op, int amd64_cond, int imm, int sign)
{
	if (imm) amd64_alu_reg_imm(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);
	else amd64_alu_reg_reg(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);
	if ((op->r_arg[0] != AMD64_RSI) && (op->r_arg[0] != AMD64_RDI)) {
		amd64_mov_reg_imm(jit->ip, op->r_arg[0], 0);
		amd64_set_reg(jit->ip, amd64_cond, op->r_arg[0], sign);
	} else {
		amd64_xchg_reg_reg(jit->ip, AMD64_RAX, op->r_arg[0], REG_SIZE);
		amd64_mov_reg_imm(jit->ip, AMD64_RAX, 0);
		amd64_set_reg(jit->ip, amd64_cond, AMD64_RAX, sign);
		amd64_xchg_reg_reg(jit->ip, AMD64_RAX, op->r_arg[0], REG_SIZE);
	}
}

static inline void __branch_op(struct jit * jit, struct jit_op * op, int amd64_cond, int imm, int sign)
{
	if (imm) amd64_alu_reg_imm(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);
	else amd64_alu_reg_reg(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	amd64_branch_disp(jit->ip, amd64_cond, __JIT_GET_ADDR(jit, op->r_arg[0]), sign);
}

static inline void __branch_mask_op(struct jit * jit, struct jit_op * op, int amd64_cond, int imm)
{
	if (imm) amd64_test_reg_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
	else amd64_test_reg_reg(jit->ip, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	amd64_branch_disp(jit->ip, amd64_cond, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}

static inline void __branch_overflow_op(struct jit * jit, struct jit_op * op, int alu_op, int imm)
{
	if (imm) amd64_alu_reg_imm(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);
	else amd64_alu_reg_reg(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	amd64_branch_disp(jit->ip, X86_CC_O, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}

/* determines whether the argument value was spilled out or not,
 * if the register is associated with the hardware register
 * it is returned through the reg argument
 */
static inline int __is_spilled(int arg_id, jit_op * prepare_op, int * reg)
{
	int args = prepare_op->arg[0];
	int fp_args = prepare_op->arg[1];
	struct __hw_reg * hreg = rmap_get(prepare_op->regmap, arg_id);

	if (hreg) {
		*reg = hreg->id;
		return 0;
	} else return 1;
}

/**
 * Assigns integer value to register which is used to pass the argument
 */
static inline void __set_arg(struct jit * jit, struct jit_out_arg * arg)
{
	int sreg;
	int reg = jit->reg_al->gp_arg_regs[arg->argpos];
	long value = arg->value.generic;
	if (arg->isreg) {
		if (__is_spilled(value, jit->prepared_args.op, &sreg)) {
			amd64_mov_reg_membase(jit->ip, reg, AMD64_RBP, __GET_REG_POS(jit, value), REG_SIZE);
		} else {
			if (reg != sreg) amd64_mov_reg_reg(jit->ip, reg, sreg, REG_SIZE);
		}
	} else amd64_mov_reg_imm_size(jit->ip, reg, value, 8);
}

/**
 * Assigns FP value to register which is used to pass the argument
 */
static inline void __set_fparg(struct jit * jit, struct jit_out_arg * arg)
{
	int sreg;
	int reg = jit->reg_al->fp_arg_regs[arg->argpos];
	long value = arg->value.generic;
	if (arg->isreg) {
		if (__is_spilled(value, jit->prepared_args.op, &sreg)) {
			int pos = __GET_REG_POS(jit, value);
			if (arg->size == sizeof(float)) 
				amd64_sse_cvtsd2ss_reg_membase(jit->ip, reg, AMD64_RBP, pos);
			else amd64_sse_movlpd_xreg_membase(jit->ip, reg, AMD64_RBP, pos);
		} else {
			if (arg->size == sizeof(float))
				amd64_sse_cvtsd2ss_reg_reg(jit->ip, reg, sreg);
			else if (reg != sreg) amd64_sse_movsd_reg_reg(jit->ip, reg, sreg);
		}
	} else { // immediate value
		if (arg->size == sizeof(float)) {
			float val = (float)arg->value.fp;
			amd64_mov_reg_imm_size(jit->ip, AMD64_RAX, *(unsigned int *)&val, 4);
			amd64_movd_xreg_reg_size(jit->ip, reg, AMD64_RAX, 4);
		} else {
			amd64_mov_reg_imm_size(jit->ip, AMD64_RAX, value, 8);
			amd64_movd_xreg_reg_size(jit->ip, reg, AMD64_RAX, 8);
		}
	}
}

/**
 * Pushes integer value on stack
 */
static inline void __push_arg(struct jit * jit, struct jit_out_arg * arg)
{
	int sreg;
	if (arg->isreg) {
		if (__is_spilled(arg->value.generic, jit->prepared_args.op, &sreg))
			amd64_push_membase(jit->ip, AMD64_RBP, __GET_REG_POS(jit, arg->value.generic));
		else amd64_push_reg(jit->ip, sreg);
	} else {
		amd64_mov_reg_reg(jit->ip, AMD64_RAX, arg->value.generic, REG_SIZE);
		amd64_push_reg(jit->ip, AMD64_RAX);
	}
}

static inline void __configure_args(struct jit * jit)
{
	int sreg;
	struct jit_out_arg * args = jit->prepared_args.args;

	for (int x = jit->prepared_args.count - 1; x >= 0; x --) {
		struct jit_out_arg * arg = &(args[x]);
		if (!arg->isfp) {
			if (arg->argpos < jit->reg_al->gp_arg_reg_cnt) __set_arg(jit, arg);
			else __push_arg(jit, arg);
		} else {
			if (arg->argpos < jit->reg_al->fp_arg_reg_cnt) __set_fparg(jit, arg);
			// FIXME:
			else assert(0);
		}
	}
	/* AL is used to pass the number of floating point arguments passed through the XMM0-XMM7 registers */
	amd64_mov_reg_imm(jit->ip, AMD64_RAX, MIN(jit->prepared_args.fp_args, jit->reg_al->fp_arg_reg_cnt)); 
}

static inline void __funcall(struct jit * jit, struct jit_op * op, int imm)
{
	__configure_args(jit);

	if (!imm) {
		amd64_call_reg(jit->ip, op->r_arg[0]);
	} else {
		op->patch_addr = __PATCH_ADDR(jit);
		amd64_call_imm(jit->ip, __JIT_GET_ADDR(jit, op->r_arg[0]) - 4); // 4: magic constant
	}

	if (jit->prepared_args.stack_size)
		amd64_alu_reg_imm(jit->ip, X86_ADD, AMD64_RSP, jit->prepared_args.stack_size);
	JIT_FREE(jit->prepared_args.args);

	__pop_caller_saved_regs(jit, op);
}

static inline void __mul(struct jit * jit, struct jit_op * op, int imm, int sign, int high_bytes)
{
	long dest = op->r_arg[0];
	long factor1 = op->r_arg[1];
	long factor2 = op->r_arg[2];

	/* optimization for special cases */
	if ((!high_bytes) && (imm)) {
		switch (factor2) {
			case 2: if (factor1 == dest) amd64_shift_reg_imm(jit->ip, X86_SHL, dest, 1);
				else amd64_lea_memindex(jit->ip, dest, factor1, 0, factor1, 0);
				return;

			case 3: amd64_lea_memindex(jit->ip, dest, factor1, 0, factor1, 1); return;

			case 4: if (factor1 != dest) amd64_mov_reg_reg(jit->ip, dest, factor1, REG_SIZE);
				amd64_shift_reg_imm(jit->ip, X86_SHL, dest, 2);
				return;
			case 5: amd64_lea_memindex(jit->ip, dest, factor1, 0, factor1, 2);
				return;
			case 8: if (factor1 != dest) amd64_mov_reg_reg(jit->ip, dest, factor1, REG_SIZE);
				amd64_shift_reg_imm(jit->ip, X86_SHL, dest, 3);
				return;
			case 9: amd64_lea_memindex(jit->ip, dest, factor1, 0, factor1, 3);
				return;
		}
	}


	/* generic multiplication */
	int rax_in_use = jit_reg_in_use(op, AMD64_RAX, 0);
	int rdx_in_use = jit_reg_in_use(op, AMD64_RDX, 0);

	if ((dest != AMD64_RAX) && rax_in_use) amd64_push_reg(jit->ip, AMD64_RAX);
	if ((dest != AMD64_RDX) && rdx_in_use) amd64_push_reg(jit->ip, AMD64_RDX);

	if (imm) {
		if (factor1 != AMD64_RAX) amd64_mov_reg_reg(jit->ip, AMD64_RAX, factor1, REG_SIZE);
		amd64_mov_reg_imm(jit->ip, AMD64_RDX, factor2);
		amd64_mul_reg(jit->ip, AMD64_RDX, sign);
	} else {
		if (factor1 == AMD64_RAX) amd64_mul_reg(jit->ip, factor2, sign);
		else if (factor2 == AMD64_RAX) amd64_mul_reg(jit->ip, factor1, sign);
		else {
			amd64_mov_reg_reg(jit->ip, AMD64_RAX, factor1, REG_SIZE);
			amd64_mul_reg(jit->ip, factor2, sign);
		}
	}

	if (!high_bytes) {
		if (dest != AMD64_RAX) amd64_mov_reg_reg(jit->ip, dest, AMD64_RAX, REG_SIZE);
	} else {
		if (dest != AMD64_RDX) amd64_mov_reg_reg(jit->ip, dest, AMD64_RDX, REG_SIZE);
	}

	if ((dest != AMD64_RDX) && rdx_in_use) amd64_pop_reg(jit->ip, AMD64_RDX);
	if ((dest != AMD64_RAX) && rax_in_use) amd64_pop_reg(jit->ip, AMD64_RAX);
}
static inline void __div(struct jit * jit, struct jit_op * op, int imm, int sign, int modulo)
{
	long dest = op->r_arg[0];
	long dividend = op->r_arg[1];
	long divisor = op->r_arg[2];

	if (imm && ((divisor == 2) || (divisor == 4) || (divisor == 8))) {
		if (dest != dividend) amd64_mov_reg_reg(jit->ip, dest, dividend, REG_SIZE);
		if (!modulo) {
			switch (divisor) {
				case 2: amd64_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 1); break;
				case 4: amd64_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 2); break;
				case 8: amd64_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 3); break;
			}
		} else {
			switch (divisor) {
				case 2: amd64_alu_reg_imm(jit->ip, X86_AND, dest, 0x1); break;
				case 4: amd64_alu_reg_imm(jit->ip, X86_AND, dest, 0x3); break;
				case 8: amd64_alu_reg_imm(jit->ip, X86_AND, dest, 0x7); break;
			}
		}
		return;
	}

	int rax_in_use = jit_reg_in_use(op, AMD64_RAX, 0);
	int rdx_in_use = jit_reg_in_use(op, AMD64_RDX, 0);

	if ((dest != AMD64_RAX) && rax_in_use) amd64_push_reg(jit->ip, AMD64_RAX);
	if ((dest != AMD64_RDX) && rdx_in_use) amd64_push_reg(jit->ip, AMD64_RDX);

	if (imm) {
		if (dividend != AMD64_RAX) amd64_mov_reg_reg(jit->ip, AMD64_RAX, dividend, REG_SIZE);
		amd64_cdq(jit->ip);
		if (dest != AMD64_RBX) amd64_push_reg(jit->ip, AMD64_RBX);
		amd64_mov_reg_imm_size(jit->ip, AMD64_RBX, divisor, 8);
		amd64_div_reg(jit->ip, AMD64_RBX, sign);
		if (dest != AMD64_RBX) amd64_pop_reg(jit->ip, AMD64_RBX);
	} else {
		if ((divisor == AMD64_RAX) || (divisor == AMD64_RDX)) {
			amd64_push_reg(jit->ip, divisor);
		}

		if (dividend != AMD64_RAX) amd64_mov_reg_reg(jit->ip, AMD64_RAX, dividend, REG_SIZE);

		amd64_cdq(jit->ip);

		if ((divisor == AMD64_RAX) || (divisor == AMD64_RDX)) {
			amd64_div_membase(jit->ip, AMD64_RSP, 0, sign);
			amd64_alu_reg_imm(jit->ip, X86_ADD, AMD64_RSP, 8);
		} else {
			amd64_div_reg(jit->ip, divisor, sign);
		}
	}

	if (!modulo) {
		if (dest != AMD64_RAX) amd64_mov_reg_reg(jit->ip, dest, AMD64_RAX, REG_SIZE);
	} else {
		if (dest != AMD64_RDX) amd64_mov_reg_reg(jit->ip, dest, AMD64_RDX, REG_SIZE);
	}

	if ((dest != AMD64_RDX) && rdx_in_use) amd64_pop_reg(jit->ip, AMD64_RDX);
	if ((dest != AMD64_RAX) && rax_in_use) amd64_pop_reg(jit->ip, AMD64_RAX);
}

/////// SSE -specific

static inline unsigned char * __sse_get_sign_mask();
static inline void __sse_change_sign(struct jit * jit, long reg)
{
	amd64_sse_xorpd_reg_mem(jit->ip, reg, __sse_get_sign_mask());
}

static inline void __sse_round(struct jit * jit, long a1, long a2)
{
	static const double x0 = 0.0;
	static const double x05 = 0.5;

	// creates a copy of the a2 and tmp_reg into high bits of a2 and tmp_reg
	//x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);
	amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);

	amd64_sse_alu_pd_reg_mem(jit->ip, X86_SSE_COMI, a2, &x0);
	

	unsigned char * branch1 = jit->ip;
	amd64_branch_disp(jit->ip, X86_CC_LT, 0, 0);

	amd64_sse_alu_sd_reg_mem(jit->ip, X86_SSE_ADD, a2, &x05);

	unsigned char * branch2 = jit->ip;
	amd64_jump_disp(jit->ip, 0);

	amd64_patch(branch1, jit->ip);

	amd64_sse_alu_sd_reg_mem(jit->ip, X86_SSE_SUB, a2, &x05);
	amd64_patch(branch2, jit->ip);

	amd64_sse_cvttsd2si_reg_reg(jit->ip, a1, a2);

	// returns values back
	amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
}

static inline void __sse_floor(struct jit * jit, long a1, long a2, int floor)
{
	int tmp_reg = (a2 == X86_XMM7 ? X86_XMM0 : X86_XMM7);

	// creates a copy of the a2 and tmp_reg into high bits of a2 and tmp_reg
	amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);
	// TODO: test if the register is in use or not
	amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, tmp_reg, tmp_reg, 0);

	// truncates the value in a2 and stores it into the a1 and tmp_reg
	amd64_sse_cvttsd2si_reg_reg(jit->ip, a1, a2);
	amd64_sse_cvtsi2sd_reg_reg(jit->ip, tmp_reg, a1);

	if (floor) {
		// if a2 < tmp_reg, it substracts 1 (using the carry flag)
		amd64_sse_comisd_reg_reg(jit->ip, a2, tmp_reg);
		amd64_alu_reg_imm(jit->ip, X86_SBB, a1, 0);
	} else { // ceil
		// if tmp_reg < a2, it adds 1 (using the carry flag)
		amd64_sse_comisd_reg_reg(jit->ip, tmp_reg, a2);
		amd64_alu_reg_imm(jit->ip, X86_ADC, a1, 0);
	}

	// returns values back
	amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
	amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, tmp_reg, tmp_reg, 1);
}

void __get_arg(struct jit * jit, jit_op * op)
{
	struct jit_func_info * info = jit_current_func_info(jit);

	int dreg = op->r_arg[0];
	int arg_id = op->r_arg[1];
	//int reg_id = arg_id + info->gp_reg_count + info->fp_reg_count;
	int reg_id = __mkreg(JIT_RTYPE_INT, JIT_RTYPE_ARG, arg_id);

	struct jit_inp_arg * arg = &(info->args[arg_id]);
	int read_from_stack = 0;
	int stack_pos;

	if (!arg->passed_by_reg) {
		read_from_stack = 1;
		stack_pos = arg->location.stack_pos;
	}
	
	if (arg->passed_by_reg && rmap_get(op->regmap, reg_id) == NULL) {
		// the register is not associated and the value has to be read from the memory
		read_from_stack = 1;
		stack_pos = arg->spill_pos;
	}

	if (read_from_stack) {
		if (arg->type != JIT_FLOAT_NUM) {
			if (arg->size == REG_SIZE) amd64_mov_reg_membase(jit->ip, dreg, AMD64_RBP, stack_pos, REG_SIZE);
			else if (arg->type == JIT_SIGNED_NUM) {
				amd64_movsx_reg_membase(jit->ip, dreg, AMD64_RBP, stack_pos, arg->size);
			} else {
				amd64_alu_reg_reg(jit->ip, X86_XOR, dreg, dreg); // register cleanup
				amd64_mov_reg_membase(jit->ip, dreg, AMD64_RBP, stack_pos, arg->size);
			}
		} else {
			if (arg->passed_by_reg) {
				if (arg->size == sizeof(float))
					amd64_sse_cvtss2sd_reg_reg(jit->ip, dreg, arg->location.reg);
				else amd64_sse_movsd_reg_reg(jit->ip, dreg, arg->location.reg); 
			} else {
				if (arg->size == sizeof(float))
					amd64_sse_cvtss2sd_reg_membase(jit->ip, dreg, AMD64_RBP, stack_pos);
				else amd64_sse_movlpd_xreg_membase(jit->ip, dreg, AMD64_RBP, stack_pos);
			}
		}
		return;
	} 

	int reg = arg->location.reg;
	if (arg->type != JIT_FLOAT_NUM) {
		if (arg->size == REG_SIZE) amd64_mov_reg_reg(jit->ip, dreg, reg, REG_SIZE);
		else if (arg->type == JIT_SIGNED_NUM) amd64_movsx_reg_reg(jit->ip, dreg, reg, arg->size);
		else amd64_movzx_reg_reg(jit->ip, dreg, reg, arg->size);
	} else {
		if (arg->size == sizeof(float)) amd64_sse_cvtss2sd_reg_reg(jit->ip, dreg, reg);
		else amd64_sse_movsd_reg_reg(jit->ip, dreg, reg);
	}
}

static inline void __ureg(struct jit * jit, long vreg, long hreg_id)
{
	if (JIT_REG(vreg).spec == JIT_RTYPE_ARG) {

		struct jit_func_info * info = jit_current_func_info(jit);
		int pos =  __GET_ARG_SPILL_POS(jit, info, JIT_REG(vreg).id);

		if (JIT_REG(vreg).type == JIT_RTYPE_FLOAT) amd64_sse_movlpd_membase_xreg(jit->ip, hreg_id, AMD64_RBP, pos);
		else amd64_mov_membase_reg(jit->ip, AMD64_RBP, pos, hreg_id, REG_SIZE);
	} else {
		if (JIT_REG(vreg).type == JIT_RTYPE_FLOAT) amd64_sse_movlpd_membase_xreg(jit->ip, hreg_id, AMD64_RBP, __GET_FPREG_POS(jit, vreg));
		else amd64_mov_membase_reg(jit->ip, AMD64_RBP, __GET_REG_POS(jit, vreg), hreg_id, REG_SIZE);
	}
}

void jit_patch_external_calls(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if ((op->code == (JIT_CALL | IMM)) && (!jit_is_label(jit, (void *)op->arg[0])))
			amd64_patch(jit->buf + (long)op->patch_addr, (unsigned char *)op->arg[0]);
		if (GET_OP(op) == JIT_MSG)
			amd64_patch(jit->buf + (long)op->patch_addr, (unsigned char *)printf);
	}
}

void jit_gen_op(struct jit * jit, struct jit_op * op)
{
	long a1 = op->r_arg[0];
	long a2 = op->r_arg[1];
	long a3 = op->r_arg[2];

	int found = 1;
	
	switch (GET_OP(op)) {
		case JIT_ADD: 
			if ((a1 != a2) && (a1 != a3)) {
				if (IS_IMM(op)) amd64_lea_membase(jit->ip, a1, a2, a3);
				else amd64_lea_memindex(jit->ip, a1, a2, 0, a3, 0);
			} else __alu_op(jit, op, X86_ADD, IS_IMM(op)); 
			break;

		case JIT_ADDC: __alu_op(jit, op, X86_ADD, IS_IMM(op)); break;
		case JIT_ADDX: __alu_op(jit, op, X86_ADC, IS_IMM(op)); break;
		case JIT_SUB: __sub_op(jit, op, IS_IMM(op)); break;
		case JIT_SUBC: __subx_op(jit, op, X86_SUB, IS_IMM(op)); break; 
		case JIT_SUBX: __subx_op(jit, op, X86_SBB, IS_IMM(op)); break; 
		case JIT_RSB: __rsb_op(jit, op, IS_IMM(op)); break; 
		case JIT_NEG: if (a1 != a2) amd64_mov_reg_reg(jit->ip, a1, a2, REG_SIZE);
			      amd64_neg_reg(jit->ip, a1); 
			      break;
		case JIT_OR: __alu_op(jit, op, X86_OR, IS_IMM(op)); break; 
		case JIT_XOR: __alu_op(jit, op, X86_XOR, IS_IMM(op)); break; 
		case JIT_AND: __alu_op(jit, op, X86_AND, IS_IMM(op)); break; 
		case JIT_NOT: if (a1 != a2) amd64_mov_reg_reg(jit->ip, a1, a2, REG_SIZE);
			      amd64_not_reg(jit->ip, a1);
			      break;
		case JIT_LSH: __shift_op(jit, op, X86_SHL, IS_IMM(op)); break;
		case JIT_RSH: __shift_op(jit, op, IS_SIGNED(op) ? X86_SAR : X86_SHR, IS_IMM(op)); break;

		case JIT_LT: __cond_op(jit, op, X86_CC_LT, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_LE: __cond_op(jit, op, X86_CC_LE, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_GT: __cond_op(jit, op, X86_CC_GT, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_GE: __cond_op(jit, op, X86_CC_GE, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_EQ: __cond_op(jit, op, X86_CC_EQ, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_NE: __cond_op(jit, op, X86_CC_NE, IS_IMM(op), IS_SIGNED(op)); break;

		case JIT_BLT: __branch_op(jit, op, X86_CC_LT, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_BLE: __branch_op(jit, op, X86_CC_LE, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_BGT: __branch_op(jit, op, X86_CC_GT, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_BGE: __branch_op(jit, op, X86_CC_GE, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_BEQ: __branch_op(jit, op, X86_CC_EQ, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_BNE: __branch_op(jit, op, X86_CC_NE, IS_IMM(op), IS_SIGNED(op)); break;

		case JIT_BMS: __branch_mask_op(jit, op, X86_CC_NZ, IS_IMM(op)); break;
		case JIT_BMC: __branch_mask_op(jit, op, X86_CC_Z, IS_IMM(op)); break;

		case JIT_BOADD: __branch_overflow_op(jit, op, X86_ADD, IS_IMM(op)); break;
		case JIT_BOSUB: __branch_overflow_op(jit, op, X86_SUB, IS_IMM(op)); break;

		case JIT_MUL: __mul(jit, op, IS_IMM(op), IS_SIGNED(op), 0); break;
		case JIT_HMUL: __mul(jit, op, IS_IMM(op), IS_SIGNED(op), 1); break;
		case JIT_DIV: __div(jit, op, IS_IMM(op), IS_SIGNED(op), 0); break;
		case JIT_MOD: __div(jit, op, IS_IMM(op), IS_SIGNED(op), 1); break;

		case JIT_CALL: __funcall(jit, op, IS_IMM(op)); break;
		case JIT_PATCH: do {
					long pa = ((struct jit_op *)a1)->patch_addr;
					amd64_patch(jit->buf + pa, jit->ip);
				} while (0);
				break;
		case JIT_JMP:
			op->patch_addr = __PATCH_ADDR(jit);
			if (op->code & REG) amd64_jump_reg(jit->ip, a1);
			else amd64_jump_disp(jit->ip, __JIT_GET_ADDR(jit, a1));
			break;
		case JIT_RET:
			if (!IS_IMM(op) && (a1 != AMD64_RAX)) amd64_mov_reg_reg(jit->ip, AMD64_RAX, a1, 8);
			if (IS_IMM(op)) amd64_mov_reg_imm(jit->ip, AMD64_RAX, a1);
			__pop_callee_saved_regs(jit);
			amd64_mov_reg_reg(jit->ip, AMD64_RSP, AMD64_RBP, 8);
			amd64_pop_reg(jit->ip, AMD64_RBP);
			amd64_ret(jit->ip);
			break;

		case JIT_PUTARG: __put_arg(jit, op); break;
		case JIT_FPUTARG: __fput_arg(jit, op); break;
		case JIT_GETARG: __get_arg(jit, op); break;
		case JIT_MSG: do {
				      struct jit_reg_allocator * al = jit->reg_al;
				      for (int i = 0; i < al->gp_reg_cnt; i++)
				      if (!al->gp_regs[i].callee_saved)
					      amd64_push_reg(jit->ip, al->gp_regs[i].id);

			      	      if (!IS_IMM(op)) amd64_mov_reg_reg_size(jit->ip, AMD64_RSI, a2, 8);
				      amd64_mov_reg_imm_size(jit->ip, AMD64_RDI, a1, 8);
				      amd64_alu_reg_reg(jit->ip, X86_XOR, AMD64_RAX, AMD64_RAX);
				      op->patch_addr = __PATCH_ADDR(jit);
				      amd64_call_imm(jit->ip, -1);

				      for (int i = al->gp_reg_cnt - 1; i >= 0; i--)
				      if (!al->gp_regs[i].callee_saved)
					      amd64_pop_reg(jit->ip, al->gp_regs[i].id);
			      } while (0);
			      break;

		case JIT_ALLOCA: break;

		default: found = 0;
	}

	if (found) return;


	switch (op->code) {
		case (JIT_MOV | REG): if (a1 != a2) amd64_mov_reg_reg(jit->ip, a1, a2, REG_SIZE); break;
		case (JIT_MOV | IMM):
			if (a2 == 0) amd64_alu_reg_reg(jit->ip, X86_XOR, a1, a1);
			else amd64_mov_reg_imm_size(jit->ip, a1, a2, 8); 
			break;

		case JIT_PREPARE: __prepare_call(jit, op, a1 + a2);
				  __push_caller_saved_regs(jit, op);
				  break;

		case JIT_PROLOG:
			  do {
				  struct jit_func_info * info = jit_current_func_info(jit);

				  *(void **)(a1) = jit->ip;
				  amd64_push_reg(jit->ip, AMD64_RBP);
				  amd64_mov_reg_reg(jit->ip, AMD64_RBP, AMD64_RSP, 8);

				  int stack_mem = info->allocai_mem + info->gp_reg_count * REG_SIZE + info->fp_reg_count * sizeof(jit_float) + info->general_arg_cnt * REG_SIZE + info->float_arg_cnt * sizeof(jit_float);

				  stack_mem = (stack_mem + 15) & 0xfffffff0; // 16-bytes aligned

				  amd64_alu_reg_imm(jit->ip, X86_SUB, AMD64_RSP, stack_mem + 8);
				  __push_callee_saved_regs(jit, op);
			  } while (0);
			break;
		
		case JIT_DECL_ARG: break;

		case JIT_RETVAL: 
			if (a1 != AMD64_RAX) amd64_mov_reg_reg(jit->ip, a1, AMD64_RAX, REG_SIZE);
			break;

		case JIT_LABEL: ((jit_label *)a1)->pos = __PATCH_ADDR(jit); break; 

		case (JIT_LD | IMM | SIGNED): 
			if (op->arg_size == REG_SIZE) amd64_mov_reg_mem(jit->ip, a1, a2, op->arg_size);
			else amd64_movsx_reg_mem(jit->ip, a1, a2, op->arg_size);
			break;

		case (JIT_LD | IMM | UNSIGNED): 
			if (op->arg_size == REG_SIZE) amd64_mov_reg_mem(jit->ip, a1, a2, op->arg_size);
			else {
				amd64_alu_reg_reg(jit->ip, X86_XOR, a1, a1); // register cleanup
				amd64_mov_reg_mem(jit->ip, a1, a2, op->arg_size);
			}
			break;

		case (JIT_LD | REG | SIGNED):
			if (op->arg_size == REG_SIZE) amd64_mov_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			else amd64_movsx_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			break;

		case (JIT_LD | REG | UNSIGNED):
			if (op->arg_size == REG_SIZE) amd64_mov_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			else  {
				amd64_alu_reg_reg(jit->ip, X86_XOR, a1, a1); // register cleanup
				amd64_mov_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			}
			break;

		case (JIT_LDX | IMM | SIGNED): 
			if (op->arg_size == REG_SIZE) amd64_mov_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			else amd64_movsx_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			break;

		case (JIT_LDX | IMM | UNSIGNED): 
			if (op->arg_size == REG_SIZE) amd64_mov_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			else {
				amd64_alu_reg_reg(jit->ip, X86_XOR, a1, a1); // register cleanup
				amd64_mov_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			}
			break;

		case (JIT_LDX | REG | SIGNED): 
			if (op->arg_size == REG_SIZE) amd64_mov_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
			else amd64_movsx_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
			break;

		case (JIT_LDX | REG | UNSIGNED): 
			if (op->arg_size == REG_SIZE) amd64_mov_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
			else {
				amd64_alu_reg_reg(jit->ip, X86_XOR, a1, a1); // register cleanup
				amd64_mov_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size);
			}
			break;
		case (JIT_ST | IMM): amd64_mov_mem_reg(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_ST | REG): amd64_mov_membase_reg(jit->ip, a1, 0, a2, op->arg_size); break;
		case (JIT_STX | IMM): amd64_mov_membase_reg(jit->ip, a2, a1, a3, op->arg_size); break;
		case (JIT_STX | REG): amd64_mov_memindex_reg(jit->ip, a1, 0, a2, 0, a3, op->arg_size); break;

		//
		// Floating-point operations;
		//
		case (JIT_FMOV | REG): amd64_sse_movsd_reg_reg(jit->ip, a1, a2); break;
		case (JIT_FMOV | IMM): amd64_movsd_reg_mem(jit->ip, a1, &op->flt_imm); break;
		case (JIT_FADD | REG): __sse_alu_op(jit, op, X86_SSE_ADD); break;
		case (JIT_FSUB | REG): __sse_sub_op(jit, a1, a2, a3); break;
		case (JIT_FRSB | REG): __sse_sub_op(jit, a1, a3, a2); break;
		case (JIT_FMUL | REG): __sse_alu_op(jit, op, X86_SSE_MUL); break;
		case (JIT_FDIV | REG): __sse_div_op(jit, a1, a2, a3); break;
                case (JIT_FNEG | REG): __sse_neg_op(jit, a1, a2); break;
		case (JIT_FBLT | REG): __sse_branch(jit, op, a1, a2, a3, X86_CC_LT); break;
                case (JIT_FBGT | REG): __sse_branch(jit, op, a1, a2, a3, X86_CC_GT); break;
                case (JIT_FBGE | REG): __sse_branch(jit, op, a1, a2, a3, X86_CC_GE); break;
                case (JIT_FBLE | REG): __sse_branch(jit, op, a1, a3, a2, X86_CC_GE); break;
                case (JIT_FBEQ | REG): __sse_branch(jit, op, a1, a3, a2, X86_CC_EQ); break;
                case (JIT_FBNE | REG): __sse_branch(jit, op, a1, a3, a2, X86_CC_NE); break;

		case (JIT_EXT | REG): amd64_sse_cvtsi2sd_reg_reg(jit->ip, a1, a2); break;
                case (JIT_TRUNC | REG): amd64_sse_cvttsd2si_reg_reg(jit->ip, a1, a2); break;
		case (JIT_CEIL | REG): __sse_floor(jit, a1, a2, 0); break;
                case (JIT_FLOOR | REG): __sse_floor(jit, a1, a2, 1); break;
		case (JIT_ROUND | REG): __sse_round(jit, a1, a2); break;

		case (JIT_FRET | REG):
			if (op->arg_size == sizeof(float)) 
				amd64_sse_cvtsd2ss_reg_reg(jit->ip, a1, a1);

			// pushes the value beyond the top of the stack
			amd64_sse_movlpd_membase_xreg(jit->ip, a1, AMD64_RSP, -8); 
			amd64_mov_reg_membase(jit->ip, AMD64_RAX, AMD64_RSP, -8, 8);
			// transfers the value from the stack to RAX
			amd64_sse_movsd_reg_membase(jit->ip, AMD64_XMM0, AMD64_RSP, -8);

			// common epilogue
			__pop_callee_saved_regs(jit);
			amd64_mov_reg_reg(jit->ip, AMD64_RSP, AMD64_RBP, 8);
			amd64_pop_reg(jit->ip, AMD64_RBP);
			amd64_ret(jit->ip);
			break;

		case JIT_FRETVAL:
			if (a2 == sizeof(float)) amd64_sse_cvtss2sd_reg_reg(jit->ip, a1, AMD64_XMM0);
			else if (a1 != AMD64_XMM0) amd64_sse_movsd_reg_reg(jit->ip, a1, AMD64_XMM0);
			break;

		case (JIT_FST | IMM): 
			if (op->arg_size == sizeof(float)) {
				int live = jitset_get(op->live_out, op->arg[1]);
				if (live) amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);
				amd64_sse_cvtsd2ss_reg_reg(jit->ip, a2, a2);
				amd64_movss_mem_reg(jit->ip, a1, a2);
				if (live) amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
			} else amd64_movlpd_mem_reg(jit->ip, a1, a2);
			break;

		case (JIT_FST | REG): 
			if (op->arg_size == sizeof(float)) {
				int live = jitset_get(op->live_out, op->arg[1]);
				if (live) amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);
				amd64_sse_cvtsd2ss_reg_reg(jit->ip, a2, a2);
				amd64_sse_movss_membase_reg(jit->ip, a1, 0, a2);
				if (live) amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
			} else amd64_sse_movlpd_membase_xreg(jit->ip, a2, a1, 0);
			break;

		case (JIT_FSTX | IMM): 
			if (op->arg_size == sizeof(float)) {
				int live = jitset_get(op->live_out, op->arg[2]);
				if (live) amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a3, a3, 0);
				amd64_sse_cvtsd2ss_reg_reg(jit->ip, a3, a3);
				amd64_sse_movss_membase_reg(jit->ip, a2, a1, a3);
				if (live) amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a3, a3, 1);
			} else amd64_sse_movlpd_membase_xreg(jit->ip, a3, a2, a1);
			break;

		case (JIT_FSTX | REG): 
			if (op->arg_size == sizeof(float)) {
				int live = jitset_get(op->live_out, op->arg[2]);
				if (live) amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a3, a3, 0);
				amd64_sse_cvtsd2ss_reg_reg(jit->ip, a3, a3);
				amd64_sse_movss_memindex_xreg(jit->ip, a1, 0, a2, 0, a3);
				if (live) amd64_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a3, a3, 1);
			} else amd64_sse_movlpd_memindex_xreg(jit->ip, a1, 0, a2, 0, a3);
			break;


		case (JIT_FLD | IMM): if (op->arg_size == sizeof(float))
					      amd64_sse_cvtss2sd_reg_mem(jit->ip, a1, a2);
				      else amd64_movsd_reg_mem(jit->ip, a1, a2);
				      break;

		case (JIT_FLD | REG): if (op->arg_size == sizeof(float))
					      amd64_sse_cvtss2sd_reg_membase(jit->ip, a1, a2, 0);
				      else amd64_sse_movlpd_xreg_membase(jit->ip, a1, a2, 0);
				      break;

		case (JIT_FLDX | IMM): if (op->arg_size == sizeof(float))
					       amd64_sse_cvtss2sd_reg_membase(jit->ip, a1, a2, a3);
				       else amd64_sse_movlpd_xreg_membase(jit->ip, a1, a2, a3);
				       break;

		case (JIT_FLDX | REG): if (op->arg_size == sizeof(float))
					       amd64_sse_cvtss2sd_reg_memindex(jit->ip, a1, a2, 0, a3, 0);
				       else amd64_sse_movlpd_xreg_memindex(jit->ip, a1, a2, 0, a3, 0);
				       break;

		case (JIT_UREG): __ureg(jit, a1, a2); break;

		case (JIT_LREG): 
			if (JIT_REG(a2).spec == JIT_RTYPE_ARG) assert(0);
			if (JIT_REG(a2).type == JIT_RTYPE_FLOAT) amd64_sse_movlpd_xreg_membase(jit->ip, a1, AMD64_RBP, __GET_FPREG_POS(jit, a2));
			else amd64_mov_reg_membase(jit->ip, a1, AMD64_RBP, __GET_REG_POS(jit, a2), REG_SIZE);
			break;

		case (JIT_SYNCREG):  __ureg(jit, a1, a2); break;
		case JIT_CODESTART: break;
		case JIT_NOP: break;

		// platform specific opcodes; used by the optimizer
		case (JIT_X86_STI | IMM): amd64_mov_mem_imm(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_X86_STI | REG): amd64_mov_membase_imm(jit->ip, a1, 0, a2, op->arg_size); break;
		case (JIT_X86_STXI | IMM): amd64_mov_membase_imm(jit->ip, a2, a1, a3, op->arg_size); break;
		case (JIT_X86_STXI | REG): amd64_mov_memindex_imm(jit->ip, a1, 0, a2, 0, a3, op->arg_size); break;

		default: printf("amd64: unknown operation (opcode: 0x%x)\n", GET_OP(op) >> 3);
	}
}

struct jit_reg_allocator * jit_reg_allocator_create()
{
	static int __arg_regs[] = { AMD64_RDI, AMD64_RSI, AMD64_RDX, AMD64_RCX, AMD64_R8, AMD64_R9 };
	static int __fp_arg_regs[] = { AMD64_XMM0, AMD64_XMM1, AMD64_XMM2, AMD64_XMM3, AMD64_XMM4, AMD64_XMM5, AMD64_XMM6, AMD64_XMM7 };

	struct jit_reg_allocator * a = JIT_MALLOC(sizeof(struct jit_reg_allocator));
	a->gp_reg_cnt = 14;

	a->gp_regpool = jit_regpool_init(a->gp_reg_cnt);
	a->gp_regs = JIT_MALLOC(sizeof(struct __hw_reg) * a->gp_reg_cnt);

	a->gp_regs[0] = (struct __hw_reg) { AMD64_RAX, 0, "rax", 0, 0, 14 };
	a->gp_regs[1] = (struct __hw_reg) { AMD64_RBX, 0, "rbx", 1, 0, 1 };
	a->gp_regs[2] = (struct __hw_reg) { AMD64_RCX, 0, "rcx", 0, 0, 11 };
	a->gp_regs[3] = (struct __hw_reg) { AMD64_RDX, 0, "rdx", 0, 0, 10 };
	a->gp_regs[4] = (struct __hw_reg) { AMD64_RSI, 0, "rsi", 0, 0, 12 };
	a->gp_regs[5] = (struct __hw_reg) { AMD64_RDI, 0, "rdi", 0, 0, 13  };
	a->gp_regs[6] = (struct __hw_reg) { AMD64_R8, 0, "r8", 0, 0, 9 };
	a->gp_regs[7] = (struct __hw_reg) { AMD64_R9, 0, "r9", 0, 0, 8 };
	a->gp_regs[8] = (struct __hw_reg) { AMD64_R10, 0, "r10", 0, 0, 6 };
	a->gp_regs[9] = (struct __hw_reg) { AMD64_R11, 0, "r11", 0, 0, 7 };
	a->gp_regs[10] = (struct __hw_reg) { AMD64_R12, 0, "r12", 1, 0, 2 };
	a->gp_regs[11] = (struct __hw_reg) { AMD64_R13, 0, "r13", 1, 0, 3 };
	a->gp_regs[12] = (struct __hw_reg) { AMD64_R14, 0, "r14", 1, 0, 4 };
	a->gp_regs[13] = (struct __hw_reg) { AMD64_R15, 0, "r15", 1, 0, 5 };


	a->gp_arg_reg_cnt = 6;

	a->fp_reg = AMD64_RBP;
	a->ret_reg = AMD64_RAX;

	a->fp_reg_cnt = 10;

	int reg = 0;
	a->fp_regpool = jit_regpool_init(a->fp_reg_cnt);
	a->fp_regs = JIT_MALLOC(sizeof(struct __hw_reg) * a->fp_reg_cnt);

	a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM13, 0, "xmm13", 0, 1, 1 };
	a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM12, 0, "xmm12", 0, 1, 2 };
	a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM0, 0, "xmm0", 0, 1, 99 };
	a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM1, 0, "xmm1", 0, 1, 98 };
	a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM2, 0, "xmm2", 0, 1, 97 };
	a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM3, 0, "xmm3", 0, 1, 96 };
	a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM4, 0, "xmm4", 0, 1, 95 };
	a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM5, 0, "xmm5", 0, 1, 94 };
	a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM6, 0, "xmm6", 0, 1, 93 };
	a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM7, 0, "xmm7", 0, 1, 92 };
	//a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM11, 0, "xmm11", 0, 1, 3 };
	//a->fp_regs[reg++] = (struct __hw_reg) { AMD64_XMM10, 0, "xmm10", 0, 1, 4 };
	/*
#ifndef JIT_REGISTER_TEST
	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM4, 0, "xmm4", 0, 1, 5 };
	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM5, 0, "xmm5", 0, 1, 6 };
	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM6, 0, "xmm6", 0, 1, 7 };
	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM7, 0, "xmm7", 0, 1, 8 };
#endif
*/

	a->gp_arg_reg_cnt = 6;
	a->gp_arg_regs = __arg_regs;

	a->fp_arg_reg_cnt = 8;
	a->fp_arg_regs = __fp_arg_regs;
	return a;
}
