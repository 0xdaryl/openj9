/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef J9_AMD64_MEMORY_REFERENCE_INCL
#define J9_AMD64_MEMORY_REFERENCE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_MEMORY_REFERENCE_CONNECTOR
#define J9_MEMORY_REFERENCE_CONNECTOR
namespace J9 { namespace X86 { namespace AMD64 { class MemoryReference; } } }
namespace J9 { typedef J9::X86::AMD64::MemoryReference MemoryReferenceConnector; }
#else
#error J9::X86::AMD64::MemoryReference expected to be a primary connector, but a J9 connector is already defined
#endif

#include "x/codegen/J9MemoryReference.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class MemoryReference; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class SymbolReference; }
namespace TR { class X86DataSnippet; }
class TR_ScratchRegisterManager;

namespace J9
{

namespace X86
{

namespace AMD64
{

class OMR_EXTENSIBLE MemoryReference : public J9::X86::MemoryReference
   {

protected:

   MemoryReference(TR::CodeGenerator *cg) :
      J9::X86::MemoryReferenceConnector(cg) {}

   MemoryReference(
      TR::Register *br,
      TR::SymbolReference *sr,
      TR::Register *ir,
      uint8_t s,
      TR::CodeGenerator *cg) :
         J9::X86::MemoryReferenceConnector(br, sr, ir, s, cg) {}

   MemoryReference(
      TR::Register *br,
      TR::Register *ir,
      uint8_t s,
      TR::CodeGenerator *cg) :
         J9::X86::MemoryReferenceConnector(br, ir, s, cg) {}

   MemoryReference(
      TR::Register *br,
      intptr_t disp,
      TR::CodeGenerator *cg) :
         J9::X86::MemoryReferenceConnector(br, disp, cg) {}

   MemoryReference(
      intptr_t disp,
      TR::CodeGenerator *cg) :
         J9::X86::MemoryReferenceConnector(disp, cg) {}

   MemoryReference(
      TR::Register *br,
      TR::Register *ir,
      uint8_t s,
      intptr_t disp,
      TR::CodeGenerator *cg) :
         J9::X86::MemoryReferenceConnector(br, ir, s, disp, cg) {}

   MemoryReference(
      TR::X86DataSnippet *cds,
      TR::CodeGenerator *cg) :
         J9::X86::MemoryReferenceConnector(cds, cg) {}

   MemoryReference(
      TR::LabelSymbol *label,
      TR::CodeGenerator *cg) :
         J9::X86::MemoryReferenceConnector(label, cg) {}

   MemoryReference(
      TR::Node *rootLoadOrStore,
      TR::CodeGenerator *cg,
      bool canRematerializeAddressAdds) :
         J9::X86::MemoryReferenceConnector(rootLoadOrStore, cg, canRematerializeAddressAdds) {}

   MemoryReference(
      TR::SymbolReference *symRef,
      TR::CodeGenerator *cg) :
         J9::X86::MemoryReferenceConnector(symRef, cg) {}

   MemoryReference(
      TR::SymbolReference *symRef,
      intptr_t displacement,
      TR::CodeGenerator *cg) :
         J9::X86::MemoryReferenceConnector(symRef, displacement, cg) {}

   MemoryReference(
      TR::MemoryReference& mr,
      intptr_t n,
      TR::CodeGenerator *cg,
      TR_ScratchRegisterManager *srm = NULL) :
         J9::X86::MemoryReferenceConnector(mr, n, cg) {}

   /**
    * @brief Creates any necessary metadata for a MOVRegImm64 instruction required
    *        for this memory reference.
    *
    * @param[in] imm64CodeAddress : code address of the Imm64 part of the MOV instruction
    * @param]in] containingInstruction : \c TR::Instruction containing this \c TR::MemoryReference
    * @param[in] symRef : \c TR::SymbolReference relevant for the \p containingInstruction
    * @param[in] cg : \c TR::CodeGenerator object
    */
   void createMetaDataForLoadImm64AddressInstruction(
      uint8_t *imm64CodeAddress,
      TR::Instruction *containingInstruction,
      TR::SymbolReference *symRef,
      TR::CodeGenerator *cg);

   };

} // namespace AMD64

} // namespace X86

} // namespace J9

#endif
