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


#ifndef JITLIB_CORE_H
#define JITLIB_CORE_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define JIT_FP		(0)
#define JIT_RETREG	(1)
#define JIT_IMMREG	(2) /* used by amd64 */

#define JIT_FORWARD	(NULL)

#define GET_OP(op) (op->code & 0xfff8)
#define GET_OP_SUFFIX(op) (op->code & 0x0007)
#define IS_IMM(op) (op->code & IMM)
#define IS_SIGNED(op) (!(op->code & UNSIGNED))

#define JIT_FORCE_REG(r) (r | 1 << 31)


#define NO  0x00
#define REG 0x01
#define IMM 0x02
#define TREG 0x03 /* target register */
#define SPEC(ARG1, ARG2, ARG3) (((ARG3) << 4) | ((ARG2) << 2) | (ARG1))
#define SPEC_SIGN(SIGN, ARG1, ARG2, ARG3) ((SIGN << 6) | ((ARG3) << 4) | ((ARG2) << 2) | (ARG1))
#define ARG_TYPE(op, arg) (((op)->spec >> ((arg) - 1) * 2) & 0x03)
#define UNSIGNED 0x04
#define SIGNED 0x00 
#define SIGN_MASK (0x04)

struct jitset;

typedef struct jit_op {
	unsigned short code;
	unsigned char spec;
	unsigned char arg_size; /* used by ld, st */
	unsigned char assigned;
	long arg[3];
	long r_arg[3];
	long patch_addr;
	struct jit_op * jmp_addr;
	struct jit_op * next;
	struct jit_op * prev;
	struct jitset * live_in; 
	struct jitset * live_out; 
	struct __hw_reg ** regmap; 
} jit_op;

typedef struct jit_label {
	long pos;
	jit_op * op;
	struct jit_label * next;
} jit_label;


struct jit_reg_allocator;
struct jit {
	// FIXME: comments

	/* buffer used to store generated code */
	unsigned char * buf;
	unsigned int buf_capacity;

	/* pointer to the buffer */
	unsigned char * ip;

	unsigned int reg_count;
	int argpos;
	unsigned int prepare_args;
	struct jit_op * ops;
	struct jit_op * last_op;
	long allocai_mem;
	struct jit_reg_allocator * reg_al;
	struct jit_op * current_func;
	jit_label * labels;
#ifdef JIT_ARCH_AMD64
	int argspos; // FIXME: better name
	struct arg {
		int isreg;
		long value;
	} * args;
#endif
};

struct __hw_reg;
struct __virtual_reg {
	int id;
	char * name;
	struct __hw_reg * hw_reg;
};

struct jit * jit_init(size_t buffer_size, unsigned int reg_count);
struct jit_op * jit_add_op(struct jit * jit, unsigned short code, unsigned char spec, long arg1, long arg2, long arg3, unsigned char arg_size);
void jit_generate_code(struct jit * jit);
void jit_free(struct jit * jit);

void jit_dump(struct jit * jit);
void jit_print_ops(struct jit * jit);
void __print_op(struct jit * jit, struct jit_op * op);
void jit_get_reg_name(char * r, int reg, jit_op * op);
void jit_patch_external_calls(struct jit * jit);
void jit_optimize_st_ops(struct jit * jit);
#ifdef JIT_ARCH_AMD64
void jit_correct_long_imms(struct jit * jit);
#endif


/* FIXME: presunout do generic-reg-allocator.h */
void jit_assign_regs(struct jit * jit);
struct jit_reg_allocator * jit_reg_allocator_create(unsigned int regcnt);
void jit_reg_allocator_free(struct jit_reg_allocator * a);
void jit_gen_op(struct jit * jit, jit_op * op);
char * jit_reg_allocator_get_hwreg_name(struct jit_reg_allocator * al, int reg);

#define JIT_CODESTART	(0x00 << 3)
#define JIT_UREG	(0x01 << 3)
#define JIT_LREG	(0x02 << 3)
//#define JIT_PROTECT	(0x03 << 3)
//#define JIT_UNPROTECT	(0x04 << 3)

#define JIT_MOV 	(0x05 << 3)
#define JIT_LD		(0x06 << 3)
#define JIT_LDX		(0x07 << 3)
#define JIT_ST		(0x08 << 3)
#define JIT_STX		(0x09 << 3)

#define JIT_JMP 	(0x10 << 3)
#define JIT_PATCH 	(0x11 << 3)
#define JIT_PREPARE 	(0x12 << 3)
//#define JIT_PREPARE_EXT	(0x13 << 3)
//#define JIT_FINISH 	(0x14 << 3)
#define JIT_PUSHARG 	(0x15 << 3)
#define JIT_CALL 	(0x16 << 3)
#define JIT_RET		(0x17 << 3)
#define JIT_PROLOG	(0x18 << 3)
#define JIT_LEAF	(0x19 << 3)
#define JIT_GETARG	(0x1a << 3)
#define JIT_RETVAL	(0x1b << 3)
#define JIT_LABEL	(0x1c << 3)
#define JIT_SYNCREG	(0x1d << 3)

#define JIT_ADD 	(0x20 << 3)
#define JIT_ADDC	(0x21 << 3)
#define JIT_ADDX	(0x22 << 3)
#define JIT_SUB		(0x23 << 3)
#define JIT_SUBC	(0x24 << 3)
#define JIT_SUBX	(0x25 << 3)
#define JIT_RSB		(0x26 << 3)
#define JIT_NEG	 	(0x27 << 3)
#define JIT_MUL		(0x28 << 3)
#define JIT_HMUL	(0x29 << 3)
#define JIT_DIV		(0x2a << 3)
#define JIT_MOD		(0x2b << 3)

#define JIT_OR	 	(0x30 << 3)
#define JIT_XOR	 	(0x31 << 3)
#define JIT_AND		(0x32 << 3)
#define JIT_LSH		(0x33 << 3)
#define JIT_RSH		(0x34 << 3)
#define JIT_NOT		(0x35 << 3)

#define JIT_LT 		(0x40 << 3)
#define JIT_LE		(0x41 << 3)
#define JIT_GT		(0x42 << 3)
#define JIT_GE		(0x43 << 3)
#define JIT_EQ		(0x44 << 3)
#define JIT_NE		(0x45 << 3)

#define JIT_BLT 	(0x50 << 3)
#define JIT_BLE		(0x51 << 3)
#define JIT_BGT		(0x52 << 3)
#define JIT_BGE		(0x53 << 3)
#define JIT_BEQ		(0x54 << 3)
#define JIT_BNE		(0x55 << 3)
#define JIT_BMS		(0x56 << 3)	
#define JIT_BMC		(0x57 << 3)	
#define JIT_BOADD	(0x58 << 3)	
#define JIT_BOSUB	(0x59 << 3)	

#define JIT_DUMPREG	(0x60 << 3)
#define JIT_NOP		(0xff << 3)


#define jit_movr(jit, a, b) jit_add_op(jit, JIT_MOV | REG, SPEC(TREG, REG, NO), a, b, 0, 0)
#define jit_movi(jit, a, b) jit_add_op(jit, JIT_MOV | IMM, SPEC(TREG, IMM, NO), a, (long)b, 0, 0)

/* functions, call, jumps, etc. */

#define jit_jmpr(jit, a) jit_add_op(jit, JIT_JMP | REG, SPEC(REG, NO, NO), a, 0, 0, 0)
#define jit_jmpi(jit, a) jit_add_op(jit, JIT_JMP | IMM, SPEC(IMM, NO, NO), (long)a, 0, 0, 0)
#define jit_patch(jit, a) jit_add_op(jit, JIT_PATCH | IMM, SPEC(IMM, NO, NO), (long)a, 0, 0, 0)

#define jit_prepare(jit, a) jit_add_op(jit, JIT_PREPARE, SPEC(IMM, NO, NO), (long)a, 0, 0, 0)
#define jit_pushargr(jit, a) jit_add_op(jit, JIT_PUSHARG | REG, SPEC(REG, NO, NO), a, 0, 0, 0)
#define jit_pushargi(jit, a) jit_add_op(jit, JIT_PUSHARG | IMM, SPEC(IMM, NO, NO), (long)a, 0, 0, 0)
#define jit_call(jit, a) jit_add_op(jit, JIT_CALL | IMM, SPEC(IMM, NO, NO), (long)a, 0, 0, 0)
#define jit_callr(jit, a) jit_add_op(jit, JIT_CALL | REG, SPEC(REG, NO, NO), a, 0, 0, 0)

#define jit_prolog(jit, a) jit_add_op(jit, JIT_PROLOG , SPEC(IMM, NO, NO), (long)a, 0, 0, 0)
#define jit_retr(jit, a) jit_add_op(jit, JIT_RET | REG, SPEC(REG, NO, NO), a, 0, 0, 0)
#define jit_reti(jit, a) jit_add_op(jit, JIT_RET | IMM, SPEC(IMM, NO, NO), (long)a, 0, 0, 0)
#define jit_retval(jit, a) jit_add_op(jit, JIT_RETVAL, SPEC(TREG, NO, NO), a, 0, 0, 0)

#define jit_getarg(jit, a, b, c) jit_add_op(jit, JIT_GETARG, SPEC(TREG, IMM, NO), a, (long)b, 0, c)
#define jit_getarg_u(jit, a, b, c) jit_add_op(jit, JIT_GETARG | UNSIGNED, SPEC(TREG, IMM, NO), a, (long)b, 0, c)

/* arithmethic */

#define jit_addr(jit, a, b, c) jit_add_op(jit, JIT_ADD | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_addi(jit, a, b, c) jit_add_op(jit, JIT_ADD | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_addcr(jit, a, b, c) jit_add_op(jit, JIT_ADDC | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_addci(jit, a, b, c) jit_add_op(jit, JIT_ADDC | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_addxr(jit, a, b, c) jit_add_op(jit, JIT_ADDX | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_addxi(jit, a, b, c) jit_add_op(jit, JIT_ADDX | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_subr(jit, a, b, c) jit_add_op(jit, JIT_SUB | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_subi(jit, a, b, c) jit_add_op(jit, JIT_SUB | IMM, SPEC(TREG, REG, IMM), a, b, (long)c, 0)
#define jit_subcr(jit, a, b, c) jit_add_op(jit, JIT_SUBC | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_subci(jit, a, b, c) jit_add_op(jit, JIT_SUBC | IMM, SPEC(TREG, REG, IMM), a, b, (long)c, 0)
#define jit_subxr(jit, a, b, c) jit_add_op(jit, JIT_SUBX | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_subxi(jit, a, b, c) jit_add_op(jit, JIT_SUBX | IMM, SPEC(TREG, REG, IMM), a, b, (long)c, 0)

#define jit_rsbr(jit, a, b, c) jit_add_op(jit, JIT_RSB | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_rsbi(jit, a, b, c) jit_add_op(jit, JIT_RSB | IMM, SPEC(TREG, REG, IMM), a, b, (long)c, 0)

#define jit_negr(jit, a, b) jit_add_op(jit, JIT_NEG, SPEC(TREG, REG, NO), a, b, 0, 0)

#define jit_mulr(jit, a, b, c) jit_add_op(jit, JIT_MUL | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_muli(jit, a, b, c) jit_add_op(jit, JIT_MUL | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_mulr_u(jit, a, b, c) jit_add_op(jit, JIT_MUL | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_muli_u(jit, a, b, c) jit_add_op(jit, JIT_MUL | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_hmulr(jit, a, b, c) jit_add_op(jit, JIT_HMUL | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_hmuli(jit, a, b, c) jit_add_op(jit, JIT_HMUL | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_hmulr_u(jit, a, b, c) jit_add_op(jit, JIT_HMUL | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_hmuli_u(jit, a, b, c) jit_add_op(jit, JIT_HMUL | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_divr(jit, a, b, c) jit_add_op(jit, JIT_DIV | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_divi(jit, a, b, c) jit_add_op(jit, JIT_DIV | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_divr_u(jit, a, b, c) jit_add_op(jit, JIT_DIV | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_divi_u(jit, a, b, c) jit_add_op(jit, JIT_DIV | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_modr(jit, a, b, c) jit_add_op(jit, JIT_MOD | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_modi(jit, a, b, c) jit_add_op(jit, JIT_MOD | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_modr_u(jit, a, b, c) jit_add_op(jit, JIT_MOD | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_modi_u(jit, a, b, c) jit_add_op(jit, JIT_MOD | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)

/* bitwise arithmethic */

#define jit_orr(jit, a, b, c) jit_add_op(jit, JIT_OR | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_ori(jit, a, b, c) jit_add_op(jit, JIT_OR | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_xorr(jit, a, b, c) jit_add_op(jit, JIT_XOR | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_xori(jit, a, b, c) jit_add_op(jit, JIT_XOR | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_andr(jit, a, b, c) jit_add_op(jit, JIT_AND | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_andi(jit, a, b, c) jit_add_op(jit, JIT_AND | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_lshr(jit, a, b, c) jit_add_op(jit, JIT_LSH | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_lshi(jit, a, b, c) jit_add_op(jit, JIT_LSH | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_rshr(jit, a, b, c) jit_add_op(jit, JIT_RSH | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_rshi(jit, a, b, c) jit_add_op(jit, JIT_RSH | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_rshr_u(jit, a, b, c) jit_add_op(jit, JIT_RSH | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_rshi_u(jit, a, b, c) jit_add_op(jit, JIT_RSH | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_notr(jit, a, b) jit_add_op(jit, JIT_NOT, SPEC(TREG, REG, NO), a, b, 0, 0)

/* branches */

#define jit_bltr(jit, a, b, c) jit_add_op(jit, JIT_BLT | REG | SIGNED, SPEC(IMM, REG, REG), (long)a, b, c, 0)
#define jit_blti(jit, a, b, c) jit_add_op(jit, JIT_BLT | IMM | SIGNED, SPEC(IMM, REG, IMM), (long)a, b, c, 0)
#define jit_bltr_u(jit, a, b, c) jit_add_op(jit, JIT_BLT | REG | UNSIGNED, SPEC(IMM, REG, REG), (long)a, b, c, 0)
#define jit_blti_u(jit, a, b, c) jit_add_op(jit, JIT_BLT | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (long)a, b, c, 0)

#define jit_bler(jit, a, b, c) jit_add_op(jit, JIT_BLE | REG | SIGNED, SPEC(IMM, REG, REG), (long)a, b, c, 0)
#define jit_blei(jit, a, b, c) jit_add_op(jit, JIT_BLE | IMM | SIGNED, SPEC(IMM, REG, IMM), (long)a, b, c, 0)
#define jit_bler_u(jit, a, b, c) jit_add_op(jit, JIT_BLE | REG | UNSIGNED, SPEC(IMM, REG, REG), (long)a, b, c, 0)
#define jit_blei_u(jit, a, b, c) jit_add_op(jit, JIT_BLE | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (long)a, b, c, 0)

#define jit_bgtr(jit, a, b, c) jit_add_op(jit, JIT_BGT | REG | SIGNED, SPEC(IMM, REG, REG), (long)a, b, c, 0)
#define jit_bgti(jit, a, b, c) jit_add_op(jit, JIT_BGT | IMM | SIGNED, SPEC(IMM, REG, IMM), (long)a, b, c, 0)
#define jit_bgtr_u(jit, a, b, c) jit_add_op(jit, JIT_BGT | REG | UNSIGNED, SPEC(IMM, REG, REG), (long)a, b, c, 0)
#define jit_bgti_u(jit, a, b, c) jit_add_op(jit, JIT_BGT | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (long)a, b, c, 0)

#define jit_bger(jit, a, b, c) jit_add_op(jit, JIT_BGE | REG | SIGNED, SPEC(IMM, REG, REG), (long)a, b, c, 0)
#define jit_bgei(jit, a, b, c) jit_add_op(jit, JIT_BGE | IMM | SIGNED, SPEC(IMM, REG, IMM), (long)a, b, c, 0)
#define jit_bger_u(jit, a, b, c) jit_add_op(jit, JIT_BGE | REG | UNSIGNED, SPEC(IMM, REG, REG), (long)a, b, c, 0)
#define jit_bgei_u(jit, a, b, c) jit_add_op(jit, JIT_BGE | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (long)a, b, c, 0)

#define jit_beqr(jit, a, b, c) jit_add_op(jit, JIT_BEQ | REG, SPEC(IMM, REG, REG), (long)a, b, c, 0)
#define jit_beqi(jit, a, b, c) jit_add_op(jit, JIT_BEQ | IMM, SPEC(IMM, REG, IMM), (long)a, b, c, 0)

#define jit_bner(jit, a, b, c) jit_add_op(jit, JIT_BNE | REG, SPEC(IMM, REG, REG), a, b, c, 0)
#define jit_bnei(jit, a, b, c) jit_add_op(jit, JIT_BNE | IMM, SPEC(IMM, REG, IMM), a, b, c, 0)

#define jit_bmsr(jit, a, b, c) jit_add_op(jit, JIT_BMS | REG, SPEC(IMM, REG, REG), a, b, c, 0)
#define jit_bmsi(jit, a, b, c) jit_add_op(jit, JIT_BMS | IMM, SPEC(IMM, REG, IMM), a, b, c, 0)

#define jit_bmcr(jit, a, b, c) jit_add_op(jit, JIT_BMC | REG, SPEC(IMM, REG, REG), a, b, c, 0)
#define jit_bmci(jit, a, b, c) jit_add_op(jit, JIT_BMC | IMM, SPEC(IMM, REG, IMM), a, b, c, 0)

#define jit_boaddr(jit, a, b, c) jit_add_op(jit, JIT_BOADD | REG | SIGNED, SPEC(IMM, REG, REG), a, b, c, 0)
#define jit_boaddi(jit, a, b, c) jit_add_op(jit, JIT_BOADD | IMM | SIGNED, SPEC(IMM, REG, IMM), a, b, c, 0)

#define jit_bosubr(jit, a, b, c) jit_add_op(jit, JIT_BOSUB | REG | SIGNED, SPEC(IMM, REG, REG), a, b, c, 0)
#define jit_bosubi(jit, a, b, c) jit_add_op(jit, JIT_BOSUB | IMM | SIGNED, SPEC(IMM, REG, IMM), a, b, c, 0)

/* conditions */

#define jit_ltr(jit, a, b, c) jit_add_op(jit, JIT_LT | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_lti(jit, a, b, c) jit_add_op(jit, JIT_LT | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_ltr_u(jit, a, b, c) jit_add_op(jit, JIT_LT | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_lti_u(jit, a, b, c) jit_add_op(jit, JIT_LT | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_ler(jit, a, b, c) jit_add_op(jit, JIT_LE | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_lei(jit, a, b, c) jit_add_op(jit, JIT_LE | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_ler_u(jit, a, b, c) jit_add_op(jit, JIT_LE | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_lei_u(jit, a, b, c) jit_add_op(jit, JIT_LE | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_gtr(jit, a, b, c) jit_add_op(jit, JIT_GT | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_gti(jit, a, b, c) jit_add_op(jit, JIT_GT | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_gtr_u(jit, a, b, c) jit_add_op(jit, JIT_GT | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_gti_u(jit, a, b, c) jit_add_op(jit, JIT_GT | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_ger(jit, a, b, c) jit_add_op(jit, JIT_GE | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_gei(jit, a, b, c) jit_add_op(jit, JIT_GE | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_ger_u(jit, a, b, c) jit_add_op(jit, JIT_GE | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_gei_u(jit, a, b, c) jit_add_op(jit, JIT_GE | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_eqr(jit, a, b, c) jit_add_op(jit, JIT_EQ | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_eqi(jit, a, b, c) jit_add_op(jit, JIT_EQ | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_ner(jit, a, b, c) jit_add_op(jit, JIT_NE | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_nei(jit, a, b, c) jit_add_op(jit, JIT_NE | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)

/* memory operations */

#define jit_ldr(jit, a, b, c) jit_add_op(jit, JIT_LD | REG | SIGNED, SPEC(TREG, REG, NO), a, b, 0, c) 
#define jit_ldi(jit, a, b, c) jit_add_op(jit, JIT_LD | IMM | SIGNED, SPEC(TREG, IMM, NO), a, (long)b, 0, c) 
#define jit_ldxr(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, d) 
#define jit_ldxi(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, (long)c, d) 
#define jit_ldr_u(jit, a, b, c) jit_add_op(jit, JIT_LD | REG | UNSIGNED, SPEC(TREG, REG, NO), a, b, 0, c) 
#define jit_ldi_u(jit, a, b, c) jit_add_op(jit, JIT_LD | IMM | UNSIGNED, SPEC(TREG, IMM, NO), a, (long)b, 0, c) 
#define jit_ldxr_u(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, d) 
#define jit_ldxi_u(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, (long)c, d) 


#define jit_str(jit, a, b, c) jit_add_op(jit, JIT_ST | REG, SPEC(REG, REG, NO), a, b, 0, c) 
#define jit_sti(jit, a, b, c) jit_add_op(jit, JIT_ST | IMM, SPEC(IMM, REG, NO), (long)a, b, 0, c) 
#define jit_stxr(jit, a, b, c, d) jit_add_op(jit, JIT_STX | REG, SPEC(REG, REG, REG), a, b, c, d) 
#define jit_stxi(jit, a, b, c, d) jit_add_op(jit, JIT_STX | IMM, SPEC(IMM, REG, REG), (long)a, b, c, d) 

#define jit_dumpreg(jit) jit_add_op(jit, JIT_DUMPREG, SPEC(NO, NO, NO), 0, 0, 0, 0)


static inline struct jit_op * __new_op(unsigned short code, unsigned char spec, long arg1, long arg2, long arg3, unsigned char arg_size)
{
	struct jit_op * r = JIT_MALLOC(sizeof(struct jit_op));
	r->code = code;
	r->spec = spec;

	r->arg[0] = arg1;
	r->arg[1] = arg2;
	r->arg[2] = arg3;

	r->assigned = 0;
	r->arg_size = arg_size;
	r->next = NULL;
	r->prev = NULL;
	r->patch_addr = 0;
	r->jmp_addr = NULL;
	r->regmap = NULL;
	r->live_in = NULL;
	r->live_out = NULL;
	return r;
}

static inline void jit_op_append(jit_op * op, jit_op * appended)
{
	appended->next = op->next;
	if (op->next != NULL) op->next->prev = appended;
	appended->prev = op;
	op->next = appended;
}

static inline void jit_op_prepend(jit_op * op, jit_op * prepended)
{
	prepended->prev = op->prev;
	if (op->prev != NULL) op->prev->next = prepended;
	prepended->next = op;
	op->prev = prepended;
}

static inline jit_op * jit_op_first(jit_op * op)
{
	while (op->prev != NULL) op = op->prev;
	return op;
}

static inline jit_op * jit_op_last(jit_op * op)
{
	while (op->next != NULL) op = op->next;
	return op;
}

static inline int jit_is_label(struct jit * jit, void * ptr)
{
	jit_label * lab = jit->labels;
	while (1) {
		if (lab == NULL) return 0;
		if (lab == ptr) return 1;
		lab = lab->next;
	}
}
#endif
