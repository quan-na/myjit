/*
 * MyJIT 
 * Copyright (C) 2010, 2015 Petr Krajca, <petr.krajca@upol.cz>
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
#define COMMON86_XMM0	X86_XMM0

#define common86_mov_reg_reg(ptr, reg1, reg2, size) 	x86_mov_reg_reg(ptr, reg1, reg2, size)
#define common86_movsx_reg_reg(ptr, reg1, reg2, size) 	x86_movsx_reg_reg(ptr, reg1, reg2, size)
#define common86_movzx_reg_reg(ptr, reg1, reg2, size) 	x86_movzx_reg_reg(ptr, reg1, reg2, size)
#define common86_mov_reg_imm(ptr, reg, imm) 		x86_mov_reg_imm(ptr, reg, imm)
#define common86_mov_reg_imm_size(ptr, reg, imm, size)	x86_mov_reg_imm(ptr, reg, imm)
#define common86_xchg_reg_reg(ptr, reg1, reg2, size) 	x86_xchg_reg_reg(ptr, reg1, reg2, size)
#define common86_mov_reg_mem(ptr, reg, mem, size) 	x86_mov_reg_mem(ptr, reg, mem, size)
#define common86_movsx_reg_mem(ptr, reg, mem, size) 	x86_movsx_reg_mem(ptr, reg, mem, size)
#define common86_movzx_reg_mem(ptr, reg, mem, size) 	x86_movzx_reg_mem(ptr, reg, mem, size)
#define common86_mov_reg_membase(ptr, reg, basereg, disp, size) 	x86_mov_reg_membase(ptr, reg, basereg, disp, size)
#define common86_movsx_reg_membase(ptr, reg, basereg, disp, size) 	x86_movsx_reg_membase(ptr, reg, basereg, disp, size)
#define common86_movzx_reg_membase(ptr, reg, basereg, disp, size) 	x86_movzx_reg_membase(ptr, reg, basereg, disp, size)
#define common86_mov_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)	x86_mov_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)
#define common86_movsx_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)	x86_movsx_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)
#define common86_movzx_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)	x86_movzx_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)

#define common86_mov_mem_reg(ptr, mem, reg, size) 			x86_mov_mem_reg(ptr, mem, reg, size)
#define common86_mov_mem_imm(ptr, mem, imm, size)			x86_mov_mem_imm(ptr, mem, imm, size)
#define common86_mov_membase_imm(ptr, basereg, disp, imm, size)		x86_mov_membase_imm(ptr, basereg, disp, imm, size)
#define common86_mov_membase_reg(ptr, basereg, disp, reg, size)		x86_mov_membase_reg(ptr, basereg, disp, reg, size)
#define common86_mov_memindex_reg(ptr, basereg, disp, indexreg, shift, reg, size) x86_mov_memindex_reg(ptr, basereg, disp, indexreg, shift, reg, size)
#define common86_mov_memindex_imm(ptr, basereg, disp, indexreg, shift, imm, size) x86_mov_memindex_imm(ptr, basereg, disp, indexreg, shift, imm, size)



#define common86_alu_reg_reg(ptr, op, reg1, reg2) 	x86_alu_reg_reg(ptr, op, reg1, reg2)
#define common86_alu_reg_imm(ptr, op, reg, imm) 	x86_alu_reg_imm(ptr, op, reg, imm)
#define common86_alu_reg_membase(ptr, op, reg, basereg, disp) 	x86_alu_reg_membase(ptr, op, reg, basereg, disp)

#define common86_shift_reg_imm(ptr, op, reg, imm) 	x86_shift_reg_imm(ptr, op, reg, imm)
#define common86_shift_reg(ptr, op, reg) 		x86_shift_reg(ptr, op, reg)
#define common86_mul_reg(ptr, reg, sign) 		x86_mul_reg(ptr, reg, sign)
#define common86_div_reg(ptr, reg, sign) 		x86_div_reg(ptr, reg, sign)
#define common86_div_membase(ptr, basereg, disp, sign) 	x86_div_membase(ptr, basereg, disp, sign)
#define common86_cdq(ptr) 				x86_cdq(ptr)
#define common86_neg_reg(ptr, reg) 			x86_neg_reg(ptr, reg)
#define common86_not_reg(ptr, reg) 			x86_not_reg(ptr, reg)


#define common86_set_reg(ptr, cond, reg, size) 		x86_set_reg(ptr, cond, reg, size)
#define common86_test_reg_reg(ptr, reg1, reg2) 		x86_test_reg_reg(ptr, reg1, reg2)
#define common86_test_reg_imm(ptr, reg, imm) 		x86_test_reg_imm(ptr, reg, imm)
#define common86_branch_disp32(ptr, cond, addr, sign)	x86_branch_disp32(ptr, cond, addr, sign)
#define common86_branch_disp(ptr, cond, addr, sign)	x86_branch_disp(ptr, cond, addr, sign)

#define common86_patch(op, ptr) 			x86_patch(op, ptr)
#define common86_jump_disp(op, disp) 			x86_jump_disp(op, disp)
#define common86_jump_disp32(op, disp) 			x86_jump_disp32(op, disp)
#define common86_jump_reg(op, reg) 			x86_jump_reg(op, reg)
#define common86_ret(ptr)				x86_ret(ptr)

#define common86_lea_membase(ptr, reg, basereg, disp)	x86_lea_membase(ptr, reg, basereg, disp)
#define common86_lea_memindex(ptr, reg, basereg, disp, indexreg, shift)	x86_lea_memindex(ptr, reg, basereg, disp, indexreg, shift)

#define common86_push_reg(ptr, reg) 			x86_push_reg(ptr, reg)
#define common86_pop_reg(ptr, reg) 			x86_pop_reg(ptr, reg)


#define common86_nop(ptr)				x86_nop(ptr)

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
#define COMMON86_XMM0	AMD64_XMM0

#define common86_mov_reg_reg(ptr, reg1, reg2, size) 	amd64_mov_reg_reg(ptr, reg1, reg2, size)
#define common86_movsx_reg_reg(ptr, reg1, reg2, size) 	amd64_movsx_reg_reg(ptr, reg1, reg2, size)
#define common86_movzx_reg_reg(ptr, reg1, reg2, size) 	amd64_movzx_reg_reg(ptr, reg1, reg2, size)
#define common86_mov_reg_imm_size(ptr, reg, imm, size)	amd64_mov_reg_imm_size(ptr, reg, imm, size)
#define common86_mov_reg_imm(ptr, reg, imm)	amd64_mov_reg_imm_size(ptr, reg, imm, sizeof(void *))
#define common86_mov_reg_mem(ptr, reg, mem, size) 	amd64_mov_reg_mem(ptr, reg, mem, size)
#define common86_movsx_reg_mem(ptr, reg, mem, size) 	amd64_movsx_reg_mem(ptr, reg, mem, size)
#define common86_movzx_reg_mem(ptr, reg, mem, size) 	amd64_movzx_reg_mem(ptr, reg, mem, size)
#define common86_mov_reg_membase(ptr, reg, basereg, disp, size) 	amd64_mov_reg_membase(ptr, reg, basereg, disp, size)
#define common86_movsx_reg_membase(ptr, reg, basereg, disp, size) 	amd64_movsx_reg_membase(ptr, reg, basereg, disp, size)
#define common86_movzx_reg_membase(ptr, reg, basereg, disp, size) 	amd64_movzx_reg_membase(ptr, reg, basereg, disp, size)

#define common86_mov_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)	amd64_mov_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)
#define common86_movsx_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)	amd64_movsx_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)
#define common86_movzx_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)	amd64_movzx_reg_memindex(ptr, reg, basereg, disp, indexreg, shift, size)

#define common86_mov_mem_reg(ptr, mem, reg, size) 			amd64_mov_mem_reg(ptr, mem, reg, size)
#define common86_mov_mem_imm(ptr, mem, imm, size)			amd64_mov_mem_imm(ptr, mem, imm, size)
#define common86_mov_membase_imm(ptr, basereg, disp, imm, size)		amd64_mov_membase_imm(ptr, basereg, disp, imm, size)
#define common86_mov_membase_reg(ptr, basereg, disp, reg, size)		amd64_mov_membase_reg(ptr, basereg, disp, reg, size)
#define common86_mov_memindex_reg(ptr, basereg, disp, indexreg, shift, reg, size) amd64_mov_memindex_reg(ptr, basereg, disp, indexreg, shift, reg, size)
#define common86_mov_memindex_imm(ptr, basereg, disp, indexreg, shift, imm, size) amd64_mov_memindex_imm(ptr, basereg, disp, indexreg, shift, imm, size)



#define common86_xchg_reg_reg(ptr, reg1, reg2, size) 	amd64_xchg_reg_reg(ptr, reg1, reg2, size)

#define common86_alu_reg_reg(ptr, op, reg1, reg2) 	amd64_alu_reg_reg(ptr, op, reg1, reg2)
#define common86_alu_reg_imm(ptr, op, reg, imm) 	amd64_alu_reg_imm(ptr, op, reg, imm)
#define common86_alu_reg_membase(ptr, op, reg, basereg, disp) 	amd64_alu_reg_membase(ptr, op, reg, basereg, disp)

#define common86_shift_reg_imm(ptr, op, reg, imm) 	amd64_shift_reg_imm(ptr, op, reg, imm)
#define common86_shift_reg(ptr, op, reg) 		amd64_shift_reg(ptr, op, reg)
#define common86_mul_reg(ptr, reg, sign) 		amd64_mul_reg(ptr, reg, sign)
#define common86_div_reg(ptr, reg, sign) 		amd64_div_reg(ptr, reg, sign)
#define common86_div_membase(ptr, basereg, disp, sign) 	amd64_div_membase(ptr, basereg, disp, sign)
#define common86_cdq(ptr) 				amd64_cdq(ptr)
#define common86_neg_reg(ptr, reg) 			amd64_neg_reg(ptr, reg)
#define common86_not_reg(ptr, reg) 			amd64_not_reg(ptr, reg)


#define common86_set_reg(ptr, cond, reg, size) 		amd64_set_reg(ptr, cond, reg, size)
#define common86_test_reg_reg(ptr, reg1, reg2) 		amd64_test_reg_reg(ptr, reg1, reg2)
#define common86_test_reg_imm(ptr, reg, imm) 		amd64_test_reg_imm(ptr, reg, imm)
#define common86_branch_disp32(ptr, cond, addr, sign)	amd64_branch_disp32(ptr, cond, addr, sign)
#define common86_branch_disp(ptr, cond, addr, sign)	amd64_branch_disp(ptr, cond, addr, sign)

#define common86_patch(op, ptr) 			amd64_patch(op, ptr)
#define common86_jump_disp(op, disp) 			amd64_jump_disp(op, disp)
#define common86_jump_disp32(op, disp) 			amd64_jump_disp32(op, disp)
#define common86_jump_reg(op, reg) 			amd64_jump_reg(op, reg)
#define common86_ret(ptr)				amd64_ret(ptr)

#define common86_lea_membase(ptr, reg, basereg, disp)	amd64_lea_membase(ptr, reg, basereg, disp)
#define common86_lea_memindex(ptr, reg, basereg, disp, indexreg, shift)	amd64_lea_memindex(ptr, reg, basereg, disp, indexreg, shift)

#define common86_push_reg(ptr, reg) 			amd64_push_reg(ptr, reg)
#define common86_pop_reg(ptr, reg) 			amd64_pop_reg(ptr, reg)

#define common86_nop(ptr)				amd64_nop(ptr)

#endif
