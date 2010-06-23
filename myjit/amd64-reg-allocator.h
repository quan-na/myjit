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

#include <string.h>
/*
 * Register allocator -- auxiliary functions
 */

static inline void unload_reg(struct jit_reg_allocator * al, jit_op * op,  struct __hw_reg * hreg, long virt_reg)
{
	jit_op * o = __new_op(JIT_UREG, SPEC(IMM, IMM, NO), virt_reg, hreg->id, 0, 0);
	o->r_arg[0] = o->arg[0];
	o->r_arg[1] = o->arg[1];
	jit_op_prepend(op, o);
}

static inline void load_reg(struct jit_op * op, struct __hw_reg * hreg, long reg)
{
	struct jit_op * o = __new_op(JIT_LREG, SPEC(IMM, IMM, NO), hreg->id, reg, 0, 0);
	o->r_arg[0] = o->arg[0];
	o->r_arg[1] = o->arg[1];
	jit_op_prepend(op, o);
}

static inline struct __hw_reg * make_free_reg(struct jit * jit, jit_op * op)
{
	int spill_candidate = -1;
	int age = -1;
	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++) {
		if (op->regmap[i]) {
			struct __hw_reg * hreg = op->regmap[i];
			if ((int)hreg->used > age) {
				spill_candidate = i;
				age = hreg->used;
			}
		}
	}

	struct __hw_reg * hreg = op->regmap[spill_candidate];
	unload_reg(jit->reg_al, op, hreg, spill_candidate);
	op->regmap[spill_candidate] = NULL;
	hreg->used = 0;
	return hreg;
}

// moves unused registers back to the register pool
static inline void free_unused_regs(struct jit * jit, jit_op * op)
{
	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++) {
		if ((op->regmap[i] != NULL) 
			&& !(jitset_get(op->live_in, i) || jitset_get(op->live_out, i)))
		{
			struct jit_reg_allocator * al = jit->reg_al;
			al->hwreg_pool[++al->hwreg_pool_pos] = op->regmap[i];
			op->regmap[i] = NULL;
		}
	}
}

static inline struct __hw_reg * get_free_callee_save_reg(struct jit_reg_allocator * al)
{
	struct __hw_reg * result = NULL;
	for (int i = 0; i <= al->hwreg_pool_pos; i++) {
		if ((al->hwreg_pool[i]->id == AMD64_RBX)
		|| (al->hwreg_pool[i]->id == AMD64_R12)
		|| (al->hwreg_pool[i]->id == AMD64_R13)
		|| (al->hwreg_pool[i]->id == AMD64_R14)
		|| (al->hwreg_pool[i]->id == AMD64_R15)) {
			result = al->hwreg_pool[i];
			if (i < al->hwreg_pool_pos) al->hwreg_pool[i] = al->hwreg_pool[al->hwreg_pool_pos];
			al->hwreg_pool_pos--;
			break;
		}
	}
	return result;
}

static inline void assign_regs(struct jit * jit, struct jit_op * op)
{
	int i;
	struct jit_reg_allocator * al = jit->reg_al;

	// initialize mappings
	if (op->prev) memcpy(op->regmap, op->prev->regmap, sizeof(struct hw_reg *) * jit->reg_count);
	
	if (GET_OP(op) == JIT_PROLOG) {
		op->regmap[3] = &(al->hw_reg[5]); // RDI
		op->regmap[4] = &(al->hw_reg[4]); // RSI
		op->regmap[5] = &al->hw_reg[3]; // RDX
		op->regmap[6] = &al->hw_reg[2]; // RCX
		op->regmap[7] = &al->hw_reg[6]; // R8
		op->regmap[8] = &al->hw_reg[7]; // R9
	}
	free_unused_regs(jit, op);

	if (GET_OP(op) == JIT_PREPARE) {
		int args = op->arg[0];
		for (i = JIT_FIRST_REG; i < jit->reg_count; i++) {
			if (op->regmap[i]) {
				struct __hw_reg  * hreg = op->regmap[i];
				if (hreg->id == AMD64_RAX) {

					// checks whether there is a free callee-saved register
					// that can be used to store the eax register
					struct __hw_reg * freereg = get_free_callee_save_reg(al);
					if (freereg) {
						// should have its own name
						jit_op * o = __new_op(JIT_MOV | REG, SPEC(REG, REG, NO), i, i, 0, 0);
						o->r_arg[0] = freereg->id;
						o->r_arg[1] = AMD64_RAX;

						jit_op_prepend(op, o);
						op->regmap[i] = freereg;

					} else {
						jit_op * o = __new_op(JIT_SYNCREG, SPEC(IMM, IMM, NO), i, hreg->id, 0, 0);
						o->r_arg[0] = o->arg[0];
						o->r_arg[1] = o->arg[1];

						jit_op_prepend(op, o);
					}
				}
				if (((hreg->id == AMD64_RDI) && (args > 0))
				|| ((hreg->id == AMD64_RSI) && (args > 1))
				|| ((hreg->id == AMD64_RDX) && (args > 2))
				|| ((hreg->id == AMD64_RCX) && (args > 3))
				|| ((hreg->id == AMD64_R8) && (args > 4))
				|| ((hreg->id == AMD64_R9) && (args > 5))) {
					//jit_op * o = __new_op(JIT_SYNCREG, SPEC(IMM, IMM, NO), i, hreg->id, 0, 0);
					jit_op * o = __new_op(JIT_UREG, SPEC(IMM, IMM, NO), i, hreg->id, 0, 0);
					o->r_arg[0] = o->arg[0];
					o->r_arg[1] = o->arg[1];

					jit_op_prepend(op, o);

					al->hwreg_pool[++al->hwreg_pool_pos] = op->regmap[i];
					op->regmap[i] = NULL;
				}
			}
		}
	}

	if ((GET_OP(op) == JIT_PUSHARG)) {
		// PUSHARG takes care of register allocation by itself 
		return;
	}

	if ((GET_OP(op) == JIT_FINISH) || (GET_OP(op) == JIT_CALL)) {
		for (i = JIT_FIRST_REG; i < jit->reg_count; i++) {
			if (op->regmap[i]) {
				struct __hw_reg  * hreg = op->regmap[i];
				if (hreg->id == AMD64_RAX) {
					al->hwreg_pool[++al->hwreg_pool_pos] = hreg;
					op->regmap[i] = NULL;

					break;
				}
			}
		}
	}

	// get registers used in the current operation
	for (i = 0; i < 3; i++)
		if ((ARG_TYPE(op, i + 1) == REG) || (ARG_TYPE(op, i + 1) == TREG)) {
			// in order to eliminate unwanted spilling of registers used
			// by the current operation, we ``touch'' the registers
			// which are to be used in this operation
			if (op->regmap[op->arg[i]]) op->regmap[op->arg[i]]->used = 0;
		}

	for (i = 0; i < 3; i++) {
		if ((ARG_TYPE(op, i + 1) == REG) || (ARG_TYPE(op, i + 1) == TREG)) {
			if (op->arg[i] == JIT_RETREG) {
				op->r_arg[i] = AMD64_RAX;
				continue;
			}
			if (op->arg[i] == JIT_FP) {
				op->r_arg[i] = AMD64_RBP;
				continue;
			}

			if (op->regmap[op->arg[i]] != NULL) op->r_arg[i] = op->regmap[op->arg[i]]->id;
			else {
				struct __hw_reg * reg;
				if (al->hwreg_pool_pos < 0) reg = make_free_reg(jit, op); 
				else reg = al->hwreg_pool[al->hwreg_pool_pos--];

				op->regmap[op->arg[i]] = reg;
				op->r_arg[i] = reg->id;
				if (jitset_get(op->live_in, op->arg[i])) {
					load_reg(op, op->regmap[op->arg[i]], op->arg[i]);
				}

			}
		} else if (ARG_TYPE(op, i + 1) == IMM) {
			// assigns immediate values
			op->r_arg[i] = op->arg[i];
		}
	}

	// increasing age of each register
	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++) {
		struct __hw_reg * reg = op->regmap[i];
		if (reg) reg->used++;
	}
}

static inline void jump_adjustment(struct jit * jit, jit_op * op)
{
	if (GET_OP(op) != JIT_JMP) return;
	struct __hw_reg ** cur_regmap = op->regmap;
	struct __hw_reg ** tgt_regmap = op->jmp_addr->regmap;

	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++)
		if (cur_regmap[i] != tgt_regmap[i])
			if (cur_regmap[i]) unload_reg(jit->reg_al, op, cur_regmap[i], i);

	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++)
		if (tgt_regmap[i] && (cur_regmap[i] != tgt_regmap[i]))
			load_reg(op, tgt_regmap[i], i);
}

static inline void branch_adjustment(struct jit * jit, jit_op * op)
{
	// beq(succ, ..., ...);
	// fail
	// ==>
	// bne(fail:, ..., ...);
	// register mangling
	// jmp succ
	// fail:
	if ((GET_OP(op) != JIT_BEQ) && (GET_OP(op) != JIT_BGT) && (GET_OP(op) != JIT_BGE)
	   && (GET_OP(op) != JIT_BNE) && (GET_OP(op) != JIT_BLT) && (GET_OP(op) != JIT_BLE)) return;

	struct __hw_reg ** cur_regmap = op->regmap;
	struct __hw_reg ** tgt_regmap = op->jmp_addr->regmap;
	int adjust = 0;

	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++)
		if (cur_regmap[i] != tgt_regmap[i]) {
			adjust = 1;
			break;
		}
	if (adjust) {
		switch (GET_OP(op)) {
			case JIT_BEQ: op->code = JIT_BNE | (op->code & 0x7); break;
			case JIT_BGT: op->code = JIT_BLE | (op->code & 0x7); break;
			case JIT_BGE: op->code = JIT_BLT | (op->code & 0x7); break;
			case JIT_BNE: op->code = JIT_BEQ | (op->code & 0x7); break;
			case JIT_BLT: op->code = JIT_BGE | (op->code & 0x7); break;
			case JIT_BLE: op->code = JIT_BGT | (op->code & 0x7); break;
		}
	
		jit_op * o = __new_op(JIT_JMP | IMM, SPEC(IMM, NO, NO), op->arg[0], 0, 0, 0);		
		o->r_arg[0] = op->r_arg[0];
	
		o->regmap = JIT_MALLOC(sizeof(struct hw_reg *) * (jit->reg_count + 2));
		memcpy(o->regmap, op->regmap, sizeof(struct hw_reg *) * (jit->reg_count + 2));

		o->live_in = jitset_clone(op->live_in); 
		o->live_out = jitset_clone(op->live_out); 
		o->jmp_addr = op->jmp_addr;

		if (!jit_is_label(jit, (void *)op->r_arg[0])) {
			op->jmp_addr->arg[0] = (long)o;
			op->jmp_addr->r_arg[0] = (long)o;
		}

		jit_op_append(op, o);

		jit_op * o2 = __new_op(JIT_PATCH, SPEC(IMM, NO, NO), (long)op, 0, 0, 0);
		o2->r_arg[0] = o2->arg[0];
		jit_op_append(o, o2);

		op->arg[0] = (long)o2;
		op->r_arg[0] = (long)o2;
		op->jmp_addr = o2;
	}
}

/*
 * API provided by allocator
 */
struct jit_reg_allocator * jit_reg_allocator_create(unsigned int regcnt)
{
	struct jit_reg_allocator * a = JIT_MALLOC(sizeof(struct jit_reg_allocator));
	a->hw_reg_cnt = 14;
	a->hwreg_pool = JIT_MALLOC(sizeof(struct __hw_reg *) * a->hw_reg_cnt);

	a->hw_reg = malloc(sizeof(struct __hw_reg) * (a->hw_reg_cnt + 1));

	a->hw_reg[0] = (struct __hw_reg) { AMD64_RAX, 0, "rax" };
	a->hw_reg[1] = (struct __hw_reg) { AMD64_RBX, 0, "rbx" };
	a->hw_reg[2] = (struct __hw_reg) { AMD64_RCX, 0, "rcx" };
	a->hw_reg[3] = (struct __hw_reg) { AMD64_RDX, 0, "rdx" };
	a->hw_reg[4] = (struct __hw_reg) { AMD64_RSI, 0, "rsi" };
	a->hw_reg[5] = (struct __hw_reg) { AMD64_RDI, 0, "rdi" };
	a->hw_reg[6] = (struct __hw_reg) { AMD64_R8, 0, "r8" };
	a->hw_reg[7] = (struct __hw_reg) { AMD64_R9, 0, "r9" };
	a->hw_reg[8] = (struct __hw_reg) { AMD64_R10, 0, "r10" };
	a->hw_reg[9] = (struct __hw_reg) { AMD64_R11, 0, "r11" };
	a->hw_reg[10] = (struct __hw_reg) { AMD64_R12, 0, "r12" };
	a->hw_reg[11] = (struct __hw_reg) { AMD64_R13, 0, "r13" };
	a->hw_reg[12] = (struct __hw_reg) { AMD64_R14, 0, "r14" };
	a->hw_reg[13] = (struct __hw_reg) { AMD64_R15, 0, "r15" };

	a->hw_reg[14] = (struct __hw_reg) { AMD64_RBP, 0, "rbp" };

	/*
	a->hwreg_pool_pos = 2;
	a->hwreg_pool[0] = &(a->hw_reg[2]);
	a->hwreg_pool[1] = &(a->hw_reg[1]);
	a->hwreg_pool[2] = &(a->hw_reg[0]);
*/
	// FIXME: more sophisticated order
	a->hwreg_pool_pos = 7;
	a->hwreg_pool[0] = &(a->hw_reg[13]);
	a->hwreg_pool[1] = &(a->hw_reg[12]);
	a->hwreg_pool[2] = &(a->hw_reg[11]);
	a->hwreg_pool[3] = &(a->hw_reg[10]);
	a->hwreg_pool[4] = &(a->hw_reg[9]);
	a->hwreg_pool[5] = &(a->hw_reg[8]);
	a->hwreg_pool[6] = &(a->hw_reg[1]);
	a->hwreg_pool[7] = &(a->hw_reg[0]);
	return a;
}

void jit_assign_regs(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		op->regmap = JIT_MALLOC(sizeof(struct hw_reg *) * jit->reg_count);
		op->regmap[JIT_FP] = &(jit->reg_al->hw_reg[14]);
		op->regmap[JIT_RETREG] = &(jit->reg_al->hw_reg[0]);
		for (int i = 2; i < jit->reg_count; i++)
			op->regmap[i] = NULL;
	}

	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		assign_regs(jit, op);

	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		branch_adjustment(jit, op);

	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		jump_adjustment(jit, op);
}

void jit_reg_allocator_free(struct jit_reg_allocator * a)
{
	JIT_FREE(a->hwreg_pool);
	JIT_FREE(a->hw_reg);
	JIT_FREE(a);
}

char * jit_reg_allocator_get_hwreg_name(struct jit_reg_allocator * al, int reg)
{
	for (int i = 0; i < al->hw_reg_cnt; i++)
		if (al->hw_reg[i].id == reg) return al->hw_reg[i].name;
	return NULL;
}
