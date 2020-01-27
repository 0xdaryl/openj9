/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#include "x/codegen/CallSnippet.hpp"

#include "codegen/AMD64CallSnippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "codegen/X86PrivateLinkage.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/SymbolReference.hpp"

uint32_t TR::X86ResolveVirtualDispatchReadOnlyDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   // call (5) + dd (4) + dd (4) + dd (4)
   //
   return 5 + 4 + 4 + 4;
   }


uint8_t *TR::X86ResolveVirtualDispatchReadOnlyDataSnippet::emitSnippetBody()
   {
   // doResolve:
   //    call resolveVirtualDispatchReadOnly

   uint8_t *cursor = cg()->getBinaryBufferCursor();

   getSnippetLabel()->setCodeLocation(cursor);

   TR::SymbolReference *resolveVirtualDispatchReadOnlySymRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64resolveVirtualDispatchReadOnly, false, false, false);

   *cursor++ = 0xe8;  // CALL
   *(int32_t *)cursor = cg()->branchDisplacementToHelperOrTrampoline(cursor+4, resolveVirtualDispatchReadOnlySymRef);

   cg()->addExternalRelocation(new (cg()->trHeapMemory())
      TR::ExternalRelocation(cursor,
                                 (uint8_t *)resolveVirtualDispatchReadOnlySymRef,
                                 TR_HelperAddress,
                                 cg()), __FILE__, __LINE__, _callNode);
   cursor += 4;

   gcMap().registerStackMap(cursor, cg());


   //   dd [RIP offset to vtableData]      ; 32-bit (position independent).  Relative to the start of the RA
   //                                      ;    of the call to `resolveVirtualDispatchReadOnly`
   //
   TR_ASSERT_FATAL(IS_32BIT_RIP(_resolveVirtualDataAddress, cursor), "resolve data is out of RIP-relative range");
   *(int32_t*)cursor = (int32_t)(_resolveVirtualDataAddress - (intptr_t)cursor);
   cursor += 4;

   //   dd [RIP offset to vtable index load in mainline]  ; 32-bit (position independent).  Relative to the start of the RA
   //                                                     ;    of the call to `resolveVirtualDispatchReadOnly`
   //
   intptr_t loadResolvedVtableOffsetLabelAddress = reinterpret_cast<intptr_t>(_loadResolvedVtableOffsetLabel->getCodeLocation());
   TR_ASSERT_FATAL(IS_32BIT_RIP(loadResolvedVtableOffsetLabelAddress, cursor-4), "load resolved vtable label is out of RIP-relative range");
   *(int32_t*)cursor = (int32_t)(loadResolvedVtableOffsetLabelAddress - (intptr_t)cursor + 4);
   cursor += 4;

   //   dd [RIP offset to post vtable dispatch instruction]  ; 32-bit (position independent).  Relative to the start of the RA
   //                                                        ;    of the call to `resolveVirtualDispatchReadOnly`
   //
   intptr_t doneLabelAddress = reinterpret_cast<intptr_t>(_doneLabel->getCodeLocation());
   TR_ASSERT_FATAL(IS_32BIT_RIP(doneLabelAddress, cursor-8), "done label is out of RIP-relative range");
   *(int32_t*)cursor = (int32_t)(doneLabelAddress - (intptr_t)cursor + 8);
   cursor += 4;

   return cursor;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::X86ResolveVirtualDispatchReadOnlyDataSnippet *snippet)
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   // call resolveVirtualDispatchReadOnly
   //
   TR::SymbolReference *resolveVirtualDispatchReadOnlySymRef = _cg->symRefTab()->findOrCreateRuntimeHelper(TR_AMD64resolveVirtualDispatchReadOnly, false, false, false);

   printPrefix(pOutFile, NULL, bufferPos, 5);
   trfprintf(pOutFile, "call\t%s \t\t%s " POINTER_PRINTF_FORMAT,
                 getName(resolveVirtualDispatchReadOnlySymRef),
                 commentString(),
                 resolveVirtualDispatchReadOnlySymRef->getMethodAddress());
   bufferPos += 5;

   // dd [RIP offset to vtableData]
   //
   printPrefix(pOutFile, NULL, bufferPos, sizeof(int32_t));
   trfprintf(
      pOutFile,
      "%s\t" POINTER_PRINTF_FORMAT "\t\t%s RIP offset to vtableData",
      ddString(),
      (void*)*(int32_t *)bufferPos,
      commentString());
   bufferPos += sizeof(int32_t);

   // dd [RIP offset to vtable index load in mainline]
   //
   printPrefix(pOutFile, NULL, bufferPos, sizeof(int32_t));
   trfprintf(
      pOutFile,
      "%s\t" POINTER_PRINTF_FORMAT "\t\t%s RIP offset to vtable index load in mainline",
      ddString(),
      (void*)*(int32_t *)bufferPos,
      commentString());
   bufferPos += sizeof(int32_t);

   // dd [RIP offset to post vtable dispatch instruction]
   //
   printPrefix(pOutFile, NULL, bufferPos, sizeof(int32_t));
   trfprintf(
      pOutFile,
      "%s\t" POINTER_PRINTF_FORMAT "\t\t%s RIP offset to post vtable dispatch instruction",
      ddString(),
      (void*)*(int32_t *)bufferPos,
      commentString());

   }
