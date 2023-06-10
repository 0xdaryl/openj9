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

#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/X86Instruction.hpp"
#include "compile/Compilation.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "env/CompilerEnv.hpp"

void
J9::X86::AMD64::MemoryReference::createMetaDataForLoadImm64AddressInstruction(
      uint8_t *imm64CodeAddress,
      TR::Instruction *containingInstruction,
      TR::SymbolReference *symRef,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   intptr_t displacement = self()->getDisplacement();

   if (_symbolReference.getSymbol())
      {
      TR::SymbolReference &sr = *symRef;
      if (self()->getUnresolvedDataSnippet())
         {
         if (comp->getOption(TR_EnableHCR) &&
             (!sr.getSymbol()->isStatic() || !sr.getSymbol()->isClassObject()))
            {
            cg->jitAddUnresolvedAddressMaterializationToPatchOnClassRedefinition(containingInstruction->getBinaryEncoding());
            }
         }
      else if ((sr.getSymbol()->isClassObject()))
         {
         if (sr.getSymbol()->isStatic())
            {
            if (cg->needClassAndMethodPointerRelocations())
               {
               if (comp->getOption(TR_UseSymbolValidationManager))
                  {
                  cg->addExternalRelocation(
                     TR::ExternalRelocation::create(
                        imm64CodeAddress,
                        (uint8_t *)sr.getSymbol()->castToStaticSymbol()->getStaticAddress(),
                        (uint8_t *)TR::SymbolType::typeClass,
                        TR_SymbolFromManager,
                        cg),
                     __FILE__,
                     __LINE__,
                     containingInstruction->getNode());
                  }
               else
                  {
                  cg->addExternalRelocation(
                     TR::ExternalRelocation::create(
                        imm64CodeAddress,
                        (uint8_t *)symRef,
                        (uint8_t *)(uintptr_t)containingInstruction->getNode()->getInlinedSiteIndex(),
                        TR_ClassAddress,
                        cg),
                     __FILE__,
                     __LINE__,
                     containingInstruction->getNode());
                  }
               }
            }
         }
      else if (sr.getSymbol()->isCountForRecompile())
         {
         if (cg->needRelocationsForPersistentInfoData())
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  imm64CodeAddress,
                  (uint8_t *) TR_CountForRecompile,
                  TR_GlobalValue,
                  cg),
               __FILE__,
               __LINE__,
               containingInstruction->getNode());
            }
         }
      else if (sr.getSymbol()->isRecompilationCounter())
         {
         if (cg->needRelocationsForBodyInfoData())
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  imm64CodeAddress,
                  0,
                  TR_BodyInfoAddress,
                  cg),
               __FILE__,
               __LINE__,
               containingInstruction->getNode());
            }
         }
      else if (sr.getSymbol()->isCatchBlockCounter())
         {
         if (cg->needRelocationsForBodyInfoData())
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  imm64CodeAddress,
                  0,
                  TR_CatchBlockCounter,
                  cg),
               __FILE__,
               __LINE__,
               containingInstruction->getNode());
            }
         }
      else if (sr.getSymbol()->isGCRPatchPoint())
         {
         if (cg->needRelocationsForStatics())
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  imm64CodeAddress,
                  0,
                  TR_AbsoluteMethodAddress,
                  cg),
               __FILE__,
               __LINE__,
               containingInstruction->getNode());
            }
         }
      else if (sr.getSymbol()->isCompiledMethod())
         {
         if (cg->needRelocationsForStatics())
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  imm64CodeAddress,
                  0,
                  TR_RamMethod,
                  cg),
               __FILE__,
               __LINE__,
               containingInstruction->getNode());
            }
         }
      else if (sr.getSymbol()->isStartPC())
         {
         if (cg->needRelocationsForStatics())
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  imm64CodeAddress,
                  0,
                  TR_AbsoluteMethodAddress,
                  cg),
               __FILE__,
               __LINE__,
               containingInstruction->getNode());
            }
         }
      else if (sr.getSymbol()->isDebugCounter())
         {
         if (cg->needRelocationsForStatics())
            {
            TR::DebugCounterBase *counter = comp->getCounterFromStaticAddress(&sr);
            if (counter == NULL)
               {
               comp->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter in J9::X86::AMD64::MemoryReference::createMetaDataForLoadImm64AddressInstruction\n");
               }

            TR::DebugCounter::generateRelocation(
               comp,
               imm64CodeAddress,
               containingInstruction->getNode(),
               counter);
            }
         }
      }
   else
      {
      if (self()->needsCodeAbsoluteExternalRelocation())
         {
         cg->addExternalRelocation(
            TR::ExternalRelocation::create(
               imm64CodeAddress,
               (uint8_t *)0,
               TR_AbsoluteMethodAddress,
               cg),
            __FILE__,
            __LINE__,
            containingInstruction->getNode());
         }
      }
   }
