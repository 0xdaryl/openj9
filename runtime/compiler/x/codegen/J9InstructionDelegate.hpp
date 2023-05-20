/*******************************************************************************
 * Copyright IBM Corp. and others 2019
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef J9_X86_INSTRUCTIONDELEGATE_INCL
#define J9_X86_INSTRUCTIONDELEGATE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_INSTRUCTIONDELEGATE_CONNECTOR
#define J9_INSTRUCTIONDELEGATE_CONNECTOR
namespace J9 { namespace X86 { class InstructionDelegate; } }
namespace J9 { typedef J9::X86::InstructionDelegate InstructionDelegateConnector; }
#else
#error J9::X86::InstructionDelegate expected to be a primary connector, but a J9 connector is already defined
#endif

#include "compiler/codegen/J9InstructionDelegate.hpp"
#include "infra/Annotations.hpp"

#include <stdint.h>

namespace TR { class X86ImmInstruction; }
namespace TR { class X86ImmSnippetInstruction; }
namespace TR { class X86ImmSymInstruction; }
namespace TR { class X86RegImmInstruction; }
namespace TR { class X86RegImmSymInstruction; }
namespace TR { class X86RegRegImmInstruction; }
namespace TR { class X86MemImmInstruction; }
namespace TR { class X86MemImmSymInstruction; }
namespace TR { class X86MemRegImmInstruction; }
namespace TR { class X86RegMemImmInstruction; }
namespace TR { class AMD64RegImm64Instruction; }
namespace TR { class AMD64RegImm64SymInstruction; }
namespace TR { class AMD64Imm64Instruction; }
namespace TR { class AMD64Imm64SymInstruction; }
namespace TR { class X86LabelInstruction; }

namespace J9
{

namespace X86
{

class OMR_EXTENSIBLE InstructionDelegate : public J9::InstructionDelegate
   {
protected:

   InstructionDelegate() {}

public:

   static void createMetaDataForCodeAddress(TR::X86ImmInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::X86ImmSnippetInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::X86ImmSymInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::X86RegImmInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::X86RegImmSymInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::X86RegRegImmInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::X86MemImmInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::X86MemImmSymInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::X86MemRegImmInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::X86RegMemImmInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::AMD64RegImm64Instruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::AMD64RegImm64SymInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::AMD64Imm64Instruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::AMD64Imm64SymInstruction *instr, uint8_t *cursor);
   static void createMetaDataForCodeAddress(TR::X86LabelInstruction *instr, uint8_t *cursor);
   };

}

}

#endif
