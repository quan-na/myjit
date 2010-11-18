/*
 * MyJIT 
 * Copyright (C) 2010 Petr Krajca, <krajcap@inf.upol.cz>
 *
 * Common stuff for i386 and AMD64 platforms
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

#define JIT_X86_STI     (0x0100 << 3)
#define JIT_X86_STXI    (0x0101 << 3)


#ifdef JIT_ARCH_AMD64
#define sse_alu_sd_reg_reg(ip,op,r1,r2) amd64_sse_alu_sd_reg_reg(ip,op,r1,r2)
#define sse_alu_pd_reg_reg(ip,op,r1,r2) amd64_sse_alu_pd_reg_reg(ip,op,r1,r2)
#define sse_movsd_reg_reg(ip,r1,r2) amd64_sse_movsd_reg_reg(ip,r1,r2)
#define sse_movsd_reg_mem(ip, r1, mem) amd64_movsd_reg_mem(ip,r1,mem)
#define sse_movlpd_membase_xreg(ip,dreg,basereg,disp) amd64_sse_movlpd_membase_xreg(ip,dreg,basereg,disp)
#define sse_movlpd_xreg_membase(ip,dreg,basereg,disp) amd64_sse_movlpd_xreg_membase(ip,dreg,basereg,disp)
#define sse_movlpd_memindex_xreg(ip, basereg, disp, indexreg, shift, reg)  amd64_sse_movlpd_memindex_xreg(ip, basereg, disp, indexreg, shift, reg)


#define sse_movlpd_mem_reg(ip, mem, reg) 			amd64_movlpd_mem_reg(ip, mem, reg)
#define sse_movlpd_xreg_memindex(ip, dreg, basereg, disp, indexreg, shift) 	amd64_sse_movlpd_xreg_memindex(ip, dreg, basereg, disp, indexreg, shift)

#define sse_movss_membase_reg(ip, basereg, disp, reg)		amd64_movss_membase_reg(ip, basereg, disp, reg)
#define sse_movss_mem_reg(ip, mem, reg)				amd64_movss_mem_reg(ip, mem, reg)
#define sse_movss_memindex_xreg(ip, basereg, disp, indexreg, shift, reg)  x86_movss_memindex_xreg(ip, reg, basereg, disp, indexreg, shift)

#define sse_comisd_reg_reg(ip, r1, r2)                  amd64_sse_comisd_reg_reg(ip, r1, r2)
#define sse_alu_pd_reg_reg_imm(ip, op, r1, r2, imm)     amd64_sse_alu_pd_reg_reg_imm(ip, op, r1, r2, imm)

#define sse_cvttsd2si_reg_reg(ip, r1, r2) 		amd64_sse_cvttsd2si_reg_reg(ip, r1, r2)
#define sse_cvtsi2sd_reg_reg(ip, r1, r2) 		amd64_sse_cvtsi2sd_reg_reg(ip, r1, r2)
#define sse_cvtsd2ss_reg_reg(ip, r1, r2) 		amd64_sse_cvtsd2ss_reg_reg(ip, r1, r2)
#define sse_cvtss2sd_reg_reg(ip, r1, r2) 		amd64_sse_cvtss2sd_reg_mem(ip, r1, r2)
#define sse_cvtss2sd_reg_mem(ip, r1, mem) 		amd64_sse_cvtss2sd_reg_mem(ip, r1, mem)
#define sse_cvtss2sd_reg_membase(ip, r1, basereg, disp)	amd64_sse_cvtss2sd_reg_membase(ip, r1, basereg, disp)
#define sse_cvtss2sd_reg_memindex(ip, r1, basereg, disp, indexreg, shift)	amd64_sse_cvtss2sd_reg_memindex(ip, r1, basereg, disp, indexreg, shift)


/**
 * This function emits SSE code which assigns value value which resides in the memory
 * in to the XMM register If the value is not addressable with 32bit address, unused 
 * register from the register pool is used to access this value.
 */
static void sse_reg_safeimm(struct jit * jit, jit_value reg, double * imm)
{
	if (((jit_value)imm) > 0xffffffffUL) {
		if (jit->reg_al->gp_regpool->pos >= 0) {
			int r = jit->reg_al->gp_regpool->pool[0]->id;
			amd64_mov_reg_imm(jit->ip, r, (jit_value)imm);
			amd64_movsd_reg_membase(jit->ip, reg, r, 0);
		} else {
			amd64_push_reg(jit->ip, AMD64_RAX);
			amd64_mov_reg_imm(jit->ip, AMD64_RAX, (jit_value)imm);
			amd64_movsd_reg_membase(jit->ip, reg, AMD64_RAX, 0);
			amd64_pop_reg(jit->ip, AMD64_RAX);
		}
	} else {
		amd64_movsd_reg_mem(jit->ip, reg, (jit_value)imm);
	}
}

/**
 * This function emits SSE code involving arithmetic (packed) operation and immediate
 * value which resides in the memory. If the value is not addressable with 32bit
 * address, unused register from the register pool is used to access
 * this value.
 */
static void sse_alu_pd_reg_safeimm(struct jit * jit, int op, int reg, double * imm)
{
	if (((jit_value)imm) > 0xffffffffUL) {
		if (jit->reg_al->gp_regpool->pos >= 0) {
			int r = jit->reg_al->gp_regpool->pool[0]->id;
			amd64_mov_reg_imm(jit->ip, r, (long)imm);
			amd64_sse_alu_pd_reg_membase(jit->ip, op, reg, r, 0);
		} else {
			amd64_push_reg(jit->ip, AMD64_RAX);
			amd64_mov_reg_imm(jit->ip, AMD64_RAX, (long)imm);
			amd64_sse_alu_pd_reg_membase(jit->ip, op, reg, AMD64_RAX, 0);
			amd64_pop_reg(jit->ip, AMD64_RAX);
		}
	} else {
		amd64_sse_alu_pd_reg_mem(jit->ip, op, reg, (long)imm);
	}
}

/**
 * This function emits SSE code involving arithmetic (single) operation and immediate
 * value which resides in the memory. If the value is not addressable with 32bit
 * address, unused register from the register pool is used to access
 * this value.
 */
static void sse_alu_sd_reg_safeimm(struct jit * jit, int op, int reg, double * imm)
{
	if (((jit_value)imm) > 0xffffffffUL) {
		if (jit->reg_al->gp_regpool->pos >= 0) {
			int r = jit->reg_al->gp_regpool->pool[0]->id;
			amd64_mov_reg_imm(jit->ip, r, (long)imm);
			amd64_sse_alu_sd_reg_membase(jit->ip, op, reg, r, 0);
		} else {
			amd64_push_reg(jit->ip, AMD64_RAX);
			amd64_mov_reg_imm(jit->ip, AMD64_RAX, (long)imm);
			amd64_sse_alu_sd_reg_membase(jit->ip, op, reg, AMD64_RAX, 0);
			amd64_pop_reg(jit->ip, AMD64_RAX);
		}
	} else {
		amd64_sse_alu_sd_reg_mem(jit->ip, op, reg, (long)imm);
	}
}

#else
#define sse_movsd_reg_reg(ip, r1, r2) 				x86_movsd_reg_reg(ip, r1, r2)
#define sse_movsd_reg_mem(ip, r1, mem) 				x86_movsd_reg_mem(ip, r1, mem)
#define sse_movlpd_membase_xreg(ip, dreg, basereg, disp) 	x86_movlpd_membase_xreg(ip, dreg, basereg, disp)
#define sse_movlpd_xreg_membase(ip, dreg, basereg, disp) 	x86_movlpd_xreg_membase(ip, dreg, basereg, disp)
#define sse_movlpd_mem_reg(ip, mem, reg) 			x86_movlpd_mem_xreg(ip, mem, reg)
#define sse_movlpd_xreg_memindex(ip, dreg, basereg, disp, indexreg, shift) 	x86_movlpd_xreg_memindex(ip, dreg, basereg, disp, indexreg, shift)
#define sse_movss_membase_reg(ip, basereg, disp, reg)		x86_movss_membase_xreg(ip, reg, basereg, disp)
#define sse_movss_mem_reg(ip, mem, reg)				x86_movss_mem_xreg(ip, reg, mem)
#define sse_movss_memindex_xreg(ip, basereg, disp, indexreg, shift, reg)  x86_movss_memindex_xreg(ip, reg, basereg, disp, indexreg, shift)
#define sse_movlpd_memindex_xreg(ip, basereg, disp, indexreg, shift, reg)  x86_movlpd_memindex_xreg(ip, reg, basereg, disp, indexreg, shift)

#define sse_alu_sd_reg_reg(ip, op, r1, r2) 		x86_sse_alu_sd_reg_reg(ip, op, r1, r2)
#define sse_alu_sd_reg_safeimm(jit, op, reg, imm) 	x86_sse_alu_sd_reg_mem(jit->ip, op, reg, imm)

#define sse_alu_pd_reg_reg(ip, op, r1, r2) 		x86_sse_alu_pd_reg_reg(ip, op, r1, r2)
#define sse_alu_pd_reg_reg_imm(ip, op, r1, r2, imm) 	x86_sse_alu_pd_reg_reg_imm(ip, op, r1, r2, imm)
#define sse_alu_pd_reg_safeimm(jit, op, reg, imm) 	x86_sse_alu_pd_reg_mem(jit->ip, op, reg, imm)

#define sse_comisd_reg_reg(ip, r1, r2)			x86_sse_alu_pd_reg_reg(ip, X86_SSE_COMI, r1, r2)

#define sse_cvttsd2si_reg_reg(ip, r1, r2) 		x86_cvttsd2si(ip, r1, r2)
#define sse_cvtsi2sd_reg_reg(ip, r1, r2) 		x86_cvtsi2sd(ip, r1, r2)
#define sse_cvtsd2ss_reg_reg(ip, r1, r2)		x86_cvtsd2ss(ip, r1, r2)
#define sse_cvtss2sd_reg_mem(ip, r1, mem) 		x86_cvtss2sd_reg_mem(ip, r1, mem)
#define sse_cvtss2sd_reg_membase(ip, r1, basereg, disp)	x86_cvtss2sd_reg_membase(ip, r1, basereg, disp)
#define sse_cvtss2sd_reg_memindex(ip, r1, basereg, disp, indexreg, shift)	x86_cvtss2sd_reg_memindex(ip, r1, basereg, disp, indexreg, shift)

#endif


//
//
// Common function which generates general operations
//
//

static void __alu_op(struct jit * jit, struct jit_op * op, int x86_op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_alu_reg_imm(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);

	}  else {
		if (op->r_arg[0] == op->r_arg[1]) {
			common86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
		} else if (op->r_arg[0] == op->r_arg[2]) {
			common86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[1]);
		} else {
			common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
			common86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
		}	
	}
}

static inline void __sub_op(struct jit * jit, struct jit_op * op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) common86_lea_membase(jit->ip, op->r_arg[0], op->r_arg[1], -op->r_arg[2]);
		else common86_alu_reg_imm(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
		return;

	}
	if (op->r_arg[0] == op->r_arg[1]) {
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
	} else if (op->r_arg[0] == op->r_arg[2]) {
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
		common86_neg_reg(jit->ip, op->r_arg[0]);
	} else {
		common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
	}	
}

static inline void __subx_op(struct jit * jit, struct jit_op * op, int x86_op, int imm)
{
	if (imm) {
		if (op->r_arg[0] != op->r_arg[1]) common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_alu_reg_imm(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
		return;

	}
	if (op->r_arg[0] == op->r_arg[1]) {
		common86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
	} else if (op->r_arg[0] == op->r_arg[2]) {
		common86_push_reg(jit->ip, op->r_arg[2]);
		common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_alu_reg_membase(jit->ip, x86_op, op->r_arg[0], COMMON86_SP, 0);
		common86_alu_reg_imm(jit->ip, X86_ADD, COMMON86_SP, 8);
	} else {
		common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_alu_reg_reg(jit->ip, x86_op, op->r_arg[0], op->r_arg[2]);
	}	
}

static inline void __rsb_op(struct jit * jit, struct jit_op * op, int imm)
{
	if (imm) {
		if (op->r_arg[0] == op->r_arg[1]) common86_alu_reg_imm(jit->ip, X86_ADD, op->r_arg[0], -op->r_arg[2]);
		else common86_lea_membase(jit->ip, op->r_arg[0], op->r_arg[1], -op->r_arg[2]);
		common86_neg_reg(jit->ip, op->r_arg[0]);
		return;
	}

	if (op->r_arg[0] == op->r_arg[1]) { // O1 = O3 - O1
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[2]);
		common86_neg_reg(jit->ip, op->r_arg[0]);
	} else if (op->r_arg[0] == op->r_arg[2]) { // O1 = O1 - O2
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
	} else {
		common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[2], REG_SIZE);
		common86_alu_reg_reg(jit->ip, X86_SUB, op->r_arg[0], op->r_arg[1]);
	}
}

static inline void __mul(struct jit * jit, struct jit_op * op, int imm, int sign, int high_bytes)
{
	jit_value dest = op->r_arg[0];
	jit_value factor1 = op->r_arg[1];
	jit_value factor2 = op->r_arg[2];

	// optimization for special cases 
	if ((!high_bytes) && (imm)) {
		switch (factor2) {
			case 2: if (factor1 == dest) common86_shift_reg_imm(jit->ip, X86_SHL, dest, 1);
				else common86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 0);
				return;

			case 3: common86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 1); return;

			case 4: if (factor1 != dest) common86_mov_reg_reg(jit->ip, dest, factor1, REG_SIZE);
				common86_shift_reg_imm(jit->ip, X86_SHL, dest, 2);
				return;
			case 5: common86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 2);
				return;
			case 8: if (factor1 != dest) common86_mov_reg_reg(jit->ip, dest, factor1, REG_SIZE);
				common86_shift_reg_imm(jit->ip, X86_SHL, dest, 3);
				return;
			case 9: common86_lea_memindex(jit->ip, dest, factor1, 0, factor1, 3);
				return;
		}
	}


	// generic multiplication
	int ax_in_use = jit_reg_in_use(op, COMMON86_AX, 0);
	int dx_in_use = jit_reg_in_use(op, COMMON86_DX, 0);

	if ((dest != COMMON86_AX) && ax_in_use) common86_push_reg(jit->ip, COMMON86_AX);
	if ((dest != COMMON86_DX) && dx_in_use) common86_push_reg(jit->ip, COMMON86_DX);

	if (imm) {
		if (factor1 != COMMON86_AX) common86_mov_reg_reg(jit->ip, COMMON86_AX, factor1, REG_SIZE);
		common86_mov_reg_imm(jit->ip, COMMON86_DX, factor2);
		common86_mul_reg(jit->ip, COMMON86_DX, sign);
	} else {
		if (factor1 == COMMON86_AX) common86_mul_reg(jit->ip, factor2, sign);
		else if (factor2 == COMMON86_AX) common86_mul_reg(jit->ip, factor1, sign);
		else {
			common86_mov_reg_reg(jit->ip, COMMON86_AX, factor1, REG_SIZE);
			common86_mul_reg(jit->ip, factor2, sign);
		}
	}

	if (!high_bytes) {
		if (dest != COMMON86_AX) common86_mov_reg_reg(jit->ip, dest, COMMON86_AX, REG_SIZE);
	} else {
		if (dest != COMMON86_DX) common86_mov_reg_reg(jit->ip, dest, COMMON86_DX, REG_SIZE);
	}

	if ((dest != COMMON86_DX) && dx_in_use) common86_pop_reg(jit->ip, COMMON86_DX);
	if ((dest != COMMON86_AX) && ax_in_use) common86_pop_reg(jit->ip, COMMON86_AX);
}

static inline void __div(struct jit * jit, struct jit_op * op, int imm, int sign, int modulo)
{
	jit_value dest = op->r_arg[0];
	jit_value dividend = op->r_arg[1];
	jit_value divisor = op->r_arg[2];

	if (imm && ((divisor == 2) || (divisor == 4) || (divisor == 8))) {
		if (dest != dividend) common86_mov_reg_reg(jit->ip, dest, dividend, REG_SIZE);
		if (!modulo) {
			switch (divisor) {
				case 2: common86_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 1); break;
				case 4: common86_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 2); break;
				case 8: common86_shift_reg_imm(jit->ip, sign ? X86_SAR : X86_SHR, dest, 3); break;
			}
		} else {
			switch (divisor) {
				case 2: common86_alu_reg_imm(jit->ip, X86_AND, dest, 0x1); break;
				case 4: common86_alu_reg_imm(jit->ip, X86_AND, dest, 0x3); break;
				case 8: common86_alu_reg_imm(jit->ip, X86_AND, dest, 0x7); break;
			}
		}
		return;
	}

	int ax_in_use = jit_reg_in_use(op, COMMON86_AX, 0);
	int dx_in_use = jit_reg_in_use(op, COMMON86_DX, 0);

	if ((dest != COMMON86_AX) && ax_in_use) common86_push_reg(jit->ip, COMMON86_AX);
	if ((dest != COMMON86_DX) && dx_in_use) common86_push_reg(jit->ip, COMMON86_DX);

	if (imm) {
		if (dividend != COMMON86_AX) common86_mov_reg_reg(jit->ip, COMMON86_AX, dividend, REG_SIZE);
		common86_cdq(jit->ip);
		if (dest != COMMON86_BX) common86_push_reg(jit->ip, COMMON86_BX);
		common86_mov_reg_imm_size(jit->ip, COMMON86_BX, divisor, REG_SIZE);
		common86_div_reg(jit->ip, COMMON86_BX, sign);
		if (dest != COMMON86_BX) common86_pop_reg(jit->ip, COMMON86_BX);
	} else {
		if ((divisor == COMMON86_AX) || (divisor == COMMON86_DX)) {
			common86_push_reg(jit->ip, divisor);
		}

		if (dividend != COMMON86_AX) common86_mov_reg_reg(jit->ip, COMMON86_AX, dividend, REG_SIZE);

		common86_cdq(jit->ip);

		if ((divisor == COMMON86_AX) || (divisor == COMMON86_DX)) {
			common86_div_membase(jit->ip, COMMON86_SP, 0, sign);
			common86_alu_reg_imm(jit->ip, X86_ADD, COMMON86_SP, 8);
		} else {
			common86_div_reg(jit->ip, divisor, sign);
		}
	}

	if (!modulo) {
		if (dest != COMMON86_AX) common86_mov_reg_reg(jit->ip, dest, COMMON86_AX, REG_SIZE);
	} else {
		if (dest != COMMON86_DX) common86_mov_reg_reg(jit->ip, dest, COMMON86_DX, REG_SIZE);
	}

	if ((dest != COMMON86_DX) && dx_in_use) common86_pop_reg(jit->ip, COMMON86_DX);
	if ((dest != COMMON86_AX) && ax_in_use) common86_pop_reg(jit->ip, COMMON86_AX);
}

static inline void __shift_op(struct jit * jit, struct jit_op * op, int shift_op, int imm)
{
	if (imm) { 
		if (op->r_arg[0] != op->r_arg[1]) common86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		common86_shift_reg_imm(jit->ip, shift_op, op->r_arg[0], op->r_arg[2]);
	} else {
		int destreg = op->r_arg[0];
		int valreg = op->r_arg[1];
		int shiftreg = op->r_arg[2];
		int cx_pathology = 0; // shifting content in the ECX register

		int cx_in_use = jit_reg_in_use(op, COMMON86_CX, 0);
		int dx_in_use = jit_reg_in_use(op, COMMON86_DX, 0);

		if (destreg == COMMON86_CX) {
			cx_pathology = 1;
			if (dx_in_use) common86_push_reg(jit->ip, COMMON86_DX); 
			destreg = COMMON86_DX;
		}

		if (shiftreg != COMMON86_CX) {
			if (cx_in_use) common86_push_reg(jit->ip, COMMON86_CX); 
			common86_mov_reg_reg(jit->ip, COMMON86_CX, shiftreg, REG_SIZE);
		}
		if (destreg != valreg) common86_mov_reg_reg(jit->ip, destreg, valreg, REG_SIZE); 
		common86_shift_reg(jit->ip, shift_op, destreg);
		if (cx_pathology) {
			common86_mov_reg_reg(jit->ip, COMMON86_CX, COMMON86_DX, REG_SIZE);
			if ((shiftreg != COMMON86_CX) && cx_in_use) common86_alu_reg_imm(jit->ip, X86_ADD, COMMON86_SP, REG_SIZE);
			if (dx_in_use) common86_pop_reg(jit->ip, COMMON86_DX);
		} else {
			if ((shiftreg != COMMON86_CX) && cx_in_use) common86_pop_reg(jit->ip, COMMON86_CX);
		}
	}
}

static inline void __cond_op(struct jit * jit, struct jit_op * op, int amd64_cond, int imm, int sign)
{
	if (imm) common86_alu_reg_imm(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);
	else common86_alu_reg_reg(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);
	if ((op->r_arg[0] != COMMON86_SI) && (op->r_arg[0] != COMMON86_DI)) {
		common86_mov_reg_imm(jit->ip, op->r_arg[0], 0);
		common86_set_reg(jit->ip, amd64_cond, op->r_arg[0], sign);
	} else {
		common86_xchg_reg_reg(jit->ip, COMMON86_AX, op->r_arg[0], REG_SIZE);
		common86_mov_reg_imm(jit->ip, COMMON86_AX, 0);
		common86_set_reg(jit->ip, amd64_cond, COMMON86_AX, sign);
		common86_xchg_reg_reg(jit->ip, COMMON86_AX, op->r_arg[0], REG_SIZE);
	}
}

static inline void __branch_op(struct jit * jit, struct jit_op * op, int cond, int imm, int sign)
{
	if (imm) common86_alu_reg_imm(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);
	else common86_alu_reg_reg(jit->ip, X86_CMP, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	common86_branch_disp32(jit->ip, cond, __JIT_GET_ADDR(jit, op->r_arg[0]), sign);
}

static inline void __branch_mask_op(struct jit * jit, struct jit_op * op, int cond, int imm)
{
	if (imm) common86_test_reg_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
	else common86_test_reg_reg(jit->ip, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	common86_branch_disp32(jit->ip, cond, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}

static inline void __branch_overflow_op(struct jit * jit, struct jit_op * op, int alu_op, int imm)
{
	if (imm) common86_alu_reg_imm(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);
	else common86_alu_reg_reg(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	common86_branch_disp32(jit->ip, X86_CC_O, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}

/* determines whether the argument value was spilled out or not,
 * if the register is associated with the hardware register
 * it is returned through the reg argument
 */
// FIXME: reports as spilled also argument which contains appropriate value
static inline int __is_spilled(int arg_id, jit_op * prepare_op, int * reg)
{
        struct __hw_reg * hreg = rmap_get(prepare_op->regmap, arg_id);

        if (hreg) {
                *reg = hreg->id;
                return 0;
        } else return 1;
}

/**
 * Emits all LD operations
 */
static inline void emit_ld_op(struct jit * jit, jit_op * op, jit_value a1, jit_value a2)
{
	if (op->arg_size == REG_SIZE) {
		if (IS_IMM(op)) common86_mov_reg_mem(jit->ip, a1, a2, op->arg_size);
		else common86_mov_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
		return;
	} 

	switch (op->code) {
		case (JIT_LD | IMM | SIGNED): common86_movsx_reg_mem(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_LD | IMM | UNSIGNED): common86_movzx_reg_mem(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_LD | REG | SIGNED): common86_movsx_reg_membase(jit->ip, a1, a2, 0, op->arg_size); break;
		case (JIT_LD | REG | UNSIGNED): common86_movzx_reg_membase(jit->ip, a1, a2, 0, op->arg_size); break;
		default: assert(0);
	}
}

/**
 * Emits all LD operations
 */
static inline void emit_ldx_op(struct jit * jit, jit_op * op, jit_value a1, jit_value a2, jit_value a3)
{
	if (op->arg_size == REG_SIZE) {
		if (IS_IMM(op)) common86_mov_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
		else common86_mov_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
		return;
	} 
	
	switch (op->code) {
		case (JIT_LDX | IMM | SIGNED): common86_movsx_reg_membase(jit->ip, a1, a2, a3, op->arg_size); break;
		case (JIT_LDX | IMM | UNSIGNED): common86_movzx_reg_membase(jit->ip, a1, a2, a3, op->arg_size); break;
		case (JIT_LDX | REG | SIGNED): common86_movsx_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); break;
		case (JIT_LDX | REG | UNSIGNED): common86_movzx_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); break;
		default: assert(0);
	}
}

//
//
// Registers
//
//

static inline void __push_reg(struct jit * jit, struct __hw_reg * r)
{
	if (!r->fp) common86_push_reg(jit->ip, r->id);
	else {
		common86_alu_reg_imm(jit->ip, X86_SUB, COMMON86_SP, 8);
		sse_movlpd_membase_xreg(jit->ip, r->id, COMMON86_SP, 0);
	}
}

static inline void __pop_reg(struct jit * jit, struct __hw_reg * r)
{
	if (!r->fp) common86_pop_reg(jit->ip, r->id);
	else {
		sse_movlpd_xreg_membase(jit->ip, r->id, COMMON86_SP, 0);
		common86_alu_reg_imm(jit->ip, X86_ADD, COMMON86_SP, 8);
	}
}

static inline int __uses_hw_reg(struct jit_op * op, long reg, int fp)
{
	for (int i = 0; i < 3; i++)
		if ((ARG_TYPE(op, i + 1) == REG) || (ARG_TYPE(op, i + 1) == TREG)) {
			if (fp && (JIT_REG(op->arg[i]).type == JIT_RTYPE_INT)) continue;
			if (!fp && (JIT_REG(op->arg[i]).type == JIT_RTYPE_FLOAT)) continue;
			if (op->r_arg[i] == reg) return 1;
		}
	return 0;
}

static int __push_callee_saved_regs(struct jit * jit, struct jit_op * op)
{
	int count = 0;
	for (int i = 0; i < jit->reg_al->gp_reg_cnt; i++) {
		struct __hw_reg * r = &(jit->reg_al->gp_regs[i]);
		if (r->callee_saved) 
			for (struct jit_op * o = op->next; o != NULL; o = o->next) {
				if (GET_OP(o) == JIT_PROLOG) break;
				if (__uses_hw_reg(o, r->id, 0)) {
					__push_reg(jit, r);
					count++;
					break;
				}
			}
	}
	return count;
}

static int __pop_callee_saved_regs(struct jit * jit)
{
	int count = 0;
	struct jit_op * op = jit->current_func;

	for (int i = jit->reg_al->gp_reg_cnt - 1; i >= 0; i--) {
		struct __hw_reg * r = &(jit->reg_al->gp_regs[i]);
		if (r->callee_saved) 
			for (struct jit_op * o = op->next; o != NULL; o = o->next) {
				if (GET_OP(o) == JIT_PROLOG) break;
				if (__uses_hw_reg(o, r->id, 0)) {
					__pop_reg(jit, r);
					count++;
					break;
				}
			}
	}
	return count;
}

static int __generic_push_caller_saved_regs(struct jit * jit, jit_op * op, int reg_count,
						    struct __hw_reg * regs, int fp, int skip_reg)
{
	int reg;
	int count = 0;
	for (int i = 0; i < reg_count; i++) {
		if ((regs[i].id == skip_reg) || (regs[i].callee_saved)) continue;
		struct __hw_reg * hreg = rmap_is_associated(op->regmap, regs[i].id, fp, &reg);
		if (hreg && jitset_get(op->live_in, reg)) __push_reg(jit, hreg), count++;
	}
	return count;
}

static int __push_caller_saved_regs(struct jit * jit, jit_op * op)
{
	while (op) {
		if (GET_OP(op) == JIT_CALL) break;
		op = op->next;
	}
	int count = __generic_push_caller_saved_regs(jit, op, jit->reg_al->gp_reg_cnt, jit->reg_al->gp_regs, 0, jit->reg_al->ret_reg);
	count += __generic_push_caller_saved_regs(jit, op, jit->reg_al->fp_reg_cnt, jit->reg_al->fp_regs, 1, jit->reg_al->fpret_reg);
	return count;
}

static int __generic_pop_caller_saved_regs(struct jit * jit, jit_op * op, int reg_count,
						    struct __hw_reg * regs, int fp, int skip_reg)
{
	int reg;
	int count = 0;
	for (int i = reg_count - 1; i >= 0; i--) {
		if ((regs[i].id == skip_reg) || (regs[i].callee_saved)) continue;
		struct __hw_reg * hreg = rmap_is_associated(op->regmap, regs[i].id, fp, &reg);
		if (hreg && jitset_get(op->live_in, reg)) __pop_reg(jit, hreg), count++;
	}
	return count;
}

static int __pop_caller_saved_regs(struct jit * jit, jit_op * op)
{
	int count = __generic_pop_caller_saved_regs(jit, op, jit->reg_al->fp_reg_cnt, jit->reg_al->fp_regs, 1, jit->reg_al->fpret_reg);
	count += __generic_pop_caller_saved_regs(jit, op, jit->reg_al->gp_reg_cnt, jit->reg_al->gp_regs, 0, jit->reg_al->ret_reg);
	return count;
}

//
//
// SSE2 
//
//
//


static void __sse_change_sign(struct jit * jit, int reg);

static void __sse_alu_op(struct jit * jit, jit_op * op, int sse_op)
{
	if (op->r_arg[0] == op->r_arg[1]) {
		sse_alu_sd_reg_reg(jit->ip, sse_op, op->r_arg[0], op->r_arg[2]);
	} else if (op->r_arg[0] == op->r_arg[2]) {
		sse_alu_sd_reg_reg(jit->ip, sse_op, op->r_arg[0], op->r_arg[1]);
	} else {
		sse_movsd_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1]);
		sse_alu_sd_reg_reg(jit->ip, sse_op, op->r_arg[0], op->r_arg[2]);
	}
}

static unsigned char * __sse_get_sign_mask()
{
	// gets 16-bytes aligned value
	static unsigned char bufx[32];
	unsigned char * buf = bufx + 1;
	while ((long)buf % 16) buf++;
	unsigned long long * bit_mask = (unsigned long long *)buf;

	// inverts 64th (sing) bit
	*bit_mask = (unsigned long long)1 << 63;
	return buf;
}

static void __sse_change_sign(struct jit * jit, int reg)
{
	sse_alu_pd_reg_safeimm(jit, X86_SSE_XOR, reg, (double *)__sse_get_sign_mask());
}

static void __sse_sub_op(struct jit * jit, long a1, long a2, long a3)
{
	if (a1 == a2) {
		sse_alu_sd_reg_reg(jit->ip, X86_SSE_SUB, a1, a3);
	} else if (a1 == a3) {
		sse_alu_sd_reg_reg(jit->ip, X86_SSE_SUB, a1, a2);
		__sse_change_sign(jit, a1);
	} else {
		sse_movsd_reg_reg(jit->ip, a1, a2);
		sse_alu_sd_reg_reg(jit->ip, X86_SSE_SUB, a1, a3);
	}
}

static void __sse_div_op(struct jit * jit, long a1, long a2, long a3)
{
	if (a1 == a2) {
		sse_alu_sd_reg_reg(jit->ip, X86_SSE_DIV, a1, a3);
	} else if (a1 == a3) {
		// creates a copy of the a2 into high bits of a2
		x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);

		// divides a2 by a3 and moves to the results
		sse_alu_sd_reg_reg(jit->ip, X86_SSE_DIV, a2, a3);
		sse_movsd_reg_reg(jit->ip, a1, a2); 

		// returns the the value of a2
		x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
	} else {
		sse_movsd_reg_reg(jit->ip, a1, a2); 
		sse_alu_sd_reg_reg(jit->ip, X86_SSE_DIV, a1, a3);
	}
}

static void __sse_neg_op(struct jit * jit, long a1, long a2)
{
	if (a1 != a2) sse_movsd_reg_reg(jit->ip, a1, a2); 
	__sse_change_sign(jit, a1);
}

static void __sse_branch(struct jit * jit, jit_op * op, long a1, long a2, long a3, int x86_cond)
{
        sse_alu_pd_reg_reg(jit->ip, X86_SSE_COMI, a2, a3);
        op->patch_addr = __PATCH_ADDR(jit);
        x86_branch_disp32(jit->ip, x86_cond, __JIT_GET_ADDR(jit, a1), 0);
}

static inline void __sse_round(struct jit * jit, jit_value a1, jit_value a2)
{
	static const double x0 = 0.0;
	static const double x05 = 0.5;

	// creates a copy of the a2 and tmp_reg into high bits of a2 and tmp_reg
	sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);

	sse_alu_pd_reg_safeimm(jit, X86_SSE_COMI, a2, (double *)&x0);

	unsigned char * branch1 = jit->ip;
	common86_branch_disp(jit->ip, X86_CC_LT, 0, 0);

	sse_alu_sd_reg_safeimm(jit, X86_SSE_ADD, a2, (double *)&x05);

	unsigned char * branch2 = jit->ip;
	common86_jump_disp(jit->ip, 0);

	common86_patch(branch1, jit->ip);

	sse_alu_sd_reg_safeimm(jit, X86_SSE_SUB, a2, (double *)&x05);
	common86_patch(branch2, jit->ip);

	sse_cvttsd2si_reg_reg(jit->ip, a1, a2);

	// returns values back
	sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
}

static inline void __sse_floor(struct jit * jit, jit_value a1, jit_value a2, int floor)
{
	int tmp_reg = (a2 == X86_XMM7 ? X86_XMM0 : X86_XMM7);

	// creates a copy of the a2 and tmp_reg into high bits of a2 and tmp_reg
	sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);
	// TODO: test if the register is in use or not
	sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, tmp_reg, tmp_reg, 0);

	// truncates the value in a2 and stores it into the a1 and tmp_reg
	sse_cvttsd2si_reg_reg(jit->ip, a1, a2);
	sse_cvtsi2sd_reg_reg(jit->ip, tmp_reg, a1);

	if (floor) {
		// if a2 < tmp_reg, it substracts 1 (using the carry flag)
		sse_comisd_reg_reg(jit->ip, a2, tmp_reg);
		common86_alu_reg_imm(jit->ip, X86_SBB, a1, 0);
	} else { // ceil
		// if tmp_reg < a2, it adds 1 (using the carry flag)
		sse_comisd_reg_reg(jit->ip, tmp_reg, a2);
		common86_alu_reg_imm(jit->ip, X86_ADC, a1, 0);
	}

	// returns values back
	sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
	sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, tmp_reg, tmp_reg, 1);
}


/**
 * Emits SSE instructions representing FST operation
 */
static void emit_sse_fst_op(struct jit * jit, jit_op * op, jit_value a1, jit_value a2)
{
	if (op->arg_size == sizeof(float)) {
		// the value has to be converted from double to float
		// we are using the given XMM register for this.
		// however, if the value in the register is supposed to be used later,
		// i.e., it's `live', we store it for a while into the upper half of the XMM register
		int live = jitset_get(op->live_out, op->arg[1]);
		if (live) sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);
		sse_cvtsd2ss_reg_reg(jit->ip, a2, a2);

		if (IS_IMM(op)) sse_movss_mem_reg(jit->ip, a1, a2);
		else sse_movss_membase_reg(jit->ip, a1, 0, a2);
 
		if (live) sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);

	} else {
		if (IS_IMM(op)) sse_movlpd_mem_reg(jit->ip, a1, a2);
		else sse_movlpd_membase_xreg(jit->ip, a2, a1, 0);
	}
}

static void emit_sse_fld_op(struct jit * jit, jit_op * op, jit_value a1, jit_value a2)
{
	if (op->arg_size == sizeof(float)) {
		if (IS_IMM(op)) sse_cvtss2sd_reg_mem(jit->ip, a1, a2);
		else sse_cvtss2sd_reg_membase(jit->ip, a1, a2, 0);
	} else {
		if (IS_IMM(op)) sse_movsd_reg_mem(jit->ip, a1, a2);
		else sse_movlpd_xreg_membase(jit->ip, a1, a2, 0);
	}
}

static void emit_sse_fldx_op(struct jit * jit, jit_op * op, jit_value a1, jit_value a2, jit_value a3)
{
	if (op->arg_size == sizeof(float)) {
		if (IS_IMM(op)) sse_cvtss2sd_reg_membase(jit->ip, a1, a2, a3);
		else sse_cvtss2sd_reg_memindex(jit->ip, a1, a2, 0, a3, 0);
	} else {
		if (IS_IMM(op)) sse_movlpd_xreg_membase(jit->ip, a1, a2, a3);
		else sse_movlpd_xreg_memindex(jit->ip, a1, a2, 0, a3, 0);
	}
}

/**
 * Emits SSE instructions representing FSTX operation
 */
static void emit_sse_fstx_op(struct jit * jit, jit_op * op, jit_value a1, jit_value a2, jit_value a3)
{
	if (op->arg_size == sizeof(float)) {
		// the value has to be converted from double to float
		// we are using the given XMM register for this.
		// however, if the value in the register is supposed to be used later,
		// i.e., it's `live', we store it for a while into the upper half of the XMM register

		int live = jitset_get(op->live_out, op->arg[2]);
		if (live) sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a3, a3, 0);
		sse_cvtsd2ss_reg_reg(jit->ip, a3, a3);

		if (IS_IMM(op)) sse_movss_membase_reg(jit->ip, a2, a1, a3);
		else sse_movss_memindex_xreg(jit->ip, a1, 0, a2, 0, a3);

		if (live) sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a3, a3, 1);

	} else {
		if (IS_IMM(op)) sse_movlpd_membase_xreg(jit->ip, a3, a2, a1);
		else sse_movlpd_memindex_xreg(jit->ip, a1, 0, a2, 0, a3);
	}
}

//
// 
// Optimizations
//
//
void jit_optimize_st_ops(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if ((GET_OP(op) == JIT_ST)
		&& (op->prev)
		&& (op->prev->code == (JIT_MOV | IMM))
		&& (op->arg[1] == op->prev->arg[0])
		&& (!jitset_get(op->live_out, op->arg[1])))
		{
			if (!IS_IMM(op)) {
				op->code = JIT_X86_STI | REG;
				op->spec = SPEC(REG, IMM, NO);
			} else {
				op->code = JIT_X86_STI | IMM;
				op->spec = SPEC(IMM, IMM, NO);
			}
			op->arg[1] = op->prev->arg[1];
			op->prev->code = JIT_NOP;
			op->prev->spec = SPEC(NO, NO, NO);
		}
		
		if ((GET_OP(op) == JIT_STX)
		&& (op->prev)
		&& (op->prev->code == (JIT_MOV | IMM))
		&& (op->arg[2] == op->prev->arg[0])
		&& (!jitset_get(op->live_out, op->arg[2])))
		{
			if (!IS_IMM(op)) {
				op->code = JIT_X86_STXI | REG;
				op->spec = SPEC(REG, REG, IMM);
			} else {
				op->code = JIT_X86_STXI | IMM;
				op->spec = SPEC(IMM, REG, IMM);
			}
			op->arg[2] = op->prev->arg[1];
			op->prev->code = JIT_NOP;
			op->prev->spec = SPEC(NO, NO, NO);
		}

	}
}
