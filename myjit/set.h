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

/*
typedef struct jitset {
	unsigned int size; // universe size
	int data;
} jitset;

*/
#include "jitlib-core.h"

#include <assert.h>

static inline jitset * jitset_new(unsigned int size)
{
	assert(size < 1000);
	jitset * s = JIT_MALLOC(sizeof(jitset));
	s->size = size;
	s->data = 0;
	return s;
}

static inline jitset * jitset_clone(jitset * s)
{
	jitset * clone = jitset_new(s->size);
//	clone->size = s->size;
	clone->data = s->data;
	return clone;
}

static inline void jitset_free(jitset * s)
{
	JIT_FREE(s);
}

static inline void jitset_or(jitset * target, jitset * s)
{
	target->data |= s->data;
}

static inline void jitset_and(jitset * target, jitset * s)
{
	target->data &= s->data;
}

static inline void jitset_diff(jitset * target, jitset * s)
{
	target->data = target->data & ~s->data;
}

static inline int jitset_get(jitset * s, unsigned int bit)
{
	return s->data & (1 << bit);
}

static inline void jitset_set(jitset * s, unsigned int bit, int value)
{
	if (value) s->data |= (1 << bit);
	else s->data &= ~(1 << bit);
}

static inline int jitset_equal(jitset * s1, jitset * s2) 
{
	return (s1->data == s2->data);
}
