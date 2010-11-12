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

/* This header file provides mappings of macros common to x86 and amd64 platform
 * according to currently used CPU.
 */
#ifdef JIT_ARCH_I386
#include "x86-codegen.h"

#define COMMON86_AX	X86_EAX
#define COMMON86_BX	X86_EBX
#define COMMON86_CX	X86_ECX
#define COMMON86_DX	X86_EDX
#define COMMON86_SI	X86_ESI
#define COMMON86_DI	X86_EDI
#define COMMON86_SP	X86_ESP
#define COMMON86_BP	X86_EBP

#define common86_mov_reg_reg(ptr, reg1, reg2, size) 	x86_mov_reg_reg(ptr, reg1, reg2, size)
#define common86_alu_reg_reg(ptr, op, reg1, reg2) 	x86_alu_reg_reg(ptr, op, reg1, reg2)
#define common86_alu_reg_imm(ptr, op, reg, imm) 	x86_alu_reg_imm(ptr, op, reg, imm)
#define common86_alu_reg_membase(ptr, op, reg, basereg, disp) 	x86_alu_reg_membase(ptr, op, reg, basereg, disp)
#define common86_neg_reg(ptr, reg) 			x86_neg_reg(ptr, reg)
#define common86_lea_membase(ptr, reg, basereg, disp)	x86_lea_membase(ptr, reg, basereg, disp)
#define common86_push_reg(ptr, reg) 			x86_push_reg(ptr, reg)


#endif

#ifdef JIT_ARCH_AMD64
#include "amd64-codegen.h"

#define COMMON86_AX	AMD64_RAX
#define COMMON86_BX	AMD64_RBX
#define COMMON86_CX	AMD64_RCX
#define COMMON86_DX	AMD64_RDX
#define COMMON86_SI	AMD64_RSI
#define COMMON86_DI	AMD64_RDI
#define COMMON86_SP	AMD64_RSP
#define COMMON86_BP	AMD64_RBP

#define common86_mov_reg_reg(ptr, reg1, reg2, size) 	amd64_mov_reg_reg(ptr, reg1, reg2, size)
#define common86_alu_reg_reg(ptr, op, reg1, reg2) 	amd64_alu_reg_reg(ptr, op, reg1, reg2)
#define common86_alu_reg_imm(ptr, op, reg, imm) 	amd64_alu_reg_imm(ptr, op, reg, imm)
#define common86_alu_reg_membase(ptr, op, reg, basereg, disp) 	amd64_alu_reg_membase(ptr, op, reg, basereg, disp)
#define common86_neg_reg(ptr, reg) 			amd64_neg_reg(ptr, reg)
#define common86_lea_membase(ptr, reg, basereg, disp)	amd64_lea_membase(ptr, reg, basereg, disp)
#define common86_push_reg(ptr, reg) 			amd64_push_reg(ptr, reg)

#endif
