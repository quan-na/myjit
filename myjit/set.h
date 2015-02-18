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

#ifndef SET_H
#define SET_H

#include <assert.h>

#include "jitlib-core.h"

static inline jit_set * jit_set_new()
{
	jit_set * s = JIT_MALLOC(sizeof(jit_set));
	s->root = NULL;
	return s;
}

static inline jit_set * jit_set_clone(jit_set * s)
{
	jit_set * clone = jit_set_new();
	clone->root = jit_tree_clone(s->root);
	return clone;
}

static inline void jit_set_free(jit_set * s)
{
	jit_tree_free(s->root);
	JIT_FREE(s);
}

static inline void jit_set_addall(jit_set * target, jit_set * s)
{
	target->root = jit_tree_addall(target->root, s->root);
}

static inline int jit_set_get(jit_set * s, int value)
{
	return (jit_tree_search(s->root, value) != NULL);
}

static inline void jit_set_add(jit_set * s, int value)
{
	s->root = jit_tree_insert(s->root, value, (void *)1, NULL);
}

static inline void jit_set_remove(jit_set * s, int value)
{
	s->root = jit_tree_delete(s->root, value, NULL);
}

static inline int jit_set_equal(jit_set * s1, jit_set * s2) 
{
	return jit_tree_equal(s1->root, s2->root);
}
#endif
