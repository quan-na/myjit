/*
 * MyJIT 
 * Copyright (C) 2015 Petr Krajca, <petr.krajca@upol.cz>
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
#include <limits.h>
/*
 * Register mappings
 */

#define RMAP_UNLOAD (1)
#define RMAP_LOAD (2)

static inline void unload_reg(jit_op * op,  jit_hw_reg * hreg, long virt_reg);
static inline void load_reg(struct jit_op * op, jit_hw_reg * hreg, long reg);

///////////////////////////////////////////////////////////////////////

static inline jit_rmap * rmap_init()
{
	jit_rmap * res = JIT_MALLOC(sizeof(jit_rmap));
	res->map = NULL;
	return res;
}

jit_hw_reg * rmap_get(jit_rmap * rmap, jit_value reg)
{
	jit_tree * found = jit_tree_search(rmap->map, reg);
	if (found) return (jit_hw_reg *) found->value;

	return NULL;
}

static inline jit_hw_reg * rmap_is_associated_aux(jit_tree * n, int reg_id, int fp, jit_value * virt_reg)
{
	if (n == NULL) return NULL;
	jit_hw_reg * r = (jit_hw_reg *) n->value;

	if ((r->fp == fp) && (r->id == reg_id)) {
		if (virt_reg) *virt_reg = (jit_value) n->key;
		return r;
	}

	r = rmap_is_associated_aux(n->left, reg_id, fp, virt_reg);
	if (r) return r;
	else return rmap_is_associated_aux(n->right, reg_id, fp, virt_reg);
}

/**
 * Checks whether the hardware register reg_is is associated with some virtual register
 * If so, returns a pointer to this register and sets the id of the virtual
 * register
 */
static jit_hw_reg * rmap_is_associated(jit_rmap * rmap, int reg_id, int fp, jit_value * virt_reg)
{
	return rmap_is_associated_aux(rmap->map, reg_id, fp, virt_reg);
}

static void rmap_assoc(jit_rmap * rmap, jit_value reg, jit_hw_reg * hreg)
{
	rmap->map = jit_tree_insert(rmap->map, reg, hreg, NULL);
}

static void rmap_unassoc(jit_rmap * rmap, jit_value reg)
{
	rmap->map = jit_tree_delete(rmap->map, reg, NULL);
}

static jit_rmap * rmap_clone(jit_rmap * rmap)
{
	jit_rmap * res = JIT_MALLOC(sizeof(jit_rmap));
	res->map = jit_tree_clone(rmap->map);
	return res;
}

static int rmap_subset(jit_op * op, jit_tree * current, jit_tree * target)
{
	if (current == NULL) return 1;

	// ignores mappings of register which are not live
	jit_set * tgt_livein = op->jmp_addr->live_in;
	if (!jit_set_get(tgt_livein, current->key) && !jit_set_get(op->live_out, current->key)) goto skip;

	jit_tree * found = jit_tree_search(target, current->key);
	if ((!found) || (current->value != found->value)) return 0;

skip:
	return rmap_subset(op, current->left, target) && rmap_subset(op, current->right, target);
}

/**
 * Compares whether register mappings in the target destination
 * are the same as the current
 */
static int rmap_equal(jit_op * op, jit_rmap * current, jit_rmap * target)
{
	return rmap_subset(op, current->map, target->map) && rmap_subset(op, target->map, current->map);
}

/**
 * Synchronizes two register mappings
 * if (mode == LOAD): 	then it loads registers which are not present in the current mapping, 
 * 			however, are in the target mapping
 * if (mode == UNLOAD): then it unloads registers which are in the current mapping,
 * 			however, which are not in the target mapping
 */
static void rmap_sync_aux(jit_tree * current, jit_tree * target, jit_op * op, int mode)
{
	if (current == NULL) return;

	// if the given register does not have relevant content, then ignore it
	if ((mode == RMAP_LOAD) && (!jit_set_get(op->live_out, current->key))) goto skip;

	// if the register is not used in the destination, then ignore it
	if ((mode == RMAP_UNLOAD) && (!jit_set_get(op->jmp_addr->live_in, current->key))) goto skip;

	jit_tree * found = jit_tree_search(target, current->key);
	int i = current->key;

	if ((!found) || (current->value != found->value)) {
		jit_hw_reg * hreg = (jit_hw_reg *) current->value;
		switch (mode) {
			case RMAP_UNLOAD: unload_reg(op, hreg, i); break;
			case RMAP_LOAD: load_reg(op, hreg, i); break;
			default: assert(0);
		}
	}
skip:
	rmap_sync_aux(current->left, target, op, mode);
	rmap_sync_aux(current->right, target, op, mode);
}

static void rmap_sync(jit_op * op, jit_rmap * current, jit_rmap * target, int mode)
{
	rmap_sync_aux(current->map, target->map, op, mode);
}

static int candidate_score(jit_op * op, jit_value virtreg, jit_hw_reg * hreg, int * spill, jit_value * associated_virtreg)
{
	int score = 0;
	score -= hreg->priority;
	
	jit_value x;
	int hw_associated = (rmap_is_associated(op->regmap, hreg->id, hreg->fp, &x) != NULL);

	int alive = 0;
	if (hw_associated) {
		alive = (jit_set_get(op->live_in, x) || jit_set_get(op->live_out, x));
	}
	if (!alive) score += 10000;		// prefers registers which are not live

	*spill = 0;
	if (hw_associated) {

		score -= 100000;	// suppress registers which are in use
		*spill = 1;

		*associated_virtreg = x;

		jit_tree * hint_node = jit_tree_search(op->allocator_hints, x);
		int is_to_be_used = (hint_node != NULL);

		if (!is_to_be_used) score += 50000; // if it's not to be used it is not so bad candidate
		else {
			struct jit_allocator_hint * hint = (struct jit_allocator_hint *)hint_node->value;
			int used_in_steps = -(hint->last_pos - op->normalized_pos);
			if (hw_associated && (used_in_steps == 0)) return INT_MIN; // register is used in the current function (it's not a good candidate for spilling)
			else score += (used_in_steps * 5);
		}
	}


#ifdef JIT_ARCH_COMMON86
	jit_tree * hint_node = jit_tree_search(op->allocator_hints, virtreg);
	if (hint_node) {
		struct jit_allocator_hint * hint = (struct jit_allocator_hint *)hint_node->value;
		if ((hreg->fp == 0) && (hreg->id == COMMON86_AX)) {
			score += hint->should_be_eax * 5;
		}
		if (hreg->callee_saved) score += (hint->should_be_calleesaved - 1) * 15;
	}
#endif
	return score;
}

/**
 * Returns suitable register which can be associated to registers virtreg in the operation op
 * Paramater spill indicates whether the content of the register has to be spilled out and if so,
 * it returns currently associated value throught the reg_to_spill argument.
 *
 * Parameter callee_saved indicates whether, the argument have to be callee-saved or not.
 */
static jit_hw_reg * rmap_spill_candidate(struct jit_reg_allocator * al, jit_op * op, jit_value virtreg, int * spill, jit_value * reg_to_spill, int callee_saved)
{
	jit_reg r = JIT_REG(virtreg);
	jit_hw_reg * regs;
	int reg_count;
	jit_hw_reg * result = NULL;
	int best_score = INT_MIN;

	if (r.type == JIT_RTYPE_INT) {
		regs = al->gp_regs;
		reg_count = al->gp_reg_cnt;
	} else {
		regs = al->fp_regs;
		reg_count = al->fp_reg_cnt;
	}

	int sp = 0;
	for (int i = 0; i < reg_count; i++) {
		if (callee_saved && !regs[i].callee_saved) continue;
		jit_value assoc = 0;
		int score = candidate_score(op, virtreg, &(regs[i]), &sp, &assoc);
		if (score > best_score) {
			if (sp) {
				*reg_to_spill = assoc; 
				*spill = sp;
			} else {
				*reg_to_spill = -1;
				*spill = 0;
			}
			result = &(regs[i]);
			best_score = score;
		}
	}
	return result;
}

void rmap_free(jit_rmap * regmap)
{
	if (!regmap) return;
	jit_tree_free(regmap->map);
	JIT_FREE(regmap);
}
