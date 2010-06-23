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
#ifdef __arch32__
#define JIT_ARCH_I386
#else
#define JIT_ARCH_AMD64
#endif

/*
 * i386 related macros
 */
#ifdef JIT_ARCH_AMD64

// number of register aliases
#define JIT_ALIAS_CNT           (2)     /* JIT_RETREG + JIT_FP */

// number of special purpose registers
#define JIT_SPP_REGS_CNT        (1 + 6) /* immediate + register for input arguments */

#endif

/*
 * AMD64 related macros
 */
#ifdef JIT_ARCH_AMD64

// number of register aliases
#define JIT_ALIAS_CNT           (2)     /* JIT_RETREG + JIT_FP */

// number of special purpose registers
#define JIT_SPP_REGS_CNT        (1 + 6) /* immediate + register for input arguments */
#endif


/*
 * Common macros
 */

// id of the first register
#define JIT_FIRST_REG   (JIT_ALIAS_CNT)

#define R(x)    ((x) + JIT_ALIAS_CNT + JIT_SPP_REGS_CNT)
#endif