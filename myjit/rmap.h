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

///////////////////////////////////////////////////////////////////////

static inline rmap_t * rmap_init(int reg_count)
{
	rmap_t * res = JIT_MALLOC(sizeof(struct hw_reg *) * reg_count);
	for (int i = 0; i < reg_count; i++)
		res[i] = NULL;
	return res;
}

/**
 * Checks whether is hardware register associated to some virtual register
 * If so, returns to pointer to this register and sets the id of the virtual
 * register
 */
static inline struct __hw_reg * rmap_is_associated(struct jit * jit, rmap_t * rmap, int reg_id, int * virt_reg)
{
	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++) {
		if (rmap[i]) {
			struct __hw_reg  * hreg = rmap[i];
			if (hreg->id == reg_id) {
				if (virt_reg) *virt_reg = i;
				return hreg;
			}
		}
	}
	return NULL;
}

static inline void rmap_assoc(rmap_t * rmap, int reg, struct __hw_reg * hreg)
{
	rmap[reg] = hreg;
}

static inline void rmap_unassoc(rmap_t * rmap, int reg)
{
	rmap[reg] = NULL;
}

static inline rmap_t * rmap_clone(struct jit * jit, rmap_t * rmap)
{
	rmap_t * o = JIT_MALLOC(sizeof(struct hw_reg *) * jit->reg_count);
	memcpy(o, rmap, sizeof(struct hw_reg *) * jit->reg_count);
	return o;
}

static inline struct __hw_reg * rmap_get(rmap_t * rmap, int reg)
{
	return rmap[reg];
}

static inline int rmap_equal(struct jit * jit, rmap_t * r1, rmap_t * r2)
{
	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++)
		if (r1[i] != r2[i]) return 0;
	return 1;
}

/**
 * Syncronizes two register mappings
 * if (mode == LOAD)i: 	then it loads registers which are not in current mapping, 
 * 			however, are in the target mapping
 * if (mode == UNLOAD): then it unloads registers which are in the current mapping,
 * 			however, which are not in the target mapping
 */
static inline void rmap_sync(struct jit * jit, jit_op * op, rmap_t * current, rmap_t * target, int mode)
{
	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++)
		if ((current[i]) && (current[i] != target[i])) {
			switch (mode) {
				case RMAP_UNLOAD: unload_reg(op, current[i], i); break;
				case RMAP_LOAD: load_reg(op, current[i], i); break;
				default: assert(0);
			}
		}
}

static inline rmap_t * rmap_clone_without_unused_regs(struct jit * jit, rmap_t * prev_rmap, jit_op * op)
{
	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++) {
		if ((prev_rmap[i] != NULL) 
			&& !(jitset_get(op->live_in, i) || jitset_get(op->live_out, i)))
		{
			struct jit_reg_allocator * al = jit->reg_al;
			al->hwreg_pool[++al->hwreg_pool_pos] = prev_rmap[i];
			rmap_unassoc(op->regmap, i);
		} else {
			rmap_assoc(op->regmap, i, prev_rmap[i]);
		}
	}
	return op->regmap;
}

static inline struct __hw_reg * rmap_spill_candidate(struct jit * jit, jit_op * op, int * candidate)
{
	int age = -1;
	struct __hw_reg * res = NULL;
	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++) {
		if (op->regmap[i]) {
			struct __hw_reg * hreg = op->regmap[i];
			if ((int)hreg->used > age) {
				*candidate = i;
				age = hreg->used;
				res = hreg;
			}
		}
	}
	return res;
}

static inline void rmap_make_older(struct jit * jit, rmap_t * regmap)
{
	for (int i = JIT_FIRST_REG; i < jit->reg_count; i++) {
		struct __hw_reg * reg = regmap[i];
		if (reg) reg->used++;
	}
}

static inline void rmap_free(rmap_t * regmap)
{
	JIT_FREE(regmap);
}
