/*
 * MyJIT 
 * Copyright (C) 2010, 2015 Petr Krajca, <petr.krajca@upol.cz>
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

#define GET_GPREG_POS(jit, r) (- ((JIT_REG(r).id + 1) * REG_SIZE) - jit_current_func_info(jit)->allocai_mem)
#define GET_FPREG_POS(jit, r) (- jit_current_func_info(jit)->gp_reg_count * REG_SIZE - (JIT_REG(r).id + 1) * sizeof(jit_float) - jit_current_func_info(jit)->allocai_mem)

#define GET_ARG_SPILL_POS(jit, info, arg) ((- (arg + info->gp_reg_count + info->fp_reg_count) * REG_SIZE) - jit_current_func_info(jit)->allocai_mem)

static inline int GET_REG_POS(struct jit * jit, int r)
{
	if (JIT_REG(r).spec == JIT_RTYPE_REG) {
		if (JIT_REG(r). type == JIT_RTYPE_INT) return GET_GPREG_POS(jit, r);
		else return GET_FPREG_POS(jit, r);
	} else return GET_ARG_SPILL_POS(jit, jit_current_func_info(jit), JIT_REG(r).id);
}

#include "x86-common-stuff.c"

void jit_init_arg_params(struct jit * jit, struct jit_func_info * info, int p, int * phys_reg)
{
	struct jit_inp_arg * a = &(info->args[p]);
	if (a->type != JIT_FLOAT_NUM) { // normal argument
		int pos = a->gp_pos;
		if (pos < jit->reg_al->gp_arg_reg_cnt) {
			a->passed_by_reg = 1;
			a->location.reg = jit->reg_al->gp_arg_regs[pos]->id;
			a->spill_pos = GET_ARG_SPILL_POS(jit, info, p);
		} else {
			int stack_pos = (pos - jit->reg_al->gp_arg_reg_cnt) + MAX(0, (a->fp_pos - jit->reg_al->fp_arg_reg_cnt));

			a->location.stack_pos = 16 + stack_pos * 8;
			a->spill_pos = 16 + stack_pos * 8;
			a->passed_by_reg = 0;
		}
		a->overflow = 0;
		return;
	}

	// FP argument
	int pos = a->fp_pos;
	if (pos < jit->reg_al->fp_arg_reg_cnt) {
		a->passed_by_reg = 1;
		a->location.reg = jit->reg_al->fp_arg_regs[pos]->id;
		a->spill_pos = GET_ARG_SPILL_POS(jit, info, p);
	} else {

		int stack_pos = (pos - jit->reg_al->fp_arg_reg_cnt) + MAX(0, (a->gp_pos - jit->reg_al->gp_arg_reg_cnt));

		a->location.stack_pos = 16 + stack_pos * 8;
		a->spill_pos = 16 + stack_pos * 8;
		a->passed_by_reg = 0;
	}
	a->overflow = 0;
}

/**
 * Assigns integer value to register which is used to pass the argument
 */
static inline void emit_set_arg(struct jit * jit, struct jit_out_arg * arg)
{
	int sreg;
	int reg = jit->reg_al->gp_arg_regs[arg->argpos]->id;
	jit_value value = arg->value.generic;
	if (arg->isreg) {
		if (is_spilled(value, jit->prepared_args.op, &sreg)) {
			amd64_mov_reg_membase(jit->ip, reg, AMD64_RBP, GET_REG_POS(jit, value), REG_SIZE);
		} else {
			if (reg != sreg) amd64_mov_reg_reg(jit->ip, reg, sreg, REG_SIZE);
		}
	} else amd64_mov_reg_imm_size(jit->ip, reg, value, 8);
}

/**
 * Assigns FP value to register which is used to pass the argument
 */
static inline void emit_set_fparg(struct jit * jit, struct jit_out_arg * arg)
{
	int sreg;
	int reg = jit->reg_al->fp_arg_regs[arg->argpos]->id;
	long value = arg->value.generic;
	if (arg->isreg) {
		if (is_spilled(value, jit->prepared_args.op, &sreg)) {
			int pos = GET_REG_POS(jit, value);
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
			unsigned int tmp;

			memcpy(&tmp, &val, sizeof(float));
			//amd64_mov_reg_imm_size(jit->ip, AMD64_RAX, *(unsigned int *)&val, 4);
			amd64_mov_reg_imm_size(jit->ip, AMD64_RAX, tmp, 4);
			amd64_movd_xreg_reg_size(jit->ip, reg, AMD64_RAX, 4);
		} else {
			amd64_mov_reg_imm_size(jit->ip, AMD64_RAX, value, 8);
			amd64_movd_xreg_reg_size(jit->ip, reg, AMD64_RAX, 8);
		}
	}
}

/**
 * Pushes integer value on the stack
 */
static inline void emit_push_arg(struct jit * jit, struct jit_out_arg * arg)
{
	int sreg;
	if (arg->isreg) {
		if (is_spilled(arg->value.generic, jit->prepared_args.op, &sreg))
			amd64_push_membase(jit->ip, AMD64_RBP, GET_REG_POS(jit, arg->value.generic));
		else amd64_push_reg(jit->ip, sreg);
	} else {
		amd64_mov_reg_imm_size(jit->ip, AMD64_RAX, arg->value.generic, REG_SIZE);
		amd64_push_reg(jit->ip, AMD64_RAX);
	}
}

/**
 * Pushes float value on the stack
 */
static inline void emit_fppush_arg(struct jit * jit, struct jit_out_arg * arg)
{
	int sreg;
	if (arg->size == sizeof(double)) {
		if (arg->isreg) {
			if (is_spilled(arg->value.generic, jit->prepared_args.op, &sreg)) {
				int pos = GET_FPREG_POS(jit, arg->value.generic);
				amd64_push_membase(jit->ip, AMD64_RBP, pos);
			} else {
				// ``PUSH sreg'' for XMM regs
				amd64_alu_reg_imm(jit->ip, X86_SUB, AMD64_RSP, 8);
				amd64_sse_movlpd_membase_xreg(jit->ip, sreg, AMD64_RSP, 0);
			}
		} else {
			double b = arg->value.fp;
			unsigned long tmp;
			memcpy(&tmp, &b, sizeof(double));
			amd64_mov_reg_imm_size(jit->ip, AMD64_RAX, tmp, 8);
			amd64_push_reg(jit->ip, AMD64_RAX);
		}

	} else {
		if (arg->isreg) {
			if (is_spilled(arg->value.generic, jit->prepared_args.op, &sreg)) {
				amd64_sse_cvtsd2ss_reg_membase(jit->ip, AMD64_XMM0, AMD64_RBP, GET_REG_POS(jit, arg->value.generic));
			} else {
				amd64_sse_cvtsd2ss_reg_reg(jit->ip, AMD64_XMM0, sreg);
			}
			amd64_alu_reg_imm(jit->ip, X86_SUB, AMD64_RSP, 8);
			amd64_sse_movlpd_membase_xreg(jit->ip, AMD64_XMM0, AMD64_RSP, 0);
		} else {
			float b = arg->value.fp;
			unsigned long tmp = 0;
			memcpy(&tmp, &b, sizeof(float));
			
			amd64_mov_reg_imm_size(jit->ip, AMD64_RAX, tmp, 8);
			amd64_push_reg(jit->ip, AMD64_RAX);
		}
	}
}

static inline int emit_arguments(struct jit * jit)
{
	int stack_correction = 0;
	struct jit_out_arg * args = jit->prepared_args.args;

	int gp_pushed = MAX(jit->prepared_args.gp_args - jit->reg_al->gp_arg_reg_cnt, 0);
	int fp_pushed = MAX(jit->prepared_args.fp_args - jit->reg_al->fp_arg_reg_cnt, 0);

	if (jit_current_func_info(jit)->has_prolog) {
		if ((jit->push_count + gp_pushed + fp_pushed) % 2) {
			amd64_alu_reg_imm(jit->ip, X86_SUB, AMD64_RSP, 8);
			stack_correction = 8;
		}
	} else {
		if ((jit->push_count + gp_pushed + fp_pushed) % 2 == 0) {
			amd64_alu_reg_imm(jit->ip, X86_SUB, AMD64_RSP, 8);
			stack_correction = 8;
		}
	}

	for (int x = jit->prepared_args.count - 1; x >= 0; x --) {
		struct jit_out_arg * arg = &(args[x]);
		if (!arg->isfp) {
			if (arg->argpos < jit->reg_al->gp_arg_reg_cnt) emit_set_arg(jit, arg);
			else emit_push_arg(jit, arg);
		} else {
			if (arg->argpos < jit->reg_al->fp_arg_reg_cnt) emit_set_fparg(jit, arg);
			else emit_fppush_arg(jit, arg);
		}
	}
	/* AL is used to pass the number of floating point arguments passed through the XMM0-XMM7 registers */
	int fp_reg_arg_cnt = MIN(jit->prepared_args.fp_args, jit->reg_al->fp_arg_reg_cnt);
	if (fp_reg_arg_cnt != 0) amd64_mov_reg_imm(jit->ip, AMD64_RAX, fp_reg_arg_cnt); 
	else amd64_alu_reg_reg_size(jit->ip, X86_XOR, AMD64_RAX, AMD64_RAX, 4);
	return stack_correction;
}

static void emit_funcall(struct jit * jit, struct jit_op * op, int imm)
{
	// correctly aligns stack to 16 bytes
	int stack_correction = emit_arguments(jit);

	if (!imm) {
		jit_hw_reg * hreg = rmap_get(op->regmap, op->arg[0]);
		if (hreg) amd64_call_reg(jit->ip, hreg->id);
		else amd64_call_membase(jit->ip, AMD64_RBP, GET_REG_POS(jit, op->arg[0]));
	} else {
		if (jit_is_label(jit, (void *)op->arg[0])) {
			op->patch_addr = JIT_BUFFER_OFFSET(jit);
			amd64_call_imm(jit->ip, JIT_GET_ADDR(jit, op->arg[0]) - 4); // 4: magic constant
		} else {
			// external functions may reside anywhere in the memory
			// even in the place which is not addressable with 32bit wide value
			// therefore external functions are called using %r11 register
			// which is caller-saved register and its value should be already on stack 
			amd64_mov_reg_imm_size(jit->ip, AMD64_R11, op->arg[0], 8);
			amd64_call_reg(jit->ip, AMD64_R11);
		}
	}

	stack_correction += jit->prepared_args.stack_size;
	if (stack_correction)
		amd64_alu_reg_imm(jit->ip, X86_ADD, AMD64_RSP, stack_correction);
	JIT_FREE(jit->prepared_args.args);

	jit->push_count -= emit_pop_caller_saved_regs(jit, op);
}

static void emit_prolog_op(struct jit * jit, jit_op * op)
{
	jit->current_func = op;
	struct jit_func_info * info = jit_current_func_info(jit);

	int prolog = jit_current_func_info(jit)->has_prolog;

	while ((jit_value)jit->ip % 16)
		amd64_nop(jit->ip);

	op->patch_addr = JIT_BUFFER_OFFSET(jit);
	if (prolog) {
		amd64_push_reg(jit->ip, AMD64_RBP);
		amd64_mov_reg_reg(jit->ip, AMD64_RBP, AMD64_RSP, 8);
	}

	int stack_mem = info->allocai_mem + info->gp_reg_count * REG_SIZE + info->fp_reg_count * sizeof(jit_float) + info->general_arg_cnt * REG_SIZE + info->float_arg_cnt * sizeof(jit_float);

	stack_mem = jit_value_align(stack_mem, JIT_STACK_ALIGNMENT); // 16-bytes aligned

	if (prolog)
		amd64_alu_reg_imm(jit->ip, X86_SUB, AMD64_RSP, stack_mem);
	jit->push_count = emit_push_callee_saved_regs(jit, op);
}

static void emit_msg_op(struct jit * jit, jit_op * op)
{
	struct jit_reg_allocator * al = jit->reg_al;
	for (int i = 0; i < al->gp_reg_cnt; i++)
		if (!al->gp_regs[i].callee_saved)
			amd64_push_reg(jit->ip, al->gp_regs[i].id);

	if (!IS_IMM(op)) amd64_mov_reg_reg_size(jit->ip, AMD64_RSI, op->r_arg[1], 8);
	amd64_mov_reg_imm_size(jit->ip, AMD64_RDI, op->r_arg[0], 8);
	amd64_alu_reg_reg(jit->ip, X86_XOR, AMD64_RAX, AMD64_RAX);
	amd64_mov_reg_imm(jit->ip, AMD64_RDX, printf);
	amd64_call_reg(jit->ip, AMD64_RDX);

	for (int i = al->gp_reg_cnt - 1; i >= 0; i--)
		if (!al->gp_regs[i].callee_saved)
			amd64_pop_reg(jit->ip, al->gp_regs[i].id);

}

static void emit_fret_op(struct jit * jit, jit_op * op)
{
	jit_value arg = op->r_arg[0];

	if (op->arg_size == sizeof(float)) 
		sse_cvtsd2ss_reg_reg(jit->ip, arg, arg);

	// pushes the value beyond the top of the stack
	sse_movlpd_membase_xreg(jit->ip, arg, COMMON86_SP, -8); 
	common86_mov_reg_membase(jit->ip, COMMON86_AX, COMMON86_SP, -8, 8);
	// transfers the value from the stack to RAX
	sse_movsd_reg_membase(jit->ip, COMMON86_XMM0, COMMON86_SP, -8);

	// common epilogue
	jit->push_count -= emit_pop_callee_saved_regs(jit);
	if (jit_current_func_info(jit)->has_prolog) {
		common86_mov_reg_reg(jit->ip, COMMON86_SP, COMMON86_BP, 8);
		common86_pop_reg(jit->ip, COMMON86_BP);
	}
	common86_ret(jit->ip);
}

static void emit_fretval_op(struct jit * jit, jit_op * op)
{
	jit_value arg = op->r_arg[0];
	if (op->arg_size == sizeof(float)) sse_cvtss2sd_reg_reg(jit->ip, arg, COMMON86_XMM0);
	else if (arg != COMMON86_XMM0) sse_movsd_reg_reg(jit->ip, arg, COMMON86_XMM0);
}

void jit_patch_external_calls(struct jit * jit)
{
	// On AMD64 called function can be anywhere in the memory, even so far that its address won't fit into
	// 32bit address, therefore all external function calls are handed through the R_IMM register
}

struct jit_reg_allocator * jit_reg_allocator_create()
{
	struct jit_reg_allocator * a = JIT_MALLOC(sizeof(struct jit_reg_allocator));
	a->gp_reg_cnt = 13;

	a->gp_regs = JIT_MALLOC(sizeof(jit_hw_reg) * a->gp_reg_cnt);

	a->gp_regs[0] = (jit_hw_reg) { AMD64_RAX, "rax", 0, 0, 7 };
	a->gp_regs[1] = (jit_hw_reg) { AMD64_RBX, "rbx", 1, 0, 8 };
	a->gp_regs[2] = (jit_hw_reg) { AMD64_RCX, "rcx", 0, 0, 4 };
	a->gp_regs[3] = (jit_hw_reg) { AMD64_RDX, "rdx", 0, 0, 3 };
	a->gp_regs[4] = (jit_hw_reg) { AMD64_RSI, "rsi", 0, 0, 2 };
	a->gp_regs[5] = (jit_hw_reg) { AMD64_RDI, "rdi", 0, 0, 1 };
	a->gp_regs[6] = (jit_hw_reg) { AMD64_R8,  "r8", 0, 0, 5 };
	a->gp_regs[7] = (jit_hw_reg) { AMD64_R9,  "r9", 0, 0, 6 };
	a->gp_regs[8] = (jit_hw_reg) { AMD64_R10, "r10", 0, 0, 9 };
	a->gp_regs[9] = (jit_hw_reg) { AMD64_R11, "r11", 0, 0, 10 };
	a->gp_regs[10] = (jit_hw_reg) { AMD64_R12, "r12", 1, 0, 11 };
	a->gp_regs[11] = (jit_hw_reg) { AMD64_R14, "r14", 1, 0, 13 };
	a->gp_regs[12] = (jit_hw_reg) { AMD64_R15, "r15", 1, 0, 14 };
	// R13 has some addressing limitations, therefore it is not used as GPR
	// since it may lead to unexpected behavior
//	a->gp_regs[13] = (jit_hw_reg) { AMD64_R13, 0, "r13", 1, 0, 12 };


	a->gp_arg_reg_cnt = 6;

	a->fp_reg = AMD64_RBP;
	a->ret_reg = &(a->gp_regs[0]);

	a->fp_reg_cnt = 10;

	int reg = 0;
	a->fp_regs = JIT_MALLOC(sizeof(jit_hw_reg) * a->fp_reg_cnt);

	a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM0, "xmm0", 0, 1, 99 };
	a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM1, "xmm1", 0, 1, 98 };
	a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM2, "xmm2", 0, 1, 97 };
	a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM3, "xmm3", 0, 1, 96 };
	a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM4, "xmm4", 0, 1, 95 };
	a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM5, "xmm5", 0, 1, 94 };
	a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM6, "xmm6", 0, 1, 93 };
	a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM7, "xmm7", 0, 1, 92 };
	//a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM11, "xmm11", 0, 1, 3 };
	//a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM10, "xmm10", 0, 1, 4 };
	a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM13, "xmm13", 0, 1, 1 };
	a->fp_regs[reg++] = (jit_hw_reg) { AMD64_XMM12, "xmm12", 0, 1, 2 };
	/*
#ifndef JIT_REGISTER_TEST
	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM4, "xmm4", 0, 1, 5 };
	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM5, "xmm5", 0, 1, 6 };
	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM6, "xmm6", 0, 1, 7 };
	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM7, "xmm7", 0, 1, 8 };
#endif
*/

	a->fpret_reg = &(a->fp_regs[0]);

	a->gp_arg_reg_cnt = 6;
	a->gp_arg_regs = JIT_MALLOC(sizeof(jit_hw_reg *) * 6); 
	a->gp_arg_regs[0] = &(a->gp_regs[5]);
	a->gp_arg_regs[1] = &(a->gp_regs[4]);
	a->gp_arg_regs[2] = &(a->gp_regs[3]);
	a->gp_arg_regs[3] = &(a->gp_regs[2]);
	a->gp_arg_regs[4] = &(a->gp_regs[6]);
	a->gp_arg_regs[5] = &(a->gp_regs[7]);

	a->fp_arg_reg_cnt = 8;
	a->fp_arg_regs = JIT_MALLOC(sizeof(jit_hw_reg *) * 8); 
	for (int i = 0; i < 8; i++)
		a->fp_arg_regs[i] = &(a->fp_regs[i]);

	return a;
}
