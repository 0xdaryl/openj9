/*******************************************************************************
 * Copyright (c) 2021, 2022 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "j9.h"
#include "ut_j9vm.h"

/**
 * @file: UpCallThunkGen.cpp
 * @brief: Service routines dealing with platform-ABI specifics for upcall
 *
 * Given an upcallMetaData, an upcall thunk/adaptor will be generated;
 * Given an upcallSignature, argListPtr, and argIndex, a pointer to that specific arg will be returned
 */

extern "C" {

#if JAVA_SPEC_VERSION >= 16

#define STACK_SLOT_SIZE 8

#define ROUND_UP_TO_SLOT_MULTIPLE(s) ( ((s) + (STACK_SLOT_SIZE-1)) & (~(STACK_SLOT_SIZE-1)) )

#define MAX_GPRS 16
#define MAX_GPRS_PASSED_IN_REGS 6
#define MAX_FPRS_PASSED_IN_REGS 8

typedef enum StructPassingMechanismEnum {
	PASS_STRUCT_IN_MEMORY,
	PASS_STRUCT_IN_ONE_FPR,
	PASS_STRUCT_IN_TWO_FPR,
	PASS_STRUCT_IN_ONE_GPR_ONE_FPR,
	PASS_STRUCT_IN_ONE_FPR_ONE_GPR,
	PASS_STRUCT_IN_ONE_GPR,
	PASS_STRUCT_IN_TWO_GPR
} X64StructPassingMechanism;

#define REX	0x40
#define REX_W	0x08
#define REX_R	0x04
#define REX_X	0x02
#define REX_B	0x01

enum X64_GPR {
	rax,
	rcx,
	rdx,
	rbx,
	rsp,
	rbp,
	rsi,
	rdi,
	r8,
	r9,
	r10,
	r11,
	r12,
	r13,
	r14,
	r15
};

enum X64_FPR {
	xmm0,
	xmm1,
	xmm2,
	xmm3,
	xmm4,
	xmm5,
	xmm6,
	xmm7
};


struct modRM_encoding {
	uint8_t rexr;
	uint8_t reg;
	uint8_t rexb;
	uint8_t rm;
};


// Also used for xmm0 - xmm7
const struct modRM_encoding modRM[MAX_GPRS] = {

	//  rexr   reg       rexb   rm
        //  ------ --------- ------ ---
 	  { 0,     0x0 << 3, 0,     0x0 }, // rax / xmm0
	  { 0,     0x1 << 3, 0,     0x1 }, // rcx / xmm1
	  { 0,     0x2 << 3, 0,     0x2 }, // rdx / xmm2
	  { 0,     0x3 << 3, 0,     0x3 }, // rbx / xmm3
	  { 0,     0x4 << 3, 0,     0x4 }, // rsp / xmm4
	  { 0,     0x5 << 3, 0,     0x5 }, // rbp / xmm5
	  { 0,     0x6 << 3, 0,     0x6 }, // rsi / xmm6
	  { 0,     0x7 << 3, 0,     0x7 }, // rdi / xmm7
	  { REX_R, 0x0 << 3, REX_B, 0x0 }, // r8
	  { REX_R, 0x1 << 3, REX_B, 0x1 }, // r9
	  { REX_R, 0x2 << 3, REX_B, 0x2 }, // r10
	  { REX_R, 0x3 << 3, REX_B, 0x3 }, // r11
	  { 0x0,   0x0,      0x0,   0x0 }, // r12 (requires SIB to encode)
	  { REX_R, 0x5 << 3, REX_B, 0x5 }, // r13
	  { REX_R, 0x6 << 3, REX_B, 0x6 }, // r14
	  { REX_R, 0x7 << 3, REX_B, 0x7 }  // r15
};

const I_8 registerValues[MAX_GPRS] = {
	0,  // rax
	1,  // rcx
	2,  // rdx
	3,  // rbx
	4,  // rsp
	5,  // rbp
	6,  // rsi
	7,  // rdi
	0,  // r8
	1,  // r9
	2,  // r10
	3,  // r11
	4,  // r12
	5,  // r13
	6,  // r14
	7   // r15
};

const X64_GPR gprParmRegs[MAX_GPRS_PASSED_IN_REGS] = {
	rdi,
	rsi,
	rdx,
	rcx,
	r8,
	r9
};

const X64_FPR fprParmRegs[MAX_FPRS_PASSED_IN_REGS] = {
	xmm0,
	xmm1,
	xmm2,
	xmm3,
	xmm4,
	xmm5,
	xmm6,
	xmm7
};

typedef struct structParmInMemoryMetaData {

	// Stack offset to struct parm passed in memory
	I_32 memParmCursor;

	// Stack offset to argList entry to copy struct parm
	I_32 frameOffsetCursor;

	// Size of the struct parm in bytes
	U_32 sizeofStruct;

	// Windows ONLY
	// memPointerRegister
	// memPointerOffset (or 0 if passed in register)
} structParmInMemoryMetaDataStruct;

#define IS_32BIT_SIGNED(x) ((x) == (int32_t)(x))

// -----------------------------------------------------------------------------
// MOV treg, [rsp + disp32]
//
#define L8_TREG_mRSP_DISP32m(cursor, treg, disp32) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x8b; \
	*cursor++ = 0x84 | modRM[treg].reg; \
	*rex |= modRM[treg].rexr; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// REX + op + modRM + SIB + disp32
#define L8_TREG_mRSP_DISP32m_LENGTH (1+3+4)

// -----------------------------------------------------------------------------
// MOV treg, [sreg + disp8]
//
// Note: this does not encode sreg=r12 (requires a SIB byte)
//
#define L8_TREG_mSREG_DISP8m(cursor, treg, sreg, disp8) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x8b; \
	*cursor++ = 0x40 | modRM[treg].reg | modRM[sreg].rm; \
	*rex |= (modRM[treg].rexr | modRM[sreg].rexb); \
	*(int32_t *)cursor = disp8; \
	cursor += 1; \
	}

// REX + op + modRM + disp8
#define L8_TREG_mSREG_DISP8m_LENGTH (1+2+1)

// -----------------------------------------------------------------------------
// MOV treg, [sreg]
//
// Note: this does not encode sreg=r12 (requires a SIB byte)
//
#define L8_TREG_mSREGm(cursor, treg, sreg) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x8b; \
	*cursor++ = 0x00 | modRM[treg].reg | modRM[sreg].rm; \
	*rex |= (modRM[treg].rexr | modRM[sreg].rexb); \
	}

// REX + op + modRM
#define L8_TREG_mSREGm_LENGTH (1+2)

// -----------------------------------------------------------------------------
// MOV [rsp + disp32], sreg
//
#define S8_mRSP_DISP32m_SREG(cursor, disp32, sreg) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x89; \
	*cursor++ = 0x84 | modRM[sreg].reg; \
	*rex |= modRM[sreg].rexr; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// REX + op + modRM + SIB + disp32
#define S8_mRSP_DISP32m_SREG_LENGTH (1+3+4)

// -----------------------------------------------------------------------------
// MOVSS treg, [rsp + disp32]
//
#define MOVSS_TREG_mRSP_DISP32m(cursor, treg, disp32) \
	{ \
	*cursor++ = 0xf3; \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x84 | modRM[treg].reg; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// 3*op + modRM + SIB + disp32
#define MOVSS_TREG_mRSP_DISP32m_LENGTH (3+2+4)

// -----------------------------------------------------------------------------
// MOVSS treg, [sreg]
//
#define MOVSS_TREG_mSREGm(cursor, treg, sreg) \
	{ \
	*cursor++ = 0xf3; \
	if (modRM[sreg].rexb) { \
		*cursor++ = REX | modRM[sreg].rexb; \
	} \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x00 | modRM[treg].reg | modRM[sreg].rm; \
	}

// REX + 3*op + modRM
// Note: REX is always conservatively included in this calculation
#define MOVSS_TREG_mSREGm_LENGTH (1+3+1)

// -----------------------------------------------------------------------------
// MOVSS treg, [sreg + disp8]
//
#define MOVSS_TREG_mSREG_DISP8m(cursor, treg, sreg, disp8) \
	{ \
	*cursor++ = 0xf3; \
	if (modRM[sreg].rexb) { \
		*cursor++ = REX | modRM[sreg].rexb; \
	} \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x40 | modRM[treg].reg | modRM[sreg].rm; \
	*(int8_t *)cursor = disp8; \
	cursor += 1; \
	}

// REX + 3*op + modRM + disp8
// Note: REX is always conservatively included in this calculation
#define MOVSS_TREG_mSREG_DISP8m_LENGTH (1+3+1+1)

// -----------------------------------------------------------------------------
// MOVSS [rsp + disp32], sreg
//
#define MOVSS_mRSP_DISP32m_SREG(cursor, disp32, sreg) \
	{ \
	*cursor++ = 0xf3; \
	*cursor++ = 0x0f; \
	*cursor++ = 0x11; \
	*cursor++ = 0x84 | modRM[sreg].reg; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// 3*op + modRM + SIB + disp32
#define MOVSS_mRSP_DISP32m_SREG_LENGTH (3+2+4)

// -----------------------------------------------------------------------------
// MOVSD treg, [rsp + disp32]
//
#define MOVSD_TREG_mRSP_DISP32m(cursor, treg, disp32) \
	{ \
	*cursor++ = 0xf2; \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x84 | modRM[treg].reg; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// 3*op + modRM + SIB + disp32
#define MOVSD_TREG_mRSP_DISP32m_LENGTH (3+2+4)

// -----------------------------------------------------------------------------
// MOVSD treg, [sreg]
//
#define MOVSD_TREG_mSREGm(cursor, treg, sreg) \
	{ \
	*cursor++ = 0xf2; \
	if (modRM[sreg].rexb) { \
		*cursor++ = REX | modRM[sreg].rexb; \
	} \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x00 | modRM[treg].reg | modRM[sreg].rm; \
	}

// REX + 3*op + modRM
// Note: REX is always conservatively included in this calculation
#define MOVSD_TREG_mSREGm_LENGTH (1+3+1)

// -----------------------------------------------------------------------------
// MOVSD treg, [sreg + disp8]
//
#define MOVSD_TREG_mSREG_DISP8m(cursor, treg, sreg, disp8) \
	{ \
	*cursor++ = 0xf2; \
	if (modRM[sreg].rexb) { \
		*cursor++ = REX | modRM[sreg].rexb; \
	} \
	*cursor++ = 0x0f; \
	*cursor++ = 0x10; \
	*cursor++ = 0x40 | modRM[treg].reg | modRM[sreg].rm; \
	*(int8_t *)cursor = disp8; \
	cursor += 1; \
	}

// REX + 3*op + modRM + disp8
// Note: REX is always conservatively included in this calculation
#define MOVSD_TREG_mSREG_DISP8m_LENGTH (1+3+1+1)

// -----------------------------------------------------------------------------
// MOVSD [rsp + disp32], sreg
//
#define MOVSD_mRSP_DISP32m_SREG(cursor, disp32, sreg) \
	{ \
	*cursor++ = 0xf2; \
	*cursor++ = 0x0f; \
	*cursor++ = 0x11; \
	*cursor++ = 0x84 | modRM[sreg].reg; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// 3*op + modRM + SIB + disp32
#define MOVSD_mRSP_DISP32m_SREG_LENGTH (3+2+4)


// -----------------------------------------------------------------------------
// MOV treg, imm32
//
#define MOV_TREG_IMM32(cursor, treg, imm32) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0xc7; \
	*cursor++ = 0xc0 | modRM[treg].rm; \
	*rex |= modRM[treg].rexb; \
	*(int32_t *)cursor = imm32; \
	cursor += 4; \
	}

// REX + op + modRM + imm32
#define MOV_TREG_IMM32_LENGTH (1+2+4)

// -----------------------------------------------------------------------------
// MOV treg, imm64
//
#define MOV_TREG_IMM64(cursor, treg, imm64) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0xb8 | modRM[treg].rm; \
	*rex |= modRM[treg].rexb; \
	*(int64_t *)cursor = imm64; \
	cursor += 8; \
	}

// REX + op + imm64
#define MOV_TREG_IMM64_LENGTH (1+1+8)

// -----------------------------------------------------------------------------
// MOV treg, sreg
//
#define MOV_TREG_SREG(cursor, treg, sreg) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x89; \
	*cursor++ = 0xc0 | modRM[treg].rm | modRM[sreg].reg; \
	*rex |= modRM[treg].rexb | modRM[sreg].rexr; \
	}

// REX + op + modRM
#define MOV_TREG_SREG_LENGTH (3)

// -----------------------------------------------------------------------------
// LEA treg, [rsp + disp32]
//
#define LEA_TREG_mRSP_DISP32m(cursor, treg, disp32) \
	{ \
	*cursor = REX | REX_W; \
	uint8_t *rex = cursor++; \
	*cursor++ = 0x8d; \
	*cursor++ = 0x84 | modRM[treg].reg; \
	*rex |= modRM[treg].rexr; \
	*cursor++ = 0x24; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// REX + op + modRM + SIB + disp32
#define LEA_TREG_mRSP_DISP32m_LENGTH (1+3+4)

// -----------------------------------------------------------------------------
// SUB rsp, imm32
//
#define SUB_RSP_IMM32(cursor, imm32) \
	*cursor++ = REX | REX_W; \
	*cursor++ = 0x81; \
	*cursor++ = 0xec; \
	*(int32_t *)cursor = imm32; \
	cursor += 4;

// REX + op + modRM + imm32
#define SUB_RSP_IMM32_LENGTH (1+2+4)

// -----------------------------------------------------------------------------
// SUB rsp, imm8
//
#define SUB_RSP_IMM8(cursor, imm8) \
	*cursor++ = REX | REX_W; \
	*cursor++ = 0x83; \
	*cursor++ = 0xec; \
	*cursor++ = imm8;

// REX + op + modRM + imm8
#define SUB_RSP_IMM8_LENGTH (1+2+1)

// -----------------------------------------------------------------------------
// ADD rsp, imm32
//
#define ADD_RSP_IMM32(cursor, imm32) \
	*cursor++ = REX | REX_W; \
	*cursor++ = 0x81; \
	*cursor++ = 0xc4; \
	*(int32_t *)cursor = imm32; \
	cursor += 4;

// REX + op + modRM + disp32
#define ADD_RSP_IMM32_LENGTH (1+2+4)

// -----------------------------------------------------------------------------
// ADD rsp, imm8
//
#define ADD_RSP_IMM8(cursor, imm8) \
	*cursor++ = REX | REX_W; \
	*cursor++ = 0x83; \
	*cursor++ = 0xc4; \
	*cursor++ = imm8;

// REX + op + modRM + imm8
#define ADD_RSP_IMM8_LENGTH (1+2+1)

// -----------------------------------------------------------------------------
// PUSH sreg
//
#define PUSH_SREG(cursor, sreg) \
	{ \
	if (modRM[sreg].rexb) { \
		*cursor++ = REX | modRM[sreg].rexb; \
	} \
	*cursor++ = 0x50 + registerValues[sreg]; \
	}

// REX + op
#define PUSH_SREG_LENGTH(sreg) ((modRM[sreg].rexb ? 1 : 0) + 1)

// -----------------------------------------------------------------------------
// POP sreg
//
#define POP_SREG(cursor, treg) \
	{ \
	if (modRM[treg].rexb) { \
		*cursor++ = REX | modRM[treg].rexb; \
	} \
	*cursor++ = 0x58 + registerValues[treg]; \
	}

// REX + op
#define POP_SREG_LENGTH(treg) ((modRM[treg].rexb ? 1 : 0) + 1)

// -----------------------------------------------------------------------------
// CALL [sreg + disp32]
//
#define CALL_mSREG_DISP32m(cursor, sreg, disp32) \
	{ \
	if (modRM[sreg].rexb) { \
		*cursor++ = (REX | modRM[sreg].rexb); \
	} \
	*cursor++ = 0xff; \
	*cursor++ = 0x90 | modRM[sreg].rm; \
	*(int32_t *)cursor = disp32; \
	cursor += 4; \
	}

// REX + op + modRM + disp32
#define CALL_mSREG_DISP32m_LENGTH(sreg) ((modRM[sreg].rexb ? 1 : 0) + 2 + 4)

// -----------------------------------------------------------------------------
// CALL [sreg + disp8]
//
#define CALL_mSREG_DISP8m(cursor, sreg, disp8) \
	{ \
	if (modRM[sreg].rexb) { \
		*cursor++ = (REX | modRM[sreg].rexb); \
	} \
	*cursor++ = 0xff; \
	*cursor++ = 0x50 | modRM[sreg].rm; \
	*cursor++ = disp8; \
	}

// REX + op + modRM + disp32
#define CALL_mSREG_DISP8m_LENGTH(sreg) ((modRM[sreg].rexb ? 1 : 0) + 2 + 1)

// -----------------------------------------------------------------------------
// RET
//
#define RET(cursor) \
	{ \
	*cursor++ = 0xc3; \
	}

#define RET_LENGTH (1)

// -----------------------------------------------------------------------------
// INT3
//
#define INT3(cursor) \
	{ \
	*cursor++ = 0xcc; \
	}

#define INT3_LENGTH (1)

// -----------------------------------------------------------------------------
// REP MOVSB
//
#define REP_MOVSB(cursor) \
	{ \
	*cursor++ = 0xf3; \
	*cursor++ = 0xa4; \
	}

#define REP_MOVSB_LENGTH (2)

#define ROUND_UP_SLOT(si)     (((si) + 7) / 8)


/**
 * Calculate the buffer size required to emit the following:
 *
 *    lea rsi, [rsp + sourceOffset]
 *    lea rdi, [rsp + destOffset]
 *    mov rcx, structSize
 *    rep movsb
 */
static I_32
calculateCopyStructInstructionsByteCount(J9UpcallSigType structParm) {

	return (LEA_TREG_mRSP_DISP32m_LENGTH
	      + LEA_TREG_mRSP_DISP32m_LENGTH
	      + MOV_TREG_IMM32_LENGTH
	      + REP_MOVSB_LENGTH);
}

static X64StructPassingMechanism
analyzeStructParm(I_32 gprRegParmCount, I_32 fprRegParmCount, J9UpcallSigType structParm) {

	I_32 structSize = structParm.sizeInByte;

	if (structSize > 16) {
		// On Linux, passed as an arg on the stack (not a pointer)
		// On Windows, memory allocated on the stack but passed as a pointer to that memory.  Stack memory is checked when locals > 8k before allocation (__chkstk)
		return PASS_STRUCT_IN_MEMORY;
	}

	switch (structParm.type) {
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
		{
			I_32 numParmFPRsRequired = (structSize <= 8) ? 1 : 2;
			if (fprRegParmCount + numParmFPRsRequired > MAX_FPRS_PASSED_IN_REGS) {
				// Struct is allocated on the stack corresponding to the arg position (i.e., not passed as a pointer to memory)
				// Struct rounded up to multiple of 8 bytes
				return PASS_STRUCT_IN_MEMORY;
			}

			// Pass in next available 1 or next available 2 numParmFPRsRequired
			return (numParmFPRsRequired == 1) ? PASS_STRUCT_IN_ONE_FPR : PASS_STRUCT_IN_TWO_FPR;
		}

		/* Windows
                4, 8, 16, 32, 64 bits passed in GPR registers
		any other length passed in memory

		1,2 floats passed in GPR corresponding to arg position if <= 4 or memory if in pos >= 5
		> 64 bits passed in mem as pointer, address in GPR corresponding to arg position if <=4 or memory if in pos >= 5

		1 double passed in GPR corresponding to arg position if <= 4 or memory if in pos >= 5
		> 64 bits (more than one double) passed in mem as a pointer, address in GPR corresponding to arg position if <=4 or memory if in pos >= 5
		*/

		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:     /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:     /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP:  /* Fall through */
		{
			// Linux: Pass in next available 2 numParmFPRsRequired; otherwise pass on stack
			// 2 floats are packed into a single XMM and/or occupy consecutive 4-byte slots in memory
			if (fprRegParmCount + 2 > MAX_FPRS_PASSED_IN_REGS) {
				return PASS_STRUCT_IN_MEMORY;
			}

			// Pass in next available 2 numParmFPRsRequired
			return PASS_STRUCT_IN_TWO_FPR;

			// Windows: Pass pointer to struct on stack
		}
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:
		{
			// Pass Misc in first avail GPR, DP in first avail FPR
			// Pass on stack if neither available
			if ((gprRegParmCount + 1 > MAX_GPRS_PASSED_IN_REGS) ||
			    (fprRegParmCount + 1 > MAX_FPRS_PASSED_IN_REGS)) {
				return PASS_STRUCT_IN_MEMORY;
			}

			return PASS_STRUCT_IN_ONE_GPR_ONE_FPR;

			// Windows: Pass pointer to struct on stack
		}
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:
		{
			// Pass Misc in first avail GPR, DP in first avail FPR
			// Pass on stack if neither available
			if ((gprRegParmCount + 1 > MAX_GPRS_PASSED_IN_REGS) ||
			    (fprRegParmCount + 1 > MAX_FPRS_PASSED_IN_REGS)) {
				return PASS_STRUCT_IN_MEMORY;
			}

			return PASS_STRUCT_IN_ONE_FPR_ONE_GPR;

			// Windows: Pass pointer to struct on stack
		}
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
		{
			// First avail GPR + Second Avail GPR
			// Pass on stack otherwise
			I_32 numParmGPRsRequired = (structSize <= 8) ? 1 : 2;
			if (gprRegParmCount + numParmGPRsRequired > MAX_GPRS_PASSED_IN_REGS) {
				return PASS_STRUCT_IN_MEMORY;
			}

			// Pass in next available 1 or next available 2 numParmGPRsRequired
			return (numParmGPRsRequired == 1) ? PASS_STRUCT_IN_ONE_GPR : PASS_STRUCT_IN_TWO_GPR;

			// Windows:
			// If length <= 8, pass in GPR
			// Else pass pointer to struct on stack
		}
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
		{
			// struct length > 16
			return PASS_STRUCT_IN_MEMORY;
			break;
		}
		default:
		{
			Assert_VM_unreachable();
			return PASS_STRUCT_IN_MEMORY;
		}
	}
}



/**
 * @brief Generate the appropriate thunk/adaptor for a given J9UpcallMetaData
 *
 * @param metaData[in/out] a pointer to the given J9UpcallMetaData
 * @return the address for this future upcall function handle, either the thunk or the thunk-descriptor
 *
 * Details:
 *   A thunk or adaptor is mainly composed of 4 parts of instructions to be counted separately:
 *   1) the eventual call to the upcallCommonDispatcher (fixed number of instructions)
 *   2) if needed, instructions to build a stack frame
 *   3) pushing in-register arguments back to the stack, either in newly-built frame or caller frame
 *   4) if needed, instructions to distribute  the java result back to the native side appropriately
 *
 *     1) and 3) are mandatory, while 2) and 4) depend on the particular signature under consideration.
 *     2) is most likely needed, since the caller frame might not contain the parameter area in most cases;
 *     4) implies needing 2), since this adaptor expects a return from java side before returning
 *        to the native caller.
 *
 *   there are two different scenarios under 4):
 *      a) distribute  result back into register-containable aggregates (either homogeneous FP or not)
 *      b) copy the result back to the area designated by the hidden-parameter
 *
 *   most of the complexities are due to handling ALL_SP homogeneous struct: the register image and
 *   memory image are different, such that it can lead to the situation in which FPR parameter registers
 *   run out before GPR parameter registers. In that case, floating point arguments need to be passed
 *   in GPRs (and in the right position if it is an SP).
 *
 *   when a new frame is needed, it is at least 32bytes in size and 16-byte-aligned. this implementation
 *   going as follows: if caller-frame parameter area can be used, the new frame will be of fixed 48-byte
 *   size; otherwise, it will be of [64 + round-up-16(parameterArea)].
 */
void *
createUpcallThunk(J9UpcallMetaData *metaData)
{
	J9JavaVM *vm = metaData->vm;
	PORT_ACCESS_FROM_JAVAVM(vm);
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9UpcallSigType *sigArray = metaData->nativeFuncSignature->sigArray;
	I_32 lastSigIdx = (I_32)(metaData->nativeFuncSignature->numSigs - 1); // The index of the return type in the signature array
	I_32 tempInt = 0;
	I_32 gprRegSpillInstructionCount = 0;
	I_32 gprRegFillInstructionCount = 0;
	I_32 fprRegSpillInstructionCount = 0;
	I_32 fprRegFillInstructionCount = 0;
	I_32 gprRegParmCount = 0;
	I_32 fprRegParmCount = 0;
	I_32 copyStructInstructionsByteCount = 0;
	I_32 prepareStructReturnInstructionsLength = 0;
	I_32 numStructsPassedInMemory = 0;
	I_32 stackSlotCount = 0;
	I_32 preservedRegisterAreaSize = 0;
	bool hiddenParameter = false;

	Assert_VM_true(lastSigIdx >= 0);

printf("XXXXX createUpcallThunk : metaData=%p, upCallCommonDispatcher=%p\n", metaData, metaData->upCallCommonDispatcher);

	// -------------------------------------------------------------------------------
	// Set up the appropriate VM upcall dispatch function based the return type
	// -------------------------------------------------------------------------------

	tempInt = sigArray[lastSigIdx].sizeInByte;
        switch (sigArray[lastSigIdx].type) {
		case J9_FFI_UPCALL_SIG_TYPE_VOID:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcall0;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_CHAR:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_SHORT: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_INT32:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcall1;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_POINTER:
		case J9_FFI_UPCALL_SIG_TYPE_INT64:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcallJ;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcallF;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcallD;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:   /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:   /* Fall through */
                case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:     /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
		{
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcallStruct;
			X64StructPassingMechanism mechanism = analyzeStructParm(0, 0, sigArray[lastSigIdx]);
			switch (mechanism) {
				case PASS_STRUCT_IN_MEMORY:
					hiddenParameter = true;
					gprRegParmCount++;

					// A preserved register will hold the hidden parameter across the function call.
					// The preserved register must therefore be saved/restored.
					preservedRegisterAreaSize = 8;

					prepareStructReturnInstructionsLength +=
						 (MOV_TREG_SREG_LENGTH   /* mov rbx, rax (preserve hidden parameter) */
						+ MOV_TREG_SREG_LENGTH   /* mov rsi, rax (return value from call) */
						+ MOV_TREG_SREG_LENGTH   /* mov rdi, rbx (address from preserved hidden parameter) */
	      					+ MOV_TREG_IMM32_LENGTH
						+ REP_MOVSB_LENGTH
						+ MOV_TREG_SREG_LENGTH); /* mov rax, rbx (return caller-supplied  buffer in rax) */
printf("XXXXX return : PASS_STRUCT_IN_MEMORY\n");
					break;
				case PASS_STRUCT_IN_ONE_FPR:
					prepareStructReturnInstructionsLength += MOVSD_TREG_mSREGm_LENGTH;
printf("XXXXX return : PASS_STRUCT_IN_ONE_FPR\n");
					break;
				case PASS_STRUCT_IN_TWO_FPR:
					prepareStructReturnInstructionsLength +=
						(MOVSD_TREG_mSREGm_LENGTH + MOVSD_TREG_mSREG_DISP8m_LENGTH);
printf("XXXXX return : PASS_STRUCT_IN_TWO_FPR\n");
					break;
				case PASS_STRUCT_IN_ONE_GPR_ONE_FPR:
					prepareStructReturnInstructionsLength +=
						(MOVSD_TREG_mSREG_DISP8m_LENGTH + L8_TREG_mSREGm_LENGTH);
printf("XXXXX return : PASS_STRUCT_IN_ONE_GPR_ONE_FPR\n");
					break;
				case PASS_STRUCT_IN_ONE_FPR_ONE_GPR:
					prepareStructReturnInstructionsLength +=
						(MOVSD_TREG_mSREGm_LENGTH + L8_TREG_mSREG_DISP8m_LENGTH);
printf("XXXXX return : PASS_STRUCT_IN_ONE_FPR_ONE_GPR\n");
					break;
				case PASS_STRUCT_IN_ONE_GPR:
					prepareStructReturnInstructionsLength += L8_TREG_mSREGm_LENGTH;
printf("XXXXX return : PASS_STRUCT_IN_ONE_GPR\n");
					break;
				case PASS_STRUCT_IN_TWO_GPR:
					prepareStructReturnInstructionsLength +=
						(L8_TREG_mSREG_DISP8m_LENGTH + L8_TREG_mSREGm_LENGTH);
printf("XXXXX return : PASS_STRUCT_IN_TWO_GPR\n");
					break;
				default:
					Assert_VM_unreachable();
			}

			break;
		}
		default:
			Assert_VM_unreachable();
	}

	// -------------------------------------------------------------------------------
	// Determine instruction and buffer requirements from each parameter
	// -------------------------------------------------------------------------------

	for (I_32 i = 0; i < lastSigIdx; i++) {
		tempInt = sigArray[i].sizeInByte;

printf("XXXXX arg %d : type=%d, size=%d : ", i, sigArray[i].type, tempInt);

		switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
			{
				stackSlotCount++;

				if (gprRegParmCount < MAX_GPRS_PASSED_IN_REGS) {
					// Parm must be spilled from parm register to argList
					gprRegParmCount++;
					gprRegSpillInstructionCount++;
printf("  REG : gprRegParmCount=%d, gprRegSpillInstructionCount=%d\n", gprRegParmCount, gprRegSpillInstructionCount);
				} else {
					// Parm must be filled from frame and spilled to argList
					gprRegFillInstructionCount++;
					gprRegSpillInstructionCount++;
printf("  MEM : gprRegFillInstructionCount=%d, gprRegSpillInstructionCount=%d\n", gprRegFillInstructionCount, gprRegSpillInstructionCount);
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			{
				stackSlotCount += 1;

				if (fprRegParmCount < MAX_FPRS_PASSED_IN_REGS) {
					// Parm must be spilled from parm register to argList
					fprRegParmCount += 1;
					fprRegSpillInstructionCount += 1;
printf("  REG : fprRegParmCount=%d, fprRegSpillInstructionCount=%d\n", fprRegParmCount, fprRegSpillInstructionCount);
				} else {
					// Parm must be filled from frame and spilled to argList
					fprRegFillInstructionCount += 1;
					fprRegSpillInstructionCount += 1;
printf("  MEM : fprRegFillInstructionCount=%d, fprRegSpillInstructionCount=%d\n", fprRegFillInstructionCount, fprRegSpillInstructionCount);
				}

				break;
			}
			default:
			{
				stackSlotCount += ROUND_UP_TO_SLOT_MULTIPLE(sigArray[i].sizeInByte) / STACK_SLOT_SIZE;

				X64StructPassingMechanism mechanism = analyzeStructParm(gprRegParmCount, fprRegParmCount, sigArray[i]);
				switch (mechanism) {
					case PASS_STRUCT_IN_MEMORY:
						copyStructInstructionsByteCount += calculateCopyStructInstructionsByteCount(sigArray[i]);
						numStructsPassedInMemory += 1;
printf("  MEM : PASS_STRUCT_IN_MEMORY : copyStructInstructionsByteCount=%d, numStructsPassedInMemory=%d\n", copyStructInstructionsByteCount, numStructsPassedInMemory);
						break;

					case PASS_STRUCT_IN_ONE_FPR:
						// Parm must be spilled from parm register to argList
						fprRegParmCount += 1;
						fprRegSpillInstructionCount += 1;
printf("  REG : PASS_STRUCT_IN_ONE_FPR : fprRegParmCount=%d, fprRegSpillInstructionCount=%d\n", fprRegParmCount, fprRegSpillInstructionCount);
						break;

					case PASS_STRUCT_IN_TWO_FPR:
						// Parm must be spilled from two parm registers to argList
						fprRegParmCount += 2;
						fprRegSpillInstructionCount += 2;
printf("  REG : PASS_STRUCT_IN_TWO_FPR : fprRegParmCount=%d, fprRegSpillInstructionCount=%d\n", fprRegParmCount, fprRegSpillInstructionCount);
						break;

					case PASS_STRUCT_IN_ONE_GPR_ONE_FPR:  /* Fall through */
					case PASS_STRUCT_IN_ONE_FPR_ONE_GPR:
						// Parm must be spilled from two parm registers to argList
						gprRegParmCount += 1;
						gprRegSpillInstructionCount += 1;
						fprRegParmCount += 1;
						fprRegSpillInstructionCount += 1;
printf("  REG : PASS_STRUCT_IN_ONE_GPR_ONE_FPR (or FPR/GPR) : gprRegParmCount=%d, gprRegSpillInstructionCount=%d, fprRegParmCount=%d, fprRegSpillInstructionCount=%d\n", gprRegParmCount, gprRegSpillInstructionCount, fprRegParmCount, fprRegSpillInstructionCount);
						break;

					case PASS_STRUCT_IN_ONE_GPR:
						// Parm must be spilled from parm register to argList
						gprRegParmCount += 1;
						gprRegSpillInstructionCount += 1;
printf("  REG : PASS_STRUCT_IN_ONE_GPR : gprRegParmCount=%d, gprRegSpillInstructionCount=%d\n", gprRegParmCount, gprRegSpillInstructionCount);
						break;

					case PASS_STRUCT_IN_TWO_GPR:
						// Parm must be spilled from two parm registers to argList
						gprRegParmCount += 2;
						gprRegSpillInstructionCount += 2;
printf("  REG : PASS_STRUCT_IN_TWO_GPR : gprRegParmCount=%d, gprRegSpillInstructionCount=%d\n", gprRegParmCount, gprRegSpillInstructionCount);
						break;

					default:
						Assert_VM_unreachable();
				}
			}
		}
	}

	// -------------------------------------------------------------------------------
	// Calculate size of VM parameter buffer
	// -------------------------------------------------------------------------------

	I_32 frameSize = stackSlotCount * STACK_SLOT_SIZE;

	// Adjust frame size such that the end of the input argument area is a multiple of 16.
	if ((frameSize + preservedRegisterAreaSize) % 16 == 0) {
		frameSize += STACK_SLOT_SIZE;
	}

printf("XXXXX stackSlotCount=%d, preservedRegisterAreaSize=%d, frameSize=%d\n", stackSlotCount, preservedRegisterAreaSize, frameSize);

	// -------------------------------------------------------------------------------
	// Allocate thunk memory
	// -------------------------------------------------------------------------------

	I_32 thunkSize = 0;
	I_32 roundedCodeSize = 0;
	I_32 breakOnEntry = 1;

	if (breakOnEntry) {
		thunkSize += INT3_LENGTH;
	}

	if (hiddenParameter) {
		// Save and restore preserved register
		thunkSize += (PUSH_SREG_LENGTH(rbx) + POP_SREG_LENGTH(rbx));
	}

	if (frameSize >= -128 && frameSize <= 127) {
		thunkSize += SUB_RSP_IMM8_LENGTH;
		thunkSize += ADD_RSP_IMM8_LENGTH;
	} else {
		thunkSize += SUB_RSP_IMM32_LENGTH;
		thunkSize += ADD_RSP_IMM32_LENGTH;
	}

	thunkSize += gprRegFillInstructionCount * L8_TREG_mRSP_DISP32m_LENGTH
	           + gprRegSpillInstructionCount * S8_mRSP_DISP32m_SREG_LENGTH;

	// MOVSS and MOVSD instructions are the same size
	thunkSize += fprRegFillInstructionCount * MOVSS_TREG_mRSP_DISP32m_LENGTH
	           + fprRegSpillInstructionCount * MOVSS_mRSP_DISP32m_SREG_LENGTH;

	thunkSize += copyStructInstructionsByteCount;

	Assert_VM_true(offsetof(J9UpcallMetaData, upCallCommonDispatcher) <= 127);

	thunkSize += MOV_TREG_IMM64_LENGTH
	           + MOV_TREG_SREG_LENGTH
	           + CALL_mSREG_DISP8m_LENGTH(rdi)
	           + RET_LENGTH;

	thunkSize += prepareStructReturnInstructionsLength;

	roundedCodeSize = ROUND_UP_TO_SLOT_MULTIPLE(thunkSize);

	// +8 accounts for cached J9UpcallMetaData pointer after thunk code
	roundedCodeSize += 8;

	metaData->thunkSize = roundedCodeSize;

	U_8 *thunkMem = (U_8 *)vmFuncs->allocateUpcallThunkMemory(metaData);
	if (NULL == thunkMem) {
printf("XXXXX allocateUpcallThunkMemory FAILED\n");
		return NULL;
	}
	metaData->thunkAddress = (void *)thunkMem;

printf("XXXXX roundedCodeSize = %d, thunkAddress = %p, frameSize = %d\n", roundedCodeSize, thunkMem, frameSize);

	// -------------------------------------------------------------------------------
	// Emit thunk instructions
	// -------------------------------------------------------------------------------

	I_32 frameOffsetCursor = 0;
	I_32 memParmCursor = 0;
	I_32 numStructsPassedInMemoryCursor = 0;
	gprRegParmCount = 0;
	fprRegParmCount = 0;
	structParmInMemoryMetaDataStruct *structParmInMemory = NULL;

	if (numStructsPassedInMemory > 0) {
		/**
		 * Allocate a bookkeeping structure for struct parms passed in memory to avoid
		 * a complete traversal of the parameters again.  The memory for this structure
		 * will be freed in this function once the information is consumed.
		 */
		structParmInMemory = (structParmInMemoryMetaDataStruct *)
			j9mem_allocate_memory(numStructsPassedInMemory * sizeof(structParmInMemoryMetaDataStruct), OMRMEM_CATEGORY_VM);

		if (NULL == structParmInMemory) {
			return NULL;
		}

printf("XXXXX allocate structParmInMemory %p for numStructsPassedInMemory=%d\n", structParmInMemory, numStructsPassedInMemory);
	}

	U_8 *thunkCursor = thunkMem;

	if (breakOnEntry) {
		INT3(thunkCursor)
	}

	if (hiddenParameter) {
		PUSH_SREG(thunkCursor, rbx)
	}

	if (frameSize > 0) {
		if (frameSize >= -128 && frameSize <= 127) {
			SUB_RSP_IMM8(thunkCursor, frameSize)
		} else {
			SUB_RSP_IMM32(thunkCursor, frameSize)
		}
	}

	if (hiddenParameter) {
		// Copy the hidden parameter to a register preserved across the call
		MOV_TREG_SREG(thunkCursor, rbx, gprParmRegs[0])
		gprRegParmCount++;
	}

	for (I_32 i = 0; i < lastSigIdx; i++) {
		switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
			{
				if (gprRegParmCount < MAX_GPRS_PASSED_IN_REGS) {
					// Parm must be spilled from parm register to argList
					S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
					gprRegParmCount++;
				} else {
					// Parm must be filled from frame and spilled to argList.
					// Use rbx as the intermediary register since it is volatile
					L8_TREG_mRSP_DISP32m(thunkCursor, rbx, frameSize + preservedRegisterAreaSize + 8 + memParmCursor)
					S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, rbx)
					memParmCursor += STACK_SLOT_SIZE;
				}

				frameOffsetCursor += STACK_SLOT_SIZE;
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			{
				bool isFloat = (sigArray[i].type == J9_FFI_UPCALL_SIG_TYPE_FLOAT);
				if (fprRegParmCount < MAX_FPRS_PASSED_IN_REGS) {
					// Parm must be spilled from parm register to argList
					if (isFloat) {
						MOVSS_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
					} else {
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
					}

					fprRegParmCount++;
				} else {
					// Parm must be filled from frame and spilled to argList.
					// Use xmm0 as the intermediary register since it is volatile and it
					// must have been processed as the first parameter already.
					if (isFloat) {
						MOVSS_TREG_mRSP_DISP32m(thunkCursor, xmm0, frameSize + preservedRegisterAreaSize + 8 + memParmCursor)
						MOVSS_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, xmm0)
					} else {
						MOVSD_TREG_mRSP_DISP32m(thunkCursor, xmm0, frameSize + preservedRegisterAreaSize + 8 + memParmCursor)
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, xmm0)
					}

					memParmCursor += STACK_SLOT_SIZE;
				}

				frameOffsetCursor += STACK_SLOT_SIZE;
				break;
			}
			default:
			{
				// Handle structs passed in registers.  Structs passed in memory will be handled
				// after all other parameters are processed

				X64StructPassingMechanism mechanism = analyzeStructParm(gprRegParmCount, fprRegParmCount, sigArray[i]);
				switch (mechanism) {
					case PASS_STRUCT_IN_MEMORY:
					{
						/**
						 * Record the source and destination offsets and the number of bytes to copy
						 */
						structParmInMemory[numStructsPassedInMemoryCursor].memParmCursor = memParmCursor;
						structParmInMemory[numStructsPassedInMemoryCursor].frameOffsetCursor = frameOffsetCursor;
						structParmInMemory[numStructsPassedInMemoryCursor].sizeofStruct = sigArray[i].sizeInByte;
printf("XXXXX PASS_STRUCT_IN_MEMORY : numStructsPassedInMemoryCursor=%d, memParmCursor=%d, frameOffsetCursor=%d, sizeofStruct=%d\n", numStructsPassedInMemoryCursor, memParmCursor, frameOffsetCursor, sigArray[i].sizeInByte);

						I_32 roundedStructSize = ROUND_UP_TO_SLOT_MULTIPLE(sigArray[i].sizeInByte);
						memParmCursor += roundedStructSize;
						frameOffsetCursor += roundedStructSize;
						numStructsPassedInMemoryCursor += 1;
						break;
					}
					case PASS_STRUCT_IN_ONE_FPR:
					{
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
						fprRegParmCount += 1;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}
					case PASS_STRUCT_IN_TWO_FPR:
					{
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
						fprRegParmCount += 1;
						frameOffsetCursor += STACK_SLOT_SIZE;
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
						fprRegParmCount += 1;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}
					case PASS_STRUCT_IN_ONE_GPR_ONE_FPR:
					{
						S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
						gprRegParmCount++;
						frameOffsetCursor += STACK_SLOT_SIZE;
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
						fprRegParmCount += 1;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}
					case PASS_STRUCT_IN_ONE_FPR_ONE_GPR:
					{
						MOVSD_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, fprParmRegs[fprRegParmCount])
						fprRegParmCount += 1;
						frameOffsetCursor += STACK_SLOT_SIZE;
						S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
						gprRegParmCount++;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}
					case PASS_STRUCT_IN_ONE_GPR:
					{
						S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
						gprRegParmCount++;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}
					case PASS_STRUCT_IN_TWO_GPR:
					{
						S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
						gprRegParmCount++;
						frameOffsetCursor += STACK_SLOT_SIZE;
						S8_mRSP_DISP32m_SREG(thunkCursor, frameOffsetCursor, gprParmRegs[gprRegParmCount])
						gprRegParmCount++;
						frameOffsetCursor += STACK_SLOT_SIZE;
						break;
					}

					default:
						Assert_VM_unreachable();
				}
			}
		}
	}

	/**
	 * Handle structs passed in memory.  These are handled last to avoid the complication of
	 * preserving registers used for REP MOVSB.
	 */
	if (numStructsPassedInMemory > 0) {
		for (I_32 i = 0; i < numStructsPassedInMemory; i++) {
			LEA_TREG_mRSP_DISP32m(thunkCursor, rsi, frameSize + preservedRegisterAreaSize + 8 + structParmInMemory[i].memParmCursor)
			LEA_TREG_mRSP_DISP32m(thunkCursor, rdi, structParmInMemory[i].frameOffsetCursor)
			MOV_TREG_IMM32(thunkCursor, rcx, structParmInMemory[i].sizeofStruct)
			REP_MOVSB(thunkCursor)
		}

		j9mem_free_memory(structParmInMemory);
	}

	// -------------------------------------------------------------------------------
	// Call or jump to the common upcall dispatcher
	// -------------------------------------------------------------------------------

	// Parm 1 : J9UpcallMetaData *data
	MOV_TREG_IMM64(thunkCursor, rdi, reinterpret_cast<int64_t>(metaData))

	// Parm 2 : void *argsListPointer
	MOV_TREG_SREG(thunkCursor, rsi, rsp)

	CALL_mSREG_DISP8m(thunkCursor, rdi, offsetof(J9UpcallMetaData, upCallCommonDispatcher))

	// -------------------------------------------------------------------------------
	// Process return value for ABI representation
	// -------------------------------------------------------------------------------

	switch (sigArray[lastSigIdx].type) {
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:   /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:   /* Fall through */
                case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:     /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
		{
			X64StructPassingMechanism mechanism = analyzeStructParm(0, 0, sigArray[lastSigIdx]);
			switch (mechanism) {
				case PASS_STRUCT_IN_MEMORY:
					// rax == buffer address from return value
					MOV_TREG_SREG(thunkCursor, rsi, rax)

					// rbx = caller-supplied buffer address (preserved in rbx)
					MOV_TREG_SREG(thunkCursor, rdi, rbx)
					MOV_TREG_IMM32(thunkCursor, rcx, sigArray[lastSigIdx].sizeInByte)
					REP_MOVSB(thunkCursor)

					// rax must contain the address of the caller-supplied buffer
					MOV_TREG_SREG(thunkCursor, rax, rbx)
					break;
				case PASS_STRUCT_IN_ONE_FPR:
					MOVSD_TREG_mSREGm(thunkCursor, xmm0, rax)
					break;
				case PASS_STRUCT_IN_TWO_FPR:
					MOVSD_TREG_mSREGm(thunkCursor, xmm0, rax)
					MOVSD_TREG_mSREG_DISP8m(thunkCursor, xmm1, rax, 8)
					break;
				case PASS_STRUCT_IN_ONE_GPR_ONE_FPR:
					MOVSD_TREG_mSREG_DISP8m(thunkCursor, xmm0, rax, 8)
					L8_TREG_mSREGm(thunkCursor, rax, rax)
					break;
				case PASS_STRUCT_IN_ONE_FPR_ONE_GPR:
					MOVSD_TREG_mSREGm(thunkCursor, xmm0, rax)
					L8_TREG_mSREG_DISP8m(thunkCursor, rax, rax, 8)
					break;
				case PASS_STRUCT_IN_ONE_GPR:
					L8_TREG_mSREGm(thunkCursor, rax, rax)
					break;
				case PASS_STRUCT_IN_TWO_GPR:
					L8_TREG_mSREG_DISP8m(thunkCursor, rdx, rax, 8)
					L8_TREG_mSREGm(thunkCursor, rax, rax)
					break;
				default:
					Assert_VM_unreachable();
			}
		}
		default:
			// For all other return types the VM helper will have placed the
			// value in the correct register for the ABI.
			break;

	}

	// -------------------------------------------------------------------------------
	// Cleanup frame and return
	// -------------------------------------------------------------------------------

	if (frameSize > 0) {
		if (frameSize >= -128 && frameSize <= 127) {
			ADD_RSP_IMM8(thunkCursor, frameSize)
		} else {
			ADD_RSP_IMM32(thunkCursor, frameSize)
		}
	}

	if (hiddenParameter) {
		POP_SREG(thunkCursor, rbx)
	}

	RET(thunkCursor)

printf("XXXXX final thunkCursor=%p, bytesUsed=%d\n", thunkMem, (I_32)(thunkCursor - thunkMem));

	// Check for thunk memory overflow
	Assert_VM_true( (thunkCursor - thunkMem) <= roundedCodeSize );

	// Store the metaData pointer
	*(J9UpcallMetaData **)((char *)thunkMem + roundedCodeSize) = metaData;

	// Finish up before returning
	vmFuncs->doneUpcallThunkGeneration(metaData, (void *)thunkMem);


printf("XXXXX DONE createUpcallThunk : metaData=%p, thunkMem=%p\n", metaData, thunkMem);

	return (void *)thunkMem;

// ORIG BELOW ------------------------------------------------------------------
#if 0
	I_32 stackSlotCount = 0;
	I_32 fprCovered = 0;
	I_32 tempInt = 0;
	I_32 instructionCount = 0;
	bool hiddenParameter = false;
	bool resultDistNeeded = false;
	bool paramAreaNeeded = true;

	Assert_VM_true(lastSigIdx >= 0);

	// To call the dispatcher: load metaData, load targetAddress, set-up argListPtr, mtctr, bctr
	instructionCount = 5;

	// Testing the return type
	tempInt = sigArray[lastSigIdx].sizeInByte;
        switch (sigArray[lastSigIdx].type) {
		case J9_FFI_UPCALL_SIG_TYPE_VOID:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcall0;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_CHAR:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_SHORT: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_INT32:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcall1;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_POINTER:
		case J9_FFI_UPCALL_SIG_TYPE_INT64:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcallJ;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcallF;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcallD;
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
		{
			Assert_VM_true(0 == (tempInt % sizeof(float)));
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcallStruct;
			resultDistNeeded = true;
			if (tempInt <= (I_32)(8 * sizeof(float))) {
				// Distribute result back into FPRs
				instructionCount += tempInt/sizeof(float);
			} else {
				// Copy back to memory area designated by a hidden parameter
				// Temporarily take a convenient short-cut of always 8-byte
				stackSlotCount += 1;
				hiddenParameter = true;
				if (tempInt <= 64) {
					// Straight-forward copy: load hidden pointer, sequence of copy
					instructionCount += 1 + (ROUND_UP_SLOT(tempInt) * 2);
				} else {
					// Loop 32-byte per iteration: load hidden pointer, set-up CTR for loop-count
					// 11-instruction loop body, residue copy
					// Note: didn't optimize for loop-entry alignment
					instructionCount += 3 + 11 + (ROUND_UP_SLOT(tempInt & 31) * 2);
				}
			}
			break;
		}
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
		{
			Assert_VM_true(0 == (tempInt % sizeof(double)));
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcallStruct;
			resultDistNeeded = true;
			if (tempInt <= (I_32)(8 * sizeof(double))) {
				// Distribute back into FPRs
				instructionCount += tempInt/sizeof(double);
			} else {
				// Loop 32-byte per iteration: load hidden pointer, set-up CTR for loop-count
				// 11-instruction loop body, residue copy
				// Note: didn't optimize for loop-entry alignment
				stackSlotCount += 1;
				hiddenParameter = true;
				instructionCount += 3 + 11 + (ROUND_UP_SLOT(tempInt & 31) * 2);
			}
			break;
		}
		// Definitely <= 16-byte
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
		{
			Assert_VM_true(tempInt <= 16);
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcallStruct;
			resultDistNeeded = true;
			instructionCount += ROUND_UP_SLOT(tempInt);
			break;
		}
		// Definitely > 16-byte
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
		{
			Assert_VM_true(tempInt > 16);
			metaData->upCallCommonDispatcher = (void *)vmFuncs->icallVMprJavaUpcallStruct;
			resultDistNeeded = true;
			hiddenParameter = true;
			stackSlotCount += 1;
			if (tempInt <= 64) {
				// Straight-forward copy: load hidden pointer, sequence of copy
				instructionCount += 1 + (ROUND_UP_SLOT(tempInt) * 2);
			} else {
				// Loop 32-byte per iteration: load hidden pointer, set-up CTR for loop-count
				// 11-instruction loop body, residue copy
				// Note: didn't optimize for loop-entry alignment
				instructionCount += 3 + 11 + (ROUND_UP_SLOT(tempInt & 31) * 2);
			}
			break;
		}
		default:
			Assert_VM_unreachable();
	}

	if (hiddenParameter) {
		instructionCount += 1;
	}

	// Loop through the arguments
	for (I_32 i = 0; i < lastSigIdx; i++) {
		// Testing this argument
		tempInt = sigArray[i].sizeInByte;
		switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
			{
				stackSlotCount += 1;
				if (stackSlotCount > 8) {
					paramAreaNeeded = false;
				} else {
					instructionCount += 1;
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			{
				stackSlotCount += 1;
				fprCovered += 1;
				if (fprCovered > 13) {
					if (stackSlotCount > 8) {
						paramAreaNeeded = false;
					} else {
						instructionCount += 1;
					}
				} else {
					instructionCount += 1;
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
			{
				Assert_VM_true(0 == (tempInt % sizeof(float)));
				stackSlotCount += ROUND_UP_SLOT(tempInt);
				if (tempInt <= (I_32)(8 * sizeof(float))) {
					if ((fprCovered + (tempInt / sizeof(float))) > 13) {
						// It is really tricky here. some remaining SPs are passed in
						// GPRs if there are free GPR parameter registers.
						I_32 restSlots = 0;
						if (fprCovered < 13) {
							instructionCount += 13 - fprCovered;
							// Round-down the already passed in FPRs
							restSlots = ROUND_UP_SLOT(tempInt) - ((13 - fprCovered) / 2);
						} else {
							restSlots = ROUND_UP_SLOT(tempInt);
						}

						Assert_VM_true(restSlots > 0);
						if ((stackSlotCount - restSlots) < 8) {
							if (stackSlotCount > 8) {
								paramAreaNeeded = false;
								instructionCount += 8 - (stackSlotCount - restSlots);
							} else {
								instructionCount += restSlots;
							}
						} else {
							paramAreaNeeded = false;
						}
					} else {
						instructionCount += tempInt / sizeof(float);
					}
					fprCovered += tempInt / sizeof(float);
				} else {
					if (stackSlotCount > 8) {
						paramAreaNeeded = false;
						if ((stackSlotCount - ROUND_UP_SLOT(tempInt)) < 8) {
							instructionCount += 8 + ROUND_UP_SLOT(tempInt) - stackSlotCount;
						}
					} else {
						instructionCount += ROUND_UP_SLOT(tempInt);
					}
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
			{
				Assert_VM_true(0 == (tempInt % sizeof(double)));
				stackSlotCount += ROUND_UP_SLOT(tempInt);
				if (tempInt <= (I_32)(8 * sizeof(double))) {
					if ((fprCovered + (tempInt / sizeof(double))) > 13) {
						I_32 restSlots = 0;
						if (fprCovered < 13) {
							instructionCount += 13 - fprCovered;
							restSlots = ROUND_UP_SLOT(tempInt) - (13 - fprCovered);
						} else {
							restSlots = ROUND_UP_SLOT(tempInt);
						}

						Assert_VM_true(restSlots > 0);
						if ((stackSlotCount - restSlots) < 8) {
							if (stackSlotCount > 8) {
								paramAreaNeeded = false;
								instructionCount += 8 - (stackSlotCount - restSlots);
							} else {
								instructionCount += restSlots;
							}
						} else {
							paramAreaNeeded = false;
						}
					} else {
						instructionCount += tempInt / sizeof(double);
					}
					fprCovered += tempInt / sizeof(double);
				} else {
					if (stackSlotCount > 8) {
						paramAreaNeeded = false;
						if ((stackSlotCount - ROUND_UP_SLOT(tempInt)) < 8) {
							instructionCount += 8 + ROUND_UP_SLOT(tempInt) - stackSlotCount;
						}
					} else {
						instructionCount += ROUND_UP_SLOT(tempInt);
					}
				}
				break;
			}
			// Definitely <= 16-byte
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
			{
				Assert_VM_true(tempInt <= 16);
				stackSlotCount += ROUND_UP_SLOT(tempInt);
				if (stackSlotCount > 8) {
					paramAreaNeeded = false;
					if ((stackSlotCount - ROUND_UP_SLOT(tempInt)) < 8) {
						instructionCount += 8 + ROUND_UP_SLOT(tempInt) - stackSlotCount;
					}
				} else {
					instructionCount += ROUND_UP_SLOT(tempInt);
				}
				break;
			}
			// Definitely > 16-byte
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
			{
				Assert_VM_true(tempInt > 16);
				stackSlotCount += ROUND_UP_SLOT(tempInt);
				if (stackSlotCount > 8) {
					paramAreaNeeded = false;
					if ((stackSlotCount - ROUND_UP_SLOT(tempInt)) < 8) {
						instructionCount += 8 + ROUND_UP_SLOT(tempInt) - stackSlotCount;
					}
				} else {
					instructionCount += ROUND_UP_SLOT(tempInt);
				}
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_VA_LIST: /* Unused */
				// This must be the last argument
				Assert_VM_true(i == (lastSigIdx - 1));
				paramAreaNeeded = false;
				break;
			default:
				Assert_VM_unreachable();
		}

		// Saturate what we want to know: if there are any in-register args to be pushed back
		if ((stackSlotCount > 8) && (fprCovered > 13)) {
			Assert_VM_true(paramAreaNeeded == false);
			break;
		}
	}

	// 7 instructions to build frame: mflr, save-return-addr, stdu-frame, addi-tear-down-frame, load-return-addr, mtlr, blr
	I_32 frameSize = 0;
	I_32 offsetToParamArea = 0;
	I_32 roundedCodeSize = 0;
	I_32 *thunkMem = NULL;  // always 4-byte instruction: convenient to use int-pointer

	if (resultDistNeeded || paramAreaNeeded) {
		instructionCount += 7;
		if (paramAreaNeeded) {
			frameSize = 64 + ((stackSlotCount + 1) / 2) * 16;
			offsetToParamArea = 48;
		} else {
			frameSize = 48;
			// Using the caller frame paramArea
			offsetToParamArea = 80;
		}
	} else {
		frameSize = 0;
		// Using the caller frame paramArea
		offsetToParamArea = 32;
	}

	// If a frame is needed, less than 22 slots are expected (8 GPR + 13 FPR);
	Assert_VM_true(frameSize <= 240);

	// Hopefully a thunk memory is 8-byte aligned. We also make sure thunkSize is multiple of 8
	// another 8-byte to store metaData pointer itself
	roundedCodeSize = ((instructionCount + 1) / 2) * 8;
	metaData->thunkSize = roundedCodeSize + 8;
	thunkMem = (I_32 *)vmFuncs->allocateUpcallThunkMemory(metaData);
	if (NULL == thunkMem) {
		return NULL;
	}
	metaData->thunkAddress = (void *)thunkMem;

	// Generate the instruction sequence according to the signature, looping over them again
	I_32 gprIdx = 3;
	I_32 fprIdx = 1;
	I_32 slotIdx = 0;
	I_32 instrIdx = 0;
	I_32 C_SP = 1;

	if (resultDistNeeded || paramAreaNeeded) {
		thunkMem[instrIdx++] = MFLR(0);
		thunkMem[instrIdx++] = STD(0, C_SP, 16);
		thunkMem[instrIdx++] = STDU(C_SP, C_SP, -frameSize);
	}

	if (hiddenParameter) {
		thunkMem[instrIdx++] = STD(gprIdx++, C_SP, offsetToParamArea);
		slotIdx += 1;
	}

	// Loop through the arguments again
	for (I_32 i = 0; i < lastSigIdx; i++) {
		// Testing this argument
		tempInt = sigArray[i].sizeInByte;
	        switch (sigArray[i].type) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:
			{
				if (slotIdx < 8) {
					thunkMem[instrIdx++] = STD(gprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
				}
				gprIdx += 1;
				slotIdx += 1;
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:
			{
				if (fprIdx <= 13) {
					thunkMem[instrIdx++] = STFS(fprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
				} else {
					if (slotIdx < 8) {
						thunkMem[instrIdx++] = STD(gprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
					}
				}
				fprIdx += 1;
				gprIdx += 1;
				slotIdx += 1;
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
			{
				if (fprIdx <= 13) {
					thunkMem[instrIdx++] = STFD(fprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
				} else {
					if (slotIdx < 8) {
						thunkMem[instrIdx++] = STD(gprIdx, C_SP, offsetToParamArea + (slotIdx * 8));
					}
				}
				fprIdx += 1;
				gprIdx += 1;
				slotIdx += 1;
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
			{
				if (tempInt <= (I_32)(8 * sizeof(float))) {
					if ((fprIdx + (tempInt / sizeof(float))) > 14) {
						I_32 restSlots = 0;
						I_32 gprStartSlot = 0;

						if (fprIdx <= 13) {
							for (I_32 eIdx = 0; eIdx < (14 - fprIdx); eIdx++) {
								thunkMem[instrIdx++] = STFS(fprIdx + eIdx, C_SP,
									offsetToParamArea + (slotIdx * 8) + (eIdx * sizeof(float)));
							}
							// Round-down the already passed in FPRs
							restSlots = ROUND_UP_SLOT(tempInt) - ((14 - fprIdx) / 2);
						} else {
							restSlots = ROUND_UP_SLOT(tempInt);
						}

						if ((gprStartSlot = (slotIdx + ROUND_UP_SLOT(tempInt) - restSlots)) < 8) {
							if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
								for (I_32 gIdx = 0; gIdx < (8 - gprStartSlot); gIdx++) {
									thunkMem[instrIdx++] = STD(3 + gprStartSlot + gIdx, C_SP,
										offsetToParamArea + ((gprStartSlot + gIdx) * 8));
								}
							} else {
								for (I_32 gIdx = 0; gIdx < restSlots; gIdx++) {
									thunkMem[instrIdx++] = STD(3 + gprStartSlot + gIdx, C_SP,
										offsetToParamArea + ((gprStartSlot + gIdx) * 8));
								}
							}
						}
					} else {
						for (I_32 eIdx = 0; eIdx < (I_32)(tempInt / sizeof(float)); eIdx++) {
							thunkMem[instrIdx++] = STFS(fprIdx + eIdx, C_SP,
								 offsetToParamArea + (slotIdx * 8) + (eIdx * sizeof(float)));
						}
					}

					fprIdx += tempInt / sizeof(float);
				} else {
					if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
						if (slotIdx < 8) {
							for (I_32 gIdx = 0; gIdx < (8 - slotIdx); gIdx++) {
								thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
									 offsetToParamArea + (slotIdx + gIdx) * 8);
							}
						}
					} else {
						for (I_32 gIdx = 0; gIdx < ROUND_UP_SLOT(tempInt); gIdx++) {
							thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP, offsetToParamArea + (slotIdx + gIdx) * 8);
						}
					}
				}
				gprIdx += ROUND_UP_SLOT(tempInt);
				slotIdx += ROUND_UP_SLOT(tempInt);
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
			{
				if (tempInt <= (I_32)(8 * sizeof(double))) {
					if ((fprIdx + (tempInt / sizeof(double))) > 14) {
						I_32 restSlots = 0;
						I_32 gprStartSlot = 0;

						if (fprIdx <= 13) {
							for (I_32 eIdx = 0; eIdx < (14 - fprIdx); eIdx++) {
								thunkMem[instrIdx++] = STFD(fprIdx + eIdx, C_SP,
									offsetToParamArea + (slotIdx + eIdx) * 8);
							}
							restSlots = ROUND_UP_SLOT(tempInt) - (14 - fprIdx);
						} else {
							restSlots = ROUND_UP_SLOT(tempInt);
						}

						if ((gprStartSlot = (slotIdx + ROUND_UP_SLOT(tempInt) - restSlots)) < 8) {
							if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
								for (I_32 gIdx = 0; gIdx < (8 - gprStartSlot); gIdx++) {
									thunkMem[instrIdx++] = STD(3 + gprStartSlot + gIdx, C_SP,
										offsetToParamArea + ((gprStartSlot + gIdx) * 8));
								}
							} else {
								for (I_32 gIdx = 0; gIdx < restSlots; gIdx++) {
									thunkMem[instrIdx++] = STD(3 + gprStartSlot + gIdx, C_SP,
										offsetToParamArea + ((gprStartSlot + gIdx) * 8));
								}
							}
						}
					} else {
						for (I_32 eIdx = 0; eIdx < (I_32)(tempInt / sizeof(double)); eIdx++) {
							thunkMem[instrIdx++] = STFD(fprIdx + eIdx, C_SP,
									offsetToParamArea + (slotIdx + eIdx) * 8);
						}
					}

					fprIdx += tempInt / sizeof(double);
				} else {
					if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
						if (slotIdx < 8) {
							for (I_32 gIdx = 0; gIdx < (8 - slotIdx); gIdx++) {
								thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
									 offsetToParamArea + (slotIdx + gIdx) * 8);
							}
						}
					} else {
						for (I_32 gIdx = 0; gIdx < ROUND_UP_SLOT(tempInt); gIdx++) {
							thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
									offsetToParamArea + (slotIdx + gIdx) * 8);
						}
					}
				}
				gprIdx += ROUND_UP_SLOT(tempInt);
				slotIdx += ROUND_UP_SLOT(tempInt);
				break;
			}
			// Definitely <= 16-byte
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
			{
				if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
					if (slotIdx < 8) {
						for (I_32 gIdx = 0; gIdx < (8 - slotIdx); gIdx++) {
							thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
									offsetToParamArea + (slotIdx + gIdx) * 8);
						}
					}
				} else {
					for (I_32 gIdx = 0; gIdx < ROUND_UP_SLOT(tempInt); gIdx++) {
						thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
								offsetToParamArea + (slotIdx + gIdx) * 8);
					}
				}
				slotIdx += ROUND_UP_SLOT(tempInt);
				gprIdx += ROUND_UP_SLOT(tempInt);
				break;
			}
			// Definitely > 16-byte
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
			{
				if ((slotIdx + ROUND_UP_SLOT(tempInt)) > 8) {
					if (slotIdx < 8) {
						for (I_32 gIdx = 0; gIdx < (8 - slotIdx); gIdx++) {
							thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
									offsetToParamArea + (slotIdx + gIdx) * 8);
						}
					}
				} else {
					for (I_32 gIdx = 0; gIdx < ROUND_UP_SLOT(tempInt); gIdx++) {
						thunkMem[instrIdx++] = STD(gprIdx + gIdx, C_SP,
								offsetToParamArea + (slotIdx + gIdx) * 8);
					}
				}
				slotIdx += ROUND_UP_SLOT(tempInt);
				gprIdx += ROUND_UP_SLOT(tempInt);
				break;
			}
			case J9_FFI_UPCALL_SIG_TYPE_VA_LIST: /* Unused */
				break;
			default:
				Assert_VM_unreachable();
		}

		// No additional arg instructions are expected
		if ((slotIdx > 8) && (fprIdx > 13)) {
			break;
		}
	}

	// Make the jump or call to the common dispatcher.
	// gr12 is currently pointing at thunkMem (by ABI requirement),
	// in which case we can load the metaData by a fixed offset
	thunkMem[instrIdx++] = LD(3, 12, roundedCodeSize);
	thunkMem[instrIdx++] = LD(12, 3, offsetof(J9UpcallMetaData, upCallCommonDispatcher));
	thunkMem[instrIdx++] = ADDI(4, C_SP, offsetToParamArea);
	thunkMem[instrIdx++] = MTCTR(12);

	if (resultDistNeeded || paramAreaNeeded) {
		thunkMem[instrIdx++] = BCTRL();

		// Distribute result if needed, then tear down the frame and return
		if (resultDistNeeded) {
			tempInt = sigArray[lastSigIdx].sizeInByte;
		        switch (sigArray[lastSigIdx].type) {
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
				{
					if (tempInt <= (I_32)(8 * sizeof(float))) {
						for (I_32 fIdx = 0; fIdx < (I_32)(tempInt/sizeof(float)); fIdx++) {
							thunkMem[instrIdx++] = LFS(1+fIdx, 3, fIdx * sizeof(float));
						}
					} else {
						if (tempInt <= 64) {
							copyBackStraight(thunkMem, &instrIdx, tempInt, offsetToParamArea);
						} else {
							// Note: didn't optimize for loop-entry alignment
							copyBackLoop(thunkMem, &instrIdx, tempInt, offsetToParamArea);
						}
					}
					break;
				}
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
				{
					if (tempInt <= (I_32)(8 * sizeof(double))) {
						for (I_32 fIdx = 0; fIdx < (I_32)(tempInt/sizeof(double)); fIdx++) {
							thunkMem[instrIdx++] = LFD(1+fIdx, 3, fIdx * sizeof(double));
						}
					} else {
						// Note: didn't optimize for loop-entry alignment
						copyBackLoop(thunkMem, &instrIdx, tempInt, offsetToParamArea);
					}
					break;
				}
				// Definitely <= 16-byte
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP:    /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP: /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP:    /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP: /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC:  /* Fall through */
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC:
				{
					if (tempInt > 8) {
						thunkMem[instrIdx++] = LD(4, 3, 8);
					}
					thunkMem[instrIdx++] = LD(3, 3, 0);
					break;
				}
				// Definitely > 16-byte
				case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
				{
					if (tempInt <= 64) {
						copyBackStraight(thunkMem, &instrIdx, tempInt, offsetToParamArea);
					} else {
						// Note: didn't optimize for loop-entry alignment
						copyBackLoop(thunkMem, &instrIdx, tempInt, offsetToParamArea);
					}
					break;
				}
				default:
					Assert_VM_unreachable();
			}
		}

		thunkMem[instrIdx++] = ADDI(C_SP, C_SP, frameSize);
		thunkMem[instrIdx++] = LD(0, C_SP, 16);
		thunkMem[instrIdx++] = MTLR(0);
		thunkMem[instrIdx++] = BLR();
	} else {
		thunkMem[instrIdx++] = BCTR();
	}

	Assert_VM_true(instrIdx == instructionCount);

	// Store the metaData pointer
	*(J9UpcallMetaData **)((char *)thunkMem + roundedCodeSize) = metaData;

	// Finish up before returning
	vmFuncs->doneUpcallThunkGeneration(metaData, (void *)thunkMem);

	return (void *)thunkMem;
#endif
}

/**
 * @brief Calculate the requested argument in-stack memory address to return
 * @param nativeSig[in] a pointer to the J9UpcallNativeSignature
 * @param argListPtr[in] a pointer to the argument list prepared by the thunk
 * @param argIdx[in] the requested argument index
 * @return address in argument list for the requested argument
 *
 * Details:
 *   A quick walk-through of the argument list ahead of the requested one
 *   Calculating its address based on argListPtr
 */
void *
getArgPointer(J9UpcallNativeSignature *nativeSig, void *argListPtr, I_32 argIdx)
{
	J9UpcallSigType *sigArray = nativeSig->sigArray;
	I_32 lastSigIdx = (I_32)(nativeSig->numSigs - 1); // The index for the return type in the signature array
	I_32 stackSlotCount = 0;
	I_32 tempInt = 0;

printf("YYYYY getArgPointer : nativeSig=%p, argListPtr=%p, argIdx=%d\n", nativeSig, argListPtr, argIdx);

	Assert_VM_true((argIdx >= 0) && (argIdx < lastSigIdx));

	// Testing the return type
	tempInt = sigArray[lastSigIdx].sizeInByte;
        switch (sigArray[lastSigIdx].type) {
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_SP:
			if (tempInt > (I_32)(8 * sizeof(float))) {
				stackSlotCount += 1;
			}
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_ALL_DP:
			if (tempInt > (I_32)(8 * sizeof(double))) {
				stackSlotCount += 1;
			}
			break;
		case J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_OTHER:
			stackSlotCount += 1;
			break;
		default:
			break;
	}

	// Loop through the arguments
	for (I_32 i = 0; i < argIdx; i++) {
		// Testing this argument
		tempInt = sigArray[i].sizeInByte;
		switch (sigArray[i].type & J9_FFI_UPCALL_SIG_TYPE_MASK) {
			case J9_FFI_UPCALL_SIG_TYPE_CHAR:    /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_SHORT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT32:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_POINTER: /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_INT64:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_FLOAT:   /* Fall through */
			case J9_FFI_UPCALL_SIG_TYPE_DOUBLE:
				stackSlotCount += 1;
				break;
			case J9_FFI_UPCALL_SIG_TYPE_STRUCT:
				stackSlotCount += ROUND_UP_SLOT(tempInt);
				break;
			default:
				Assert_VM_unreachable();
		}
	}

void *argPtr = (void *)((char *)argListPtr + (stackSlotCount * STACK_SLOT_SIZE));
printf("YYYYY : argPtr=%p [%08lx]\n", argPtr, *( (uint64_t *)argPtr) );

	return argPtr;
}


#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */

#if 0
        /* @brief Check the merged composition types of both the first 8 bytes and the next 8 bytes
         * of the 16-byte composition type array so as to determine the aggregate subtype of
         * a struct equal to or less than 16 bytes in size).
         *
         * @param first16ByteComposTypes[in] A pointer to a composition type array for the 1st 16bytes of the struct signature string
         * @return an encoded AGGREGATE subtype for the struct signature
         */
        static U_8
        getStructSigTypeFrom16ByteComposTypes(U_8 *first16ByteComposTypes)
        {
                U_8 structSigType = 0;
                U_8 first8ByteComposType = getComposTypeFrom8Bytes(first16ByteComposTypes, 0);
                U_8 second8ByteComposType = getComposTypeFrom8Bytes(first16ByteComposTypes, 8);
                U_8 structSigComposType = (first8ByteComposType << 4) | second8ByteComposType;

                switch (structSigComposType) {
                case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_F_E_D:
                        /* The aggregate subtype is set for the struct {float, padding, double} */
                        structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_DP;
                        break;
                case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_F_F_D:
                        /* The aggregate subtype is set for the struct {float, float, double} */
                        structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_SP_DP;
                        break;
                case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_D_F_E:
                        /* The aggregate subtype is set for the struct {double, float, padding} */
                        structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP;
                        break;
                case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_D_F_F:
                        /* The aggregate subtype is set for the struct {double, float, float} */
                        structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_SP_SP;
                        break;
                case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_M_F_E:
                        /* The aggregate subtype is set for structs starting with the mix of any integer type/float
                         * in the first 8 bytes followed by one float in the second 8 bytes.
                         * e.g. {int, float, float} or {float, int, float}.
                         */
                        structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_SP;
                        break;
                case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_M_F_F: /* Fall through */
                case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_M_D:
                        /* The aggregate subtype is set for a struct starting with the mix of any integer type/float in the
                         * first 8 bytes followed by a double or two floats(treated as a double) in the second 8 bytes.
                         * e.g. {int, float, double}, {float, int, double}, {long, double}, {int, float, float, float}
                         * or {long, float, float}.
                         */
                        structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC_DP;
                        break;
                case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_F_E_M:
                        /* The aggregate subtype is set for a struct starting with a float in the first 8 bytes
                         * followed by the mix of any integer type/float in the second 8 bytes.
                         * e.g. {float, padding, long}.
                         */
                        structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_SP_MISC;
                        break;
                case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_F_F_M: /* Fall through */
                case J9_FFI_UPCALL_STRU_COMPOSITION_TYPE_D_M:
                        /* The aggregate subtype is set for a struct starting with a double or two floats in the
                         * first 8 bytes, followed by the mix of any integer type/float in the second 8 bytes.
                         * e.g. {double, float, int}, {double, long} or  {float, float, float, int}
                         * or {float, float, long}.
                         */
                        structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_DP_MISC;
                        break;
                default:
                        /* The aggregate subtype is set for a struct mixed with any integer type/float
                         * without pure float/double in the first/second 8 bytes.
                         * e.g. {short a[3], char b} or {int, float, int, float}.
                         */
                        structSigType = J9_FFI_UPCALL_SIG_TYPE_STRUCT_AGGREGATE_MISC;
                        break;
                }

                return structSigType;
        }
#endif
