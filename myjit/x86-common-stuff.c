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
