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

/**
 * Creates an empty register pool
 */
struct jit_regpool * jit_regpool_init(int size)
{
	struct jit_regpool * r = JIT_MALLOC(sizeof(struct jit_regpool));
	r->pool = JIT_MALLOC(sizeof(struct __hw_reg *) * size);
	r->pos = 0;
	return r;
}

void jit_regpool_free(struct jit_regpool * p)
{
	JIT_FREE(p->pool);
	JIT_FREE(p);
}

static inline void jit_regpool_put(struct jit_regpool * rp, struct __hw_reg * hreg)
{
	rp->pool[++rp->pos] = hreg;
	
	// reorders registers according to their priority
#ifdef JIT_ARCH_SPARC
	int i = rp->pos;
	while (i > 0) {
		if (rp->pool[i - 1]->priority <= rp->pool[i]->priority) break;
		struct __hw_reg * x = rp->pool[i];
		rp->pool[i] = rp->pool[i - 1];
		rp->pool[i - 1] = x;
		i--;
	}
#endif
}

static inline struct __hw_reg * jit_regpool_get(struct jit_regpool * rp)
{
	if (rp->pos < 0) return NULL;
	else return rp->pool[rp->pos--];
}

static inline struct __hw_reg * jit_regpool_get_by_id(struct jit_regpool * rp, int id)
{
	for (int i = 0; i <= rp->pos; i++) {
		if (rp->pool[i]->id == id) {
			struct __hw_reg * hreg = rp->pool[i];
			for (int j = i; j < rp->pos - 1; j++)
				rp->pool[j] = rp->pool[j + 1];
			rp->pos--;
			return hreg;
		}
	}
	return NULL;
}

/**
 * Adds all registers which are not used to pass the arguments into the register pool
 */
static inline void jit_regpool_prepare(struct jit_regpool * rp, struct __hw_reg * regs, int regcnt, int * arg_registers, int arg_registers_cnt)
{
	rp->pos = -1;
	for (int i = 0; i < regcnt; i++) {
		struct __hw_reg * hreg = &(regs[i]);

		int is_argument = 0;
		for (int j = 0; j < arg_registers_cnt; j++) {
			if (hreg->id == arg_registers[j]) {
				is_argument = 1;
				break;
			}
		}
		if (!is_argument) jit_regpool_put(rp, hreg);
	}
}

static inline struct __hw_reg * __get_reg(struct jit_reg_allocator * al, int reg_id)
{
	for (int i = 0; i < al->gp_reg_cnt; i++)
		if (al->gp_regs[i].id == reg_id) return &(al->gp_regs[i]);
	return NULL;
}

static inline struct __hw_reg * __get_fp_reg(struct jit_reg_allocator * al, int reg_id)
{
	for (int i = 0; i < al->fp_reg_cnt; i++)
		if (al->fp_regs[i].id == reg_id) return &(al->fp_regs[i]);
	return NULL;
}

///////////////////////////////////////////////////////////////////

static inline void unload_reg(jit_op * op,  struct __hw_reg * hreg, long virt_reg)
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

static struct __hw_reg * make_free_reg(jit_op * op, int fp)
{
	int spill_candidate;
	struct __hw_reg * hreg = rmap_spill_candidate(op, fp, &spill_candidate);
	unload_reg(op, hreg, spill_candidate);
	rmap_unassoc(op->regmap, spill_candidate, fp);
	hreg->used = 0;
	return hreg;
}

static inline struct __hw_reg * get_free_callee_save_reg(struct jit_regpool * rp)
{
	struct __hw_reg * result = NULL;
	for (int i = 0; i <= rp->pos; i++) {
		if (rp->pool[i]->callee_saved) {
			result = rp->pool[i];
			if (i < rp->pos) rp->pool[i] = rp->pool[rp->pos];
			rp->pos--;
			break;
		}
	}
	return result;
}

///////////////////////////////////////////////////////////////////////////////

static inline void assign_regs(struct jit * jit, struct jit_op * op)
{
	int i, r, reg;
	struct jit_reg_allocator * al = jit->reg_al;

	// initialize mappings
	// PROLOG needs some special care
	if (GET_OP(op) == JIT_PROLOG) {
		
		struct jit_func_info * info = (struct jit_func_info *) op->arg[1];

		op->regmap = rmap_init();
		jit_regpool_prepare(al->gp_regpool, al->gp_regs, al->gp_reg_cnt, al->gp_arg_regs, MIN(info->general_arg_cnt, al->gp_arg_reg_cnt));

		jit_regpool_prepare(al->fp_regpool, al->fp_regs, al->fp_reg_cnt, al->fp_arg_regs, MIN(info->float_arg_cnt, al->fp_arg_reg_cnt));

		int assoc_gp_regs = 0;
		int assoc_fp_regs = 0;
		for (int i = 0; i < info->general_arg_cnt + info->float_arg_cnt; i++) {
			int isfp_arg = (info->args[i].type == JIT_FLOAT_NUM);
			if (!isfp_arg && (assoc_gp_regs < al->gp_arg_reg_cnt)) {
				rmap_assoc(op->regmap, __mkreg(JIT_RTYPE_INT, JIT_RTYPE_ARG, i), __get_reg(al, al->gp_arg_regs[i]));
				assoc_gp_regs++;
			}
			if (isfp_arg && (assoc_fp_regs < al->fp_arg_reg_cnt)) {
				rmap_assoc(op->regmap, __mkreg(JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, i), __get_fp_reg(al, al->fp_arg_regs[i]));
				assoc_fp_regs++;
			}
		}
	} else {
		// initializes register mappings for standard operations
		if (op->prev) op->regmap = rmap_clone_without_unused_regs(jit, op->prev->regmap, op); 
	}

	if (GET_OP(op) == JIT_PREPARE) {
		struct __hw_reg * hreg = rmap_is_associated(op->regmap, al->ret_reg, 0, &r);
		if (hreg) {
			// checks whether there is a free callee-saved register
			// that can be used to store the eax register
			struct __hw_reg * freereg = get_free_callee_save_reg(al->gp_regpool);
			if (freereg) {
				// should have its own name
				jit_op * o = __new_op(JIT_RENAMEREG, SPEC(IMM, IMM, NO), freereg->id, al->ret_reg, 0, 0);

				o->r_arg[0] = o->arg[0];
				o->r_arg[1] = o->arg[1];

				jit_op_prepend(op, o);
				rmap_assoc(op->regmap, r, freereg);

				jit_regpool_put(al->gp_regpool, hreg);
			} else {
				jit_op * o = __new_op(JIT_SYNCREG, SPEC(IMM, IMM, NO), r, hreg->id, 0, 0);
				o->r_arg[0] = o->arg[0];
				o->r_arg[1] = o->arg[1];

				jit_op_prepend(op, o);
			}
		}

		// synchronizes fp-register which can be used to return the value
		if (al->fpret_reg >= 0) {
			hreg = rmap_is_associated(op->regmap, al->fpret_reg, 1, &r);
			if (hreg) {
				jit_op * o = __new_op(JIT_SYNCREG, SPEC(IMM, IMM, NO), r, hreg->id, 0, 0);
				o->r_arg[0] = o->arg[0];
				o->r_arg[1] = o->arg[1];

				jit_op_prepend(op, o);
			}
		}

		// spills registers which are used to pass the arguments
		// FIXME: duplicities
		int args = MIN(op->arg[0], al->gp_arg_reg_cnt);
		for (int q = 0; q < args; q++) {
			struct __hw_reg * hreg = rmap_is_associated(op->regmap, al->gp_arg_regs[q], 0, &reg);
			if (hreg) {
				unload_reg(op, hreg, reg);
				jit_regpool_put(al->gp_regpool, hreg);
				rmap_unassoc(op->regmap, reg, 0);
			}
		}
		args = MIN(op->arg[1], al->fp_arg_reg_cnt);
		for (int q = 0; q < args; q++) {
			struct __hw_reg * hreg = rmap_is_associated(op->regmap, al->fp_arg_regs[q], 1, &reg);
			if (hreg) {
				if (hreg->id != al->fpret_reg) {
					unload_reg(op, hreg, reg);
					jit_regpool_put(al->fp_regpool, hreg);
				}
				rmap_unassoc(op->regmap, reg, 0);
			}
		}
	}

	// PUTARG have to take care of the register allocation by itself
	if ((GET_OP(op) == JIT_PUTARG) || (GET_OP(op) == JIT_FPUTARG)) return;

	if (GET_OP(op) == JIT_RETVAL) {
		rmap_assoc(op->regmap, op->arg[0], jit_regpool_get_by_id(al->gp_regpool, al->ret_reg));
		return;
	}

	if (GET_OP(op) == JIT_CALL) {

		// spills register which are used to return value
		struct __hw_reg * hreg = rmap_is_associated(op->regmap, al->ret_reg, 0, &r);
		if (hreg) {
			jit_regpool_put(al->gp_regpool, hreg);
			rmap_unassoc(op->regmap, r, hreg->fp);
		}
		if (al->fpret_reg >= 0) {
			hreg = rmap_is_associated(op->regmap, al->fpret_reg, 1, &r);
			if (hreg) {
				jit_regpool_put(al->fp_regpool, hreg);
				rmap_unassoc(op->regmap, r, hreg->fp);
			}
		}
	}

	// get registers used in the current operation
	for (i = 0; i < 3; i++)
		if ((ARG_TYPE(op, i + 1) == REG) || (ARG_TYPE(op, i + 1) == TREG)) {
			// in order to eliminate unwanted spilling of registers used
			// by the current operation, we ``touch'' the registers
			// which are to be used in this operation
			struct __hw_reg * hreg = rmap_get(op->regmap, op->arg[i]);
			if (hreg) hreg->used = 0;
		}

	for (i = 0; i < 3; i++) {
		if ((ARG_TYPE(op, i + 1) == REG) || (ARG_TYPE(op, i + 1) == TREG)) {
			jit_reg virt_reg = JIT_REG(op->arg[i]);
			if (virt_reg.spec == JIT_RTYPE_ALIAS) {
				if ((int)op->arg[i] == (int)R_OUT) op->r_arg[i] = al->ret_reg;
				else if ((int)op->arg[i] == (int)R_FP) op->r_arg[i] = al->fp_reg;
				else assert(0);
				continue;
			}

			struct __hw_reg * reg = rmap_get(op->regmap, op->arg[i]);
			if (reg) op->r_arg[i] = reg->id;
			else {
				if (virt_reg.type == JIT_RTYPE_INT) {
					reg = jit_regpool_get(al->gp_regpool);
					if (reg == NULL) reg = make_free_reg(op, 0);
				} else {
					reg = jit_regpool_get(al->fp_regpool);
					if (reg == NULL) reg = make_free_reg(op, 1);
				}

				rmap_assoc(op->regmap, op->arg[i], reg);

				op->r_arg[i] = reg->id;
				if (jitset_get(op->live_in, op->arg[i]))
					load_reg(op, rmap_get(op->regmap, op->arg[i]), op->arg[i]);
			}
		} else if (ARG_TYPE(op, i + 1) == IMM) {
			// assigns immediate values
			op->r_arg[i] = op->arg[i];
		}
	}

	// increasing age of each register
	rmap_make_older(op->regmap);
}

static inline void jump_adjustment(struct jit * jit, jit_op * op)
{
	if (GET_OP(op) != JIT_JMP) return;
	rmap_t * cur_regmap = op->regmap;
	rmap_t * tgt_regmap = op->jmp_addr->regmap;

	rmap_sync(op, cur_regmap, tgt_regmap, RMAP_UNLOAD);
	rmap_sync(op, tgt_regmap, cur_regmap, RMAP_LOAD);
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

	rmap_t * cur_regmap = op->regmap;
	rmap_t * tgt_regmap = op->jmp_addr->regmap;

	if (!rmap_equal(cur_regmap, tgt_regmap)) {
		// FIXME: floating points
		switch (GET_OP(op)) {
			case JIT_BEQ: op->code = JIT_BNE | (op->code & 0x7); break;
			case JIT_BGT: op->code = JIT_BLE | (op->code & 0x7); break;
			case JIT_BGE: op->code = JIT_BLT | (op->code & 0x7); break;
			case JIT_BNE: op->code = JIT_BEQ | (op->code & 0x7); break;
			case JIT_BLT: op->code = JIT_BGE | (op->code & 0x7); break;
			case JIT_BLE: op->code = JIT_BGT | (op->code & 0x7); break;

			case JIT_FBEQ: op->code = JIT_FBNE | (op->code & 0x7); break;
			case JIT_FBGT: op->code = JIT_FBLE | (op->code & 0x7); break;
			case JIT_FBGE: op->code = JIT_FBLT | (op->code & 0x7); break;
			case JIT_FBNE: op->code = JIT_FBEQ | (op->code & 0x7); break;
			case JIT_FBLT: op->code = JIT_FBGE | (op->code & 0x7); break;
			case JIT_FBLE: op->code = JIT_FBGT | (op->code & 0x7); break;
		}
	
		jit_op * o = __new_op(JIT_JMP | IMM, SPEC(IMM, NO, NO), op->arg[0], 0, 0, 0);		
		o->r_arg[0] = op->r_arg[0];

		o->regmap = rmap_clone(op->regmap);
	
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

void jit_assign_regs(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		op->regmap = rmap_init();

	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		assign_regs(jit, op);

	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		branch_adjustment(jit, op);

	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		jump_adjustment(jit, op);
}

void jit_reg_allocator_free(struct jit_reg_allocator * a)
{
	jit_regpool_free(a->gp_regpool);
	jit_regpool_free(a->fp_regpool);
	if (a->fp_regs) JIT_FREE(a->fp_regs);
	JIT_FREE(a->gp_regs);
	JIT_FREE(a);
}

/**
 * @return true if the given hw. register (e.g., X86_EAX) is in use
 */
int jit_reg_in_use(jit_op * op, int reg, int fp)
{
	return rmap_is_associated(op->regmap, reg, fp, NULL) != NULL;
}
