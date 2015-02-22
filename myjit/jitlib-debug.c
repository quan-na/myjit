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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <ctype.h>

#include "llrb.c"

#define OUTPUT_BUF_SIZE         (8192)

static inline int bufprint(char *buf, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	int result = vsprintf(buf + strlen(buf), format, ap);
	va_end(ap);
	return result;
}

static void gcc_based_debugger(struct jit * jit)
{
	char obj_file_name[] = "myjitXXXXXX";
	int obj_file_fd = mkstemp(obj_file_name);	


	char * cmd1_fmt = "gcc -x assembler -c -o %s -";
	char * cmd2_fmt = "objdump -d -M intel %s";

	char cmd1[strlen(cmd1_fmt) + strlen(obj_file_name) + 1];
	char cmd2[strlen(cmd2_fmt) + strlen(obj_file_name) + 1];

	sprintf(cmd1, cmd1_fmt, obj_file_name);

	FILE * f = popen(cmd1, "w");

	int size = jit->ip - jit->buf;
	fprintf (f, ".text\n.align 4\n.globl main\n.type main,@function\nmain:\n");
	for (int i = 0; i < size; i++)
		fprintf(f, ".byte %d\n", (unsigned int) jit->buf[i]);
	fclose(f);

	sprintf(cmd2, cmd2_fmt, obj_file_name);
	system(cmd2);

	close(obj_file_fd);
	unlink(obj_file_name);
}

void jit_dump_code(struct jit * jit, int verbosity)
{
	gcc_based_debugger(jit);
}

typedef struct jit_disasm {
	struct jit *jit;
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
	char *generic_jit_tree_valueemplate;
} jit_disasm;

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
		case JIT_LEAF:		return "leaf";
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

		case JIT_UREG:		return ".ureg";
		case JIT_LREG:		return ".lreg";
		case JIT_CODESTART:	return ".code";
		case JIT_LABEL:		return ".label";
		case JIT_SYNCREG:	return ".syncreg";
		case JIT_RENAMEREG:	return ".renamereg";
		case JIT_MSG:		return "msg";
		case JIT_NOP:		return "nop";
		case JIT_CODE_ALIGN:	return ".align";
		case JIT_DATA_BYTE:	return ".byte";
		case JIT_DATA_CADDR:	return ".caddr";
		case JIT_DATA_DADDR:	return ".daddr";
		case JIT_CODE_ADDR:	return "code_addr";
		case JIT_DATA_ADDR:	return "data_addr";

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

static inline int jit_op_is_label_or_patch_op(jit_op * op)
{
	return (op != NULL) && ((GET_OP(op) == JIT_LABEL) || (GET_OP(op) == JIT_PATCH));
}

static void assing_labels(jit_tree * n, long * id)
{
	if (n == NULL) return;
	assing_labels(n->left, id);
	n->value = (void *)*id;
	*id += 1;
	assing_labels(n->right, id);
}

static jit_tree * prepare_labels(struct jit * jit)
{
	long x = 1;
	jit_tree * n = NULL;
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		jit_op * xop = op;
		if (jit_op_is_label_or_patch_op(op)) {
			for (; xop != NULL; xop = xop->next) {
				if (!jit_op_is_label_or_patch_op(xop)) break;
			}
			n = jit_tree_insert(n, (long)xop, (void *)0, NULL);
		}
	}
	assing_labels(n, &x);
	return n;
}

// FIXME: find_patch i find_label maji stejny kod
static int find_patch(struct jit * jit, jit_tree * labels, jit_op * op)
{
	if (jit == NULL) return -1;
	for (jit_op * xop = jit_op_first(jit->ops); xop != NULL; xop = xop->next) {
		if (GET_OP(xop) != JIT_PATCH) continue;

		// label found
		if (xop->arg[0] == (long)op) {
			// looks for an operation associated with the label
			jit_op * yop = xop;
			for (; yop != NULL; yop = yop->next)
				if (!jit_op_is_label_or_patch_op(yop)) break;
			jit_tree * n = jit_tree_search(labels, (long)yop);
			if (n) return (long)n->value;
			else return -1;
		}
	}
	return -1;
}

static int find_label(struct jit * jit, jit_tree * labels, jit_label *label)
{
	if (jit == NULL) return -1;
	for (jit_op * xop = jit_op_first(jit->ops); xop != NULL; xop = xop->next) {
		if (GET_OP(xop) != JIT_LABEL) continue;

		// label found
		if (xop->arg[0] == (jit_value) label) {
			// looks for an operation associated with the label
			jit_op * yop = xop;
			for (; yop != NULL; yop = yop->next)
				if (!jit_op_is_label_or_patch_op(yop)) break;
			jit_tree * n = jit_tree_search(labels, (long)yop);
			if (n) return (long)n->value;
			else return -1;
		}
	}
	return -1;
}

static inline void print_addr(struct jit_disasm *disasm, char *buf, jit_tree *labels, jit_op *op, int arg_pos)
{
	void *arg = (void *)op->arg[arg_pos];
	struct jit *jit = disasm->jit;
	if (jit == NULL) {
		bufprint(buf, disasm->label_template, -1);
	} else if (arg == NULL)
		bufprint(buf, disasm->label_forward_template, find_patch(jit, labels, op));
	else if (jit_is_label(jit, arg)) 
		bufprint(buf, disasm->label_template, find_label(jit, labels, arg));
	else 
		bufprint(buf, disasm->generic_addr_template, arg);
}

static inline void print_arg(struct jit_disasm *disasm, char * buf, struct jit_op * op, int arg)
{
	long a = op->arg[arg - 1];
	if (ARG_TYPE(op, arg) == IMM) bufprint(buf, disasm->generic_jit_tree_valueemplate, a);
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

/*
static jit_tree * get_prev_rmap(jit_op * op)
{
	while (!op->regmap) op = op->prev;
	char * op_name = jit_get_op_name(op);
	printf("REEE:%s\n", op_name);
	print_rmap(op->regmap->map);
	return op->regmap->map;
}
*/

int print_op(struct jit_disasm * disasm, struct jit_op * op, jit_tree * labels, int verbosity)
{
	char linebuf[OUTPUT_BUF_SIZE];
	linebuf[0] = '\0';

	jit_tree * lab = jit_tree_search(labels, (long)op);
	if (lab) {
		bufprint(linebuf, disasm->label_template, (long)lab->value);
		bufprint(linebuf, ":\n");
	}

	/*
	int op_code = GET_OP(op);
	if (verbosity & JIT_DEBUG_LOADS) {
		char buf[256];
		int is_load_op = 1;
		jit_hw_reg * hreg;
		switch (op_code) {
			case JIT_SYNCREG: 
				jit_get_reg_name(buf, op->arg[0]);
				hreg = (jit_hw_reg *) jit_tree_search(get_prev_rmap(op), op->arg[0]);
				printf("\t.syncreg    %s <- %s", buf, hreg->name);
				break;
			default: is_load_op = 0;
		}
		if (is_load_op) goto print;
	}
*/


	char * op_name = jit_get_op_name(op);
	if (op_name[0] != '.')  strcat(linebuf, disasm->indent_template);
	else {
		switch (GET_OP(op)) {
			case JIT_DATA_BYTE:
			case JIT_CODE_ALIGN:
				bufprint(linebuf, "%s%s ", disasm->indent_template, op_name);
				while (strlen(linebuf) < 13) strcat(linebuf, " ");
				bufprint(linebuf, disasm->generic_jit_tree_valueemplate, op->arg[0]);
				goto print;
			case JIT_DATA_CADDR:
			case JIT_DATA_DADDR:
				bufprint(linebuf, "%s%s ", disasm->indent_template, op_name);
				while (strlen(linebuf) < 13) strcat(linebuf, " ");
				print_addr(disasm, linebuf, labels, op, 0); 
				goto print;
			default: 
				goto print;
				
		}
/*
		if (verbosity & JIT_DEBUG_LOADS) {
			switch (GET_OP(op)) {
				case JIT_LREG:
					strcat(linebuf, "\t");
					strcat(linebuf, op_name);
					while (strlen(linebuf) < 13) strcat(linebuf, " ");
					jit_get_reg_name(linebuf + strlen(linebuf), op->arg[1]);
					goto print;
				case JIT_UREG:
					strcat(linebuf, "\t");
					strcat(linebuf, op_name);
					while (strlen(linebuf) < 13) strcat(linebuf, " ");
					jit_get_reg_name(linebuf + strlen(linebuf), op->arg[0]);
					goto print;
				case JIT_SYNCREG:
					strcat(linebuf, ":XXX:");
					strcat(linebuf, op_name);
					while (strlen(linebuf) < 13) strcat(linebuf, " ");
					jit_get_reg_name(linebuf + strlen(linebuf), op->arg[0]);
					// FIXME: operands
					goto print;
				case JIT_RENAMEREG:
					strcat(linebuf, ":XXX:");
					strcat(linebuf, op_name);
					goto print;
				default:
					return;

			}
		} else return;
	//	strcat(linebuf, op_name);
	//
*/
	}
	strcat(linebuf, op_name);


	if (GET_OP_SUFFIX(op) & IMM) strcat(linebuf, "i");
	if (GET_OP_SUFFIX(op) & REG) strcat(linebuf, "r");
	if (GET_OP_SUFFIX(op) & UNSIGNED) strcat(linebuf, " uns.");

	while (strlen(linebuf) < 12) strcat(linebuf, " ");

	if (op->arg_size == 1) strcat(linebuf, " (byte)");
	if (op->arg_size == 2) strcat(linebuf, " (word)");
	if (op->arg_size == 4) strcat(linebuf, " (dword)");
	if (op->arg_size == 8) strcat(linebuf, " (qword)");

	if (GET_OP(op) == JIT_MSG) {
		print_str(linebuf, (char *)op->arg[0]);
		if (!IS_IMM(op)) strcat(linebuf, ", "), print_arg(disasm, linebuf, op, 2);
		goto print;
	}

	if ((GET_OP(op) == JIT_CODE_ADDR) || (GET_OP(op) == JIT_DATA_ADDR)) {
		strcat(linebuf, " ");
		print_arg(disasm, linebuf, op, 1);
		strcat(linebuf, ", ");
		print_addr(disasm, linebuf, labels, op, 1); 
		goto print;
	}

	if (GET_OP(op) == JIT_DECL_ARG) {
		switch (op->arg[0]) {
			case JIT_SIGNED_NUM: strcat(linebuf, " integer"); break;
			case JIT_UNSIGNED_NUM: strcat(linebuf, " uns. integer"); break;
			case JIT_FLOAT_NUM: strcat(linebuf, " float"); break;
			case JIT_PTR: strcat(linebuf, " ptr"); break;
			default: assert(0);
		};
		strcat(linebuf, ", ");
		print_arg(disasm, linebuf, op, 2);
		goto print;
	}


	if (ARG_TYPE(op, 1) != NO) {
		strcat(linebuf, " ");
		if (jit_op_is_cflow(op)) print_addr(disasm, linebuf, labels, op, 0); 
		else print_arg(disasm, linebuf, op, 1);
	} if (ARG_TYPE(op, 2) != NO) strcat(linebuf, ", "), print_arg(disasm, linebuf, op, 2);
	if (ARG_TYPE(op, 3) != NO) strcat(linebuf, ", "), print_arg(disasm, linebuf, op, 3);
print:
	printf("%s", linebuf);
	return strlen(linebuf);
}


int print_op_compilable(struct jit_disasm *disasm, struct jit_op * op, jit_tree * labels, int verbosity)
{
	char linebuf[OUTPUT_BUF_SIZE];
	linebuf[0] = '\0';

	jit_tree * lab = jit_tree_search(labels, (long)op);
	if (lab) {
		bufprint(linebuf, "// ");
		bufprint(linebuf, disasm->label_template, (long)lab->value);
		bufprint(linebuf, ":\n");
	}

	if (GET_OP(op) == JIT_LABEL) {
		bufprint(linebuf, "%sjit_label * ", disasm->indent_template);
		bufprint(linebuf, disasm->label_template, find_label(disasm->jit, labels, (jit_label *)op->arg[0]));
		bufprint(linebuf, " = jit_get_label(p");
		goto print;
	}

	if (GET_OP(op) == JIT_PATCH) {
		bufprint(linebuf, "%sjit_patch  (p, op_%li", disasm->indent_template, ((unsigned long)op->arg[0]) >> 4);
		goto print;
	}


	if (GET_OP(op) == JIT_DATA_BYTE) {
		bufprint(linebuf, "%sjit_data_byte(p, ", disasm->indent_template);
		bufprint(linebuf, disasm->generic_jit_tree_valueemplate, op->arg[0]);
		goto print;
	}

	if ((GET_OP(op) == JIT_CODE_ADDR) || (GET_OP(op) == JIT_DATA_ADDR)) {
		char * op_name = jit_get_op_name(op);
		bufprint(linebuf, "%sjit_op * op_%li = jit_%s(p, ", disasm->indent_template, ((unsigned long)op) >> 4, op_name);
		print_arg(disasm, linebuf, op, 1);
		strcat(linebuf, ", ");
		print_addr(disasm, linebuf, labels, op, 1); 
		goto print;
	}

	if ((GET_OP(op) == JIT_DATA_CADDR) || (GET_OP(op) == JIT_DATA_DADDR)) {
		char * op_name = jit_get_op_name(op);
		op_name++; // skips leading '.'
		bufprint(linebuf, "%sjit_op * op_%li = jit_data_%s(p, ", disasm->indent_template, ((unsigned long)op) >> 4, op_name);
		print_addr(disasm, linebuf, labels, op, 0); 
		goto print;
	}

	if (GET_OP(op) == JIT_CODE_ALIGN) {
		bufprint(linebuf, "%sjit_code_align  (p, ", disasm->indent_template);
		bufprint(linebuf, disasm->generic_jit_tree_valueemplate, op->arg[0]);
		goto print;
	}

	char * op_name = jit_get_op_name(op);
	if (op_name[0] != '.')  strcat(linebuf, disasm->indent_template);
	else goto direct_print;

	if (jit_op_is_cflow(op) && ((void *)op->arg[0] == JIT_FORWARD))
		bufprint(linebuf, "jit_op * op_%li = ", ((unsigned long)op) >> 4);


	bufprint(linebuf, "jit_%s", op_name);

	if (GET_OP(op) == JIT_CALL) goto no_suffix;

	if (GET_OP_SUFFIX(op) & IMM) strcat(linebuf, "i");
	if (GET_OP_SUFFIX(op) & REG) strcat(linebuf, "r");
	if (GET_OP_SUFFIX(op) & UNSIGNED) strcat(linebuf, "_u");
no_suffix:



	while (strlen(linebuf) < 15) strcat(linebuf, " ");
	if (GET_OP(op) == JIT_PREPARE) {
		strcat(linebuf, "(p");
		goto print;
	}
	strcat(linebuf, "(p,");

	
	if (GET_OP(op) == JIT_MSG) {
		print_str(linebuf, (char *)op->arg[0]);
		if (!IS_IMM(op)) strcat(linebuf, ", "), print_arg(disasm, linebuf, op, 2);
		goto print;
	}



	if (GET_OP(op) == JIT_DECL_ARG) {
		switch (op->arg[0]) {
			case JIT_SIGNED_NUM: strcat(linebuf, "JIT_SIGNED_NUM"); break;
			case JIT_UNSIGNED_NUM: strcat(linebuf, "JIT_UNSIGNED_NUM"); break;
			case JIT_FLOAT_NUM: strcat(linebuf, "JIT_FLOAT_NUM"); break;
			case JIT_PTR: strcat(linebuf, "JIT_PTR"); break;
			default: assert(0);
		};
		strcat(linebuf, ", ");
		print_arg(disasm, linebuf, op, 2);
		goto print;
	}


	if (ARG_TYPE(op, 1) != NO) {
		strcat(linebuf, " ");
		if (jit_op_is_cflow(op)) print_addr(disasm, linebuf, labels, op, 0); 
		else print_arg(disasm, linebuf, op, 1);
	} if (ARG_TYPE(op, 2) != NO) strcat(linebuf, ", "), print_arg(disasm, linebuf, op, 2);
	if (ARG_TYPE(op, 3) != NO) strcat(linebuf, ", "), print_arg(disasm, linebuf, op, 3);
	
	if (op->arg_size) {
		strcat(linebuf, ", ");
		bufprint(linebuf, "%i", op->arg_size);
	}
	
print:
	strcat(linebuf, ");");
direct_print:
	printf("%s", linebuf);
	return strlen(linebuf);
}


void jit_dump_ops(struct jit * jit, int verbosity)
{
	jit_tree * labels = prepare_labels(jit);
	struct jit_disasm general_disasm = (struct jit_disasm) {
		.jit = jit,
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
		.generic_jit_tree_valueemplate = "0x%lx",
	};
	struct jit_disasm compilable_disasm = (struct jit_disasm) {
		.jit = jit,
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
		.generic_jit_tree_valueemplate = "%li",
	};

	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		int size;
		if (verbosity & JIT_DEBUG_COMPILABLE)  size = print_op_compilable(&compilable_disasm, op, labels, verbosity);
		else size = print_op(&general_disasm, op, labels, verbosity);

		if (size == 0) continue;

		while (size < 35) {
			printf(" ");
			size++;
		}
		
		if ((verbosity & JIT_DEBUG_LIVENESS) && (op->live_in) && (op->live_out)) {
			printf("In: ");
			print_reg_liveness(&general_disasm, op->live_in->root);
			printf("\tOut: ");
			print_reg_liveness(&general_disasm, op->live_out->root);
		}

		if ((verbosity & JIT_DEBUG_ASSOC) && (op->regmap)) {
			printf("\tAssoc: ");
			print_rmap(&general_disasm, op->regmap->map);
		}

		printf("\n");
	}
	jit_tree_free(labels);
}
