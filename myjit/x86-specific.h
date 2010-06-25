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

#include "x86-codegen.h"

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

#define __JIT_GET_ADDR(jit, imm) (!jit_is_label(jit, (void *)(imm)) ? (imm) :  \
		(((long)jit->buf + ((jit_label *)(imm))->pos - (long)jit->ip)))
#define __GET_REG_POS(r) ((- r * REG_SIZE) - REG_SIZE)
#define __PATCH_ADDR(jit)	((long)jit->ip - (long)jit->buf)

void jit_dump_registers(struct jit * jit, long * hwregs);

/* platform specific */
static inline int jit_arg(struct jit * jit)
{
	int r = jit->argpos;
	jit->argpos += REG_SIZE;
	return r;
}

static inline int jit_allocai(struct jit * jit, int size)
{
	int real_size = (size + 3) & 0xfffffffc; // 4-bytes aligned
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

static inline void __alu_op(struct jit * jit, struct jit_op * op, int x86_op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) x86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		x86_alu_reg_imm(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);

	}  else {
		if (op->r_arg[0] == op->r_arg[1]) {
			x86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
		} else if (op->r_arg[0] == op->r_arg[2]) {
			x86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[1]);
		} else {
			x86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
			x86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
		}	
	}
}

static inline void __sub_op(struct jit * jit, struct jit_op * op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) x86_lea_membase(jit->ip, op->r_arg[0], op->r_arg[1], -op->r_arg[2]);
		else x86_alu_reg_imm(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
		return;

	}
	if (op->r_arg[0] == op->r_arg[1]) {
		x86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
	} else if (op->r_arg[0] == op->r_arg[2]) {
		x86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
		x86_neg_reg(jit->ip, op->r_arg[0]);
	} else {
		x86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		x86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
	}	
}

static inline void __subx_op(struct jit * jit, struct jit_op * op, int x86_op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) x86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		x86_alu_reg_imm(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
		return;

	}
	if (op->r_arg[0] == op->r_arg[1]) {
		x86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
	} else if (op->r_arg[0] == op->r_arg[2]) {
		x86_push_reg(jit->ip, op->r_arg[2]);
		x86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		x86_alu_reg_membase(jit->ip, x86_op, op->r_arg[0], X86_ESP, 0);
		x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, 4);
	} else {
		x86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		x86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
	}	
}

static inline void __rsb_op(struct jit * jit, struct jit_op * op, int imm)
{
	if (imm) {
		if (op->r_arg[0] == op->r_arg[1]) x86_alu_reg_imm(jit->ip, X86_ADD, op->r_arg[0], -op->r_arg[2]);
		else x86_lea_membase(jit->ip, op->r_arg[0], op->r_arg[1], -op->r_arg[2]);
		x86_neg_reg(jit->ip, op->r_arg[0]);
		return;
	}

	if (op->r_arg[0] == op->r_arg[1]) { // O1 = O3 - O1
		x86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
		x86_neg_reg(jit->ip, op->r_arg[0]);
	} else if (op->r_arg[0] == op->r_arg[2]) { // O1 = O1 - O2
		x86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
	} else {
		x86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[2], REG_SIZE);
		x86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
	}
}

static inline void __shift_op(struct jit * jit, struct jit_op * op, int shift_op, int imm)
{
	if (imm) { 
		if (op->r_arg[0] != op->r_arg[1]) x86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		x86_shift_reg_imm(jit->ip, shift_op, op->r_arg[0], op->r_arg[2]);
	} else {
		int destreg = op->r_arg[0];
		int valreg = op->r_arg[1];
		int shiftreg = op->r_arg[2];
		int ecx_pathology = 0;

		if (destreg == X86_ECX) {
			ecx_pathology = 1;
			x86_push_reg(jit->ip, X86_EDI); /* TODO: test if the EDI is in use, or not */
			destreg = X86_EDI;
		}

		if (shiftreg != X86_ECX) {
			x86_push_reg(jit->ip, X86_ECX); /* TODO: test if the ECX register is in use, or not */
			x86_mov_reg_reg(jit->ip, X86_ECX, shiftreg, REG_SIZE);
		}
		if (destreg != valreg) x86_mov_reg_reg(jit->ip, destreg, valreg, REG_SIZE); 
		x86_shift_reg(jit->ip, shift_op, destreg);
		if (ecx_pathology) {
			x86_mov_reg_reg(jit->ip, X86_ECX, X86_EDI, REG_SIZE);
			if (shiftreg != X86_ECX) x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, 4);
			x86_pop_reg(jit->ip, X86_EDI);
		} else {
			if (shiftreg != X86_ECX) x86_pop_reg(jit->ip, X86_ECX);
		}
	}
}

static inline void __cond_op(struct jit * jit, struct jit_op * op, int x86_cond, int imm, int sign)
{
	if (imm) x86_cmp_reg_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
	else x86_cmp_reg_reg(jit->ip, op->r_arg[1], op->r_arg[2]);
	if ((op->r_arg[0] != X86_ESI) && (op->r_arg[0] != X86_EDI)) {
		x86_mov_reg_imm(jit->ip, op->r_arg[0], 0);
		x86_set_reg(jit->ip, x86_cond, op->r_arg[0], sign);
	} else {
		x86_xchg_reg_reg(jit->ip, X86_EAX, op->r_arg[0], REG_SIZE);
		x86_mov_reg_imm(jit->ip, X86_EAX, 0);
		x86_set_reg(jit->ip, x86_cond, X86_EAX, sign);
		x86_xchg_reg_reg(jit->ip, X86_EAX, op->r_arg[0], REG_SIZE);
	}
}

static inline void __branch_op(struct jit * jit, struct jit_op * op, int x86_cond, int imm, int sign)
{
	if (imm) x86_cmp_reg_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
	else x86_cmp_reg_reg(jit->ip, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	//x86_branch(jit->ip, x86_cond, (unsigned char *)op->r_arg[0], sign);
	x86_branch_disp(jit->ip, x86_cond, __JIT_GET_ADDR(jit, op->r_arg[0]), sign);
}

static inline void __branch_mask_op(struct jit * jit, struct jit_op * op, int x86_cond, int imm)
{
	if (imm) x86_test_reg_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
	else x86_test_reg_reg(jit->ip, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	//x86_branch(jit->ip, x86_cond, (unsigned char *)op->r_arg[0], 0);
	x86_branch_disp(jit->ip, x86_cond, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}

static inline void __branch_overflow_op(struct jit * jit, struct jit_op * op, int alu_op, int imm)
{
	if (imm) x86_alu_reg_imm(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);
	else x86_alu_reg_reg(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit); 

	//x86_branch(jit->ip, X86_CC_O, (unsigned char *)op->r_arg[0], 0);
	x86_branch_disp(jit->ip, X86_CC_O, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}


static inline void __funcall(struct jit * jit, struct jit_op * op, int imm, int cleanup)
{
	if (!imm) {
		x86_call_reg(jit->ip, op->r_arg[0]);
	} else {
		op->patch_addr = __PATCH_ADDR(jit); 
		//x86_call_code(jit->ip, __JIT_GET_ADDR(jit, op->r_arg[0]));
		x86_call_imm(jit->ip, __JIT_GET_ADDR(jit, op->r_arg[0]) - 4); /* 4: magic constant */
	}
	if (cleanup) {
		if (jit->prepare_args) 
			x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, jit->prepare_args * PTR_SIZE);
		jit->prepare_args = 0;
	}
	/* pops caller saved registers */
	for (int i = jit->reg_count - 1; i >= 0; i--) {
		if (!jitset_get(op->live_in, R(i))) continue;
		if (op->regmap[R(i)]) {
			if (op->regmap[R(i)]->id == X86_ECX) x86_pop_reg(jit->ip, X86_ECX);
			if (op->regmap[R(i)]->id == X86_EDX) x86_pop_reg(jit->ip, X86_EDX);
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
			case 2: if (factor1 == dest) x86_shift_reg_imm(jit->ip, X86_SHL, dest, 1);
				else x86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 0);
				return;

			case 3: x86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 1); return;

			case 4: if (factor1 != dest) x86_mov_reg_reg(jit->ip, dest, factor1, REG_SIZE);
				x86_shift_reg_imm(jit->ip, X86_SHL, dest, 2);
				return;
			case 5: x86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 2);
				return;
			case 8: if (factor1 != dest) x86_mov_reg_reg(jit->ip, dest, factor1, REG_SIZE);
				x86_shift_reg_imm(jit->ip, X86_SHL, dest, 3);
				return;
			case 9: x86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 3);
				return;
		}
	}


	/* generic multiplication */

	if (dest != X86_EAX) x86_push_reg(jit->ip, X86_EAX);
	if (dest != X86_EDX) x86_push_reg(jit->ip, X86_EDX);

	if (imm) {
		if (factor1 != X86_EAX) x86_mov_reg_reg(jit->ip, X86_EAX, factor1, REG_SIZE);
		x86_mov_reg_imm(jit->ip, X86_EDX, factor2);
		x86_mul_reg(jit->ip, X86_EDX, sign);
	} else {
		if (factor1 == X86_EAX) x86_mul_reg(jit->ip, factor2, sign);
		else if (factor2 == X86_EAX) x86_mul_reg(jit->ip, factor1, sign);
		else {
			x86_mov_reg_reg(jit->ip, X86_EAX, factor1, REG_SIZE);
			x86_mul_reg(jit->ip, factor2, sign);
		}
	}

	if (!high_bytes) {
		if (dest != X86_EAX) x86_mov_reg_reg(jit->ip, dest, X86_EAX, REG_SIZE);
	} else {
		if (dest != X86_EDX) x86_mov_reg_reg(jit->ip, dest, X86_EDX, REG_SIZE);
	}

	if (dest != X86_EDX) x86_pop_reg(jit->ip, X86_EDX);
	if (dest != X86_EAX) x86_pop_reg(jit->ip, X86_EAX);
}
static inline void __div(struct jit * jit, struct jit_op * op, int imm, int sign, int modulo)
{
	long dest = op->r_arg[0];
	long dividend = op->r_arg[1];
	long divisor = op->r_arg[2];

	if (imm && ((divisor == 2) || (divisor == 4) || (divisor == 8))) {
		if (dest != dividend) x86_mov_reg_reg(jit->ip, dest, dividend, REG_SIZE);
		if (!modulo) {
			switch (divisor) {
				case 2: x86_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 1); break;
				case 4: x86_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 2); break;
				case 8: x86_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 3); break;
			}
		} else {
			switch (divisor) {
				case 2: x86_alu_reg_imm(jit->ip, X86_AND, dest, 0x1); break;
				case 4: x86_alu_reg_imm(jit->ip, X86_AND, dest, 0x3); break;
				case 8: x86_alu_reg_imm(jit->ip, X86_AND, dest, 0x7); break;
			}
		}
		return;
	}

	if (dest != X86_EAX) x86_push_reg(jit->ip, X86_EAX);
	if (dest != X86_EDX) x86_push_reg(jit->ip, X86_EDX);

	if (imm) {
		if (dividend != X86_EAX) x86_mov_reg_reg(jit->ip, X86_EAX, dividend, REG_SIZE);
		x86_cdq(jit->ip);
		if (dest != X86_EBX) x86_push_reg(jit->ip, X86_EBX);
		x86_mov_reg_imm(jit->ip, X86_EBX, divisor);
		x86_div_reg(jit->ip, X86_EBX, sign);
		if (dest != X86_EBX) x86_pop_reg(jit->ip, X86_EBX);
	} else {
		if ((divisor == X86_EAX) || (divisor == X86_EDX)) {
			x86_push_reg(jit->ip, divisor);
		}

		if (dividend != X86_EAX) x86_mov_reg_reg(jit->ip, X86_EAX, dividend, REG_SIZE);

		x86_cdq(jit->ip);

		if ((divisor == X86_EAX) || (divisor == X86_EDX)) {
			x86_div_membase(jit->ip, X86_ESP, 0, sign);
			x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, 4);
		} else {
			x86_div_reg(jit->ip, divisor, sign);
		}
	}

	if (!modulo) {
		if (dest != X86_EAX) x86_mov_reg_reg(jit->ip, dest, X86_EAX, REG_SIZE);
	} else {
		if (dest != X86_EDX) x86_mov_reg_reg(jit->ip, dest, X86_EDX, REG_SIZE);
	}

	if (dest != X86_EDX) x86_pop_reg(jit->ip, X86_EDX);
	if (dest != X86_EAX) x86_pop_reg(jit->ip, X86_EAX);
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
		if (__uses_hw_reg(o, X86_EBX)) { x86_push_reg(jit->ip, X86_EBX); break; }
	}

	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, X86_ESI)) { x86_push_reg(jit->ip, X86_ESI); break; }
	}
	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, X86_EDI)) { x86_push_reg(jit->ip, X86_EDI); break; }
	}
}

static inline void __pop_callee_saved_regs(struct jit * jit)
{
	struct jit_op * op = jit->current_func;

	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, X86_EDI)) { x86_pop_reg(jit->ip, X86_EDI); break; }
	}

	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, X86_ESI)) { x86_pop_reg(jit->ip, X86_ESI); break; }
	}
	
	for (struct jit_op * o = op->next; o != NULL; o = o->next) {
		if (GET_OP(o) == JIT_PROLOG) break;
		if (__uses_hw_reg(o, X86_EBX)) { x86_pop_reg(jit->ip, X86_EBX); break; }
	}
}

static inline void __push_caller_saved_regs(struct jit * jit, jit_op * op)
{
	while (op) {
		if ((GET_OP(op) == JIT_CALL) || (GET_OP(op) == JIT_FINISH)) break;
		op = op->next;
	}
	for (int i = 0; i < jit->reg_count; i++) {
		if (!jitset_get(op->live_in, R(i))) continue;
		if (op->regmap[R(i)]) {
			if (op->regmap[R(i)]->id == X86_ECX) x86_push_reg(jit->ip, X86_ECX);
			if (op->regmap[R(i)]->id == X86_EDX) x86_push_reg(jit->ip, X86_EDX);
		}
	}
}

void jit_patch_external_calls(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if ((op->code == (JIT_FINISH | IMM)) && (!jit_is_label(jit, (void *)op->arg[0]))) {
			x86_patch(jit->buf + (long)op->patch_addr, (unsigned char *)op->arg[0]);
		}
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
				if (IS_IMM(op)) x86_lea_membase(jit->ip, a1, a2, a3);
				else x86_lea_memindex(jit->ip, a1, a2, 0, a3, 0);
			} else __alu_op(jit, op, X86_ADD, IS_IMM(op)); 
			break;

		case JIT_ADDC: __alu_op(jit, op, X86_ADD, IS_IMM(op)); break;
		case JIT_ADDX: __alu_op(jit, op, X86_ADC, IS_IMM(op)); break;
		case JIT_SUB: __sub_op(jit, op, IS_IMM(op)); break;
		case JIT_SUBC: __subx_op(jit, op, X86_SUB, IS_IMM(op)); break; 
		case JIT_SUBX: __subx_op(jit, op, X86_SBB, IS_IMM(op)); break; 
		case JIT_RSB: __rsb_op(jit, op, IS_IMM(op)); break; 
		case JIT_NEG: if (a1 != a2) x86_mov_reg_reg(jit->ip, a1, a2, REG_SIZE);
			      x86_neg_reg(jit->ip, a1); 
			      break;
		case JIT_OR: __alu_op(jit, op, X86_OR, IS_IMM(op)); break; 
		case JIT_XOR: __alu_op(jit, op, X86_XOR, IS_IMM(op)); break; 
		case JIT_AND: __alu_op(jit, op, X86_AND, IS_IMM(op)); break; 
		case JIT_NOT: if (a1 != a2) x86_mov_reg_reg(jit->ip, a1, a2, REG_SIZE);
			      x86_not_reg(jit->ip, a1);
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

		//case JIT_PATCH: x86_patch(((struct jit_op *)a1)->patch_addr, jit->ip); break;
		case JIT_PATCH: do {
					long pa = ((struct jit_op *)a1)->patch_addr;
					x86_patch(jit->buf + pa, jit->ip);
				} while (0);
				break;
		case JIT_JMP:
			op->patch_addr = __PATCH_ADDR(jit); 
			if (op->code & REG) x86_jump_reg(jit->ip, a1);
			//else x86_jump_code(jit->ip, __JIT_GET_ADDR(jit, a1));
			else x86_jump_disp(jit->ip, __JIT_GET_ADDR(jit, a1));
			break;
		case JIT_RET:
			if (!IS_IMM(op) && (a1 != X86_EAX)) x86_mov_reg_reg(jit->ip, X86_EAX, a1, 4);
			if (IS_IMM(op)) x86_mov_reg_imm(jit->ip, X86_EAX, a1);
			__pop_callee_saved_regs(jit);
			x86_mov_reg_reg(jit->ip, X86_ESP, X86_EBP, 4);
			x86_pop_reg(jit->ip, X86_EBP);
			x86_ret(jit->ip);
			break;

		case JIT_GETARG:
			if (op->arg_size == REG_SIZE) x86_mov_reg_membase(jit->ip, a1, X86_EBP, 8 + a2, REG_SIZE); 
			else if (IS_SIGNED(op)) x86_movsx_reg_membase(jit->ip, a1, X86_EBP, 8 + a2, op->arg_size); 
			else x86_movzx_reg_membase(jit->ip, a1, X86_EBP, 8 + a2, op->arg_size); 
			break;
			

		default: found = 0;
	}

	if (found) return;


	switch (op->code) {
		case (JIT_MOV | REG): if (a1 != a2) x86_mov_reg_reg(jit->ip, a1, a2, REG_SIZE); break;
		case (JIT_MOV | IMM):
			if (a2 == 0) x86_alu_reg_reg(jit->ip, X86_XOR, a1, a1);
			else x86_mov_reg_imm(jit->ip, a1, a2); 
			break;

		case JIT_PREPARE: jit->prepare_args = a1; 
				  __push_caller_saved_regs(jit, op);
				  break;

		case JIT_PUSHARG | REG: x86_push_reg(jit->ip, a1); break;
		case JIT_PUSHARG | IMM: x86_push_imm(jit->ip, a1); break;

		case JIT_PROLOG:
			//*(void **)(a1) = jit->ip;
			//op->patch_addr = jit->ip - (long)jit->buf;
			op->patch_addr = __PATCH_ADDR(jit);
			x86_push_reg(jit->ip, X86_EBP);
			x86_mov_reg_reg(jit->ip, X86_EBP, X86_ESP, 4);
			if (jit->allocai_mem) x86_alu_reg_imm(jit->ip, X86_SUB, X86_ESP, jit->allocai_mem);
			__push_callee_saved_regs(jit, op);
			break;

		case JIT_RETVAL: 
			if (a1 != X86_EAX) x86_mov_reg_reg(jit->ip, a1, X86_EAX, REG_SIZE);
			break;

		case JIT_LABEL: ((jit_label *)a1)->pos = __PATCH_ADDR(jit); break;

		case (JIT_LD | IMM | SIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_mem(jit->ip, a1, a2, op->arg_size);
			else x86_movsx_reg_mem(jit->ip, a1, a2, op->arg_size);
			break;

		case (JIT_LD | IMM | UNSIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_mem(jit->ip, a1, a2, op->arg_size);
			else x86_movzx_reg_mem(jit->ip, a1, a2, op->arg_size);
			break;

		case (JIT_LD | REG | SIGNED):
			if (op->arg_size == REG_SIZE) x86_mov_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			else x86_movsx_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			break;

		case (JIT_LD | REG | UNSIGNED):
			if (op->arg_size == REG_SIZE) x86_mov_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			else  x86_movzx_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			break;

		case (JIT_LDX | IMM | SIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			else x86_movsx_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			break;

		case (JIT_LDX | IMM | UNSIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			else x86_movzx_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			break;

		case (JIT_LDX | REG | SIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
			else x86_movsx_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
			break;
		case (JIT_LDX | REG | UNSIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
			else x86_movzx_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
			break;

		case (JIT_ST | IMM): x86_mov_mem_reg(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_ST | REG): x86_mov_membase_reg(jit->ip, a1, 0, a2, op->arg_size); break;
		case (JIT_STX | IMM): x86_mov_membase_reg(jit->ip, a2, a1, a3, op->arg_size); break;
		case (JIT_STX | REG): x86_mov_memindex_reg(jit->ip, a1, 0, a2, 0, a3, op->arg_size); break;

		case (JIT_UREG): x86_mov_membase_reg(jit->ip, X86_EBP, __GET_REG_POS(a1), a2, REG_SIZE); break;
		case (JIT_LREG): x86_mov_reg_membase(jit->ip, a1, X86_EBP, __GET_REG_POS(a2), REG_SIZE); break;
		case (JIT_SYNCREG): x86_mov_membase_reg(jit->ip, X86_EBP, __GET_REG_POS(a1), a2, REG_SIZE);  break;
		case JIT_CODESTART: break;
		case JIT_NOP: break;
		case JIT_DUMPREG: 
				    // creates an array of values
				    x86_push_reg(jit->ip, X86_EBP);
				    x86_push_reg(jit->ip, X86_EDI);
				    x86_push_reg(jit->ip, X86_ESI);
				    x86_push_reg(jit->ip, X86_EDX);
				    x86_push_reg(jit->ip, X86_ECX);
				    x86_push_reg(jit->ip, X86_EBX);
				    x86_push_reg(jit->ip, X86_EAX);
	
				    // push the 2nd argument
				    x86_push_reg(jit->ip, X86_ESP);

				    // push the 1st argument	
				    x86_mov_reg_imm(jit->ip, X86_EAX, jit);
				    x86_push_reg(jit->ip, X86_EAX);

				    x86_call_code(jit->ip, (long)jit_dump_registers);
				    x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, sizeof(long) * 2);

				    x86_push_reg(jit->ip, X86_EAX);
				    x86_push_reg(jit->ip, X86_EBX);
				    x86_push_reg(jit->ip, X86_ECX);
				    x86_push_reg(jit->ip, X86_EDX);
				    x86_push_reg(jit->ip, X86_ESI);
				    x86_push_reg(jit->ip, X86_EDI);
				    x86_push_reg(jit->ip, X86_EBP);
				    break;
		// platform specific opcodes; used byt optimizer
		case (JIT_X86_STI | IMM): x86_mov_mem_imm(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_X86_STI | REG): x86_mov_membase_imm(jit->ip, a1, 0, a2, op->arg_size); break;
		case (JIT_X86_STXI | IMM): x86_mov_membase_imm(jit->ip, a2, a1, a3, op->arg_size); break;
		case (JIT_X86_STXI | REG): x86_mov_memindex_imm(jit->ip, a1, 0, a2, 0, a3, op->arg_size); break;

		default: printf("x86: unknown operation (opcode: %i)\n", GET_OP(op));
	}
}


/* platform specific */
void jit_dump_registers(struct jit * jit, long * hwregs)
{
	// FIXME: asi je to rozbite
	/*
	for (int i = 0; i < jit->reg_count; i++) {
		char * sysname = "";
		long reg_value;
		if (i == 0) fprintf(stderr, "FP:\t0x%lx (ebp)\n", hwregs[6]);
		if (i == 1) fprintf(stderr, "RETREG:\t0x%lx\n", hwregs[0]);
		if (jit->reg_al->virt_reg[i].hw_reg) {
			switch (jit->reg_al->virt_reg[i].hw_reg->id) {
				case X86_EAX: reg_value = hwregs[0], sysname = "(eax)"; break;
				case X86_EBX: reg_value = hwregs[1], sysname = "(ebx)"; break;
				case X86_ECX: reg_value = hwregs[2], sysname = "(ecx)"; break;
				case X86_EDX: reg_value = hwregs[3], sysname = "(edx)"; break;
				case X86_ESI: reg_value = hwregs[4], sysname = "(esi)"; break;
				case X86_EDI: reg_value = hwregs[5], sysname = "(edi)"; break;
				default: reg_value = jit->regs[i];
			}
		} else reg_value = jit->regs[i];
		if (i > 2) fprintf(stderr, "%i:\t0x%lx %s\n", i - 2, reg_value, sysname);
	}
	*/
}
