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

struct __hw_reg {
	int id;
	unsigned long used;
	char * name;
};

struct jit_reg_allocator {
	int hw_reg_cnt;
	struct __hw_reg * hw_reg;
	struct __hw_reg ** hwreg_pool;
	int hwreg_pool_pos;
};



#define JIT_X86_STI	(0x0100 << 3)
#define JIT_X86_STXI	(0x0101 << 3)

#define __JIT_GET_ADDR(jit, imm) (!jit_is_label(jit, (void *)(imm)) ? (unsigned char *)imm : (unsigned char *)((jit_label *)imm)->pos)
#define __GET_REG_POS(r) ((- (r) * REG_SIZE) - REG_SIZE)

void jit_dump_registers(struct jit * jit, long * hwregs);

/* platform specific */
static inline int jit_arg(struct jit * jit)
{
	int r = jit->argpos;
	jit->argpos += 1;
	return r;
}

static inline int jit_allocai(struct jit * jit, int size)
{
	int real_size = (size + 15) & 0xfffffff0; // 16-bytes aligned
	jit->allocai_mem += real_size;	
	return -(jit->allocai_mem);
}

static inline jit_label * jit_get_label(struct jit * jit)
{
	jit_label * r = JIT_MALLOC(sizeof(jit_label));
	jit_add_op(jit, JIT_LABEL, SPEC(IMM, NO, NO), (long)r, 0, 0, 0);
	r->next = jit->labels;
	jit->labels = r;
	return r;
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
		int ecx_pathology = 0;

		if (destreg == AMD64_RCX) {
			ecx_pathology = 1;
			amd64_push_reg(jit->ip, AMD64_RDI); /* TODO: test if the EDI is in use, or not */
			destreg = AMD64_RDI;
		}

		if (shiftreg != AMD64_RCX) {
			amd64_push_reg(jit->ip, AMD64_RCX); /* TODO: test if the ECX register is in use, or not */
			amd64_mov_reg_reg(jit->ip, AMD64_RCX, shiftreg, REG_SIZE);
		}
		if (destreg != valreg) amd64_mov_reg_reg(jit->ip, destreg, valreg, REG_SIZE); 
		amd64_shift_reg(jit->ip, shift_op, destreg);
		if (ecx_pathology) {
			amd64_mov_reg_reg(jit->ip, AMD64_RCX, AMD64_RDI, REG_SIZE);
			if (shiftreg != AMD64_RCX) amd64_alu_reg_imm(jit->ip, X86_ADD, AMD64_RSP, 8);
			amd64_pop_reg(jit->ip, AMD64_RDI);
		} else {
			if (shiftreg != AMD64_RCX) amd64_pop_reg(jit->ip, AMD64_RCX);
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

	op->patch_addr = jit->ip;

	amd64_branch(jit->ip, amd64_cond, __JIT_GET_ADDR(jit, op->r_arg[0]), sign);
}

static inline void __branch_mask_op(struct jit * jit, struct jit_op * op, int amd64_cond, int imm)
{
	if (imm) amd64_test_reg_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
	else amd64_test_reg_reg(jit->ip, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = jit->ip;

	//amd64_branch(jit->ip, amd64_cond, (unsigned char *)op->r_arg[0], 0);
	amd64_branch(jit->ip, amd64_cond, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}

static inline void __branch_overflow_op(struct jit * jit, struct jit_op * op, int alu_op, int imm)
{
	if (imm) amd64_alu_reg_imm(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);
	else amd64_alu_reg_reg(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = jit->ip;

	//amd64_branch(jit->ip, X86_CC_O, (unsigned char *)op->r_arg[0], 0);
	amd64_branch(jit->ip, X86_CC_O, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}


/* determines whether the argument value was spilled or not,
 * if the register is associated with the hardware register
 * it is returned through the reg argument
 */
static inline int __is_spilled(int arg_id, jit_op * prepare_op, int * reg)
{
	int args = prepare_op->arg[0];
	struct __hw_reg * hreg = prepare_op->regmap[arg_id];
	if ((!hreg)
	|| ((hreg->id == AMD64_RDI) && (args > 0))
	|| ((hreg->id == AMD64_RSI) && (args > 1))
	|| ((hreg->id == AMD64_RDX) && (args > 2))
	|| ((hreg->id == AMD64_RCX) && (args > 3))
	|| ((hreg->id == AMD64_R8) && (args > 4))
	|| ((hreg->id == AMD64_R9) && (args > 5)))
		return 1;

	*reg = hreg->id;
	return 0;
}

static inline void __funcall(struct jit * jit, struct jit_op * op, int imm, int cleanup)
{
	int pos, i, sreg;

	jit_op * prepare = op;
	while (GET_OP(prepare) != JIT_PREPARE)
		prepare = prepare->prev;

	for (i = jit->prepare_args - 1, pos = 0; i >= 0; i--, pos++) {
		int reg;
		if (pos < 6) {
			switch (pos) {
				case 0: reg = AMD64_RDI; break;
				case 1: reg = AMD64_RSI; break;
				case 2: reg = AMD64_RDX; break;
				case 3: reg = AMD64_RCX; break;
				case 4: reg = AMD64_R8; break;
				case 5: reg = AMD64_R9; break;
			}

			if (jit->args[i].isreg) {
				if (__is_spilled(jit->args[i].value, prepare, &sreg)) {
					amd64_mov_reg_membase(jit->ip, reg, AMD64_RBP, __GET_REG_POS(jit->args[i].value), REG_SIZE);
				} else {
					amd64_mov_reg_reg(jit->ip, reg, sreg, REG_SIZE);
				}
			} else amd64_mov_reg_imm_size(jit->ip, reg, jit->args[i].value, 8);

		} else {
			int x = pos - 6;
			if (jit->args[x].isreg) {
				if (__is_spilled(jit->args[x].value, prepare, &sreg)) {
					amd64_push_membase(jit->ip, AMD64_RBP,  __GET_REG_POS(jit->args[x].value));
				} else {
					amd64_push_reg(jit->ip, sreg);
				}

			} else amd64_push_imm(jit->ip, jit->args[x].value);
		}
	}
	/* al is used to pass number of floating point arguments */
	amd64_alu_reg_reg(jit->ip, X86_XOR, AMD64_RAX, AMD64_RAX);
	if (!imm) {
		amd64_call_reg(jit->ip, op->r_arg[0]);
	} else {
		op->patch_addr = jit->ip;
		amd64_call_code(jit->ip, __JIT_GET_ADDR(jit, op->r_arg[0]));
	}
	if (cleanup) {
		if (jit->prepare_args > 6) {
			amd64_alu_reg_imm(jit->ip, X86_ADD, AMD64_RSP, (jit->prepare_args - 6) * PTR_SIZE);
			JIT_FREE(jit->args);
		}
		jit->prepare_args = 0;
	}
	/* pops caller saved registers */
	for (int i = jit->reg_count - 1; i >= 2; i--) {
		if (!jitset_get(op->live_in, i)) continue;
		if (op->regmap[i]) {
			if (op->regmap[i]->id == AMD64_RCX) amd64_pop_reg(jit->ip, AMD64_RCX);
			if (op->regmap[i]->id == AMD64_RDX) amd64_pop_reg(jit->ip, AMD64_RDX);
			if (op->regmap[i]->id == AMD64_RSI) amd64_pop_reg(jit->ip, AMD64_RSI);
			if (op->regmap[i]->id == AMD64_RDI) amd64_pop_reg(jit->ip, AMD64_RDI);
			if (op->regmap[i]->id == AMD64_R8) amd64_pop_reg(jit->ip, AMD64_R8);
			if (op->regmap[i]->id == AMD64_R9) amd64_pop_reg(jit->ip, AMD64_R9);

			if (op->regmap[i]->id == AMD64_R10) amd64_pop_reg(jit->ip, AMD64_R10);
			if (op->regmap[i]->id == AMD64_R11) amd64_pop_reg(jit->ip, AMD64_R11);
		}
	}
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

	if (dest != AMD64_RAX) amd64_push_reg(jit->ip, AMD64_RAX);
	if (dest != AMD64_RDX) amd64_push_reg(jit->ip, AMD64_RDX);

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

	if (dest != AMD64_RDX) amd64_pop_reg(jit->ip, AMD64_RDX);
	if (dest != AMD64_RAX) amd64_pop_reg(jit->ip, AMD64_RAX);
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

	if (dest != AMD64_RAX) amd64_push_reg(jit->ip, AMD64_RAX);
	if (dest != AMD64_RDX) amd64_push_reg(jit->ip, AMD64_RDX);

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

	if (dest != AMD64_RDX) amd64_pop_reg(jit->ip, AMD64_RDX);
	if (dest != AMD64_RAX) amd64_pop_reg(jit->ip, AMD64_RAX);
}


static inline int __uses_hw_reg(struct jit_op * op, long reg)
{
	for (int i = 0; i < 3; i++)
		if ((ARG_TYPE(op, i + 1) == REG) || (ARG_TYPE(op, i + 1) == TREG)) {
			if (op->r_arg[i] == reg) return 1;
		}
	return 0;
}


static inline void __push_callee_saved_regs(struct jit * jit, struct jit_op * op)
{
	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, AMD64_RBX)) { amd64_push_reg(jit->ip, AMD64_RBX); break; }
	}

	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, AMD64_R12)) { amd64_push_reg(jit->ip, AMD64_R12); break; }
	}
	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, AMD64_R13)) { amd64_push_reg(jit->ip, AMD64_R13); break; }
	}
	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, AMD64_R14)) { amd64_push_reg(jit->ip, AMD64_R14); break; }
	}
	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, AMD64_R15)) { amd64_push_reg(jit->ip, AMD64_R15); break; }
	}
}

static inline void __pop_callee_saved_regs(struct jit * jit)
{
	struct jit_op * op = jit->current_func;


	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, AMD64_R15)) { amd64_pop_reg(jit->ip, AMD64_R15); break; }
	}
	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, AMD64_R14)) { amd64_pop_reg(jit->ip, AMD64_R14); break; }
	}
	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, AMD64_R13)) { amd64_pop_reg(jit->ip, AMD64_R13); break; }
	}
	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, AMD64_R12)) { amd64_pop_reg(jit->ip, AMD64_R12); break; }
	}
	
	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, AMD64_RBX)) { amd64_pop_reg(jit->ip, AMD64_RBX); break; }
	}
}

static inline void __push_caller_saved_regs(struct jit * jit, jit_op * op)
{
	while (op) {
		if ((GET_OP(op) == JIT_CALL) || (GET_OP(op) == JIT_FINISH)) break;
		op = op->next;
	}
	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++) {
		if (!jitset_get(op->live_in, i)) continue;
		if (op->regmap[i]) {
			if (op->regmap[i]->id == AMD64_RCX) amd64_push_reg(jit->ip, AMD64_RCX);
			if (op->regmap[i]->id == AMD64_RDX) amd64_push_reg(jit->ip, AMD64_RDX);
			if (op->regmap[i]->id == AMD64_RSI) amd64_push_reg(jit->ip, AMD64_RSI);
			if (op->regmap[i]->id == AMD64_RDI) amd64_push_reg(jit->ip, AMD64_RDI);
			if (op->regmap[i]->id == AMD64_R8) amd64_push_reg(jit->ip, AMD64_R8);
			if (op->regmap[i]->id == AMD64_R9) amd64_push_reg(jit->ip, AMD64_R9);
			if (op->regmap[i]->id == AMD64_R10) amd64_push_reg(jit->ip, AMD64_R10);
			if (op->regmap[i]->id == AMD64_R11) amd64_push_reg(jit->ip, AMD64_R11);
		}
	}
}

void __get_arg(struct jit * jit, jit_op * op, int reg)
{
	int arg_id = op->r_arg[1];
	int reg_id = arg_id + JIT_FIRST_REG + 1; /* 1 -- is JIT_IMMREG */
	int dreg = op->r_arg[0];

	if (op->regmap[reg_id] == NULL) {
		/* the register is not associated and the value has to be read from the memory */
		int reg_offset = __GET_REG_POS(reg_id);
		if (op->arg_size == REG_SIZE) amd64_mov_reg_membase(jit->ip, dreg, AMD64_RBP, reg_offset, REG_SIZE);
		else if (IS_SIGNED(op)) amd64_movsx_reg_membase(jit->ip, dreg, AMD64_RBP, reg_offset, op->arg_size);
		else {
			amd64_alu_reg_reg(jit->ip, X86_XOR, dreg, dreg); // register cleanup
			amd64_mov_reg_membase(jit->ip, dreg, AMD64_RBP, reg_offset, op->arg_size);
		}
		return;
	}

	if (op->arg_size == REG_SIZE) amd64_mov_reg_reg(jit->ip, dreg, reg, 8);
	else if (IS_SIGNED(op)) amd64_movsx_reg_reg(jit->ip, dreg, reg, op->arg_size);
	else amd64_movzx_reg_reg(jit->ip, dreg, reg, op->arg_size);
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

		case JIT_CALL: __funcall(jit, op, IS_IMM(op), 0); break;
		case JIT_FINISH: __funcall(jit, op, IS_IMM(op), 1); break;

		case JIT_PATCH: amd64_patch(((struct jit_op *)a1)->patch_addr, jit->ip); break;
		case JIT_JMP:
			op->patch_addr = jit->ip;
			if (op->code & REG) amd64_jump_reg(jit->ip, a1);
			else amd64_jump_code(jit->ip, __JIT_GET_ADDR(jit, a1));
			break;
		case JIT_RET:
			if (!IS_IMM(op) && (a1 != AMD64_RAX)) amd64_mov_reg_reg(jit->ip, AMD64_RAX, a1, 8);
			if (IS_IMM(op)) amd64_mov_reg_imm(jit->ip, AMD64_RAX, a1);
			__pop_callee_saved_regs(jit);
			amd64_mov_reg_reg(jit->ip, AMD64_RSP, AMD64_RBP, 8);
			amd64_pop_reg(jit->ip, AMD64_RBP);
			amd64_ret(jit->ip);
			break;

		case JIT_GETARG:
			if (a2 == 0) __get_arg(jit, op, AMD64_RDI);
			if (a2 == 1) __get_arg(jit, op, AMD64_RSI);
			if (a2 == 2) __get_arg(jit, op, AMD64_RDX);
			if (a2 == 3) __get_arg(jit, op, AMD64_RCX);
			if (a2 == 4) __get_arg(jit, op, AMD64_R8);
			if (a2 == 5) __get_arg(jit, op, AMD64_R9);
			if (a2 > 5) {
				if (op->arg_size == REG_SIZE) amd64_mov_reg_membase(jit->ip, a1, AMD64_RBP, 8 + (a2 - 5) * 8, REG_SIZE); 
				else if (IS_SIGNED(op)) {
					amd64_movsx_reg_membase(jit->ip, a1, AMD64_RBP, 8 + (a2 - 5) * 8, op->arg_size);
				} else {
					amd64_alu_reg_reg(jit->ip, X86_XOR, a1, a1); // register cleanup
					amd64_mov_reg_membase(jit->ip, a1, AMD64_RBP, 8 + (a2 - 5) * 8, op->arg_size); 
				}

			}
			break;
			

		default: found = 0;
	}

	if (found) return;


	switch (op->code) {
		case (JIT_MOV | REG): if (a1 != a2) amd64_mov_reg_reg(jit->ip, a1, a2, REG_SIZE); break;
		case (JIT_MOV | IMM):
			if (a2 == 0) amd64_alu_reg_reg(jit->ip, X86_XOR, a1, a1);
			//else amd64_mov_reg_imm(jit->ip, a1, a2); 
			else amd64_mov_reg_imm_size(jit->ip, a1, a2, 8); 
			break;

		case JIT_PREPARE: jit->prepare_args = a1; 
				  jit->argspos = 0;
				  jit->args = JIT_MALLOC(sizeof(struct arg) * a1);
				  __push_caller_saved_regs(jit, op);
				  break;

		case JIT_PUSHARG | REG: jit->args[jit->argspos].isreg = 1;
				  	jit->args[jit->argspos++].value = op->arg[0];
					break;

		case JIT_PUSHARG | IMM: jit->args[jit->argspos].isreg = 0;
				  	jit->args[jit->argspos++].value = op->arg[0];
					break;

		case JIT_PROLOG:
			*(void **)(a1) = jit->ip;
			amd64_push_reg(jit->ip, AMD64_RBP);
			amd64_mov_reg_reg(jit->ip, AMD64_RBP, AMD64_RSP, 8);
			if (jit->allocai_mem) amd64_alu_reg_imm(jit->ip, X86_SUB, AMD64_RSP, jit->allocai_mem);
			__push_callee_saved_regs(jit, op);
			break;

		case JIT_RETVAL: 
			if (a1 != AMD64_RAX) amd64_mov_reg_reg(jit->ip, a1, AMD64_RAX, REG_SIZE);
			break;

		case JIT_LABEL: ((jit_label *)a1)->pos = jit->ip; break;

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

		case (JIT_UREG): amd64_mov_membase_reg(jit->ip, AMD64_RBP, __GET_REG_POS(a1), a2, REG_SIZE); break;
		case (JIT_LREG): amd64_mov_reg_membase(jit->ip, a1, AMD64_RBP, __GET_REG_POS(a2), REG_SIZE); break;
		case (JIT_SYNCREG): amd64_mov_membase_reg(jit->ip, AMD64_RBP, __GET_REG_POS(a1), a2, REG_SIZE);  break;
		case JIT_CODESTART: break;
		case JIT_NOP: break;
		case JIT_DUMPREG: 
				    // creates an array of values
				    amd64_push_reg(jit->ip, AMD64_RBP);
				    amd64_push_reg(jit->ip, AMD64_RDI);
				    amd64_push_reg(jit->ip, AMD64_RSI);
				    amd64_push_reg(jit->ip, AMD64_RDX);
				    amd64_push_reg(jit->ip, AMD64_RCX);
				    amd64_push_reg(jit->ip, AMD64_RBX);
				    amd64_push_reg(jit->ip, AMD64_RAX);
	
				    // push the 2nd argument
				    amd64_push_reg(jit->ip, AMD64_RSP);

				    // push the 1st argument	
				    amd64_mov_reg_imm(jit->ip, AMD64_RAX, jit);
				    amd64_push_reg(jit->ip, AMD64_RAX);

				    amd64_call_code(jit->ip, (long)jit_dump_registers);
				    amd64_alu_reg_imm(jit->ip, X86_ADD, AMD64_RSP, sizeof(long) * 2);

				    amd64_push_reg(jit->ip, AMD64_RAX);
				    amd64_push_reg(jit->ip, AMD64_RBX);
				    amd64_push_reg(jit->ip, AMD64_RCX);
				    amd64_push_reg(jit->ip, AMD64_RDX);
				    amd64_push_reg(jit->ip, AMD64_RSI);
				    amd64_push_reg(jit->ip, AMD64_RDI);
				    amd64_push_reg(jit->ip, AMD64_RBP);
				    break;
		// platform specific opcodes; used by the optimizer
		case (JIT_X86_STI | IMM): amd64_mov_mem_imm(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_X86_STI | REG): amd64_mov_membase_imm(jit->ip, a1, 0, a2, op->arg_size); break;
		case (JIT_X86_STXI | IMM): amd64_mov_membase_imm(jit->ip, a2, a1, a3, op->arg_size); break;
		case (JIT_X86_STXI | REG): amd64_mov_memindex_imm(jit->ip, a1, 0, a2, 0, a3, op->arg_size); break;

		default: printf("amd64: unknown operation (opcode: 0x%x)\n", GET_OP(op));
	}
}


/* platform specific */
void jit_dump_registers(struct jit * jit, long * hwregs)
{
	// FIXME: missing
	fprintf(stderr, "We are very sorry but this function is out of order now.");
}

void jit_optimize(struct jit * jit)
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

void jit_correct_long_imms(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if (!IS_IMM(op)) continue;
		int imm_arg;
		for (int i = 1; i < 4; i++)
			if (ARG_TYPE(op, i) == IMM) imm_arg = i - 1;
		long value = op->arg[imm_arg];
		
		int transform = 0;
		long high_bits = (value & 0xffffffff80000000) >> 31;
		if (IS_SIGNED(op)) {
			if ((high_bits != 0) && (high_bits != 0x1ffffffff)) transform = 1; 
		} else {
			if (high_bits != 0) transform = 1;
		}

		if (transform) {
			jit_op * newop = __new_op(JIT_MOV | IMM, SPEC(TREG, IMM, NO), JIT_IMMREG, value, 0, REG_SIZE);
			jit_op_prepend(op, newop);

			op->code &= ~(0x3);
			op->code |= REG;

			op->spec &= ~(0x3 << (2 * imm_arg));
			op->spec |=  (REG << (2 * imm_arg));
			op->arg[imm_arg] = JIT_IMMREG;
		}
	}
}
