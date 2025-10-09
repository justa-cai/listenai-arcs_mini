#ifndef __LUNA_ISA_H__
#define __LUNA_ISA_H__

#include "luna_isa_macro.h"

#define INST_OP_CMP					(0b000010<<26)
#define INST_OP_B					(0b000011<<26)
#define INST_OP_LDR					(0b001010<<26)
#define INST_OP_STR					(0b001011<<26)
#define INST_OP_SETR				(0b010000<<26)
#define INST_OP_GOP					(0b010001<<26)
#define INST_OP_AND					(0b010010<<26)
#define INST_OP_ORR					(0b010011<<26)
#define INST_OP_LSR					(0b010100<<26)
#define INST_OP_LSL					(0b010101<<26)
#define INST_OP_MUL					(0b010110<<26)
#define INST_OP_MOV					(0b011000<<26)
#define INST_OP_WAIT				(0b100000<<26)

#define INST_B_MODE_OFFSE			( 0b0<<23 )
#define INST_B_MODE_DIRECT			( 0b1<<23 )
#define INST_B_MODE_LR				( 0b1<<22 )
#define INST_B_COND_GT				( 0b00<<20 )
#define INST_B_COND_EQ				( 0b01<<20 )
#define INST_B_COND_LT				( 0b10<<20 )
#define INST_B_COND_NONE			( 0b11<<20 )


#define INST_LDR_MODE_WORD			( 0b00<<21 )
#define INST_LDR_MODE_HALF_WORD		( 0b01<<21 )
#define INST_LDR_MODE_BYTE			( 0b10<<21 )
#define INST_LDR_NO_SIGN_EXT		( 0b1<<25 )
#define INST_LDR_SP_UPDATE			( 0b1<<24 )

#define INST_STR_MODE_WORD			( 0b00<<21 )
#define INST_STR_MODE_HALF_WORD		( 0b01<<21 )
#define INST_STR_MODE_BYTE			( 0b10<<21 )
#define INST_STR_NO_SIGN_EXT		( 0b1<<25 )
#define INST_STR_SP_UPDATE			( 0b1<<24 )

#define INST_GOP_MODE_ADD			( 0b0<<23 )
#define INST_GOP_MODE_SUB			( 0b1<<23 )
#define INST_GOP_MODE_LSB			( 0b10<<21 )
#define INST_GOP_MODE_MSB			( 0b01<<21 )
#define INST_GOP_MODE_WORD			( 0b00<<21 )

#define INST_AND_MODE_LSB			( 0b10<<21 )
#define INST_AND_MODE_MSB			( 0b01<<21 )
#define INST_AND_MODE_WORD			( 0b00<<21 )

#define INST_ORR_MODE_LSB			( 0b10<<21 )
#define INST_ORR_MODE_MSB			( 0b01<<21 )
#define INST_ORR_MODE_WORD			( 0b00<<21 )

#define INST_MOV_NO_SIGN_EXT		( 0b1<<25 )
#define INST_MOV_IMM				( 0b1<<24 )

#define INST_SETR_LOW				( 0b1100 << 21 )
#define INST_SETR_HIGH				( 0b0011 << 21 )

#define INST_WAIT_INTTERUPT			( 0b1 << 24 )

// macro definitions which comply the assembly rules
#define cmp(reg_dst, operand) 		( INST_OP_CMP | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define b(operand)					( INST_OP_B | INST_B_COND_NONE | (luna_isa_table_##operand) | ((~((luna_isa_table_##operand & (0b1<<24))>>1))&(0b1<<23)) )
#define bl(operand)					( INST_OP_B | INST_B_COND_NONE | INST_B_MODE_LR | (luna_isa_table_##operand) | ((~((luna_isa_table_##operand & (0b1<<24))>>1))&(0b1<<23)) )
#define bgt(operand)				( INST_OP_B | INST_B_COND_GT | (luna_isa_table_##operand) | ((~((luna_isa_table_##operand & (0b1<<24))>>1))&(0b1<<23)) )
#define blt(operand)				( INST_OP_B | INST_B_COND_LT | (luna_isa_table_##operand) | ((~((luna_isa_table_##operand & (0b1<<24))>>1))&(0b1<<23)) )
#define beq(operand)				( INST_OP_B | INST_B_COND_EQ |(luna_isa_table_##operand) | ((~((luna_isa_table_##operand & (0b1<<24))>>1))&(0b1<<23)) )
#define ldr(reg_dst, operand)		( INST_OP_LDR | INST_LDR_MODE_WORD | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define ldrb(reg_dst, operand) 		( INST_OP_LDR | INST_LDR_MODE_BYTE | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define ldrh(reg_dst, operand) 		( INST_OP_LDR | INST_LDR_MODE_HALF_WORD | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define ldrsb(reg_dst, operand) 	( INST_OP_LDR | INST_LDR_MODE_BYTE | INST_LDR_SIGN_EXT | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define ldrsh(reg_dst, operand) 	( INST_OP_LDR | INST_LDR_MODE_HALF_WORD | INST_LDR_SIGN_EXT | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define pop(reg_dst) 				( INST_OP_LDR | INST_LDR_SP_UPDATE | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_sp))
#define str(reg_dst, operand)		( INST_OP_STR | INST_STR_MODE_WORD | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define strb(reg_dst, operand) 		( INST_OP_STR | INST_STR_MODE_BYTE | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define strh(reg_dst, operand) 		( INST_OP_STR | INST_STR_MODE_HALF_WORD | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define strsb(reg_dst, operand) 	( INST_OP_STR | INST_STR_MODE_BYTE | INST_STR_SIGN_EXT | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define strsh(reg_dst, operand) 	( INST_OP_STR | INST_STR_MODE_HALF_WORD | INST_STR_SIGN_EXT | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define push(reg_dst) 				( INST_OP_STR | INST_STR_SP_UPDATE | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_sp))
#define setr(reg_dst, mod, operand)	( INST_OP_SETR | (luna_isa_table_reg_##reg_dst) | (mod<<21) | (operand) )
#define setrh(reg_dst, operand) 	( INST_OP_SETR | INST_SETR_HIGH | (luna_isa_table_reg_##reg_dst) | (operand&0x0000FFFF) )
#define setrl(reg_dst, operand) 	( INST_OP_SETR | INST_SETR_LOW | (luna_isa_table_reg_##reg_dst) | (operand&0x0000FFFF) )
#define add(reg_dst, operand) 		( INST_OP_GOP | INST_GOP_MODE_ADD | INST_GOP_MODE_WORD | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define addm(reg_dst, operand) 		( INST_OP_GOP | INST_GOP_MODE_ADD | INST_GOP_MODE_MSB | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define addl(reg_dst, operand) 		( INST_OP_GOP | INST_GOP_MODE_ADD | INST_GOP_MODE_LSB | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define sub(reg_dst, operand) 		( INST_OP_GOP | INST_GOP_MODE_SUB | INST_GOP_MODE_WORD | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define subm(reg_dst, operand) 		( INST_OP_GOP | INST_GOP_MODE_SUB | INST_GOP_MODE_MSB | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define subl(reg_dst, operand) 		( INST_OP_GOP | INST_GOP_MODE_SUB | INST_GOP_MODE_LSB | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define and(reg_dst, operand) 		( INST_OP_AND | INST_AND_MODE_WORD | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define andm(reg_dst, operand) 		( INST_OP_AND | INST_AND_MODE_MSB | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define andl(reg_dst, operand) 		( INST_OP_AND | INST_AND_MODE_LSB | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define orr(reg_dst, operand) 		( INST_OP_ORR | INST_ORR_MODE_WORD | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define orrm(reg_dst, operand) 		( INST_OP_ORR | INST_ORR_MODE_MSB | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define orrl(reg_dst, operand) 		( INST_OP_ORR | INST_ORR_MODE_LSB | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define lsr(reg_dst, operand) 		( INST_OP_LSR | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define lsl(reg_dst, operand) 		( INST_OP_LSL | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define mul(reg_dst, operand) 		( INST_OP_MUL | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define mov(reg_dst, operand) 		( INST_OP_MOV | INST_MOV_NO_SIGN_EXT | (luna_isa_table_reg_##reg_dst) | (luna_isa_table_##operand) )
#define wait(evt, ...)				( INST_OP_WAIT | (luna_isa_table_wait_##evt) | (luna_isa_table_wait_##__VA_ARGS__) )


// macro definitions which make the parameters variable
#define mov_reg_reg(rn, rx) 		( INST_OP_MOV | INST_MOV_NO_SIGN_EXT | (rn<<16) | (rx<<8) )
#define mov_reg_imm(rn, val) 		( INST_OP_MOV | INST_MOV_NO_SIGN_EXT | INST_MOV_IMM | (rn<<16) | (val) )
#define setrh_word(reg_dst, word)	( INST_OP_SETR | INST_SETR_HIGH | (luna_isa_table_reg_##reg_dst) | (word>>16) )
#define setrl_word(reg_dst, word)	( INST_OP_SETR | INST_SETR_LOW | (luna_isa_table_reg_##reg_dst) | (word&0x0000FFFF) )
#define setrh_reg_word(rn, word)	( INST_OP_SETR | INST_SETR_HIGH | (rn<<16) | (word>>16) )
#define setrl_reg_word(rn, word)	( INST_OP_SETR | INST_SETR_LOW | (rn<<16) | (word&0x0000FFFF) )
#define cmp_reg_reg(rn, rx) 		( INST_OP_CMP | (rn<<16) | (rx<<8) )
#define cmp_reg_imm(rn, val) 		( INST_OP_CMP | (rn<<16) | (val) )

#define ldr_reg_reg(rn, rx) 		( INST_OP_LDR | INST_LDR_MODE_WORD | (rn<<16) | (rx<<8) )
#define str_reg_reg(rn, rx) 		( INST_OP_STR | INST_STR_MODE_WORD | (rn<<16) | (rx<<8) )

#define add_reg_reg(rn, rx) 		( INST_OP_GOP | INST_GOP_MODE_ADD | INST_GOP_MODE_WORD | (rn<<16) | (rx<<8) )
#define sub_reg_reg(rn, rx) 		( INST_OP_GOP | INST_GOP_MODE_SUB | INST_GOP_MODE_WORD | (rn<<16) | (rx<<8) )
#define and_reg_reg(rn, rx) 		( INST_OP_AND | INST_AND_MODE_WORD | (rn<<16) | (rx<<8) )
#define orr_reg_reg(rn, rx) 		( INST_OP_ORR | INST_ORR_MODE_WORD | (rn<<16) | (rx<<8) )
#define lsr_reg_reg(rn, rx) 		( INST_OP_LSR | (rn<<16) | (rx<<8) )
#define lsl_reg_reg(rn, rx) 		( INST_OP_LSL | (rn<<16) | (rx<<8) )
#define mul_reg_reg(rn, rx) 		( INST_OP_MUL | (rn<<16) | (rx<<8) )


#endif /* __LUNA_ISA_H__ */
