/*
 * MyJIT 
 * Copyright (C) 2010, 2015 Petr Krajca, <petr.krajca@upol.cz>
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

#include <stdint.h>
#define IS_32BIT_VALUE(x) ((((long)(x)) >= INT32_MIN) && (((long)(x)) <= INT32_MAX))

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
		&& (IS_32BIT_VALUE(op->prev->arg[1]))
		&& (!jit_set_get(op->live_out, op->arg[1])))
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
		&& (IS_32BIT_VALUE(op->prev->arg[1]))
		&& (!jit_set_get(op->live_out, op->arg[2])))
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

void jit_optimize_frame_ptr(struct jit * jit)
{
	if (!jit->optimizations & JIT_OPT_OMIT_FRAME_PTR) return;

	struct jit_func_info * info = NULL;
	int uses_frame_ptr = 0;
	for (jit_op * op = jit_op_first(jit->ops); ; op = op->next) {
		if (!op || GET_OP(op) == JIT_PROLOG) {
			if (info && !uses_frame_ptr) {
				info->has_prolog = 0;
				uses_frame_ptr = 0;
			}

			if (op) info = (struct jit_func_info *) op->arg[1];
		}
		if (!op) break;
		if ((GET_OP(op) == JIT_ALLOCA) || (GET_OP(op) == JIT_UREG) || (GET_OP(op) == JIT_LREG) || (GET_OP(op) == JIT_SYNCREG)) {
			uses_frame_ptr |= 1;
		}
	}
}

void jit_optimize_unused_assignments(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if (ARG_TYPE(op, 1) == TREG) {
			// we have to skip these operations since, these are setting carry flag
			if ((GET_OP(op) == JIT_ADDC) || (GET_OP(op) == JIT_ADDX)
			|| (GET_OP(op) == JIT_SUBC) || (GET_OP(op) == JIT_SUBX)) continue;

			if (!jit_set_get(op->live_out, op->arg[0])) {
				op->code = JIT_NOP;
				op->spec = SPEC(NO, NO, NO);
			}
		}
	}
}

//
//
// Optimizations combining mul+add, add+add, and such operations into the one operation
//
//

static inline void make_nop(jit_op * op)
{
	op->code = JIT_NOP;
	op->spec = SPEC(NO, NO, NO);
}

static jit_op * get_related_op(jit_op * op, int result_reg)
{
	jit_op * nextop = op->next; 

	if ((nextop->arg[0] != result_reg) && (jit_set_get(nextop->live_out, result_reg))) return NULL;

	int used = 0;
	for (int i = 0; i < 3; i++)
		if ((ARG_TYPE(nextop, i + 1) == REG) && (nextop->arg[i])) {
			used = 1;
			break;
		}
	if (used) return nextop;
	return NULL;
}

static int join_2ops(jit_op * op, int opcode1, int opcode2, int (* joinfn)(jit_op *, jit_op *))
{
	if (op->code == opcode1) {
		jit_value result_reg = op->arg[0];
		jit_op * nextop = get_related_op(op, result_reg);
		if (nextop && (nextop->code == opcode2)) return joinfn(op, nextop); 
	}
	return 0;
}

static int shift_index(int arg)
{
	if (arg == 2) return 1;
	if (arg == 4) return 2;
	if (arg == 8) return 3;
	assert(0);
}

static inline int pow2(int arg)
{
	int r = 1;
	for (int i = 0; i < arg; i++)
		r *= 2;
	return r;
}

static inline int is_suitable_mul(jit_op * op)
{
	jit_value arg = op->arg[2];
	return ((((op->code == (JIT_MUL | IMM)) && ((arg == 2) || (arg == 4) || (arg == 8))))
	|| ((op->code == (JIT_LSH | IMM)) && ((arg == 1) || (arg == 2) || (arg == 3))));
}

static inline int make_addmuli(jit_op * op, jit_op * nextop)
{
	nextop->code = JIT_X86_ADDMUL | IMM;
	nextop->spec = SPEC(TREG, REG, IMM);

	nextop->arg[1] = op->arg[1];
	nextop->arg_size = (GET_OP(op) == JIT_MUL ? shift_index(op->arg[2]) : op->arg[2]);

	make_nop(op);
	return 1;
}

// combines:
// muli r1, r2, [2, 4, 8] (or lsh r1, r2, [1, 2, 3])
// addi r3, r1, imm
// -->
// addmuli r3, r2*size, r1	... lea reg, [reg*size + imm]
static int join_muli_addi(jit_op * op, jit_op * nextop)
{
	if (!IS_32BIT_VALUE(nextop->arg[2])) return 0;
	if (!is_suitable_mul(op)) return 0;
	return make_addmuli(op, nextop);
}

// combines:
// muli r1, r2, [2, 4, 8] (or lsh r1, r2, [1, 2, 3])
// ori r3, r1, [0..8]
// -->
// addmuli r3, r2*size, r1	... lea reg, [reg*size + imm]
static int join_muli_ori(jit_op * op, jit_op * nextop)
{
	if (!is_suitable_mul(op)) return 0;
	int max = (GET_OP(op) == JIT_MUL) ? max = op->arg[2] : pow2(op->arg[2]);

	if ((nextop->arg[2] > 0) && (nextop->arg[2] < max)) return make_addmuli(op, nextop);
	else return 0;
}

// combines:
// muli r1, r2, [2, 4, 8] (or lsh r1, r2, [1, 2, 3])
// addr r3, r1, r4
// -->
// addmulr r3, r2*size, r4	... lea reg, [reg*size + reg]
static int join_muli_addr(jit_op * op, jit_op * nextop)
{
	if ((!is_suitable_mul(op) || (nextop->arg[1] == nextop->arg[2]))) return 0;

	jit_value add_reg = (nextop->arg[1] == op->arg[0]) ? nextop->arg[2] : nextop->arg[1];

	nextop->code = JIT_X86_ADDMUL | REG;
	nextop->spec = SPEC(TREG, REG, REG);

	nextop->arg[1] = add_reg;
	nextop->arg[2] = op->arg[1];
	nextop->arg_size = (GET_OP(op) == JIT_MUL ? shift_index(op->arg[2]) : op->arg[2]);

	make_nop(op);
	return 1;
}

int jit_optimize_join_addmul(struct jit * jit)
{
	int change = 0;
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		change |= join_2ops(op, JIT_MUL | IMM, JIT_ADD | IMM, join_muli_addi);
		change |= join_2ops(op, JIT_LSH | IMM, JIT_ADD | IMM, join_muli_addi);
		change |= join_2ops(op, JIT_MUL | IMM, JIT_ADD | REG, join_muli_addr);
		change |= join_2ops(op, JIT_LSH | IMM, JIT_ADD | REG, join_muli_addr);
		change |= join_2ops(op, JIT_MUL | IMM, JIT_OR | IMM, join_muli_ori);
		change |= join_2ops(op, JIT_LSH | IMM, JIT_OR | IMM, join_muli_ori);
	}
	return change;
}

// combines:
// addr r1, r2, r3 
// addi r4, r1, imm
// -->
// addimm r4, r2, r3, imm  i.e., r4 := r2 + r3 + imm
static int join_addr_addi(jit_op * op, jit_op * nextop)
{
	if (!IS_32BIT_VALUE(nextop->arg[2])) return 0;
	make_nop(op);

	nextop->code = JIT_X86_ADDIMM;
	nextop->spec = SPEC(TREG, REG, REG);

	nextop->arg[2] = nextop->arg[2];
	//nextop->flt_imm = *(double *)&(nextop->arg[2]);
	memcpy(&nextop->flt_imm, &(nextop->arg[2]), sizeof(jit_value));
	nextop->arg[1] = op->arg[1];
	nextop->arg[2] = op->arg[2];

	return 1;
}

// combines:
// addi r1, r2, imm 
// addr r3, r1, r4 (or addr r3, r4, r1)
// -->
// addimm r3, r2, r4, imm  i.e., r3 := r2 + r4 + imm
static int join_addi_addr(jit_op * op, jit_op * nextop)
{
	if (!IS_32BIT_VALUE(op->arg[2])) return 0;
	if (GET_OP(op) == JIT_SUB) op->arg[2] = -op->arg[2];

	make_nop(op);

	nextop->code = JIT_X86_ADDIMM;
	nextop->spec = SPEC(TREG, REG, REG);

	if (nextop->arg[1] == nextop->arg[2]) op->arg[2] *= 2;
	memcpy(&nextop->flt_imm, &(op->arg[2]), sizeof(jit_value));

	if (nextop->arg[1] == op->arg[0]) nextop->arg[1] = op->arg[1];
	if (nextop->arg[2] == op->arg[0]) nextop->arg[2] = op->arg[1];

	return 1;
}

int jit_optimize_join_addimm(struct jit * jit)
{
	int change = 0;
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		change |= join_2ops(op, JIT_ADD | REG, JIT_ADD | IMM, join_addr_addi);
		change |= join_2ops(op, JIT_ADD | REG, JIT_SUB | IMM, join_addr_addi);
		change |= join_2ops(op, JIT_ADD | IMM, JIT_ADD | REG, join_addi_addr);
		change |= join_2ops(op, JIT_SUB | IMM, JIT_ADD | REG, join_addi_addr);
	}
	return change;
}
