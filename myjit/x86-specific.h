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

#include "x86-codegen.h"

#define __JIT_GET_ADDR(jit, imm) (!jit_is_label(jit, (void *)(imm)) ? (imm) :  \
		(((long)jit->buf + ((jit_label *)(imm))->pos - (long)jit->ip)))
#define __GET_REG_POS(jit, r) (- ((JIT_REG(r).id + 1) * REG_SIZE) - jit_current_func_info(jit)->allocai_mem)
#define __GET_FPREG_POS(jit, r) (- jit_current_func_info(jit)->gp_reg_count * REG_SIZE - (JIT_REG(r).id + 1) * sizeof(jit_float) - jit_current_func_info(jit)->allocai_mem)
#define __PATCH_ADDR(jit)	((long)jit->ip - (long)jit->buf)

#include "common86-codegen.h"
#include "x86-common-stuff.c"

static inline int jit_allocai(struct jit * jit, int size)
{
	int real_size = (size + 3) & 0xfffffffc; // 4-bytes aligned
	jit_add_op(jit, JIT_ALLOCA | IMM, SPEC(IMM, NO, NO), (long)real_size, 0, 0, 0);
	jit_current_func_info(jit)->allocai_mem += real_size;	
	return -(jit_current_func_info(jit)->allocai_mem);
}

void jit_init_arg_params(struct jit * jit, struct jit_func_info * info, int p, int * phys_reg)
{
	struct jit_inp_arg * a = &(info->args[p]);
	a->passed_by_reg = 0;

	if (p == 0) a->location.stack_pos = 8;
	else {
		struct jit_inp_arg * prev_a = &(info->args[p - 1]);
		int stack_shift = (prev_a->size + 3) & 0xfffffffc; // 4-bytes aligned
		a->location.stack_pos = prev_a->location.stack_pos + stack_shift;
	}

	a->spill_pos = a->location.stack_pos; 
	a->overflow = 0;
}

static inline void __push_reg(struct jit * jit, struct __hw_reg * r)
{
	if (!r->fp) x86_push_reg(jit->ip, r->id);
	else {
		x86_alu_reg_imm(jit->ip, X86_SUB, X86_ESP, 8);
		x86_movlpd_membase_xreg(jit->ip, r->id, X86_ESP, 0);
	}
}

static inline void __pop_reg(struct jit * jit, struct __hw_reg * r)
{
	if (!r->fp) x86_pop_reg(jit->ip, r->id);
	else {
		x86_movlpd_xreg_membase(jit->ip, r->id, X86_ESP, 0);
		x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, 8);
	}
}

/*
static void __shift_op(struct jit * jit, struct jit_op * op, int shift_op, int imm)
{
	if (imm) { 
		if (op->r_arg[0] != op->r_arg[1]) x86_mov_reg_reg(jit->ip, op->r_arg[0], op->r_arg[1], REG_SIZE); 
		x86_shift_reg_imm(jit->ip, shift_op, op->r_arg[0], op->r_arg[2]);
	} else {
		int destreg = op->r_arg[0];
		int valreg = op->r_arg[1];
		int shiftreg = op->r_arg[2];
		int ecx_pathology = 0; // shifting content in the ECX register

		int ecx_in_use = jit_reg_in_use(op, X86_ECX, 0);
		int edx_in_use = jit_reg_in_use(op, X86_EDX, 0);

		if (destreg == X86_ECX) {
			ecx_pathology = 1;
			if (edx_in_use) x86_push_reg(jit->ip, X86_EDX);
			destreg = X86_EDX;
		}

		if (shiftreg != X86_ECX) {
			if (ecx_in_use) x86_push_reg(jit->ip, X86_ECX); 
			x86_mov_reg_reg(jit->ip, X86_ECX, shiftreg, REG_SIZE);
		}
		if (destreg != valreg) x86_mov_reg_reg(jit->ip, destreg, valreg, REG_SIZE); 
		x86_shift_reg(jit->ip, shift_op, destreg);
		if (ecx_pathology) {
			x86_mov_reg_reg(jit->ip, X86_ECX, X86_EDX, REG_SIZE);
			if ((shiftreg != X86_ECX) && ecx_in_use) x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, 4);
			if (edx_in_use) x86_pop_reg(jit->ip, X86_EDX);
		} else {
			if ((shiftreg != X86_ECX) && ecx_in_use) x86_pop_reg(jit->ip, X86_ECX);
		}
	}
}
*/
static void __cond_op(struct jit * jit, struct jit_op * op, int x86_cond, int imm, int sign)
{
	if (imm) x86_cmp_reg_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
	else x86_cmp_reg_reg(jit->ip, op->r_arg[1], op->r_arg[2]);
	if ((op->r_arg[0] != X86_ESI) && (op->r_arg[0] != X86_EDI)) {
		x86_mov_reg_imm(jit->ip, op->r_arg[0], 0);
		x86_set_reg(jit->ip, x86_cond, op->r_arg[0], sign);
	} else {
		x86_xchg_reg_reg(jit->ip, X86_EAX, op->r_arg[0], REG_SIZE);
		x86_mov_reg_imm(jit->ip, X86_EAX, 0);
		x86_set_reg(jit->ip, x86_cond, X86_EAX, sign);
		x86_xchg_reg_reg(jit->ip, X86_EAX, op->r_arg[0], REG_SIZE);
	}
}

static void __branch_op(struct jit * jit, struct jit_op * op, int x86_cond, int imm, int sign)
{
	if (imm) x86_cmp_reg_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
	else x86_cmp_reg_reg(jit->ip, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	x86_branch_disp32(jit->ip, x86_cond, __JIT_GET_ADDR(jit, op->r_arg[0]), sign);
}

static void __branch_mask_op(struct jit * jit, struct jit_op * op, int x86_cond, int imm)
{
	if (imm) x86_test_reg_imm(jit->ip, op->r_arg[1], op->r_arg[2]);
	else x86_test_reg_reg(jit->ip, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit);

	x86_branch_disp32(jit->ip, x86_cond, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}

static void __branch_overflow_op(struct jit * jit, struct jit_op * op, int alu_op, int imm)
{
	if (imm) x86_alu_reg_imm(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);
	else x86_alu_reg_reg(jit->ip, alu_op, op->r_arg[1], op->r_arg[2]);

	op->patch_addr = __PATCH_ADDR(jit); 

	x86_branch_disp32(jit->ip, X86_CC_O, __JIT_GET_ADDR(jit, op->r_arg[0]), 0);
}

/* determines whether the argument value was spilled out or not,
 * if the register is associated with the hardware register
 * it is returned through the reg argument
 */
static inline int __is_spilled(struct jit * jit, int arg_id, int * reg)
{
	struct __hw_reg * hreg = rmap_get(jit->prepared_args.op->regmap, arg_id);
	if (hreg) {
		*reg = hreg->id;
		return 0;
	} else return 1;
}

static int __configure_args(struct jit * jit)
{
	int stack_correction = 0;
	struct jit_out_arg * args = jit->prepared_args.args;

#ifdef __APPLE__
	int alignment = jit->push_count * 4 + jit->prepared_args.stack_size;

	while (((alignment + stack_correction) % 16) != 8)
		stack_correction += 4;

	if (stack_correction)
                x86_alu_reg_imm(jit->ip, X86_SUB, X86_ESP, stack_correction);
#endif

	int sreg;
	for (int i = jit->prepared_args.ready - 1; i >= 0; i--) {
		if (!args[i].isfp) { // integers
			if (!args[i].isreg) x86_push_imm(jit->ip, args[i].value.generic);
			else {
				if (__is_spilled(jit, args[i].value.generic, &sreg))
					x86_push_membase(jit->ip, X86_EBP, __GET_REG_POS(jit, args[i].value.generic));
				else x86_push_reg(jit->ip, sreg);
			}
			continue;
		}
		//
		// floats (double)
		// 
		if (args[i].size == sizeof(double)) {
			if (args[i].isreg) { // doubles
				if (__is_spilled(jit, args[i].value.generic, &sreg)) {
					int pos = __GET_FPREG_POS(jit, args[i].value.generic);
					x86_push_membase(jit->ip, X86_EBP, pos + 4);
					x86_push_membase(jit->ip, X86_EBP, pos);
				} else {
					// ``PUSH sreg'' for XMM regs
					x86_alu_reg_imm(jit->ip, X86_SUB, X86_ESP, 8);
					x86_movlpd_membase_xreg(jit->ip, sreg, X86_ESP, 0);
				}
			} else {
				int fl_val[2];
				memcpy(fl_val, &(args[i].value.fp), sizeof(double));
				x86_push_imm(jit->ip, fl_val[1]);
				x86_push_imm(jit->ip, fl_val[0]);
			}
			continue;
		}
		//
		// single prec. floats
		//
		if (args[i].isreg) { 
			if (__is_spilled(jit, args[i].value.generic, &sreg)) {
				int pos = __GET_FPREG_POS(jit, args[i].value.generic);
				x86_fld_membase(jit->ip, X86_EBP, pos, 1); 
				x86_alu_reg_imm(jit->ip, X86_SUB, X86_ESP, 4);
				x86_fst_membase(jit->ip, X86_ESP, 0, 0, 1);
			} else {
				// pushes the value beyond the top of the stack
				x86_movlpd_membase_xreg(jit->ip, sreg, X86_ESP, -8); 
				x86_fld_membase(jit->ip, X86_ESP, -8, 1); 
				x86_alu_reg_imm(jit->ip, X86_SUB, X86_ESP, 4);
				x86_fst_membase(jit->ip, X86_ESP, 0, 0, 1);
			}
		} else {
			int fl_val;
			float b = (float)args[i].value.fp;
			memcpy(&fl_val, &b, sizeof(float));
			x86_push_imm(jit->ip, fl_val);
		}
	}
	return stack_correction;
}

static void __funcall(struct jit * jit, struct jit_op * op, int imm)
{
	int stack_correction = __configure_args(jit);

	if (!imm) {
		x86_call_reg(jit->ip, op->r_arg[0]);
	} else {
		op->patch_addr = __PATCH_ADDR(jit); 
		x86_call_imm(jit->ip, __JIT_GET_ADDR(jit, op->r_arg[0]) - 4); /* 4: magic constant */
	}
	
	if (jit->prepared_args.stack_size + stack_correction) 
		x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, jit->prepared_args.stack_size + stack_correction);
	JIT_FREE(jit->prepared_args.args);

	jit->push_count -= __pop_caller_saved_regs(jit, op);
}

static void __sse_change_sign(struct jit * jit, int reg)
{
	x86_sse_alu_pd_reg_mem(jit->ip, X86_SSE_XOR, reg, __sse_get_sign_mask());
}

static void __sse_round(struct jit * jit, jit_value a1, jit_value a2)
{
	static const double x0 = 0.0;
	static const double x05 = 0.5;

	// creates a copy of the a2 and tmp_reg into high bits of a2 and tmp_reg
	x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);

	x86_sse_alu_pd_reg_mem(jit->ip, X86_SSE_COMI, a2, &x0);
	
	unsigned char * branch1 = jit->ip;
	x86_branch_disp(jit->ip, X86_CC_LT, 0, 0);

	x86_sse_alu_sd_reg_mem(jit->ip, X86_SSE_ADD, a2, &x05);

	unsigned char * branch2 = jit->ip;
	x86_jump_disp(jit->ip, 0);

	x86_patch(branch1, jit->ip);

	x86_sse_alu_sd_reg_mem(jit->ip, X86_SSE_SUB, a2, &x05);
	x86_patch(branch2, jit->ip);

	x86_cvttsd2si(jit->ip, a1, a2);

	// returns values back
	x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
}

static void __sse_floor(struct jit * jit, jit_value a1, jit_value a2, int floor)
{
	int tmp_reg = (a2 == X86_XMM7 ? X86_XMM0 : X86_XMM7);

	// creates a copy of the a2 and tmp_reg into high bits of a2 and tmp_reg
	x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);
	// TODO: test if the register is in use or not
	x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, tmp_reg, tmp_reg, 0);

	// truncates the value in a2 and stores it into the a1 and tmp_reg
	x86_cvttsd2si(jit->ip, a1, a2);
	x86_cvtsi2sd(jit->ip, tmp_reg, a1);

	if (floor) {
		// if a2 < tmp_reg, it substracts 1 (using the carry flag)
		x86_sse_alu_pd_reg_reg(jit->ip, X86_SSE_COMI, a2, tmp_reg);
		x86_alu_reg_imm(jit->ip, X86_SBB, a1, 0);
	} else { // ceil
		// if tmp_reg < a2, it adds 1 (using the carry flag)
		x86_sse_alu_pd_reg_reg(jit->ip, X86_SSE_COMI, tmp_reg, a2);
		x86_alu_reg_imm(jit->ip, X86_ADC, a1, 0);
	}

	// returns values back
	x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
	x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, tmp_reg, tmp_reg, 1);
}

void jit_patch_external_calls(struct jit * jit)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if ((op->code == (JIT_CALL | IMM)) && (!jit_is_label(jit, (void *)op->arg[0])))
			x86_patch(jit->buf + (long)op->patch_addr, (unsigned char *)op->arg[0]);
		if (GET_OP(op) == JIT_MSG)
			x86_patch(jit->buf + (long)op->patch_addr, (unsigned char *)printf);
	}
}

void jit_gen_op(struct jit * jit, struct jit_op * op)
{
	jit_value a1 = op->r_arg[0];
	jit_value a2 = op->r_arg[1];
	jit_value a3 = op->r_arg[2];

	int found = 1;
	
	switch (GET_OP(op)) {
		case JIT_ADD: 
			if ((a1 != a2) && (a1 != a3)) {
				if (IS_IMM(op)) x86_lea_membase(jit->ip, a1, a2, a3);
				else x86_lea_memindex(jit->ip, a1, a2, 0, a3, 0);
			} else __alu_op(jit, op, X86_ADD, IS_IMM(op)); 
			break;

		case JIT_ADDC: __alu_op(jit, op, X86_ADD, IS_IMM(op)); break;
		case JIT_ADDX: __alu_op(jit, op, X86_ADC, IS_IMM(op)); break;
		case JIT_SUB: __sub_op(jit, op, IS_IMM(op)); break;
		case JIT_SUBC: __subx_op(jit, op, X86_SUB, IS_IMM(op)); break; 
		case JIT_SUBX: __subx_op(jit, op, X86_SBB, IS_IMM(op)); break; 
		case JIT_RSB: __rsb_op(jit, op, IS_IMM(op)); break; 
		case JIT_NEG: if (a1 != a2) x86_mov_reg_reg(jit->ip, a1, a2, REG_SIZE);
			      x86_neg_reg(jit->ip, a1); 
			      break;
		case JIT_OR: __alu_op(jit, op, X86_OR, IS_IMM(op)); break; 
		case JIT_XOR: __alu_op(jit, op, X86_XOR, IS_IMM(op)); break; 
		case JIT_AND: __alu_op(jit, op, X86_AND, IS_IMM(op)); break; 
		case JIT_NOT: if (a1 != a2) x86_mov_reg_reg(jit->ip, a1, a2, REG_SIZE);
			      x86_not_reg(jit->ip, a1);
			      break;
		case JIT_LSH: __shift_op(jit, op, X86_SHL, IS_IMM(op)); break;
		case JIT_RSH: __shift_op(jit, op, IS_SIGNED(op) ? X86_SAR : X86_SHR, IS_IMM(op)); break;

		case JIT_LT: __cond_op(jit, op, X86_CC_LT, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_LE: __cond_op(jit, op, X86_CC_LE, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_GT: __cond_op(jit, op, X86_CC_GT, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_GE: __cond_op(jit, op, X86_CC_GE, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_EQ: __cond_op(jit, op, X86_CC_EQ, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_NE: __cond_op(jit, op, X86_CC_NE, IS_IMM(op), IS_SIGNED(op)); break;

		case JIT_BLT: __branch_op(jit, op, X86_CC_LT, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_BLE: __branch_op(jit, op, X86_CC_LE, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_BGT: __branch_op(jit, op, X86_CC_GT, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_BGE: __branch_op(jit, op, X86_CC_GE, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_BEQ: __branch_op(jit, op, X86_CC_EQ, IS_IMM(op), IS_SIGNED(op)); break;
		case JIT_BNE: __branch_op(jit, op, X86_CC_NE, IS_IMM(op), IS_SIGNED(op)); break;

		case JIT_BMS: __branch_mask_op(jit, op, X86_CC_NZ, IS_IMM(op)); break;
		case JIT_BMC: __branch_mask_op(jit, op, X86_CC_Z, IS_IMM(op)); break;

		case JIT_BOADD: __branch_overflow_op(jit, op, X86_ADD, IS_IMM(op)); break;
		case JIT_BOSUB: __branch_overflow_op(jit, op, X86_SUB, IS_IMM(op)); break;

		case JIT_MUL: __mul(jit, op, IS_IMM(op), IS_SIGNED(op), 0); break;
		case JIT_HMUL: __mul(jit, op, IS_IMM(op), IS_SIGNED(op), 1); break;
		case JIT_DIV: __div(jit, op, IS_IMM(op), IS_SIGNED(op), 0); break;
		case JIT_MOD: __div(jit, op, IS_IMM(op), IS_SIGNED(op), 1); break;

		case JIT_CALL: __funcall(jit, op, IS_IMM(op)); break;

		//case JIT_PATCH: x86_patch(((struct jit_op *)a1)->patch_addr, jit->ip); break;
		case JIT_PATCH: do {
					long pa = ((struct jit_op *)a1)->patch_addr;
					x86_patch(jit->buf + pa, jit->ip);
				} while (0);
				break;
		case JIT_JMP:
			op->patch_addr = __PATCH_ADDR(jit); 
			if (op->code & REG) x86_jump_reg(jit->ip, a1);
			//else x86_jump_disp(jit->ip, __JIT_GET_ADDR(jit, a1));
			else x86_jump_disp32(jit->ip, __JIT_GET_ADDR(jit, a1));
			break;

		case JIT_RET:
			if (!IS_IMM(op) && (a1 != X86_EAX)) x86_mov_reg_reg(jit->ip, X86_EAX, a1, 4);
			if (IS_IMM(op)) x86_mov_reg_imm(jit->ip, X86_EAX, a1);
			__pop_callee_saved_regs(jit);
			x86_mov_reg_reg(jit->ip, X86_ESP, X86_EBP, 4);
			x86_pop_reg(jit->ip, X86_EBP);
			x86_ret(jit->ip);
			break;

		case JIT_PUTARG: __put_arg(jit, op); break;
		case JIT_FPUTARG: __fput_arg(jit, op); break;

		case JIT_GETARG:
			do {
				struct jit_inp_arg * arg = &(jit_current_func_info(jit)->args[a2]);
				int stack_pos = arg->location.stack_pos;
				if (arg->type != JIT_FLOAT_NUM) {
					if (arg->size == REG_SIZE) x86_mov_reg_membase(jit->ip, a1, X86_EBP, stack_pos, REG_SIZE); 
					else if (arg->type == JIT_SIGNED_NUM) x86_movsx_reg_membase(jit->ip, a1, X86_EBP, stack_pos, arg->size); 
					else x86_movzx_reg_membase(jit->ip, a1, X86_EBP, stack_pos, arg->size); 
				} else {
					if (arg->size == sizeof(double)) x86_movlpd_xreg_membase(jit->ip, a1, X86_EBP, stack_pos);
					else if (arg->size == sizeof(float))
						x86_cvtss2sd_reg_membase(jit->ip, a1, X86_EBP, stack_pos);
					else assert(0);
				}

			} while (0);
			break;
			
		case JIT_MSG: x86_pushad(jit->ip);
			      if (!IS_IMM(op)) x86_push_reg(jit->ip, a2);
			      x86_push_imm(jit->ip, a1);
			      op->patch_addr = __PATCH_ADDR(jit); 
			      x86_call_imm(jit->ip, printf);
			      if (IS_IMM(op)) x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, 4);
			      else x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, 8);
			      x86_popad(jit->ip);
			      break; 
		case JIT_ALLOCA: break;
		default: found = 0;
	}

	if (found) return;


	switch (op->code) {
		case (JIT_MOV | REG): if (a1 != a2) x86_mov_reg_reg(jit->ip, a1, a2, REG_SIZE); break;
		case (JIT_MOV | IMM):
			if (a2 == 0) x86_alu_reg_reg(jit->ip, X86_XOR, a1, a1);
			else x86_mov_reg_imm(jit->ip, a1, a2); 
			break;

		case JIT_PREPARE: __prepare_call(jit, op, a1 + a2); 
				  jit->push_count += __push_caller_saved_regs(jit, op);
				  break;
		case JIT_PROLOG:
			do {
				jit->current_func = op;
				struct jit_func_info * info = jit_current_func_info(jit);
				while ((long)jit->ip % 8) 
					x86_nop(jit->ip);

				op->patch_addr = __PATCH_ADDR(jit);
				x86_push_reg(jit->ip, X86_EBP);
				x86_mov_reg_reg(jit->ip, X86_EBP, X86_ESP, 4);
				int stack_mem = info->allocai_mem + info->gp_reg_count * REG_SIZE + info->fp_reg_count * sizeof(double);
				
#ifdef __APPLE__
				stack_mem = (stack_mem + 15) & 0xfffffff0; // 16-bytes aligned
#endif
				x86_alu_reg_imm(jit->ip, X86_SUB, X86_ESP, stack_mem);
				jit->push_count = __push_callee_saved_regs(jit, op);
			} while(0);
			break;

		case JIT_DECL_ARG: break;

		case JIT_RETVAL: // reg. allocator takes care of the proper register assignment 
			//if (a1 != X86_EAX) x86_mov_reg_reg(jit->ip, a1, X86_EAX, REG_SIZE);
			break;

		case JIT_LABEL: ((jit_label *)a1)->pos = __PATCH_ADDR(jit); break;

		case (JIT_LD | IMM | SIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_mem(jit->ip, a1, a2, op->arg_size);
			else x86_movsx_reg_mem(jit->ip, a1, a2, op->arg_size);
			break;

		case (JIT_LD | IMM | UNSIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_mem(jit->ip, a1, a2, op->arg_size);
			else x86_movzx_reg_mem(jit->ip, a1, a2, op->arg_size);
			break;

		case (JIT_LD | REG | SIGNED):
			if (op->arg_size == REG_SIZE) x86_mov_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			else x86_movsx_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			break;

		case (JIT_LD | REG | UNSIGNED):
			if (op->arg_size == REG_SIZE) x86_mov_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			else  x86_movzx_reg_membase(jit->ip, a1, a2, 0, op->arg_size); 
			break;

		case (JIT_LDX | IMM | SIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			else x86_movsx_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			break;

		case (JIT_LDX | IMM | UNSIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			else x86_movzx_reg_membase(jit->ip, a1, a2, a3, op->arg_size);
			break;

		case (JIT_LDX | REG | SIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
			else x86_movsx_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
			break;
		case (JIT_LDX | REG | UNSIGNED): 
			if (op->arg_size == REG_SIZE) x86_mov_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
			else x86_movzx_reg_memindex(jit->ip, a1, a2, 0, a3, 0, op->arg_size); 
			break;

		case (JIT_ST | IMM): x86_mov_mem_reg(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_ST | REG): x86_mov_membase_reg(jit->ip, a1, 0, a2, op->arg_size); break;
		case (JIT_STX | IMM): x86_mov_membase_reg(jit->ip, a2, a1, a3, op->arg_size); break;
		case (JIT_STX | REG): x86_mov_memindex_reg(jit->ip, a1, 0, a2, 0, a3, op->arg_size); break;

		case (JIT_UREG):
			if (JIT_REG(a1).spec == JIT_RTYPE_REG) {
				if (JIT_REG(a1).type == JIT_RTYPE_FLOAT) x86_movlpd_membase_xreg(jit->ip, a2, X86_EBP, __GET_FPREG_POS(jit, a1));
				else x86_mov_membase_reg(jit->ip, X86_EBP, __GET_REG_POS(jit, a1), a2, REG_SIZE);
			}
			break;
		case (JIT_LREG): 
			if (JIT_REG(a2).type == JIT_RTYPE_FLOAT) x86_movlpd_xreg_membase(jit->ip, a1, X86_EBP, __GET_FPREG_POS(jit, a2));
			else x86_mov_reg_membase(jit->ip, a1, X86_EBP, __GET_REG_POS(jit, a2), REG_SIZE);
			break;
		case (JIT_SYNCREG):
			if (JIT_REG(a1).type == JIT_RTYPE_FLOAT) x86_movlpd_membase_xreg(jit->ip, a2, X86_EBP, __GET_FPREG_POS(jit, a1));
			else x86_mov_membase_reg(jit->ip, X86_EBP, __GET_REG_POS(jit, a1), a2, REG_SIZE);
			break;
		case JIT_RENAMEREG: x86_mov_reg_reg(jit->ip, a1, a2, REG_SIZE); break;

		case JIT_CODESTART: break;
		case JIT_NOP: break;

		// Floating-point operations;
		case (JIT_FMOV | REG): x86_movsd_reg_reg(jit->ip, a1, a2); break;
		case (JIT_FMOV | IMM): x86_movsd_reg_mem(jit->ip, a1, &op->flt_imm); break;
		case (JIT_FADD | REG): __sse_alu_op(jit, op, X86_SSE_ADD); break;
		case (JIT_FSUB | REG): __sse_sub_op(jit, a1, a2, a3); break;
		case (JIT_FRSB | REG): __sse_sub_op(jit, a1, a3, a2); break;
		case (JIT_FMUL | REG): __sse_alu_op(jit, op, X86_SSE_MUL); break;
		case (JIT_FDIV | REG): __sse_div_op(jit, a1, a2, a3); break;
		case (JIT_FNEG | REG): __sse_neg_op(jit, a1, a2); break;
		case (JIT_FBLT | REG): __sse_branch(jit, op, a1, a2, a3, X86_CC_LT); break;
		case (JIT_FBGT | REG): __sse_branch(jit, op, a1, a2, a3, X86_CC_GT); break;
		case (JIT_FBGE | REG): __sse_branch(jit, op, a1, a2, a3, X86_CC_GE); break;
		case (JIT_FBLE | REG): __sse_branch(jit, op, a1, a3, a2, X86_CC_GE); break;
		case (JIT_FBEQ | REG): __sse_branch(jit, op, a1, a3, a2, X86_CC_EQ); break;
		case (JIT_FBNE | REG): __sse_branch(jit, op, a1, a3, a2, X86_CC_NE); break;

		case (JIT_EXT | REG): x86_cvtsi2sd(jit->ip, a1, a2); break;
		case (JIT_TRUNC | REG): x86_cvttsd2si(jit->ip, a1, a2); break;
		case (JIT_CEIL | REG): __sse_floor(jit, a1, a2, 0); break;
		case (JIT_FLOOR | REG): __sse_floor(jit, a1, a2, 1); break;
		case (JIT_ROUND | REG): __sse_round(jit, a1, a2); break;

		case (JIT_FST | IMM):
			if (op->arg_size == 4) {
				int live = jitset_get(op->live_out, op->arg[1]);
				if (live) x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);
				x86_cvtsd2ss(jit->ip, a2, a2);
				x86_movss_mem_xreg(jit->ip, a2, a1);
				if (live) x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
			} else x86_movlpd_mem_xreg(jit->ip, a2, a1);
			break;		 

		case (JIT_FST | REG):
			if (op->arg_size == 4) {
				int live = jitset_get(op->live_out, op->arg[1]);
				if (live) x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 0);
				x86_cvtsd2ss(jit->ip, a2, a2);
				x86_movss_membase_xreg(jit->ip, a2, a1, 0);
				if (live) x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a2, a2, 1);
			} else x86_movlpd_membase_xreg(jit->ip, a2, a1, 0);
			break;

		case (JIT_FSTX | IMM):
			if (op->arg_size == 4) {
				int live = jitset_get(op->live_out, op->arg[2]);
				if (live) x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a3, a3, 0);
				x86_cvtsd2ss(jit->ip, a3, a3);
				x86_movss_membase_xreg(jit->ip, a3, a2, a1);
				if (live) x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a3, a3, 1);
			} else x86_movlpd_membase_xreg(jit->ip, a3, a2, a1);
			break;

		case (JIT_FSTX | REG):
			if (op->arg_size == 4) {
				int live = jitset_get(op->live_out, op->arg[2]);
				if (live) x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a3, a3, 0);
				x86_cvtsd2ss(jit->ip, a3, a3);
				x86_movss_memindex_xreg(jit->ip, a3, a2, 0, a1, 0);
				if (live) x86_sse_alu_pd_reg_reg_imm(jit->ip, X86_SSE_SHUF, a3, a3, 1);
			} else x86_movlpd_memindex_xreg(jit->ip, a3, a2, 0, a1, 0);
			break;

		case (JIT_FLD | IMM): if (op->arg_size == 4) x86_cvtss2sd_reg_mem(jit->ip, a1, a2);
				      else x86_movlpd_xreg_mem(jit->ip, a1, a2);
				      break;

		case (JIT_FLD | REG): if (op->arg_size == 4) x86_cvtss2sd_reg_membase(jit->ip, a1, a2, 0);
				      else x86_movlpd_xreg_membase(jit->ip, a1, a2, 0);
				      break;

		case (JIT_FLDX | IMM): 	if (op->arg_size == 4) x86_cvtss2sd_reg_membase(jit->ip, a1, a2, a3);
				 	else x86_movlpd_xreg_membase(jit->ip, a1, a2, a3);
				      	break;

		case (JIT_FLDX | REG): if (op->arg_size == 4) x86_cvtss2sd_reg_memindex(jit->ip, a1, a2, 0, a3, 0);
				      else x86_movlpd_xreg_memindex(jit->ip, a1, a2, 0, a3, 0);
				      break;

		case (JIT_FRET | REG): x86_movlpd_membase_xreg(jit->ip, a1, X86_ESP, -8); // pushes the value beyond the top of the stack
				       x86_fld_membase(jit->ip, X86_ESP, -8, 1);            // transfers the value from the stack to the ST(0)

				       // common epilogue
				       jit->push_count -= __pop_callee_saved_regs(jit);
				       x86_mov_reg_reg(jit->ip, X86_ESP, X86_EBP, 4);
				       x86_pop_reg(jit->ip, X86_EBP);
				       x86_ret(jit->ip);
				       break;
		case JIT_FRETVAL: x86_fst_membase(jit->ip, X86_ESP, -8, 1, 1);
				  x86_movlpd_xreg_membase(jit->ip, a1, X86_ESP, -8);
				  break;

		// platform specific opcodes; used byt optimizer
		case (JIT_X86_STI | IMM): x86_mov_mem_imm(jit->ip, a1, a2, op->arg_size); break;
		case (JIT_X86_STI | REG): x86_mov_membase_imm(jit->ip, a1, 0, a2, op->arg_size); break;
		case (JIT_X86_STXI | IMM): x86_mov_membase_imm(jit->ip, a2, a1, a3, op->arg_size); break;
		case (JIT_X86_STXI | REG): x86_mov_memindex_imm(jit->ip, a1, 0, a2, 0, a3, op->arg_size); break;

		default: printf("x86: unknown operation (opcode: %i)\n", GET_OP(op));
	}
}

/* register allocator initilization */
struct jit_reg_allocator * jit_reg_allocator_create()
{
	int reg = 0;
	struct jit_reg_allocator * a = JIT_MALLOC(sizeof(struct jit_reg_allocator));
#ifndef JIT_REGISTER_TEST
	a->gp_reg_cnt = 6;
#else
	a->gp_reg_cnt = 4;
#endif
	a->gp_regpool = jit_regpool_init(a->gp_reg_cnt);
	a->gp_regs = JIT_MALLOC(sizeof(struct __hw_reg) * (a->gp_reg_cnt + 1));

	a->gp_regs[reg++] = (struct __hw_reg) { X86_EAX, 0, "eax", 0, 0, 5 };
	a->gp_regs[reg++] = (struct __hw_reg) { X86_EBX, 0, "ebx", 1, 0, 0 };
	a->gp_regs[reg++] = (struct __hw_reg) { X86_ECX, 0, "ecx", 0, 0, 3 };
	a->gp_regs[reg++] = (struct __hw_reg) { X86_EDX, 0, "edx", 0, 0, 4 };
#ifndef JIT_REGISTER_TEST
	a->gp_regs[reg++] = (struct __hw_reg) { X86_ESI, 0, "esi", 1, 0, 1 };
	a->gp_regs[reg++] = (struct __hw_reg) { X86_EDI, 0, "edi", 1, 0, 2 };
#endif
	a->gp_regs[reg++] = (struct __hw_reg) { X86_EBP, 0, "ebp", 0, 0, 100 };
	a->gp_arg_reg_cnt = 0;

	a->fp_reg = X86_EBP;
	a->ret_reg = X86_EAX;
	a->fpret_reg = -1; // unused

#ifndef JIT_REGISTER_TEST
	a->fp_reg_cnt = 8;
#else
	a->fp_reg_cnt = 4;
#endif

	reg = 0;
	a->fp_regpool = jit_regpool_init(a->fp_reg_cnt);
	a->fp_regs = JIT_MALLOC(sizeof(struct __hw_reg) * a->fp_reg_cnt);

	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM0, 0, "xmm0", 0, 1, 1 };
	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM1, 0, "xmm1", 0, 1, 2 };
	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM2, 0, "xmm2", 0, 1, 3 };
	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM3, 0, "xmm3", 0, 1, 4 };
#ifndef JIT_REGISTER_TEST
	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM4, 0, "xmm4", 0, 1, 5 };
	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM5, 0, "xmm5", 0, 1, 6 };
	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM6, 0, "xmm6", 0, 1, 7 };
	a->fp_regs[reg++] = (struct __hw_reg) { X86_XMM7, 0, "xmm7", 0, 1, 8 };
#endif

	a->fp_arg_reg_cnt = 0;
	return a;
}
