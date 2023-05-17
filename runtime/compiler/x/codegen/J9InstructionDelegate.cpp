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

void
J9::X86::InstructionDelegate::createMetaDataForCodeAddress(TR::X86ImmSymInstruction *instr, uint8_t *cursor)
   {
   TR::CodeGenerator *cg = instr->cg();
   TR::Compilation *comp = cg->comp();
   TR::Node *instrNode = instr->getNode();

   if (instr->getOpCode().hasIntImmediate())
      {
      TR::Symbol *sym = instr->getSymbolReference()->getSymbol();

      if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), instr) != comp->getStaticHCRPICSites()->end())
         {
         cg->jitAdd32BitPicToPatchOnClassRedefinition(((void *)(uintptr_t) instr->getSourceImmediateAsAddress()), (void *) cursor);
         }

      if (instr->getOpCode().isCallImmOp() || instr->getOpCode().isBranchOp())
         {
         if (!comp->isRecursiveMethodTarget(sym))
            {
            TR::LabelSymbol *labelSym = sym->getLabelSymbol();

            if (labelSym)
               {
               cg->addRelocation(new (cg->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, labelSym));
               }
            else
               {
               TR::MethodSymbol *methodSym = sym->getMethodSymbol();
               TR::ResolvedMethodSymbol *resolvedMethodSym = sym->getResolvedMethodSymbol();
               TR_ResolvedMethod *resolvedMethod = resolvedMethodSym ? resolvedMethodSym->getResolvedMethod() : 0;

               if (methodSym && methodSym->isHelper())
                  {
                  cg->addProjectSpecializedRelocation(
                     cursor,
                     (uint8_t *)instr->getSymbolReference(),
                     NULL,
                     TR_HelperAddress,
                     __FILE__,
                     __LINE__,
                     instrNode);
                  }
               else if (methodSym && methodSym->isJNI() && instrNode && instrNode->isPreparedForDirectJNI())
                  {
                  static const int methodJNITypes = 4;
                  static const int reloTypes[methodJNITypes] = {TR_JNIVirtualTargetAddress, 0 /*Interfaces*/, TR_JNIStaticTargetAddress, TR_JNISpecialTargetAddress};
                  int rType = methodSym->getMethodKind()-1;  //method kinds are 1-based
                  TR_ASSERT(reloTypes[rType], "There shouldn't be direct JNI interface calls!");

                  uint8_t *startOfInstruction = instr->getBinaryEncoding();
                  uint8_t *startOfImmediate = cursor;
                  intptr_t diff = reinterpret_cast<intptr_t>(startOfImmediate) - reinterpret_cast<intptr_t>(startOfInstruction);
                  TR_ASSERT_FATAL(diff > 0, "Address of immediate %p less than address of instruction %p\n",
                                  startOfImmediate, startOfInstruction);

                  TR_RelocationRecordInformation *info =
                     reinterpret_cast<TR_RelocationRecordInformation *>(
                        comp->trMemory()->allocateHeapMemory(sizeof(TR_RelocationRecordInformation)));
                  info->data1 = static_cast<uintptr_t>(diff);
                  info->data2 = reinterpret_cast<uintptr_t>(instrNode->getSymbolReference());
                  int16_t inlinedSiteIndex = instrNode ? instrNode->getInlinedSiteIndex() : -1;
                  info->data3 = static_cast<uintptr_t>(inlinedSiteIndex);

                  cg->addExternalRelocation(
                     TR::ExternalRelocation::create(
                        startOfInstruction,
                        reinterpret_cast<uint8_t *>(info),
                        static_cast<TR_ExternalRelocationTargetKind>(reloTypes[rType]),
                        cg),
                     __FILE__,
                     __LINE__,
                     instrNode);
                  }
               else if (resolvedMethod)
                  {
                  cg->addProjectSpecializedRelocation(
                     cursor,
                     (uint8_t *)instr->getSymbolReference()->getMethodAddress(),
                     NULL,
                     TR_MethodCallAddress,
                     __FILE__,
                     __LINE__,
                     instrNode);
                  }
               else
                  {
                  cg->addProjectSpecializedRelocation(
                     cursor,
                     (uint8_t *)instr->getSymbolReference(),
                     NULL,
                     TR_RelativeMethodAddress,
                     __FILE__,
                     __LINE__,
                     instrNode);
                  }
               }
            }
         }
      else if (instr->getOpCodeValue() == TR::InstOpCode::DDImm4)
         {
         cg->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *)(uintptr_t)instr->getSourceImmediate(),
               instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
               TR_ConstantPool,
               cg),
            __FILE__,
            __LINE__,
            instrNode);
         }
      else if (instr->getOpCodeValue() == TR::InstOpCode::PUSHImm4)
         {
         TR::Symbol *symbol = instr->getSymbolReference()->getSymbol();
         if (symbol->isConst())
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *)instr->getSymbolReference()->getOwningMethod(comp)->constantPool(),
                  instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
                  TR_ConstantPool,
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            }
         else if (symbol->isClassObject())
            {
            if (cg->needClassAndMethodPointerRelocations())
               {
               if (comp->getOption(TR_UseSymbolValidationManager))
                  {
                  cg->addExternalRelocation(
                     TR::ExternalRelocation::create(
                        cursor,
                        (uint8_t *)(uintptr_t)instr->getSourceImmediate(),
                        (uint8_t *)TR::SymbolType::typeClass,
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
                        (uint8_t *)instr->getSymbolReference(),
                        instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
                        TR_ClassAddress,
                        cg),
                     __FILE__,
                     __LINE__,
                     instrNode);
                  }
               }
            }
         else if (symbol->isMethod())
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *)instr->getSymbolReference(),
                  instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
                  TR_MethodObject,
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            }
         else
            {
            TR::StaticSymbol *staticSym = sym->getStaticSymbol();
            if (staticSym && staticSym->isCompiledMethod())
               {
               cg->addExternalRelocation(
                  TR::ExternalRelocation::create(
                     cursor,
                     0,
                     TR_RamMethod,
                     cg),
                  __FILE__,
                  __LINE__,
                  instrNode);
               }
            else if (staticSym && staticSym->isStartPC())
               {
               cg->addExternalRelocation(
                  TR::ExternalRelocation::create(
                     cursor,
                     (uint8_t *) (staticSym->getStaticAddress()),
                     TR_AbsoluteMethodAddress,
                     cg),
                  __FILE__,
                  __LINE__,
                  instrNode);
               }
            else if (sym->isDebugCounter())
               {
               TR::DebugCounterBase *counter = comp->getCounterFromStaticAddress(instr->getSymbolReference());
               if (counter == NULL)
                  {
                  comp->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter for a TR::X86ImmSymInstruction\n");
                  }
               TR::DebugCounter::generateRelocation(comp, cursor, instrNode, counter);
               }
            else if (sym->isBlockFrequency())
               {
               TR_RelocationRecordInformation *recordInfo =
                  (TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof(TR_RelocationRecordInformation), heapAlloc);
               recordInfo->data1 = (uintptr_t)instr->getSymbolReference();
               recordInfo->data2 = 0; // seqKind
               cg->addExternalRelocation(
                  TR::ExternalRelocation::create(
                     cursor,
                     (uint8_t *)recordInfo,
                     TR_BlockFrequency,
                     cg),
                  __FILE__,
                  __LINE__,
                  instrNode);
               }
            else if (sym->isRecompQueuedFlag())
               {
               cg->addExternalRelocation(
                  TR::ExternalRelocation::create(
                     cursor,
                     NULL,
                     TR_RecompQueuedFlag,
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
                     (uint8_t *)instr->getSymbolReference(),
                     instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
                     TR_DataAddress,
                     cg),
                  __FILE__,
                  __LINE__,
                  instrNode);
               }
            }
         }
      }
   }

void
J9::X86::InstructionDelegate::createMetaDataForCodeAddress(TR::X86RegImmInstruction *instr, uint8_t *cursor)
   {
   TR::CodeGenerator *cg = instr->cg();
   TR::Compilation *comp = cg->comp();
   TR::Node *instrNode = instr->getNode();

   if (instr->getOpCode().hasIntImmediate())
      {
      // TODO: PIC registration code only works for 32-bit platforms as static PIC address is
      // a 32 bit quantity
      //
      bool staticPIC = false;
      if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), instr) != comp->getStaticPICSites()->end())
         {
         TR_ASSERT(instr->getOpCode().hasIntImmediate(), "StaticPIC: class pointer cannot be smaller than 4 bytes");
         staticPIC = true;
         }

      // HCR register HCR PIC sites in TR::X86RegImmInstruction::generateBinaryEncoding
      bool staticHCRPIC = false;
      if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), instr) != comp->getStaticHCRPICSites()->end())
         {
         TR_ASSERT(instr->getOpCode().hasIntImmediate(), "StaticHCRPIC: class pointer cannot be smaller than 4 bytes");
         staticHCRPIC = true;
         }

      bool staticMethodPIC = false;
      if (std::find(comp->getStaticMethodPICSites()->begin(), comp->getStaticMethodPICSites()->end(), instr) != comp->getStaticMethodPICSites()->end())
         staticMethodPIC = true;

      if (staticPIC)
         {
         cg->jitAdd32BitPicToPatchOnClassUnload(((void *)(uintptr_t) instr->getSourceImmediateAsAddress()), (void *) cursor);
         }

      if (staticHCRPIC)
         {
         cg->addExternalRelocation(
            TR::ExternalRelocation::create(
               (uint8_t *)cursor,
               (uint8_t *)(uintptr_t)instr->getSourceImmediate(),
               TR_HCR,
               cg),
            __FILE__,
            __LINE__,
            instrNode);

         cg->jitAdd32BitPicToPatchOnClassRedefinition(((void *)(uintptr_t) instr->getSourceImmediateAsAddress()), (void *) cursor);
         }

      if (staticMethodPIC)
         {
         void *classPointer = (void *) cg->fe()->createResolvedMethod(cg->trMemory(), (TR_OpaqueMethodBlock *)(uintptr_t) instr->getSourceImmediateAsAddress(), comp->getCurrentMethod())->classOfMethod();
         cg->jitAdd32BitPicToPatchOnClassUnload(classPointer, (void *) cursor);
         }

      TR::SymbolType symbolKind = TR::SymbolType::typeClass;
      switch (instr->getReloKind())
         {
         case TR_HEAP_BASE:
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t*)TR_HeapBase,
                  TR_GlobalValue,
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            break;

         case TR_HEAP_TOP:
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t*)TR_HeapTop,
                  TR_GlobalValue,
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            break;

         case TR_HEAP_BASE_FOR_BARRIER_RANGE:
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t*)TR_HeapBaseForBarrierRange0,
                  TR_GlobalValue,
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            break;

         case TR_HEAP_SIZE_FOR_BARRIER_RANGE:
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t*)TR_HeapSizeForBarrierRange0,
                  TR_GlobalValue,
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            break;

         case TR_ACTIVE_CARD_TABLE_BASE:
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t*)TR_ActiveCardTableBase,
                  TR_GlobalValue,
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
                     (uint8_t *)(uintptr_t)instr->getSourceImmediate(),
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
                     (uint8_t*)instrNode,
                     (TR_ExternalRelocationTargetKind) instr->getReloKind(),
                     cg),
                  __FILE__,
                  __LINE__,
                  instrNode);
               }
            break;

         default:
            break;
         }
      }
   }

void
J9::X86::InstructionDelegate::createMetaDataForCodeAddress(TR::X86RegImmSymInstruction *instr, uint8_t *cursor)
   {
   TR::CodeGenerator *cg = instr->cg();
   TR::Compilation *comp = cg->comp();
   TR::Node *instrNode = instr->getNode();

   if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), instr) != comp->getStaticHCRPICSites()->end())
      {
      cg->jitAdd32BitPicToPatchOnClassRedefinition(((void *)(uintptr_t) instr->getSourceImmediateAsAddress()), (void *) cursor);
      }

   TR::Symbol *symbol = instr->getSymbolReference()->getSymbol();
   TR::SymbolType symbolKind = TR::SymbolType::typeClass;

   switch (instr->getReloKind())
      {
      case TR_ConstantPool:
         TR_ASSERT(symbol->isConst() || symbol->isConstantPoolAddress(), "unknown symbol type for TR_ConstantPool relocation %p\n", instr);
         cg->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *)instr->getSymbolReference()->getOwningMethod(comp)->constantPool(),
               instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
               (TR_ExternalRelocationTargetKind)instr->getReloKind(),
               cg),
            __FILE__,
            __LINE__,
            instrNode);
         break;

      case TR_ClassAddress:
      case TR_ClassObject:
         {
         if (cg->needClassAndMethodPointerRelocations())
            {
            TR_ASSERT(!(instr->getSymbolReference()->isUnresolved() && !symbol->isClassObject()), "expecting a resolved symbol for this instruction class!\n");

            *(int32_t *)cursor = (int32_t)TR::Compiler->cls.persistentClassPointerFromClassPointer(comp, (TR_OpaqueClassBlock*)(uintptr_t)instr->getSourceImmediate());
            if (comp->getOption(TR_UseSymbolValidationManager))
               {
               cg->addExternalRelocation(
                  TR::ExternalRelocation::create(
                     cursor,
                     (uint8_t *)(uintptr_t)instr->getSourceImmediate(),
                     (uint8_t *)TR::SymbolType::typeClass,
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
                     (uint8_t *)instr->getSymbolReference(),
                     instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
                     (TR_ExternalRelocationTargetKind)instr->getReloKind(),
                     cg),
                  __FILE__,
                  __LINE__,
                  instrNode);
               }
            }
         }
         break;

      case TR_MethodObject:
      case TR_DataAddress:
         TR_ASSERT(!(instr->getSymbolReference()->isUnresolved() && !symbol->isClassObject()), "expecting a resolved symbol for this instruction class!\n");

         cg->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *)instr->getSymbolReference(),
               instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
               (TR_ExternalRelocationTargetKind)instr->getReloKind(),
               cg),
            __FILE__,
            __LINE__,
            instrNode);
         break;

      case TR_MethodPointer:
         if (instrNode && instrNode->getInlinedSiteIndex() == -1 &&
             (void *)(uintptr_t) instr->getSourceImmediate() == comp->getCurrentMethod()->resolvedMethodAddress())
            {
            instr->setReloKind(TR_RamMethod);
            }
         symbolKind = TR::SymbolType::typeMethod;
         // intentional fall-through
      case TR_ClassPointer:
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *)(uintptr_t)instr->getSourceImmediate(),
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
                  (uint8_t*)instrNode,
                  (TR_ExternalRelocationTargetKind)instr->getReloKind(),
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            }
         break;

      case TR_DebugCounter:
         {
         TR::DebugCounterBase *counter = comp->getCounterFromStaticAddress(instr->getSymbolReference());
         if (counter == NULL)
            {
            comp->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter for a TR::X86RegImmSymInstruction\n");
            }

         TR::DebugCounter::generateRelocation(comp, cursor, instrNode, counter);
         }
         break;

      case TR_BlockFrequency:
         {
         TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
         recordInfo->data1 = (uintptr_t)instr->getSymbolReference();
         recordInfo->data2 = 0; // seqKind
         cg->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *)recordInfo,
               TR_BlockFrequency,
               cg),
            __FILE__,
            __LINE__,
            instrNode);
         }
         break;

      case TR_RecompQueuedFlag:
         {
         cg->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               NULL,
               TR_RecompQueuedFlag,
               cg),
            __FILE__,
            __LINE__,
            instrNode);
         }
         break;

      default:
         TR_ASSERT(0, "invalid relocation kind for a TR::X86RegImmSymInstruction");
      }
   }

void
J9::X86::InstructionDelegate::createMetaDataForCodeAddress(TR::X86RegRegImmInstruction *instr, uint8_t *cursor)
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

void
J9::X86::InstructionDelegate::createMetaDataForCodeAddress(TR::X86MemImmInstruction *instr, uint8_t *cursor)
   {
   TR::CodeGenerator *cg = instr->cg();
   TR::Compilation *comp = cg->comp();
   TR::Node *instrNode = instr->getNode();

   if (instr->getOpCode().hasIntImmediate())
      {
      // TODO: PIC registration code only works for 32-bit platforms as static PIC address is
      // a 32 bit quantity
      //
      bool staticPIC = false;
      if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), instr) != comp->getStaticPICSites()->end())
         {
         TR_ASSERT(instr->getOpCode().hasIntImmediate(), "StaticPIC: class pointer cannot be smaller than 4 bytes");
         staticPIC = true;
         }

      // HCR register HCR pic sites in TR::X86MemImmInstruction::generateBinaryEncoding
      bool staticHCRPIC = false;
      if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), instr) != comp->getStaticHCRPICSites()->end())
         {
         TR_ASSERT(instr->getOpCode().hasIntImmediate(), "StaticPIC: class pointer cannot be smaller than 4 bytes");
         staticHCRPIC = true;
         }

      bool staticMethodPIC = false;
      if (std::find(comp->getStaticMethodPICSites()->begin(), comp->getStaticMethodPICSites()->end(), instr) != comp->getStaticMethodPICSites()->end())
         staticMethodPIC = true;

      if (staticPIC)
         {
         cg->jitAdd32BitPicToPatchOnClassUnload(((void *)(uintptr_t) instr->getSourceImmediateAsAddress()), (void *) cursor);
         }

      if (staticHCRPIC)
         {
         cg->jitAdd32BitPicToPatchOnClassRedefinition(((void *)(uintptr_t) instr->getSourceImmediateAsAddress()), (void *) cursor);
         }

      if (staticMethodPIC)
         {
         void *classPointer = (void *) cg->fe()->createResolvedMethod(cg->trMemory(), (TR_OpaqueMethodBlock *)(uintptr_t) instr->getSourceImmediateAsAddress(), comp->getCurrentMethod())->classOfMethod();
         cg->jitAdd32BitPicToPatchOnClassUnload(classPointer, (void *) cursor);
         }

      if (instr->getReloKind() == TR_ClassAddress && cg->needClassAndMethodPointerRelocations())
         {
         TR_ASSERT(instrNode, "node expected to be non-NULL here");
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *)(uintptr_t)instr->getSourceImmediate(),
                  (uint8_t *)TR::SymbolType::typeClass,
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
                  (uint8_t *)instrNode->getSymbolReference(),
                  (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex(),
                  TR_ClassAddress,
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            }
         }
      }
   }

void
J9::X86::InstructionDelegate::createMetaDataForCodeAddress(TR::X86MemImmSymInstruction *instr, uint8_t *cursor)
   {
   TR::CodeGenerator *cg = instr->cg();
   TR::Compilation *comp = cg->comp();
   TR::Node *instrNode = instr->getNode();
   TR::SymbolReference *instrSymRef = instr->getSymbolReference();

   if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), instr) != comp->getStaticHCRPICSites()->end())
      {
      cg->jitAdd32BitPicToPatchOnClassRedefinition(((void *)(uintptr_t) instr->getSourceImmediateAsAddress()), (void *) cursor);
      }

   TR::Symbol *symbol = instrSymRef->getSymbol();

   TR_ASSERT(!(instrSymRef->isUnresolved() && !symbol->isClassObject()),
      "expecting a resolved symbol for this instruction class!\n");

   if (symbol->isConst())
      {
      cg->addExternalRelocation(
         TR::ExternalRelocation::create(
            cursor,
            (uint8_t *)instrSymRef->getOwningMethod(comp)->constantPool(),
            instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
            TR_ConstantPool,
            cg),
         __FILE__,
         __LINE__,
         instrNode);
      }
   else if (symbol->isClassObject())
      {
      TR_ASSERT(instrNode, "No node where expected!");
      if (cg->needClassAndMethodPointerRelocations())
         {
         *(int32_t *)cursor = (int32_t)TR::Compiler->cls.persistentClassPointerFromClassPointer(comp, (TR_OpaqueClassBlock*)(uintptr_t)instr->getSourceImmediate());
         if (comp->getOption(TR_UseSymbolValidationManager))
            {
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *)(uintptr_t)instr->getSourceImmediate(),
                  (uint8_t *)TR::SymbolType::typeClass,
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
                  (uint8_t *)instrSymRef,
                  instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
                  TR_ClassAddress,
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            }
         }
      }
   else if (symbol->isMethod())
      {
      cg->addExternalRelocation(
         TR::ExternalRelocation::create(
            cursor,
            (uint8_t *)instrSymRef,
            instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
            TR_MethodObject,
            cg),
         __FILE__,
         __LINE__,
         instrNode);
      }
   else if (symbol->isDebugCounter())
      {
      TR::DebugCounterBase *counter = comp->getCounterFromStaticAddress(instrSymRef);
      if (counter == NULL)
         {
         comp->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter for a TR::X86MemImmSymInstruction\n");
         }

      TR::DebugCounter::generateRelocation(comp, cursor, instrNode, counter);
      }
   else if (symbol->isBlockFrequency())
      {
      TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data1 = (uintptr_t)instrSymRef;
      recordInfo->data2 = 0; // seqKind
      cg->addExternalRelocation(
         TR::ExternalRelocation::create(
            cursor,
            (uint8_t *)recordInfo,
            TR_BlockFrequency,
            cg),
         __FILE__,
         __LINE__,
         instrNode);
      }
   else if (symbol->isRecompQueuedFlag())
      {
      cg->addExternalRelocation(
         TR::ExternalRelocation::create(
            cursor,
            NULL,
            TR_RecompQueuedFlag,
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
            (uint8_t *)instrSymRef,
            instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
            TR_DataAddress,
            cg),
         __FILE__,
         __LINE__,
         instrNode);
      }
   }

void
J9::X86::InstructionDelegate::createMetaDataForCodeAddress(TR::X86MemRegImmInstruction *instr, uint8_t *cursor)
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

void
J9::X86::InstructionDelegate::createMetaDataForCodeAddress(TR::X86RegMemImmInstruction *instr, uint8_t *cursor)
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

void
J9::X86::InstructionDelegate::createMetaDataForCodeAddress(TR::AMD64RegImm64Instruction *instr, uint8_t *cursor)
   {
   TR::CodeGenerator *cg = instr->cg();
   TR::Compilation *comp = cg->comp();
   TR::Node *instrNode = instr->getNode();

   bool staticPIC = false;
   if (std::find(comp->getStaticPICSites()->begin(), comp->getStaticPICSites()->end(), instr) != comp->getStaticPICSites()->end())
      {
      staticPIC = true;
      }

   bool staticHCRPIC = false;
   if (std::find(comp->getStaticHCRPICSites()->begin(), comp->getStaticHCRPICSites()->end(), instr) != comp->getStaticHCRPICSites()->end())
      {
      staticHCRPIC = true;
      }

   bool staticMethodPIC = false;
   if (std::find(comp->getStaticMethodPICSites()->begin(), comp->getStaticMethodPICSites()->end(), instr) != comp->getStaticMethodPICSites()->end())
      {
      staticMethodPIC = true;
      }

   TR::SymbolReference *methodSymRef = instrNode->getOpCode().hasSymbolReference() ? instrNode->getSymbolReference() : NULL;

   if (cg->needRelocationsForHelpers())
      {
      if (instrNode->getOpCode().hasSymbolReference() &&
          methodSymRef &&
          (methodSymRef->getReferenceNumber() == TR_referenceArrayCopy ||
          methodSymRef->getReferenceNumber() == TR_prepareForOSR))
         {
         // The reference number is set in J9X86Evaluator.cpp/VMarrayStoreCheckArrayCopyEvaluator
         cg->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *)methodSymRef,
               TR_AbsoluteHelperAddress,
               cg),
            __FILE__,
            __LINE__,
            instrNode);
         }
      }

   if (comp->fej9()->needRelocationsForStatics())
      {
      switch (instr->getReloKind())
         {
         case TR_HEAP_BASE_FOR_BARRIER_RANGE:
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t*)TR_HeapBaseForBarrierRange0,
                  TR_GlobalValue,
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            break;
         case TR_HEAP_SIZE_FOR_BARRIER_RANGE:
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t*)TR_HeapSizeForBarrierRange0,
                  TR_GlobalValue,
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            break;
         case TR_ACTIVE_CARD_TABLE_BASE:
            cg->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t*)TR_ActiveCardTableBase,
                  TR_GlobalValue,
                  cg),
               __FILE__,
               __LINE__,
               instrNode);
            break;
         }
      }

   if (comp->fej9()->needClassAndMethodPointerRelocations())
      {
      if (((instrNode->getOpCodeValue() == TR::aconst) && instrNode->isMethodPointerConstant() && instr->needsAOTRelocation()) || staticHCRPIC)
         {
         cg->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               NULL,
               TR_RamMethod,
               cg),
            __FILE__,
            __LINE__,
            instrNode);
         }
      else
         {
         TR::SymbolType symbolKind = TR::SymbolType::typeClass;
         switch (instr->getReloKind())
            {
            case TR_ClassAddress:
               {
               TR_ASSERT(instrNode, "node assumed to be non-NULL here");
               if (comp->getOption(TR_UseSymbolValidationManager))
                  {
                  cg->addExternalRelocation(
                     TR::ExternalRelocation::create(
                        cursor,
                        (uint8_t *)instr->getSourceImmediate(),
                        (uint8_t *)TR::SymbolType::typeClass,
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
                        (uint8_t *)methodSymRef,
                        (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex(),
                        TR_ClassAddress,
                        cg),
                     __FILE__,
                     __LINE__,
                     instrNode);
                  }
               }
               break;

            case TR_MethodPointer:
               if (instrNode && instrNode->getInlinedSiteIndex() == -1 &&
                   (void *) instr->getSourceImmediate() == comp->getCurrentMethod()->resolvedMethodAddress())
                  {
                  instr->setReloKind(TR_RamMethod);
                  }
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
                        (uint8_t *)instr->getSourceImmediate(),
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
                        (uint8_t*)instrNode,
                        (TR_ExternalRelocationTargetKind) instr->getReloKind(),
                        cg),
                     __FILE__,
                     __LINE__,
                     instrNode);
                  }
               break;

            case TR_StaticRamMethodConst:
            case TR_VirtualRamMethodConst:
            case TR_SpecialRamMethodConst:
               cg->addExternalRelocation(
                  TR::ExternalRelocation::create(
                     cursor,
                     (uint8_t *) instrNode->getSymbolReference(),
                     instrNode ? (uint8_t *)(intptr_t)instrNode->getInlinedSiteIndex() : (uint8_t *)-1,
                     (TR_ExternalRelocationTargetKind) instr->getReloKind(),
                     cg),
                  __FILE__,
                  __LINE__,
                  instrNode);
               break;

            case TR_JNIStaticTargetAddress:
            case TR_JNISpecialTargetAddress:
            case TR_JNIVirtualTargetAddress:
               {
               uint8_t *startOfInstruction = instr->getBinaryEncoding();
               uint8_t *startOfImmediate = cursor;
               intptr_t diff = reinterpret_cast<intptr_t>(startOfImmediate) - reinterpret_cast<intptr_t>(startOfInstruction);

               TR_ASSERT_FATAL(diff > 0, "Address of immediate %p less than address of instruction %p\n",
                               startOfImmediate, startOfInstruction);

               TR_RelocationRecordInformation *info =
                  reinterpret_cast<TR_RelocationRecordInformation *>(
                     comp->trMemory()->allocateHeapMemory(sizeof(TR_RelocationRecordInformation)));
               info->data1 = static_cast<uintptr_t>(diff);
               info->data2 = reinterpret_cast<uintptr_t>(instrNode->getSymbolReference());
               int16_t inlinedSiteIndex = instrNode ? instrNode->getInlinedSiteIndex() : -1;
               info->data3 = static_cast<uintptr_t>(inlinedSiteIndex);

               cg->addExternalRelocation(
                  TR::ExternalRelocation::create(
                     startOfInstruction,
                     reinterpret_cast<uint8_t *>(info),
                     static_cast<TR_ExternalRelocationTargetKind>(instr->getReloKind()),
                     cg),
                  __FILE__,
                  __LINE__,
                  instrNode);
               }
               break;
            }
         }
      }

   if (staticPIC)
      {
      cg->jitAddPicToPatchOnClassUnload(((void *) instr->getSourceImmediate()), (void *) cursor);
      }

   if (staticHCRPIC)
      {
      cg->addExternalRelocation(
         TR::ExternalRelocation::create(
            (uint8_t *)cursor,
            (uint8_t *)instr->getSourceImmediate(),
            TR_HCR,
            cg),
         __FILE__,
         __LINE__,
         instrNode);

      cg->jitAddPicToPatchOnClassRedefinition(((void *) instr->getSourceImmediate()), (void *) cursor);
      }

   if (staticMethodPIC)
      {
      void *classPointer = (void *) cg->fe()->createResolvedMethod(
         cg->trMemory(),
         (TR_OpaqueMethodBlock *) instr->getSourceImmediate(),
         comp->getCurrentMethod())->classOfMethod();

      cg->jitAddPicToPatchOnClassUnload(classPointer, (void *) cursor);
      }
   }
