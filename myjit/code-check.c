/*
 * MyJIT 
 * Copyright (C) 2015 Petr Krajca, <petr.krajca@upol.cz>
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

static void jit_dead_code_analysis(struct jit *jit, int remove_dead_code);
static inline void jit_expand_patches_and_labels(struct jit *jit);
static inline void jit_flw_analysis(struct jit *jit);
static inline void jit_prepare_reg_counts(struct jit *jit);
static inline void jit_prepare_arguments(struct jit *jit);
void jit_get_reg_name(struct jit_disasm *disasm, char * r, int reg);

static struct jit_disasm jit_debugging_disasm =  {
	.jit = NULL,
	.indent_template = "    ",
	.reg_template = "r%i",
	.freg_template = "fr%i",
	.arg_template = "arg%i",
	.farg_template = "farg%i",
	.reg_out_template = "out",
	.reg_fp_template = "fp",
	.reg_imm_template = "imm",
	.reg_fimm_template = "fimm",
	.reg_unknown_template = "(unknown reg.)",
	.label_template = "<label>",
	.label_forward_template = "<label>",
	.generic_addr_template = "<addr: 0x%lx>",
	.generic_jit_tree_valueemplate = "0x%lx",
};



static void report_warning(struct jit *jit, jit_op *op, char *desc)
{
	printf("%s at function `%s' (%s:%i)\n", desc, op->debug_info->function, op->debug_info->filename, op->debug_info->lineno);
	print_op(&jit_debugging_disasm, op, NULL, 0);
	printf("\n");
}

static void append_msg(char *buf, char *format, ...)
{
	va_list ap;
	if (strlen(buf)) strcat(buf, ", ");
	va_start(ap, format);
	vsprintf(buf + strlen(buf), format, ap);
	va_end(ap);
}

static void cleanup(struct jit *jit)
{
	for (jit_op *op = jit_op_first(jit->ops); op; op = op->next) {
		if (op->live_in) {
			jit_set_free(op->live_in);
			op->live_in = NULL;
		} 
		if (op->live_out) {
			jit_set_free(op->live_out);
			op->live_out = NULL;
		}
		if (GET_OP(op) == JIT_PROLOG) {
			if (op->arg[1]) {
				struct jit_func_info *info = (struct jit_func_info *)op->arg[1];
				JIT_FREE(info->args);
				info->args = NULL;
			}
		}
		if (GET_OP(op) == JIT_PREPARE) {
			op->arg[0] = 0;
			op->arg[1] = 0;
		}
	}
}

static int check_dead_code(jit_op *op, char *msg_buf)
{
	if (!op->in_use) {
		append_msg(msg_buf, "unreachable operation");
		return JIT_WARN_DEAD_CODE;
	}
	return 0;
}

static int check_missing_patches(jit_op *op, char *msg_buf)
{
	if ((((GET_OP(op) == JIT_CALL) || (GET_OP(op) == JIT_JMP)) && IS_IMM(op)) || is_cond_branch_op(op))  {
		if ((op->arg[0] == (jit_value) JIT_FORWARD) && (op->jmp_addr == NULL)) {
			append_msg(msg_buf, "missing patch");
			return JIT_WARN_MISSING_PATCH;
		}
	}
	// FIXME: DATA_CADDR, DATA_DADDR, CODE_ADDR, DATA_ADDR
	return 0;
}

static int check_op_without_effect(jit_op *op, char *msg_buf)
{
	// we have to skip these operations since, these are setting carry flag
	if ((GET_OP(op) == JIT_ADDC) || (GET_OP(op) == JIT_ADDX)
	|| (GET_OP(op) == JIT_SUBC) || (GET_OP(op) == JIT_SUBX)) return 0;

	if ((ARG_TYPE(op, 1) == TREG) && (!jit_set_get(op->live_out, op->arg[0]))) {
		append_msg(msg_buf, "operation without effect");
		return JIT_WARN_OP_WITHOUT_EFFECT;
	}
	return 0;
}

static void print_regs(jit_tree_key reg, jit_tree_value v, void *thunk)
{
	char buf[32];
	if (reg == R_FP) return; 
	jit_get_reg_name(&jit_debugging_disasm, buf, reg);
	strcat(thunk, " ");
	strcat(thunk, buf);
}

static int check_uninitialized_registers(jit_op *op, char *msg_buf)
{
	if (GET_OP(op) != JIT_PROLOG) return 0;
	
	if (op->live_in->root != NULL) {
		char buf[4096];
		buf[0] = '\0';
		jit_tree_walk(op->live_in->root, print_regs, buf);
		if (strlen(buf)) {
			append_msg(msg_buf, "uninitialized register(s): %s", buf);
			return JIT_WARN_UNINITIALIZED_REG;
		}
	}
	return 0;
}

void jit_check_code(struct jit *jit, int warnings)
{
	char buf[8192];

	jit_expand_patches_and_labels(jit);
	jit_dead_code_analysis(jit, 0);
	jit_prepare_reg_counts(jit);
        jit_prepare_arguments(jit);
	jit_flw_analysis(jit);

	for (jit_op *op = jit_op_first(jit->ops); op; op = op->next) {
		if (!op->debug_info) continue;
		buf[0] = '\0';

		if (warnings & JIT_WARN_DEAD_CODE) op->debug_info->warnings |= check_dead_code(op, buf);
		if (warnings & JIT_WARN_MISSING_PATCH) op->debug_info->warnings |= check_missing_patches(op, buf);
		if (warnings & JIT_WARN_OP_WITHOUT_EFFECT) op->debug_info->warnings |= check_op_without_effect(op, buf);
		if (warnings & JIT_WARN_UNINITIALIZED_REG) op->debug_info->warnings |= check_uninitialized_registers(op, buf);

		if (op->debug_info->warnings) report_warning(jit, op, buf);
	}

	cleanup(jit);
}
