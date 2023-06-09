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

#include "codegen/SnippetDelegate.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/HelperCallSnippet.hpp"
#include "compile/Compilation.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "infra/Assert.hpp"
#include "runtime/Runtime.hpp"

void
J9::X86::SnippetDelegate::createMetaDataForCodeAddress(
      TR::X86HelperCallSnippet *snippet,
      uint8_t *cursor)
   {
   TR::CodeGenerator *cg = snippet->cg();

   cg->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         (uint8_t *)snippet->getDestination(),
         TR_HelperAddress,
         cg),
      __FILE__,
      __LINE__,
      snippet->getCallNode());
   }

void
J9::X86::SnippetDelegate::createMetaDataForLoadaddrArg(
      TR::X86HelperCallSnippet *snippet,
      uint8_t *cursor,
      TR::Node *loadAddrNode)
   {
   TR::CodeGenerator *cg = snippet->cg();

   TR_ASSERT_FATAL(cg->comp()->target().is32Bit(), "not applicable to 64-bit");

   TR::Symbol *sym = loadAddrNode->getSymbol();

   if (cg->comp()->getOption(TR_EnableHCR) && !sym->isClassObject())
      {
      TR::StaticSymbol *staticSym = sym->getStaticSymbol();
      TR_ASSERT_FATAL(staticSym, "expecting a static symbol");

      cg->jitAdd32BitPicToPatchOnClassRedefinition(
         (void *)(uintptr_t)staticSym->getStaticAddress(),
         (void *)cursor);
      }
   }

void
J9::X86::SnippetDelegate::createMetaDataForCodeAddress(
      TR::X86DataSnippet *snippet,
      uint8_t *cursor)
   {
   // Add dummy class unload/redefinition assumption
   //
   if (snippet->isClassAddress())
      {
      TR::CodeGenerator *cg = snippet->cg();
      TR::Compilation *comp = cg->comp();

      bool needRelocation = TR::Compiler->cls.classUnloadAssumptionNeedsRelocation(comp);
      if (needRelocation && !comp->compileRelocatableCode())
         {
         cg->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               NULL,
               TR_ClassUnloadAssumption,
               cg),
            __FILE__,
            __LINE__,
            snippet->getNode());
         }

      if (!needRelocation)
         {
         if (comp->target().is64Bit())
            {
            cg->jitAddPicToPatchOnClassUnload((void*)-1, (void *)cursor);
            }
         else
            {
            cg->jitAdd32BitPicToPatchOnClassUnload((void*)-1, (void *)cursor);
            }
         }

      TR_OpaqueClassBlock *clazz = snippet->getData<TR_OpaqueClassBlock *>();
      if (clazz && comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager))
         {
         cg->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *)clazz,
               (uint8_t *)TR::SymbolType::typeClass,
               TR_SymbolFromManager,
               cg),
            __FILE__,
            __LINE__,
            snippet->getNode());
         }
      }
   }
