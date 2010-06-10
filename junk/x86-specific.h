#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "x86-codegen.h"

#define JIT_RET (JIT_FORCE_REG(X86_EAX))
#define JIT_R0 (0)
#define JIT_R1 (1)
#define JIT_R2 (2)
#define JIT_R3 (3)
#define JIT_R4 (4)
#define JIT_R5 (5)
#define JIT_R6 (6)
#define JIT_R7 (7)

#define JIT_GP_REG_COUNT (4)

struct jit_native_reg_info ** jit_init_native_regs()
{
	struct jit_native_reg_info ** info = (struct jit_native_reg_info **) JIT_MALLOC(sizeof(struct jit_native_reg_info *) * JIT_GP_REG_COUNT);
	info[0] = jit_create_reg_info(X86_EAX, 0, "eax"); 
	info[1] = jit_create_reg_info(X86_EBX, 1, "ebx"); 
	info[2] = jit_create_reg_info(X86_ECX, 0, "ecx"); 
	info[3] = jit_create_reg_info(X86_EDX, 0, "edx"); 
	info[4] = jit_create_reg_info(X86_ESI, 1, "esi"); 
	info[5] = jit_create_reg_info(X86_EDI, 1, "edi"); 
	return info;
}

/**
 * @return pointer to a function
 */
void * jit_new_func(struct jit * jit)
{
	unsigned char * r = jit->ip;
	x86_push_reg(jit->ip, X86_EBP);
	x86_mov_reg_reg(jit->ip, X86_EBP, X86_ESP, 4);
	return r;
}

static inline void jit_lru_reg_to_mem(struct jit * jit, reg_t nat_reg, reg_t virt_reg)
{
	//x86_mov_mem_reg(jit->ip, &jit->reg[id], jit->nat_reg_info[JIT_GP_REG_COUNT - 1]->reg_id, REG_SIZE); 
	x86_mov_mem_reg(jit->ip, &jit->reg[virt_reg], nat_reg, REG_SIZE); 
}

static inline void jit_lru_mem_to_reg(struct jit * jit, reg_t id)
{
	x86_mov_reg_mem(jit->ip, jit->nat_reg_info[0]->reg_id, &jit->reg[id], REG_SIZE); 
}

int jit_arg(struct jit * jit, size_t size)
{
	int r = jit->argpos;
	jit->argpos += size;
	return r;
}

#define jit_getarg(jit,dest,pos,size) \
	do { \
		reg_t r_dest = jit_get_reg_ex(jit, dest, 1); \
		x86_mov_reg_membase(jit->ip, r_dest, X86_EBP, 8 + pos, size); \
	} while (0); 

#define jit_movr(jit,dest,src,size) \
	do { \
		jit_touch_reg2(jit, dest, src); \
		reg_t r_dest = jit_get_reg_ex(jit, dest, 1); \
		reg_t r_src = jit_get_reg_ex(jit, src, 0); \
		x86_mov_reg_reg(jit->ip, r_dest, r_src, size); \
	} while (0); 

#define jit_movi(jit,dest,val) \
	do { \
		reg_t r_dest = jit_get_reg_ex(jit, dest, 1); \
		x86_mov_reg_imm(jit->ip, r_dest, val); \
	} while (0);

#define __ALU_OP_IMM(__fun_name, __alu_op) \
static inline void __fun_name(struct jit * jit, reg_t dest, reg_t src, long val)  \
{ \
	jit_touch_reg2(jit, dest, src); \
	reg_t r_dest = jit_get_reg_ex(jit, dest, 1); \
	reg_t r_src = jit_get_reg_ex(jit, src, 0); \
	if (src != dest) x86_mov_reg_reg(jit->ip, r_dest, r_src, REG_SIZE); \
	x86_alu_reg_imm(jit->ip, __alu_op, r_dest, val); \
}
#define __ALU_OP_REG(__fun_name, __alu_op) \
static inline void __fun_name(struct jit * jit, reg_t dest, reg_t src, reg_t val)  \
{ \
	jit_touch_reg3(jit, dest, src, val); \
	reg_t r_dest = jit_get_reg_ex(jit, dest, 1); \
	reg_t r_src = jit_get_reg_ex(jit, src, 0); \
	reg_t r_val = jit_get_reg_ex(jit, val, 0); \
	if (src != dest) x86_mov_reg_reg(jit->ip, r_dest, r_src, REG_SIZE); \
	x86_alu_reg_reg(jit->ip, __alu_op, r_dest, r_val); \
}
#define __ALU_OP(reg_name, imm_name, alu_op) \
	__ALU_OP_IMM(imm_name, alu_op) \
	__ALU_OP_REG(reg_name, alu_op)

__ALU_OP(jit_addr, jit_addi, X86_ADD)
__ALU_OP(jit_addcr, jit_addci, X86_ADD)
__ALU_OP(jit_subr, jit_subi, X86_SUB)
__ALU_OP(jit_subcr, jit_subci, X86_SUB)
__ALU_OP(jit_andr, jit_andi, X86_AND)
__ALU_OP(jit_orr, jit_ori, X86_OR)
__ALU_OP(jit_xorr, jit_xori, X86_XOR)

#define jit_inc(jit,dest) \
	do { \
		reg_t r_dest = jit_get_reg_ex(jit, dest, 0); \
		x86_inc_reg(jit->ip, r_dest); \
	} while (0);

#define jit_dec(jit,dest) \
	do { \
		reg_t r_dest = jit_get_reg_ex(jit, dest, 0); \
		x86_dec_reg(jit->ip, r_dest); \
	} while (0);

#define jit_str(jit,memreg,valreg,size) \
	do { \
		jit_touch_reg2(jit, memreg, valreg); \
		reg_t r_memreg = jit_get_reg_ex(jit, memreg, 0); \
		reg_t r_valreg = jit_get_reg_ex(jit, valreg, 0); \
		x86_mov_membase_reg(jit->ip, r_memreg, 0, r_valreg,size); \
	} while (0);

#define jit_sti(jit,mem,valreg,size) \
	do { \
		reg_t r_valreg = jit_get_reg_ex(jit, valreg, 0); \
		x86_mov_mem_reg(jit->ip, mem, r_valreg, size); \
	} while (0);

#define jit_stxr(jit,memreg,shiftreg,valreg,size) \
	do { \
		jit_touch_reg3(jit, memreg, valreg, shiftreg); \
		reg_t r_memreg = jit_get_reg_ex(jit, memreg, 0); \
		reg_t r_valreg = jit_get_reg_ex(jit, valreg, 0); \
		reg_t r_shiftreg = jit_get_reg_ex(jit, valreg, 0); \
		x86_mov_memindex_reg(jit->ip, r_memreg, 0, r_shiftreg, 0, r_valreg, size); \
	} while (0);

#define jit_stxi(jit,mem,memreg,valreg,size) \
	do { \
		jit_touch_reg2(jit, memreg, valreg); \
		reg_t r_memreg = jit_get_reg_ex(jit, memreg, 0); \
		reg_t r_valreg = jit_get_reg_ex(jit, valreg, 0); \
		x86_mov_membase_reg(jit->ip, r_memreg, mem, r_valreg, size); \
	} while (0);

#define jit_ldr(jit,dest,memreg,size) \
	do { \
		jit_touch_reg2(jit, dest, memreg); \
		reg_t r_dest = jit_get_reg_ex(jit, dest, 1); \
		reg_t r_memreg = jit_get_reg_ex(jit, memreg, 0); \
		x86_mov_reg_membase(jit->ip, r_dest, r_memreg, 0, size); \
	while (0);

#define jit_ldi(jit,dest,mem,size) \
	do { \
		reg_t r_dest = jit_get_reg_ex(jit, dest, 1); \
		x86_mov_reg_mem(jit->ip, r_dest, mem, size); \
	} while(0);

#define jit_ldxr(jit,dest,memreg,shiftreg,size) \
	do { \
		jit_touch_reg3(jit,memreg, dest, shiftreg); \
		reg_t r_memreg = jit_get_reg_ex(jit, memreg, 0); \
		reg_t r_dest = jit_get_reg_ex(jit, dest, 1); \
		reg_t r_shiftreg = jit_get_reg_ex(jit, shiftreg, 0); \
		x86_mov_reg_memindex(jit->ip, r_dest, r_memreg, 0, r_shiftreg, 0, size); \
	} while (0);

#define jit_ldxi(jit,dest,memreg,shift,size) \
	do { \
		jit_touch_reg2(jit, dest, memreg); \
		reg_t r_dest = jit_get_reg_ex(jit, dest, 1); \
		reg_t r_memreg = jit_get_reg_ex(jit, memreg, 0); \
		x86_mov_reg_membase(jit->ip, r_dest, r_memreg, shift, size); \
	} while (0);

#define jit_prepare(jit,args) \
	jit->prepare_args = args;

#define jit_pusharg(jit,reg) \
       do { \
	       reg_t r = jit_get_reg_ex(reg, 0); \
	       x86_push_reg(jit->ip, r); \
       } while (0);

#define jit_cleanup_regs(jit)\
	x86_alu_reg_imm(jit->ip, X86_ADD, X86_ESP, jit->prepare_args * PTR_SIZE); \
	jit->prepare_args = 0;

static inline unsigned char * jit_calli(struct jit * jit, void * fn) {
	unsigned char * pos = jit->ip;
	x86_call_code(jit->ip, fn);
	return pos;
}

#define jit_callr(jit,reg) \
       do { \
	       reg_t r = jit_get_reg_ex(reg, 0); \
	       x86_call_reg(jit->ip, r); \
       } while (0);

static inline unsigned char * jit_jmpi(struct jit * jit, void * addr) {
	unsigned char * pos = jit->ip;
	x86_jump32(jit->ip, addr);
	return pos;
}

#define jit_jmpr(jit,reg) \
       do { \
	       reg_t r = jit_get_reg_ex(reg, 0); \
	       x86_jump_reg(jit->ip, r); \
       } while(0);

#define jit_patch(jit,op_pos) \
	x86_patch(op_pos, jit->ip)

#define __BRANCH_COND_OP_IMM(__name,__cond,__signed) \
static inline unsigned char * __name(struct jit * jit, unsigned char * addr, reg_t arg, long imm) { \
	reg_t r_arg = jit_get_reg_ex(jit, arg, 0); \
	x86_cmp_reg_imm(jit->ip, r_arg, imm); \
	unsigned char * pos = jit->ip; \
	x86_branch(jit->ip, __cond, addr, __signed); \
	return pos; \
}

#define __BRANCH_COND_OP_REG(__name,__cond,__signed) \
static inline unsigned char * __name(struct jit * jit, unsigned char * addr, reg_t arg1, reg_t arg2) {\
	jit_touch_reg2(jit, arg1, arg2); \
	reg_t r_arg1 = jit_get_reg_ex(jit, arg1, 0); \
	reg_t r_arg2 = jit_get_reg_ex(jit, arg2, 0); \
	x86_cmp_reg_reg(jit->ip, r_arg1, r_arg2); \
	unsigned char * pos = jit->ip; \
	x86_branch(jit->ip, __cond, addr, __signed); \
	return pos; \
}

#define __BRANCH_COND_OP(reg_name, imm_name, cond, is_signed) \
	__BRANCH_COND_OP_REG(reg_name, cond, is_signed); \
	__BRANCH_COND_OP_IMM(imm_name, cond, is_signed);

#define __BRANCH_MASK_OP_IMM(__name,__cond) \
static inline unsigned char * __name(struct jit * jit, unsigned char * addr, reg_t arg, long imm) { \
	reg_t r_arg = jit_get_reg_ex(jit, arg, 0); \
	x86_test_reg_imm(jit->ip, r_arg, imm); \
	unsigned char * pos = jit->ip; \
	x86_branch(jit->ip, __cond, addr, 0); \
	return pos; \
}

#define __BRANCH_MASK_OP_REG(__name,__cond) \
static inline unsigned char * __name(struct jit * jit, unsigned char * addr, reg_t arg1, reg_t arg2) {\
	jit_touch_reg2(jit, arg1, arg2); \
	reg_t r_arg1 = jit_get_reg_ex(jit, arg1, 0); \
	reg_t r_arg2 = jit_get_reg_ex(jit, arg2, 0); \
	x86_test_reg_reg(jit->ip, r_arg1, r_arg2); \
	unsigned char * pos = jit->ip; \
	x86_branch(jit->ip, __cond, addr, 0); \
	return pos; \
}

#define __BRANCH_MASK_OP(reg_name, imm_name, cond) \
	__BRANCH_MASK_OP_REG(reg_name, cond); \
	__BRANCH_MASK_OP_IMM(imm_name, cond);

#define __COND_OP_IMM(__name,__cond,__signed) \
static inline void __name(struct jit * jit, reg_t target, reg_t arg, long imm) { \
	jit_touch_reg2(jit, target, arg); \
	reg_t r_arg = jit_get_reg_ex(jit, arg, 0); \
	reg_t r_target = jit_get_reg_ex(jit, target, 0); \
	x86_cmp_reg_imm(jit->ip, r_arg, imm); \
	x86_mov_reg_imm(jit->ip, r_target, 0); \
	x86_set_reg(jit->ip, __cond, r_target, __signed); \
}

#define __COND_OP_REG(__name,__cond,__signed) \
static inline void __name(struct jit * jit, reg_t target, reg_t arg1, reg_t arg2) {\
	jit_touch_reg3(jit, target, arg1, arg2); \
	reg_t r_target = jit_get_reg_ex(jit, target, 0); \
	reg_t r_arg1 = jit_get_reg_ex(jit, arg1, 0); \
	reg_t r_arg2 = jit_get_reg_ex(jit, arg2, 0); \
	x86_cmp_reg_reg(jit->ip, r_arg1, r_arg2); \
	x86_mov_reg_imm(jit->ip, r_target, 0); \
	x86_set_reg(jit->ip, __cond, r_target, __signed); \
}

#define __COND_OP(reg_name, imm_name, cond, is_signed) \
	__COND_OP_REG(reg_name, cond, is_signed); \
	__COND_OP_IMM(imm_name, cond, is_signed);


__BRANCH_COND_OP(jit_beqr, jit_beqi, X86_CC_EQ, 0)
__BRANCH_COND_OP(jit_bner, jit_bnei, X86_CC_NE, 0)
__BRANCH_COND_OP(jit_bltr, jit_blti, X86_CC_LT, 1)
__BRANCH_COND_OP(jit_bltr_u, jit_blti_u, X86_CC_LT, 0)
__BRANCH_COND_OP(jit_bler, jit_blei, X86_CC_LE, 1)
__BRANCH_COND_OP(jit_bler_u, jit_blei_u, X86_CC_LE, 0)
__BRANCH_COND_OP(jit_bgtr, jit_bgti, X86_CC_GT, 1)
__BRANCH_COND_OP(jit_bgtr_u, jit_bgti_u, X86_CC_GT, 0)
__BRANCH_COND_OP(jit_bger, jit_bgei, X86_CC_GE, 1)
__BRANCH_COND_OP(jit_bger_u, jit_bgei_u, X86_CC_GE, 0)

__BRANCH_MASK_OP(jit_bmsr, jit_bmsi, X86_CC_NZ)
__BRANCH_MASK_OP(jit_bmcr, jit_bmci, X86_CC_Z)

__COND_OP(jit_eqr, jit_eqi, X86_CC_EQ, 0)
__COND_OP(jit_ner, jit_nei, X86_CC_NE, 0)
__COND_OP(jit_ltr, jit_lti, X86_CC_LT, 1)
__COND_OP(jit_ltr_u, jit_lti_u, X86_CC_LT, 0)
__COND_OP(jit_ler, jit_lei, X86_CC_LE, 1)
__COND_OP(jit_ler_u, jit_lei_u, X86_CC_LE, 0)
__COND_OP(jit_gtr, jit_gti, X86_CC_GT, 1)
__COND_OP(jit_gtr_u, jit_gti_u, X86_CC_GT, 0)
__COND_OP(jit_ger, jit_gei, X86_CC_GE, 1)
__COND_OP(jit_ger_u, jit_gei_u, X86_CC_GE, 0)

void jit_return(struct jit * jit)
{
	x86_mov_reg_reg(jit->ip, X86_ESP, X86_EBP, 4);
	x86_pop_reg(jit->ip, X86_EBP);
	x86_ret(jit->ip);
}
