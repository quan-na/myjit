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
	.generic_value_template = "0x%lx",
};



static void report_warning(struct jit *jit, jit_op *op, char *desc)
{
	fprintf(stdout, "%s at function `%s' (%s:%i)\n", desc, op->debug_info->function, op->debug_info->filename, op->debug_info->lineno);
	print_op(stdout, &jit_debugging_disasm, op, NULL, 0);
	fprintf(stdout, "\n");
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
	// we have to skip these operations since, these are working with the flag register
	jit_opcode code = GET_OP(op);
	if ((code == JIT_ADDC) || (code == JIT_ADDX) || (code == JIT_SUBC) || (code == JIT_SUBX)
	|| (code == JIT_BOADD) || (code == JIT_BOSUB) || (code == JIT_BNOADD) || (code == JIT_BNOSUB)) return 0;

	for (int i = 0; i < 3; i++) {
		if ((ARG_TYPE(op, i + 1) == TREG) && (!jit_set_get(op->live_out, op->arg[i]))) {
			append_msg(msg_buf, "operation without effect");
			return JIT_WARN_OP_WITHOUT_EFFECT;
		}
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

static int valid_size(int size) {
	switch (size) {
		case 1: case 2: case 4:
#ifdef JIT_ARCH_AMD64
		case 8:
#endif
			return 1;
		default:
			return 0;
	}
}

static int valid_fsize(int size) 
{
	return (size == 4) || (size == 8);
}

static int check_argument_sizes(jit_op *op, char *msg_buf)
{
	switch (GET_OP(op)) {
		case JIT_LD: case JIT_LDX: case JIT_ST: case JIT_STX:
			if (valid_size(op->arg_size)) return 0;
			break;

		case JIT_FLD: case JIT_FLDX: case JIT_FST: case JIT_FSTX:
		case JIT_FPUTARG: case JIT_FRET: case JIT_FRETVAL:
			if (valid_fsize(op->arg_size)) return 0;
			break;

		case JIT_DECL_ARG:
			if (((op->arg[0] == JIT_SIGNED_NUM) || (op->arg[0] == JIT_UNSIGNED_NUM)) && valid_size(op->arg[1])) return 0;
			if ((op->arg[0] == JIT_FLOAT_NUM) && valid_fsize(op->arg[1])) return 0;
			if ((op->arg[0] == JIT_PTR) && (op->arg[1] == sizeof(void *))) return 0;
			break;
		default:
			return 0;
	}

	append_msg(msg_buf, "invalid data size");
	return JIT_WARN_INVALID_DATA_SIZE;
}

#define CHECK_ARG_TYPE(op, index, _type) (((ARG_TYPE(op, index) != REG) && (ARG_TYPE(op, index) != TREG)) || (JIT_REG(op->arg[index - 1]).type == _type))

static int check_register_types(struct jit *jit, jit_op *op, char *msg_buf)
{
	switch (GET_OP(op)) {
		case JIT_GETARG: {
			struct jit_func_info * info = jit_current_func_info(jit);
			if (info->args[op->arg[1]].type == JIT_FLOAT_NUM) {
				if (CHECK_ARG_TYPE(op, 1, JIT_RTYPE_FLOAT)) return 0;
			} else {
				if (CHECK_ARG_TYPE(op, 1, JIT_RTYPE_INT)) return 0;
			} 
			break;
		}
		case JIT_FST:
		case JIT_TRUNC:
		case JIT_CEIL:
		case JIT_ROUND:
		case JIT_FLOOR:
			if (CHECK_ARG_TYPE(op, 1, JIT_RTYPE_INT) && CHECK_ARG_TYPE(op, 2, JIT_RTYPE_FLOAT)) return 0;
			break;
		case JIT_EXT:
		case JIT_FLD:
			if (CHECK_ARG_TYPE(op, 1, JIT_RTYPE_FLOAT) && CHECK_ARG_TYPE(op, 2, JIT_RTYPE_INT)) return 0;
			break;
		case JIT_FLDX:
			if (CHECK_ARG_TYPE(op, 1, JIT_RTYPE_FLOAT) && CHECK_ARG_TYPE(op, 2, JIT_RTYPE_INT) && CHECK_ARG_TYPE(op, 3, JIT_RTYPE_INT)) return 0;
			break;
		case JIT_FSTX:
			if (CHECK_ARG_TYPE(op, 1, JIT_RTYPE_INT) && CHECK_ARG_TYPE(op, 2, JIT_RTYPE_INT) && CHECK_ARG_TYPE(op, 3, JIT_RTYPE_FLOAT)) return 0;
			break;
		case JIT_FORCE_SPILL:
		case JIT_FORCE_ASSOC:
			return 0;
		default: 
			if (!op->fp && CHECK_ARG_TYPE(op, 1, JIT_RTYPE_INT) && CHECK_ARG_TYPE(op, 2, JIT_RTYPE_INT) && CHECK_ARG_TYPE(op, 3, JIT_RTYPE_INT)) return 0;
			if (op->fp && CHECK_ARG_TYPE(op, 1, JIT_RTYPE_FLOAT) && CHECK_ARG_TYPE(op, 2, JIT_RTYPE_FLOAT) && CHECK_ARG_TYPE(op, 3, JIT_RTYPE_FLOAT)) return 0;
	}
	append_msg(msg_buf, "register type mismatch");
	return JIT_WARN_REGISTER_TYPE_MISMATCH;
}

static int jit_op_is_data_op(jit_op *op) 
{
	while (op && ((GET_OP(op) == JIT_LABEL) || (GET_OP(op) == JIT_PATCH))) op = op->next;
	if (!op) return 0;
	
	jit_opcode code = GET_OP(op);
	return ((code == JIT_DATA_BYTE) || (code == JIT_DATA_REF_CODE) || (code == JIT_DATA_REF_DATA));
}

static int check_data_alignment(jit_op *op, char *msg_buf)
{
	if (jit_op_is_data_op(op) || (GET_OP(op) == JIT_CODE_ALIGN)) return 0;
	if ((GET_OP(op) == JIT_LABEL) || (GET_OP(op) == JIT_PATCH)) return 0;
	jit_op * prev = op->prev;
	while (prev) {
		if ((GET_OP(prev) == JIT_LABEL) || (GET_OP(prev) == JIT_PATCH)) 
			prev = prev->prev;
		else break;
	}
	if (jit_op_is_data_op(prev)) {
		append_msg(msg_buf, "instruction follows unaligned data block");
		return JIT_WARN_UNALIGNED_CODE;
	}
	return 0;
}

static int check_data_references(jit_op *op, char *msg_buf)
{
	if (((GET_OP(op) == JIT_REF_DATA) || (GET_OP(op) == JIT_DATA_REF_DATA)) && !jit_op_is_data_op(op->jmp_addr)) {
		append_msg(msg_buf, "invalid data reference");
		return JIT_WARN_INVALID_DATA_REFERENCE;
	}
	return 0;
}

static int check_code_references(jit_op *op, char *msg_buf)
{
	if (((GET_OP(op) == JIT_REF_CODE) || (GET_OP(op) == JIT_DATA_REF_CODE)) && jit_op_is_data_op(op->jmp_addr)) {
		append_msg(msg_buf, "invalid code reference");
		return JIT_WARN_INVALID_CODE_REFERENCE;
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
		if (GET_OP(op) == JIT_PROLOG) jit->current_func = op;
		if (!op->debug_info) continue;
		buf[0] = '\0';

		if (warnings & JIT_WARN_DEAD_CODE) op->debug_info->warnings |= check_dead_code(op, buf);
		if (warnings & JIT_WARN_MISSING_PATCH) op->debug_info->warnings |= check_missing_patches(op, buf);
		if (warnings & JIT_WARN_OP_WITHOUT_EFFECT) op->debug_info->warnings |= check_op_without_effect(op, buf);
		if (warnings & JIT_WARN_UNINITIALIZED_REG) op->debug_info->warnings |= check_uninitialized_registers(op, buf);
		if (warnings & JIT_WARN_INVALID_DATA_SIZE) op->debug_info->warnings |= check_argument_sizes(op, buf);
		if (warnings & JIT_WARN_REGISTER_TYPE_MISMATCH) op->debug_info->warnings |= check_register_types(jit, op, buf);
		if (warnings & JIT_WARN_UNALIGNED_CODE) op->debug_info->warnings |= check_data_alignment(op, buf);
		if (warnings & JIT_WARN_INVALID_DATA_REFERENCE) op->debug_info->warnings |= check_data_references(op, buf);
		if (warnings & JIT_WARN_INVALID_CODE_REFERENCE) op->debug_info->warnings |= check_code_references(op, buf);

		if (op->debug_info->warnings) report_warning(jit, op, buf);
	}

	cleanup(jit);
}
