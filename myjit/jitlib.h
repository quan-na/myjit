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

#ifndef JITLIB_H
#define JITLIB_H

#define _XOPEN_SOURCE 600

#include <stdint.h>
#include <string.h> // FIXME: get rid of memcpy 
#include "cpu-detect.h"

/*
 * Memory management
 */

#ifndef JIT_MALLOC
#define JIT_MALLOC	malloc
#endif

#ifndef JIT_REALLOC
#define JIT_REALLOC	realloc
#endif

#ifndef JIT_FREE
#define JIT_FREE	free
#endif

void *JIT_MALLOC(size_t size);
void JIT_FREE(void *ptr);
void *JIT_REALLOC(void *ptr, size_t size);

/*
 * Data structures
 */
struct jit_tree;
struct jit_set;
struct jit_rmap;
struct jit_debug_info;

typedef struct jit_op {
        unsigned short code;            // operation code
        unsigned char spec;             // argument types, e.g REG+REG+IMM
        unsigned char arg_size;         // used by ld, st
        unsigned char assigned;
        unsigned char fp;               // FP if it's a floating-point operation        
	unsigned char in_use;		// used be dead-code analyzer
        double flt_imm;                 // floating point immediate value
        jit_value arg[3];               // arguments passed by user
        jit_value r_arg[3];             // arguments transformed by register allocator
        long patch_addr;
        struct jit_op * jmp_addr;
        struct jit_op * next;
        struct jit_op * prev;
        struct jit_set * live_in;
        struct jit_set * live_out;
        struct jit_rmap * regmap;                // register mappings 
        int normalized_pos;             // number of operations from the end of the function
        struct jit_tree * allocator_hints; // reg. allocator to collect statistics on used registers
	struct jit_debug_info *debug_info;
	unsigned long code_offset;			// offset in the output buffer
	unsigned long code_length;			// offset in the output buffer
} jit_op;

typedef struct jit_label {
        long pos;
        jit_op * op;
        struct jit_label * next;
} jit_label;

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


/*
 * internal auxiliary functions 
 */

// FIXME: replace memcpy with union 
static inline jit_value JIT_REG_TO_JIT_VALUE(jit_reg r)
{
        jit_value v;
        memcpy(&v, &r, sizeof(jit_reg));
        return v;
}

static inline jit_reg JIT_REG(jit_value v)
{
        jit_reg r;
        memcpy(&r, &v, sizeof(jit_value));
        return r;
}

static inline jit_value jit_mkreg(int type, int spec, int id)
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

static inline jit_value jit_mkreg_ex(int type, int spec, int id)
{
	jit_reg r;
	r.type = type;
	r.spec = spec;
	r.id = id;
	r.part = 1;
#ifdef JIT_ARCH_AMD64
	r.reserved = 0;
#endif
	return JIT_REG_TO_JIT_VALUE(r);
}


/*
 * Registers
 */

#define JIT_RTYPE_REG   (0)
#define JIT_RTYPE_IMM   (1)
#define JIT_RTYPE_ALIAS (2)
#define JIT_RTYPE_ARG   (3)

#define JIT_RTYPE_INT   (0)
#define JIT_RTYPE_FLOAT (1)

#define R(x) (jit_mkreg(JIT_RTYPE_INT, JIT_RTYPE_REG, (x)))
#define FR(x) (jit_mkreg(JIT_RTYPE_FLOAT, JIT_RTYPE_REG, (x)))

#define R_FP    (jit_mkreg(JIT_RTYPE_INT, JIT_RTYPE_ALIAS, 0))
#define R_OUT   (jit_mkreg(JIT_RTYPE_INT, JIT_RTYPE_ALIAS, 1))

#define JIT_FORWARD    (NULL)

/*
 * Opcodes
 */
typedef enum {
	JIT_NOP		= (0x00 << 3),
	JIT_CODESTART	= (0x01 << 3),
	JIT_UREG	= (0x02 << 3),
	JIT_LREG	= (0x03 << 3),
	JIT_SYNCREG	= (0x04 << 3),
	JIT_RENAMEREG	= (0x05 << 3), 
	JIT_FULL_SPILL	= (0x06 << 3),

	JIT_PROLOG	= (0x10 << 3),
	JIT_LABEL	= (0x11 << 3),
	JIT_PATCH 	= (0x12 << 3),
	JIT_DECL_ARG	= (0x13 << 3),
	JIT_ALLOCA	= (0x14 << 3),

	JIT_MOV 	= (0x20 << 3),
	JIT_LD		= (0x21 << 3),
	JIT_LDX		= (0x22 << 3),
	JIT_ST		= (0x23 << 3),
	JIT_STX		= (0x24 << 3),

	JIT_JMP 	= (0x30 << 3),
	JIT_PREPARE 	= (0x31 << 3),
	JIT_PUTARG 	= (0x32 << 3),
	JIT_FPUTARG	= (0x33 << 3),
	JIT_CALL 	= (0x34 << 3),
	JIT_RET		= (0x35 << 3),
	JIT_GETARG	= (0x36 << 3),
	JIT_RETVAL	= (0x37 << 3),
	JIT_FRETVAL	= (0x88 << 3),

	JIT_ADD 	= (0x40 << 3),
	JIT_ADDC	= (0x41 << 3),
	JIT_ADDX	= (0x42 << 3),
	JIT_SUB		= (0x43 << 3),
	JIT_SUBC	= (0x44 << 3),
	JIT_SUBX	= (0x45 << 3),
	JIT_RSB		= (0x46 << 3),
	JIT_NEG 	= (0x47 << 3),
	JIT_MUL		= (0x48 << 3),
	JIT_HMUL	= (0x49 << 3),
	JIT_DIV		= (0x4a << 3),
	JIT_MOD		= (0x4b << 3),

	JIT_OR	 	= (0x50 << 3),
	JIT_XOR 	= (0x51 << 3),
	JIT_AND		= (0x52 << 3),
	JIT_LSH		= (0x53 << 3),
	JIT_RSH		= (0x54 << 3),
	JIT_NOT		= (0x55 << 3),

	JIT_LT		= (0x60 << 3),
	JIT_LE		= (0x61 << 3),
	JIT_GT		= (0x62 << 3),
	JIT_GE		= (0x63 << 3),
	JIT_EQ		= (0x64 << 3),
	JIT_NE		= (0x65 << 3),

	JIT_BLT 	= (0x70 << 3),
	JIT_BLE		= (0x71 << 3),
	JIT_BGT		= (0x72 << 3),
	JIT_BGE		= (0x73 << 3),
	JIT_BEQ		= (0x74 << 3),
	JIT_BNE		= (0x75 << 3),
	JIT_BMS		= (0x76 << 3),	
	JIT_BMC		= (0x77 << 3),	
	JIT_BOADD	= (0x78 << 3),	
	JIT_BOSUB	= (0x79 << 3),	
	JIT_BNOADD	= (0x7a << 3),	
	JIT_BNOSUB	= (0x7b << 3),	

	JIT_FMOV	= (0x80 << 3),
	JIT_FADD	= (0x81 << 3),
	JIT_FSUB	= (0x82 << 3),
	JIT_FRSB 	= (0x83 << 3),
	JIT_FMUL	= (0x84 << 3),
	JIT_FDIV	= (0x85 << 3),
	JIT_FNEG	= (0x86 << 3),

	JIT_EXT		= (0x89 << 3),
	JIT_ROUND	= (0x8a << 3),
	JIT_TRUNC	= (0x8b << 3),
	JIT_FLOOR	= (0x8c << 3),
	JIT_CEIL	= (0x8d << 3),

	JIT_FBLT 	= (0x90 << 3),
	JIT_FBLE	= (0x91 << 3),
	JIT_FBGT	= (0x92 << 3),
	JIT_FBGE	= (0x93 << 3),
	JIT_FBEQ	= (0x94 << 3),
	JIT_FBNE	= (0x95 << 3),

	JIT_FLD		= (0xa0 << 3),
	JIT_FLDX	= (0xa1 << 3),
	JIT_FST		= (0xa2 << 3),
	JIT_FSTX	= (0xa3 << 3),

	JIT_FRET	= (0xa5 << 3),

	JIT_DATA_BYTE	= (0xb0 << 3),
	JIT_DATA_REF_CODE  = (0xb1 << 3),
	JIT_DATA_REF_DATA  = (0xb2 << 3),
	JIT_CODE_ALIGN	= (0xb3 << 3),
	JIT_REF_CODE	= (0xb4 << 3),
	JIT_REF_DATA	= (0xb5 << 3),

	JIT_MSG		= (0xf0 << 3),
	JIT_COMMENT	= (0xf1 << 3),

	// platform specific opcodes, for optimization purposes only
	JIT_X86_STI     = (0x0100 << 3),
	JIT_X86_STXI    = (0x0101 << 3),
	JIT_X86_ADDMUL  = (0x0102 << 3),
	JIT_X86_ADDIMM  = (0x0103 << 3),

	// opcodes for testing and debugging purposes only
	JIT_FORCE_SPILL	= (0x0200 << 3),
	JIT_FORCE_ASSOC = (0x0201 << 3)
} jit_opcode;

enum jit_inp_type {
	JIT_SIGNED_NUM,
	JIT_UNSIGNED_NUM,
	JIT_FLOAT_NUM,
	JIT_PTR
};

enum jit_warning {
	JIT_WARN_DEAD_CODE 		= 0x00000001,
	JIT_WARN_OP_WITHOUT_EFFECT 	= 0x00000002,
	JIT_WARN_INVALID_DATA_SIZE	= 0x00000004,
	JIT_WARN_UNINITIALIZED_REG	= 0x00000008,
	JIT_WARN_UNALIGNED_CODE		= 0x00000010,
	JIT_WARN_INVALID_CODE_REFERENCE = 0x00000020,
	JIT_WARN_INVALID_DATA_REFERENCE = 0x00000040,
	JIT_WARN_MISSING_PATCH		= 0x00000080,
	JIT_WARN_REGISTER_TYPE_MISMATCH	= 0x00000100,
	JIT_WARN_ALL			= 0x7fffffff
};


/*
 * Public interface
 */

#define JIT_DEBUG_OPS		(0x01)
#define JIT_DEBUG_CODE		(0x02)
#define JIT_DEBUG_COMBINED	(0x04)
#define JIT_DEBUG_COMPILABLE	(0x08)


#define JIT_DEBUG_LOADS         (0x100)
#define JIT_DEBUG_ASSOC         (0x200)
#define JIT_DEBUG_LIVENESS      (0x400)

#define JIT_OPT_OMIT_FRAME_PTR                  (0x01)
#define JIT_OPT_OMIT_UNUSED_ASSIGNEMENTS        (0x02)
#define JIT_OPT_JOIN_ADDMUL                     (0x04)
#define JIT_OPT_ALL                             (0xff)

struct jit * jit_init();
struct jit_op * jit_add_op(struct jit * jit, unsigned short code, unsigned char spec, jit_value arg1, jit_value arg2, jit_value arg3, unsigned char arg_sizee, struct jit_debug_info *debug_info);
struct jit_op * jit_add_fop(struct jit * jit, unsigned short code, unsigned char spec, jit_value arg1, jit_value arg2, jit_value arg3, double flt_imm, unsigned char arg_sizee, struct jit_debug_info *debug_info);
struct jit_debug_info *jit_debug_info_new(const char *filename, const char *function, int lineno);
void jit_generate_code(struct jit * jit);
void jit_free(struct jit * jit);

void jit_dump_ops(struct jit * jit, int verbosity);
void jit_check_code(struct jit *jit, int warnings);

void jit_enable_optimization(struct jit * jit, int opt);
void jit_disable_optimization(struct jit * jit, int opt);

#define NO  0x00
#define REG 0x01
#define IMM 0x02
#define TREG 0x03 /* target register */
#define SPEC(ARG1, ARG2, ARG3) (((ARG3) << 4) | ((ARG2) << 2) | (ARG1))
#define UNSIGNED 0x04
#define SIGNED 0x00

jit_op * jit_add_prolog(struct jit *, void *, struct jit_debug_info *);
jit_label * jit_get_label(struct jit * jit);
int jit_allocai(struct jit * jit, int size);


#define jit_prolog(jit, _func) jit_add_prolog(jit, _func, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_movr(jit, a, b) jit_add_op(jit, JIT_MOV | REG, SPEC(TREG, REG, NO), a, b, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_movi(jit, a, b) jit_add_op(jit, JIT_MOV | IMM, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

/* functions, call, jumps, etc. */

#define jit_jmpr(jit, a) jit_add_op(jit, JIT_JMP | REG, SPEC(REG, NO, NO), a, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_jmpi(jit, a) jit_add_op(jit, JIT_JMP | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_patch(jit, a) jit_add_op(jit, JIT_PATCH | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_prepare(jit) jit_add_op(jit, JIT_PREPARE, SPEC(IMM, IMM, NO), 0, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_putargr(jit, a) jit_add_op(jit, JIT_PUTARG | REG, SPEC(REG, NO, NO), a, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_putargi(jit, a) jit_add_op(jit, JIT_PUTARG | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_call(jit, a) jit_add_op(jit, JIT_CALL | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_callr(jit, a) jit_add_op(jit, JIT_CALL | REG, SPEC(REG, NO, NO), a, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_declare_arg(jit, a, b) jit_add_op(jit, JIT_DECL_ARG, SPEC(IMM, IMM, NO), a, b, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_retr(jit, a) jit_add_op(jit, JIT_RET | REG, SPEC(REG, NO, NO), a, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_reti(jit, a) jit_add_op(jit, JIT_RET | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_retval(jit, a) jit_add_op(jit, JIT_RETVAL, SPEC(TREG, NO, NO), a, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_getarg(jit, a, b) jit_add_op(jit, JIT_GETARG, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

/* arithmetics */

#define jit_addr(jit, a, b, c) jit_add_op(jit, JIT_ADD | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_addi(jit, a, b, c) jit_add_op(jit, JIT_ADD | IMM, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_addcr(jit, a, b, c) jit_add_op(jit, JIT_ADDC | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_addci(jit, a, b, c) jit_add_op(jit, JIT_ADDC | IMM, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_addxr(jit, a, b, c) jit_add_op(jit, JIT_ADDX | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_addxi(jit, a, b, c) jit_add_op(jit, JIT_ADDX | IMM, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_subr(jit, a, b, c) jit_add_op(jit, JIT_SUB | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_subi(jit, a, b, c) jit_add_op(jit, JIT_SUB | IMM, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_subcr(jit, a, b, c) jit_add_op(jit, JIT_SUBC | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_subci(jit, a, b, c) jit_add_op(jit, JIT_SUBC | IMM, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_subxr(jit, a, b, c) jit_add_op(jit, JIT_SUBX | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_subxi(jit, a, b, c) jit_add_op(jit, JIT_SUBX | IMM, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_rsbr(jit, a, b, c) jit_add_op(jit, JIT_RSB | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_rsbi(jit, a, b, c) jit_add_op(jit, JIT_RSB | IMM, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_negr(jit, a, b) jit_add_op(jit, JIT_NEG, SPEC(TREG, REG, NO), a, b, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_mulr(jit, a, b, c) jit_add_op(jit, JIT_MUL | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_muli(jit, a, b, c) jit_add_op(jit, JIT_MUL | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_mulr_u(jit, a, b, c) jit_add_op(jit, JIT_MUL | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_muli_u(jit, a, b, c) jit_add_op(jit, JIT_MUL | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_hmulr(jit, a, b, c) jit_add_op(jit, JIT_HMUL | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_hmuli(jit, a, b, c) jit_add_op(jit, JIT_HMUL | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_hmulr_u(jit, a, b, c) jit_add_op(jit, JIT_HMUL | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_hmuli_u(jit, a, b, c) jit_add_op(jit, JIT_HMUL | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_divr(jit, a, b, c) jit_add_op(jit, JIT_DIV | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_divi(jit, a, b, c) jit_add_op(jit, JIT_DIV | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_divr_u(jit, a, b, c) jit_add_op(jit, JIT_DIV | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_divi_u(jit, a, b, c) jit_add_op(jit, JIT_DIV | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_modr(jit, a, b, c) jit_add_op(jit, JIT_MOD | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_modi(jit, a, b, c) jit_add_op(jit, JIT_MOD | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_modr_u(jit, a, b, c) jit_add_op(jit, JIT_MOD | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_modi_u(jit, a, b, c) jit_add_op(jit, JIT_MOD | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

/* bitwise arithmetics */

#define jit_orr(jit, a, b, c) jit_add_op(jit, JIT_OR | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ori(jit, a, b, c) jit_add_op(jit, JIT_OR | IMM, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_xorr(jit, a, b, c) jit_add_op(jit, JIT_XOR | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_xori(jit, a, b, c) jit_add_op(jit, JIT_XOR | IMM, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_andr(jit, a, b, c) jit_add_op(jit, JIT_AND | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_andi(jit, a, b, c) jit_add_op(jit, JIT_AND | IMM, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_lshr(jit, a, b, c) jit_add_op(jit, JIT_LSH | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_lshi(jit, a, b, c) jit_add_op(jit, JIT_LSH | IMM, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_rshr(jit, a, b, c) jit_add_op(jit, JIT_RSH | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_rshi(jit, a, b, c) jit_add_op(jit, JIT_RSH | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_rshr_u(jit, a, b, c) jit_add_op(jit, JIT_RSH | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_rshi_u(jit, a, b, c) jit_add_op(jit, JIT_RSH | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_notr(jit, a, b) jit_add_op(jit, JIT_NOT, SPEC(TREG, REG, NO), a, b, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

/* branches */

#define jit_bltr(jit, a, b, c) jit_add_op(jit, JIT_BLT | REG | SIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_blti(jit, a, b, c) jit_add_op(jit, JIT_BLT | IMM | SIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bltr_u(jit, a, b, c) jit_add_op(jit, JIT_BLT | REG | UNSIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_blti_u(jit, a, b, c) jit_add_op(jit, JIT_BLT | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_bler(jit, a, b, c) jit_add_op(jit, JIT_BLE | REG | SIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_blei(jit, a, b, c) jit_add_op(jit, JIT_BLE | IMM | SIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bler_u(jit, a, b, c) jit_add_op(jit, JIT_BLE | REG | UNSIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_blei_u(jit, a, b, c) jit_add_op(jit, JIT_BLE | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_bgtr(jit, a, b, c) jit_add_op(jit, JIT_BGT | REG | SIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bgti(jit, a, b, c) jit_add_op(jit, JIT_BGT | IMM | SIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bgtr_u(jit, a, b, c) jit_add_op(jit, JIT_BGT | REG | UNSIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bgti_u(jit, a, b, c) jit_add_op(jit, JIT_BGT | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_bger(jit, a, b, c) jit_add_op(jit, JIT_BGE | REG | SIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bgei(jit, a, b, c) jit_add_op(jit, JIT_BGE | IMM | SIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bger_u(jit, a, b, c) jit_add_op(jit, JIT_BGE | REG | UNSIGNED, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bgei_u(jit, a, b, c) jit_add_op(jit, JIT_BGE | IMM | UNSIGNED, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_beqr(jit, a, b, c) jit_add_op(jit, JIT_BEQ | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_beqi(jit, a, b, c) jit_add_op(jit, JIT_BEQ | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_bner(jit, a, b, c) jit_add_op(jit, JIT_BNE | REG, SPEC(IMM, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bnei(jit, a, b, c) jit_add_op(jit, JIT_BNE | IMM, SPEC(IMM, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_bmsr(jit, a, b, c) jit_add_op(jit, JIT_BMS | REG, SPEC(IMM, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bmsi(jit, a, b, c) jit_add_op(jit, JIT_BMS | IMM, SPEC(IMM, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_bmcr(jit, a, b, c) jit_add_op(jit, JIT_BMC | REG, SPEC(IMM, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bmci(jit, a, b, c) jit_add_op(jit, JIT_BMC | IMM, SPEC(IMM, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_boaddr(jit, a, b, c) jit_add_op(jit, JIT_BOADD | REG | SIGNED, SPEC(IMM, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_boaddi(jit, a, b, c) jit_add_op(jit, JIT_BOADD | IMM | SIGNED, SPEC(IMM, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_bosubr(jit, a, b, c) jit_add_op(jit, JIT_BOSUB | REG | SIGNED, SPEC(IMM, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bosubi(jit, a, b, c) jit_add_op(jit, JIT_BOSUB | IMM | SIGNED, SPEC(IMM, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_bnoaddr(jit, a, b, c) jit_add_op(jit, JIT_BNOADD | REG | SIGNED, SPEC(IMM, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bnoaddi(jit, a, b, c) jit_add_op(jit, JIT_BNOADD | IMM | SIGNED, SPEC(IMM, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_bnosubr(jit, a, b, c) jit_add_op(jit, JIT_BNOSUB | REG | SIGNED, SPEC(IMM, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_bnosubi(jit, a, b, c) jit_add_op(jit, JIT_BNOSUB | IMM | SIGNED, SPEC(IMM, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

/* conditions */

#define jit_ltr(jit, a, b, c) jit_add_op(jit, JIT_LT | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_lti(jit, a, b, c) jit_add_op(jit, JIT_LT | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ltr_u(jit, a, b, c) jit_add_op(jit, JIT_LT | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_lti_u(jit, a, b, c) jit_add_op(jit, JIT_LT | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_ler(jit, a, b, c) jit_add_op(jit, JIT_LE | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_lei(jit, a, b, c) jit_add_op(jit, JIT_LE | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ler_u(jit, a, b, c) jit_add_op(jit, JIT_LE | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_lei_u(jit, a, b, c) jit_add_op(jit, JIT_LE | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_gtr(jit, a, b, c) jit_add_op(jit, JIT_GT | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_gti(jit, a, b, c) jit_add_op(jit, JIT_GT | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_gtr_u(jit, a, b, c) jit_add_op(jit, JIT_GT | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_gti_u(jit, a, b, c) jit_add_op(jit, JIT_GT | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_ger(jit, a, b, c) jit_add_op(jit, JIT_GE | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_gei(jit, a, b, c) jit_add_op(jit, JIT_GE | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ger_u(jit, a, b, c) jit_add_op(jit, JIT_GE | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_gei_u(jit, a, b, c) jit_add_op(jit, JIT_GE | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_eqr(jit, a, b, c) jit_add_op(jit, JIT_EQ | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_eqi(jit, a, b, c) jit_add_op(jit, JIT_EQ | IMM, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_ner(jit, a, b, c) jit_add_op(jit, JIT_NE | REG, SPEC(TREG, REG, REG), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_nei(jit, a, b, c) jit_add_op(jit, JIT_NE | IMM, SPEC(TREG, REG, IMM), a, b, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

/* memory operations */

#define jit_ldr(jit, a, b, c) jit_add_op(jit, JIT_LD | REG | SIGNED, SPEC(TREG, REG, NO), a, b, 0, c, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ldi(jit, a, b, c) jit_add_op(jit, JIT_LD | IMM | SIGNED, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, c, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ldxr(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | REG | SIGNED, SPEC(TREG, REG, REG), a, b, c, d, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ldxi(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | IMM | SIGNED, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), d, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ldr_u(jit, a, b, c) jit_add_op(jit, JIT_LD | REG | UNSIGNED, SPEC(TREG, REG, NO), a, b, 0, c, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ldi_u(jit, a, b, c) jit_add_op(jit, JIT_LD | IMM | UNSIGNED, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, c, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ldxr_u(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | REG | UNSIGNED, SPEC(TREG, REG, REG), a, b, c, d, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ldxi_u(jit, a, b, c, d) jit_add_op(jit, JIT_LDX | IMM | UNSIGNED, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), d, jit_debug_info_new(__FILE__, __func__, __LINE__))


#define jit_str(jit, a, b, c) jit_add_op(jit, JIT_ST | REG, SPEC(REG, REG, NO), a, b, 0, c, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_sti(jit, a, b, c) jit_add_op(jit, JIT_ST | IMM, SPEC(IMM, REG, NO), (jit_value)(a), b, 0, c, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_stxr(jit, a, b, c, d) jit_add_op(jit, JIT_STX | REG, SPEC(REG, REG, REG), a, b, c, d, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_stxi(jit, a, b, c, d) jit_add_op(jit, JIT_STX | IMM, SPEC(IMM, REG, REG), (jit_value)(a), b, c, d, jit_debug_info_new(__FILE__, __func__, __LINE__))

/* debugging */

#define jit_msg(jit, a) jit_add_op(jit, JIT_MSG | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_msgr(jit, a, b) jit_add_op(jit, JIT_MSG | REG, SPEC(IMM, REG, NO), (jit_value)(a), b, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_comment(jit, a) jit_add_op(jit, JIT_COMMENT, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

/* FPU */

#define jit_fmovr(jit, a, b) jit_add_fop(jit, JIT_FMOV | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fmovi(jit, a, b) jit_add_fop(jit, JIT_FMOV | IMM, SPEC(TREG, IMM, NO), a, 0, 0, b, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_faddr(jit, a, b, c) jit_add_fop(jit, JIT_FADD | REG, SPEC(TREG, REG, REG), a, b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_faddi(jit, a, b, c) jit_add_fop(jit, JIT_FADD | IMM, SPEC(TREG, REG, IMM), a, b, 0, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fsubr(jit, a, b, c) jit_add_fop(jit, JIT_FSUB | REG, SPEC(TREG, REG, REG), a, b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fsubi(jit, a, b, c) jit_add_fop(jit, JIT_FSUB | IMM, SPEC(TREG, REG, IMM), a, b, 0, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_frsbr(jit, a, b, c) jit_add_fop(jit, JIT_FRSB | REG, SPEC(TREG, REG, REG), a, b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_frsbi(jit, a, b, c) jit_add_fop(jit, JIT_FRSB | IMM, SPEC(TREG, REG, IMM), a, b, 0, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fmulr(jit, a, b, c) jit_add_fop(jit, JIT_FMUL | REG, SPEC(TREG, REG, REG), a, b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fmuli(jit, a, b, c) jit_add_fop(jit, JIT_FMUL | IMM, SPEC(TREG, REG, IMM), a, b, 0, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fdivr(jit, a, b, c) jit_add_fop(jit, JIT_FDIV | REG, SPEC(TREG, REG, REG), a, b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fdivi(jit, a, b, c) jit_add_fop(jit, JIT_FDIV | IMM, SPEC(TREG, REG, IMM), a, b, 0, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_fnegr(jit, a, b) jit_add_fop(jit, JIT_FNEG | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_extr(jit, a, b) jit_add_fop(jit, JIT_EXT | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_truncr(jit, a, b) jit_add_fop(jit, JIT_TRUNC | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_floorr(jit, a, b) jit_add_fop(jit, JIT_FLOOR | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ceilr(jit, a, b) jit_add_fop(jit, JIT_CEIL | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_roundr(jit, a, b) jit_add_fop(jit, JIT_ROUND | REG, SPEC(TREG, REG, NO), a, b, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_fbltr(jit, a, b, c) jit_add_fop(jit, JIT_FBLT | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fblti(jit, a, b, c) jit_add_fop(jit, JIT_FBLT | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, 0, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fbgtr(jit, a, b, c) jit_add_fop(jit, JIT_FBGT | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fbgti(jit, a, b, c) jit_add_fop(jit, JIT_FBGT | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, 0, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_fbler(jit, a, b, c) jit_add_fop(jit, JIT_FBLE | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fblei(jit, a, b, c) jit_add_fop(jit, JIT_FBLE | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, 0, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fbger(jit, a, b, c) jit_add_fop(jit, JIT_FBGE | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fbgei(jit, a, b, c) jit_add_fop(jit, JIT_FBGE | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, 0, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_fbeqr(jit, a, b, c) jit_add_fop(jit, JIT_FBEQ | REG, SPEC(IMM, REG, REG), (jit_value)(a), b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fbeqi(jit, a, b, c) jit_add_fop(jit, JIT_FBEQ | IMM, SPEC(IMM, REG, IMM), (jit_value)(a), b, 0, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_fbner(jit, a, b, c) jit_add_fop(jit, JIT_FBNE | REG, SPEC(IMM, REG, REG), a, b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fbnei(jit, a, b, c) jit_add_fop(jit, JIT_FBNE | IMM, SPEC(IMM, REG, IMM), a, b, 0, c, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_fstr(jit, a, b, c) jit_add_op(jit, JIT_FST | REG, SPEC(REG, REG, NO), a, b, 0, c, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fsti(jit, a, b, c) jit_add_op(jit, JIT_FST | IMM, SPEC(IMM, REG, NO), (jit_value)(a), b, 0, c, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fstxr(jit, a, b, c, d) jit_add_op(jit, JIT_FSTX | REG, SPEC(REG, REG, REG), a, b, c, d, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fstxi(jit, a, b, c, d) jit_add_op(jit, JIT_FSTX | IMM, SPEC(IMM, REG, REG), (jit_value)(a), b, c, d, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_fldr(jit, a, b, c) jit_add_op(jit, JIT_FLD | REG, SPEC(TREG, REG, NO), a, b, 0, c, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fldi(jit, a, b, c) jit_add_op(jit, JIT_FLD | IMM, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, c, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fldxr(jit, a, b, c, d) jit_add_op(jit, JIT_FLDX | REG, SPEC(TREG, REG, REG), a, b, c, d, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fldxi(jit, a, b, c, d) jit_add_op(jit, JIT_FLDX | IMM, SPEC(TREG, REG, IMM), a, b, (jit_value)(c), d, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_fputargr(jit, a, b) jit_add_fop(jit, JIT_FPUTARG | REG, SPEC(REG, NO, NO), (a), 0, 0, 0, (b), jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_fputargi(jit, a, b) jit_add_fop(jit, JIT_FPUTARG | IMM, SPEC(IMM, NO, NO), 0, 0, 0, (a), (b), jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_fretr(jit, a, b) jit_add_fop(jit, JIT_FRET | REG, SPEC(REG, NO, NO), a, 0, 0, 0, b, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_freti(jit, a, b) jit_add_fop(jit, JIT_FRET | IMM, SPEC(IMM, NO, NO), 0, 0, 0, a, b, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_fretval(jit, a, b) jit_add_fop(jit, JIT_FRETVAL, SPEC(TREG, NO, NO), a, 0, 0, 0, b, jit_debug_info_new(__FILE__, __func__, __LINE__))

/*
 * direct data and code emission 
 */

#define jit_ref_code(jit, a, b) jit_add_op(jit, JIT_REF_CODE, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_ref_data(jit, a, b) jit_add_op(jit, JIT_REF_DATA, SPEC(TREG, IMM, NO), a, (jit_value)(b), 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_code_align(jit, a) jit_add_op(jit, JIT_CODE_ALIGN| IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_data_byte(jit, a)  jit_add_op(jit, JIT_DATA_BYTE | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_data_str(jit, a)   jit_data_bytes(jit, strlen(a) + 1, ((unsigned char *)a))

#define jit_data(jit, a)  do { jit_value _x = (jit_value)(a); jit_data_bytes(jit, sizeof(jit_value), (unsigned char*) &_x); } while(0)
#define jit_data_word(jit, a)  do { short _x = (a); jit_data_bytes(jit, 2, (unsigned char*) &_x); } while(0)
#define jit_data_dword(jit, a)  do { int _x = (a); jit_data_bytes(jit, 4, (unsigned char*) &_x); } while(0)
#define jit_data_qword(jit, a)  do { int64_t _x = (a); jit_data_bytes(jit, 8, (unsigned char*) &_x); } while(0)
#define jit_data_ptr(jit, a)  do { void * _x = (void *)(a); jit_data_bytes(jit, sizeof(void *), (unsigned char*) &_x); } while(0)
#define jit_data_ref_code(jit, a)	jit_add_op(jit, JIT_DATA_REF_CODE | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_data_ref_data(jit, a)	jit_add_op(jit, JIT_DATA_REF_DATA | IMM, SPEC(IMM, NO, NO), (jit_value)(a), 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))

#define jit_data_emptyarea(jit, count) \
	do {  \
		for (int i = 0; i < count; i++) jit_data_byte(jit, 0x00);\
	} while(0) 

#define jit_data_bytes(jit, count, data) \
do {\
	unsigned char *_data = (unsigned char *) (data);\
	for (int i = 0; i < count; i++, _data++)\
		jit_data_byte(jit, *(_data));\
} while (0)

/*
 * testing and debugging
 */
#define jit_force_spill(jit, a) jit_add_fop(jit, JIT_FORCE_SPILL, SPEC(REG, NO, NO), a, 0, 0, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#define jit_force_assoc(jit, a, b, c) jit_add_fop(jit, JIT_FORCE_ASSOC, SPEC(REG, IMM, NO), a, b, c, 0, 0, jit_debug_info_new(__FILE__, __func__, __LINE__))
#endif
