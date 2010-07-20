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
 * Register mappings
 */

#define RMAP_UNLOAD (1)
#define RMAP_LOAD (2)

static inline void unload_reg(jit_op * op,  struct __hw_reg * hreg, long virt_reg);
static inline void load_reg(struct jit_op * op, struct __hw_reg * hreg, long reg);
static inline void jit_regpool_put(struct jit_regpool * rp, struct __hw_reg * hreg);

///////////////////////////////////////////////////////////////////////

static inline rmap_t * rmap_init()
{
	rmap_t * res = JIT_MALLOC(sizeof(rmap_t));
	res->map = NULL;
	res->revmap = NULL;
	return res;
}

static inline struct __hw_reg * rmap_get(rmap_t * rmap, int reg)
{
	rb_node * found = rb_search(rmap->map, reg);
	if (found) return (struct __hw_reg *) found->value;

	return NULL;
}

/**
 * Checks whether is hardware register associated to some virtual register
 * If so, returns to pointer to this register and sets the id of the virtual
 * register
 */
static inline struct __hw_reg * rmap_is_associated(rmap_t * rmap, int reg_id, int fp, int * virt_reg)
{
	if (fp) reg_id = -reg_id - 1;
	rb_node * found = rb_search(rmap->revmap, reg_id);
	if (found) {
		int r = (long)found->value;
		if (virt_reg) *virt_reg = r;
		return rmap_get(rmap, r);
	}
	return NULL;
}

static inline void rmap_assoc(rmap_t * rmap, int reg, struct __hw_reg * hreg)
{
	rmap->map = rb_insert(rmap->map, reg, hreg, NULL);
	if (!hreg->fp) rmap->revmap = rb_insert(rmap->revmap, hreg->id, (void *)(long)reg, NULL);
	else rmap->revmap = rb_insert(rmap->revmap, - hreg->id - 1, (void *)(long)reg, NULL);
}

static inline void rmap_unassoc(rmap_t * rmap, int reg, int fp)
{
	struct __hw_reg * hreg = (struct __hw_reg *)rb_search(rmap->map, reg);
	rmap->map = rb_delete(rmap->map, reg, NULL);
	if (!fp) rmap->revmap = rb_delete(rmap->revmap, hreg->id, NULL);
	rmap->revmap = rb_delete(rmap->revmap, - hreg->id - 1, NULL);
}

static inline rmap_t * rmap_clone(rmap_t * rmap)
{
	rmap_t * res = JIT_MALLOC(sizeof(rmap_t));
	res->map = rb_clone(rmap->map);
	res->revmap = rb_clone(rmap->revmap);
	return res;
}

static inline int rmap_equal(rmap_t * r1, rmap_t * r2)
{
	return rb_equal(r1->map, r2->map);
}

/**
 * Syncronizes two register mappings
 * if (mode == LOAD): 	then it loads registers which are not in current mapping, 
 * 			however, are in the target mapping
 * if (mode == UNLOAD): then it unloads registers which are in the current mapping,
 * 			however, which are not in the target mapping
 */

static void __sync(rb_node * current, rb_node * target, jit_op * op, int mode)
{
	if (current == NULL) return;
	rb_node * found = rb_search(target, current->key);
	int i = current->key;
	if ((!found) || (current->value != found->value)) {
		struct __hw_reg * hreg = (struct __hw_reg *) current->value;
		switch (mode) {
			case RMAP_UNLOAD: unload_reg(op, hreg, i); break;
			case RMAP_LOAD: load_reg(op, hreg, i); break;
			default: assert(0);
		}
	}
	__sync(current->left, target, op, mode);
	__sync(current->right, target, op, mode);
}

static inline void rmap_sync(jit_op * op, rmap_t * current, rmap_t * target, int mode)
{
	__sync(current->map, target->map, op, mode);
}

static void __clone_wo_regs(struct jit_reg_allocator * al, rmap_t * target, rb_node * n, jit_op * op)
{
	if (n == NULL) return;

	int i = n->key;
	struct __hw_reg * hreg = (struct __hw_reg *) n->value;
	if (!hreg) assert(0);
	if (!(jitset_get(op->live_in, i) || jitset_get(op->live_out, i))) {
		//rmap_unassoc(target, i);
		if (!hreg->fp) jit_regpool_put(al->gp_regpool, hreg);
		else jit_regpool_put(al->fp_regpool, hreg);
	} else {
		rmap_assoc(target, i, hreg);
	}
	__clone_wo_regs(al, target, n->left, op);
	__clone_wo_regs(al, target, n->right, op);

}

static inline rmap_t * rmap_clone_without_unused_regs(struct jit * jit, rmap_t * prev_rmap, jit_op * op)
{
	rmap_t * res = rmap_init();
	__clone_wo_regs(jit->reg_al, res, prev_rmap->map, op);
	op->regmap = res;

	return op->regmap;
}

static void __spill_candidate(rb_node * n, int fp, int * age, struct __hw_reg ** cand_hreg, int * candidate)
{
	if (n == NULL) return;

	struct __hw_reg * hreg = (struct __hw_reg *) n->value; 
	if (hreg->fp == fp) {
		if ((int)hreg->used > *age) {
			*candidate = n->key;
			*age = hreg->used;
			*cand_hreg = hreg;
		}
	}
	__spill_candidate(n->left, fp, age, cand_hreg, candidate);
	__spill_candidate(n->right, fp, age, cand_hreg, candidate);
}

static inline struct __hw_reg * rmap_spill_candidate(jit_op * op, int fp, int * candidate)
{
	int age = -1;
	struct __hw_reg * res;
	__spill_candidate(op->regmap->map, fp, &age, &res, candidate);
	return res;
}

static void __make_older(rb_node * n)
{
	if (n == NULL) return;
	((struct __hw_reg *)n->value)->used++;
	__make_older(n->left);
	__make_older(n->right);
}

static inline void rmap_make_older(rmap_t * regmap)
{
	__make_older(regmap->map);
}

static inline void rmap_free(rmap_t * regmap)
{
	rb_free(regmap->map);
	rb_free(regmap->revmap);
	JIT_FREE(regmap);
}
