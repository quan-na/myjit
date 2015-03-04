/*
 * MyJIT 
 * Copyright (C) 2010, 2011, 2015 Petr Krajca, <petr.krajca@upol.cz>
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
#include "x86-common-stuff.c"

#define JIT_GET_ADDR(jit, imm) (!jit_is_label(jit, (void *)(imm)) ? (imm) :  \
		((((long)jit->buf + (long)((jit_label *)(imm))->pos - (long)jit->ip)) / 4))

#define EXTRA_SPACE (16)

inline jit_hw_reg * rmap_get(jit_rmap * rmap, jit_value reg);

static inline int GET_REG_POS(struct jit * jit, int r)
{
	struct jit_func_info * info = jit_current_func_info(jit);
	if (JIT_REG(r).spec == JIT_RTYPE_REG) {
		if (JIT_REG(r).type == JIT_RTYPE_INT) {
			return - (info->allocai_mem + EXTRA_SPACE + JIT_REG(r).id * REG_SIZE + REG_SIZE);
		} else {
			return - (info->allocai_mem + EXTRA_SPACE + info->gp_reg_count * REG_SIZE + JIT_REG(r).id * sizeof(double) + sizeof(double));
		}
	}
	if (JIT_REG(r).spec == JIT_RTYPE_ARG) {
		int arg_id = JIT_REG(r).id;
		struct jit_inp_arg * a = &(jit_current_func_info(jit)->args[arg_id]);
		return a->spill_pos;
	}
	assert(0);
}

static inline int space_for_outgoing_args(struct jit *jit, jit_op *op) {
	int passed_gp_args = 0;
	int passed_fp_args = 0;
	op = op->next;
	while (op && (GET_OP(op) != JIT_PROLOG)) {
		if (GET_OP(op) == JIT_PREPARE) {
			passed_gp_args = MAX(passed_gp_args, op->arg[0]);
			passed_fp_args = MAX(passed_fp_args, op->arg[1]);
		}
		op = op->next;
	}
	int stack_gp_args = MAX(0, passed_gp_args - jit->reg_al->gp_arg_reg_cnt);
	int stack_fp_args = MAX(0, passed_fp_args - jit->reg_al->fp_arg_reg_cnt);
	return stack_gp_args * REG_SIZE + stack_fp_args * sizeof(double);
}

int jit_allocai(struct jit * jit, int size)
{
	int real_size = jit_value_align(size, 16);
	jit_add_op(jit, JIT_ALLOCA | IMM, SPEC(IMM, NO, NO), (long)real_size, 0, 0, 0, NULL);
	jit_current_func_info(jit)->allocai_mem += real_size;	
	return -(jit_current_func_info(jit)->allocai_mem + EXTRA_SPACE);
}

static inline void init_arg(struct jit_inp_arg * arg, int p)
{
	static const int in_regs[] = { sparc_i0, sparc_i1, sparc_i2, sparc_i3, sparc_i4, sparc_i5 };
	
	if (p < 6) {
		arg->passed_by_reg = 1;
		arg->location.reg = in_regs[p];
		//arg->spill_pos = 68 + (p - 6) * 4;
		arg->spill_pos = 92 + (p - 6) * 4;
	} else {
		arg->passed_by_reg = 0;
		arg->location.stack_pos = 92 + (p - 6) * 4;
		arg->spill_pos = arg->location.stack_pos; 
	}
	arg->overflow = 0;
	arg->phys_reg = p;
}

void jit_init_arg_params(struct jit * jit, struct jit_func_info * info, int p, int * phys_reg)
{
	struct jit_inp_arg * a = &(info->args[p]);
	init_arg(a, *phys_reg);
	*phys_reg = *phys_reg + 1;
	if ((a->type == JIT_FLOAT_NUM) && (a->size == sizeof(double))) {
		a->overflow = 1;
		*phys_reg = *phys_reg + 1;
	}
}

static inline void emit_cond_op_op(struct jit * jit, struct jit_op * op, int cond, int imm)
{
	if (imm) {
		if (op->r_arg[2] != 0) sparc_cmp_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
		else sparc_cmp(jit->ip, op->r_arg[1], sparc_g0);
	} else sparc_cmp(jit->ip, op->r_arg[1], op->r_arg[2]);
	sparc_or_imm(jit->ip, FALSE, sparc_g0, 0, op->r_arg[0]); // clear
	sparc_movcc_imm(jit->ip, sparc_xcc, cond, 1, op->r_arg[0]);
}

static inline void emit_branch_op(struct jit * jit, struct jit_op * op, int cond, int imm)
{
	if (imm) {
		if (op->r_arg[2] != 0) sparc_cmp_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
		else sparc_cmp(jit->ip, op->r_arg[1], sparc_g0);
	} else sparc_cmp(jit->ip, op->r_arg[1], op->r_arg[2]);
	op->patch_addr = JIT_BUFFER_OFFSET(jit);
	sparc_branch (jit->ip, FALSE, cond, JIT_GET_ADDR(jit, op->r_arg[0]));
	sparc_nop(jit->ip);
}

static inline void emit_branch_mask_op(struct jit * jit, struct jit_op * op, int cond, int imm)
{
	if (imm) {
		if (op->r_arg[2] != 0) sparc_and_imm(jit->ip, sparc_cc, op->r_arg[1], op->r_arg[2], sparc_g0);
		else sparc_and(jit->ip, sparc_cc, op->r_arg[1], sparc_g0, sparc_g0);
	} else sparc_and(jit->ip, sparc_cc, op->r_arg[1], op->r_arg[2], sparc_g0);
	op->patch_addr = JIT_BUFFER_OFFSET(jit);
	sparc_branch (jit->ip, FALSE, cond, JIT_GET_ADDR(jit, op->r_arg[0]));
	sparc_nop(jit->ip);
}

static inline void emit_op_and_overflow_branch(struct jit * jit, struct jit_op * op, int alu_op, int imm, int negation)
{
	long a1 = op->r_arg[0];
	long a2 = op->r_arg[1];
	long a3 = op->r_arg[2];
	if (imm) {
		switch (alu_op) {
			case JIT_ADD: sparc_add_imm(jit->ip, sparc_cc, a2, a3, a2); break;
			case JIT_SUB: sparc_sub_imm(jit->ip, sparc_cc, a2, a3, a2); break;
			default: assert(0);
		}
	} else {
		switch (alu_op) {
			case JIT_ADD: sparc_add(jit->ip, sparc_cc, a2, a3, a2); break;
			case JIT_SUB: sparc_sub(jit->ip, sparc_cc, a2, a3, a2); break;
			default: assert(0);
		}
	}
	op->patch_addr = JIT_BUFFER_OFFSET(jit);
	if (!negation) sparc_branch (jit->ip, FALSE, sparc_boverflow, JIT_GET_ADDR(jit, a1));
	else sparc_branch (jit->ip, FALSE, sparc_bnoverflow, JIT_GET_ADDR(jit, a1));
	sparc_nop(jit->ip);
}

static inline void emit_fpbranch_op(struct jit * jit, struct jit_op * op, int cond, int arg1, int arg2)
{
	sparc_fcmpd(jit->ip, arg1, arg2);
	op->patch_addr = JIT_BUFFER_OFFSET(jit);
	sparc_fbranch (jit->ip, FALSE, cond, JIT_GET_ADDR(jit, op->r_arg[0]));
	sparc_nop(jit->ip);
}

/* determines whether the argument value was spilled out or not,
 * if the register is associated with the hardware register
 * it is returned through the reg argument
 */
// FIXME: more general, should use information from the reg. allocator
static inline int is_spilled(int arg_id, jit_op * prepare_op, int * reg)
{
	jit_hw_reg * hreg = rmap_get(prepare_op->regmap, arg_id);
	if (hreg) {
		*reg = hreg->id;
		return 0;
	}
	return 1;
}

static const int OUT_REGS[] = { sparc_o0, sparc_o1, sparc_o2, sparc_o3, sparc_o4, sparc_o5 };

static inline void emit_set_arg_imm(struct jit * jit, int value, int slot)
{
	if (slot < 6) sparc_set32(jit->ip, value, OUT_REGS[slot]);
	else {
		sparc_set32(jit->ip, value, sparc_g1);
		sparc_st_imm(jit->ip, sparc_g1, sparc_sp, 92 + (slot - 6) * 4);
	}
}

static inline void emit_set_arg_reg(struct jit * jit, int sreg, int slot) 
{
	if (slot < 6) sparc_mov_reg_reg(jit->ip, sreg, OUT_REGS[slot]);
	else sparc_st_imm(jit->ip, sreg, sparc_sp, 92 + (slot - 6) * 4);
}

static inline void emit_set_arg_freg(struct jit * jit, int sreg, int slot) 
{
	if (slot < 6) { 
		sparc_stf_imm(jit->ip, sreg, sparc_fp, -8);
		sparc_ld_imm(jit->ip, sparc_fp, -8, OUT_REGS[slot]);
	} else sparc_stf_imm(jit->ip, sreg, sparc_sp, 92 + (slot - 6) * 4);
}

static inline void emit_set_arg_mem(struct jit * jit, int disp, int slot) 
{
	if (slot < 6) sparc_ld_imm(jit->ip, sparc_fp, disp, OUT_REGS[slot]);
	else {
		sparc_ld_imm(jit->ip, sparc_fp, disp, sparc_g1);
		sparc_st_imm(jit->ip, sparc_g1, sparc_sp, 92 + (slot - 6) * 4);
	}
}

static inline void emit_arguments(struct jit * jit)
{
	int assoc_gp = 0;
	int sreg = 0;

	for (int i = 0; i < jit->prepared_args.count; i++) {
		struct jit_out_arg * arg = &(jit->prepared_args.args[i]);
		long value = arg->value.generic;
		// integers
		if (!arg->isfp) {
			if (arg->isreg) {
				if (is_spilled(value, jit->prepared_args.op, &sreg))
					emit_set_arg_mem(jit, GET_REG_POS(jit, value), assoc_gp++);
				else emit_set_arg_reg(jit, sreg, assoc_gp++); 
			} else emit_set_arg_imm(jit, value, assoc_gp++);
			continue;
		} 

		// floats
		if (arg->size == sizeof(float)) {
			if (arg->isreg) {
				if (is_spilled(value, jit->prepared_args.op, &sreg)) {
					sparc_lddf_imm(jit->ip, sparc_fp, GET_REG_POS(jit, value), sparc_f30);
					sreg = sparc_f30;
				}
				sparc_fdtos(jit->ip, sreg, sparc_f30);
				emit_set_arg_freg(jit, sparc_f30, assoc_gp++);
			} else {
				float fl = (float)arg->value.fp;
				int fl_val;
				memcpy(&fl_val, &fl, sizeof(float));
				emit_set_arg_imm(jit, fl_val, assoc_gp++);
			}
		} else {
			// doubles
			if (arg->isreg) {
				long value = arg->value.generic;
				if (is_spilled(value, jit->prepared_args.op, &sreg)) {
					sparc_lddf_imm(jit->ip, sparc_fp, GET_REG_POS(jit, value), sparc_f30);
					sreg = sparc_f30;
				}
				emit_set_arg_freg(jit, sreg, assoc_gp++);
				emit_set_arg_freg(jit, sreg + 1, assoc_gp++);
			} else {
				int fl_val[2];
				memcpy(fl_val, &(arg->value.fp), sizeof(double));
				emit_set_arg_imm(jit, fl_val[0], assoc_gp++);
				emit_set_arg_imm(jit, fl_val[1], assoc_gp++);
			}
		}
	}
}

static inline void emit_funcall(struct jit * jit, struct jit_op * op, int imm)
{
	emit_arguments(jit);
	if (!imm) {
		sparc_call(jit->ip, op->r_arg[0], sparc_g0);
	} else {
		op->patch_addr = JIT_BUFFER_OFFSET(jit);
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

static void emit_get_arg_int(struct jit * jit, struct jit_inp_arg * arg, int dest_reg, int associated)
{
	int read_from_stack = 0;
	int stack_pos;

	if (!arg->passed_by_reg) {
		read_from_stack = 1;
		stack_pos = arg->location.stack_pos;
	}

	if (arg->passed_by_reg && !associated) {
		// the register is not associated and the value has to be read from the memory
		read_from_stack = 1;
		stack_pos = arg->spill_pos;
	}

	if (read_from_stack) sparc_ld_imm(jit->ip, sparc_fp, stack_pos, dest_reg);
	else sparc_mov_reg_reg(jit->ip, arg->location.reg, dest_reg);
}

static void emit_get_arg_float(struct jit * jit, struct jit_inp_arg * arg, int dest_reg, int associated)
{
	if (associated) {
		sparc_st_imm(jit->ip, arg->location.reg, sparc_fp, -8);
		sparc_ldf_imm(jit->ip, sparc_fp, -8, sparc_f30);
		sparc_fstod(jit->ip, sparc_f30, dest_reg);
	} else {
		sparc_ldf_imm(jit->ip, sparc_fp, arg->location.stack_pos, sparc_f30);
		sparc_fstod(jit->ip, sparc_f30, dest_reg);
	}
}

static void emit_get_arg_double(struct jit * jit, jit_op * op, struct jit_inp_arg * arg, int dest_reg, int associated)
{
	int arg_id = op->r_arg[1];

	// moves the first part of the register
	if (associated) {
		sparc_st_imm(jit->ip, arg->location.reg, sparc_fp, -8);
		sparc_ldf_imm(jit->ip, sparc_fp, -8, dest_reg);
	} else {
		sparc_ldf_imm(jit->ip, sparc_fp, arg->location.stack_pos, dest_reg);
	}

	// moves the second part
	int reg_id = jit_mkreg_ex(arg->type == JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, arg_id);
	associated = (rmap_get(op->regmap, reg_id) == NULL);

	struct jit_inp_arg arg2;
	init_arg(&arg2, arg->phys_reg + 1);

	if (associated && arg2.passed_by_reg) {
		sparc_st_imm(jit->ip, arg2.location.reg, sparc_fp, -4);
		sparc_ldf_imm(jit->ip, sparc_fp, -4, dest_reg + 1);
	} else {
		sparc_ldf_imm(jit->ip, sparc_fp, arg2.spill_pos, dest_reg + 1);
	}
}

static void emit_get_arg(struct jit * jit, jit_op * op)
{
	int dreg = op->r_arg[0];
	int arg_id = op->r_arg[1];

	struct jit_inp_arg * arg = &(jit_current_func_info(jit)->args[arg_id]);
	int reg_id = jit_mkreg(arg->type == JIT_FLOAT_NUM ? JIT_RTYPE_FLOAT : JIT_RTYPE_INT, JIT_RTYPE_ARG, arg_id);

	int associated = (rmap_get(op->regmap, reg_id) != NULL);
	
	if (arg->type != JIT_FLOAT_NUM) emit_get_arg_int(jit, arg, dreg, associated);
	else {
		if (arg->size == sizeof(float)) emit_get_arg_float(jit, arg, dreg, associated);
		if (arg->size == sizeof(double)) emit_get_arg_double(jit, op, arg, dreg, associated);

	}
}

void jit_patch_external_calls(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if ((op->code == (JIT_CALL | IMM)) && (!jit_is_label(jit, (void *)op->arg[0])))
			sparc_patch(jit->buf + (long)op->patch_addr, (long)op->r_arg[0]);
		if (GET_OP(op) == JIT_MSG)
			sparc_patch(jit->buf + (long)op->patch_addr, printf);
	}
}

void jit_patch_local_addrs(struct jit *jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
	
		if ((GET_OP(op) == JIT_REF_CODE) || (GET_OP(op) == JIT_REF_DATA)) {
			unsigned char *buf = jit->buf + (long) op->patch_addr;
			jit_value addr = jit_is_label(jit, (void *)op->arg[1]) ? ((jit_label *)op->arg[1])->pos : op->arg[1];
			sparc_set32x(buf, jit->buf + addr, op->r_arg[0]);
		}

		if ((GET_OP(op) == JIT_DATA_REF_CODE) || (GET_OP(op) == JIT_DATA_REF_DATA)) {
			unsigned char *buf = jit->buf + (long) op->patch_addr;
			jit_value addr = jit_is_label(jit, (void *)op->arg[0]) ? ((jit_label *)op->arg[0])->pos : op->arg[0];
			*((jit_value *)buf) = (jit_value) (jit->buf + addr);
		}
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
void emit_optimized_multiplication(struct jit * jit, long a1, long a2, long a3)
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


void emit_mul(struct jit * jit, jit_op * op)
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
			emit_optimized_multiplication(jit, a1, a2, a3);
			return;
		}
		if ((a3 < 0) && (_bit_pop(-a3) <= 5)) {
			emit_optimized_multiplication(jit, a1, a2, -a3);
			sparc_neg(jit->ip, a1);
			return;
		}
		sparc_smul_imm(jit->ip, FALSE, a2, a3, a1);
	} else sparc_smul(jit->ip, FALSE, a2, a3, a1);
}

// common function for floor & ceil ops
static void emit_sparc_round(struct jit * jit, long a1, long a2)
{
	static double zero_point_5 = 0.5;
	sparc_set32(jit->ip, 0, sparc_g1);

	// tests the sign of the value
	sparc_fabss(jit->ip, a2, sparc_f30);
	sparc_fmovs(jit->ip, a2 + 1, sparc_f31);
	sparc_fcmpd(jit->ip, a2, sparc_f30);

	sparc_set32(jit->ip, (long)&zero_point_5, sparc_g1);
	sparc_lddf(jit->ip, sparc_g1, sparc_g0, sparc_f30);

	// if a2 is greater than 0 -> skip negation
	unsigned char * br1 = jit->ip;
	sparc_fbranch(jit->ip, FALSE, sparc_fbge, 0); 
	sparc_nop(jit->ip);
	sparc_fnegs(jit->ip, sparc_f30, sparc_f30);

	sparc_patch(br1, jit->ip);

	// conversion 
	sparc_faddd(jit->ip, a2, sparc_f30, sparc_f30);
	sparc_fdtoi(jit->ip, sparc_f30, sparc_f30);
	sparc_stdf_imm(jit->ip, sparc_f30, sparc_fp, -8);
	sparc_ld_imm(jit->ip, sparc_fp, -8, a1);
}

// common function for floor & ceil ops
static void emit_sparc_floor(struct jit * jit, long a1, long a2, int floor)
{
	sparc_set32(jit->ip, 0, sparc_g1);

	// tests the sign of the value
	sparc_fabss(jit->ip, a2, sparc_f30);
	sparc_fmovs(jit->ip, a2 + 1, sparc_f31);
	sparc_fcmpd(jit->ip, a2, sparc_f30);

	unsigned char * br1 = jit->ip;
	sparc_fbranch(jit->ip, FALSE, (floor ? sparc_fbe : sparc_fbne), 0);
	sparc_nop(jit->ip);

	// branch1: conversion with correction
	sparc_fdtoi(jit->ip, a2, sparc_f30);
	sparc_fitod(jit->ip, sparc_f30, sparc_f30);

	sparc_fcmpd(jit->ip, a2, sparc_f30);

	unsigned char * br2 = jit->ip;
	sparc_fbranch(jit->ip, FALSE, sparc_fbe, 0); // skip correction if not desired
	sparc_nop(jit->ip);

	sparc_set32(jit->ip, (floor ? -1 : 1), sparc_g1);

	sparc_patch(br1, jit->ip);
	sparc_patch(br2, jit->ip);

	// branch2: direct conversion 
	sparc_fdtoi(jit->ip, a2, sparc_f30);
	sparc_stdf_imm(jit->ip, sparc_f30, sparc_fp, -8);
	sparc_ld_imm(jit->ip, sparc_fp, -8, a1);

	// adds correction
	sparc_add(jit->ip, FALSE, a1, sparc_g1, a1);
}

static inline void emit_ureg(struct jit * jit, long vreg, long hreg_id)
{
	if (JIT_REG(vreg).spec == JIT_RTYPE_ARG) {
		if (JIT_REG(vreg).type == JIT_RTYPE_INT) sparc_st_imm(jit->ip, hreg_id, sparc_fp, GET_REG_POS(jit, vreg));
		else {
			int arg_id = JIT_REG(vreg).id;
			struct jit_inp_arg * a = &(jit_current_func_info(jit)->args[arg_id]);
			if (a->passed_by_reg) sparc_st_imm(jit->ip, hreg_id, sparc_fp, a->spill_pos);
		} 
	}
	if (JIT_REG(vreg).spec == JIT_RTYPE_REG) {
		if (JIT_REG(vreg).type != JIT_RTYPE_FLOAT)
			sparc_st_imm(jit->ip, hreg_id, sparc_fp, GET_REG_POS(jit, vreg));
		else sparc_stdf_imm(jit->ip, hreg_id, sparc_fp, GET_REG_POS(jit, vreg));
	}
}

#define emit_alu_op(cc, reg_op, imm_op) \
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
		case JIT_ADD: emit_alu_op(FALSE, sparc_add, sparc_add_imm);
		case JIT_ADDC: emit_alu_op(sparc_cc, sparc_add, sparc_add_imm);
		case JIT_ADDX: emit_alu_op(FALSE, sparc_addx, sparc_addx_imm);
		case JIT_SUB: emit_alu_op(FALSE, sparc_sub, sparc_sub_imm);
		case JIT_SUBC: emit_alu_op(sparc_cc, sparc_sub, sparc_sub_imm);
		case JIT_SUBX: emit_alu_op(FALSE, sparc_subx, sparc_subx_imm);
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
		case JIT_OR:  emit_alu_op(FALSE, sparc_or, sparc_or_imm);
		case JIT_AND: emit_alu_op(FALSE, sparc_and, sparc_and_imm);
		case JIT_XOR: emit_alu_op(FALSE, sparc_xor, sparc_xor_imm);
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

		case JIT_MUL:  emit_mul(jit, op); break;

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
		case JIT_LT: emit_cond_op_op(jit, op, IS_SIGNED(op) ? sparc_bl : sparc_blu, IS_IMM(op)); break;
		case JIT_LE: emit_cond_op_op(jit, op, IS_SIGNED(op) ? sparc_ble : sparc_bleu, IS_IMM(op)); break;
		case JIT_GT: emit_cond_op_op(jit, op, IS_SIGNED(op) ? sparc_bg : sparc_bgu, IS_IMM(op)); break;
		case JIT_GE: emit_cond_op_op(jit, op, IS_SIGNED(op) ? sparc_bge : sparc_bgeu, IS_IMM(op)); break;
		case JIT_EQ: emit_cond_op_op(jit, op, sparc_be, IS_IMM(op)); break;
		case JIT_NE: emit_cond_op_op(jit, op, sparc_bne, IS_IMM(op)); break;
		case JIT_BLT: emit_branch_op(jit, op, IS_SIGNED(op) ? sparc_bl : sparc_blu, IS_IMM(op)); break;
		case JIT_BGT: emit_branch_op(jit, op, IS_SIGNED(op) ? sparc_bg : sparc_bgu, IS_IMM(op)); break;
		case JIT_BLE: emit_branch_op(jit, op, IS_SIGNED(op) ? sparc_ble : sparc_bleu, IS_IMM(op)); break;
		case JIT_BGE: emit_branch_op(jit, op, IS_SIGNED(op) ? sparc_bge : sparc_bgeu, IS_IMM(op)); break;
		case JIT_BEQ: emit_branch_op(jit, op, sparc_be, IS_IMM(op)); break;
		case JIT_BNE: emit_branch_op(jit, op, sparc_bne, IS_IMM(op)); break;
		case JIT_BMS: emit_branch_mask_op(jit, op, sparc_bne, IS_IMM(op)); break;
		case JIT_BMC: emit_branch_mask_op(jit, op, sparc_be, IS_IMM(op)); break;
		case JIT_BOADD: emit_op_and_overflow_branch(jit, op, JIT_ADD, IS_IMM(op), 0); break;
		case JIT_BOSUB: emit_op_and_overflow_branch(jit, op, JIT_SUB, IS_IMM(op), 0); break;
		case JIT_BNOADD: emit_op_and_overflow_branch(jit, op, JIT_ADD, IS_IMM(op), 1); break;
		case JIT_BNOSUB: emit_op_and_overflow_branch(jit, op, JIT_SUB, IS_IMM(op), 1); break;

		case JIT_CALL: emit_funcall(jit, op, IS_IMM(op)); break;

		case JIT_PATCH: do {
				struct jit_op *target = (struct jit_op *) a1;
				switch (GET_OP(target)) {
					case JIT_REF_CODE:
					case JIT_REF_DATA:
						target->arg[1] = JIT_BUFFER_OFFSET(jit);
						break;
					case JIT_DATA_REF_CODE:
					case JIT_DATA_REF_DATA:
						target->arg[0] = JIT_BUFFER_OFFSET(jit);
						break;
					default: {
						long pa = ((struct jit_op *)a1)->patch_addr;
						sparc_patch(jit->buf + pa, jit->ip);
					}
				}
			} while (0);
			break;

		case JIT_JMP:
			op->patch_addr = JIT_BUFFER_OFFSET(jit);
			if (op->code & REG) sparc_jmp(jit->ip, a1, sparc_g0);
			else sparc_branch(jit->ip, FALSE, sparc_balways, JIT_GET_ADDR(jit, op->r_arg[0]));
			sparc_nop(jit->ip);
			break;
		case JIT_RET:
			if (!IS_IMM(op) && (a1 != sparc_i0)) sparc_mov_reg_reg(jit->ip, a1, sparc_i0);
			if (IS_IMM(op)) sparc_set32(jit->ip, a1, sparc_i0);
			sparc_ret(jit->ip);
			sparc_restore_imm(jit->ip, sparc_g0, 0, sparc_g0);
			break;

		case JIT_PUTARG: funcall_put_arg(jit, op); break;
		case JIT_FPUTARG: funcall_fput_arg(jit, op); break;
		case JIT_GETARG: emit_get_arg(jit, op); break;
		case JIT_MSG:
				 sparc_set(jit->ip, a1, sparc_o0);
				 if (!IS_IMM(op)) sparc_mov_reg_reg(jit->ip, a2, sparc_o1);
				 op->patch_addr = JIT_BUFFER_OFFSET(jit);
				 sparc_call_simple(jit->ip, printf);
				 sparc_nop(jit->ip);
				 break;

		case JIT_ALLOCA: break;

		case JIT_CODE_ALIGN: { 
				int count = op->arg[0]; 
				assert(!(count % 4));
				while ((unsigned long)jit->ip % count) {
					if ((unsigned long)jit->ip % 4) {
						*jit->ip = 0;
						jit->ip++;
					}
				        else sparc_nop(jit->ip);
				}	
			}
			break;

		case JIT_REF_CODE:
		case JIT_REF_DATA:
			op->patch_addr = JIT_BUFFER_OFFSET(jit);
			sparc_set32x(jit->ip, 0xdeadbeef, a1);
			break;


		 // platform independent opcodes handled in the jitlib-core.c
		case JIT_DATA_BYTE: break;
		case JIT_FULL_SPILL: break;

		default: found = 0;
	}

op_complete:
	if (found) return;

	switch (op->code) {
		case (JIT_MOV | REG): if (a1 != a2) sparc_mov_reg_reg(jit->ip, a2, a1); break;
		case (JIT_MOV | IMM): sparc_set32(jit->ip, a2, a1); break;
		case JIT_PREPARE: funcall_prepare(jit, op, a1 + a2); break;
		case JIT_PROLOG:
			do {
				jit->current_func = op;
				struct jit_func_info * info = jit_current_func_info(jit);
				int stack_mem = 96;

				op->patch_addr = JIT_BUFFER_OFFSET(jit);
				stack_mem += info->allocai_mem;
				stack_mem += info->gp_reg_count * REG_SIZE;
				stack_mem += info->fp_reg_count * sizeof(double);
				stack_mem += info->float_arg_cnt * sizeof(double);
				stack_mem += space_for_outgoing_args(jit, op);

				stack_mem = jit_value_align(stack_mem, 16);
				sparc_save_imm(jit->ip, sparc_sp, -stack_mem, sparc_sp);
			} while (0);
			break;
		case JIT_RETVAL: 
				  // o0 is not a part of the reg. pool and thus 
				  // reg. allocator can not take care of the proper register assignment
				  if (a1 != sparc_o0) sparc_mov_reg_reg(jit->ip, sparc_o0, a1); 
				  break;

		case JIT_DECL_ARG: break;

		case JIT_LABEL: ((jit_label *)a1)->pos = JIT_BUFFER_OFFSET(jit); break; 

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

		//
		// Floating-point operations;
		//
		case (JIT_FMOV | REG):
			sparc_fmovs(jit->ip, a2, a1);
			sparc_fmovs(jit->ip, a2 + 1, a1 + 1);
			break;
		case (JIT_FMOV | IMM):
			sparc_set32(jit->ip, (long)&op->flt_imm, sparc_g1);
			sparc_lddf(jit->ip, sparc_g1, sparc_g0, a1);
			break;
		case (JIT_FADD | REG): sparc_faddd(jit->ip, a2, a3, a1); break;
		case (JIT_FSUB | REG): sparc_fsubd(jit->ip, a2, a3, a1); break;
		case (JIT_FRSB | REG): sparc_fsubd(jit->ip, a3, a2, a1); break;
		case (JIT_FMUL | REG): sparc_fmuld(jit->ip, a2, a3, a1); break;
		case (JIT_FDIV | REG): sparc_fdivd(jit->ip, a2, a3, a1); break;
		case (JIT_FNEG | REG): sparc_fnegs(jit->ip, a2, a1); break;

		case (JIT_FBLT | REG): emit_fpbranch_op(jit, op, sparc_fbl, a2, a3); break;
		case (JIT_FBGT | REG): emit_fpbranch_op(jit, op, sparc_fbg, a2, a3); break;
		case (JIT_FBLE | REG): emit_fpbranch_op(jit, op, sparc_fble, a2, a3); break;
		case (JIT_FBGE | REG): emit_fpbranch_op(jit, op, sparc_fbge, a2, a3); break;
		case (JIT_FBEQ | REG): emit_fpbranch_op(jit, op, sparc_fbe, a2, a3); break;
		case (JIT_FBNE | REG): emit_fpbranch_op(jit, op, sparc_fbne, a2, a3); break;
		case (JIT_TRUNC | REG): 
			sparc_fdtoi(jit->ip, a2, sparc_f30);
			sparc_stdf_imm(jit->ip, sparc_f30, sparc_fp, -8);
			sparc_ld_imm(jit->ip, sparc_fp, -8, a1);
			break;
		case (JIT_EXT | REG):
			sparc_st_imm(jit->ip, a2, sparc_fp, -8); 
			sparc_ldf_imm(jit->ip, sparc_fp, -8, sparc_f30);
			sparc_fitod(jit->ip, sparc_f30, a1);
			break;

		case (JIT_FLOOR | REG): emit_sparc_floor(jit, a1, a2, 1); break;
		case (JIT_CEIL | REG): emit_sparc_floor(jit, a1, a2, 0); break;
		case (JIT_ROUND | REG): emit_sparc_round(jit, a1, a2); break;

		case (JIT_FRET | REG):
			if (op->arg_size == sizeof(float)) {
				sparc_fdtos(jit->ip, a1, sparc_f0);
			} else {
				sparc_fmovs(jit->ip, a1, sparc_f0);
				sparc_fmovs(jit->ip, a1 + 1, sparc_f1);
			}
			sparc_ret(jit->ip);
			sparc_restore_imm(jit->ip, sparc_g0, 0, sparc_g0);
			break;

		case (JIT_FRETVAL):
			// reg. allocator takes care of proper assignment of the register
			if (op->arg_size == sizeof(float)) sparc_fstod(jit->ip, sparc_f0, sparc_f0);
			break;

		case (JIT_FLD | REG):
			if (op->arg_size == sizeof(double)) sparc_lddf(jit->ip, a2, sparc_g0, a1); 
			else {
				sparc_ldf(jit->ip, a2, sparc_g0, a1);
				sparc_fstod(jit->ip, a1, a1);
			}
			break;

		case (JIT_FLD | IMM):
			if (op->arg_size == sizeof(double)) sparc_lddf_imm(jit->ip, sparc_g0, a2, a1);
			else {
				sparc_ldf_imm(jit->ip, sparc_g0, a2, a1);
				sparc_fstod(jit->ip, a1, a1);
			}
			break;

		case (JIT_FLDX | REG):
			if (op->arg_size == sizeof(double)) sparc_lddf(jit->ip, a2, a3, a1); 
			else {
				sparc_ldf(jit->ip, a2, a3, a1);
				sparc_fstod(jit->ip, a1, a1);
			}
			break;

		case (JIT_FLDX | IMM):
			if (op->arg_size == sizeof(double)) sparc_lddf_imm(jit->ip, a2, a3, a1);
			else {
				sparc_ldf_imm(jit->ip, a2, a3, a1);
				sparc_fstod(jit->ip, a1, a1);
			}
			break;

		case (JIT_FST | REG):
			if (op->arg_size == sizeof(double)) sparc_stdf(jit->ip, a2, a1, sparc_g0);
			else {
				sparc_fdtos(jit->ip, a2, sparc_f30);
				sparc_stf(jit->ip, sparc_f30, a1, sparc_g0);
			}
			break;

		case (JIT_FST | IMM):
			if (op->arg_size == sizeof(double)) sparc_stdf_imm(jit->ip, a2, sparc_g0, a1);
			else {
				sparc_fdtos(jit->ip, a2, sparc_f30);
				sparc_stdf_imm(jit->ip, sparc_f30, sparc_g0, a1);
			}
			break;

		case (JIT_FSTX | REG):
			if (op->arg_size == sizeof(double)) sparc_stdf(jit->ip, a3, a2, a1);
			else {
				sparc_fdtos(jit->ip, a3, sparc_f30);
				sparc_stf(jit->ip, sparc_f30, a2, a1);
			}
			break;

		case (JIT_FSTX | IMM):
			if (op->arg_size == sizeof(double)) sparc_stdf_imm(jit->ip, a3, a2, a1);
			else {
				sparc_fdtos(jit->ip, a3, sparc_f30);
				sparc_stf_imm(jit->ip, sparc_f30, a2, a1);
			}
			break;

		case (JIT_UREG): emit_ureg(jit, a1, a2); break;
		case (JIT_SYNCREG): emit_ureg(jit, a1, a2); break;
		case (JIT_LREG): 
			if (JIT_REG(a2).spec == JIT_RTYPE_ARG) assert(0);
			if (JIT_REG(a2).type == JIT_RTYPE_INT)
				sparc_ld_imm(jit->ip, sparc_fp, GET_REG_POS(jit, a2), a1);
			else sparc_lddf_imm(jit->ip, sparc_fp, GET_REG_POS(jit, a2), a1);
			break;
		case JIT_RENAMEREG: sparc_mov_reg_reg(jit->ip, a2, a1); break;
		case JIT_CODESTART: break;
		case JIT_NOP: break;
		default: printf("sparc: unknown operation (opcode: 0x%x)\n", GET_OP(op) >> 3);
	}
}

struct jit_reg_allocator * jit_reg_allocator_create()
{
	struct jit_reg_allocator * a = JIT_MALLOC(sizeof(struct jit_reg_allocator));
	a->gp_reg_cnt = 14;
#ifdef JIT_REGISTER_TEST
	a->gp_reg_cnt -= 5;
#endif 
	a->gp_regs = JIT_MALLOC(sizeof(jit_hw_reg) * (a->gp_reg_cnt + 1));

	/* only the l0-l7 and i0-i5 registers are used
	 * all these registers are callee-saved withou special care
	 * register o0-o5 are used only for argument passing
	 * all g1-g3 are free for use in the codegenarator
	 */
	int i = 0;
	a->gp_regs[i++] = (jit_hw_reg) { sparc_i0, "i0", 1, 0, 11 };
	a->gp_regs[i++] = (jit_hw_reg) { sparc_i1, "i1", 1, 0, 12 };
	a->gp_regs[i++] = (jit_hw_reg) { sparc_i2, "i2", 1, 0, 13 };
	a->gp_regs[i++] = (jit_hw_reg) { sparc_i3, "i3", 1, 0, 14 };
	a->gp_regs[i++] = (jit_hw_reg) { sparc_i4, "i4", 1, 0, 15 };
	a->gp_regs[i++] = (jit_hw_reg) { sparc_i5, "i5", 1, 0, 16 };

	a->gp_regs[i++] = (jit_hw_reg) { sparc_l0, "l0", 1, 0, 1 };
	a->gp_regs[i++] = (jit_hw_reg) { sparc_l1, "l1", 1, 0, 2 };
	a->gp_regs[i++] = (jit_hw_reg) { sparc_l2, "l2", 1, 0, 3 };
#ifndef JIT_REGISTER_TEST
	a->gp_regs[i++] = (jit_hw_reg) { sparc_l3, "l3", 1, 0, 4 };
	a->gp_regs[i++] = (jit_hw_reg) { sparc_l4, "l4", 1, 0, 5 };
	a->gp_regs[i++] = (jit_hw_reg) { sparc_l5, "l5", 1, 0, 6 };
	a->gp_regs[i++] = (jit_hw_reg) { sparc_l6, "l6", 1, 0, 7 };
	a->gp_regs[i++] = (jit_hw_reg) { sparc_l7, "l7", 1, 0, 8 };
#endif

	a->gp_regs[i++] = (jit_hw_reg) { sparc_fp, "fp", 0, 0, 0 };

	a->fp_reg_cnt = 4;
	a->fp_regs = JIT_MALLOC(sizeof(jit_hw_reg) * a->fp_reg_cnt);

	i = 0;
	a->fp_regs[i++] = (jit_hw_reg) { sparc_f0, "f0", 0, 1, 1 };
	a->fp_regs[i++] = (jit_hw_reg) { sparc_f2, "f2", 0, 1, 2 };
	a->fp_regs[i++] = (jit_hw_reg) { sparc_f4, "f4", 0, 1, 3 };
	a->fp_regs[i++] = (jit_hw_reg) { sparc_f6, "f6", 0, 1, 4 };

	jit_hw_reg * reg_i7 = malloc(sizeof(jit_hw_reg));
	
	*reg_i7 = (jit_hw_reg) { sparc_i7, "iX", 1, 0, 0 };

	a->fp_reg = sparc_fp;
	a->ret_reg = NULL;
	a->fpret_reg = &(a->fp_regs[0]);


	a->gp_arg_reg_cnt = 6;
	a->gp_arg_regs = JIT_MALLOC(sizeof(jit_hw_reg *) * 6);
	for (int i = 0; i < 6; i++)
		a->gp_arg_regs[i] = &(a->gp_regs[i]);

	a->fp_arg_reg_cnt = 0;
	a->fp_arg_regs = NULL;

	return a;
}
