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


#ifndef JITLIB_CORE_H
#define JITLIB_CORE_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include "jitlib.h"
#include "llrb.c"


#define FR_IMM	(jit_mkreg(JIT_RTYPE_FLOAT, JIT_RTYPE_IMM, 0))
#define R_IMM	(jit_mkreg(JIT_RTYPE_INT, JIT_RTYPE_IMM, 0)) // used by amd64 and sparc

#define GET_OP(op) ((jit_opcode) (op->code & 0xfff8))
#define GET_OP_SUFFIX(op) (op->code & 0x0007)
#define IS_IMM(op) (op->code & IMM)
#define IS_SIGNED(op) (!(op->code & UNSIGNED))
#define ARG_TYPE(op, arg) (((op)->spec >> ((arg) - 1) * 2) & 0x03)

#define JIT_BUFFER_OFFSET(jit)       ((jit_value)jit->ip - (jit_value)jit->buf)


#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif


typedef struct {
	int id;
	char * name;
	char callee_saved;
	char fp;
	short priority;
} jit_hw_reg;

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

typedef struct jit_rmap {
	jit_tree * map;		// R/B tree which maps virtual registers to hardware registers
} jit_rmap;

struct jit_allocator_hint {
	int last_pos;
	int should_be_calleesaved;
	int should_be_eax;
	int refs;			// counts number of references to the hint
};

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

typedef struct jit_set {
	jit_tree * root;
} jit_set;

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
	int has_prolog;			// flag indicating if the function has a complete prologue and epilogue
	struct jit_op *first_op;	// first operation of the function
};

struct jit {
	unsigned char * buf; 		// buffer used to store generated code
	unsigned int buf_capacity; 	// its capacity

	unsigned char * ip;		// pointer to the buffer

	struct jit_op * ops;		// list of operations
	struct jit_op * last_op;	// last operation
	struct jit_reg_allocator * reg_al; // register allocatot
	struct jit_op * current_func;	// pointer to the PROLOG operation of the currently processed function
	jit_label * labels;		// list of labels used in the c
	jit_prepared_args prepared_args; // list of arguments passed between PREPARE-CALL
	int push_count;			// number of values pushed on the stack; used by AMD64
	unsigned int optimizations;
};

struct jit_debug_info {
        const char *filename;
	const char *function;
        int lineno;
        int warnings;
};

//void jit_get_reg_name(char * r, int reg);
void jit_patch_external_calls(struct jit * jit);
void jit_patch_local_addrs(struct jit *jit);

void jit_collect_statistics(struct jit * jit);

void jit_optimize_st_ops(struct jit * jit);
int jit_optimize_join_addmul(struct jit * jit);
int jit_optimize_join_addimm(struct jit * jit);
void jit_optimize_frame_ptr(struct jit * jit);
void jit_optimize_unused_assignments(struct jit * jit);
static int is_cond_branch_op(jit_op *op); // FIXME: rename to: jit_op_is_cond_branch
static inline void jit_set_free(jit_set * s);

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
void rmap_free(jit_rmap * regmap);
void jit_allocator_hints_free(jit_tree *);


static struct jit_op * jit_op_new(unsigned short code, unsigned char spec, long arg1, long arg2, long arg3, unsigned char arg_size)
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
	r->debug_info = NULL;
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

static inline void jit_free_op(struct jit_op *op)
{
        if (op->live_in) jit_set_free(op->live_in);
        if (op->live_out) jit_set_free(op->live_out);
        rmap_free(op->regmap);
        jit_allocator_hints_free(op->allocator_hints);
	if (op->debug_info) JIT_FREE(op->debug_info);

        if (GET_OP(op) == JIT_PROLOG) {
                struct jit_func_info * info = (struct jit_func_info *)op->arg[1];
                JIT_FREE(info->args);
                JIT_FREE(info);
        }

        JIT_FREE(op);
}

static inline void jit_op_delete(jit_op *op)
{
	op->prev->next = op->next;
	if (op->next) op->next->prev = op->prev;
	jit_free_op(op);
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

static inline struct jit_func_info * jit_current_func_info(struct jit * jit)
{
	return (struct jit_func_info *)(jit->current_func->arg[1]);
}

static inline void funcall_prepare(struct jit * jit, jit_op * op, int count)
{
	jit->prepared_args.args = JIT_MALLOC(sizeof(struct jit_out_arg) * count);
	jit->prepared_args.count = count;
	jit->prepared_args.ready = 0;
	jit->prepared_args.stack_size = 0;
	jit->prepared_args.op = op;
	jit->prepared_args.gp_args = 0;
	jit->prepared_args.fp_args = 0;
}

static inline void funcall_put_arg(struct jit * jit, jit_op * op)
{
	int pos = jit->prepared_args.ready;
	struct jit_out_arg * arg = &(jit->prepared_args.args[pos]);
	arg->isreg = !IS_IMM(op);
	arg->isfp = 0;
	arg->value.generic = op->arg[0];
	arg->argpos = jit->prepared_args.gp_args++;
	jit->prepared_args.ready++;

	if (jit->prepared_args.ready > jit->reg_al->gp_arg_reg_cnt)
		jit->prepared_args.stack_size += REG_SIZE;
}

static inline void funcall_fput_arg(struct jit * jit, jit_op * op)
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

	if (jit->prepared_args.ready > jit->reg_al->fp_arg_reg_cnt) {
#ifdef JIT_ARCH_AMD64
		jit->prepared_args.stack_size += REG_SIZE;
#else
		jit->prepared_args.stack_size += op->arg_size;
#endif
	}
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
