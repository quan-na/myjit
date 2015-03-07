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

//
//
// Provides aliases for SSE operation accordingly to the hardware platform
//
//

//
// I386 support
//

#ifdef JIT_ARCH_I386
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
#define sse_mov_reg_safeimm(jit, xop, reg, imm)		x86_movsd_reg_mem(jit->ip, reg, imm)
#define sse_alu_sd_reg_safeimm(jit, xop, op, reg, imm) 	x86_sse_alu_sd_reg_mem(jit->ip, op, reg, imm)

#define sse_alu_pd_reg_reg(ip, op, r1, r2) 		x86_sse_alu_pd_reg_reg(ip, op, r1, r2)
#define sse_alu_pd_reg_reg_imm(ip, op, r1, r2, imm) 	x86_sse_alu_pd_reg_reg_imm(ip, op, r1, r2, imm)
#define sse_alu_pd_reg_safeimm(jit, xop, op, reg, imm) 	x86_sse_alu_pd_reg_mem(jit->ip, op, reg, imm)

#define sse_comisd_reg_reg(ip, r1, r2)			x86_sse_alu_pd_reg_reg(ip, X86_SSE_COMI, r1, r2)

#define sse_cvttsd2si_reg_reg(ip, r1, r2) 		x86_cvttsd2si(ip, r1, r2)
#define sse_cvtsi2sd_reg_reg(ip, r1, r2) 		x86_cvtsi2sd(ip, r1, r2)
#define sse_cvtsd2ss_reg_reg(ip, r1, r2)		x86_cvtsd2ss(ip, r1, r2)
#define sse_cvtss2sd_reg_reg(ip, r1, r2) 		x86_cvtss2sd(ip, r1, r2)
#define sse_cvtss2sd_reg_mem(ip, r1, mem) 		x86_cvtss2sd_reg_mem(ip, r1, mem)
#define sse_cvtss2sd_reg_membase(ip, r1, basereg, disp)	x86_cvtss2sd_reg_membase(ip, r1, basereg, disp)
#define sse_cvtss2sd_reg_memindex(ip, r1, basereg, disp, indexreg, shift)	x86_cvtss2sd_reg_memindex(ip, r1, basereg, disp, indexreg, shift)

#endif

//
// AMD64 support
//

#ifdef JIT_ARCH_AMD64

#define sse_alu_sd_reg_reg(ip,op,r1,r2) amd64_sse_alu_sd_reg_reg(ip,op,r1,r2)
#define sse_alu_pd_reg_reg(ip,op,r1,r2) amd64_sse_alu_pd_reg_reg(ip,op,r1,r2)
#define sse_movsd_reg_reg(ip,r1,r2) amd64_sse_movsd_reg_reg(ip,r1,r2)
#define sse_movsd_reg_mem(ip, r1, mem) amd64_movsd_reg_mem(ip,r1,mem)
#define sse_movsd_reg_membase(ip, r1, basereg, disp) 	amd64_movsd_reg_membase(ip, r1, basereg, disp)
#define sse_movlpd_membase_xreg(ip,dreg,basereg,disp) 	amd64_sse_movlpd_membase_xreg(ip,dreg,basereg,disp)
#define sse_movlpd_xreg_membase(ip,dreg,basereg,disp) 	amd64_sse_movlpd_xreg_membase(ip,dreg,basereg,disp)
#define sse_movlpd_memindex_xreg(ip, basereg, disp, indexreg, shift, reg)  amd64_sse_movlpd_memindex_xreg(ip, basereg, disp, indexreg, shift, reg)


#define sse_movlpd_mem_reg(ip, mem, reg) 			amd64_movlpd_mem_reg(ip, mem, reg)
#define sse_movlpd_xreg_memindex(ip, dreg, basereg, disp, indexreg, shift) 	amd64_sse_movlpd_xreg_memindex(ip, dreg, basereg, disp, indexreg, shift)

#define sse_movss_membase_reg(ip, basereg, disp, reg)		amd64_movss_membase_reg(ip, basereg, disp, reg)
#define sse_movss_mem_reg(ip, mem, reg)				amd64_movss_mem_reg(ip, mem, reg)
#define sse_movss_memindex_xreg(ip, basereg, disp, indexreg, shift, reg)  amd64_sse_movss_memindex_xreg(ip, basereg,disp, indexreg,shift, reg)

#define sse_comisd_reg_reg(ip, r1, r2)                  amd64_sse_comisd_reg_reg(ip, r1, r2)
#define sse_alu_pd_reg_reg_imm(ip, op, r1, r2, imm)     amd64_sse_alu_pd_reg_reg_imm(ip, op, r1, r2, imm)

#define sse_cvttsd2si_reg_reg(ip, r1, r2) 		amd64_sse_cvttsd2si_reg_reg(ip, r1, r2)
#define sse_cvtsi2sd_reg_reg(ip, r1, r2) 		amd64_sse_cvtsi2sd_reg_reg(ip, r1, r2)
#define sse_cvtsd2ss_reg_reg(ip, r1, r2) 		amd64_sse_cvtsd2ss_reg_reg(ip, r1, r2)
#define sse_cvtss2sd_reg_reg(ip, r1, r2) 		amd64_sse_cvtss2sd_reg_reg(ip, r1, r2)
#define sse_cvtss2sd_reg_mem(ip, r1, mem) 		amd64_sse_cvtss2sd_reg_mem(ip, r1, mem)
#define sse_cvtss2sd_reg_membase(ip, r1, basereg, disp)	amd64_sse_cvtss2sd_reg_membase(ip, r1, basereg, disp)
#define sse_cvtss2sd_reg_memindex(ip, r1, basereg, disp, indexreg, shift)	amd64_sse_cvtss2sd_reg_memindex(ip, r1, basereg, disp, indexreg, shift)


/**
 * This function emits SSE code which assigns value value which resides in the memory
 * in to the XMM register If the value is not addressable with 32bit address, unused 
 * register from the register pool is used to access this value.
 */
static void sse_mov_reg_safeimm(struct jit * jit, jit_op * op, jit_value reg, double * imm)
{
	if (((jit_unsigned_value)imm) > 0xffffffffUL) {
		jit_hw_reg * r = jit_get_unused_reg(jit->reg_al, op, 0);
		if (r) {
			amd64_mov_reg_imm(jit->ip, r->id, (jit_value)imm);
			amd64_movsd_reg_membase(jit->ip, reg, r->id, 0);
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
static void sse_alu_pd_reg_safeimm(struct jit * jit, jit_op * op, int op_id, int reg, double * imm)
{
	if (((jit_unsigned_value)imm) > 0xffffffffUL) {
		jit_hw_reg * r = jit_get_unused_reg(jit->reg_al, op, 0);
		if (r) {
			amd64_mov_reg_imm(jit->ip, r->id, (long)imm);
			amd64_sse_alu_pd_reg_membase(jit->ip, op_id, reg, r->id, 0);
		} else {
			amd64_push_reg(jit->ip, AMD64_RAX);
			amd64_mov_reg_imm(jit->ip, AMD64_RAX, (long)imm);
			amd64_sse_alu_pd_reg_membase(jit->ip, op_id, reg, AMD64_RAX, 0);
			amd64_pop_reg(jit->ip, AMD64_RAX);
		}
	} else {
		amd64_sse_alu_pd_reg_mem(jit->ip, op_id, reg, (long)imm);
	}
}

/**
 * This function emits SSE code involving arithmetic (single) operation and immediate
 * value which resides in the memory. If the value is not addressable with 32bit
 * address, unused register from the register pool is used to access
 * this value.
 */
static void sse_alu_sd_reg_safeimm(struct jit * jit, jit_op * op, int op_id, int reg, double * imm)
{
	if (((jit_unsigned_value)imm) > 0xffffffffUL) {
		jit_hw_reg * r = jit_get_unused_reg(jit->reg_al, op, 0);
		if (r) {
			amd64_mov_reg_imm(jit->ip, r->id, (long)imm);
			amd64_sse_alu_sd_reg_membase(jit->ip, op_id, reg, r->id, 0);
		} else {
			amd64_push_reg(jit->ip, AMD64_RAX);
			amd64_mov_reg_imm(jit->ip, AMD64_RAX, (long)imm);
			amd64_sse_alu_sd_reg_membase(jit->ip, op_id, reg, AMD64_RAX, 0);
			amd64_pop_reg(jit->ip, AMD64_RAX);
		}
	} else {
		amd64_sse_alu_sd_reg_mem(jit->ip, op_id, reg, (long)imm);
	}
}

#else
#endif

//
//
// Implementation of functions emitting high-level floating point operations
//
//

static unsigned char * emit_sse_get_sign_mask()
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

static void emit_sse_alu_op(struct jit * jit, jit_op * op, int sse_op)
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

static void emit_sse_change_sign(struct jit * jit, jit_op * op, int reg)
{
	sse_alu_pd_reg_safeimm(jit, op, X86_SSE_XOR, reg, (double *)emit_sse_get_sign_mask());
}

static void emit_sse_sub_op(struct jit * jit, jit_op * op, long a1, long a2, long a3)
{
	if (a1 == a2) {
		sse_alu_sd_reg_reg(jit->ip, X86_SSE_SUB, a1, a3);
	} else if (a1 == a3) {
		sse_alu_sd_reg_reg(jit->ip, X86_SSE_SUB, a1, a2);
		emit_sse_change_sign(jit, op, a1);
	} else {
		sse_movsd_reg_reg(jit->ip, a1, a2);
		sse_alu_sd_reg_reg(jit->ip, X86_SSE_SUB, a1, a3);
	}
}

static void emit_sse_div_op(struct jit * jit, long a1, long a2, long a3)
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

static void emit_sse_neg_op(struct jit * jit, jit_op * op, long a1, long a2)
{
	if (a1 != a2) sse_movsd_reg_reg(jit->ip, a1, a2); 
	emit_sse_change_sign(jit, op, a1);
}

static void emit_sse_branch(struct jit * jit, jit_op * op, long a1, long a2, long a3, int x86_cond)
{
        sse_alu_pd_reg_reg(jit->ip, X86_SSE_COMI, a2, a3);
        op->patch_addr = JIT_BUFFER_OFFSET(jit);
        x86_branch_disp32(jit->ip, x86_cond, JIT_GET_ADDR(jit, a1), 0);
}

static void emit_sse_round(struct jit * jit, jit_op * op, jit_value a1, jit_value a2)
{
	static const double x0 = 0.0;
	static const double x05 = 0.5;

	// creates a copy of the a2 and tmp_reg into high bits of a2 and tmp_reg
	sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);

	sse_alu_pd_reg_safeimm(jit, op, X86_SSE_COMI, a2, (double *)&x0);

	unsigned char * branch1 = jit->ip;
	common86_branch_disp(jit->ip, X86_CC_LT, 0, 0);

	sse_alu_sd_reg_safeimm(jit, op, X86_SSE_ADD, a2, (double *)&x05);

	unsigned char * branch2 = jit->ip;
	common86_jump_disp(jit->ip, 0);

	common86_patch(branch1, jit->ip);

	sse_alu_sd_reg_safeimm(jit, op, X86_SSE_SUB, a2, (double *)&x05);
	common86_patch(branch2, jit->ip);

	sse_cvttsd2si_reg_reg(jit->ip, a1, a2);

	// returns values back
	sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
}

static void emit_sse_floor(struct jit * jit, jit_value a1, jit_value a2, int floor)
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
		int live = jit_set_get(op->live_out, op->arg[1]);
		if (live) sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);
		sse_cvtsd2ss_reg_reg(jit->ip, a2, a2);

		if (IS_IMM(op)) sse_movss_mem_reg(jit->ip, a1, a2);
		else sse_movss_membase_reg(jit->ip, a1, 0, a2);
 
		if (live) sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);

	} else {
		if (IS_IMM(op)) sse_movlpd_mem_reg(jit->ip, a2, a1);
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

		int live = jit_set_get(op->live_out, op->arg[2]);
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
