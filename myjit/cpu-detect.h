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
 * Detects CPU and setups platform specific constants and macros used across the library
 */

#ifndef CPU_DETECT_H
#define CPU_DETECT_H

// pretty lousy processor detection
#ifdef __i386__
	#define JIT_ARCH_I386
	#define JIT_ARCH_COMMON86
#else
	#ifdef __sparc__
		#define JIT_ARCH_SPARC
	#else
		#define JIT_ARCH_AMD64
		#define JIT_ARCH_COMMON86
	#endif
#endif

// enable this to test register allocation
#define JIT_REGISTER_TEST
#undef JIT_REGISTER_TEST

/*
 * i386 related macros
 */
#ifdef JIT_ARCH_I386

// maximum size of value (in bits) that can be used as an immediate value without the ``sign bit''
// (i386 does not need to transform large immediates values)
#define JIT_IMM_BITS	(-1)

// stack alignment (in bytes)
#define JIT_STACK_ALIGNMENT	(4)

#endif

/*
 * AMD64 related macros
 */
#ifdef JIT_ARCH_AMD64

// maximum size of value (in bits) that can be used as an immediate value without the ``sign bit''
// (i386 does not need to transform large immediates values)
#define JIT_IMM_BITS	(31)

// stack alignment (in bytes)
#define JIT_STACK_ALIGNMENT	(16)

#endif

/*
 * SPARC related macros
 */
#ifdef JIT_ARCH_SPARC
// maximum size of value (in bits) that can be used as an immediate value without the ``sign bit''
// (i386 does not need to transform large immediates values)
#define JIT_IMM_BITS	(12)
#endif



/*
 * Common macros
 */

#define R(x) (__mkreg(JIT_RTYPE_INT, JIT_RTYPE_REG, (x)))
#define FR(x) (__mkreg(JIT_RTYPE_FLOAT, JIT_RTYPE_REG, (x)))

#ifndef JIT_MALLOC
#define JIT_MALLOC(x)	(malloc(x))
#endif

#ifndef JIT_REALLOC
#define JIT_REALLOC(x, size)	(realloc((x), (size)))
#endif

#ifndef JIT_FREE
#define JIT_FREE(x)	(free(x))
#endif

#define INT_SIZE (sizeof(int))
#define PTR_SIZE (sizeof(void *))
#define REG_SIZE (sizeof(void *))

typedef long jit_value;
typedef unsigned long jit_unsigned_value;

#endif
