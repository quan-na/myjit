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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define ABS(x)	((x) < 0 ? - (x) : x)

#include "llrb.c"

#define OUTPUT_BUF_SIZE         (8192)
#define print_padding(buf, size) while (strlen((buf)) < (size)) { strcat((buf), " "); }

static jit_hw_reg * rmap_is_associated(jit_rmap * rmap, int reg_id, int fp, jit_value * virt_reg);

static inline int bufprint(char *buf, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	int result = vsprintf(buf + strlen(buf), format, ap);
	va_end(ap);
	return result;
}

static void compiler_based_debugger(struct jit * jit)
{
	char obj_file_name[] = "myjitXXXXXX";
	int obj_file_fd = mkstemp(obj_file_name);	


#ifndef __APPLE_CC__
	char * cmd1_fmt = "gcc -x assembler -c -o %s -";
	char * cmd2_fmt = "objdump -d -M intel %s";
#else
	char * cmd1_fmt = "cc -x assembler -c -o %s -";
	char * cmd2_fmt = "otool -tvVj %s";
#endif

	char cmd1[strlen(cmd1_fmt) + strlen(obj_file_name) + 1];
	char cmd2[strlen(cmd2_fmt) + strlen(obj_file_name) + 1];

	sprintf(cmd1, cmd1_fmt, obj_file_name);

	FILE * f = popen(cmd1, "w");

	int size = jit->ip - jit->buf;
#ifndef __APPLE_CC__
	fprintf (f, ".text\n.align 4\n.globl main\n.type main,@function\nmain:\n");
	for (int i = 0; i < size; i++)
		fprintf(f, ".byte %d\n", (unsigned int) jit->buf[i]);
#else
// clang & OS X
	fprintf (f, ".text\n.align 4\n.globl main\n\nmain:\n");
	for (int i = 0; i < size; i++)
		fprintf(f, ".byte 0x%x\n", (unsigned int) jit->buf[i]);
#endif
	pclose(f);
	

	sprintf(cmd2, cmd2_fmt, obj_file_name);
	system(cmd2);

	close(obj_file_fd);
	unlink(obj_file_name);
}

typedef struct jit_disasm {
	char *indent_template;
	char *reg_template;
	char *freg_template;
	char *arg_template;
	char *farg_template;
	char *reg_fp_template;
	char *reg_out_template;
	char *reg_imm_template;
	char *reg_fimm_template;
	char *reg_unknown_template;
	char *label_template;
	char *label_forward_template;
	char *generic_addr_template;
	char *generic_value_template;
} jit_disasm;

struct jit_disasm jit_disasm_general = {
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
	.label_template = "L%i",
	.label_forward_template = "L%i",
	.generic_addr_template = "<addr: 0x%lx>",
	.generic_value_template = "0x%lx",
};

struct jit_disasm jit_disasm_compilable = {
	.indent_template = "    ",   
	.reg_template = "R(%i)",
	.freg_template = "FR(%i)",
	.arg_template = "arg(%i)",
	.farg_template = "farg(%i)",
	.reg_fp_template = "R_FP",
	.reg_out_template = "R_OUT",
	.reg_imm_template = "R_IMM",
	.reg_fimm_template = "FR_IMM",
	.reg_unknown_template = "(unknown reg.)",
	.label_template = "label_%03i",
	.label_forward_template = "/* label_%03i */ JIT_FORWARD",
	.generic_addr_template = "<addr: 0x%lx>",
	.generic_value_template = "%li",
};



char * jit_get_op_name(struct jit_op * op)
{
	switch (GET_OP(op)) {
		case JIT_MOV:	return "mov";
		case JIT_LD:	return "ld";
		case JIT_LDX:	return "ldx";
		case JIT_ST:	return "st";
		case JIT_STX:	return "stx";

		case JIT_JMP:		return "jmp";
		case JIT_PATCH:		return ".patch";
		case JIT_PREPARE:	return "prepare";
		case JIT_PUTARG:	return "putarg";
		case JIT_CALL:		return "call";
		case JIT_RET:		return "ret";
		case JIT_PROLOG:	return "prolog";
		case JIT_GETARG:	return "getarg";
		case JIT_RETVAL:	return "retval";
		case JIT_ALLOCA:	return "alloca";
		case JIT_DECL_ARG:	return "declare_arg";

		case JIT_ADD:	return "add";
		case JIT_ADDC:	return "addc";
		case JIT_ADDX:	return "addx";
		case JIT_SUB:	return "sub";
		case JIT_SUBC:	return "subc";
		case JIT_SUBX:	return "subx";
		case JIT_RSB:	return "rsb";
		case JIT_NEG:	return "neg";
		case JIT_MUL:	return "mul";
		case JIT_HMUL:	return "hmul";
		case JIT_DIV:	return "div";
		case JIT_MOD:	return "mod";

		case JIT_OR:	return "or";
		case JIT_XOR:	return "xor";
		case JIT_AND:	return "and";
		case JIT_LSH:	return "lsh";
		case JIT_RSH:	return "rsh";
		case JIT_NOT:	return "not";

		case JIT_LT:	return "lt";
		case JIT_LE:	return "le";
		case JIT_GT:	return "gt";
		case JIT_GE:	return "ge";
		case JIT_EQ:	return "eq";
		case JIT_NE:	return "ne";

		case JIT_BLT:	return "blt";
		case JIT_BLE:	return "ble";
		case JIT_BGT:	return "bgt";
		case JIT_BGE:	return "bge";
		case JIT_BEQ:	return "beq";
		case JIT_BNE:	return "bne";
		case JIT_BMS:	return "bms";
		case JIT_BMC:	return "bmc";
		case JIT_BOADD:	return "boadd";
		case JIT_BOSUB:	return "bosub";
		case JIT_BNOADD:return "bnoadd";
		case JIT_BNOSUB:return "bnosub";

		case JIT_UREG:		return ".ureg";
		case JIT_LREG:		return ".lreg";
		case JIT_CODESTART:	return ".code";
		case JIT_LABEL:		return ".label";
		case JIT_SYNCREG:	return ".syncreg";
		case JIT_RENAMEREG:	return ".renamereg";
		case JIT_MSG:		return "msg";
		case JIT_COMMENT:	return ".comment";
		case JIT_NOP:		return "nop";
		case JIT_CODE_ALIGN:	return ".align";
		case JIT_DATA_BYTE:	return ".byte";
		case JIT_DATA_REF_CODE:	return ".ref_code";
		case JIT_DATA_REF_DATA:	return ".ref_data";
		case JIT_REF_CODE:	return "ref_code";
		case JIT_REF_DATA:	return "ref_data";
		case JIT_FULL_SPILL:	return ".full_spill";
		case JIT_FORCE_SPILL:	return "force_spill";
		case JIT_FORCE_ASSOC:	return "force_assoc";

		case JIT_FMOV:	return "fmov";
		case JIT_FADD: 	return "fadd";
		case JIT_FSUB: 	return "fsub";
		case JIT_FRSB: 	return "frsb";
		case JIT_FMUL: 	return "fmul";
		case JIT_FDIV: 	return "fdiv";
		case JIT_FNEG: 	return "fneg";
		case JIT_FRETVAL: return "fretval";
		case JIT_FPUTARG: return "fputarg";

		case JIT_EXT: 	return "ext";
		case JIT_ROUND: return "round";
		case JIT_TRUNC: return "trunc";
		case JIT_FLOOR: return "floor";
		case JIT_CEIL: 	return "ceil";

		case JIT_FBLT: return "fblt";
		case JIT_FBLE: return "fble";
		case JIT_FBGT: return "fbgt";
		case JIT_FBGE: return "fbge";
		case JIT_FBEQ: return "fbeq";
		case JIT_FBNE: return "fbne";

		case JIT_FLD: 	return "fld";
		case JIT_FLDX:  return "fldx";
		case JIT_FST:	return "fst";
		case JIT_FSTX:	return "fstx";

		case JIT_FRET: return "fret";
		default: return "(unknown)";
	}
}

void jit_get_reg_name(struct jit_disasm *disasm, char * r, int reg)
{
	if (reg == R_FP) strcpy(r, disasm->reg_fp_template);
	else if (reg == R_OUT) strcpy(r, disasm->reg_out_template);
	else if (reg == R_IMM) strcpy(r, disasm->reg_imm_template);
	else if (reg == FR_IMM) strcpy(r, disasm->reg_fimm_template);
	else {
		if (JIT_REG(reg).spec == JIT_RTYPE_REG) {
			if (JIT_REG(reg).type == JIT_RTYPE_INT) sprintf(r, disasm->reg_template, JIT_REG(reg).id);
			else sprintf(r, disasm->freg_template, JIT_REG(reg).id);
		}
		else if (JIT_REG(reg).spec == JIT_RTYPE_ARG) {
			if (JIT_REG(reg).type == JIT_RTYPE_INT) sprintf(r, disasm->arg_template, JIT_REG(reg).id);
			else sprintf(r, disasm->farg_template, JIT_REG(reg).id);
		} else sprintf(r, "%s", disasm->reg_unknown_template);
	} 
}

static void print_rmap_callback(jit_tree_key key, jit_tree_value value, void *disasm)
{
	char buf[256];
	jit_get_reg_name((struct jit_disasm *)disasm, buf, key);
	printf("%s=%s ", buf, ((jit_hw_reg *)value)->name);
}

static void print_reg_liveness_callback(jit_tree_key key, jit_tree_value value, void *disasm)
{
	char buf[256];
	jit_get_reg_name(disasm, buf, key);
	printf("%s ", buf);
}

#define print_rmap(disasm, n) jit_tree_walk(n, print_rmap_callback, disasm)
#define print_reg_liveness(disasm, n) jit_tree_walk(n, print_reg_liveness_callback, disasm)


static inline int jit_op_is_cflow(jit_op * op)
{
	if (((GET_OP(op) == JIT_CALL) || (GET_OP(op) == JIT_JMP)) && (IS_IMM(op))) return 1;
	if (is_cond_branch_op(op)) return 1;
 
	return 0;
}

static jit_tree * prepare_labels(struct jit * jit)
{
	long x = 1;
	jit_tree * n = NULL;
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if (GET_OP(op) == JIT_PATCH) {
			n = jit_tree_insert(n, (long) op, (void *)x, NULL);
			n = jit_tree_insert(n, op->arg[0], (void *) -x, NULL);
			x++;
		}
		if (GET_OP(op) == JIT_LABEL) {
			n = jit_tree_insert(n, op->arg[0], (void *)x, NULL);
			x++;
		}
	}
	return n;
}

static inline void print_addr(struct jit_disasm *disasm, char *buf, jit_tree *labels, jit_op *op, int arg_pos)
{
	void *arg = (void *)op->arg[arg_pos];

	jit_tree *label_item = jit_tree_search(labels, (long) op);
	if (label_item) bufprint(buf, disasm->label_forward_template, - (long) label_item->value);
	else {
		label_item = jit_tree_search(labels, (long) arg);
		if (label_item) bufprint(buf, disasm->label_template, (long) label_item->value);
		else bufprint(buf, disasm->generic_addr_template, arg);
	}
}

static inline void print_arg(struct jit_disasm *disasm, char * buf, struct jit_op * op, int arg)
{
	long a = op->arg[arg - 1];
	if (ARG_TYPE(op, arg) == IMM) bufprint(buf, disasm->generic_value_template, a);
	if ((ARG_TYPE(op, arg) == REG) || (ARG_TYPE(op, arg) == TREG)) {
		char value[256];
		jit_get_reg_name(disasm, value, a);
		strcat(buf, value);
	}
}

static inline void print_str(char * buf, char * str)
{
	strcat(buf, " \"");
	for (int i = 0; i < strlen(str); i++) {
		if (str[i] >= 32) {
			int s = strlen(buf);
			buf[s++] = str[i];
			buf[s] = '\0';
		} else  {
			char xbuf[16];
			switch (str[i]) {
				case 9: strcpy(xbuf, "\\t"); break;
				case 10: strcpy(xbuf, "\\n"); break;
				case 13: strcpy(xbuf, "\\r"); break;
				default: sprintf(xbuf, "\\x%02x", str[i]);
			}
			strcat(buf, xbuf);
		}
	}
	strcat(buf, "\"");
}

static void print_args(struct jit_disasm *disasm, char *linebuf, jit_op *op, jit_tree *labels) 
{
	for (int i = 1; i <= 3; i++) {
		if (ARG_TYPE(op, i) == NO) continue;
		strcat(linebuf, i == 1 ? " " : ", ");
		if ((i == 1) && jit_op_is_cflow(op)) print_addr(disasm, linebuf, labels, op, 0); 
		else print_arg(disasm, linebuf, op, i);
	}
}

void print_full_op_name(char *linebuf, jit_op *op)
{
	char *op_name = jit_get_op_name(op);
	strcat(linebuf, op_name);
	if ((GET_OP(op) == JIT_CALL) && (GET_OP_SUFFIX(op) & IMM)) return;
	if (GET_OP_SUFFIX(op) & IMM) strcat(linebuf, "i");
	if (GET_OP_SUFFIX(op) & REG) strcat(linebuf, "r");
	if (GET_OP_SUFFIX(op) & UNSIGNED) strcat(linebuf, "_u");
}

static int print_load_op(struct jit_disasm *disasm, char *linebuf, jit_op *op)
{
	
	switch (GET_OP(op)) {
		case JIT_LREG:
			strcat(linebuf, disasm->indent_template);
			strcat(linebuf, jit_get_op_name(op));
			print_padding(linebuf, 13);
			jit_get_reg_name(disasm, linebuf + strlen(linebuf), op->arg[1]);
			return 1;
		case JIT_UREG:
		case JIT_SYNCREG:
			strcat(linebuf, disasm->indent_template);
			strcat(linebuf, jit_get_op_name(op));
			print_padding(linebuf, 13);
			jit_get_reg_name(disasm, linebuf + strlen(linebuf), op->arg[0]);
			return 1;
		case JIT_RENAMEREG: {
				jit_value reg;
				rmap_is_associated(op->prev->regmap, op->arg[1], 0, &reg);
				strcat(linebuf, disasm->indent_template);
				strcat(linebuf, jit_get_op_name(op));
				strcat(linebuf, " ");
				print_padding(linebuf, 13);
				jit_get_reg_name(disasm, linebuf + strlen(linebuf), reg);
				return 1;
			}
		case JIT_FULL_SPILL:
			strcat(linebuf, disasm->indent_template);
			strcat(linebuf, jit_get_op_name(op));
			return 1;
		default:
			return 0;

	}
}

void print_comment(char *linebuf, jit_op *op)
{
	char *str = (char *)op->arg[0];
	bufprint(linebuf, "// ");	
	for (int i = 0; i < strlen(str); i++) {
		if ((str[i] == '\r') || (str[i] == '\n')) bufprint(linebuf, "\n// ");
		else bufprint(linebuf, "%c", str[i]);
	}
}


int print_op(FILE *f, struct jit_disasm * disasm, struct jit_op *op, jit_tree *labels, int verbosity)
{
	char linebuf[OUTPUT_BUF_SIZE];
	linebuf[0] = '\0';

	if ((GET_OP(op) == JIT_LABEL) || (GET_OP(op) == JIT_PATCH)) {
		jit_tree * lab = jit_tree_search(labels, (long)op->arg[0]);
		if (lab) {
			bufprint(linebuf, disasm->label_template, ABS((long)lab->value));
			bufprint(linebuf, ":");
		}
		goto print;
	}

	if (GET_OP(op) == JIT_COMMENT) {
		print_comment(linebuf, op);
		goto print;
	}
	
	char * op_name = jit_get_op_name(op);
	if ((op_name[0] == '.') && (verbosity & JIT_DEBUG_LOADS)) {
		if (print_load_op(disasm, linebuf, op)) goto print;
	}

	strcat(linebuf, disasm->indent_template);
	if (op_name[0] == '.') {
		switch (GET_OP(op)) {
			case JIT_DATA_BYTE:
			case JIT_CODE_ALIGN:
				bufprint(linebuf, "%s ", op_name);
				print_padding(linebuf, 13);
				bufprint(linebuf, disasm->generic_value_template, op->arg[0]);
				goto print;
			case JIT_DATA_REF_CODE:
			case JIT_DATA_REF_DATA:
				bufprint(linebuf, "%s ", op_name);
				print_padding(linebuf, 13);
				print_addr(disasm, linebuf, labels, op, 0); 
				goto print;
			default: 
				goto print;
				
		}
	}
	print_full_op_name(linebuf, op);

	print_padding(linebuf, 12);

	if (op->arg_size == 1) strcat(linebuf, " (byte)");
	if (op->arg_size == 2) strcat(linebuf, " (word)");
	if (op->arg_size == 4) strcat(linebuf, " (dword)");
	if (op->arg_size == 8) strcat(linebuf, " (qword)");

	switch (GET_OP(op)) {
		case JIT_PREPARE: break;
		case JIT_MSG:
			print_str(linebuf, (char *)op->arg[0]);
			if (!IS_IMM(op)) strcat(linebuf, ", "), print_arg(disasm, linebuf, op, 2);
			break;
		case JIT_REF_CODE:
		case JIT_REF_DATA:
			strcat(linebuf, " ");
			print_arg(disasm, linebuf, op, 1);
			strcat(linebuf, ", ");
			print_addr(disasm, linebuf, labels, op, 1); 
			break;
		case JIT_DECL_ARG:
			switch (op->arg[0]) {
				case JIT_SIGNED_NUM: strcat(linebuf, " integer"); break;
				case JIT_UNSIGNED_NUM: strcat(linebuf, " uns. integer"); break;
				case JIT_FLOAT_NUM: strcat(linebuf, " float"); break;
				case JIT_PTR: strcat(linebuf, " ptr"); break;
				default: assert(0);
			};
			strcat(linebuf, ", ");
			print_arg(disasm, linebuf, op, 2);
			break;
		default:
			print_args(disasm, linebuf, op, labels);
	}
print:
	fprintf(f, "%s", linebuf);
	return strlen(linebuf);
}


#define OP_ID(op) (((unsigned long) (op)) >> 4)

int print_op_compilable(struct jit_disasm *disasm, struct jit_op * op, jit_tree * labels)
{
	char linebuf[OUTPUT_BUF_SIZE];
	linebuf[0] = '\0';

	jit_tree * lab = jit_tree_search(labels, (long)op);
	if (lab && ((long) lab->value > 0)) {
		bufprint(linebuf, "// ");
		bufprint(linebuf, disasm->label_template, (long)lab->value);
		bufprint(linebuf, ":\n");
	}

	if (GET_OP(op) == JIT_COMMENT) {
		print_comment(linebuf, op);
		goto direct_print;
	}


	strcat(linebuf, disasm->indent_template);

	if ((jit_op_is_cflow(op) && ((void *)op->arg[0] == JIT_FORWARD))
	|| (GET_OP(op) == JIT_REF_CODE) || (GET_OP(op) == JIT_REF_DATA)
	|| (GET_OP(op) == JIT_DATA_REF_CODE) || (GET_OP(op) == JIT_DATA_REF_DATA))
		bufprint(linebuf, "jit_op * op_%li = ", OP_ID(op));

	switch (GET_OP(op)) {
		case JIT_LABEL: {
			bufprint(linebuf, "jit_label * ");

			jit_tree * lab = jit_tree_search(labels, (long)op->arg[0]);
			if (lab) bufprint(linebuf, disasm->label_template, (long) lab->value);
			bufprint(linebuf, " = jit_get_label(p");
			goto print;
		}
		case JIT_PATCH:
			bufprint(linebuf, "jit_patch  (p, op_%li", OP_ID(op->arg[0]));
			goto print;

		case JIT_DATA_BYTE:
			bufprint(linebuf, "jit_data_byte(p, ");
			bufprint(linebuf, disasm->generic_value_template, op->arg[0]);
			goto print;
		case JIT_REF_CODE:
		case JIT_REF_DATA:
			bufprint(linebuf, "jit_%s(p, ", jit_get_op_name(op));
			print_arg(disasm, linebuf, op, 1);
			strcat(linebuf, ", ");
			print_addr(disasm, linebuf, labels, op, 1); 
			goto print;
		case JIT_DATA_REF_CODE:
		case JIT_DATA_REF_DATA:
			bufprint(linebuf, "jit_data_%s(p, ", jit_get_op_name(op) + 1); // +1 skips leading '.'
			print_addr(disasm, linebuf, labels, op, 0); 
			goto print;
		case JIT_CODE_ALIGN:
			bufprint(linebuf, "jit_code_align  (p, ");
			bufprint(linebuf, disasm->generic_value_template, op->arg[0]);
			goto print;
		case JIT_PREPARE:
			bufprint(linebuf, "jit_prepare(p");
			goto print;
		default:
			break;
	}

	if (jit_get_op_name(op)[0] == '.') goto direct_print;

	strcat(linebuf, "jit_");
	print_full_op_name(linebuf, op);

	print_padding(linebuf, 15);

	strcat(linebuf, "(p,");

	switch (GET_OP(op)) {
		case JIT_MSG:
			print_str(linebuf, (char *)op->arg[0]);
			if (!IS_IMM(op)) strcat(linebuf, ", "), print_arg(disasm, linebuf, op, 2);
			break;
		case JIT_DECL_ARG:
			switch (op->arg[0]) {
				case JIT_SIGNED_NUM: strcat(linebuf, "JIT_SIGNED_NUM"); break;
				case JIT_UNSIGNED_NUM: strcat(linebuf, "JIT_UNSIGNED_NUM"); break;
				case JIT_FLOAT_NUM: strcat(linebuf, "JIT_FLOAT_NUM"); break;
				case JIT_PTR: strcat(linebuf, "JIT_PTR"); break;
				default: assert(0);
			};
			strcat(linebuf, ", ");
			print_arg(disasm, linebuf, op, 2);
			break;
		default:
			print_args(disasm, linebuf, op, labels);
	}
	
	if (op->arg_size) bufprint(linebuf, ", %i", op->arg_size);
	
print:
	strcat(linebuf, ");");
direct_print:
	printf("%s", linebuf);
	return strlen(linebuf);
}

static void jit_dump_ops_compilable(struct jit *jit, jit_tree *labels)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		print_op_compilable(&jit_disasm_compilable, op, labels);
		printf("\n");
	}
}

static void jit_dump_ops_general(struct jit *jit, jit_tree *labels, int verbosity)
{
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		int size = print_op(stdout, &jit_disasm_general, op, labels, verbosity);

		if (size == 0) return;

		for (; size < 35; size++)
			printf(" ");

		if ((verbosity & JIT_DEBUG_LIVENESS) && (op->live_in) && (op->live_out)) {
			printf("In: ");
			print_reg_liveness(&jit_disasm_general, op->live_in->root);
			printf("\tOut: ");
			print_reg_liveness(&jit_disasm_general, op->live_out->root);
		}

		if ((verbosity & JIT_DEBUG_ASSOC) && (op->regmap)) {
			printf("\tAssoc: ");
			print_rmap(&jit_disasm_general, op->regmap->map);
		}

		printf("\n");
	}
}

static char *platform_id()
{
#ifdef JIT_ARCH_AMD64
	return "amd64";
#elif JIT_ARCH_I386
	return "i386";
#else
	return "sparc";
#endif
}

static inline void print_op_bytes(FILE *f, struct jit *jit, jit_op *op) {
	for (int i = 0; i < op->code_length; i++)
		fprintf(f, " %02x", jit->buf[op->code_offset + i]);
	fprintf(f, "\n.nl\n");
}


static void jit_dump_ops_combined(struct jit *jit, jit_tree *labels)
{

	int fds[2];
	pipe(fds);

	pid_t child = fork();
	if (child == 0) {
		close(fds[1]); // write end
		dup2(fds[0], STDIN_FILENO);

		char *path;
		
		path = "./myjit-disasm";
		execlp(path, path, NULL);

		path = "myjit-disasm";
		execlp(path, path, NULL);

		path = getenv("MYJIT_DISASM");
		if (path) execlp(path, path, NULL);

		// FIXME: better explanation
		printf("myjit-disasm not found\n");
		exit(1);
	}

	// parent
	close(fds[0]); // read end
	FILE * f = fdopen(fds[1], "w");

	fprintf(f, ".addr=%lx\n", (unsigned long)jit->buf);
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		if (GET_OP(op) == JIT_DATA_BYTE) {
			fprintf(f, ".text\n%s.byte\n", jit_disasm_general.indent_template);
			fprintf(f, ".data\n");
			while (op && (GET_OP(op) == JIT_DATA_BYTE)) {
				fprintf(f, "%02x ", (unsigned char) op->arg[0]);
				op = op->next;
			}
			fprintf(f, "\n");

			if (!op) break;
			op = op->prev;
			continue;	
		}

		if (GET_OP(op) == JIT_COMMENT) {
			fprintf(f, ".comment\n");
			print_op(f, &jit_disasm_general, op, labels, JIT_DEBUG_LOADS);
			fprintf(f, "\n");
			continue;
		}

		fprintf(f, ".text\n");
		print_op(f, &jit_disasm_general, op, labels, JIT_DEBUG_LOADS);
		fprintf(f, "\n");

		switch (GET_OP(op)) {
			case JIT_CODE_ALIGN:
				if (op->next) {
					fprintf(f, "\n.nl\n");
					fprintf(f, ".addr=%lx\n", (unsigned long) (jit->buf + op->next->code_offset));
				}
				break;
			case JIT_DATA_REF_CODE:
			case JIT_DATA_REF_DATA:
				fprintf(f, ".data\n");
				print_op_bytes(f, jit, op);
				break;

			default:
				if (!op->code_length) break;
				fprintf(f, ".%s\n", platform_id());
				print_op_bytes(f, jit, op);
		}
	}

	fclose(f);
	close(fds[1]);
	wait(NULL);
}

void jit_dump_ops(struct jit * jit, int verbosity)
{
	if (!(verbosity & (JIT_DEBUG_CODE | JIT_DEBUG_COMPILABLE | JIT_DEBUG_COMBINED)))
		verbosity |= JIT_DEBUG_OPS;

	jit_tree * labels = prepare_labels(jit);
	if (verbosity & JIT_DEBUG_OPS) jit_dump_ops_general(jit, labels, verbosity);
	if (verbosity & JIT_DEBUG_CODE) compiler_based_debugger(jit);
	if (verbosity & JIT_DEBUG_COMPILABLE) jit_dump_ops_compilable(jit, labels);
	if (verbosity & JIT_DEBUG_COMBINED) jit_dump_ops_combined(jit, labels);
	jit_tree_free(labels);
}
