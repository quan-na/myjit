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

#include "sparc-codegen.h"

#define __JIT_GET_ADDR(jit, imm) (!jit_is_label(jit, (void *)(imm)) ? (imm) :  \
		((((long)jit->buf + (long)((jit_label *)(imm))->pos - (long)jit->ip)) / 4))
#define __GET_REG_POS(r) ((- (r) * REG_SIZE) - REG_SIZE)
#define __PATCH_ADDR(jit)       ((long)jit->ip - (long)jit->buf)

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
	return (jit->allocai_mem);
}

static inline void __cond_op(struct jit * jit, struct jit_op * op, int cond, int imm)
{
	if (imm) {
		if (op->r_arg[2] != 0) sparc_cmp_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
		else sparc_cmp(jit->ip, op->r_arg[1], sparc_g0);
	} else sparc_cmp(jit->ip, op->r_arg[1], op->r_arg[2]);
	sparc_or_imm(jit->ip, FALSE, sparc_g0, 0, op->r_arg[0]); // clear
	sparc_movcc_imm(jit->ip, sparc_xcc, cond, 1, op->r_arg[0]);
}

static inline void __branch_op(struct jit * jit, struct jit_op * op, int cond, int imm)
{
	if (imm) {
		if (op->r_arg[2] != 0) sparc_cmp_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
		else sparc_cmp(jit->ip, op->r_arg[1], sparc_g0);
	} else sparc_cmp(jit->ip, op->r_arg[1], op->r_arg[2]);
	op->patch_addr = __PATCH_ADDR(jit);
	sparc_branch (jit->ip, FALSE, cond, __JIT_GET_ADDR(jit, op->r_arg[0]));
	sparc_nop(jit->ip);
}

static inline void __branch_mask_op(struct jit * jit, struct jit_op * op, int cond, int imm)
{
	if (imm) {
		if (op->r_arg[2] != 0) sparc_and_imm(jit->ip, sparc_cc, op->r_arg[1], op->r_arg[2], sparc_g0);
		else sparc_and(jit->ip, sparc_cc, op->r_arg[1], sparc_g0, sparc_g0);
	} else sparc_and(jit->ip, sparc_cc, op->r_arg[1], op->r_arg[2], sparc_g0);
	op->patch_addr = __PATCH_ADDR(jit);
	sparc_branch (jit->ip, FALSE, cond, __JIT_GET_ADDR(jit, op->r_arg[0]));
	sparc_nop(jit->ip);
}

static inline void __branch_overflow_op(struct jit * jit, struct jit_op * op, int alu_op, int imm)
{
	long a1 = op->r_arg[0];
	long a2 = op->r_arg[1];
	long a3 = op->r_arg[2];
	if (imm) {
		switch (alu_op) {
			case JIT_ADD: sparc_add_imm(jit->ip, sparc_cc, a2, a3, a1); break;
			case JIT_SUB: sparc_sub_imm(jit->ip, sparc_cc, a2, a3, a1); break;
			default: assert(0);
		}
	} else {
		switch (alu_op) {
			case JIT_ADD: sparc_add(jit->ip, sparc_cc, a2, a3, a1); break;
			case JIT_SUB: sparc_sub(jit->ip, sparc_cc, a2, a3, a1); break;
			default: assert(0);
		}
	}
	op->patch_addr = __PATCH_ADDR(jit);
	sparc_branch (jit->ip, FALSE, sparc_boverflow, __JIT_GET_ADDR(jit, op->r_arg[0]));
	sparc_nop(jit->ip);
}

/* determines whether the argument value was spilled out or not,
 * if the register is associated with the hardware register
 * it is returned through the reg argument
 */
// FIXME: more general, should use information from the reg. allocator
static inline int __is_spilled(int arg_id, jit_op * prepare_op, int * reg)
{
	int args = prepare_op->arg[0];
	struct __hw_reg * hreg = rmap_get(prepare_op->regmap, arg_id);
	if ((!hreg)
	|| ((hreg->id == sparc_o0) && (args > 0))
	|| ((hreg->id == sparc_o1) && (args > 1))
	|| ((hreg->id == sparc_o2) && (args > 2))
	|| ((hreg->id == sparc_o3) && (args > 3))
	|| ((hreg->id == sparc_o4) && (args > 4))
	|| ((hreg->id == sparc_o5) && (args > 5)))
		return 1;

	*reg = hreg->id;
	return 0;
}

static inline void __set_arg(struct jit * jit, struct jit_arg * arg, int reg)
{
	int sreg;
	long value = arg->value.generic;
	if (arg->isreg) {
		if (__is_spilled(value, jit->prepared_args->op, &sreg)) {
			sparc_ld_imm(jit->ip, sparc_fp, - value * 4, reg);
		} else {
			sparc_mov_reg_reg(jit->ip, sreg, reg);
		}
	} else sparc_set32(jit->ip, value, reg);
}

static inline void __configure_args(struct jit * jit)
{
	int sreg;
	struct jit_arg * args = jit->prepared_args->args;
	int argcnt = jit->prepared_args->count;

	if (argcnt > 0) __set_arg(jit, &(args[0]), sparc_o0);
	if (argcnt > 1) __set_arg(jit, &(args[1]), sparc_o1);
	if (argcnt > 2) __set_arg(jit, &(args[2]), sparc_o2);
	if (argcnt > 3) __set_arg(jit, &(args[3]), sparc_o3);
	if (argcnt > 4) __set_arg(jit, &(args[4]), sparc_o4);
	if (argcnt > 5) __set_arg(jit, &(args[5]), sparc_o5);

	for (int i = 6; i < argcnt; i++) {
		long value = args[i].value.generic;
		if (args[i].isreg) {
			if (__is_spilled(value, jit->prepared_args->op, &sreg)) {
				sparc_ld_imm(jit->ip, sparc_fp, - value * 4, sparc_g1);
				sparc_st_imm(jit->ip, sparc_g1, sparc_sp, 92 + (i - 6) * 4);
			} else {
				sparc_st_imm(jit->ip, sreg, sparc_sp, 92 + (i - 6) * 4);
			}
		} else {
			sparc_set32(jit->ip, value, sparc_g1);
			sparc_st_imm(jit->ip, sparc_g1, sparc_sp, 92 + (i - 6) * 4);
		}
	}
}

static inline void __funcall(struct jit * jit, struct jit_op * op, int imm)
{
	__configure_args(jit);
	if (!imm) {
		sparc_call(jit->ip, op->r_arg[0], sparc_g0);
	} else {
		op->patch_addr = __PATCH_ADDR(jit);
		if (op->r_arg[0] == (long)JIT_FORWARD) {
			sparc_call_simple(jit->ip, 0); 
		} else if (jit_is_label(jit, (void *)op->r_arg[0]))

			sparc_call_simple(jit->ip, ((long)jit->buf - (long)jit->ip) + (long)((jit_label *)(op->r_arg[0]))->pos); 
		else {
			sparc_call_simple(jit->ip, (long)op->r_arg[0] - (long)jit->ip);
		}
	}
	sparc_nop(jit->ip);
}

void __get_arg(struct jit * jit, jit_op * op, int reg)
{
	int arg_id = op->r_arg[1];
	int reg_id = arg_id + JIT_FIRST_REG + 1; // 1 -- is R_IMM
	int dreg = op->r_arg[0];

	if (rmap_get(op->regmap, reg_id) == NULL) {
		// the register is not associated and the value has to be read from the memory
		//sparc_ld_imm(jit->ip, sparc_sp, 96 + reg_id * 4, dreg);
		sparc_ld_imm(jit->ip, sparc_fp, -reg_id * 4, dreg);
		return;
	}

	sparc_mov_reg_reg(jit->ip, reg, dreg);
}

void jit_patch_external_calls(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if ((op->code == (JIT_CALL | IMM)) && (!jit_is_label(jit, (void *)op->arg[0])))
			sparc_patch(jit->buf + (long)op->patch_addr, (long)op->r_arg[0]);
		if (GET_OP(op) == JIT_MSG)
			sparc_patch(jit->buf + (long)op->patch_addr, (unsigned char *)printf);
	}
}

/**
 * computes number of 1's in the given binary number
 * this was taken from the Hacker's Delight book by Henry S. Warren
 */
static inline int _bit_pop(unsigned int x) {
	x = (x & 0x55555555) + ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x & 0x0F0F0F0F) + ((x >> 4) & 0x0F0F0F0F);
	x = (x & 0x00FF00FF) + ((x >> 8) & 0x00FF00FF);
	x = (x & 0x0000FFFF) + ((x >>16) & 0x0000FFFF);
	return x;
}

/**
 * Generates multiplication using only the shift left and add operations
 */
void __gener_mult(struct jit * jit, long a1, long a2, long a3)
{
	int bits = _bit_pop(a3);
	unsigned long ar = (unsigned long)a3;
	int in_tmp = 0; // 1 if there's something the temporary registers g1, g2
	for (int i = 0; i < 32; i++) {
		if (ar & 0x1) {
			bits--;
			if (bits == 0) {
				// last and the only one bit to multiply with
				if (!in_tmp) sparc_sll_imm(jit->ip, a2, i, a1);
				else {
					sparc_sll_imm(jit->ip, a2, i, sparc_g2);
					sparc_add(jit->ip, FALSE, sparc_g1, sparc_g2, a1);
				}
			} else  {
				if (!in_tmp) {
					sparc_sll_imm(jit->ip, a2, i, sparc_g1);
					in_tmp = 1;
				} else {
					sparc_sll_imm(jit->ip, a2, i, sparc_g2);
					sparc_add(jit->ip, FALSE, sparc_g1, sparc_g2, sparc_g1);
				}
			}
		}
		ar >>= 1;
		if (bits == 0) break;
	}
}


void __mul(struct jit * jit, jit_op * op)
{
	long a1 = op->r_arg[0];
	long a2 = op->r_arg[1];
	long a3 = op->r_arg[2];
	if (IS_IMM(op)) {
		if (a3 == 0) {
			sparc_mov_reg_reg(jit->ip, sparc_g0, a1);
			return;
		}
		if (a3 == 1) {
			if (a1 != a2) sparc_mov_reg_reg(jit->ip, a2, a1);
			return;
		}
		if ((a3 > 0) && (_bit_pop(a3) <= 5)) {
			__gener_mult(jit, a1, a2, a3);
			return;
		}
		if ((a3 < 0) && (_bit_pop(-a3) <= 5)) {
			__gener_mult(jit, a1, a2, -a3);
			sparc_neg(jit->ip, a1);
			return;
		}
		sparc_smul_imm(jit->ip, FALSE, a2, a3, a1);
	} else sparc_smul(jit->ip, FALSE, a2, a3, a1);
}

#define __alu_op(cc, reg_op, imm_op) \
	if (IS_IMM(op)) {\
		if (a3 != 0) imm_op(jit->ip, cc, a2, a3, a1); \
		else reg_op(jit->ip, cc, a2, sparc_g0, a1); \
	} else reg_op(jit->ip, cc, a2, a3, a1); \
	break;

void jit_gen_op(struct jit * jit, struct jit_op * op)
{
	long a1 = op->r_arg[0];
	long a2 = op->r_arg[1];
	long a3 = op->r_arg[2];

	int found = 1;
	switch (GET_OP(op)) {
		case JIT_ADD: __alu_op(FALSE, sparc_add, sparc_add_imm);
		case JIT_ADDC: __alu_op(sparc_cc, sparc_add, sparc_add_imm);
		case JIT_ADDX: __alu_op(FALSE, sparc_addx, sparc_addx_imm);
		case JIT_SUB: __alu_op(FALSE, sparc_sub, sparc_sub_imm);
		case JIT_SUBC: __alu_op(sparc_cc, sparc_sub, sparc_sub_imm);
		case JIT_SUBX: __alu_op(FALSE, sparc_subx, sparc_subx_imm);
		case JIT_RSB: 
			if (IS_IMM(op)) {
				sparc_set32(jit->ip, a3, sparc_g1);
				sparc_sub(jit->ip, FALSE, sparc_g1, a2, a1);
			} else sparc_sub(jit->ip, FALSE, a3, a2, a1);
			break;

		case JIT_NEG: if (a1 != a2) sparc_mov_reg_reg(jit->ip, a2, a1);
			      sparc_neg(jit->ip, a1); 
			      break;

		case JIT_NOT: if (a1 != a2) sparc_mov_reg_reg(jit->ip, a2, a1);
			      sparc_not(jit->ip, a1); 
			      break;
		case JIT_OR:  __alu_op(FALSE, sparc_or, sparc_or_imm);
		case JIT_AND: __alu_op(FALSE, sparc_and, sparc_and_imm);
		case JIT_XOR: __alu_op(FALSE, sparc_xor, sparc_xor_imm);
		case JIT_LSH: if (IS_IMM(op)) sparc_sll_imm(jit->ip, a2, a3, a1);
			else sparc_sll(jit->ip, a2, a3, a1);
			break;
		case JIT_RSH:
			if (IS_SIGNED(op)) {
				if (IS_IMM(op)) sparc_sra_imm(jit->ip, a2, a3, a1);
				else sparc_sra(jit->ip, a2, a3, a1);
			} else {
				if (IS_IMM(op)) sparc_srl_imm(jit->ip, a2, a3, a1);
				else sparc_srl(jit->ip, a2, a3, a1);
			}
			break;

		case JIT_MUL:  __mul(jit, op); break;

		case JIT_HMUL: 
			if (IS_IMM(op)) sparc_smul_imm(jit->ip, FALSE, a2, a3, sparc_g0);
			else sparc_smul(jit->ip, FALSE, a2, a3, sparc_g0);
			sparc_nop(jit->ip);
			sparc_rdy(jit->ip, a1);
			break;

		case JIT_DIV: 
			if (IS_IMM(op)) {
				switch (a3) {
					case 2: sparc_sra_imm(jit->ip, a2, 1, a1); goto op_complete;
					case 4: sparc_sra_imm(jit->ip, a2, 2, a1); goto op_complete;
					case 8: sparc_sra_imm(jit->ip, a2, 3, a1); goto op_complete;
					case 16: sparc_sra_imm(jit->ip, a2, 4, a1); goto op_complete;
					case 32: sparc_sra_imm(jit->ip, a2, 5, a1); goto op_complete;
				}
			} 
			sparc_sra_imm(jit->ip, a2, 31, sparc_g1);
			sparc_wry(jit->ip, sparc_g1, sparc_g0);
			sparc_nop(jit->ip);
			sparc_nop(jit->ip);
			sparc_nop(jit->ip);

			if (IS_IMM(op)) sparc_sdiv_imm(jit->ip, FALSE, a2, a3, a1);
			else sparc_sdiv(jit->ip, FALSE, a2, a3, a1);
			break;

		case JIT_MOD: 
			if (IS_IMM(op)) {
				switch (a3) {
					case 2: sparc_and_imm(jit->ip, FALSE, a2, 0x01, a1); goto op_complete;
					case 4: sparc_and_imm(jit->ip, FALSE, a2, 0x03, a1); goto op_complete;
					case 8: sparc_and_imm(jit->ip, FALSE, a2, 0x07, a1); goto op_complete;
					case 16: sparc_and_imm(jit->ip, FALSE, a2, 0x0f, a1); goto op_complete;
					case 32: sparc_and_imm(jit->ip, FALSE, a2, 0x1f, a1); goto op_complete;
				}
			}
			sparc_sra_imm(jit->ip, a2, 31, sparc_g1);
			sparc_wry(jit->ip, sparc_g1, sparc_g0);
			sparc_nop(jit->ip);
			sparc_nop(jit->ip);
			sparc_nop(jit->ip);
			if (IS_IMM(op)) {
				sparc_sdiv_imm(jit->ip, FALSE, a2, a3, sparc_g1);
				sparc_smul_imm(jit->ip, FALSE, sparc_g1, a3, sparc_g1);
			} else {
				sparc_sdiv(jit->ip, FALSE, a2, a3, sparc_g1);
				sparc_smul(jit->ip, FALSE, sparc_g1, a3, sparc_g1);
			}
			sparc_sub(jit->ip, FALSE, a2, sparc_g1, a1);
			break;
		case JIT_LT: __cond_op(jit, op, IS_SIGNED(op) ? sparc_bl : sparc_blu, IS_IMM(op)); break;
		case JIT_LE: __cond_op(jit, op, IS_SIGNED(op) ? sparc_ble : sparc_bleu, IS_IMM(op)); break;
		case JIT_GT: __cond_op(jit, op, IS_SIGNED(op) ? sparc_bg : sparc_bgu, IS_IMM(op)); break;
		case JIT_GE: __cond_op(jit, op, IS_SIGNED(op) ? sparc_bge : sparc_bgeu, IS_IMM(op)); break;
		case JIT_EQ: __cond_op(jit, op, sparc_be, IS_IMM(op)); break;
		case JIT_NE: __cond_op(jit, op, sparc_bne, IS_IMM(op)); break;
		case JIT_BLT: __branch_op(jit, op, IS_SIGNED(op) ? sparc_bl : sparc_blu, IS_IMM(op)); break;
		case JIT_BGT: __branch_op(jit, op, IS_SIGNED(op) ? sparc_bg : sparc_bgu, IS_IMM(op)); break;
		case JIT_BLE: __branch_op(jit, op, IS_SIGNED(op) ? sparc_ble : sparc_bleu, IS_IMM(op)); break;
		case JIT_BGE: __branch_op(jit, op, IS_SIGNED(op) ? sparc_bge : sparc_bgeu, IS_IMM(op)); break;
		case JIT_BEQ: __branch_op(jit, op, sparc_be, IS_IMM(op)); break;
		case JIT_BNE: __branch_op(jit, op, sparc_bne, IS_IMM(op)); break;
		case JIT_BMS: __branch_mask_op(jit, op, sparc_be, IS_IMM(op)); break;
		case JIT_BMC: __branch_mask_op(jit, op, sparc_bne, IS_IMM(op)); break;
		case JIT_BOADD: __branch_overflow_op(jit, op, JIT_ADD, IS_IMM(op)); break;
		case JIT_BOSUB: __branch_overflow_op(jit, op, JIT_SUB, IS_IMM(op)); break;

		case JIT_CALL: __funcall(jit, op, IS_IMM(op)); break;

		case JIT_PATCH: do {
					long pa = ((struct jit_op *)a1)->patch_addr;
					sparc_patch(jit->buf + pa, jit->ip);
				} while (0);
				break;
		case JIT_JMP:
			op->patch_addr = __PATCH_ADDR(jit);
			if (op->code & REG) sparc_jmp(jit->ip, a1, sparc_g0);
			else sparc_branch(jit->ip, FALSE, sparc_balways, __JIT_GET_ADDR(jit, op->r_arg[0]));
			sparc_nop(jit->ip);
			break;
		case JIT_RET:
			if (!IS_IMM(op) && (a1 != sparc_i0)) sparc_mov_reg_reg(jit->ip, a1, sparc_i0);
			if (IS_IMM(op)) sparc_set32(jit->ip, a1, sparc_i0);
			sparc_ret(jit->ip);
			sparc_restore_imm(jit->ip, sparc_g0, 0, sparc_g0);
			break;

		case JIT_PUTARG: __put_arg(jit, op); break;

		case JIT_GETARG:
			if (a2 == 0) __get_arg(jit, op, sparc_i0);
			if (a2 == 1) __get_arg(jit, op, sparc_i1);
			if (a2 == 2) __get_arg(jit, op, sparc_i2);
			if (a2 == 3) __get_arg(jit, op, sparc_i3);
			if (a2 == 4) __get_arg(jit, op, sparc_i4);
			if (a2 == 5) __get_arg(jit, op, sparc_i5);
			if (a2 > 5) sparc_ld_imm(jit->ip, sparc_fp, 92 + (a2 - 6) * 4, a1);
			break;
		case JIT_MSG:
			sparc_set(jit->ip, a1, sparc_o0);
			if (!IS_IMM(op)) sparc_mov_reg_reg(jit->ip, a2, sparc_o1);
			op->patch_addr = __PATCH_ADDR(jit);
			sparc_call_simple(jit->ip, printf);
			sparc_nop(jit->ip);
			break;

		default: found = 0;
	}

op_complete:
	if (found) return;

	switch (op->code) {
		case (JIT_MOV | REG): if (a1 != a2) sparc_mov_reg_reg(jit->ip, a2, a1); break;
		case (JIT_MOV | IMM): sparc_set32(jit->ip, a2, a1); break;
		case JIT_PREPARE: __prepare_call(jit, op, a1); break;
		case JIT_PROLOG:
			*(void **)(a1) = jit->ip;
			sparc_save_imm(jit->ip, sparc_sp, -96 - jit->allocai_mem, sparc_sp);
			break;
		case JIT_RETVAL: 
			if (a1 != sparc_o0) sparc_mov_reg_reg(jit->ip, sparc_o0, a1); 
			break;

		case JIT_LABEL: ((jit_label *)a1)->pos = __PATCH_ADDR(jit); break; 

		case (JIT_LD | REG | SIGNED): 
			switch (op->arg_size) {
				case 1: sparc_ldsb(jit->ip, a2, sparc_g0, a1); break;
				case 2: sparc_ldsh(jit->ip, a2, sparc_g0, a1); break;
				case 4: sparc_ld(jit->ip, a2, sparc_g0, a1); break;
				default: abort();
			} break;

		case (JIT_LD | REG | UNSIGNED): 
			switch (op->arg_size) {
				case 1: sparc_ldub(jit->ip, a2, sparc_g0, a1); break;
				case 2: sparc_lduh(jit->ip, a2, sparc_g0, a1); break;
				case 4: sparc_ld(jit->ip, a2, sparc_g0, a1); break;
				default: abort();
			} break;

		case (JIT_LD | IMM | SIGNED): 
			switch (op->arg_size) {
				case 1: sparc_ldsb_imm(jit->ip, sparc_g0, a2, a1); break;
				case 2: sparc_ldsh_imm(jit->ip, sparc_g0, a2, a1); break;
				case 4: sparc_ld_imm(jit->ip, sparc_g0, a2, a1); break;
				default: abort();
			} break;

		case (JIT_LD | IMM | UNSIGNED): 
			switch (op->arg_size) {
				case 1: sparc_ldub_imm(jit->ip, sparc_g0, a2, a1); break;
				case 2: sparc_lduh_imm(jit->ip, sparc_g0, a2, a1); break;
				case 4: sparc_ld_imm(jit->ip, sparc_g0, a2, a1); break;
				default: abort();
			} break;

		case (JIT_LDX | IMM | SIGNED): 
			switch (op->arg_size) {
				case 1: sparc_ldsb_imm(jit->ip, a2, a3, a1); break;
				case 2: sparc_ldsh_imm(jit->ip, a2, a3, a1); break;
				case 4: sparc_ld_imm(jit->ip, a2, a3, a1); break;
				default: abort();
			} break;

		case (JIT_LDX | IMM | UNSIGNED): 
			switch (op->arg_size) {
				case 1: sparc_ldub_imm(jit->ip, a2, a3, a1); break;
				case 2: sparc_lduh_imm(jit->ip, a2, a3, a1); break;
				case 4: sparc_ld_imm(jit->ip, a2, a3, a1); break;
				default: abort();
			} break;

		case (JIT_LDX | REG | SIGNED): 
			switch (op->arg_size) {
				case 1: sparc_ldsb(jit->ip, a2, a3, a1); break;
				case 2: sparc_ldsh(jit->ip, a2, a3, a1); break;
				case 4: sparc_ld(jit->ip, a2, a3, a1); break;
				default: abort();
			} break;

		case (JIT_LDX | REG | UNSIGNED): 
			switch (op->arg_size) {
				case 1: sparc_ldub(jit->ip, a2, a3, a1); break;
				case 2: sparc_lduh(jit->ip, a2, a3, a1); break;
				case 4: sparc_ld(jit->ip, a2, a3, a1); break;
				default: abort();
			} break;

		case (JIT_ST | REG):
			switch (op->arg_size) {
				case 1: sparc_stb(jit->ip, a2, sparc_g0, a1); break;
				case 2: sparc_sth(jit->ip, a2, sparc_g0, a1); break;
				case 4: sparc_st(jit->ip, a2, sparc_g0, a1); break;
				default: abort();
			} break;
		
		case (JIT_ST | IMM):
			switch (op->arg_size) {
				case 1: sparc_stb_imm(jit->ip, sparc_g0, a2, a1); break;
				case 2: sparc_sth_imm(jit->ip, sparc_g0, a2, a1); break;
				case 4: sparc_st_imm(jit->ip, sparc_g0, a2, a1); break;
				default: abort();
			} break;


		case (JIT_STX | REG):
			switch (op->arg_size) {
				case 1: sparc_stb(jit->ip, a3, a2, a1); break;
				case 2: sparc_sth(jit->ip, a3, a2, a1); break;
				case 4: sparc_st(jit->ip, a3, a2, a1); break;
				default: abort();
			} break;

		case (JIT_STX | IMM):
			switch (op->arg_size) {
				case 1: sparc_stb_imm(jit->ip, a3, a2, a1); break;
				case 2: sparc_sth_imm(jit->ip, a3, a2, a1); break;
				case 4: sparc_st_imm(jit->ip, a3, a2, a1); break;
				default: abort();
			} break;
		case (JIT_UREG): sparc_st_imm(jit->ip, a2, sparc_fp, - a1 * 4); break;
		case (JIT_SYNCREG): sparc_st_imm(jit->ip, a2, sparc_fp, - a1 * 4); break;
		case (JIT_LREG): sparc_ld_imm(jit->ip, sparc_fp, - a2 * 4, a1); break;
		case JIT_CODESTART: break;
		case JIT_NOP: break;
		default: printf("sparc: unknown operation (opcode: 0x%x)\n", GET_OP(op) >> 3);
	}
}

struct jit_reg_allocator * jit_reg_allocator_create()
{
	static int __arg_regs[] = { sparc_i0, sparc_i1, sparc_i2, sparc_i3, sparc_i4, sparc_i5 };
	struct jit_reg_allocator * a = JIT_MALLOC(sizeof(struct jit_reg_allocator));
	a->gp_reg_cnt = 14;
#ifdef JIT_REGISTER_TEST
	a->gp_reg_cnt -= 5;
#endif 
	a->gp_regpool = jit_regpool_init(a->gp_reg_cnt);
	a->gp_regs = JIT_MALLOC(sizeof(struct __hw_reg) * (a->gp_reg_cnt + 1));

	/* only the l0-l7 and i0-i5 registers are used
	 * all these registers are callee-saved withou special care
	 * register o0-o5 are used only for argument passing
	 * all g1-g3 are free for use in the codegenarator
	 */
	int i = 0;
	a->gp_regs[i++] = (struct __hw_reg) { sparc_l0, 0, "l0", 1, 0, 14 };
	a->gp_regs[i++] = (struct __hw_reg) { sparc_l1, 0, "l1", 1, 0, 13 };
	a->gp_regs[i++] = (struct __hw_reg) { sparc_l2, 0, "l2", 1, 0, 12 };
#ifndef JIT_REGISTER_TEST
	a->gp_regs[i++] = (struct __hw_reg) { sparc_l3, 0, "l3", 1, 0, 11 };
	a->gp_regs[i++] = (struct __hw_reg) { sparc_l4, 0, "l4", 1, 0, 10 };
	a->gp_regs[i++] = (struct __hw_reg) { sparc_l5, 0, "l5", 1, 0, 9 };
	a->gp_regs[i++] = (struct __hw_reg) { sparc_l6, 0, "l6", 1, 0, 8 };
	a->gp_regs[i++] = (struct __hw_reg) { sparc_l7, 0, "l7", 1, 0, 7 };
#endif

	a->gp_regs[i++] = (struct __hw_reg) { sparc_i0, 0, "i0", 1, 0, 1 };
	a->gp_regs[i++] = (struct __hw_reg) { sparc_i1, 0, "i1", 1, 0, 2 };
	a->gp_regs[i++] = (struct __hw_reg) { sparc_i2, 0, "i2", 1, 0, 3 };
	a->gp_regs[i++] = (struct __hw_reg) { sparc_i3, 0, "i3", 1, 0, 4 };
	a->gp_regs[i++] = (struct __hw_reg) { sparc_i4, 0, "i4", 1, 0, 5 };
	a->gp_regs[i++] = (struct __hw_reg) { sparc_i5, 0, "i5", 1, 0, 6 };

	a->gp_regs[i++] = (struct __hw_reg) { sparc_fp, 0, "fp", 0, 0, 0 };

	a->fp_reg = sparc_fp;
	a->ret_reg = sparc_i7;

	a->fp_reg_cnt = 0;
	a->fp_regpool = jit_regpool_init(a->fp_reg_cnt);
	a->fp_regs = JIT_MALLOC(sizeof(struct __hw_reg) * a->fp_reg_cnt);

	a->gp_arg_reg_cnt = 6;
	a->gp_arg_regs = __arg_regs;

	a->fp_arg_reg_cnt = 0;
	a->fp_arg_regs = NULL;

	return a;
}
