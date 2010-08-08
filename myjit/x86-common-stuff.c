/*
 * MyJIT 
 * Copyright (C) 2010 Petr Krajca, <krajcap@inf.upol.cz>
 *
 * Common stuff for i386 and AMD64 platforms
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

#define JIT_X86_STI     (0x0100 << 3)
#define JIT_X86_STXI    (0x0101 << 3)

//
//
// Registers
//
//

static inline void __push_reg(struct jit * jit, struct __hw_reg * r);
static inline void __pop_reg(struct jit * jit, struct __hw_reg * r);

static inline int __uses_hw_reg(struct jit_op * op, long reg, int fp)
{
	for (int i = 0; i < 3; i++)
		if ((ARG_TYPE(op, i + 1) == REG) || (ARG_TYPE(op, i + 1) == TREG)) {
			if (fp && (op->arg[i] >= 0)) continue;
			if (!fp && (op->arg[i] < 0)) continue;
			if (op->r_arg[i] == reg) return 1;
		}
	return 0;
}

static inline void __push_callee_saved_regs(struct jit * jit, struct jit_op * op)
{
	for (int i = 0; i < jit->reg_al->gp_reg_cnt; i++) {
		struct __hw_reg * r = &(jit->reg_al->gp_regs[i]);
		if (r->callee_saved) 
			for (struct jit_op * o = op->next; o != NULL; o = o->next) {
				if (GET_OP(o) == JIT_PROLOG) break;
				if (__uses_hw_reg(o, r->id, 0)) {
					__push_reg(jit, r);
					break;
				}
			}
	}
}

static inline void __pop_callee_saved_regs(struct jit * jit)
{
	struct jit_op * op = jit->current_func;

	for (int i = jit->reg_al->gp_reg_cnt - 1; i >= 0; i--) {
		struct __hw_reg * r = &(jit->reg_al->gp_regs[i]);
		if (r->callee_saved) 
			for (struct jit_op * o = op->next; o != NULL; o = o->next) {
				if (GET_OP(o) == JIT_PROLOG) break;
				if (__uses_hw_reg(o, r->id, 0)) {
					__pop_reg(jit, r);
					break;
				}
			}
	}
}

static inline void __generic_push_caller_saved_regs(struct jit * jit, jit_op * op, int reg_count,
						    struct __hw_reg * regs, int fp, int skip_reg)
{
	int reg;
	for (int i = 0; i < reg_count; i++) {
		if ((regs[i].id == skip_reg) || (regs[i].callee_saved)) continue;
		struct __hw_reg * hreg = rmap_is_associated(op->regmap, regs[i].id, fp, &reg);
		if (hreg && jitset_get(op->live_in, reg)) __push_reg(jit, hreg);
	}
}

static inline void __push_caller_saved_regs(struct jit * jit, jit_op * op)
{
	while (op) {
		if (GET_OP(op) == JIT_CALL) break;
		op = op->next;
	}
	__generic_push_caller_saved_regs(jit, op, jit->reg_al->gp_reg_cnt, jit->reg_al->gp_regs, 0, jit->reg_al->ret_reg);
	__generic_push_caller_saved_regs(jit, op, jit->reg_al->fp_reg_cnt, jit->reg_al->fp_regs, 1, jit->reg_al->fpret_reg);
}

static inline void __generic_pop_caller_saved_regs(struct jit * jit, jit_op * op, int reg_count,
						    struct __hw_reg * regs, int fp, int skip_reg)
{
	int reg;
	for (int i = reg_count - 1; i >= 0; i--) {
		if ((regs[i].id == skip_reg) || (regs[i].callee_saved)) continue;
		struct __hw_reg * hreg = rmap_is_associated(op->regmap, regs[i].id, fp, &reg);
		if (hreg && jitset_get(op->live_in, reg)) __pop_reg(jit, hreg);
	}
}

static inline void __pop_caller_saved_regs(struct jit * jit, jit_op * op)
{
	__generic_pop_caller_saved_regs(jit, op, jit->reg_al->fp_reg_cnt, jit->reg_al->fp_regs, 1, jit->reg_al->fpret_reg);
	__generic_pop_caller_saved_regs(jit, op, jit->reg_al->gp_reg_cnt, jit->reg_al->gp_regs, 0, jit->reg_al->ret_reg);
}

//
//
// SSE2 
//
//

static inline void __sse_change_sign(struct jit * jit, long reg);

static inline void __sse_alu_op(struct jit * jit, jit_op * op, int sse_op)
{
	if (op->r_arg[0] == op->r_arg[1]) {
		x86_sse_alu_sd_reg_reg(jit->ip, sse_op, op->r_arg[0], op->r_arg[2]);
	} else if (op->r_arg[0] == op->r_arg[2]) {
		x86_sse_alu_sd_reg_reg(jit->ip, sse_op, op->r_arg[0], op->r_arg[1]);
	} else {
		x86_movsd_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1]);
		x86_sse_alu_sd_reg_reg(jit->ip, sse_op, op->r_arg[0], op->r_arg[2]);
	}
}

static inline unsigned char * __sse_get_sign_mask()
{
	// gets 16-bytes aligned value
	static unsigned char bufx[32];
	unsigned char * buf = bufx + 1;
	while ((long)buf % 16) buf++;
	unsigned long long * bit_mask = (unsigned long long *)buf;

	// inverts 64th (sing) bit
	*bit_mask = (unsigned long long)1 << 63;
	return buf;
}

static inline void __sse_sub_op(struct jit * jit, long a1, long a2, long a3)
{
	if (a1 == a2) {
		x86_sse_alu_sd_reg_reg(jit->ip, X86_SSE_SUB, a1, a3);
	} else if (a1 == a3) {
		x86_sse_alu_sd_reg_reg(jit->ip, X86_SSE_SUB, a1, a2);
		__sse_change_sign(jit, a1);
	} else {
		x86_movsd_reg_reg(jit->ip, a1, a2);
		x86_sse_alu_sd_reg_reg(jit->ip, X86_SSE_SUB, a1, a3);
	}
}

static inline void __sse_div_op(struct jit * jit, long a1, long a2, long a3)
{
	if (a1 == a2) {
		x86_sse_alu_sd_reg_reg(jit->ip, X86_SSE_DIV, a1, a3);
	} else if (a1 == a3) {
		// creates a copy of the a2 into high bits of a2
		x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);

		// divides a2 by a3 and moves to the results
		x86_sse_alu_sd_reg_reg(jit->ip, X86_SSE_DIV, a2, a3);
		x86_movsd_reg_reg(jit->ip, a1, a2); 

		// returns the the value of a2
		x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
	} else {
		x86_movsd_reg_reg(jit->ip, a1, a2); 
		x86_sse_alu_sd_reg_reg(jit->ip, X86_SSE_DIV, a1, a3);
	}
}

static inline void __sse_neg_op(struct jit * jit, long a1, long a2)
{
	if (a1 != a2) x86_movsd_reg_reg(jit->ip, a1, a2); 
	__sse_change_sign(jit, a1);
}

static inline void __sse_branch(struct jit * jit, jit_op * op, long a1, long a2, long a3, int x86_cond)
{
        x86_sse_alu_pd_reg_reg(jit->ip, X86_SSE_COMI, a2, a3);
        op->patch_addr = __PATCH_ADDR(jit);
        x86_branch_disp(jit->ip, x86_cond, __JIT_GET_ADDR(jit, a1), 0);
}

//
// 
// Optimizations
//
//
void jit_optimize_st_ops(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if ((GET_OP(op) == JIT_ST)
		&& (op->prev)
		&& (op->prev->code == (JIT_MOV | IMM))
		&& (op->arg[1] == op->prev->arg[0])
		&& (!jitset_get(op->live_out, op->arg[1])))
		{
			if (!IS_IMM(op)) {
				op->code = JIT_X86_STI | REG;
				op->spec = SPEC(REG, IMM, NO);
			} else {
				op->code = JIT_X86_STI | IMM;
				op->spec = SPEC(IMM, IMM, NO);
			}
			op->arg[1] = op->prev->arg[1];
			op->prev->code = JIT_NOP;
			op->prev->spec = SPEC(NO, NO, NO);
		}
		
		if ((GET_OP(op) == JIT_STX)
		&& (op->prev)
		&& (op->prev->code == (JIT_MOV | IMM))
		&& (op->arg[2] == op->prev->arg[0])
		&& (!jitset_get(op->live_out, op->arg[2])))
		{
			if (!IS_IMM(op)) {
				op->code = JIT_X86_STXI | REG;
				op->spec = SPEC(REG, REG, IMM);
			} else {
				op->code = JIT_X86_STXI | IMM;
				op->spec = SPEC(IMM, REG, IMM);
			}
			op->arg[2] = op->prev->arg[1];
			op->prev->code = JIT_NOP;
			op->prev->spec = SPEC(NO, NO, NO);
		}

	}
}
