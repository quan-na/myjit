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

#include "set.h"
static inline void jit_flw_initialize(struct jit * jit)
{
	jit_op * op = jit_op_first(jit->ops);
	while (op) {
		op->live_in = jitset_new();
		op->live_out = jitset_new();
		op = op->next;
	}
}

static inline int __flw_analyze_op(struct jit * jit, jit_op * op, struct jit_func_info * func_info)
{
	int result;
	jitset * in1 = jitset_clone(op->live_in);
	jitset * out1 = jitset_clone(op->live_out);

	jitset_free(op->live_in);
	op->live_in = jitset_clone(op->live_out);

	for (int i = 0; i < 3; i++)
		if (ARG_TYPE(op, i + 1) == TREG) jitset_set(op->live_in, op->arg[i], 0);

// FIXME: should be generic for all architectures
// marks registers which are used to pass arguments
#if defined(JIT_ARCH_AMD64) 
	if (GET_OP(op) == JIT_PROLOG) {
		func_info = (struct jit_func_info *)op->arg[1];
		int argcount = MIN(func_info->general_arg_cnt, jit->reg_al->gp_arg_reg_cnt);
		for (int j = 0; j < argcount; j++)
			jitset_set(op->live_in, __mkreg(JIT_RTYPE_INT, JIT_RTYPE_ARG, j), 0); 

		argcount = MIN(func_info->float_arg_cnt, jit->reg_al->fp_arg_reg_cnt);
		for (int j = 0; j < argcount; j++)
			jitset_set(op->live_in, __mkreg(JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, j), 0); 
	}
#endif

#if defined(JIT_ARCH_SPARC)
	if (GET_OP(op) == JIT_PROLOG) {
		func_info = (struct jit_func_info *)op->arg[1];
		int assoc_gp = 0;
		int argcount = func_info->general_arg_cnt + func_info->float_arg_cnt;
		for (int j = 0; j < argcount; j++) {
			if (assoc_gp >= jit->reg_al->gp_arg_reg_cnt) break;
			if (func_info->args[j].type != JIT_FLOAT_NUM) {
				jitset_set(op->live_in, __mkreg(JIT_RTYPE_INT, JIT_RTYPE_ARG, j), 0);	
				assoc_gp++;
			} else {
				jitset_set(op->live_in, __mkreg(JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, j), 0);	
				assoc_gp++;
				if (func_info->args[j].size == sizeof(double)) {
					jitset_set(op->live_in, __mkreg_ex(JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, j), 0);	
					assoc_gp++;
				}
			}
		}
	}
#endif

	for (int i = 0; i < 3; i++)
		if (ARG_TYPE(op, i + 1) == REG)
			jitset_set(op->live_in, op->arg[i], 1);

#if defined(JIT_ARCH_AMD64) || defined(JIT_ARCH_SPARC)
	if (GET_OP(op) == JIT_GETARG) {
		int arg_id = op->arg[1];
		if (func_info->args[arg_id].type != JIT_FLOAT_NUM) {
			jitset_set(op->live_in, __mkreg(JIT_RTYPE_INT, JIT_RTYPE_ARG, arg_id), 1); 
		} else {
			jitset_set(op->live_in, __mkreg(JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, arg_id), 1); 
			if (func_info->args[arg_id].overflow)
				jitset_set(op->live_in, __mkreg_ex(JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, arg_id), 1); 
		}
	}
#endif

	jitset_free(op->live_out);

	if (GET_OP(op) == JIT_RET) {
		op->live_out = jitset_new(); 
		goto skip;
	}

	if (GET_OP(op) == JIT_JMP) {
		op->live_out = jitset_clone(op->jmp_addr->live_in);
		goto skip;
	}

	if (op->next) op->live_out = jitset_clone(op->next->live_in);
	else op->live_out = jitset_new();

	if (op->jmp_addr) jitset_or(op->live_out, op->jmp_addr->live_in);
skip:

	result = !(jitset_equal(in1, op->live_in) && jitset_equal(out1, op->live_out));
	jitset_free(in1);
	jitset_free(out1);
	return result;
}

static inline void jit_flw_analysis(struct jit * jit)
{
	jit_flw_initialize(jit);
	int changed;
	struct jit_func_info * func_info = NULL;
	do {
		changed = 0;
		jit_op * op = jit_op_first(jit->ops);
		while (op) {
			if (GET_OP(op) == JIT_PROLOG)
				func_info = (struct jit_func_info *)op->arg[1];

			changed |= __flw_analyze_op(jit, op, func_info);
			op = op->next;
		}
	} while (changed);
}
