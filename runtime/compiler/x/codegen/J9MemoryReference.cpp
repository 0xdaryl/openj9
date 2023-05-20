/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/MemoryReference.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/DataSnippet.hpp"
#include "codegen/Relocation.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"

#include <stdint.h>

void
J9::X86::MemoryReference::createMetaDataForCodeAddress(
      uint32_t addressTypes,
      uint8_t *cursor,
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   switch (addressTypes)
      {
      case 6:
      case 2:
         {
         if (self()->needsCodeAbsoluteExternalRelocation())
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  0,
                  TR_AbsoluteMethodAddress,
                  cg),
               __FILE__,
               __LINE__,
               node);
            }
         else if (self()->getReloKind() == TR_ACTIVE_CARD_TABLE_BASE)
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t*)TR_ActiveCardTableBase,
                  TR_GlobalValue,
                  cg),
               __FILE__,
               __LINE__,
               node);
            }

         break;
         }

      case 4:
         {
         TR::Symbol *symbol = self()->getSymbolReference().getSymbol();
         if (symbol)
            {
            TR::StaticSymbol * staticSym = symbol->getStaticSymbol();

            if (staticSym)
               {
               if (self()->getUnresolvedDataSnippet() == NULL)
                  {
                  if (symbol->isConst())
                     {
                     cg->addExternalRelocation(
                        TR::ExternalRelocation::create(
                           cursor,
                           (uint8_t *)self()->getSymbolReference().getOwningMethod(comp)->constantPool(),
                           node ? (uint8_t *)(intptr_t)node->getInlinedSiteIndex() : (uint8_t *)-1,
                           TR_ConstantPool,
                           cg),
                        __FILE__,
                        __LINE__,
                        node);
                     }
                  else if (symbol->isClassObject())
                     {
                     if (cg->needClassAndMethodPointerRelocations())
                        {
                        *(int32_t *)cursor =
                           (int32_t)(TR::Compiler->cls.persistentClassPointerFromClassPointer(
                              comp,
                              (TR_OpaqueClassBlock*)(self()->getSymbolReference().getOffset() + (intptr_t)staticSym->getStaticAddress())));

                        if (comp->getOption(TR_UseSymbolValidationManager))
                           {
                           cg->addExternalRelocation(
                              TR::ExternalRelocation::create(
                                 cursor,
                                 (uint8_t *)(self()->getSymbolReference().getOffset() + (intptr_t)staticSym->getStaticAddress()),
                                 (uint8_t *)TR::SymbolType::typeClass,
                                 TR_SymbolFromManager,
                                 cg),
                              __FILE__,
                              __LINE__,
                              node);
                           }
                        else
                           {
                           cg->addExternalRelocation(
                              TR::ExternalRelocation::create(
                                 cursor,
                                 (uint8_t *)&self()->getSymbolReference(),
                                 node ? (uint8_t *)(intptr_t)node->getInlinedSiteIndex() : (uint8_t *)-1,
                                 TR_ClassAddress,
                                 cg),
                              __FILE__,
                              __LINE__,
                              node);
                           }
                        }
                     }
                  else
                     {
                     if (staticSym->isCountForRecompile())
                        {
                        cg->addExternalRelocation(
                           TR::ExternalRelocation::create(
                              cursor,
                              (uint8_t *) TR_CountForRecompile,
                              TR_GlobalValue,
                              cg),
                           __FILE__,
                           __LINE__,
                           node);
                        }
                     else if (staticSym->isRecompilationCounter())
                        {
                        cg->addExternalRelocation(
                           TR::ExternalRelocation::create(
                              cursor,
                              0,
                              TR_BodyInfoAddress,
                              cg),
                           __FILE__,
                           __LINE__,
                           node);
                        }
                     else if (staticSym->isCatchBlockCounter())
                        {
                        cg->addExternalRelocation(
                           TR::ExternalRelocation::create(
                              cursor,
                              0,
                              TR_CatchBlockCounter,
                              cg),
                           __FILE__,
                           __LINE__,
                           node);
                        }
                     else if (staticSym->isGCRPatchPoint())
                        {
                        cg->addExternalRelocation(
                           TR::ExternalRelocation::create(
                              cursor,
                              0,
                              TR_AbsoluteMethodAddress,
                              cg),
                           __FILE__,
                           __LINE__,
                           node);
                        }
                     else if (symbol->isDebugCounter())
                        {
                        TR::DebugCounterBase *counter = comp->getCounterFromStaticAddress(&(self()->getSymbolReference()));
                        if (counter == NULL)
                           {
                           comp->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter in J9::X86::MemoryReference::createMetaDataForCodeAddress\n");
                           }

                        TR::DebugCounter::generateRelocation(comp, cursor, node, counter);
                        }
                     else
                        {
                        cg->addExternalRelocation(
                           TR::ExternalRelocation::create(
                              cursor,
                              (uint8_t *)&self()->getSymbolReference(),
                              node ? (uint8_t *)(uintptr_t)node->getInlinedSiteIndex() : (uint8_t *)-1,
                              TR_DataAddress,
                              cg),
                           __FILE__,
                           __LINE__,
                           node);
                        }
                     }
                  }
               }
            }
         else
            {
            TR::X86DataSnippet* cds = self()->getDataSnippet();
            TR::LabelSymbol *label = NULL;

            if (cds)
               label = cds->getSnippetLabel();
            else
               label = self()->getLabel();

            if (label != NULL)
               {
               if (comp->target().is64Bit())
                  {
                  // Assume the snippet is in RIP range
                  // TODO:AMD64: Would it be cleaner to have some kind of "isRelative" flag rather than "is64BitTarget"?
                  cg->addRelocation(new (cg->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, label));
                  }
               else
                  {
                  cg->addRelocation(new (cg->trHeapMemory()) TR::LabelAbsoluteRelocation(cursor, label));
                  cg->addExternalRelocation(
                     TR::ExternalRelocation::create(
                        cursor,
                        0,
                        TR_AbsoluteMethodAddress,
                        cg),
                     __FILE__,
                     __LINE__,
                     node);
                  }
               }
            }

         break;
         }

      default:
         break;
      }
   }
