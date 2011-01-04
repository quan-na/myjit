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
	r->pool = JIT_MALLOC(sizeof(jit_hw_reg *) * size);
	r->pos = 0;
	return r;
}

void jit_regpool_free(struct jit_regpool * p)
{
	JIT_FREE(p->pool);
	JIT_FREE(p);
}

static inline void jit_regpool_put(struct jit_regpool * rp, jit_hw_reg * hreg)
{
	rp->pool[++rp->pos] = hreg;
	
	// reorders registers according to their priority
#if defined(JIT_ARCH_SPARC) || defined(JIT_ARCH_AMD64)
	int i = rp->pos;
	while (i > 0) {
		if (rp->pool[i - 1]->priority <= rp->pool[i]->priority) break;
		jit_hw_reg * x = rp->pool[i];
		rp->pool[i] = rp->pool[i - 1];
		rp->pool[i - 1] = x;
		i--;
	}
	/*
	for (int i = 0; i < rp->pos; i++)
		printf("%s ", rp->pool[i]->name);
	printf("\n");
	*/
#endif
}

static inline jit_hw_reg * jit_regpool_get(struct jit_regpool * rp)
{
	if (rp->pos < 0) return NULL;
	else return rp->pool[rp->pos--];
}

static inline jit_hw_reg * jit_regpool_get_by_id(struct jit_regpool * rp, int id)
{
	for (int i = 0; i <= rp->pos; i++) {
		if (rp->pool[i]->id == id) {
			jit_hw_reg * hreg = rp->pool[i];
			for (int j = i; j < rp->pos; j++)
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
static inline void jit_regpool_prepare(struct jit_regpool * rp, jit_hw_reg * regs, int regcnt, int * arg_registers, int arg_registers_cnt)
{
	rp->pos = -1;
	for (int i = 0; i < regcnt; i++) {
		jit_hw_reg * hreg = &(regs[i]);

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

static inline jit_hw_reg * __get_reg(struct jit_reg_allocator * al, int reg_id)
{
	for (int i = 0; i < al->gp_reg_cnt; i++)
		if (al->gp_regs[i].id == reg_id) return &(al->gp_regs[i]);
	return NULL;
}

static inline jit_hw_reg * __get_fp_reg(struct jit_reg_allocator * al, int reg_id)
{
	for (int i = 0; i < al->fp_reg_cnt; i++)
		if (al->fp_regs[i].id == reg_id) return &(al->fp_regs[i]);
	return NULL;
}

///////////////////////////////////////////////////////////////////

//
// Auxiliary functions which are used to insert operations
// moving values from/to registers
// 

/**
 * Prepends given LREG/UREG/etc. operation before `op' operation
 */
static void insert_reg_op(int opcode, jit_op * op,  jit_value r1, jit_value r2)
{
	jit_op * o = __new_op(opcode, SPEC(IMM, IMM, NO), r1, r2, 0, 0);
	o->r_arg[0] = o->arg[0];
	o->r_arg[1] = o->arg[1];
	jit_op_prepend(op, o);
}
static void unload_reg(jit_op * op,  jit_hw_reg * hreg, jit_value virt_reg)
{
	insert_reg_op(JIT_UREG, op, virt_reg, hreg->id);
}

static void sync_reg(jit_op * op,  jit_hw_reg * hreg, jit_value virt_reg)
{
	insert_reg_op(JIT_SYNCREG, op, virt_reg, hreg->id);
}

static void load_reg(jit_op * op, jit_hw_reg * hreg, jit_value reg)
{
	insert_reg_op(JIT_LREG, op, hreg->id, reg);
}

/**
 * Renames register (actually, it assigns value from r2 to r1) 
 */
static void rename_reg(jit_op * op, int r1, int r2)
{
	insert_reg_op(JIT_RENAMEREG, op, r1, r2);
}

static jit_hw_reg * make_free_reg2(struct jit_reg_allocator * al, jit_op * op, jit_value for_reg)
{
	int spill;
	jit_value spill_candidate;
#ifdef LTU_ALLOCATOR
	jit_hw_reg * hreg = rmap_spill_candidate2(al, op, for_reg, &spill, &spill_candidate);
#endif
#ifdef LRU_ALLOCATOR
	jit_hw_reg * hreg = rmap_spill_candidate(op, 0, &spill_candidate);
#endif

	if (jitset_get(op->live_in, spill_candidate))
		unload_reg(op, hreg, spill_candidate);
	rmap_unassoc(op->regmap, spill_candidate, JIT_REG(for_reg).type == JIT_RTYPE_FLOAT);
	hreg->used = 0;
	return hreg;
}

static jit_hw_reg * make_free_reg(jit_op * op, int fp)
{
	jit_value spill_candidate;
	jit_hw_reg * hreg = rmap_spill_candidate(op, fp, &spill_candidate);
	if (jitset_get(op->live_in, spill_candidate))
		unload_reg(op, hreg, spill_candidate);
	rmap_unassoc(op->regmap, spill_candidate, fp);
	hreg->used = 0;
	return hreg;
}

static inline jit_hw_reg * get_free_callee_save_reg(struct jit_regpool * rp)
{
	jit_hw_reg * result = NULL;
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

////////////////////////////////////////////////////////////////////////////////

#ifdef JIT_ARCH_COMMON86
/**
 * Assigns registers which are used to pass arguments
 */
static void assign_regs_for_args(struct jit_reg_allocator * al, jit_op * op)
{
	struct jit_func_info * info = (struct jit_func_info *) op->arg[1];

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
}
#endif

#ifdef JIT_ARCH_SPARC
/**
 * Assigns registers which are used to pass arguments
 *
 * This function has to consider that SPARC is using GPR to pass
 * FP values. Doubles are passed using two GPR's
 */
static void assign_regs_for_args(struct jit_reg_allocator * al, jit_op * op)
{
	// counts howmany registers are really used to pass FP arguments
	int fp_arg_cnt = 0;
	struct jit_func_info * info = (struct jit_func_info *) op->arg[1];
	for (int i = 0; i < info->general_arg_cnt + info->float_arg_cnt; i++) {
		struct jit_inp_arg a = info->args[i];
		if (a.type == JIT_FLOAT_NUM) fp_arg_cnt += a.size / sizeof(float);
	}


	jit_regpool_prepare(al->gp_regpool, al->gp_regs, al->gp_reg_cnt, al->gp_arg_regs, MIN(info->general_arg_cnt + fp_arg_cnt, al->gp_arg_reg_cnt));

	jit_regpool_prepare(al->fp_regpool, al->fp_regs, al->fp_reg_cnt, NULL, 0);

	int assoc_gp_regs = 0;
	for (int i = 0; i < info->general_arg_cnt + info->float_arg_cnt; i++) {
		if (assoc_gp_regs >= al->gp_arg_reg_cnt) break;
		int isfp_arg = (info->args[i].type == JIT_FLOAT_NUM);

		if (!isfp_arg) {
			rmap_assoc(op->regmap, __mkreg(JIT_RTYPE_INT, JIT_RTYPE_ARG, i), __get_reg(al, al->gp_arg_regs[assoc_gp_regs]));
			assoc_gp_regs++;
		}
		if (isfp_arg) {
			rmap_assoc(op->regmap, __mkreg(JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, i), __get_reg(al, al->gp_arg_regs[assoc_gp_regs]));
			assoc_gp_regs++;
			// initializes the second part of the register
			if ((info->args[i].size == sizeof(double)) && (assoc_gp_regs < al->gp_reg_cnt)) {
				jit_value r = __mkreg_ex(JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, i);
				rmap_assoc(op->regmap, (int)r, __get_reg(al, al->gp_arg_regs[assoc_gp_regs]));
				assoc_gp_regs++;
			}
		}
	}

}
#endif


static void prepare_registers_for_call(struct jit_reg_allocator * al, jit_op * op)
{
	jit_value r, reg;
	jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->ret_reg, 0, &r);
	if (hreg) {
		// checks whether there is a free callee-saved register
		// that can be used to store the eax register
		jit_hw_reg * freereg = get_free_callee_save_reg(al->gp_regpool);
		if (freereg) {
			rename_reg(op, freereg->id, al->ret_reg);

			jit_regpool_put(al->gp_regpool, hreg);

			rmap_unassoc(op->regmap, r, 0);
			rmap_assoc(op->regmap, r, freereg);
		} else {
			sync_reg(op, hreg, r);
		}
	}

	// synchronizes fp-register which can be used to return the value
	if (al->fpret_reg >= 0) {
		hreg = rmap_is_associated(op->regmap, al->fpret_reg, 1, &r);
		if (hreg) sync_reg(op, hreg, r);
	}

	// spills registers which are used to pass the arguments
	// FIXME: duplicities
	int args = MIN(op->arg[0], al->gp_arg_reg_cnt);
	for (int q = 0; q < args; q++) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->gp_arg_regs[q], 0, &reg);
		if (hreg) {
			unload_reg(op, hreg, reg);
			jit_regpool_put(al->gp_regpool, hreg);
			rmap_unassoc(op->regmap, reg, 0);
		}
	}
	args = MIN(op->arg[1], al->fp_arg_reg_cnt);
	for (int q = 0; q < args; q++) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->fp_arg_regs[q], 1, &reg);
		if (hreg) {
			if (hreg->id != al->fpret_reg) {
				unload_reg(op, hreg, reg);
				jit_regpool_put(al->fp_regpool, hreg);
			}
			rmap_unassoc(op->regmap, reg, 1);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
#ifdef LRU_ALLOCATOR
static inline void assign_regs(struct jit * jit, struct jit_op * op)
{
	int i, r;
	struct jit_reg_allocator * al = jit->reg_al;

	// initialize mappings
	// PROLOG needs some special care
	if (GET_OP(op) == JIT_PROLOG) {
		
		struct jit_func_info * info = (struct jit_func_info *) op->arg[1];
		al->current_func_info = info;

		op->regmap = rmap_init();

		assign_regs_for_args(al, op);
	} else {
		// initializes register mappings for standard operations
		if (op->prev) op->regmap = rmap_clone_without_unused_regs(jit, op->prev->regmap, op); 
	}

	if (GET_OP(op) == JIT_PREPARE) prepare_registers_for_call(al, op);

	// PUTARG have to take care of the register allocation by itself
	if ((GET_OP(op) == JIT_PUTARG) || (GET_OP(op) == JIT_FPUTARG)) return;

	if (GET_OP(op) == JIT_RETVAL) {
		jit_hw_reg * hreg = jit_regpool_get_by_id(al->gp_regpool, al->ret_reg);
		if (hreg) {
			rmap_assoc(op->regmap, op->arg[0], hreg);
			return;
		}
	}

	if (GET_OP(op) == JIT_GETARG) {
		// optimization which eliminates undesirable assignments
		int arg_id = op->arg[1];
		struct jit_inp_arg * arg = &(al->current_func_info->args[arg_id]);
		int reg_id = __mkreg(arg->type == JIT_FLOAT_NUM ? JIT_RTYPE_FLOAT : JIT_RTYPE_INT, JIT_RTYPE_ARG, arg_id);
		if (!jitset_get(op->live_out, reg_id)) {
			if (((arg->type != JIT_FLOAT_NUM) && (arg->size == REG_SIZE))
#ifdef JIT_ARCH_AMD64
			|| ((arg->type == JIT_FLOAT_NUM) && (arg->size == sizeof(double)))
#endif
			) {
				jit_hw_reg * hreg = rmap_get(op->regmap, reg_id);
				if (hreg) {
					   rmap_unassoc(op->regmap, reg_id, arg->type == JIT_FLOAT_NUM);
					   rmap_assoc(op->regmap, op->arg[0], hreg);
					   op->r_arg[0] = hreg->id;
					   op->r_arg[1] = op->arg[1];
					   // FIXME: should have its own name
					   op->code = JIT_NOP;
					   return;
				}
			}
		}
	}

#ifdef JIT_ARCH_SPARC
	if (GET_OP(op) == JIT_FRETVAL) {
		jit_hw_reg * hreg = jit_regpool_get_by_id(al->fp_regpool, al->fpret_reg);
		if (hreg) {
			rmap_assoc(op->regmap, op->arg[0], hreg);
			return;
		}
	}
#endif

	if (GET_OP(op) == JIT_CALL) {

		// spills register which are used to return value
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->ret_reg, 0, &r);
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
#ifdef JIT_ARCH_SPARC
		int reg;
		// unloads all FP registers
		for (int q = 0; q < al->fp_reg_cnt; q++) {
			jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->fp_regs[q].id, 1, &reg);
			if (hreg) {
				unload_reg(op, hreg, reg);
				jit_regpool_put(al->fp_regpool, hreg);
				rmap_unassoc(op->regmap, reg, 1);
			}
		}
#endif
#ifdef JIT_ARCH_AMD64
		// since the CALLR may use the given register also to pass an argument,
		// code genarator has to take care of the register value itself
		return;
#endif

	}

	// get registers used in the current operation
	for (i = 0; i < 3; i++)
		if ((ARG_TYPE(op, i + 1) == REG) || (ARG_TYPE(op, i + 1) == TREG)) {
			// in order to eliminate unwanted spilling of registers used
			// by the current operation, we ``touch'' the registers
			// which are to be used in this operation
			jit_hw_reg * hreg = rmap_get(op->regmap, op->arg[i]);
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

			jit_hw_reg * reg = rmap_get(op->regmap, op->arg[i]);
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
#endif
//////////////////////////////////////////////////////////////////

#ifdef LTU_ALLOCATOR

/**
 * If the hw. register which is used to return values is unused (i.e., it's
 * in the register pool) it associates this register with the given virtual register
 * and returns 1.
 */
static int assign_ret_reg(jit_op * op, struct jit_regpool * regpool, int ret_reg)
{	
	jit_hw_reg * hreg = jit_regpool_get_by_id(regpool, ret_reg);
	if (hreg) {
		rmap_assoc(op->regmap, op->arg[0], hreg);
		return 1;
	}
	return 0;
}

static int assign_getarg(jit_op * op, struct jit_reg_allocator * al)
{
	// optimization which eliminates undesirable assignments
	int arg_id = op->arg[1];
	struct jit_inp_arg * arg = &(al->current_func_info->args[arg_id]);
	int reg_id = __mkreg(arg->type == JIT_FLOAT_NUM ? JIT_RTYPE_FLOAT : JIT_RTYPE_INT, JIT_RTYPE_ARG, arg_id);
	if (!jitset_get(op->live_out, reg_id)) {
		if (((arg->type != JIT_FLOAT_NUM) && (arg->size == REG_SIZE))
#ifdef JIT_ARCH_AMD64
				|| ((arg->type == JIT_FLOAT_NUM) && (arg->size == sizeof(double)))
#endif
		   ) {
			jit_hw_reg * hreg = rmap_get(op->regmap, reg_id);
			if (hreg) {
				rmap_unassoc(op->regmap, reg_id, arg->type == JIT_FLOAT_NUM);
				rmap_assoc(op->regmap, op->arg[0], hreg);
				op->r_arg[0] = hreg->id;
				op->r_arg[1] = op->arg[1];
				// FIXME: should have its own name
				op->code = JIT_NOP;
				return 1;
			}
		}
	}
	return 0;
}

/**
 * Spills registers which are used to return value
 */
static void spill_ret_retreg(jit_op * op, struct jit_regpool * regpool, int ret_reg, int fp)
{
	jit_value r;
	if (ret_reg >= 0) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, ret_reg, fp, &r);
		if (hreg) {
			jit_regpool_put(regpool, hreg);
			rmap_unassoc(op->regmap, r, hreg->fp);
		}
	}
}

static int assign_call(jit_op * op, struct jit_reg_allocator * al)
{
	spill_ret_retreg(op, al->gp_regpool, al->ret_reg, 0);
	spill_ret_retreg(op, al->fp_regpool, al->fpret_reg, 1);
#ifdef JIT_ARCH_SPARC
	int reg;
	// unloads all FP registers
	for (int q = 0; q < al->fp_reg_cnt; q++) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->fp_regs[q].id, 1, &reg);
		if (hreg) {
			unload_reg(op, hreg, reg);
			jit_regpool_put(al->fp_regpool, hreg);
			rmap_unassoc(op->regmap, reg, 1);
		}
	}
#endif
#ifdef JIT_ARCH_AMD64
	// since the CALLR may use the given register also to pass an argument,
	// code genarator has to take care of the register value itself
	return 1;
#else
	return 0;
#endif
}

static void associate_register_alias(struct jit_reg_allocator * al, jit_op * op, int i)
{
	if ((int)op->arg[i] == (int)R_OUT) op->r_arg[i] = al->ret_reg;
	else if ((int)op->arg[i] == (int)R_FP) op->r_arg[i] = al->fp_reg;
	else assert(0);
}

static void associate_register(struct jit_reg_allocator * al, jit_op * op, int i)
{
	jit_reg virt_reg = JIT_REG(op->arg[i]);
	jit_hw_reg * reg = rmap_get(op->regmap, op->arg[i]);
	if (reg) op->r_arg[i] = reg->id;
	else {
		if (virt_reg.type == JIT_RTYPE_INT) {
			reg = jit_regpool_get(al->gp_regpool);
		} else {
			reg = jit_regpool_get(al->fp_regpool);
		}
		if (reg == NULL) reg = make_free_reg2(al, op, op->arg[i]);

		rmap_assoc(op->regmap, op->arg[i], reg);

		op->r_arg[i] = reg->id;
		if (jitset_get(op->live_in, op->arg[i]))
			load_reg(op, rmap_get(op->regmap, op->arg[i]), op->arg[i]);
	}
}

static void assign_regs(struct jit * jit, struct jit_op * op)
{
	int i;
	int skip = 0;
	struct jit_reg_allocator * al = jit->reg_al;

	// initialize mappings
	// PROLOG needs some special care
	if (GET_OP(op) == JIT_PROLOG) {
		
		struct jit_func_info * info = (struct jit_func_info *) op->arg[1];
		al->current_func_info = info;

		op->regmap = rmap_init();

		assign_regs_for_args(al, op);
	} else {
		// initializes register mappings for standard operations
		//if (op->prev) op->regmap = rmap_clone_without_unused_regs(jit, op->prev->regmap, op); 
		if (op->prev) op->regmap = rmap_clone(op->prev->regmap); 
	}

	// operations reuqiring some special core
	switch (GET_OP(op)) {
		case JIT_PREPARE: prepare_registers_for_call(al, op); break;
		// PUTARG have to take care of the register allocation by itself
		case JIT_PUTARG: return;
		case JIT_FPUTARG: return;
		case JIT_RETVAL: skip = assign_ret_reg(op, al->gp_regpool, al->ret_reg); break;
#ifdef JIT_ARCH_SPARC
		case JIT_FRETVAL: skip = assign_ret_reg(op, al->fp_regpool, al->fpret_reg); break;
#endif
		case JIT_GETARG: skip = assign_getarg(op, al); break;
		case JIT_CALL: skip = assign_call(op, al); break;
	}

	if (skip) return;

	// associates virtual registers with their hardware counterparts
	for (i = 0; i < 3; i++) {
		if ((ARG_TYPE(op, i + 1) == REG) || (ARG_TYPE(op, i + 1) == TREG)) {
			jit_reg virt_reg = JIT_REG(op->arg[i]);
			if (virt_reg.spec == JIT_RTYPE_ALIAS) associate_register_alias(al, op, i);
			else associate_register(al, op, i);
		} else if (ARG_TYPE(op, i + 1) == IMM) {
			// assigns immediate values
			op->r_arg[i] = op->arg[i];
		} 
	}
}

/**
 * Collects statistics on used registers
 */
static void collect_statistics(struct jit * jit)
{
	int i, j;
	int ops_from_return = 0;
	struct rb_node * last_hints = NULL;

	for (jit_op * op = jit_op_last(jit->ops); op != NULL; op = op->prev) {
		struct rb_node * new_hints = rb_clone(last_hints);
		op->normalized_pos = ops_from_return;
	
	
		// determines used registers	
		// FIXME: should be separate function
		jit_value regs[3];
		int found_regs = 0;
		for (i = 0; i < 3; i++)
			if ((ARG_TYPE(op, i + 1) == REG) || (ARG_TYPE(op, i + 1) == TREG)) {
				int found = 0;
				jit_value reg = op->arg[i];
				for (j = 0; j < found_regs; j++) {
					if (regs[j] == reg) {
						found = 1;
						break;
					}
				}
				if (!found) regs[found_regs++] = reg; 
			}

		// marks register usage 
		for (i = 0; i < found_regs; i++) {
			jit_value reg = regs[i];

			rb_node * hint = rb_search(new_hints, reg);
			struct jit_allocator_hint * new_hint = JIT_MALLOC(sizeof(struct jit_allocator_hint));
			if (hint) memcpy(new_hint, hint->value, sizeof(struct jit_allocator_hint));
			else {
				new_hint->last_pos = 0;
				new_hint->should_be_calleesaved = 0;
			}

			new_hint->last_pos = ops_from_return;
//			printf("XXX:%i:%i\n", (int)reg,  ops_from_return);
			new_hints = rb_insert(new_hints, reg, new_hint, NULL);
		}
		op->allocator_hints = new_hints;

		if (GET_OP(op) == JIT_PROLOG) {
			last_hints = NULL;
			ops_from_return = 0;
		}
		else {
			last_hints = new_hints;
			ops_from_return++;
		}
	}
}
#endif
//////////////////////////////////////////////////////////////////


/**
 * This function adjusts assocaiation of registers to the state
 * which is expected at the destination address of the jump
 * operation
 */
static inline void jump_adjustment(struct jit * jit, jit_op * op)
{
	if (GET_OP(op) != JIT_JMP) return;
	rmap_t * cur_regmap = op->regmap;
	rmap_t * tgt_regmap = op->jmp_addr->regmap;

	rmap_sync(op, cur_regmap, tgt_regmap, RMAP_UNLOAD);
	rmap_sync(op, tgt_regmap, cur_regmap, RMAP_LOAD);
}

/**
 * There must be same register mappings at both ends of the conditional jump.
 * If this condition is not met, contents of registers have to be moved
 * appropriately. This can be achieved with the jump_adjustment function.
 * 
 * This function converts conditional jumps as follows:
 *	beq(succ, ..., ...);
 *	fail
 *	==>
 *	bne(fail:, ..., ...);
 *	register reorganization 
 *	jmp succ
 *	fail:
 */
static inline void branch_adjustment(struct jit * jit, jit_op * op)
{
	if ((GET_OP(op) != JIT_BEQ) && (GET_OP(op) != JIT_BGT) && (GET_OP(op) != JIT_BGE)
	   && (GET_OP(op) != JIT_BNE) && (GET_OP(op) != JIT_BLT) && (GET_OP(op) != JIT_BLE)) return;

	rmap_t * cur_regmap = op->regmap;
	rmap_t * tgt_regmap = op->jmp_addr->regmap;

//	if (!rmap_equal(cur_regmap, tgt_regmap)) {
	if (!rmap_equal2(op, cur_regmap, tgt_regmap)) {
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
			op->jmp_addr->arg[0] = (jit_value) o;
			op->jmp_addr->r_arg[0] = (jit_value) o;
		}

		jit_op_append(op, o);

		jit_op * o2 = __new_op(JIT_PATCH, SPEC(IMM, NO, NO), (jit_value) op, 0, 0, 0);
		o2->r_arg[0] = o2->arg[0];
		jit_op_append(o, o2);

		op->arg[0] = (jit_value) o2;
		op->r_arg[0] = (jit_value) o2;
		op->jmp_addr = o2;
	}
}

void jit_assign_regs(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		op->regmap = rmap_init();

#ifdef LTU_ALLOCATOR
	collect_statistics(jit);
#endif

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
