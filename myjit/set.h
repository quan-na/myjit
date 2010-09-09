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

#ifndef SET_H
#define SET_H

#include <assert.h>

#include "jitlib-core.h"

static inline jitset * jitset_new()
{
	jitset * s = JIT_MALLOC(sizeof(jitset));
	s->root = NULL;
	return s;
}

static inline jitset * jitset_clone(jitset * s)
{
	jitset * clone = jitset_new();
	clone->root = rb_clone(s->root);
	return clone;
}

static inline void jitset_free(jitset * s)
{
	rb_free(s->root);
	JIT_FREE(s);
}

static inline void jitset_or(jitset * target, jitset * s)
{
	target->root = rb_addall(target->root, s->root);
}

static inline int jitset_get(jitset * s, int bit)
{
	return (rb_search(s->root, bit) != NULL);
}

static inline void jitset_set(jitset * s, int bit, int value)
{
	if (value) s->root = rb_insert(s->root, bit, (void *)1, NULL);
	else s->root = rb_delete(s->root, bit, NULL);
}

static inline int jitset_equal(jitset * s1, jitset * s2) 
{
	return rb_equal(s1->root, s2->root);
}
#endif
