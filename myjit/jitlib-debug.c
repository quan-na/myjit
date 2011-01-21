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
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <ctype.h>

#include "llrb.c"

#define OUTPUT_BUF_SIZE         (8192)

static void gcc_based_debugger(struct jit * jit)
{
	char * obj_file_name = tempnam(NULL, "myjit");

	char * cmd1_fmt = "gcc -x assembler -c -o %s -";
	char * cmd2_fmt = "objdump -d -M intel %s";

	char cmd1[strlen(cmd1_fmt) + strlen(obj_file_name)];
	char cmd2[strlen(cmd2_fmt) + strlen(obj_file_name)];

	sprintf(cmd1, cmd1_fmt, obj_file_name);

	FILE * f = popen(cmd1, "w");

	int size = jit->ip - jit->buf;
	fprintf (f, ".text\n.align 4\n.globl main\n.type main,@function\nmain:\n");
	for (int i = 0; i < size; i++)
		fprintf(f, ".byte %d\n", (unsigned int) jit->buf[i]);
	fclose(f);

	sprintf(cmd2, cmd2_fmt, obj_file_name);
	system(cmd2);

	unlink(obj_file_name);
	free(obj_file_name);
}

void jit_dump_code(struct jit * jit, int verbosity)
{
	gcc_based_debugger(jit);
}

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
		case JIT_DECL_ARG:	return "declarg";

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
		case JIT_MSG:		return "msg";
		case JIT_NOP:		return "nop";

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

void jit_get_reg_name(char * r, int reg)
{
	if (reg == R_FP) strcpy(r, "fp");
	else if (reg == R_OUT) strcpy(r, "out");
	else if (reg == R_IMM) strcpy(r, "imm");
	else if (reg == FR_IMM) strcpy(r, "fimm");
	else {
		if (JIT_REG(reg).spec == JIT_RTYPE_REG) {
			if (JIT_REG(reg).type == JIT_RTYPE_INT) sprintf(r, "r%i", JIT_REG(reg).id);
			else sprintf(r, "f%i", JIT_REG(reg).id);
		}
		else if (JIT_REG(reg).spec == JIT_RTYPE_ARG) {
			if (JIT_REG(reg).type == JIT_RTYPE_INT) sprintf(r, "arg%i", JIT_REG(reg).id);
			else sprintf(r, "farg%i", JIT_REG(reg).id);
		} else sprintf(r, "(unknown)");
	} 
}

static void print_rmap(rb_node * n)
{
	char buf[256];
	if (!n) return;
	print_rmap(n->left);

	jit_get_reg_name(buf, n->key);
	printf("%s=%s ", buf, ((struct __hw_reg *)n->value)->name);

	print_rmap(n->right);
}

static void print_reg_liveness(rb_node * n)
{
	char buf[256];
	if (!n) return;

	print_reg_liveness(n->left);

	jit_get_reg_name(buf, n->key);
	printf("%s ", buf);

	print_reg_liveness(n->right);
}

static inline int __is_cflow(jit_op * op)
{
	if (((GET_OP(op) == JIT_CALL) || (GET_OP(op) == JIT_JMP)) && (IS_IMM(op))) return 1;
	if ((GET_OP(op) == JIT_BLT) || (GET_OP(op) == JIT_BLE) || (GET_OP(op) == JIT_BGT)
	|| (GET_OP(op) == JIT_BGE) || (GET_OP(op) == JIT_BEQ) ||  (GET_OP(op) == JIT_BNE)
	|| (GET_OP(op) == JIT_FBLT) || (GET_OP(op) == JIT_FBLE) || (GET_OP(op) == JIT_FBGT)
	|| (GET_OP(op) == JIT_FBGE) || (GET_OP(op) == JIT_FBEQ) ||  (GET_OP(op) == JIT_FBNE)) return 1;
 
	return 0;
}

static inline int __is_label_op(jit_op * op)
{
	return (op != NULL) && ((GET_OP(op) == JIT_LABEL) || (GET_OP(op) == JIT_PATCH));
}

static void __assign_labels(rb_node * n, long * id)
{
	if (n == NULL) return;
	__assign_labels(n->left, id);
	n->value = (void *)*id;
	*id += 1;
	__assign_labels(n->right, id);
}

static rb_node * prepare_labels(struct jit * jit)
{
	long x = 1;
	rb_node * n = NULL;
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next) {
		jit_op * xop = op;
		if (__is_label_op(op)) {
			for (; xop != NULL; xop = xop->next) {
				if (!__is_label_op(xop)) break;
			}
			n = rb_insert(n, (long)xop, (void *)0, NULL);
		}
	}
	__assign_labels(n, &x);
	return n;
}

static int __find_patch(struct jit * jit, rb_node * labels, jit_op * op)
{
	for (jit_op * xop = jit_op_first(jit->ops); xop != NULL; xop = xop->next) {
		if (GET_OP(xop) != JIT_PATCH) continue;

		// label found
		if (xop->arg[0] == (long)op) {
			// looks for an operation associated with the label
			jit_op * yop = xop;
			for (; yop != NULL; yop = yop->next)
				if (!__is_label_op(yop)) break;
			rb_node * n = rb_search(labels, (long)yop);
			if (n) return (long)n->value;
			else return -1;
		}
	}
	return -1;
}

static int __find_label(struct jit * jit, rb_node * labels, jit_op * op)
{
	for (jit_op * xop = jit_op_first(jit->ops); xop != NULL; xop = xop->next) {
		if (GET_OP(xop) != JIT_LABEL) continue;

		// label found
		if (xop->arg[0] == op->arg[0]) {
			// looks for an operation associated with the label
			jit_op * yop = xop;
			for (; yop != NULL; yop = yop->next)
				if (!__is_label_op(yop)) break;
			rb_node * n = rb_search(labels, (long)yop);
			if (n) return (long)n->value;
			else return -1;
		}
	}
	return -1;
}

static inline void print_addr(char * buf, struct jit * jit, rb_node * labels, jit_op * op)
{
	char xbuf[256];
	long arg = op->arg[0];
	if ((void *)arg == NULL) {
		sprintf(xbuf, "L%i", __find_patch(jit, labels, op));
		strcat(buf, xbuf);
		return;
	}
	if (jit_is_label(jit, (void *)op->arg[0])) {
		sprintf(xbuf, "L%i", __find_label(jit, labels, op));
		strcat(buf, xbuf);
		return;
	}

	sprintf(xbuf, "<addr: %lx>", op->arg[0]);
	strcat(buf, xbuf);
}

static inline void print_arg(char * buf, struct jit_op * op, int arg)
{
	long a;
	char value[256];
	a = op->arg[arg - 1];
	if (ARG_TYPE(op, arg) == IMM) sprintf(value, "0x%lx", a);
	if ((ARG_TYPE(op, arg) == REG) || (ARG_TYPE(op, arg) == TREG)) jit_get_reg_name(value, a);
	strcat(buf, value);
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
static rb_node * __get_prev_rmap(jit_op * op)
{
	while (!op->regmap) op = op->prev;
	char * op_name = jit_get_op_name(op);
	printf("REEE:%s\n", op_name);
	print_rmap(op->regmap->map);
	return op->regmap->map;
}
*/
void __print_op(struct jit * jit, struct jit_op * op, rb_node * labels, int verbosity)
{
	rb_node * lab = rb_search(labels, (long)op);
	if (lab) printf("L%li:\n", (long)lab->value);

	char linebuf[OUTPUT_BUF_SIZE];
	linebuf[0] = '\0';

	/*
	int op_code = GET_OP(op);
	if (verbosity & JIT_DEBUG_LOADS) {
		char buf[256];
		int is_load_op = 1;
		struct __hw_reg * hreg;
		switch (op_code) {
			case JIT_SYNCREG: 
				jit_get_reg_name(buf, op->arg[0]);
				hreg = (struct __hw_reg *) rb_search(__get_prev_rmap(op), op->arg[0]);
				printf("\t.syncreg    %s <- %s", buf, hreg->name);
				break;
			default: is_load_op = 0;
		}
		if (is_load_op) goto print;
	}
*/
	char * op_name = jit_get_op_name(op);
	if (op_name[0] != '.')  strcat(linebuf, "\t");
	else return;
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
		if (!IS_IMM(op)) strcat(linebuf, ", "), print_arg(linebuf, op, 2);
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
		print_arg(linebuf, op, 2);
		goto print;
	}

	if (ARG_TYPE(op, 1) != NO) {
		strcat(linebuf, " ");
		if (__is_cflow(op)) print_addr(linebuf, jit, labels, op); 
		else print_arg(linebuf, op, 1);
	} if (ARG_TYPE(op, 2) != NO) strcat(linebuf, ", "), print_arg(linebuf, op, 2);
	if (ARG_TYPE(op, 3) != NO) strcat(linebuf, ", "), print_arg(linebuf, op, 3);
	
print:
	printf("%s", linebuf);

	if (verbosity) {
		for (int i = 35 - strlen(linebuf); i >= 0; i--)
			printf(" ");

		if ((verbosity & JIT_DEBUG_LIVENESS) && (op->live_in) && (op->live_out)) {
			printf("In: ");
			print_reg_liveness(op->live_in->root);
			printf("\tOut: ");
			print_reg_liveness(op->live_out->root);
		}

		if ((verbosity & JIT_DEBUG_ASSOC) && (op->regmap)) {
			printf("\tAssoc: ");
			print_rmap(op->regmap->map);
		}
	}
	printf("\n");
}

void jit_dump_ops(struct jit * jit, int verbosity)
{
	rb_node * labels = prepare_labels(jit);
	for (jit_op * op = jit_op_first(jit->ops); op != NULL; op = op->next)
		__print_op(jit, op, labels, verbosity);
	rb_free(labels);
}
