/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "codegen/ARM64Instruction.hpp"
#include "codegen/ARM64PrivateLinkage.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/StackCheckFailureSnippet.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "infra/Assert.hpp"


TR::ARM64PrivateLinkage::ARM64PrivateLinkage(TR::CodeGenerator *cg)
   : TR::Linkage(cg)
   {
   int32_t i;

   _properties._properties = 0;

   _properties._registerFlags[TR::RealRegister::NoReg] = 0;
   _properties._registerFlags[TR::RealRegister::x0]    = IntegerArgument|IntegerReturn;
   _properties._registerFlags[TR::RealRegister::x1]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x2]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x3]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x4]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x5]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x6]    = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::x7]    = IntegerArgument;

   for (i = TR::RealRegister::x8; i <= TR::RealRegister::x15; i++)
      _properties._registerFlags[i] = 0; // x8 - x15 volatile

   _properties._registerFlags[TR::RealRegister::x16]   = ARM64_Reserved; // IP0
   _properties._registerFlags[TR::RealRegister::x17]   = ARM64_Reserved; // IP1

   _properties._registerFlags[TR::RealRegister::x18]   = Preserved;

   _properties._registerFlags[TR::RealRegister::x19]   = ARM64_Reserved; // vmThread
   _properties._registerFlags[TR::RealRegister::x20]   = ARM64_Reserved; // Java SP

   for (i = TR::RealRegister::x21; i <= TR::RealRegister::x28; i++)
      _properties._registerFlags[i] = Preserved; // x18 - x28 Preserved

   _properties._registerFlags[TR::RealRegister::x29]   = ARM64_Reserved; // FP
   _properties._registerFlags[TR::RealRegister::x30]   = ARM64_Reserved; // LR
   _properties._registerFlags[TR::RealRegister::sp]    = ARM64_Reserved;
   _properties._registerFlags[TR::RealRegister::xzr]   = ARM64_Reserved;

   _properties._registerFlags[TR::RealRegister::v0]    = FloatArgument|FloatReturn;
   _properties._registerFlags[TR::RealRegister::v1]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v2]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v3]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v4]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v5]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v6]    = FloatArgument;
   _properties._registerFlags[TR::RealRegister::v7]    = FloatArgument;

   for (i = TR::RealRegister::v8; i <= TR::RealRegister::LastFPR; i++)
      _properties._registerFlags[i] = 0; // v8 - v31 volatile

   _properties._numIntegerArgumentRegisters  = 8;
   _properties._firstIntegerArgumentRegister = 0;
   _properties._numFloatArgumentRegisters    = 8;
   _properties._firstFloatArgumentRegister   = 8;

   _properties._argumentRegisters[0]  = TR::RealRegister::x0;
   _properties._argumentRegisters[1]  = TR::RealRegister::x1;
   _properties._argumentRegisters[2]  = TR::RealRegister::x2;
   _properties._argumentRegisters[3]  = TR::RealRegister::x3;
   _properties._argumentRegisters[4]  = TR::RealRegister::x4;
   _properties._argumentRegisters[5]  = TR::RealRegister::x5;
   _properties._argumentRegisters[6]  = TR::RealRegister::x6;
   _properties._argumentRegisters[7]  = TR::RealRegister::x7;
   _properties._argumentRegisters[8]  = TR::RealRegister::v0;
   _properties._argumentRegisters[9]  = TR::RealRegister::v1;
   _properties._argumentRegisters[10] = TR::RealRegister::v2;
   _properties._argumentRegisters[11] = TR::RealRegister::v3;
   _properties._argumentRegisters[12] = TR::RealRegister::v4;
   _properties._argumentRegisters[13] = TR::RealRegister::v5;
   _properties._argumentRegisters[14] = TR::RealRegister::v6;
   _properties._argumentRegisters[15] = TR::RealRegister::v7;

   _properties._firstIntegerReturnRegister = 0;
   _properties._firstFloatReturnRegister   = 1;

   _properties._returnRegisters[0]  = TR::RealRegister::x0;
   _properties._returnRegisters[1]  = TR::RealRegister::v0;

   _properties._numAllocatableIntegerRegisters = 25;
   _properties._numAllocatableFloatRegisters   = 32;

   _properties._preservedRegisterMapForGC   = 0x1fe40000;
   _properties._methodMetaDataRegister      = TR::RealRegister::x19;
   _properties._stackPointerRegister        = TR::RealRegister::x20;
   _properties._framePointerRegister        = TR::RealRegister::x29;
   _properties._vtableIndexArgumentRegister = TR::RealRegister::x9;
   _properties._j9methodArgumentRegister    = TR::RealRegister::x0;
   _properties._numberOfDependencyGPRegisters = 32;
   _properties._offsetToFirstParm             = 0; // To be determined
   _properties._offsetToFirstLocal            = 0; // To be determined
   }

TR::ARM64LinkageProperties& TR::ARM64PrivateLinkage::getProperties()
   {
   return _properties;
   }

uint32_t TR::ARM64PrivateLinkage::getRightToLeft()
   {
   return getProperties().getRightToLeft();
   }

void TR::ARM64PrivateLinkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   const TR::ARM64LinkageProperties& linkageProperties = getProperties();
   int32_t firstLocalOffset = linkageProperties.getOffsetToFirstLocal();
   uint32_t stackIndex = firstLocalOffset;
   int32_t lowGCOffset = stackIndex;

   TR::GCStackAtlas *atlas = cg()->getStackAtlas();

   // Map all garbage collected references together so can concisely represent
   // stack maps. They must be mapped so that the GC map index in each local
   // symbol is honoured.
   //
   uint32_t numberOfLocalSlotsMapped = atlas->getNumberOfSlotsMapped() - atlas->getNumberOfParmSlotsMapped();

   stackIndex -= numberOfLocalSlotsMapped * TR::Compiler->om.sizeofReferenceAddress();

   if (comp()->useCompressedPointers())
      {
      // If there are any local objects we have to make sure they are aligned properly
      // when compressed pointers are used.  Otherwise, pointer compression may clobber
      // part of the pointer.
      //
      // Each auto's GC index will have already been aligned, so just the starting stack
      // offset needs to be aligned.
      //
      uint32_t unalignedStackIndex = stackIndex;
      stackIndex &= ~(TR::Compiler->om.objectAlignmentInBytes() - 1);
      uint32_t paddingBytes = unalignedStackIndex - stackIndex;
      if (paddingBytes > 0)
         {
         TR_ASSERT((paddingBytes & (TR::Compiler->om.sizeofReferenceAddress() - 1)) == 0, "Padding bytes should be a multiple of the slot/pointer size");
         uint32_t paddingSlots = paddingBytes / TR::Compiler->om.sizeofReferenceAddress();
         atlas->setNumberOfSlotsMapped(atlas->getNumberOfSlotsMapped() + paddingSlots);
         }
      }

   ListIterator<TR::AutomaticSymbol> automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol *localCursor;
   int32_t firstLocalGCIndex = atlas->getNumberOfParmSlotsMapped();

   // Map local references to set the stack position correct according to the GC map index
   //
   for (localCursor = automaticIterator.getFirst(); localCursor; localCursor = automaticIterator.getNext())
      {
      if (localCursor->getGCMapIndex() >= 0)
         {
         localCursor->setOffset(stackIndex + TR::Compiler->om.sizeofReferenceAddress() * (localCursor->getGCMapIndex() - firstLocalGCIndex));
         if (localCursor->getGCMapIndex() == atlas->getIndexOfFirstInternalPointer())
            {
            atlas->setOffsetOfFirstInternalPointer(localCursor->getOffset() - firstLocalOffset);
            }
         }
      }

   method->setObjectTempSlots((lowGCOffset - stackIndex) / TR::Compiler->om.sizeofReferenceAddress());
   lowGCOffset = stackIndex;

   // Now map the rest of the locals
   //
   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      if (localCursor->getGCMapIndex() < 0 &&
          localCursor->getSize() != 8)
         {
         mapSingleAutomatic(localCursor, stackIndex);
         }

      localCursor = automaticIterator.getNext();
      }

   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      if (localCursor->getGCMapIndex() < 0 &&
          localCursor->getSize() == 8)
         {
         stackIndex -= (stackIndex & 0x4)?4:0;
         mapSingleAutomatic(localCursor, stackIndex);
         }

      localCursor = automaticIterator.getNext();
      }

   method->setLocalMappingCursor(stackIndex);

   // Map the parameters
   //
   ListIterator<TR::ParameterSymbol> parameterIterator(&method->getParameterList());
   TR::ParameterSymbol *parmCursor = parameterIterator.getFirst();

   int32_t offsetToFirstParm = linkageProperties.getOffsetToFirstParm();
   uint32_t sizeOfParameterArea = method->getNumParameterSlots() * TR::Compiler->om.sizeofReferenceAddress();

   while (parmCursor != NULL)
      {
      uint32_t parmSize = (parmCursor->getDataType() != TR::Address) ? parmCursor->getSize()*2 : parmCursor->getSize();

      parmCursor->setParameterOffset(sizeOfParameterArea -
                                     parmCursor->getParameterOffset() -
                                     parmSize +
                                     offsetToFirstParm);

      parmCursor = parameterIterator.getNext();
      }

   atlas->setLocalBaseOffset(lowGCOffset - firstLocalOffset);
   atlas->setParmBaseOffset(atlas->getParmBaseOffset() + offsetToFirstParm - firstLocalOffset);
   }

void TR::ARM64PrivateLinkage::mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex)
   {
   int32_t roundup = (comp()->useCompressedPointers() && p->isLocalObject() ? TR::Compiler->om.objectAlignmentInBytes() : TR::Compiler->om.sizeofReferenceAddress()) - 1;
   int32_t roundedSize = (p->getSize() + roundup) & (~roundup);
   if (roundedSize == 0)
      roundedSize = 4;

   p->setOffset(stackIndex -= roundedSize);
   }

static void lockRegister(TR::RealRegister *regToAssign)
   {
   regToAssign->setState(TR::RealRegister::Locked);
   regToAssign->setAssignedRegister(regToAssign);
   }

void TR::ARM64PrivateLinkage::initARM64RealRegisterLinkage()
   {
   TR::Machine *machine = cg()->machine();
   TR::RealRegister *reg;
   int icount;

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x16); // IP0
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x17); // IP1
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x19); // vmThread
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x20); // Java SP
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x29); // FP
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::x30); // LR
   lockRegister(reg);

   reg = machine->getRealRegister(TR::RealRegister::RegNum::sp); // SP
   lockRegister(reg);

   // assign "maximum" weight to registers x0-x15
   for (icount = TR::RealRegister::x0; icount <= TR::RealRegister::x15; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);

   // assign "maximum" weight to registers x18 and x21-x28
   machine->getRealRegister(TR::RealRegister::RegNum::x18)->setWeight(0xf000);
   for (icount = TR::RealRegister::x20; icount <= TR::RealRegister::x28; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);

   // assign "maximum" weight to registers v0-v31
   for (icount = TR::RealRegister::v0; icount <= TR::RealRegister::v31; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000);
   }


int32_t
TR::ARM64PrivateLinkage::calculatePreservedRegisterSaveSize(
      uint32_t &registerSaveDescription,
      uint32_t &numGPRsSaved)
   {
   TR::Machine *machine = cg()->machine();

   TR::RealRegister::RegNum firstPreservedGPR = TR::RealRegister::x21;
   TR::RealRegister::RegNum lastPreservedGPR = TR::RealRegister::x28;

   // Create a bit vector of preserved registers that have been modified
   // in this method.
   //
   for (int32_t i = firstPreservedGPR; i <= lastPreservedGPR; i++)
      {
      if (machine->getRealRegister((TR::RealRegister::RegNum)i)->getHasBeenAssignedInMethod())
         {
         registerSaveDescription |= 1 << (i-1);
         numGPRsSaved++;
         }
      }

   return numGPRsSaved*8;
   }


void TR::ARM64PrivateLinkage::createPrologue(TR::Instruction *cursor)
   {

   // Prologues are emitted post-RA so it is fine to use real registers directly
   // in instructions
   //
   TR::ARM64LinkageProperties& properties = getProperties();
   TR::Machine *machine = cg()->machine();
   TR::RealRegister *vmThread = machine->getRealRegister(properties.getMethodMetaDataRegister());   // x19
   TR::RealRegister *javaSP = machine->getRealRegister(properties.getStackPointerRegister());       // x20

   // Entry breakpoint
   //
   if (comp()->getOption(TR_EntryBreakPoints))
      {
      cursor = generateExceptionInstruction(cg(), TR::InstOpCode::brkarm64, NULL, 0, cursor);
      }

   // --------------------------------------------------------------------------
   // Determine the bitvector of registers to preserve in the prologue
   //
   uint32_t registerSaveDescription = 0;
   uint32_t numGPRsSaved = 0;

   uint32_t preservedRegisterSaveSize = calculatePreservedRegisterSaveSize(registerSaveDescription, numGPRsSaved);

   // Offset between the entry JavaSP of a method and the first mapped local.  This covers
   // the space needed to preserve the RA.  It is a negative (or zero) offset.
   //
   int32_t firstLocalOffset = properties.getOffsetToFirstLocal();

   // The localMappingCursor is a negative-offset mapping of locals (autos and spills) to
   // the stack relative to the entry JavaSP of a method.  It includes the offset to the
   // first mapped local.
   //
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   int32_t localsSize = -(int32_t)(bodySymbol->getLocalMappingCursor());

   // Size of the frame needed to handle the argument storage requirements of any method
   // call in the current method.
   //
   // The offset to the first parm is the offset between the entry JavaSP and the first
   // mapped parameter.  It is a positive (or zero) offset.
   //
   int32_t outgoingArgsSize = cg()->getLargestOutgoingArgSize() + properties.getOffsetToFirstParm();

   int32_t frameSize = preservedRegisterSaveSize + localsSize + outgoingArgsSize;

   // Align the frame to 16 bytes
   //
   int32_t frameSizeWithAlignment = (frameSize + 15) & ~15;

   registerSaveDescription |= (frameSizeWithAlignment - outgoingArgsSize) & 0xffff;

   cg()->setRegisterSaveDescription(registerSaveDescription);
   cg()->setFrameSizeInBytes(frameSizeWithAlignment + firstLocalOffset);

   // --------------------------------------------------------------------------
   // Store return address (RA)
   //
   TR::MemoryReference *returnAddressMR = new (cg()->trHeapMemory()) TR::MemoryReference(javaSP, 0, cg());
   cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::strimmx, NULL, returnAddressMR, machine->getRealRegister(TR::RealRegister::x30), cursor);

   // --------------------------------------------------------------------------
   // Speculatively adjust Java SP with the needed frame size.
   // This includes the preserved RA slot.
   //
   if (constantIsUnsignedImm12(frameSizeWithAlignment))
      {
      cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::subimmx, NULL, javaSP, javaSP, frameSizeWithAlignment, cursor);
      }
   else
      {
      TR::RealRegister *x9Reg = machine->getRealRegister(TR::RealRegister::RegNum::x9);

      if (constantIsUnsignedImm16(frameSizeWithAlignment))
         {
         // x9 will contain the aligned frame size
         //
         cursor = loadConstant32(cg(), NULL, frameSizeWithAlignment, x9Reg, cursor);
         cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::subx, NULL, javaSP, javaSP, x9Reg, cursor);
         }
      else
         {
         TR_ASSERT_FATAL(0, "Large frame size not supported in prologue yet");
         }
      }

   // --------------------------------------------------------------------------
   // Perform javaSP overflow check
   //
   if (!comp()->isDLT())
      {
      //    if (javaSP < vmThread->SOM)
      //       goto stackOverflowSnippetLabel
      //
      // stackOverflowRestartLabel:
      //
      TR::MemoryReference *somMR = new (cg()->trHeapMemory()) TR::MemoryReference(vmThread, cg()->getStackLimitOffset(), cg());
      TR::RealRegister *somReg = machine->getRealRegister(TR::RealRegister::RegNum::x10);
      cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, NULL, somReg, somMR, cursor);

      TR::RealRegister *zeroReg = machine->getRealRegister(TR::RealRegister::xzr);
      cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::subsx, NULL, zeroReg, javaSP, somReg, cursor);

      TR::LabelSymbol *stackOverflowSnippetLabel = generateLabelSymbol(cg());
      cursor = generateConditionalBranchInstruction(cg(), TR::InstOpCode::b_cond, NULL, stackOverflowSnippetLabel, TR::CC_LS, cursor);

      TR::LabelSymbol *stackOverflowRestartLabel = generateLabelSymbol(cg());
      cursor = generateLabelInstruction(cg(), TR::InstOpCode::label, NULL, stackOverflowRestartLabel, cursor);

      cg()->addSnippet(new (cg()->trHeapMemory()) TR::ARM64StackCheckFailureSnippet(cg(), NULL, stackOverflowRestartLabel, stackOverflowSnippetLabel));
      }

   // --------------------------------------------------------------------------
   // Preserve GPRs
   //
   // javaSP has been adjusted, so preservedRegs start at offset outgoingArgSize
   // relative to the javaSP
   //
   // Registers are preserved in order from x21 (high memory) -> x28 (low memory)
   //
   if (numGPRsSaved)
      {
      TR::RealRegister::RegNum firstPreservedGPR = TR::RealRegister::x21;
      TR::RealRegister::RegNum lastPreservedGPR = TR::RealRegister::x28;

      int32_t preservedRegisterOffset = outgoingArgsSize + (numGPRsSaved * 8);

      for (TR::RealRegister::RegNum regIndex = firstPreservedGPR; regIndex <= lastPreservedGPR; regIndex=(TR::RealRegister::RegNum)((uint32_t)regIndex+1))
         {
         TR::RealRegister *preservedRealReg = machine->getRealRegister(regIndex);
         if (preservedRealReg->getHasBeenAssignedInMethod())
            {
            TR::MemoryReference *preservedRegMR = new (cg()->trHeapMemory()) TR::MemoryReference(javaSP, preservedRegisterOffset, cg());
            cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::strimmx, NULL, preservedRegMR, preservedRealReg, cursor);
            preservedRegisterOffset -= 8;
            numGPRsSaved--;
            }
         }

      TR_ASSERT_FATAL(numGPRsSaved == 0, "preserved register mismatch in prologue");
      }

   // --------------------------------------------------------------------------
   // Initialize locals
   //
   TR::GCStackAtlas *atlas = cg()->getStackAtlas();
   if (atlas)
      {
      // The GC stack maps are conservative in that they all say that
      // collectable locals are live. This means that these locals must be
      // cleared out in case a GC happens before they are allocated a valid
      // value.
      // The atlas contains the number of locals that need to be cleared. They
      // are all mapped together starting at GC index 0.
      //
      uint32_t numLocalsToBeInitialized = atlas->getNumberOfSlotsToBeInitialized();
      if (numLocalsToBeInitialized > 0 || atlas->getInternalPointerMap())
         {
         // The LocalBaseOffset and firstLocalOffset are either negative or zero values
         //
         int32_t initializedLocalsOffsetFromAdjustedJavaSP = frameSizeWithAlignment + (atlas->getLocalBaseOffset() + firstLocalOffset);

         TR::RealRegister *zeroReg = machine->getRealRegister(TR::RealRegister::RegNum::x10);
         cursor = loadConstant64(cg(), NULL, 0, zeroReg, cursor);

         for (int32_t i = 0; i < numLocalsToBeInitialized; i++, initializedLocalsOffsetFromAdjustedJavaSP += TR::Compiler->om.sizeofReferenceAddress())
            {
            TR::MemoryReference *localMR = new (cg()->trHeapMemory()) TR::MemoryReference(javaSP, initializedLocalsOffsetFromAdjustedJavaSP, cg());
            cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::strimmx, NULL, localMR, zeroReg, cursor);
            }

         if (atlas->getInternalPointerMap())
            {
            TR_ASSERT_FATAL(0, "internal pointer initialization not available yet");
            }
         }
      }

   // Adjust final offsets on locals and parm symbols now that the frame size is known.
   // These offsets are relative to the javaSP which has been adjusted downward to
   // accommodate the frame of this method.
   //
   ListIterator<TR::AutomaticSymbol> automaticIterator(&bodySymbol->getAutomaticList());
   TR::AutomaticSymbol *localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      localCursor->setOffset(localCursor->getOffset() + frameSizeWithAlignment);
      localCursor = automaticIterator.getNext();
      }

   ListIterator<TR::ParameterSymbol> parameterIterator(&bodySymbol->getParameterList());
   TR::ParameterSymbol *parmCursor = parameterIterator.getFirst();
   while (parmCursor != NULL)
      {
      parmCursor->setParameterOffset(parmCursor->getParameterOffset() + frameSizeWithAlignment);
      parmCursor = parameterIterator.getNext();
      }

// TODO:
// * spill linkage registers based on RA
// * why is ResolvedMethodSymbol->localMappingCursor an uint32?  StackIndices are stored zero or negative.
// * reverse order of registers in RSD to make stack walk scanning more efficient
// * use store pairs for register preservation
// * initialized locals should be done more efficiently for larger numbers of locals in a loop in the prologue

   }

void TR::ARM64PrivateLinkage::createEpilogue(TR::Instruction *cursor)
   {
   TR_UNIMPLEMENTED();
   }

int32_t TR::ARM64PrivateLinkage::buildArgs(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

TR::Register *TR::ARM64PrivateLinkage::buildDirectDispatch(TR::Node *callNode)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *TR::ARM64PrivateLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }
