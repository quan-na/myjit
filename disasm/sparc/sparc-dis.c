#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include "sparc-dis.h"

#define ABS(x)  ((x) < 0 ? - (x) : x)


char *fops[] = {
        /* fop1 format */
	[196] = "fitos", 
	[200] = "fitod", 
	[204] = "fitoq", 
	[132] = "fxtos", 
	[136] = "fxtod", 
	[140] = "fxtoq", 
	[209] = "fstoi", 
	[210] = "fdtoi", 
	[211] = "fqtoi", 
	[201] = "fstod", 
	[205] = "fstoq", 
	[198] = "fdtos", 
	[206] = "fdtoq", 
	[199] = "fqtos", 
	[203] = "fqtod", 
	[1]   = "fmovs", 
	[2]   = "fmovd", 
	[5]   = "fnegs", 
	[6]   = "fnegd", 
	[9]   = "fabss", 
	[10]  = "fabsd", 
	[41]  = "fsqrts",
	[42]  = "fsqrtd",
	[43]  = "fsqrtq",
	[65]  = "fadds", 
	[66]  = "faddd", 
	[67]  = "faddq", 
	[69]  = "fsubs", 
	[70]  = "fsubd", 
	[71]  = "fsubq", 
	[73]  = "fmuls", 
	[74]  = "fmuld", 
	[75]  = "fmulq", 
	[105] = "fsmuld",
	[111] = "fdmulq",
	[77]  = "fdivs", 
	[78]  = "fdivd", 
	[79]  = "fdivq", 

	/* fop2 format */
	[81]  = "fcmps", 
	[82]  = "fcmpd", 
	[83]  = "fcmpq", 
	[85]  = "fcmpes",
	[86]  = "fcmped",
	[87]  = "fcmpeq",
};
typedef enum {
        /* fop1 format */
        sparc_fitos_val = 196,
        sparc_fitod_val = 200,
        sparc_fitoq_val = 204,
        sparc_fxtos_val = 132,
        sparc_fxtod_val = 136,
        sparc_fxtoq_val = 140,
        sparc_fstoi_val = 209,
        sparc_fdtoi_val = 210,
        sparc_fqtoi_val = 211,
        sparc_fstod_val = 201,
        sparc_fstoq_val = 205,
        sparc_fdtos_val = 198,
        sparc_fdtoq_val = 206,
        sparc_fqtos_val = 199,
        sparc_fqtod_val = 203,
        sparc_fmovs_val  = 1,
        sparc_fmovd_val  = 2,
        sparc_fnegs_val  = 5,
        sparc_fnegd_val  = 6,
        sparc_fabss_val  = 9,
        sparc_fabsd_val  = 10,
        sparc_fsqrts_val = 41,
        sparc_fsqrtd_val = 42,
        sparc_fsqrtq_val = 43,
        sparc_fadds_val  = 65,
        sparc_faddd_val  = 66,
        sparc_faddq_val  = 67,
        sparc_fsubs_val  = 69,
        sparc_fsubd_val  = 70,
        sparc_fsubq_val  = 71,
        sparc_fmuls_val  = 73,
        sparc_fmuld_val  = 74,
        sparc_fmulq_val  = 75,
        sparc_fsmuld_val = 105,
        sparc_fdmulq_val = 111,
        sparc_fdivs_val  = 77,
        sparc_fdivd_val  = 78,
        sparc_fdivq_val  = 79,
        /* fop2 format */
        sparc_fcmps_val  = 81,
        sparc_fcmpd_val  = 82,
        sparc_fcmpq_val  = 83,
        sparc_fcmpes_val = 85,
        sparc_fcmped_val = 86,
        sparc_fcmpeq_val = 87
} SparcFOp;

//
//

#define sparc_inst_op(inst) ((inst) >> 30)
#define sparc_inst_op2(inst) (((inst) >> 22) & 0x7)
#define sparc_inst_rd(inst) (((inst) >> 25) & 0x1f)
#define sparc_inst_op3(inst) (((inst) >> 19) & 0x3f)
#define sparc_inst_i(inst) (((inst) >> 13) & 0x1)
#define sparc_inst_rs1(inst) (((inst) >> 14) & 0x1f)
#define sparc_inst_rs2(inst) (((inst) >> 0) & 0x1f)
#define sparc_inst_imm(inst) (((inst) >> 13) & 0x1)
#define sparc_inst_imm13(inst) (((inst) >> 0) & 0x1fff)
#define sparc_inst_imm22(inst) (((inst) >> 0) & 0x3fffff)
#define sparc_inst_imm30(inst) (((inst) >> 0) & 0x3fffffff)
#define sparc_inst_cond(inst) (((inst) >> 25) & 0xf)
#define sparc_inst_a(inst) (((inst) >> 29) & 0x1)
#define sparc_inst_opf(inst) (((inst) >> 5) & 0x1ff)

/*
typedef struct spd_t {
	unsigned long pc;
	char out[1024];
	uint32_t *in;
} spd_t;
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

static char *regs[] = 
{
	"g0", "g1", "g2", "g3", "g4", "g5", "g6", "g7",
	"o0", "o1", "o2", "o3", "o4", "o5", "sp", "o7", 
	"l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7", 
	"i0", "i1", "i2", "i3", "i4", "i5", "fp", "i7"
};

static void spd_out_reg(spd_t *dis, int reg)
{
	strcat(dis->out, "%");
	strcat(dis->out, regs[reg]);
}

static void spd_out_imm(spd_t *dis, int imm)
{
	spd_out_printf(dis->out, "0x%x", imm);
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

static void spd_out_indirect_addr(spd_t *dis, int reg1, int reg2)
{
	strcat(dis->out, "[ ");
	if (reg1 && reg2) {
		spd_out_reg(dis, reg1);
		strcat(dis->out, " + ");
		spd_out_reg(dis, reg2);
	} else if (reg1) spd_out_reg(dis, reg1);
	else spd_out_reg(dis, reg2);
	strcat(dis->out, " ]");
}

static void spd_out_indirect_addr_imm(spd_t *dis, int reg1, int imm)
{
	strcat(dis->out, "[ ");
	spd_out_reg(dis, reg1);
	if (imm > 0) {
		strcat(dis->out, " + ");
		spd_out_printf(dis->out, "%i", imm);
	}
	if (imm < 0) {
		strcat(dis->out, " - ");
		spd_out_printf(dis->out, "%i", -imm);
	}
	strcat(dis->out, " ]");
}

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
	}

	if (op2 == 0x02) {
		spd_out_name(dis, bops[sparc_inst_cond(insn)]); 
		if (sparc_inst_a(insn)) strcat(dis->out + strlen(dis->out) - 1, ",a ");
		int simm22;
		if (imm22 & (1 << 21)) simm22 = imm22 | (-1 << 21);
		else simm22 = imm22;

		spd_out_addr(dis, simm22 * 4);
	}

	if (op2 == 0x06) {
		spd_out_name(dis, bfops[sparc_inst_cond(insn)]); 
		if (sparc_inst_a(insn)) strcat(dis->out + strlen(dis->out) - 1, ",a ");
		int simm22;
		if (imm22 & (1 << 21)) simm22 = imm22 | (-1 << 21);
		else simm22 = imm22;

		spd_out_addr(dis, simm22 * 4);
	}

}

#define OP_LOAD 	(1)
#define OP_LOAD_FLOAT 	(2)
#define OP_STORE	(4)
#define OP_STORE_FLOAT 	(5)
#define OP_LOGIC	(11)
#define OP_SHIFT	(12)
#define OP_ADD		(13)
#define OP_SUB		(15)
#define OP_MUL		(18)
#define OP_DIV		(19)
#define OP_STACK	(20)
#define OP_JMPL		(25)
#define OP_FOP_2A	(10033)
#define OP_FOP_2B	(20033)
#define OP_FOP_3	(30033)
static void decode_format3(spd_t *dis, uint32_t insn)
{

	int rd = sparc_inst_rd(insn);
	int rs1 = sparc_inst_rs1(insn);
	int rs2 = sparc_inst_rs2(insn);
	int op3 = sparc_inst_op3(insn);
	int is_imm = sparc_inst_i(insn);
	int imm13 = sparc_inst_imm13(insn); 
	int simm13;
	if (imm13 & (1 << 12)) simm13 = imm13 | (-1 << 12);
	else simm13 = imm13;
	char *op_name = NULL;

	int op_category;
	if (sparc_inst_op(insn) == 3) {


		switch (op3) {
			case 0x00: op_category = OP_LOAD; op_name = "ld"; break;
			case 0x01: op_category = OP_LOAD; op_name = "ldub"; break;
			case 0x02: op_category = OP_LOAD; op_name = "lduh"; break;
			case 0x03: op_category = OP_LOAD; op_name = "ldd"; break;
			case 0x09: op_category = OP_LOAD; op_name = "ldsb"; break;
			case 0x0a: op_category = OP_LOAD; op_name = "ldsh"; break;
			case 0x04: op_category = OP_STORE; op_name = "st"; break;
			case 0x05: op_category = OP_STORE; op_name = "stb"; break;
			case 0x06: op_category = OP_STORE; op_name = "sth"; break;
			case 0x07: op_category = OP_STORE; op_name = "std"; break;
			case 0x20: op_category = OP_LOAD_FLOAT; op_name = "ldf"; break;
			case 0x23: op_category = OP_LOAD_FLOAT; op_name = "lddf"; break;
			case 0x24: op_category = OP_STORE_FLOAT; op_name = "stf"; break;
			case 0x27: op_category = OP_STORE_FLOAT; op_name = "stdf"; break;

		}
		switch (op_category) {
			case OP_LOAD:
				spd_out_name(dis, op_name);
				if (is_imm) spd_out_indirect_addr_imm(dis, rs1, simm13);
				else spd_out_indirect_addr(dis, rs1, rs2);
				spd_out_comma(dis);
				spd_out_reg(dis, rd);
				break;

			case OP_LOAD_FLOAT:
				spd_out_name(dis, op_name);
				if (is_imm) spd_out_indirect_addr_imm(dis, rs1, simm13);
				else spd_out_indirect_addr(dis, rs1, rs2);
				spd_out_comma(dis);
				spd_out_freg(dis, rd);
				break;

			case OP_STORE:
				spd_out_name(dis, op_name);
				spd_out_reg(dis, rd);
				spd_out_comma(dis);
				if (is_imm) spd_out_indirect_addr_imm(dis, rs1, simm13);
				else spd_out_indirect_addr(dis, rs1, rs2);
				break;
			case OP_STORE_FLOAT:
				spd_out_name(dis, op_name);
				spd_out_freg(dis, rd);
				spd_out_comma(dis);
				if (is_imm) spd_out_indirect_addr_imm(dis, rs1, simm13);
				else spd_out_indirect_addr(dis, rs1, rs2);
				break;



		}
	}

	if (sparc_inst_op(insn) == 2) {
		switch (op3) {
			case 0x01: op_category = OP_LOGIC; op_name = "and"; break;
			case 0x11: op_category = OP_LOGIC; op_name = "andcc"; break;
			case 0x05: op_category = OP_LOGIC; op_name = "andn"; break;
			case 0x15: op_category = OP_LOGIC; op_name = "andncc"; break;
			case 0x02: op_category = OP_LOGIC; op_name = "or"; break;
			case 0x12: op_category = OP_LOGIC; op_name = "orcc"; break;
			case 0x16: op_category = OP_LOGIC; op_name = "orn"; break;
			case 0x03: op_category = OP_LOGIC; op_name = "xor"; break;
			case 0x13: op_category = OP_LOGIC; op_name = "xorcc"; break;
			case 0x07: op_category = OP_LOGIC; op_name = "xnor"; break;
			case 0x17: op_category = OP_LOGIC; op_name = "xnorcc"; break;
			case 0x25: op_category = OP_SHIFT; op_name = "sll"; break;
			case 0x26: op_category = OP_SHIFT; op_name = "srl"; break;
			case 0x27: op_category = OP_SHIFT; op_name = "sra"; break;

			case 0x00: op_category = OP_ADD; op_name = "add"; break;
			case 0x10: op_category = OP_ADD; op_name = "addcc"; break;
			case 0x08: op_category = OP_ADD; op_name = "addx"; break;
			case 0x18: op_category = OP_ADD; op_name = "addxcc"; break;


			case 0x04: op_category = OP_SUB; op_name = "sub"; break;
			case 0x14: op_category = OP_SUB; op_name = "subcc"; break;
			case 0x0c: op_category = OP_SUB; op_name = "subx"; break;
			case 0x1c: op_category = OP_SUB; op_name = "subxcc"; break;


			case 0x0a: op_category = OP_MUL; op_name = "umul"; break;
			case 0x0b: op_category = OP_MUL; op_name = "smul"; break;
			case 0x1a: op_category = OP_MUL; op_name = "umulcc"; break;
			case 0x1b: op_category = OP_MUL; op_name = "smulcc"; break;


			case 0x0e: op_category = OP_DIV; op_name = "udiv"; break;
			case 0x0f: op_category = OP_DIV; op_name = "sdiv"; break;
			case 0x1e: op_category = OP_DIV; op_name = "udivcc"; break;
			case 0x1f: op_category = OP_DIV; op_name = "sdivcc"; break;

			case 0x3c: op_category = OP_STACK; op_name = "save"; break;
			case 0x3d: op_category = OP_STACK; op_name = "restore"; break;

			case 0x38: op_category = OP_JMPL; op_name = "jmpl"; break;

			case 0x34:
				op_name = fops[sparc_inst_opf(insn)];
				switch (sparc_inst_opf(insn)) {
					case sparc_fitos_val:
					case sparc_fitod_val:
					case sparc_fitoq_val:
					case sparc_fstoi_val:
					case sparc_fdtoi_val:
					case sparc_fqtoi_val:
					case sparc_fstoq_val:
					case sparc_fdtos_val:
					case sparc_fdtoq_val:
					case sparc_fqtos_val:
					case sparc_fqtod_val:
					case sparc_fmovs_val:
					case sparc_fmovd_val:
					case sparc_fstod_val:
					case sparc_fabss_val:
					case sparc_fabsd_val:
					case sparc_fnegs_val:
					case sparc_fnegd_val:
					case sparc_fsqrts_val:
					case sparc_fsqrtd_val:
					case sparc_fsqrtq_val:
						op_category = OP_FOP_2A;
						break;

					case sparc_fadds_val:
					case sparc_faddd_val:
					case sparc_faddq_val:
					case sparc_fsubs_val:
					case sparc_fsubd_val:
					case sparc_fsubq_val:
					case sparc_fmuls_val:
					case sparc_fmuld_val:
					case sparc_fmulq_val:
					case sparc_fsmuld_val:
					case sparc_fdmulq_val:
					case sparc_fdivs_val:
					case sparc_fdivd_val:
					case sparc_fdivq_val:
						op_category = OP_FOP_3;
						break;
				}
				break;

			case 0x35:
				op_name = fops[sparc_inst_opf(insn)];
				op_category = OP_FOP_2B;
				break;

		}

/*
	// B.28 - rd y
	// B.29 - wr y
*/

		switch (op_category) {
			case OP_LOGIC:
			case OP_SHIFT:
				spd_out_name(dis, op_name);
				spd_out_reg(dis, rs1);
				spd_out_comma(dis);
				if (is_imm) spd_out_imm(dis, imm13);
				else spd_out_reg(dis, rs2);
				spd_out_comma(dis);
				spd_out_reg(dis, rd);
				break;
			case OP_ADD:
			case OP_SUB:
			case OP_MUL:
			case OP_DIV:
			case OP_STACK:
				spd_out_name(dis, op_name);
				spd_out_reg(dis, rs1);
				spd_out_comma(dis);
				if (is_imm) spd_out_simm(dis, simm13);
				else spd_out_reg(dis, rs2);
				spd_out_comma(dis);
				spd_out_reg(dis, rd);
				break;

			case OP_JMPL:
				spd_out_name(dis, op_name);
				// FIXME: adresa

				spd_out_comma(dis);
				spd_out_reg(dis, rd);
				break;

			case OP_FOP_2A:
				spd_out_name(dis, op_name);
				spd_out_freg(dis, rs2);
				spd_out_comma(dis);
				spd_out_freg(dis, rd);
				break;
			case OP_FOP_2B:
				spd_out_name(dis, op_name);
				spd_out_comma(dis);
				spd_out_freg(dis, rs1);
				spd_out_comma(dis);
				spd_out_freg(dis, rs2);
				break;
				
			case OP_FOP_3:
				spd_out_name(dis, op_name);
				spd_out_freg(dis, rs1);
				spd_out_comma(dis);
				spd_out_freg(dis, rs2);
				spd_out_comma(dis);
				spd_out_freg(dis, rd);
				break;

				break;
	
		}

	}
}

static void decode(spd_t *dis, uint32_t insn)
{
	switch (sparc_inst_op(insn)) {
		case 1: decode_format1(dis, insn); break;
		case 0: decode_format2(dis, insn); break;
		default: decode_format3(dis, insn); break;
	}
}


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


/*
int main()
{
	spd_t dis;
//	84 10 a3 ff     or  %g2, 0x3ff, %g2 
//	84 00 60 04 
	decode(&dis, 0x84006004);
	printf("#%s#\n", dis.out);
}
*/
