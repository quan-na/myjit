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

#include <string.h>

//
// Auxiliary functions which are used to insert operations
// moving values from/to registers
// 

/**
 * Prepends given LREG/UREG/etc. operation before `op' operation
 */
static void insert_reg_op(int opcode, jit_op * op,  jit_value r1, jit_value r2)
{
	jit_op * o = jit_op_new(opcode, SPEC(IMM, IMM, NO), r1, r2, 0, 0);
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

static jit_hw_reg * make_free_reg(struct jit_reg_allocator * al, jit_op * op, jit_value for_reg)
{
	int spill = 0;
	jit_value spill_candidate = -1;
	jit_hw_reg * hreg = rmap_spill_candidate(al, op, for_reg, &spill, &spill_candidate, 0);

	if (spill) {
		if (jit_set_get(op->live_in, spill_candidate))
			unload_reg(op, hreg, spill_candidate);
		rmap_unassoc(op->regmap, spill_candidate);
	}
	return hreg;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef JIT_ARCH_COMMON86
/**
 * Assigns registers which are used to pass arguments
 */
static void assign_regs_for_args(struct jit_reg_allocator * al, jit_op * op)
{
	struct jit_func_info * info = (struct jit_func_info *) op->arg[1];

	int assoc_gp_regs = 0;
	int assoc_fp_regs = 0;
	for (int i = 0; i < info->general_arg_cnt + info->float_arg_cnt; i++) {
		int isfp_arg = (info->args[i].type == JIT_FLOAT_NUM);
		if (!isfp_arg && (assoc_gp_regs < al->gp_arg_reg_cnt)) {
			rmap_assoc(op->regmap, jit_mkreg(JIT_RTYPE_INT, JIT_RTYPE_ARG, i), al->gp_arg_regs[i]);
			assoc_gp_regs++;
		}
		if (isfp_arg && (assoc_fp_regs < al->fp_arg_reg_cnt)) {
			rmap_assoc(op->regmap, jit_mkreg(JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, i), al->fp_arg_regs[i]);
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

	int assoc_gp_regs = 0;
	for (int i = 0; i < info->general_arg_cnt + info->float_arg_cnt; i++) {
		if (assoc_gp_regs >= al->gp_arg_reg_cnt) break;
		int isfp_arg = (info->args[i].type == JIT_FLOAT_NUM);

		if (!isfp_arg) {
			rmap_assoc(op->regmap, jit_mkreg(JIT_RTYPE_INT, JIT_RTYPE_ARG, i), al->gp_arg_regs[assoc_gp_regs]);
			assoc_gp_regs++;
		}
		if (isfp_arg) {
			rmap_assoc(op->regmap, jit_mkreg(JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, i), al->gp_arg_regs[assoc_gp_regs]);
			assoc_gp_regs++;
			// initializes the second part of the register
			if ((info->args[i].size == sizeof(double)) && (assoc_gp_regs < al->gp_reg_cnt)) {
				jit_value r = jit_mkreg_ex(JIT_RTYPE_FLOAT, JIT_RTYPE_ARG, i);
				rmap_assoc(op->regmap, (int)r, al->gp_arg_regs[assoc_gp_regs]);
				assoc_gp_regs++;
			}
		}
	}

}
#endif

static void prepare_registers_for_call(struct jit_reg_allocator * al, jit_op * op)
{
	jit_value r, reg;
	jit_hw_reg * hreg = NULL;
#ifdef JIT_ARCH_COMMON86
	if (al->ret_reg) hreg = rmap_is_associated(op->regmap, al->ret_reg->id, 0, &r);
	if (hreg) {
		// checks whether there is a free callee-saved register
		// that can be used to take over content from eax register
		
		int alive = (jit_set_get(op->live_out, r) || (jit_set_get(op->live_in, r)));
		if (!alive) goto skip;
		int spill;
		jit_value spill_reg;
		jit_hw_reg * freereg = rmap_spill_candidate(al, op, r, &spill, &spill_reg, 1);
		if (freereg && !spill) {
			// FIXME: works only for GP registers
			rename_reg(op, freereg->id, al->ret_reg->id);

			rmap_unassoc(op->regmap, r);
			rmap_assoc(op->regmap, r, freereg);
		} else {
			sync_reg(op, hreg, r);
		}
	}
skip:

#endif
	// synchronizes fp-register which can be used to return the value
	if (al->fpret_reg) {
		hreg = rmap_is_associated(op->regmap, al->fpret_reg->id, 1, &r);
		if (hreg) sync_reg(op, hreg, r);
	}

	// spills registers which are used to pass the arguments
	// FIXME: duplicities
	int args = MIN(op->arg[0], al->gp_arg_reg_cnt);
	for (int q = 0; q < args; q++) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->gp_arg_regs[q]->id, 0, &reg);
		if (hreg) {
			if (jit_set_get(op->live_out, reg)) unload_reg(op, hreg, reg);
			rmap_unassoc(op->regmap, reg);
		}
	}
	args = MIN(op->arg[1], al->fp_arg_reg_cnt);
	for (int q = 0; q < args; q++) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->fp_arg_regs[q]->id, 1, &reg);
		if (hreg) {
			if ((hreg->id != al->fpret_reg->id) && jit_set_get(op->live_out, reg)) unload_reg(op, hreg, reg);
			rmap_unassoc(op->regmap, reg);
		}
	}
}

/**
 * If the hw. register which is used to return values is unused (i.e., it's
 * in the register pool) it associates this register with the given virtual register
 * and returns 1.
 * Tacitly assumes, that return register has been unassociated before function call.
 */
static int assign_ret_reg(jit_op * op, jit_hw_reg * ret_reg)
{
	rmap_assoc(op->regmap, op->arg[0], ret_reg);
	return 1;
}

static int assign_getarg(jit_op * op, struct jit_reg_allocator * al)
{
	// optimization which eliminates undesirable assignments
	int arg_id = op->arg[1];
	struct jit_inp_arg * arg = &(al->current_func_info->args[arg_id]);
	int reg_id = jit_mkreg(arg->type == JIT_FLOAT_NUM ? JIT_RTYPE_FLOAT : JIT_RTYPE_INT, JIT_RTYPE_ARG, arg_id);
	if (!jit_set_get(op->live_out, reg_id)) {
		if (((arg->type != JIT_FLOAT_NUM) && (arg->size == REG_SIZE))
#ifdef JIT_ARCH_AMD64
				|| ((arg->type == JIT_FLOAT_NUM) && (arg->size == sizeof(double)))
#endif
		   ) {
			jit_hw_reg * hreg = rmap_get(op->regmap, reg_id);
			if (hreg) {
				rmap_unassoc(op->regmap, reg_id);
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

static void spill_ret_retreg(jit_op * op, jit_hw_reg * ret_reg)
{
	jit_value r;
	if (ret_reg) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, ret_reg->id, ret_reg->fp, &r);
		if (hreg) rmap_unassoc(op->regmap, r);
	}
}

/**
 * Spills registers that are in use before JMPR is performed
 */
static int assign_jmp(jit_op * op, struct jit_reg_allocator * al)
{
	if (op->code == (JIT_JMP | IMM)) return 0; 
	jit_value reg;
	for (int i = 0; i < al->gp_reg_cnt; i++) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->gp_regs[i].id, 0, &reg);
		if (hreg && jit_set_get(op->live_out, reg)) sync_reg(op, hreg, reg);
	}

	for (int i = 0; i < al->fp_reg_cnt; i++) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->fp_regs[i].id, 1, &reg);
		if (hreg && jit_set_get(op->live_out, reg)) sync_reg(op, hreg, reg);
	}
	return 0;
}

static int assign_call(jit_op * op, struct jit_reg_allocator * al)
{
	spill_ret_retreg(op, al->ret_reg);
	spill_ret_retreg(op, al->fpret_reg);
#ifdef JIT_ARCH_SPARC
	jit_value reg;
	// unloads all FP registers
	for (int q = 0; q < al->fp_reg_cnt; q++) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->fp_regs[q].id, 1, &reg);
		if (hreg) {
			unload_reg(op, hreg, reg);
			rmap_unassoc(op->regmap, reg);
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

static int spill_all_registers(jit_op *op, struct jit_reg_allocator * al)
{
	jit_value reg;
	for (int i = 0; i < al->gp_reg_cnt; i++) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->gp_regs[i].id, 0, &reg);
		if (hreg && (jit_set_get(op->live_out, reg))) {
			if (op->in_use) unload_reg(op, hreg, reg);
			rmap_unassoc(op->regmap, reg);
		}
	}

	for (int i = 0; i < al->fp_reg_cnt; i++) {
		jit_hw_reg * hreg = rmap_is_associated(op->regmap, al->fp_regs[i].id, 1, &reg);
		if (hreg && (jit_set_get(op->live_out, reg))) {
			if (op->in_use) unload_reg(op, hreg, reg);
			rmap_unassoc(op->regmap, reg);
		}
	}

	return 1;
}

static int force_spill(jit_op *op)
{
	jit_value reg = op->arg[0];
	jit_hw_reg *hreg = rmap_get(op->regmap, reg);
	if (hreg) {
		unload_reg(op, hreg, reg);
		rmap_unassoc(op->regmap, reg);
	}
	return 1;
}

static int force_assoc(jit_op *op, struct jit_reg_allocator *al)
{
	jit_hw_reg *hreg = (op->arg[1] == 0 ? &al->gp_regs[op->arg[2]] : &al->fp_regs[op->arg[2]]); 
	rmap_assoc(op->regmap, op->arg[0], hreg); 

	load_reg(op, hreg, op->arg[0]);
	return 1;
}

static void associate_register_alias(struct jit_reg_allocator * al, jit_op * op, int i)
{
	if ((int)op->arg[i] == (int)R_OUT) op->r_arg[i] = al->ret_reg->id;
	else if ((int)op->arg[i] == (int)R_FP) op->r_arg[i] = al->fp_reg;
	else assert(0);
}

static void associate_register(struct jit_reg_allocator * al, jit_op * op, int i)
{
	jit_hw_reg * reg = rmap_get(op->regmap, op->arg[i]);
	if (reg) op->r_arg[i] = reg->id;
	else {
		reg = make_free_reg(al, op, op->arg[i]);
		rmap_assoc(op->regmap, op->arg[i], reg);

		op->r_arg[i] = reg->id;
		if (jit_set_get(op->live_in, op->arg[i]))
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

		//op->regmap = rmap_init();

		assign_regs_for_args(al, op);
	} else {
		// initializes register mappings for standard operations
		if (op->prev) {
			rmap_free(op->regmap);
			op->regmap = rmap_clone(op->prev->regmap); 
		}
	}

	// operations reuqiring some special core
	switch (GET_OP(op)) {
		case JIT_PREPARE: prepare_registers_for_call(al, op); break;
		// PUTARG have to take care of the register allocation by itself
		case JIT_PUTARG: skip = 1; break;
		case JIT_FPUTARG: skip = 1; break;

#ifdef JIT_ARCH_COMMON86
		case JIT_RETVAL: skip = assign_ret_reg(op, al->ret_reg); break;
#endif
#ifdef JIT_ARCH_SPARC
		case JIT_FRETVAL: skip = assign_ret_reg(op, al->fpret_reg); break;
#endif
		case JIT_GETARG: skip = assign_getarg(op, al); break;
		case JIT_CALL: skip = assign_call(op, al); break;
		case JIT_JMP: skip = assign_jmp(op, al); break;
		case JIT_FULL_SPILL: skip = spill_all_registers(op, al); break;
		case JIT_FORCE_SPILL: skip = force_spill(op); break;
		case JIT_FORCE_ASSOC: skip = force_assoc(op, al); break;
		default: break;
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

static void mark_calleesaved_regs(jit_tree * hint, jit_op * op)
{
	if (hint == NULL) return;
	struct jit_allocator_hint * h = (struct jit_allocator_hint *) hint->value;
	jit_value reg = (jit_value) hint->key;
	if (jit_set_get(op->live_out, reg)) h->should_be_calleesaved++;

	mark_calleesaved_regs(hint->left, op);
	mark_calleesaved_regs(hint->right, op);
}

/**
 * Collects statistics on used registers
 */
static void hints_refcount_inc(jit_tree * hints);
void jit_collect_statistics(struct jit * jit)
{
	int i, j;
	int ops_from_return = 0;
	jit_tree * last_hints = NULL;

	for (jit_op * op = jit_op_last(jit->ops); op != NULL; op = op->prev) {
		jit_tree * new_hints = jit_tree_clone(last_hints);
		op->normalized_pos = ops_from_return;
	
		// determines used registers	
		// FIXME: should be a separate function
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

			jit_tree * hint = jit_tree_search(new_hints, reg);
			struct jit_allocator_hint * new_hint = JIT_MALLOC(sizeof(struct jit_allocator_hint));
			if (hint) memcpy(new_hint, hint->value, sizeof(struct jit_allocator_hint));
			else {
				new_hint->last_pos = 0;
				new_hint->should_be_calleesaved = 0;
				new_hint->should_be_eax = 0;
			}
			new_hint->refs = 0;

			new_hint->last_pos = ops_from_return;
#ifdef JIT_ARCH_COMMON86
			if ((GET_OP(op) == JIT_RETVAL) || (GET_OP(op) == JIT_RET)) 
				new_hint->should_be_eax++;
#endif 
			new_hints = jit_tree_insert(new_hints, reg, new_hint, NULL);
		}
#ifdef JIT_ARCH_COMMON86
		if (GET_OP(op) == JIT_CALL) mark_calleesaved_regs(new_hints, op);
#endif
		hints_refcount_inc(new_hints);
		op->allocator_hints = new_hints;

		if (GET_OP(op) == JIT_PROLOG) {
			last_hints = NULL;
			ops_from_return = 0;
		} else {
			last_hints = new_hints;
			ops_from_return++;
		}
	}
}

static void hints_refcount_inc(jit_tree * hints)
{
	if (hints == NULL) return;
	((struct jit_allocator_hint*) hints->value)->refs++;
	hints_refcount_inc(hints->left);
	hints_refcount_inc(hints->right);
}

void jit_allocator_hints_free(jit_tree * hints)
{
	if (hints == NULL) return;
	jit_allocator_hints_free(hints->left);
	jit_allocator_hints_free(hints->right);

	int refs = --((struct jit_allocator_hint*) hints->value)->refs;
	if (refs == 0) JIT_FREE(hints->value);

	JIT_FREE(hints);
}
//////////////////////////////////////////////////////////////////

/**
 * This function adjusts association of registers to the state
 * which is expected at the destination address of the jump
 * operation
 */
static inline void jump_adjustment(struct jit * jit, jit_op * op)
{
	if (op->code == (JIT_JMP | IMM)) {
		jit_rmap * cur_regmap = op->regmap;
		jit_rmap * tgt_regmap = op->jmp_addr->regmap;

		rmap_sync(op, cur_regmap, tgt_regmap, RMAP_UNLOAD);
		rmap_sync(op, tgt_regmap, cur_regmap, RMAP_LOAD);
	}
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
	if (!is_cond_branch_op(op)) return;
	jit_rmap * cur_regmap = op->regmap;
	jit_rmap * tgt_regmap = op->jmp_addr->regmap;

	if (!rmap_equal(op, cur_regmap, tgt_regmap)) {
		switch (GET_OP(op)) {
			case JIT_BEQ: op->code = JIT_BNE | (op->code & 0x7); break;
			case JIT_BGT: op->code = JIT_BLE | (op->code & 0x7); break;
			case JIT_BGE: op->code = JIT_BLT | (op->code & 0x7); break;
			case JIT_BNE: op->code = JIT_BEQ | (op->code & 0x7); break;
			case JIT_BLT: op->code = JIT_BGE | (op->code & 0x7); break;
			case JIT_BLE: op->code = JIT_BGT | (op->code & 0x7); break;

			case JIT_BOADD: op->code = JIT_BNOADD | (op->code & 0x7); break;
			case JIT_BOSUB: op->code = JIT_BNOSUB | (op->code & 0x7); break;
			case JIT_BNOADD: op->code = JIT_BOADD | (op->code & 0x7); break;
			case JIT_BNOSUB: op->code = JIT_BOSUB | (op->code & 0x7); break;

			case JIT_FBEQ: op->code = JIT_FBNE | (op->code & 0x7); break;
			case JIT_FBGT: op->code = JIT_FBLE | (op->code & 0x7); break;
			case JIT_FBGE: op->code = JIT_FBLT | (op->code & 0x7); break;
			case JIT_FBNE: op->code = JIT_FBEQ | (op->code & 0x7); break;
			case JIT_FBLT: op->code = JIT_FBGE | (op->code & 0x7); break;
			case JIT_FBLE: op->code = JIT_FBGT | (op->code & 0x7); break;
			default: break;
		}
	
		jit_op * o = jit_op_new(JIT_JMP | IMM, SPEC(IMM, NO, NO), op->arg[0], 0, 0, 0);		
		o->r_arg[0] = op->r_arg[0];

		o->regmap = rmap_clone(op->regmap);
	
		o->live_in = jit_set_clone(op->live_in); 
		o->live_out = jit_set_clone(op->live_out); 
		o->jmp_addr = op->jmp_addr;

		if (!jit_is_label(jit, (void *)op->r_arg[0])) {
			op->jmp_addr->arg[0] = (jit_value) o;
			op->jmp_addr->r_arg[0] = (jit_value) o;
		}

		jit_op_append(op, o);

		jit_op * o2 = jit_op_new(JIT_PATCH, SPEC(IMM, NO, NO), (jit_value) op, 0, 0, 0);
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

	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		assign_regs(jit, op);

	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		branch_adjustment(jit, op);

	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		jump_adjustment(jit, op);
}

void jit_reg_allocator_free(struct jit_reg_allocator * a)
{
	if (a->fp_regs) JIT_FREE(a->fp_regs);
	JIT_FREE(a->gp_regs);
	if (a->fp_arg_regs) JIT_FREE(a->fp_arg_regs);
	if (a->gp_arg_regs) JIT_FREE(a->gp_arg_regs);
	JIT_FREE(a);
}

/**
 * returns 1 if the given hw. register (e.g., X86_EAX) is in use
 */
int jit_reg_in_use(jit_op * op, int reg, int fp)
{
	jit_value virt_reg;
	if (rmap_is_associated(op->regmap, reg, fp, &virt_reg)
	&& ((jit_set_get(op->live_in, virt_reg) || (jit_set_get(op->live_out, virt_reg))))) return 1;
	else return 0;
}

/**
 * returns a register which is unused, otherwise returns NULL
 */
jit_hw_reg * jit_get_unused_reg(struct jit_reg_allocator * al, jit_op * op, int fp)
{
	jit_hw_reg * regs;
	int reg_count;

	if (!fp) {
		regs = al->gp_regs;
		reg_count = al->gp_reg_cnt;
	} else {
		regs = al->fp_regs;
		reg_count = al->fp_reg_cnt;
	}
	for (int i = 0; i < reg_count; i++)
		if (!jit_reg_in_use(op, regs[i].id, fp)) return &(regs[i]);
	return NULL;
}
