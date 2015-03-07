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

#define GET_GPREG_POS(jit, r) (- ((JIT_REG(r).id + 1) * REG_SIZE) - jit_current_func_info(jit)->allocai_mem)
#define GET_FPREG_POS(jit, r) (- jit_current_func_info(jit)->gp_reg_count * REG_SIZE - (JIT_REG(r).id + 1) * sizeof(jit_float) - jit_current_func_info(jit)->allocai_mem)

static inline int GET_REG_POS(struct jit * jit, int r)
{
	if (JIT_REG(r).spec == JIT_RTYPE_REG) {
		if (JIT_REG(r). type == JIT_RTYPE_INT) return GET_GPREG_POS(jit, r);
		else return GET_FPREG_POS(jit, r);
	} else assert(0); 
}

#include "x86-common-stuff.c"

void jit_init_arg_params(struct jit * jit, struct jit_func_info * info, int p, int * phys_reg)
{
	struct jit_inp_arg * a = &(info->args[p]);

	if (p == 0) a->location.stack_pos = 8;
	else {
		struct jit_inp_arg * prev_a = &(info->args[p - 1]);
		int stack_shift = jit_value_align(prev_a->size, 4); // 4-bytes aligned
		a->location.stack_pos = prev_a->location.stack_pos + stack_shift;
	}

	a->spill_pos = a->location.stack_pos; 
	a->passed_by_reg = 0;
	a->overflow = 0;
}

static int emit_arguments(struct jit * jit)
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
		long value = args[i].value.generic;
		if (!args[i].isfp) { // integers
			if (!args[i].isreg) x86_push_imm(jit->ip, args[i].value.generic);
			else {
				if (is_spilled(value, jit->prepared_args.op, &sreg)) 
					x86_push_membase(jit->ip, X86_EBP, GET_REG_POS(jit, args[i].value.generic));
				else x86_push_reg(jit->ip, sreg);
			}
			continue;
		}
		//
		// floats (double)
		// 
		if (args[i].size == sizeof(double)) {
			if (args[i].isreg) { // doubles
				if (is_spilled(value, jit->prepared_args.op, &sreg)) {
					int pos = GET_FPREG_POS(jit, args[i].value.generic);
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
			if (is_spilled(value, jit->prepared_args.op, &sreg)) {
				int pos = GET_FPREG_POS(jit, args[i].value.generic);
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

static void emit_funcall(struct jit * jit, struct jit_op * op, int imm)
{
	int stack_correction = emit_arguments(jit);

	if (!imm) {
		x86_call_reg(jit->ip, op->r_arg[0]);
	} else {
		op->patch_addr = JIT_BUFFER_OFFSET(jit); 
		x86_call_imm(jit->ip, JIT_GET_ADDR(jit, op->r_arg[0]) - 4); /* 4: magic constant */
	}
	
	if (jit->prepared_args.stack_size + stack_correction) 
		x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, jit->prepared_args.stack_size + stack_correction);
	JIT_FREE(jit->prepared_args.args);

	jit->push_count -= emit_pop_caller_saved_regs(jit, op);
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

static void emit_prolog_op(struct jit * jit, jit_op * op)
{
	jit->current_func = op;
	struct jit_func_info * info = jit_current_func_info(jit);
	int prolog = jit_current_func_info(jit)->has_prolog;
	while ((long)jit->ip % 8) 
		x86_nop(jit->ip);

	op->patch_addr = JIT_BUFFER_OFFSET(jit);
	if (prolog) {
		x86_push_reg(jit->ip, X86_EBP);
		x86_mov_reg_reg(jit->ip, X86_EBP, X86_ESP, 4);
	}
	int stack_mem = info->allocai_mem + info->gp_reg_count * REG_SIZE + info->fp_reg_count * sizeof(double);

#ifdef __APPLE__
	stack_mem = jit_value_align(stack_mem, 16);
#endif
	if (prolog)
		x86_alu_reg_imm(jit->ip, X86_SUB, X86_ESP, stack_mem);
	jit->push_count = emit_push_callee_saved_regs(jit, op);
}

static void emit_msg_op(struct jit * jit, jit_op * op)
{
	x86_pushad(jit->ip);
	if (!IS_IMM(op)) x86_push_reg(jit->ip, op->r_arg[1]);
	x86_push_imm(jit->ip, op->r_arg[0]);
	op->patch_addr = JIT_BUFFER_OFFSET(jit); 
	x86_call_imm(jit->ip, printf);
	if (IS_IMM(op)) x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, 4);
	else x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, 8);
	x86_popad(jit->ip);
}

static void emit_fret_op(struct jit * jit, jit_op * op)
{
	jit_value arg = op->r_arg[0];

	x86_movlpd_membase_xreg(jit->ip, arg, X86_ESP, -8); // pushes the value beyond the top of the stack
	x86_fld_membase(jit->ip, X86_ESP, -8, 1);            // transfers the value from the stack to the ST(0)

	// common epilogue
	jit->push_count -= emit_pop_callee_saved_regs(jit);
	if (jit_current_func_info(jit)->has_prolog) {
		x86_mov_reg_reg(jit->ip, X86_ESP, X86_EBP, 4);
		x86_pop_reg(jit->ip, X86_EBP);
	}
	x86_ret(jit->ip);
}

static void emit_fretval_op(struct jit * jit, jit_op * op)
{
	x86_fst_membase(jit->ip, X86_ESP, -8, 1, 1);
	x86_movlpd_xreg_membase(jit->ip, op->r_arg[0], X86_ESP, -8);
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
	a->gp_regs = JIT_MALLOC(sizeof(jit_hw_reg) * (a->gp_reg_cnt + 1));

	a->gp_regs[reg++] = (jit_hw_reg) { X86_EAX, "eax", 0, 0, 5 };
	a->gp_regs[reg++] = (jit_hw_reg) { X86_EBX, "ebx", 1, 0, 0 };
	a->gp_regs[reg++] = (jit_hw_reg) { X86_ECX, "ecx", 0, 0, 3 };
	a->gp_regs[reg++] = (jit_hw_reg) { X86_EDX, "edx", 0, 0, 4 };
#ifndef JIT_REGISTER_TEST
	a->gp_regs[reg++] = (jit_hw_reg) { X86_ESI, "esi", 1, 0, 1 };
	a->gp_regs[reg++] = (jit_hw_reg) { X86_EDI, "edi", 1, 0, 2 };
#endif
	a->gp_regs[reg++] = (jit_hw_reg) { X86_EBP, "ebp", 0, 0, 100 };
	a->gp_arg_reg_cnt = 0;

	a->fp_reg = X86_EBP;
	a->ret_reg = &(a->gp_regs[0]);
	a->fpret_reg = NULL; // unused

#ifndef JIT_REGISTER_TEST
	a->fp_reg_cnt = 8;
#else
	a->fp_reg_cnt = 4;
#endif

	reg = 0;
	a->fp_regs = JIT_MALLOC(sizeof(jit_hw_reg) * a->fp_reg_cnt);

	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM0, "xmm0", 0, 1, 1 };
	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM1, "xmm1", 0, 1, 2 };
	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM2, "xmm2", 0, 1, 3 };
	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM3, "xmm3", 0, 1, 4 };
#ifndef JIT_REGISTER_TEST
	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM4, "xmm4", 0, 1, 5 };
	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM5, "xmm5", 0, 1, 6 };
	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM6, "xmm6", 0, 1, 7 };
	a->fp_regs[reg++] = (jit_hw_reg) { X86_XMM7, "xmm7", 0, 1, 8 };
#endif

	a->fp_arg_reg_cnt = 0;

	a->gp_arg_regs = NULL;
	a->fp_arg_regs = NULL;
	return a;
}
