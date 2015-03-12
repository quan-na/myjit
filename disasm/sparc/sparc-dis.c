/*
 * MyJIT Disassembler 
 *
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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include "sparc-dis.h"

#define ABS(x)  ((x) < 0 ? - (x) : x)

#define SPARC_G0	(0)
#define SPARC_O7	(15)
#define SPARC_I7	(31)

#define sparc_inst_op(inst) ((inst) >> 30)
#define sparc_inst_op2(inst) (((inst) >> 22) & 0x7)
#define sparc_inst_rd(inst) (((inst) >> 25) & 0x1f)
#define sparc_inst_op3(inst) (((inst) >> 19) & 0x3f)
#define sparc_inst_i(inst) (((inst) >> 13) & 0x1)
#define sparc_inst_rs1(inst) (((inst) >> 14) & 0x1f)
#define sparc_inst_rs2(inst) (((inst) >> 0) & 0x1f)
#define sparc_inst_imm(inst) (((inst) >> 13) & 0x1)
#define sparc_inst_imm11(inst) (((inst) >> 0) & 0x7ff)
#define sparc_inst_imm13(inst) (((inst) >> 0) & 0x1fff)
#define sparc_inst_imm22(inst) (((inst) >> 0) & 0x3fffff)
#define sparc_inst_imm30(inst) (((inst) >> 0) & 0x3fffffff)
#define sparc_inst_cond(inst) (((inst) >> 25) & 0xf)
#define sparc_inst_a(inst) (((inst) >> 29) & 0x1)
#define sparc_inst_opf(inst) (((inst) >> 5) & 0x1ff)
#define sparc_inst_cond_format4(inst) (((inst) >> 14) & 0xf)

typedef struct {
	int opc;
	void (*print_fn)(spd_t *, char *op_name, int is_imm, int rd, int rs1, int rs2, int simm13);
	char *op_name;
} format3;

#define DEFINE_FORMAT(_fmt_name) \
static void _fmt_name(spd_t *dis, char *op_name, int is_imm, int rd, int rs1, int rs2, int simm13)

#define NAME		spd_out_name(dis, op_name);
#define OPNAME(_x)	spd_out_name(dis, (_x));
#define REG(_r) 	spd_out_reg(dis, (_r));
#define FREG(_r) 	spd_out_freg(dis, (_r));
#define COMMA		spd_out_comma(dis);
#define MEM do { \
	if (is_imm) spd_out_mem_imm(dis, rs1, simm13); \
	else spd_out_mem_reg(dis, rs1, rs2);\
} while(0);

#define REG_OR_SIMM13 do { \
	if (is_imm) spd_out_simm(dis, simm13); \
	else spd_out_reg(dis, rs2); \
} while(0);

#define REG_OR_IMM13 do { \
	if (is_imm) spd_out_imm(dis, simm13); \
	else spd_out_reg(dis, rs2); \
} while(0);


#define INDIRECT_ADDR13 do{ \
	if (is_imm) spd_out_indirect_addr_imm(dis, rs1, simm13); \
	else spd_out_indirect_addr_reg(dis, rs1, rs2); \
} while(0);

static inline int sign_ext(int bits, int value)
{
	if (value & (1 << (bits - 1))) return value | (-1 << (bits - 1));
	return value;
}

/*
 *
 * Output functions
 *
 */
static int spd_out_printf(char *buf, const char *format, ...) {
        va_list ap;
        va_start(ap, format);
        int result = vsprintf(buf + strlen(buf), format, ap);
        va_end(ap);
        return result;
}

static void spd_out_name(spd_t *dis, char *opname)
{
	memcpy(dis->out, opname, strlen(opname) + 1);
	strcat(dis->out, " ");
}

static void spd_out_addr(spd_t *dis, int addr)
{
	spd_out_printf(dis->out, "%x <pc %s %i>", 
			dis->pc + addr,
			addr < 0 ? "-" : "+", 
			ABS(addr));
}

static void spd_out_comma(spd_t *dis)
{
	strcat(dis->out, ", ");
}

static void spd_out_reg(spd_t *dis, int reg)
{
	static char *regs[] = {
		"g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
		"o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7", 
		"l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7", 
		"i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7"
	};

	strcat(dis->out, "%");
	strcat(dis->out, regs[reg]);
}

static void spd_out_imm(spd_t *dis, int imm)
{
	spd_out_printf(dis->out, "0x%x", imm & 0x1ff);
}

static void spd_out_simm(spd_t *dis, int simm)
{
	spd_out_printf(dis->out, "%i", simm);
}

static void spd_out_freg(spd_t *dis, int freg)
{
	strcat(dis->out,"%");
	spd_out_printf(dis->out, "f%i", freg);
}

static void spd_out_indirect_addr_reg(spd_t *dis, int reg1, int reg2)
{
	if (reg1 && reg2) {
		spd_out_reg(dis, reg1);
		strcat(dis->out, " + ");
		spd_out_reg(dis, reg2);
	} else if (reg1) spd_out_reg(dis, reg1);
	else spd_out_reg(dis, reg2);
}

static void spd_out_indirect_addr_imm(spd_t *dis, int reg1, int imm)
{
	spd_out_reg(dis, reg1);
	if (imm > 0) {
		strcat(dis->out, " + ");
		spd_out_printf(dis->out, "%i", imm);
	}
	if (imm < 0) {
		strcat(dis->out, " - ");
		spd_out_printf(dis->out, "%i", -imm);
	}
}


static void spd_out_mem_reg(spd_t *dis, int reg1, int reg2)
{
	strcat(dis->out, "[ ");
	spd_out_indirect_addr_reg(dis, reg1, reg2);
	strcat(dis->out, " ]");
}

static void spd_out_mem_imm(spd_t *dis, int reg1, int imm)
{
	strcat(dis->out, "[ ");
	spd_out_indirect_addr_imm(dis, reg1, imm);
	strcat(dis->out, " ]");
}

/*
 *
 * Decoding
 *
 */

static void decode_format1(spd_t *dis, uint32_t insn)
{
	spd_out_name(dis, "call");
	spd_out_addr(dis, sparc_inst_imm30(insn) << 2);
} 

static void decode_format2(spd_t *dis, uint32_t insn)
{

	static char *bops[] = { "bn", "be", "ble", "bl", "bleu", "bcs", "bneg", "bvs",
				"ba", "bne", "bg", "bge", "bgu", "bcc", "bpos", "bvc" };

	static char *bfops[] = { "fbn", "fbne",  "fblg", "fbul", "fbl", "fbug", "fbg", "fbu",
				"fba", "fbe", "fbue", "fbge", "fbuge", "fble", "fbule", "fbo" };

	int rd = sparc_inst_rd(insn);
	int op2 = sparc_inst_op2(insn);
	int imm22 = sparc_inst_imm22(insn);
	if (op2 == 0x04) {
		if (rd) {
			spd_out_name(dis, "sethi "); 
			spd_out_printf(dis->out, "%%hi(0x%x)", imm22 << 10);
			spd_out_comma(dis);
			spd_out_reg(dis, rd);
		} else {
			spd_out_name(dis, "nop"); 
		}
		return;
	}

	if ((op2 == 0x02) || (op2 == 0x06)) {

		if (op2 == 0x02) spd_out_name(dis, bops[sparc_inst_cond(insn)]); 
		if (op2 == 0x06) spd_out_name(dis, bfops[sparc_inst_cond(insn)]); 

		if (sparc_inst_a(insn)) strcat(dis->out + strlen(dis->out) - 1, ",a ");
		spd_out_addr(dis, sign_ext(22, imm22) * 4);
	}
}


static void decode_format4(spd_t *dis, uint32_t insn)
{
	static char *movops[] = { "movn", "move", "movle", "movl", "movleu", "movcs", "movneg", "movvs",
				"mova", "movne", "movg", "movge", "movgu", "movcc", "movpos", "movvc" };

	int rd = sparc_inst_rd(insn);
	int rs2 = sparc_inst_rs2(insn);
	int op3 = sparc_inst_op3(insn);
	int simm11 = sign_ext(11, sparc_inst_imm11(insn));
	int is_imm = sparc_inst_i(insn);

	if (op3 != 0x2c) return;

	spd_out_name(dis, movops[sparc_inst_cond_format4(insn)]);

	if (is_imm) spd_out_simm(dis, simm11);
	else spd_out_reg(dis, rs2);

	spd_out_comma(dis);
	spd_out_reg(dis, rd);
}

static int decode_synthetic_3_11(spd_t *dis, int op3, int is_imm, int rd, int rs1, int rs2, int simm13)
{
	if (rd == SPARC_G0) {
		switch (op3) {
			case 0x04: OPNAME("clr"); MEM; return 1;
			case 0x05: OPNAME("clrb"); MEM; return 1;
			case 0x06: OPNAME("clrh"); MEM; return 1;
		}
	}
	return 0;
}

static int decode_synthetic_3_10(spd_t *dis, int op3, int is_imm, int rd, int rs1, int rs2, int simm13)
{
	if ((op3 == 0x14 /* subcc */) && (rd == SPARC_G0)) { OPNAME("cmp") REG(rs1) COMMA REG_OR_SIMM13; return 1; }
	if ((op3 == 0x12 /* orcc */) && !is_imm && (rd == SPARC_G0) && (rs1 == SPARC_G0)) { OPNAME("tst"); REG(rs2); return 1; }
	if ((op3 == 0x07 /* xnor */) && !is_imm && (rs2 == SPARC_G0)) { 
		OPNAME("not"); 
		if (rs1 != rd) { REG(rs1) COMMA };
		REG(rd);
		return 1;
	}

	if ((op3 == 0x04 /* sub */) && !is_imm && (rs2 == SPARC_G0)) {
		OPNAME("neg");
		if (rs1 != rd) { REG(rs1) COMMA };
		REG(rd);
		return 1;
	}


	if ((op3 == 0x02 /* or */) && !is_imm && (rs1 == SPARC_G0) && (rs2 == SPARC_G0)) { OPNAME("clr") REG(rd); return 1; };
	if ((op3 == 0x02 /* or */) && (rs1 == SPARC_G0)) { OPNAME("mov") REG_OR_SIMM13 COMMA REG(rd); return 1; };
		
	if ((rs1 == rd) && (is_imm))  {
		int match = 0;
		switch (op3) {
			case 0x00: OPNAME("inc"); match = 1; break;
			case 0x10: OPNAME("inccc"); match = 1; break;
			case 0x04: OPNAME("dec"); match = 1; break;
			case 0x14: OPNAME("deccc"); match = 1; break;
		}
		if (match) {
			if (simm13 != 1) {
				spd_out_simm(dis, simm13);
				spd_out_comma(dis);
			}

			REG(rd);
			return 1;
		}
	}

	if ((op3 == 0x3d /*restore */) && (!is_imm) && (rd == SPARC_G0) && (rs1 == SPARC_G0) && (rs2 == SPARC_G0)) {
		OPNAME("restore");
		return 1;
	}

	if ((op3 == 0x38 /* jmpl */) && (rd == SPARC_G0)) {
		if (is_imm && (simm13 == 8)) {
			if (rs1 == SPARC_I7) { OPNAME("ret"); return 1; }
			if (rs1 == SPARC_O7) { OPNAME("retl"); return 1; }
		}
		OPNAME("jmp"); INDIRECT_ADDR13; 
		return 1;
	}

	if ((op3 == 0x38 /* jmpl */) && (rd == SPARC_O7)) { OPNAME("call"); INDIRECT_ADDR13; return 1; }


	if ((op3 == 0x28 /* rd */) && (rs1 == SPARC_G0)) { 
		OPNAME("rd");
		strcat(dis->out, "%y");
		COMMA;
		REG(rd);
		return 1; 
	}

	if ((op3 == 0x30 /* wr */) && (rd == SPARC_G0)) { 
		OPNAME("wr");
		REG(rs1);
		COMMA;
		REG_OR_IMM13;
		COMMA;
		strcat(dis->out, "%y");
		return 1; 
	}


	return 0;
}

DEFINE_FORMAT(fmt_load) 	{ NAME MEM COMMA REG(rd) }
DEFINE_FORMAT(fmt_load_float) 	{ NAME MEM COMMA FREG(rd) }
DEFINE_FORMAT(fmt_store) 	{ NAME REG(rd) COMMA MEM }
DEFINE_FORMAT(fmt_store_float) 	{ NAME FREG(rd) COMMA MEM }
DEFINE_FORMAT(fmt_logic)	{ NAME REG(rs1) COMMA REG_OR_IMM13 COMMA REG(rd) }
DEFINE_FORMAT(fmt_arithmetics)	{ NAME REG(rs1) COMMA REG_OR_SIMM13 COMMA REG(rd) }
DEFINE_FORMAT(fmt_jmpl)		{ NAME INDIRECT_ADDR13 COMMA REG(rd) }
DEFINE_FORMAT(fmt_fcmp)		{ NAME FREG(rs1) COMMA FREG(rs2) }
DEFINE_FORMAT(fmt_fop2)		{ NAME FREG(rs2) COMMA FREG(rd) }
DEFINE_FORMAT(fmt_fop3)		{ NAME FREG(rs1) COMMA FREG(rs2) COMMA FREG(rd) }

static format3 opcodes_3_11[] = {
	{ .opc = 0x00, .print_fn = fmt_load, .op_name = "ld" }, 
	{ .opc = 0x01, .print_fn = fmt_load, .op_name = "ldub" }, 
	{ .opc = 0x02, .print_fn = fmt_load, .op_name = "lduh" }, 
	{ .opc = 0x03, .print_fn = fmt_load, .op_name = "ldd" }, 
	{ .opc = 0x09, .print_fn = fmt_load, .op_name = "ldsb" }, 
	{ .opc = 0x0a, .print_fn = fmt_load, .op_name = "ldsh" }, 
	{ .opc = 0x04, .print_fn = fmt_store, .op_name = "st" }, 
	{ .opc = 0x05, .print_fn = fmt_store, .op_name = "stb" }, 
	{ .opc = 0x06, .print_fn = fmt_store, .op_name = "sth" }, 
	{ .opc = 0x07, .print_fn = fmt_store, .op_name = "std" }, 
	{ .opc = 0x20, .print_fn = fmt_load_float, .op_name = "ldf" }, 
	{ .opc = 0x23, .print_fn = fmt_load_float, .op_name = "lddf" }, 
	{ .opc = 0x24, .print_fn = fmt_store_float, .op_name = "stf" }, 
	{ .opc = 0x27, .print_fn = fmt_store_float, .op_name = "stdf" }, 
};

static format3 opcodes_3_10[] = {
	{ .opc = 0x01, .print_fn = fmt_logic, .op_name = "and" },
	{ .opc = 0x11, .print_fn = fmt_logic, .op_name = "andcc" },
	{ .opc = 0x05, .print_fn = fmt_logic, .op_name = "andn" },
	{ .opc = 0x15, .print_fn = fmt_logic, .op_name = "andncc" },
	{ .opc = 0x02, .print_fn = fmt_logic, .op_name = "or" },
	{ .opc = 0x12, .print_fn = fmt_logic, .op_name = "orcc" },
	{ .opc = 0x16, .print_fn = fmt_logic, .op_name = "orn" },
	{ .opc = 0x03, .print_fn = fmt_logic, .op_name = "xor" },
	{ .opc = 0x13, .print_fn = fmt_logic, .op_name = "xorcc" },
	{ .opc = 0x07, .print_fn = fmt_logic, .op_name = "xnor" },
	{ .opc = 0x17, .print_fn = fmt_logic, .op_name = "xnorcc" },
	{ .opc = 0x25, .print_fn = fmt_arithmetics, .op_name = "sll" },
	{ .opc = 0x26, .print_fn = fmt_arithmetics, .op_name = "srl" },
	{ .opc = 0x27, .print_fn = fmt_arithmetics, .op_name = "sra" },

	{ .opc = 0x00, .print_fn = fmt_arithmetics, .op_name = "add" },
	{ .opc = 0x10, .print_fn = fmt_arithmetics, .op_name = "addcc" },
	{ .opc = 0x08, .print_fn = fmt_arithmetics, .op_name = "addx" },
	{ .opc = 0x18, .print_fn = fmt_arithmetics, .op_name = "addxcc" },

	{ .opc = 0x04, .print_fn = fmt_arithmetics, .op_name = "sub" },
	{ .opc = 0x14, .print_fn = fmt_arithmetics, .op_name = "subcc" },
	{ .opc = 0x0c, .print_fn = fmt_arithmetics, .op_name = "subx" },
	{ .opc = 0x1c, .print_fn = fmt_arithmetics, .op_name = "subxcc" },

	{ .opc = 0x0a, .print_fn = fmt_arithmetics, .op_name = "umul" },
	{ .opc = 0x0b, .print_fn = fmt_arithmetics, .op_name = "smul" },
	{ .opc = 0x1a, .print_fn = fmt_arithmetics, .op_name = "umulcc" },
	{ .opc = 0x1b, .print_fn = fmt_arithmetics, .op_name = "smulcc" },

	{ .opc = 0x0e, .print_fn = fmt_arithmetics, .op_name = "udiv" },
	{ .opc = 0x0f, .print_fn = fmt_arithmetics, .op_name = "sdiv" },
	{ .opc = 0x1e, .print_fn = fmt_arithmetics, .op_name = "udivcc" },
	{ .opc = 0x1f, .print_fn = fmt_arithmetics, .op_name = "sdivcc" },

	{ .opc = 0x3c, .print_fn = fmt_arithmetics, .op_name = "save" },
	{ .opc = 0x3d, .print_fn = fmt_arithmetics, .op_name = "restore" },

	{ .opc = 0x38, .print_fn = fmt_jmpl, .op_name = "jmpl" },
};

static format3 opcodes_3fop[] = {
        /* fop1 format */
	{ .opc = 196, .print_fn = fmt_fop2, .op_name = "fitos" }, 
	{ .opc = 200, .print_fn = fmt_fop2, .op_name = "fitod" }, 
	{ .opc = 204, .print_fn = fmt_fop2, .op_name = "fitoq" }, 
	{ .opc = 132, .print_fn = fmt_fop2, .op_name = "fxtos" }, 
	{ .opc = 136, .print_fn = fmt_fop2, .op_name = "fxtod" }, 
	{ .opc = 140, .print_fn = fmt_fop2, .op_name = "fxtoq" }, 
	{ .opc = 209, .print_fn = fmt_fop2, .op_name = "fstoi" }, 
	{ .opc = 210, .print_fn = fmt_fop2, .op_name = "fdtoi" }, 
	{ .opc = 211, .print_fn = fmt_fop2, .op_name = "fqtoi" }, 
	{ .opc = 201, .print_fn = fmt_fop2, .op_name = "fstod" }, 
	{ .opc = 205, .print_fn = fmt_fop2, .op_name = "fstoq" }, 
	{ .opc = 198, .print_fn = fmt_fop2, .op_name = "fdtos" }, 
	{ .opc = 206, .print_fn = fmt_fop2, .op_name = "fdtoq" }, 
	{ .opc = 199, .print_fn = fmt_fop2, .op_name = "fqtos" }, 
	{ .opc = 203, .print_fn = fmt_fop2, .op_name = "fqtod" }, 
	{ .opc = 1, .print_fn = fmt_fop2,   .op_name = "fmovs" }, 
	{ .opc = 2, .print_fn = fmt_fop2,   .op_name = "fmovd" }, 
	{ .opc = 5, .print_fn = fmt_fop2,   .op_name = "fnegs" }, 
	{ .opc = 6, .print_fn = fmt_fop2,   .op_name = "fnegd" }, 
	{ .opc = 9, .print_fn = fmt_fop2,   .op_name = "fabss" }, 
	{ .opc = 10, .print_fn = fmt_fop2,  .op_name = "fabsd" }, 
	{ .opc = 41, .print_fn = fmt_fop2,  .op_name = "fsqrts" },
	{ .opc = 42, .print_fn = fmt_fop2,  .op_name = "fsqrtd" },
	{ .opc = 43, .print_fn = fmt_fop2,  .op_name = "fsqrtq" },
	{ .opc = 65, .print_fn = fmt_fop3,  .op_name = "fadds" }, 
	{ .opc = 66, .print_fn = fmt_fop3,  .op_name = "faddd" }, 
	{ .opc = 67, .print_fn = fmt_fop3,  .op_name = "faddq" }, 
	{ .opc = 69, .print_fn = fmt_fop3,  .op_name = "fsubs" }, 
	{ .opc = 70, .print_fn = fmt_fop3,  .op_name = "fsubd" }, 
	{ .opc = 71, .print_fn = fmt_fop3,  .op_name = "fsubq" }, 
	{ .opc = 73, .print_fn = fmt_fop3,  .op_name = "fmuls" }, 
	{ .opc = 74, .print_fn = fmt_fop3,  .op_name = "fmuld" }, 
	{ .opc = 75, .print_fn = fmt_fop3,  .op_name = "fmulq" }, 
	{ .opc = 105, .print_fn = fmt_fop3, .op_name = "fsmuld" },
	{ .opc = 111, .print_fn = fmt_fop3, .op_name = "fdmulq" },
	{ .opc = 77, .print_fn = fmt_fop3,  .op_name = "fdivs" }, 
	{ .opc = 78, .print_fn = fmt_fop3,  .op_name = "fdivd" }, 
	{ .opc = 79, .print_fn = fmt_fop3,  .op_name = "fdivq" }, 

	/* fop2 format */
	{ .opc = 81, .print_fn = fmt_fcmp,  .op_name = "fcmps" }, 
	{ .opc = 82, .print_fn = fmt_fcmp,  .op_name = "fcmpd" }, 
	{ .opc = 83, .print_fn = fmt_fcmp,  .op_name = "fcmpq" }, 
	{ .opc = 85, .print_fn = fmt_fcmp,  .op_name = "fcmpes" },
	{ .opc = 86, .print_fn = fmt_fcmp,  .op_name = "fcmped" },
	{ .opc = 87, .print_fn = fmt_fcmp,  .op_name = "fcmpeq" },
};

static void decode_format3(spd_t *dis, uint32_t insn)
{
	int rd = sparc_inst_rd(insn);
	int rs1 = sparc_inst_rs1(insn);
	int rs2 = sparc_inst_rs2(insn);
	int op3 = sparc_inst_op3(insn);
	int is_imm = sparc_inst_i(insn);
	int imm13 = sparc_inst_imm13(insn); 
	int simm13 = sign_ext(13, imm13);

	format3 *op_table;
	int op_table_size;
	int op;

	if (sparc_inst_op(insn) == 2) {
		if (decode_synthetic_3_10(dis, op3, is_imm, rd, rs1, rs2, simm13)) return;
		op_table = opcodes_3_10;
		op_table_size = sizeof(opcodes_3_10);
		op = op3;
	}

	if (sparc_inst_op(insn) == 3) {
		if (decode_synthetic_3_11(dis, op3, is_imm, rd, rs1, rs2, simm13)) return;
		op_table = opcodes_3_11;
		op_table_size = sizeof(opcodes_3_11);
		op = op3;
	}

	if ((sparc_inst_op(insn) == 2) && ((op3 == 0x34) || (op3 == 0x35))) {
		op_table = opcodes_3fop;
		op_table_size = sizeof(opcodes_3fop);
		op = sparc_inst_opf(insn);
	}

	if ((sparc_inst_op(insn) == 2) && (op3 == 0x2c)) {
		decode_format4(dis, insn);
		return;
	}

	for (int i = 0; i < op_table_size; i++)
		if (op_table[i].opc == op) {
			char *op_name = op_table[i].op_name;
			op_table[i].print_fn(dis, op_name, is_imm, rd, rs1, rs2, simm13);
			return;
		}
	// B.28 - rd y
	// B.29 - wr y
}

static void decode(spd_t *dis, uint32_t insn)
{
	switch (sparc_inst_op(insn)) {
		case 1: decode_format1(dis, insn); break;
		case 0: decode_format2(dis, insn); break;
		default: decode_format3(dis, insn); break;
	}
}

/*
 *
 * Public API
 *
 */
void spd_init(spd_t *dis)
{
	dis->ibuf = NULL;
	dis->ibuf_index = 0;
	dis->ibuf_size = 0;
	dis->pc = 0;
}

void spd_set_input_buffer(spd_t *dis, unsigned char *buf, int len)
{
	dis->ibuf = (uint32_t *) buf;
	dis->ibuf_index = 0;
	dis->ibuf_size = len / 4;
}

int spd_disassemble(spd_t *dis)
{
	spd_out_name(dis, "???");
	if (dis->ibuf_index >= dis->ibuf_size) return 0;
	decode(dis, dis->ibuf[dis->ibuf_index]);
	dis->ibuf_index++;
	dis->pc += 4;
	return 1;
}

char *spd_insn_asm(spd_t *dis)
{
	return dis->out;
}

void spd_set_pc(spd_t *dis, unsigned long pc)
{
	dis->pc = pc;
}
