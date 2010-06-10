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

void jit_dump(struct jit * jit)
{
	FILE * f = stderr;

	char * source_file_name = tempnam(NULL, "myjit");
	char * obj_file_name = tempnam(NULL, "myjit");

	char * cmd1_fmt = "gcc -x assembler -c -o %s %s";
	char cmd1[strlen(cmd1_fmt) + strlen(source_file_name) + strlen(obj_file_name)];

	char * cmd2_fmt = "objdump -d -M intel %s";
	char cmd2[strlen(cmd2_fmt) + strlen(obj_file_name)];
	
	f = fopen(source_file_name, "w");

	int size = jit->ip - jit->buf;
	fprintf (f, ".text\n.align 4\n.globl main\n.type main,@function\nmain:\n");
        for (int i = 0; i < size; i++)
		fprintf(f, ".byte %d\n", (unsigned int) jit->buf[i]);
	fclose(f);

	sprintf(cmd1, cmd1_fmt, obj_file_name, source_file_name);
	system(cmd1);

	sprintf(cmd2, cmd2_fmt, obj_file_name);
	system(cmd2);

	unlink(source_file_name);
	unlink(obj_file_name);

	free(source_file_name);
	free(obj_file_name);
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
		case JIT_PATCH:		return "patch";
		case JIT_PREPARE:	return "prepare";
		case JIT_FINISH:	return "finish";
		case JIT_PUSHARG:	return "pusharg";
		case JIT_CALL:		return "call";
		case JIT_RET:		return "ret";
		case JIT_PROLOG:	return "prolog";
		case JIT_LEAF:		return "leaf";
		case JIT_GETARG:	return "getarg";
		case JIT_RETVAL:	return "retval";

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

		case JIT_UREG:	return ".ureg";
		case JIT_LREG:	return ".lreg";
		case JIT_CODESTART:	return ".code";
		default: return NULL;
	}
}

/*
 * FIXME: duplicita s struct __hw_reg
 */

struct __hw_reg_xxx {
	int id;
	unsigned long used;
	char * name;
	struct __virtual_reg * virt_reg;
};
void jit_get_reg_name(char * r, int reg, jit_op * op)
{
	if (reg == JIT_FP) strcpy(r, "FP");
	else if (reg == JIT_RETREG) strcpy(r, "RETREG");
	else if (!op->regmap) sprintf(r, "R%i", reg - 2);
	else sprintf(r, "R%i/%s", reg - 2, ((struct __hw_reg_xxx *)op->regmap[reg])->name);
}

static inline int jitset_get(jitset * s, unsigned int bit);
static inline void print_reg_liveness(struct jitset * s)
{
	if (!s) return;
	for (int i = 0; i < s->size; i++)
		printf("%i", (jitset_get(s, i) > 0));
}

static inline void print_arg(struct jit_op * op, int arg)
{
	long a;
	char rname[20];
	a = op->arg[arg - 1];
	if (ARG_TYPE(op, arg) == IMM) printf("0x%lx", a);
	if ((ARG_TYPE(op, arg) == REG) || (ARG_TYPE(op, arg) == TREG)) {
		jit_get_reg_name(rname, a, op);
		printf("%s", rname);
	}
}

void __print_op(struct jit * jit, struct jit_op * op)
{
	printf("%s", jit_get_op_name(op));

	if (op->code == JIT_LREG) {
		char rname[20];
		jit_get_reg_name(rname, op->arg[1], op);
		printf(" %s, %s\n", jit_reg_allocator_get_hwreg_name(jit->reg_al, op->arg[0]), rname);
		return;
	}
	if (op->code == JIT_UREG) {
		char rname[20];
		jit_get_reg_name(rname, op->arg[0], op);
		printf(" %s, %s\n", rname, jit_reg_allocator_get_hwreg_name(jit->reg_al, op->arg[1]));
		return;
	}

	if (GET_OP_SUFFIX(op) & IMM) printf("i");
	if (GET_OP_SUFFIX(op) & REG) printf("r");
	if (GET_OP_SUFFIX(op) & UNSIGNED) printf("_u");

	if (op->arg_size == 1) printf(" byte");
	if (op->arg_size == 2) printf(" word");
	if (op->arg_size == 4) printf(" dword");
	if (op->arg_size == 8) printf(" qword");

	if (ARG_TYPE(op, 1) != NO) printf(" "), print_arg(op, 1);
	if (ARG_TYPE(op, 2) != NO) printf(", "), print_arg(op, 2);
	if (ARG_TYPE(op, 3) != NO) printf(", "), print_arg(op, 3);

	printf("\t");
	printf("\t");
	print_reg_liveness(op->live_in);
	printf(" ");
	print_reg_liveness(op->live_out);

	printf("\n");
}

void jit_print_ops(struct jit * jit)
{
	for (jit_op * op = jit->ops; op != NULL; op = op->next)
		__print_op(jit, op);
}
