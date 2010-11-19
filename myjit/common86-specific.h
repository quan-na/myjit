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

#include "common86-codegen.h"


static inline int __is_spilled(int arg_id, jit_op * prepare_op, int * reg);
static int __push_callee_saved_regs(struct jit * jit, jit_op * op);
static int __push_caller_saved_regs(struct jit * jit, jit_op * op);
static int __pop_callee_saved_regs(struct jit * jit);
static int __pop_caller_saved_regs(struct jit * jit, jit_op * op);

#ifdef JIT_ARCH_I386
#include "x86-specific.h"
#endif

#ifdef JIT_ARCH_AMD64
#include "amd64-specific.h"
#endif

#include "sse2-specific.h"


//
//
// Registers
//
//

static inline void __push_reg(struct jit * jit, struct __hw_reg * r)
{
	if (!r->fp) common86_push_reg(jit->ip, r->id);
	else {
		common86_alu_reg_imm(jit->ip, X86_SUB, COMMON86_SP, 8);
		sse_movlpd_membase_xreg(jit->ip, r->id, COMMON86_SP, 0);
	}
}

static inline void __pop_reg(struct jit * jit, struct __hw_reg * r)
{
	if (!r->fp) common86_pop_reg(jit->ip, r->id);
	else {
		sse_movlpd_xreg_membase(jit->ip, r->id, COMMON86_SP, 0);
		common86_alu_reg_imm(jit->ip, X86_ADD, COMMON86_SP, 8);
	}
}

static inline int __uses_hw_reg(struct jit_op * op, long reg, int fp)
{
	for (int i = 0; i < 3; i++)
		if ((ARG_TYPE(op, i + 1) == REG) || (ARG_TYPE(op, i + 1) == TREG)) {
			if (fp && (JIT_REG(op->arg[i]).type == JIT_RTYPE_INT)) continue;
			if (!fp && (JIT_REG(op->arg[i]).type == JIT_RTYPE_FLOAT)) continue;
			if (op->r_arg[i] == reg) return 1;
		}
	return 0;
}

static int __push_callee_saved_regs(struct jit * jit, jit_op * op)
{
	int count = 0;
	for (int i = 0; i < jit->reg_al->gp_reg_cnt; i++) {
		struct __hw_reg * r = &(jit->reg_al->gp_regs[i]);
		if (r->callee_saved) 
			for (struct jit_op * o = op->next; o != NULL; o = o->next) {
				if (GET_OP(o) == JIT_PROLOG) break;
				if (__uses_hw_reg(o, r->id, 0)) {
					__push_reg(jit, r);
					count++;
					break;
				}
			}
	}
	return count;
}

static int __pop_callee_saved_regs(struct jit * jit)
{
	int count = 0;
	struct jit_op * op = jit->current_func;

	for (int i = jit->reg_al->gp_reg_cnt - 1; i >= 0; i--) {
		struct __hw_reg * r = &(jit->reg_al->gp_regs[i]);
		if (r->callee_saved) 
			for (struct jit_op * o = op->next; o != NULL; o = o->next) {
				if (GET_OP(o) == JIT_PROLOG) break;
				if (__uses_hw_reg(o, r->id, 0)) {
					__pop_reg(jit, r);
					count++;
					break;
				}
			}
	}
	return count;
}

static int __generic_push_caller_saved_regs(struct jit * jit, jit_op * op, int reg_count,
						    struct __hw_reg * regs, int fp, int skip_reg)
{
	int reg;
	int count = 0;
	for (int i = 0; i < reg_count; i++) {
		if ((regs[i].id == skip_reg) || (regs[i].callee_saved)) continue;
		struct __hw_reg * hreg = rmap_is_associated(op->regmap, regs[i].id, fp, &reg);
		if (hreg && jitset_get(op->live_in, reg)) __push_reg(jit, hreg), count++;
	}
	return count;
}

static int __push_caller_saved_regs(struct jit * jit, jit_op * op)
{
	while (op) {
		if (GET_OP(op) == JIT_CALL) break;
		op = op->next;
	}
	int count = __generic_push_caller_saved_regs(jit, op, jit->reg_al->gp_reg_cnt, jit->reg_al->gp_regs, 0, jit->reg_al->ret_reg);
	count += __generic_push_caller_saved_regs(jit, op, jit->reg_al->fp_reg_cnt, jit->reg_al->fp_regs, 1, jit->reg_al->fpret_reg);
	return count;
}

static int __generic_pop_caller_saved_regs(struct jit * jit, jit_op * op, int reg_count,
						    struct __hw_reg * regs, int fp, int skip_reg)
{
	int reg;
	int count = 0;
	for (int i = reg_count - 1; i >= 0; i--) {
		if ((regs[i].id == skip_reg) || (regs[i].callee_saved)) continue;
		struct __hw_reg * hreg = rmap_is_associated(op->regmap, regs[i].id, fp, &reg);
		if (hreg && jitset_get(op->live_in, reg)) __pop_reg(jit, hreg), count++;
	}
	return count;
}

static int __pop_caller_saved_regs(struct jit * jit, jit_op * op)
{
	int count = __generic_pop_caller_saved_regs(jit, op, jit->reg_al->fp_reg_cnt, jit->reg_al->fp_regs, 1, jit->reg_al->fpret_reg);
	count += __generic_pop_caller_saved_regs(jit, op, jit->reg_al->gp_reg_cnt, jit->reg_al->gp_regs, 0, jit->reg_al->ret_reg);
	return count;
}


//
//
// Common ALU operation
//
//

static void emit_alu_op(struct jit * jit, struct jit_op * op, int x86_op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_alu_reg_imm(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);

	}  else {
		if (op->r_arg[0] == op->r_arg[1]) {
			common86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
		} else if (op->r_arg[0] == op->r_arg[2]) {
			common86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[1]);
		} else {
			common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
			common86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
		}	
	}
}

static inline void emit_sub_op(struct jit * jit, struct jit_op * op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) common86_lea_membase(jit->ip, op->r_arg[0], op->r_arg[1], -op->r_arg[2]);
		else common86_alu_reg_imm(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
		return;

	}
	if (op->r_arg[0] == op->r_arg[1]) {
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
	} else if (op->r_arg[0] == op->r_arg[2]) {
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
		common86_neg_reg(jit->ip, op->r_arg[0]);
	} else {
		common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
	}	
}

static inline void emit_subx_op(struct jit * jit, struct jit_op * op, int x86_op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_alu_reg_imm(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
		return;

	}
	if (op->r_arg[0] == op->r_arg[1]) {
		common86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
	} else if (op->r_arg[0] == op->r_arg[2]) {
		common86_push_reg(jit->ip, op->r_arg[2]);
		common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_alu_reg_membase(jit->ip, x86_op, op->r_arg[0], COMMON86_SP, 0);
		common86_alu_reg_imm(jit->ip, X86_ADD, COMMON86_SP, 8);
	} else {
		common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
	}	
}

static inline void emit_rsb_op(struct jit * jit, struct jit_op * op, int imm)
{
	if (imm) {
		if (op->r_arg[0] == op->r_arg[1]) common86_alu_reg_imm(jit->ip, X86_ADD, op->r_arg[0], -op->r_arg[2]);
		else common86_lea_membase(jit->ip, op->r_arg[0], op->r_arg[1], -op->r_arg[2]);
		common86_neg_reg(jit->ip, op->r_arg[0]);
		return;
	}

	if (op->r_arg[0] == op->r_arg[1]) { // O1 = O3 - O1
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
		common86_neg_reg(jit->ip, op->r_arg[0]);
	} else if (op->r_arg[0] == op->r_arg[2]) { // O1 = O1 - O2
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
	} else {
		common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[2], REG_SIZE);
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
	}
}

static inline void emit_mul_op(struct jit * jit, struct jit_op * op, int imm, int sign, int high_bytes)
{
	jit_value dest = op->r_arg[0];
	jit_value factor1 = op->r_arg[1];
	jit_value factor2 = op->r_arg[2];

	// optimization for special cases 
	if ((!high_bytes) && (imm)) {
		switch (factor2) {
			case 2: if (factor1 == dest) common86_shift_reg_imm(jit->ip, X86_SHL, dest, 1);
				else common86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 0);
				return;

			case 3: common86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 1); return;

			case 4: if (factor1 != dest) common86_mov_reg_reg(jit->ip, dest, factor1, REG_SIZE);
				common86_shift_reg_imm(jit->ip, X86_SHL, dest, 2);
				return;
			case 5: common86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 2);
				return;
			case 8: if (factor1 != dest) common86_mov_reg_reg(jit->ip, dest, factor1, REG_SIZE);
				common86_shift_reg_imm(jit->ip, X86_SHL, dest, 3);
				return;
			case 9: common86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 3);
				return;
		}
	}


	// generic multiplication
	int ax_in_use = jit_reg_in_use(op, COMMON86_AX, 0);
	int dx_in_use = jit_reg_in_use(op, COMMON86_DX, 0);

	if ((dest != COMMON86_AX) && ax_in_use) common86_push_reg(jit->ip, COMMON86_AX);
	if ((dest != COMMON86_DX) && dx_in_use) common86_push_reg(jit->ip, COMMON86_DX);

	if (imm) {
		if (factor1 != COMMON86_AX) common86_mov_reg_reg(jit->ip, COMMON86_AX, factor1, REG_SIZE);
		common86_mov_reg_imm(jit->ip, COMMON86_DX, factor2);
		common86_mul_reg(jit->ip, COMMON86_DX, sign);
	} else {
		if (factor1 == COMMON86_AX) common86_mul_reg(jit->ip, factor2, sign);
		else if (factor2 == COMMON86_AX) common86_mul_reg(jit->ip, factor1, sign);
		else {
			common86_mov_reg_reg(jit->ip, COMMON86_AX, factor1, REG_SIZE);
			common86_mul_reg(jit->ip, factor2, sign);
		}
	}

	if (!high_bytes) {
		if (dest != COMMON86_AX) common86_mov_reg_reg(jit->ip, dest, COMMON86_AX, REG_SIZE);
	} else {
		if (dest != COMMON86_DX) common86_mov_reg_reg(jit->ip, dest, COMMON86_DX, REG_SIZE);
	}

	if ((dest != COMMON86_DX) && dx_in_use) common86_pop_reg(jit->ip, COMMON86_DX);
	if ((dest != COMMON86_AX) && ax_in_use) common86_pop_reg(jit->ip, COMMON86_AX);
}

static inline void emit_div_op(struct jit * jit, struct jit_op * op, int imm, int sign, int modulo)
{
	jit_value dest = op->r_arg[0];
	jit_value dividend = op->r_arg[1];
	jit_value divisor = op->r_arg[2];

	if (imm && ((divisor == 2) || (divisor == 4) || (divisor == 8))) {
		if (dest != dividend) common86_mov_reg_reg(jit->ip, dest, dividend, REG_SIZE);
		if (!modulo) {
			switch (divisor) {
				case 2: common86_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 1); break;
				case 4: common86_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 2); break;
				case 8: common86_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 3); break;
			}
		} else {
			switch (divisor) {
				case 2: common86_alu_reg_imm(jit->ip, X86_AND, dest, 0x1); break;
				case 4: common86_alu_reg_imm(jit->ip, X86_AND, dest, 0x3); break;
				case 8: common86_alu_reg_imm(jit->ip, X86_AND, dest, 0x7); break;
			}
		}
		return;
	}

	int ax_in_use = jit_reg_in_use(op, COMMON86_AX, 0);
	int dx_in_use = jit_reg_in_use(op, COMMON86_DX, 0);

	if ((dest != COMMON86_AX) && ax_in_use) common86_push_reg(jit->ip, COMMON86_AX);
	if ((dest != COMMON86_DX) && dx_in_use) common86_push_reg(jit->ip, COMMON86_DX);

	if (imm) {
		if (dividend != COMMON86_AX) common86_mov_reg_reg(jit->ip, COMMON86_AX, dividend, REG_SIZE);
		common86_cdq(jit->ip);
		if (dest != COMMON86_BX) common86_push_reg(jit->ip, COMMON86_BX);
		common86_mov_reg_imm_size(jit->ip, COMMON86_BX, divisor, REG_SIZE);
		common86_div_reg(jit->ip, COMMON86_BX, sign);
		if (dest != COMMON86_BX) common86_pop_reg(jit->ip, COMMON86_BX);
	} else {
		if ((divisor == COMMON86_AX) || (divisor == COMMON86_DX)) {
			common86_push_reg(jit->ip, divisor);
		}

		if (dividend != COMMON86_AX) common86_mov_reg_reg(jit->ip, COMMON86_AX, dividend, REG_SIZE);

		common86_cdq(jit->ip);

		if ((divisor == COMMON86_AX) || (divisor == COMMON86_DX)) {
			common86_div_membase(jit->ip, COMMON86_SP, 0, sign);
			common86_alu_reg_imm(jit->ip, X86_ADD, COMMON86_SP, 8);
		} else {
			common86_div_reg(jit->ip, divisor, sign);
		}
	}

	if (!modulo) {
		if (dest != COMMON86_AX) common86_mov_reg_reg(jit->ip, dest, COMMON86_AX, REG_SIZE);
	} else {
		if (dest != COMMON86_DX) common86_mov_reg_reg(jit->ip, dest, COMMON86_DX, REG_SIZE);
	}

	if ((dest != COMMON86_DX) && dx_in_use) common86_pop_reg(jit->ip, COMMON86_DX);
	if ((dest != COMMON86_AX) && ax_in_use) common86_pop_reg(jit->ip, COMMON86_AX);
}

static inline void emit_shift_op(struct jit * jit, struct jit_op * op, int shift_op, int imm)
{
	if (imm) { 
		if (op->r_arg[0] != op->r_arg[1]) common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_shift_reg_imm(jit->ip, shift_op, op->r_arg[0], op->r_arg[2]);
	} else {
		int destreg = op->r_arg[0];
		int valreg = op->r_arg[1];
		int shiftreg = op->r_arg[2];
		int cx_pathology = 0; // shifting content in the ECX register

		int cx_in_use = jit_reg_in_use(op, COMMON86_CX, 0);
		int dx_in_use = jit_reg_in_use(op, COMMON86_DX, 0);

		if (destreg == COMMON86_CX) {
			cx_pathology = 1;
			if (dx_in_use) common86_push_reg(jit->ip, COMMON86_DX); 
			destreg = COMMON86_DX;
		}

		if (shiftreg != COMMON86_CX) {
			if (cx_in_use) common86_push_reg(jit->ip, COMMON86_CX); 
			common86_mov_reg_reg(jit->ip, COMMON86_CX, shiftreg, REG_SIZE);
		}
		if (destreg != valreg) common86_mov_reg_reg(jit->ip, destreg, valreg, REG_SIZE); 
		common86_shift_reg(jit->ip, shift_op, destreg);
		if (cx_pathology) {
			common86_mov_reg_reg(jit->ip, COMMON86_CX, COMMON86_DX, REG_SIZE);
			if ((shiftreg != COMMON86_CX) && cx_in_use) common86_alu_reg_imm(jit->ip, X86_ADD, COMMON86_SP, REG_SIZE);
			if (dx_in_use) common86_pop_reg(jit->ip, COMMON86_DX);
		} else {
			if ((shiftreg != COMMON86_CX) && cx_in_use) common86_pop_reg(jit->ip, COMMON86_CX);
		}
	}
}

static inline void emit_cond_op(struct jit * jit, struct jit_op * op, int amd64_cond, int imm, int sign)
{
	if (imm) common86_alu_reg_imm(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);
	else common86_alu_reg_reg(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);
	if ((op->r_arg[0] != COMMON86_SI) && (op->r_arg[0] != COMMON86_DI)) {
		common86_mov_reg_imm(jit->ip, op->r_arg[0], 0);
		common86_set_reg(jit->ip, amd64_cond, op->r_arg[0], sign);
	} else {
		common86_xchg_reg_reg(jit->ip, COMMON86_AX, op->r_arg[0], REG_SIZE);
		common86_mov_reg_imm(jit->ip, COMMON86_AX, 0);
		common86_set_reg(jit->ip, amd64_cond, COMMON86_AX, sign);
		common86_xchg_reg_reg(jit->ip, COMMON86_AX, op->r_arg[0], REG_SIZE);
	}
}

static inline void emit_branch_op(struct jit * jit, struct jit_op * op, int cond, int imm, int sign)
{
	if (imm) common86_alu_reg_imm(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);
	else common86_alu_reg_reg(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	common86_branch_disp32(jit->ip, cond, __JIT_GET_ADDR(jit, op->r_arg[0]), sign);
}

static inline void emit_branch_mask_op(struct jit * jit, struct jit_op * op, int cond, int imm)
{
	if (imm) common86_test_reg_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
	else common86_test_reg_reg(jit->ip, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	common86_branch_disp32(jit->ip, cond, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}

static inline void emit_branch_overflow_op(struct jit * jit, struct jit_op * op, int alu_op, int imm)
{
	if (imm) common86_alu_reg_imm(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);
	else common86_alu_reg_reg(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	common86_branch_disp32(jit->ip, X86_CC_O, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}

/* determines whether the argument value was spilled out or not,
 * if the register is associated with the hardware register
 * it is returned through the reg argument
 */
// FIXME: reports as spilled also argument which contains appropriate value
static inline int __is_spilled(int arg_id, jit_op * prepare_op, int * reg)
{
        struct __hw_reg * hreg = rmap_get(prepare_op->regmap, arg_id);

        if (hreg) {
                *reg = hreg->id;
                return 0;
        } else return 1;
}

/**
 * Emits all LD operations
 */
static inline void emit_ld_op(struct jit * jit, jit_op * op, jit_value a1, jit_value a2)
{
	if (op->arg_size == REG_SIZE) {
		if (IS_IMM(op)) common86_mov_reg_mem(jit->ip, a1, a2, op->arg_size);
		else common86_mov_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
		return;
	} 

	switch (op->code) {
		case (JIT_LD | IMM | SIGNED): common86_movsx_reg_mem(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_LD | IMM | UNSIGNED): common86_movzx_reg_mem(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_LD | REG | SIGNED): common86_movsx_reg_membase(jit->ip, a1, a2, 0, op->arg_size); break;
		case (JIT_LD | REG | UNSIGNED): common86_movzx_reg_membase(jit->ip, a1, a2, 0, op->arg_size); break;
		default: assert(0);
	}
}

/**
 * Emits all LD operations
 */
static inline void emit_ldx_op(struct jit * jit, jit_op * op, jit_value a1, jit_value a2, jit_value a3)
{
	if (op->arg_size == REG_SIZE) {
		if (IS_IMM(op)) common86_mov_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
		else common86_mov_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
		return;
	} 
	
	switch (op->code) {
		case (JIT_LDX | IMM | SIGNED): common86_movsx_reg_membase(jit->ip, a1, a2, a3, op->arg_size); break;
		case (JIT_LDX | IMM | UNSIGNED): common86_movzx_reg_membase(jit->ip, a1, a2, a3, op->arg_size); break;
		case (JIT_LDX | REG | SIGNED): common86_movsx_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); break;
		case (JIT_LDX | REG | UNSIGNED): common86_movzx_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); break;
		default: assert(0);
	}
}

//
//
// Main switch 
//
//
void jit_gen_op(struct jit * jit, struct jit_op * op)
{
	jit_value a1 = op->r_arg[0];
	jit_value a2 = op->r_arg[1];
	jit_value a3 = op->r_arg[2];
	int imm = IS_IMM(op);
	int sign = IS_SIGNED(op);

	int found = 1;
	
	switch (GET_OP(op)) {
		case JIT_ADD: 
			if ((a1 != a2) && (a1 != a3)) {
				if (imm) common86_lea_membase(jit->ip, a1, a2, a3);
				else common86_lea_memindex(jit->ip, a1, a2, 0, a3, 0);
			} else emit_alu_op(jit, op, X86_ADD, imm); 
			break;

		case JIT_ADDC: 	emit_alu_op(jit, op, X86_ADD, imm); break;
		case JIT_ADDX: 	emit_alu_op(jit, op, X86_ADC, imm); break;
		case JIT_SUB: 	emit_sub_op(jit, op, imm); break;
		case JIT_SUBC: 	emit_subx_op(jit, op, X86_SUB, imm); break; 
		case JIT_SUBX: 	emit_subx_op(jit, op, X86_SBB, imm); break; 
		case JIT_RSB: 	emit_rsb_op(jit, op, imm); break; 
		case JIT_NEG:
				if (a1 != a2) common86_mov_reg_reg(jit->ip, a1, a2, REG_SIZE);
				common86_neg_reg(jit->ip, a1); 
				break;
		case JIT_OR: 	emit_alu_op(jit, op, X86_OR, imm); break; 
		case JIT_XOR: 	emit_alu_op(jit, op, X86_XOR, imm); break; 
		case JIT_AND: 	emit_alu_op(jit, op, X86_AND, imm); break; 
		case JIT_NOT: 	if (a1 != a2) common86_mov_reg_reg(jit->ip, a1, a2, REG_SIZE);
			      	common86_not_reg(jit->ip, a1);
			      	break;
		case JIT_LSH: 	emit_shift_op(jit, op, X86_SHL, imm); break;
		case JIT_RSH: 	emit_shift_op(jit, op, IS_SIGNED(op) ? X86_SAR : X86_SHR, imm); break;

		case JIT_LT: 	emit_cond_op(jit, op, X86_CC_LT, imm, IS_SIGNED(op)); break;
		case JIT_LE: 	emit_cond_op(jit, op, X86_CC_LE, imm, IS_SIGNED(op)); break;
		case JIT_GT: 	emit_cond_op(jit, op, X86_CC_GT, imm, IS_SIGNED(op)); break;
		case JIT_GE: 	emit_cond_op(jit, op, X86_CC_GE, imm, IS_SIGNED(op)); break;
		case JIT_EQ: 	emit_cond_op(jit, op, X86_CC_EQ, imm, IS_SIGNED(op)); break;
		case JIT_NE: 	emit_cond_op(jit, op, X86_CC_NE, imm, IS_SIGNED(op)); break;

		case JIT_BLT: 	emit_branch_op(jit, op, X86_CC_LT, imm, IS_SIGNED(op)); break;
		case JIT_BLE: 	emit_branch_op(jit, op, X86_CC_LE, imm, IS_SIGNED(op)); break;
		case JIT_BGT: 	emit_branch_op(jit, op, X86_CC_GT, imm, IS_SIGNED(op)); break;
		case JIT_BGE: 	emit_branch_op(jit, op, X86_CC_GE, imm, IS_SIGNED(op)); break;
		case JIT_BEQ: 	emit_branch_op(jit, op, X86_CC_EQ, imm, IS_SIGNED(op)); break;
		case JIT_BNE: 	emit_branch_op(jit, op, X86_CC_NE, imm, IS_SIGNED(op)); break;

		case JIT_BMS: 	emit_branch_mask_op(jit, op, X86_CC_NZ, imm); break;
		case JIT_BMC: 	emit_branch_mask_op(jit, op, X86_CC_Z, imm); break;

		case JIT_BOADD: emit_branch_overflow_op(jit, op, X86_ADD, imm); break;
		case JIT_BOSUB: emit_branch_overflow_op(jit, op, X86_SUB, imm); break;

		case JIT_MUL: 	emit_mul_op(jit, op, imm, IS_SIGNED(op), 0); break;
		case JIT_HMUL: 	emit_mul_op(jit, op, imm, IS_SIGNED(op), 1); break;
		case JIT_DIV: 	emit_div_op(jit, op, imm, IS_SIGNED(op), 0); break;
		case JIT_MOD: 	emit_div_op(jit, op, imm, IS_SIGNED(op), 1); break;

		case JIT_CALL: 	__funcall(jit, op, imm); break;
		case JIT_PATCH: do {
					jit_value pa = ((struct jit_op *)a1)->patch_addr;
					common86_patch(jit->buf + pa, jit->ip);
				} while (0);
				break;
		case JIT_JMP:
			op->patch_addr = __PATCH_ADDR(jit);
			if (op->code & REG) common86_jump_reg(jit->ip, a1);
			else common86_jump_disp32(jit->ip, __JIT_GET_ADDR(jit, a1));
			break;
		case JIT_RET:
			if (!imm && (a1 != COMMON86_AX)) common86_mov_reg_reg(jit->ip, COMMON86_AX, a1, REG_SIZE);
			if (imm) common86_mov_reg_imm(jit->ip, COMMON86_AX, a1);
			__pop_callee_saved_regs(jit);
			common86_mov_reg_reg(jit->ip, COMMON86_SP, COMMON86_BP, REG_SIZE);
			common86_pop_reg(jit->ip, COMMON86_BP);
			common86_ret(jit->ip);
			break;

		case JIT_PUTARG: __put_arg(jit, op); break;
		case JIT_FPUTARG: __fput_arg(jit, op); break;
		case JIT_GETARG: __get_arg(jit, op); break;
		case JIT_MSG: 	emit_msg_op(jit, op); break;

		case JIT_LD: 	emit_ld_op(jit, op, a1, a2); break;
		case JIT_LDX: 	emit_ldx_op(jit, op, a1, a2, a3); break;
		case JIT_FST: 	emit_sse_fst_op(jit, op, a1, a2); break;
		case JIT_FSTX: 	emit_sse_fstx_op(jit, op, a1, a2, a3); break;
		case JIT_FLD: 	emit_sse_fld_op(jit, op, a1, a2); break;
		case JIT_FLDX: 	emit_sse_fldx_op(jit, op, a1, a2, a3); break;

		case JIT_ALLOCA: break;
		case JIT_DECL_ARG: break;
		case JIT_RETVAL: break; // reg. allocator takes care of the proper register assignment
		case JIT_LABEL: ((jit_label *)a1)->pos = __PATCH_ADDR(jit); break; 

		default: found = 0;
	}

	if (found) return;


	switch (op->code) {
		case (JIT_MOV | REG): if (a1 != a2) common86_mov_reg_reg(jit->ip, a1, a2, REG_SIZE); break;
		case (JIT_MOV | IMM):
			if (a2 == 0) common86_alu_reg_reg(jit->ip, X86_XOR, a1, a1);
			else common86_mov_reg_imm_size(jit->ip, a1, a2, 8); 
			break;

		case JIT_PREPARE: __prepare_call(jit, op, a1 + a2);
				  jit->push_count += __push_caller_saved_regs(jit, op);
				  break;

		case JIT_PROLOG: emit_prolog_op(jit, op); break;
		case (JIT_ST | IMM): common86_mov_mem_reg(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_ST | REG): common86_mov_membase_reg(jit->ip, a1, 0, a2, op->arg_size); break;
		case (JIT_STX | IMM): common86_mov_membase_reg(jit->ip, a2, a1, a3, op->arg_size); break;
		case (JIT_STX | REG): common86_mov_memindex_reg(jit->ip, a1, 0, a2, 0, a3, op->arg_size); break;

		//
		// Floating-point operations;
		//
		case (JIT_FMOV | REG): sse_movsd_reg_reg(jit->ip, a1, a2); break;
		case (JIT_FMOV | IMM): sse_reg_safeimm(jit, a1, &op->flt_imm); break; 
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

		case (JIT_EXT | REG): sse_cvtsi2sd_reg_reg(jit->ip, a1, a2); break;
                case (JIT_TRUNC | REG): sse_cvttsd2si_reg_reg(jit->ip, a1, a2); break;
		case (JIT_CEIL | REG): __sse_floor(jit, a1, a2, 0); break;
                case (JIT_FLOOR | REG): __sse_floor(jit, a1, a2, 1); break;
		case (JIT_ROUND | REG): __sse_round(jit, a1, a2); break;

		case (JIT_FRET | REG): emit_fret_op(jit, op); break;
		case JIT_FRETVAL: emit_fretval_op(jit, op); break;

		case (JIT_UREG): __ureg(jit, a1, a2); break;
		case (JIT_LREG): __lreg(jit, a1, a2); break;
		case (JIT_SYNCREG):  __ureg(jit, a1, a2); break;
		case JIT_RENAMEREG: common86_mov_reg_reg(jit->ip, a1, a2, REG_SIZE); break;

		case JIT_CODESTART: break;
		case JIT_NOP: break;

		// platform specific opcodes; used by optimizer
		case (JIT_X86_STI | IMM): common86_mov_mem_imm(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_X86_STI | REG): common86_mov_membase_imm(jit->ip, a1, 0, a2, op->arg_size); break;
		case (JIT_X86_STXI | IMM): common86_mov_membase_imm(jit->ip, a2, a1, a3, op->arg_size); break;
		case (JIT_X86_STXI | REG): common86_mov_memindex_imm(jit->ip, a1, 0, a2, 0, a3, op->arg_size); break;

		default: printf("common86: unknown operation (opcode: 0x%x)\n", GET_OP(op) >> 3);
	}
}
