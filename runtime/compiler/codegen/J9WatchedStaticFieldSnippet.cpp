/*******************************************************************************
 * Copyright (c) 2019, 2023 IBM Corp. and others
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

#include "codegen/J9WatchedStaticFieldSnippet.hpp"
#include "codegen/Relocation.hpp"
#include "il/SymbolReference.hpp"
#include "ras/Logger.hpp"

TR::J9WatchedStaticFieldSnippet::J9WatchedStaticFieldSnippet(TR::CodeGenerator *cg, TR::Node *node, J9Method *m, UDATA loc, void *fieldAddress, J9Class *fieldClass)
   : TR::Snippet(cg, node, generateLabelSymbol(cg), false)
   {
   staticFieldData.method = m;
   staticFieldData.location = loc;
   staticFieldData.fieldAddress = fieldAddress;
   staticFieldData.fieldClass = fieldClass;
   }

uint8_t *TR::J9WatchedStaticFieldSnippet::emitSnippetBody()
   {
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);
   TR::Node *node = getNode();

   // We emit the dataSnippet based on the assumption that the J9JITWatchedStaticFieldData structure is laid out as below:
   /*
   typedef struct J9JITWatchedStaticFieldData {
         J9Method *method;               // Currently executing method
         UDATA location;                 // Bytecode PC index
         void *fieldAddress;             // Address of static field storage
         J9Class *fieldClass;            // Declaring class of static field
   } J9JITWatchedStaticFieldData;
   */

   // Emit each field and add a relocation record (for AOT compiles) for any field if needed.
   J9JITWatchedStaticFieldData *str = reinterpret_cast<J9JITWatchedStaticFieldData *>(cursor);
   str->method = staticFieldData.method;
   str->location = staticFieldData.location;
   str->fieldAddress = staticFieldData.fieldAddress;
   str->fieldClass = staticFieldData.fieldClass;

   if (cg()->comp()->getOption(TR_UseSymbolValidationManager))
      {
      cg()->addExternalRelocation(
         new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor + offsetof(J9JITWatchedStaticFieldData, method), reinterpret_cast<uint8_t *>(staticFieldData.method), reinterpret_cast<uint8_t *>(TR::SymbolType::typeMethod), TR_SymbolFromManager, cg()),
         __FILE__,
         __LINE__,
         node);
      }
   else if (cg()->needClassAndMethodPointerRelocations())
      {
      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor + offsetof(J9JITWatchedStaticFieldData, method), NULL, TR_RamMethod, cg()), __FILE__, __LINE__, node);
      }

   bool isResolved = !node->getSymbolReference()->isUnresolved();
   // If the field is unresolved then we populate these snippet fields with the correct value at runtime (via the instructions generated by generateFillInDataBlockSequenceForUnresolvedField)
   // and hence don't need to add relocation records here.
   if (isResolved)
      {
      if (cg()->needRelocationsForStatics())
         {
         cg()->addExternalRelocation(
            new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor + offsetof(J9JITWatchedStaticFieldData, fieldAddress), reinterpret_cast<uint8_t *>(node->getSymbolReference()), reinterpret_cast<uint8_t *>(node->getInlinedSiteIndex()), TR_DataAddress, cg()),
            __FILE__,
            __LINE__,
            node);
         }

      if (cg()->comp()->getOption(TR_UseSymbolValidationManager))
         {
         cg()->addExternalRelocation(
            new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor + offsetof(J9JITWatchedStaticFieldData, fieldClass), reinterpret_cast<uint8_t *>(staticFieldData.fieldClass), reinterpret_cast<uint8_t *>(TR::SymbolType::typeClass), TR_SymbolFromManager, cg()),
            __FILE__,
            __LINE__,
            node);
         }
      // relocations for TR_ClassAddress are needed for AOT/AOTaaS compiles and not needed for regular JIT and JITServer compiles.
      // cg->needClassAndMethodPointerRelocations() tells us whether a relocation is needed depending on the type of compile being performed.
      else if (cg()->needClassAndMethodPointerRelocations())
         {
         // As things currently stand, this will not work on Power because TR_ClassAddress is used to a generate a 5 instruction sequence that materializes the address into a register. Meanwhile we are using TR_ClassAddress here to represent a contiguous word.
         // A short-term solution would be to use TR_ClassPointer. However this is hacky because TR_ClassPointer expects an aconst node (so we would have to create a dummy node). The proper solution would be to implement the functionality in the power
         // codegenerator to be able to patch TR_ClassAddress contiguous word.
         cg()->addExternalRelocation(
            new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor + offsetof(J9JITWatchedStaticFieldData, fieldClass), reinterpret_cast<uint8_t *>(node->getSymbolReference()), reinterpret_cast<uint8_t *>(node->getInlinedSiteIndex()), TR_ClassAddress, cg()),
            __FILE__,
            __LINE__,
            node);
         }
      }

   cursor += sizeof(J9JITWatchedStaticFieldData);

   return cursor;
   }

void TR::J9WatchedStaticFieldSnippet::print(TR::Logger *log, TR_Debug *debug)
   {
   uint8_t *bufferPos = getSnippetLabel()->getCodeLocation();

   debug->printSnippetLabel(log, getSnippetLabel(), bufferPos, "J9WatchedStaticFieldSnippet");

   debug->printPrefix(log, NULL, bufferPos, sizeof(J9Method *));
   log->printf("DC   \t%p \t\t# J9Method", *(reinterpret_cast<J9Method **>(bufferPos)));
   bufferPos += sizeof(J9Method *);

   debug->printPrefix(log, NULL, bufferPos, sizeof(UDATA));
   log->printf("DC   \t%lu \t\t# location", *(reinterpret_cast<UDATA *>(bufferPos)));
   bufferPos += sizeof(UDATA);

   debug->printPrefix(log, NULL, bufferPos, sizeof(void *));
   log->printf("DC   \t%p \t\t# fieldAddress", *(reinterpret_cast<void **>(bufferPos)));
   bufferPos += sizeof(void *);

   debug->printPrefix(log, NULL, bufferPos, sizeof(J9Class *));
   log->printf("DC   \t%p \t\t# fieldClass", *(reinterpret_cast<J9Class **>(bufferPos)));
   bufferPos += sizeof(J9Class *);
   }

