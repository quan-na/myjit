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
#define JIT_X86_ADDMUL	(0x0102 << 3)

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

void jit_optimize_frame_ptr(struct jit * jit)
{
	struct jit_func_info * info = NULL;
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if (GET_OP(op) == JIT_PROLOG) {
			info = (struct jit_func_info *) op->arg[1];
			info->uses_frame_ptr = 0;
		}
		if ((GET_OP(op) == JIT_ALLOCA) || (GET_OP(op) == JIT_UREG)
		|| (GET_OP(op) == JIT_LREG) || (GET_OP(op) == JIT_SYNCREG)) {
			info->uses_frame_ptr = 1;
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

			if (!jitset_get(op->live_out, op->arg[0])) {
				op->code = JIT_NOP;
				op->spec = SPEC(NO, NO, NO);
			}
		}
	}
}

static jit_op * get_related_op(jit_op * op, int result_reg)
{
	rb_node * hint_node = rb_search(op->next->allocator_hints, result_reg);
	if (!hint_node) return NULL;
	int expected_pos = ((struct jit_allocator_hint *)hint_node->value)->last_pos;

	for (jit_op * nextop = op->next; nextop != NULL; nextop = nextop->next) {
		if (expected_pos == nextop->normalized_pos) return nextop;
	}
	return NULL;
}

void jit_optimize_join_addmul(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		jit_value arg = op->arg[2];
		if ((((op->code == (JIT_MUL | IMM)) && ((arg == 2) || (arg == 4) || (arg == 8))))
		|| ((op->code == (JIT_LSH | IMM)) && ((arg == 1) || (arg == 2) || (arg == 3)))) {
			jit_value result_reg = op->arg[0];

			int m_arg;
			int s_arg;

			if (op->code == (JIT_LSH | IMM)) {
				s_arg = arg;
				if (arg == 1) m_arg = 2;
				if (arg == 2) m_arg = 4;
				if (arg == 3) m_arg = 8;
			} else {
				m_arg = arg;
				if (arg == 2) s_arg = 1;
				if (arg == 4) s_arg = 2;
				if (arg == 8) s_arg = 3;
			}

			jit_op * nextop = get_related_op(op, result_reg);

			if ((nextop->arg[0] != result_reg) && jitset_get(nextop->live_out, result_reg)) continue;

			jit_value mul_reg = op->arg[1];

			if (nextop->code == (JIT_ADD | REG)) {
				if (nextop->arg[1] != nextop->arg[2]) {
					jit_value add_reg = (nextop->arg[1] == result_reg) ? nextop->arg[2] : nextop->arg[1];

					op->code = JIT_NOP;
					op->spec = SPEC(NO, NO, NO);

					nextop->code = JIT_X86_ADDMUL | REG;
					nextop->spec = SPEC(REG, REG, REG);

					nextop->arg[1] = add_reg;
					nextop->arg[2] = mul_reg;
					nextop->arg_size = s_arg;
				}
			}

			if (nextop->code == (JIT_ADD | IMM)) {
				op->code = JIT_NOP;
				op->spec = SPEC(NO, NO, NO);

				nextop->code = JIT_X86_ADDMUL | IMM;
				nextop->spec = SPEC(REG, REG, IMM);

				nextop->arg[1] = mul_reg;
				nextop->arg_size = s_arg;
			}

			if (nextop->code == (JIT_OR | IMM)) {
				if ((nextop->arg[2] > 0) && (nextop->arg[2] < m_arg)) {
					op->code = JIT_NOP;
					op->spec = SPEC(NO, NO, NO);

					nextop->code = JIT_X86_ADDMUL | IMM;
					nextop->spec = SPEC(REG, REG, IMM);

					nextop->arg[1] = mul_reg;
					nextop->arg_size = s_arg;
				}
			}
		}
	}
}
