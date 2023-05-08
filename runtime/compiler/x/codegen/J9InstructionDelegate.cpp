/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/InstructionDelegate.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/X86Instruction.hpp"
#include "compile/Compilation.hpp"
#include "runtime/Runtime.hpp"

void
J9::X86::InstructionDelegate::createMetaDataForCodeAddress(TR::X86ImmInstruction *instr, uint8_t *cursor)
   {
   TR::CodeGenerator *cg = instr->cg();
   TR::Compilation *comp = cg->comp();
   TR::Node *instrNode = instr->getNode();

   if (instr->getOpCode().hasIntImmediate())
      {
      if (instr->needsAOTRelocation())
         {
         cg->addExternalRelocation(
            TR::ExternalRelocation::create(cursor, 0, TR_BodyInfoAddress, cg),
            __FILE__,
            __LINE__,
            instrNode);
         }

      if (instr->getReloKind() != -1) // TODO: need to change Body info one to use this
         {
         TR::SymbolType symbolKind = TR::SymbolType::typeClass;
         switch (instr->getReloKind())
            {
            case TR_StaticRamMethodConst:
            case TR_VirtualRamMethodConst:
            case TR_SpecialRamMethodConst:
               cg->addExternalRelocation(
                  TR::ExternalRelocation::create(
                     cursor,
                     (uint8_t *)instrNode->getSymbolReference(),
                     (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex(),
                     (TR_ExternalRelocationTargetKind)instr->getReloKind(),
                     cg),
                  __FILE__,
                  __LINE__,
                  instrNode);
               break;
            case TR_MethodPointer:
               if (instrNode && instrNode->getInlinedSiteIndex() == -1 &&
                  (void *)(uintptr_t) instr->getSourceImmediate() == comp->getCurrentMethod()->resolvedMethodAddress())
                  instr->setReloKind(TR_RamMethod);
               // intentional fall-through
            case TR_RamMethod:
               symbolKind = TR::SymbolType::typeMethod;
               // intentional fall-through
            case TR_ClassPointer:
               if (comp->getOption(TR_UseSymbolValidationManager))
                  {
                  cg->addExternalRelocation(
                     TR::ExternalRelocation::create(
                        cursor,
                        (uint8_t *)(uintptr_t)(instr->getSourceImmediate()),
                        (uint8_t *)symbolKind,
                        TR_SymbolFromManager,
                        cg),
                     __FILE__,
                     __LINE__,
                     instrNode);
                  }
               else
                  {
                  cg->addExternalRelocation(
                     TR::ExternalRelocation::create(
                        cursor,
                        (uint8_t*)(instr->getNode()),
                        (TR_ExternalRelocationTargetKind)instr->getReloKind(),
                        cg),
                     __FILE__,
                     __LINE__,
                     instrNode);
                  }
                  break;
            default:
               cg->addExternalRelocation(
                  TR::ExternalRelocation::create(
                     cursor,
                     0,
                     (TR_ExternalRelocationTargetKind)instr->getReloKind(),
                     cg),
                  __FILE__,
                  __LINE__,
                  instrNode);
            }
         }

      if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), instr) != comp->getStaticHCRPICSites()->end())
         {
         cg->jitAdd32BitPicToPatchOnClassRedefinition(((void *)(uintptr_t) instr->getSourceImmediateAsAddress()), (void *) cursor);
         }
      }

   }


void
J9::X86::InstructionDelegate::createMetaDataForCodeAddress(TR::X86ImmSnippetInstruction *instr, uint8_t *cursor)
   {
   TR::CodeGenerator *cg = instr->cg();
   TR::Compilation *comp = cg->comp();

   if (instr->getOpCode().hasIntImmediate())
      {
      if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), instr) != comp->getStaticHCRPICSites()->end())
         {
         cg->jitAdd32BitPicToPatchOnClassRedefinition(((void *)(uintptr_t) instr->getSourceImmediateAsAddress()), (void *) cursor);
         }
      }
   }
