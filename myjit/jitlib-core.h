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
#include <string.h>
#include "llrb.c"

#define FR_IMM	(__mkreg(JIT_RTYPE_FLOAT, JIT_RTYPE_IMM, 0))
#define R_IMM	(__mkreg(JIT_RTYPE_INT, JIT_RTYPE_IMM, 0)) // used by amd64 and sparc

#define R_FP	(__mkreg(JIT_RTYPE_INT, JIT_RTYPE_ALIAS, 0))
#define R_OUT	(__mkreg(JIT_RTYPE_INT, JIT_RTYPE_ALIAS, 1))

#define JIT_FORWARD    (NULL)

#define GET_OP(op) (op->code & 0xfff8)
#define GET_OP_SUFFIX(op) (op->code & 0x0007)
#define IS_IMM(op) (op->code & IMM)
#define IS_SIGNED(op) (!(op->code & UNSIGNED))


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

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))


struct jitset;

typedef double jit_float;

typedef struct {
	unsigned type: 1; // INT / FP
	unsigned spec: 2; // register, alias, immediate, argument's shadow space
	unsigned part: 1; // allows to split one virtual register into two hw. registers (implicitly 0)
	unsigned id: 28;
#ifndef JIT_ARCH_AMD64
#else
	unsigned reserved: 32; 
#endif
} jit_reg;

#define JIT_RTYPE_REG	(0)
#define JIT_RTYPE_IMM	(1)
#define JIT_RTYPE_ALIAS	(2)
#define JIT_RTYPE_ARG	(3)

#define JIT_RTYPE_INT	(0)
#define JIT_RTYPE_FLOAT	(1)

#define JIT_DEBUG_LOADS		(0x01)
#define JIT_DEBUG_ASSOC		(0x02)
#define JIT_DEBUG_LIVENESS	(0x04)

#define JIT_OPT_OMIT_FRAME_PTR			(0x01)
#define JIT_OPT_OMIT_UNUSED_ASSIGNEMENTS	(0x02)
#define JIT_OPT_JOIN_ADDMUL			(0x03)
#define JIT_OPT_ALL				(0xff)

static inline jit_value JIT_REG_TO_JIT_VALUE(jit_reg r)
{
	jit_value v;
	memcpy(&v, &r, sizeof(jit_reg));
	return v;
}

static inline jit_value __mkreg(int type, int spec, int id)
{
	jit_reg r;
	r.type = type;
	r.spec = spec;
	r.id = id;
	r.part = 0;
#ifdef JIT_ARCH_AMD64
	r.reserved = 0;
#endif
	return JIT_REG_TO_JIT_VALUE(r);
}

static inline jit_value __mkreg_ex(int type, int spec, int id)
{
	jit_reg r;
	r.type = type;
	r.spec = spec;
	r.id = id;
	r.part = 1;
	return JIT_REG_TO_JIT_VALUE(r);
}

static inline jit_reg JIT_REG(jit_value v)
{
	jit_reg r;
	memcpy(&r, &v, sizeof(jit_value));
	return r;
}

struct __hw_reg {
	int id;
	unsigned long used; // FIXME: unnecessary for LTU allocator
	char * name;
	char callee_saved;
	char fp;
	short priority;
};

typedef struct __hw_reg jit_hw_reg;

struct jit_reg_allocator {
	int gp_reg_cnt;				// number of general purpose registers 
	int fp_reg_cnt;				// number of floating-point registers 
	int fp_reg; 				// frame pointer; register used to access the local variables
	int gp_arg_reg_cnt;			// number of GP registers used to pass arguments
	int fp_arg_reg_cnt;			// number of FP registers used to pass arguments
	jit_hw_reg * ret_reg; 			// register used to return value
	jit_hw_reg * fpret_reg;			// register used to return FP value

	jit_hw_reg * gp_regs;			// array of available GP registers
	jit_hw_reg * fp_regs;			// array of available floating-point registers
	jit_hw_reg ** gp_arg_regs;		// array of GP registers used to pass arguments (in the given order) 
	jit_hw_reg ** fp_arg_regs;		// array of FP registers used to pass arguments (in the given order) 
	struct jit_func_info * current_func_info; // information on currently processed function
};

typedef struct rmap_t {
	struct rb_node * map;		// R/B tree which maps virtual registers to hardware registers
} rmap_t;

typedef struct jit_op {
	unsigned short code;		// operation code
	unsigned char spec;		// argument types, e.g REG+REG+IMM
	unsigned char arg_size; 	// used by ld, st
	unsigned char assigned;
	unsigned char fp;		// FP if it's a floating-point operation	
	double flt_imm;			// floating point immediate value
	jit_value arg[3];		// arguments passed by user
	jit_value r_arg[3];		// arguments transformed by register allocator
	long patch_addr;
	struct jit_op * jmp_addr;
	struct jit_op * next;
	struct jit_op * prev;
	struct jitset * live_in;
	struct jitset * live_out;
	rmap_t * regmap;		// register mappings 
	int normalized_pos;		// number of operations from the end of the function
	struct rb_node * allocator_hints; // reg. allocator to collect statistics on used registers
} jit_op;

struct jit_allocator_hint {
	int last_pos;
	int should_be_calleesaved;
	int should_be_eax;
	int refs;			// counts number of references to the hint
};

typedef struct jit_label {
	long pos;
	jit_op * op;
	struct jit_label * next;
} jit_label;

typedef struct jit_prepared_args {
	int count;	// number of arguments to prepare
	int ready;	// number of arguments that have been prapared
	int gp_args;	// number of prepared GP arguments
	int fp_args;	// number od prepared FP arguments
	int stack_size; // size of stack occupied by passed arguments
	jit_op * op;	// corresponding ``PREPARE'' operation
	struct jit_out_arg {// array of arguments
		union {
			long generic;
			double fp;
		} value;
		int argpos; // position in the list of GP or FP arguments
		char isreg; // argument is given by value of the register (if zero, the immediate value is used)
		char isfp;
		char size;
	} * args;
} jit_prepared_args;

enum jit_inp_type {
	JIT_SIGNED_NUM,
	JIT_UNSIGNED_NUM,
	JIT_FLOAT_NUM,
	JIT_PTR
};

typedef struct jitset {
	rb_node * root;
} jitset;

struct jit_func_info {			// collection of information related to one function
	int general_arg_cnt;		// number of non-FP arguments
	int float_arg_cnt;		// number of FP arguments
	long allocai_mem;		// size of the locally allocated memory
	int arg_capacity;		// size of the `args' array
	struct jit_inp_arg { 
		enum jit_inp_type type; // type of the argument
		int size;		// its size
		char passed_by_reg; 	// indicates whether the argument was passed by register
		union { 
			int reg;
			int stack_pos;
		} location;		// location of the value
		int spill_pos;		// location of the argument on the stack, if the value was spilled off
		int gp_pos;		// position of the argument in the list of GP/FP
		int fp_pos;		// position of the argument in the list of GP/FP
		int overflow;		// indicates whether one argument overflow into the adjacent register
		int phys_reg;		
	} * args;			// collection of all arguments

	int gp_reg_count;		// total number of GP registers used in the processed function
	int fp_reg_count;		// total number of FP registers used in the processed function
	int uses_frame_ptr;		// this flag indicates whether there is an operation using frame pointer
};

struct jit {
	unsigned char * buf; 		// buffer used to store generated code
	unsigned int buf_capacity; 	// its capacity

	unsigned char * ip;		// pointer to the buffer

	int argpos;			// FIXME: REMOVEME
	struct jit_op * ops;		// list of operations
	struct jit_op * last_op;	// last operation
	struct jit_reg_allocator * reg_al; // register allocatot
	struct jit_op * current_func;	// pointer to the PROLOG operation of the currently processed function
	jit_label * labels;		// list of labels used in the c
	jit_prepared_args prepared_args; // list of arguments passed between PREPARE-CALL
	int push_count;			// number of values pushed on the stack; used by AMD64
	unsigned int optimizations;
};

struct jit * jit_init();
struct jit_op * jit_add_op(struct jit * jit, unsigned short code, unsigned char spec, jit_value arg1, jit_value arg2, jit_value arg3, unsigned char arg_size);
struct jit_op * jit_add_fop(struct jit * jit, unsigned short code, unsigned char spec, jit_value arg1, jit_value arg2, jit_value arg3, double flt_imm, unsigned char arg_size);
void jit_generate_code(struct jit * jit);
void jit_free(struct jit * jit);

void jit_dump_code(struct jit * jit, int verbosity);
void jit_dump_ops(struct jit * jit, int verbosity);

void jit_get_reg_name(char * r, int reg);
void jit_patch_external_calls(struct jit * jit);

void jit_collect_statistics(struct jit * jit);

void jit_optimize_st_ops(struct jit * jit);
int jit_optimize_join_addmul(struct jit * jit);
int jit_optimize_join_addimm(struct jit * jit);
void jit_optimize_frame_ptr(struct jit * jit);
void jit_optimize_unused_assignments(struct jit * jit);

void jit_enable_optimization(struct jit * jit, int opt);
void jit_disable_optimization(struct jit * jit, int opt);

/**
 * Initialize argpos-th argument.
 * phys_reg indicates how many actual registers have been already used to pass
 * the arguments. This is used to correctly pass arguments which occupies two
 * or more registers
 */
void jit_init_arg_params(struct jit * jit, struct jit_func_info * info, int argpos, int * phys_reg);

/* FIXME: presunout do generic-reg-allocator.h */
void jit_assign_regs(struct jit * jit);
struct jit_reg_allocator * jit_reg_allocator_create();
void jit_reg_allocator_free(struct jit_reg_allocator * a);
void jit_gen_op(struct jit * jit, jit_op * op);
char * jit_reg_allocator_get_hwreg_name(struct jit_reg_allocator * al, int reg);
int jit_reg_in_use(jit_op * op, int reg, int fp);
jit_hw_reg * jit_get_unused_reg(struct jit_reg_allocator * al, jit_op * op, int fp);
void rmap_free(rmap_t * regmap);
void jit_allocator_hints_free(struct rb_node *);

#define JIT_CODESTART	(0x00 << 3)
#define JIT_UREG	(0x01 << 3)
#define JIT_LREG	(0x02 << 3)
#define JIT_SYNCREG	(0x03 << 3)
#define JIT_RENAMEREG	(0x0f << 3) 

#define JIT_DECL_ARG	(0x04 << 3)
#define JIT_ALLOCA	(0x05 << 3)

#define JIT_MOV 	(0x06 << 3)
#define JIT_LD		(0x07 << 3)
#define JIT_LDX		(0x08 << 3)
#define JIT_ST		(0x09 << 3)
#define JIT_STX		(0x0a << 3)

#define JIT_JMP 	(0x10 << 3)
#define JIT_PATCH 	(0x11 << 3)
#define JIT_PREPARE 	(0x12 << 3)
#define JIT_PUTARG 	(0x15 << 3)
#define JIT_CALL 	(0x16 << 3)
#define JIT_RET		(0x17 << 3)
#define JIT_PROLOG	(0x18 << 3)
#define JIT_LEAF	(0x19 << 3)
#define JIT_GETARG	(0x1a << 3)
#define JIT_RETVAL	(0x1b << 3)
#define JIT_LABEL	(0x1c << 3)

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

#define JIT_MSG		(0x60 << 3)
#define JIT_NOP		(0xff << 3)

#define JIT_FMOV	(0x70 << 3)
#define JIT_FADD	(0x71 << 3)
#define JIT_FSUB	(0x72 << 3)
#define JIT_FRSB 	(0x73 << 3)
#define JIT_FMUL	(0x74 << 3)
#define JIT_FDIV	(0x75 << 3)
#define JIT_FNEG	(0x76 << 3)
#define JIT_FRETVAL	(0x77 << 3)
#define JIT_FPUTARG	(0x78 << 3)

#define JIT_EXT		(0x79 << 3)
#define JIT_ROUND	(0x7a << 3)
#define JIT_TRUNC	(0x7b << 3)
#define JIT_FLOOR	(0x7c << 3)
#define JIT_CEIL	(0x7d << 3)

/*
#define JIT_FLT 	(0x80 << 3)
#define JIT_FLE		(0x81 << 3)
#define JIT_FGT		(0x82 << 3)
#define JIT_FGE		(0x83 << 3)
#define JIT_FEQ		(0x84 << 3)
#define JIT_FNE		(0x85 << 3)
*/

#define JIT_FBLT 	(0x86 << 3)
#define JIT_FBLE	(0x87 << 3)
#define JIT_FBGT	(0x88 << 3)
#define JIT_FBGE	(0x89 << 3)
#define JIT_FBEQ	(0x8a << 3)
#define JIT_FBNE	(0x8b << 3)

#define JIT_FLD		(0x90 << 3)
#define JIT_FLDX	(0x91 << 3)
#define JIT_FST		(0x92 << 3)
#define JIT_FSTX	(0x93 << 3)

#define JIT_FRET	(0x95 << 3)


#define jit_movr(jit, a, b) jit_add_op(jit, JIT_MOV | REG, SPEC(TREG, REG, NO), a, b, 0, 0)
#define jit_movi(jit, a, b) jit_add_op(jit, JIT_MOV | IMM, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, 0)

/* functions, call, jumps, etc. */

#define jit_jmpr(jit, a) jit_add_op(jit, JIT_JMP | REG, SPEC(REG, NO, NO), a, 0, 0, 0)
#define jit_jmpi(jit, a) jit_add_op(jit, JIT_JMP | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0)
#define jit_patch(jit, a) jit_add_op(jit, JIT_PATCH | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0)

#define jit_prepare(jit) jit_add_op(jit, JIT_PREPARE, SPEC(IMM, IMM, NO), 0, 0, 0, 0)
#define jit_putargr(jit, a) jit_add_op(jit, JIT_PUTARG | REG, SPEC(REG, NO, NO), a, 0, 0, 0)
#define jit_putargi(jit, a) jit_add_op(jit, JIT_PUTARG | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0)
#define jit_call(jit, a) jit_add_op(jit, JIT_CALL | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0)
#define jit_callr(jit, a) jit_add_op(jit, JIT_CALL | REG, SPEC(REG, NO, NO), a, 0, 0, 0)

#define jit_declare_arg(jit, a, b) jit_add_op(jit, JIT_DECL_ARG, SPEC(IMM, IMM, NO), a, b, 0, 0)

#define jit_retr(jit, a) jit_add_op(jit, JIT_RET | REG, SPEC(REG, NO, NO), a, 0, 0, 0)
#define jit_reti(jit, a) jit_add_op(jit, JIT_RET | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0)
#define jit_retval(jit, a) jit_add_op(jit, JIT_RETVAL, SPEC(TREG, NO, NO), a, 0, 0, 0)

#define jit_getarg(jit, a, b) jit_add_op(jit, JIT_GETARG, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, 0)

/* arithmetics */

#define jit_addr(jit, a, b, c) jit_add_op(jit, JIT_ADD | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_addi(jit, a, b, c) jit_add_op(jit, JIT_ADD | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_addcr(jit, a, b, c) jit_add_op(jit, JIT_ADDC | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_addci(jit, a, b, c) jit_add_op(jit, JIT_ADDC | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)
#define jit_addxr(jit, a, b, c) jit_add_op(jit, JIT_ADDX | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_addxi(jit, a, b, c) jit_add_op(jit, JIT_ADDX | IMM, SPEC(TREG, REG, IMM), a, b, c, 0)

#define jit_subr(jit, a, b, c) jit_add_op(jit, JIT_SUB | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_subi(jit, a, b, c) jit_add_op(jit, JIT_SUB | IMM, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), 0)
#define jit_subcr(jit, a, b, c) jit_add_op(jit, JIT_SUBC | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_subci(jit, a, b, c) jit_add_op(jit, JIT_SUBC | IMM, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), 0)
#define jit_subxr(jit, a, b, c) jit_add_op(jit, JIT_SUBX | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_subxi(jit, a, b, c) jit_add_op(jit, JIT_SUBX | IMM, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), 0)

#define jit_rsbr(jit, a, b, c) jit_add_op(jit, JIT_RSB | REG, SPEC(TREG, REG, REG), a, b, c, 0)
#define jit_rsbi(jit, a, b, c) jit_add_op(jit, JIT_RSB | IMM, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), 0)

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

/* bitwise arithmetics */

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

#define jit_bltr(jit, a, b, c) jit_add_op(jit, JIT_BLT | REG | SIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0)
#define jit_blti(jit, a, b, c) jit_add_op(jit, JIT_BLT | IMM | SIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0)
#define jit_bltr_u(jit, a, b, c) jit_add_op(jit, JIT_BLT | REG | UNSIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0)
#define jit_blti_u(jit, a, b, c) jit_add_op(jit, JIT_BLT | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0)

#define jit_bler(jit, a, b, c) jit_add_op(jit, JIT_BLE | REG | SIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0)
#define jit_blei(jit, a, b, c) jit_add_op(jit, JIT_BLE | IMM | SIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0)
#define jit_bler_u(jit, a, b, c) jit_add_op(jit, JIT_BLE | REG | UNSIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0)
#define jit_blei_u(jit, a, b, c) jit_add_op(jit, JIT_BLE | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0)

#define jit_bgtr(jit, a, b, c) jit_add_op(jit, JIT_BGT | REG | SIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0)
#define jit_bgti(jit, a, b, c) jit_add_op(jit, JIT_BGT | IMM | SIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0)
#define jit_bgtr_u(jit, a, b, c) jit_add_op(jit, JIT_BGT | REG | UNSIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0)
#define jit_bgti_u(jit, a, b, c) jit_add_op(jit, JIT_BGT | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0)

#define jit_bger(jit, a, b, c) jit_add_op(jit, JIT_BGE | REG | SIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0)
#define jit_bgei(jit, a, b, c) jit_add_op(jit, JIT_BGE | IMM | SIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0)
#define jit_bger_u(jit, a, b, c) jit_add_op(jit, JIT_BGE | REG | UNSIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0)
#define jit_bgei_u(jit, a, b, c) jit_add_op(jit, JIT_BGE | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0)

#define jit_beqr(jit, a, b, c) jit_add_op(jit, JIT_BEQ | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0)
#define jit_beqi(jit, a, b, c) jit_add_op(jit, JIT_BEQ | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0)

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
#define jit_ldi(jit, a, b, c) jit_add_op(jit, JIT_LD | IMM | SIGNED, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, c) 
#define jit_ldxr(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, d) 
#define jit_ldxi(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), d) 
#define jit_ldr_u(jit, a, b, c) jit_add_op(jit, JIT_LD | REG | UNSIGNED, SPEC(TREG, REG, NO), a, b, 0, c) 
#define jit_ldi_u(jit, a, b, c) jit_add_op(jit, JIT_LD | IMM | UNSIGNED, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, c) 
#define jit_ldxr_u(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, d) 
#define jit_ldxi_u(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), d) 


#define jit_str(jit, a, b, c) jit_add_op(jit, JIT_ST | REG, SPEC(REG, REG, NO), a, b, 0, c) 
#define jit_sti(jit, a, b, c) jit_add_op(jit, JIT_ST | IMM, SPEC(IMM, REG, NO), (jit_value)(a), b, 0, c) 
#define jit_stxr(jit, a, b, c, d) jit_add_op(jit, JIT_STX | REG, SPEC(REG, REG, REG), a, b, c, d) 
#define jit_stxi(jit, a, b, c, d) jit_add_op(jit, JIT_STX | IMM, SPEC(IMM, REG, REG), (jit_value)(a), b, c, d) 

#define jit_msg(jit, a) jit_add_op(jit, JIT_MSG | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0)
#define jit_msgr(jit, a, b) jit_add_op(jit, JIT_MSG | REG, SPEC(IMM, REG, NO), (jit_value)(a), b, 0, 0)

/* FPU */

#define jit_fmovr(jit, a, b) jit_add_fop(jit, JIT_FMOV | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0)
#define jit_fmovi(jit, a, b) jit_add_fop(jit, JIT_FMOV | IMM, SPEC(TREG, IMM, NO), a, 0, 0, b, 0)

#define jit_faddr(jit, a, b, c) jit_add_fop(jit, JIT_FADD | REG, SPEC(TREG, REG, REG), a, b, c, 0, 0)
#define jit_faddi(jit, a, b, c) jit_add_fop(jit, JIT_FADD | IMM, SPEC(TREG, REG, IMM), a, b, 0, c, 0)
#define jit_fsubr(jit, a, b, c) jit_add_fop(jit, JIT_FSUB | REG, SPEC(TREG, REG, REG), a, b, c, 0, 0)
#define jit_fsubi(jit, a, b, c) jit_add_fop(jit, JIT_FSUB | IMM, SPEC(TREG, REG, IMM), a, b, 0, c, 0)
#define jit_frsbr(jit, a, b, c) jit_add_fop(jit, JIT_FRSB | REG, SPEC(TREG, REG, REG), a, b, c, 0, 0)
#define jit_frsbi(jit, a, b, c) jit_add_fop(jit, JIT_FRSB | IMM, SPEC(TREG, REG, IMM), a, b, 0, c, 0)
#define jit_fmulr(jit, a, b, c) jit_add_fop(jit, JIT_FMUL | REG, SPEC(TREG, REG, REG), a, b, c, 0, 0)
#define jit_fmuli(jit, a, b, c) jit_add_fop(jit, JIT_FMUL | IMM, SPEC(TREG, REG, IMM), a, b, 0, c, 0)
#define jit_fdivr(jit, a, b, c) jit_add_fop(jit, JIT_FDIV | REG, SPEC(TREG, REG, REG), a, b, c, 0, 0)
#define jit_fdivi(jit, a, b, c) jit_add_fop(jit, JIT_FDIV | IMM, SPEC(TREG, REG, IMM), a, b, 0, c, 0)

#define jit_fnegr(jit, a, b) jit_add_fop(jit, JIT_FNEG | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0)

#define jit_extr(jit, a, b) jit_add_fop(jit, JIT_EXT | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0)
#define jit_truncr(jit, a, b) jit_add_fop(jit, JIT_TRUNC | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0)
#define jit_floorr(jit, a, b) jit_add_fop(jit, JIT_FLOOR | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0)
#define jit_ceilr(jit, a, b) jit_add_fop(jit, JIT_CEIL | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0)
#define jit_roundr(jit, a, b) jit_add_fop(jit, JIT_ROUND | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0)

#define jit_fbltr(jit, a, b, c) jit_add_fop(jit, JIT_FBLT | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, 0)
#define jit_fblti(jit, a, b, c) jit_add_fop(jit, JIT_FBLT | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, 0, c, 0)
#define jit_fbgtr(jit, a, b, c) jit_add_fop(jit, JIT_FBGT | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, 0)
#define jit_fbgti(jit, a, b, c) jit_add_fop(jit, JIT_FBGT | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, 0, c, 0)

#define jit_fbler(jit, a, b, c) jit_add_fop(jit, JIT_FBLE | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, 0)
#define jit_fblei(jit, a, b, c) jit_add_fop(jit, JIT_FBLE | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, 0, c, 0)
#define jit_fbger(jit, a, b, c) jit_add_fop(jit, JIT_FBGE | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, 0)
#define jit_fbgei(jit, a, b, c) jit_add_fop(jit, JIT_FBGE | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, 0, c, 0)

#define jit_fbeqr(jit, a, b, c) jit_add_fop(jit, JIT_FBEQ | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, 0)
#define jit_fbeqi(jit, a, b, c) jit_add_fop(jit, JIT_FBEQ | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, 0, c, 0)

#define jit_fbner(jit, a, b, c) jit_add_fop(jit, JIT_FBNE | REG, SPEC(IMM, REG, REG), a, b, c, 0, 0)
#define jit_fbnei(jit, a, b, c) jit_add_fop(jit, JIT_FBNE | IMM, SPEC(IMM, REG, IMM), a, b, 0, c, 0)

#define jit_fstr(jit, a, b, c) jit_add_op(jit, JIT_FST | REG, SPEC(REG, REG, NO), a, b, 0, c) 
#define jit_fsti(jit, a, b, c) jit_add_op(jit, JIT_FST | IMM, SPEC(IMM, REG, NO), (jit_value)(a), b, 0, c) 
#define jit_fstxr(jit, a, b, c, d) jit_add_op(jit, JIT_FSTX | REG, SPEC(REG, REG, REG), a, b, c, d) 
#define jit_fstxi(jit, a, b, c, d) jit_add_op(jit, JIT_FSTX | IMM, SPEC(IMM, REG, REG), (jit_value)(a), b, c, d) 

#define jit_fldr(jit, a, b, c) jit_add_op(jit, JIT_FLD | REG, SPEC(TREG, REG, NO), a, b, 0, c) 
#define jit_fldi(jit, a, b, c) jit_add_op(jit, JIT_FLD | IMM, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, c) 
#define jit_fldxr(jit, a, b, c, d) jit_add_op(jit, JIT_FLDX | REG, SPEC(TREG, REG, REG), a, b, c, d) 
#define jit_fldxi(jit, a, b, c, d) jit_add_op(jit, JIT_FLDX | IMM, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), d) 

#define jit_fputargr(jit, a, b) jit_add_fop(jit, JIT_FPUTARG | REG, SPEC(REG, NO, NO), (a), 0, 0, 0, (b))
#define jit_fputargi(jit, a, b) jit_add_fop(jit, JIT_FPUTARG | IMM, SPEC(IMM, NO, NO), 0, 0, 0, (a), (b))

#define jit_fretr(jit, a, b) jit_add_fop(jit, JIT_FRET | REG, SPEC(REG, NO, NO), a, 0, 0, 0, b)
#define jit_freti(jit, a, b) jit_add_fop(jit, JIT_FRET | IMM, SPEC(IMM, NO, NO), 0, 0, 0, a, b)

#define jit_fretval(jit, a, b) jit_add_fop(jit, JIT_FRETVAL, SPEC(TREG, NO, NO), a, 0, 0, 0, b)

static struct jit_op * __new_op(unsigned short code, unsigned char spec, long arg1, long arg2, long arg3, unsigned char arg_size)
{
	struct jit_op * r = JIT_MALLOC(sizeof(struct jit_op));
	r->code = code;
	r->spec = spec;
	r->fp = 0;

	r->arg[0] = arg1;
	r->arg[1] = arg2;
	r->arg[2] = arg3;

	r->r_arg[0] = -1;
	r->r_arg[1] = -1;
	r->r_arg[2] = -1;

	r->assigned = 0;
	r->arg_size = arg_size;
	r->next = NULL;
	r->prev = NULL;
	r->patch_addr = 0;
	r->jmp_addr = NULL;
	r->regmap = NULL;
	r->live_in = NULL;
	r->live_out = NULL;
	r->allocator_hints = NULL;
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

static inline jit_label * jit_get_label(struct jit * jit)
{
	jit_label * r = JIT_MALLOC(sizeof(jit_label));
	jit_add_op(jit, JIT_LABEL, SPEC(IMM, NO, NO), (long)r, 0, 0, 0);
	r->next = jit->labels;
	jit->labels = r;
	return r;
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

static inline void jit_prolog(struct jit * jit, void * func)
{
	jit_op * op = jit_add_op(jit, JIT_PROLOG , SPEC(IMM, NO, NO), (long)func, 0, 0, 0);
	struct jit_func_info * info = JIT_MALLOC(sizeof(struct jit_func_info));
	op->arg[1] = (long)info;

	jit->current_func = op;

	info->allocai_mem = 0;
	info->general_arg_cnt = 0;
	info->float_arg_cnt = 0;
}

static inline struct jit_func_info * jit_current_func_info(struct jit * jit)
{
	return (struct jit_func_info *)(jit->current_func->arg[1]);
}

static inline void __prepare_call(struct jit * jit, jit_op * op, int count)
{
	jit->prepared_args.args = JIT_MALLOC(sizeof(struct jit_out_arg) * count);
	jit->prepared_args.count = count;
	jit->prepared_args.ready = 0;
	jit->prepared_args.stack_size = 0;
	jit->prepared_args.op = op;
	jit->prepared_args.gp_args = 0;
	jit->prepared_args.fp_args = 0;
}

static inline void __put_arg(struct jit * jit, jit_op * op)
{
	int pos = jit->prepared_args.ready;
	struct jit_out_arg * arg = &(jit->prepared_args.args[pos]);
	arg->isreg = !IS_IMM(op);
	arg->isfp = 0;
	arg->value.generic = op->arg[0];
	arg->argpos = jit->prepared_args.gp_args++;
	jit->prepared_args.ready++;

	if (jit->prepared_args.ready >= jit->reg_al->gp_arg_reg_cnt)
		jit->prepared_args.stack_size += REG_SIZE;
}

static inline void __fput_arg(struct jit * jit, jit_op * op)
{
	int pos = jit->prepared_args.ready;
	struct jit_out_arg * arg = &(jit->prepared_args.args[pos]);
	arg->isreg = !IS_IMM(op);
	arg->isfp = 1;
	arg->size = op->arg_size;
	arg->argpos = jit->prepared_args.fp_args++;
	if (IS_IMM(op)) arg->value.fp = op->flt_imm;
	else arg->value.generic = op->arg[0];
	jit->prepared_args.ready++;

	if (jit->prepared_args.ready >= jit->reg_al->fp_arg_reg_cnt)
		jit->prepared_args.stack_size += op->arg_size;
}

/**
 * Returns number which is a multiple of the alignment and is the closest
 * greater value than @param value
 */
static inline jit_value jit_value_align(jit_value value, jit_value alignment)
{
	return (value + (alignment - 1)) & (- alignment);
}
#endif
