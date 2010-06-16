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
		op->live_in = jitset_new(jit->reg_count);
		op->live_out = jitset_new(jit->reg_count);
		op = op->next;
	}
}

static inline int __flw_analyze_op(jit_op * op)
{
	int result;
	jitset * in1 = jitset_clone(op->live_in);
	jitset * out1 = jitset_clone(op->live_out);

	jitset_free(op->live_in);
	op->live_in = jitset_clone(op->live_out);

	for (int i = 0; i < 3; i++)
		if (ARG_TYPE(op, i + 1) == TREG) jitset_set(op->live_in, op->arg[i], 0);

	for (int i = 0; i < 3; i++)
		if (ARG_TYPE(op, i + 1) == REG)
			jitset_set(op->live_in, op->arg[i], 1);

	jitset_free(op->live_out);

#ifdef JIT_ARCH_AMD64
	// FIXME: magic constants
	if (GET_OP(op) == JIT_GETARG) {
		if (op->arg[1] == 0) jitset_set(op->live_in, 3, 1);
		if (op->arg[1] == 1) jitset_set(op->live_in, 4, 1);
		if (op->arg[1] == 2) jitset_set(op->live_in, 5, 1);
		if (op->arg[1] == 3) jitset_set(op->live_in, 6, 1);
		if (op->arg[1] == 4) jitset_set(op->live_in, 7, 1);
		if (op->arg[1] == 5) jitset_set(op->live_in, 8, 1);
	}
#endif

	if (GET_OP(op) == JIT_RET) {
		op->live_out = jitset_new(out1->size);
		goto skip;
	}

	if (GET_OP(op) == JIT_JMP) {
		op->live_out = jitset_clone(op->jmp_addr->live_in);
		goto skip;
	}

	if (op->next) op->live_out = jitset_clone(op->next->live_in);
	else op->live_out = jitset_new(out1->size);

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
	do {
		changed = 0;
		jit_op * op = jit_op_first(jit->ops);
		while (op) {
			changed |= __flw_analyze_op(op);
			op = op->next;
		}
	} while (changed);
}
