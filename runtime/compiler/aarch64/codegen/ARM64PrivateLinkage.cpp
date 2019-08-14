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
#include "codegen/CallSnippet.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
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
#include "env/StackMemoryRegion.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
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

   _properties._registerFlags[TR::RealRegister::x19]   = Preserved|ARM64_Reserved; // vmThread
   _properties._registerFlags[TR::RealRegister::x20]   = Preserved|ARM64_Reserved; // Java SP

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


TR::Instruction *
TR::ARM64PrivateLinkage::createPrePrologue(TR::Instruction *cursor)
   {
   return cursor;
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
   const TR::ARM64LinkageProperties& properties = getProperties();
   TR::Machine *machine = cg()->machine();
   TR::Node *lastNode = cursor->getNode();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister *javaSP = machine->getRealRegister(properties.getStackPointerRegister()); // x20

   // restore preserved GPRs
   int32_t preservedRegisterOffset = cg()->getLargestOutgoingArgSize() + properties.getOffsetToFirstParm(); // outgoingArgsSize
   TR::RealRegister::RegNum firstPreservedGPR = TR::RealRegister::x28;
   TR::RealRegister::RegNum lastPreservedGPR = TR::RealRegister::x21;
   for (TR::RealRegister::RegNum r = firstPreservedGPR; r >= lastPreservedGPR; r = (TR::RealRegister::RegNum)((uint32_t)r-1))
      {
      TR::RealRegister *rr = machine->getRealRegister(r);
      if (rr->getHasBeenAssignedInMethod())
         {
         TR::MemoryReference *preservedRegMR = new (trHeapMemory()) TR::MemoryReference(javaSP, preservedRegisterOffset, cg());
         cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, lastNode, rr, preservedRegMR, cursor);
         preservedRegisterOffset += 8;
         }
      }

   // remove space for preserved registers
   uint32_t frameSize = cg()->getFrameSizeInBytes();
   if (constantIsUnsignedImm12(frameSize))
      {
      cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addimmx, lastNode, javaSP, javaSP, frameSize, cursor);
      }
   else
      {
      TR::RealRegister *x9Reg = machine->getRealRegister(TR::RealRegister::RegNum::x9);
      cursor = loadConstant32(cg(), lastNode, frameSize, x9Reg, cursor);
      cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::addx, lastNode, javaSP, javaSP, x9Reg, cursor);
      }

   // restore return address
   TR::RealRegister *lr = machine->getRealRegister(TR::RealRegister::x30); // lr
   if (machine->getLinkRegisterKilled())
      {
      TR::MemoryReference *returnAddressMR = new (cg()->trHeapMemory()) TR::MemoryReference(javaSP, 0, cg());
      cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::ldrimmx, lastNode, lr, returnAddressMR, cursor);
      }

   // return
   generateRegBranchInstruction(cg(), TR::InstOpCode::ret, lastNode, lr, cursor);
   }

int32_t TR::ARM64PrivateLinkage::buildArgs(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies)
   {
   return buildPrivateLinkageArgs(callNode, dependencies, TR_Private);
   }

int32_t TR::ARM64PrivateLinkage::buildPrivateLinkageArgs(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies,
   TR_LinkageConventions linkage)
   {
   TR_ASSERT(linkage == TR_Private || linkage == TR_Helper || linkage == TR_CHelper, "Unexpected linkage convention");
   TR_ASSERT_FATAL(linkage == TR_Private, "Helper and CHelper linkages are not implemented yet");

   const TR::ARM64LinkageProperties& properties = getProperties();
   TR::ARM64MemoryArgument *pushToMemory = NULL;
   TR::Register *argMemReg;
   TR::Register *tempReg;
   int32_t argIndex = 0;
   int32_t numMemArgs = 0;
   int32_t from, to, step;
   int32_t totalSize = 0;

   uint32_t numIntegerArgs = 0;
   uint32_t numFloatArgs = 0;

   TR::Node *child;
   TR::DataType childType;
   TR::DataType resType = callNode->getType();

   uint32_t firstArgumentChild = callNode->getFirstArgumentIndex();

   TR::MethodSymbol *callSymbol = callNode->getSymbol()->castToMethodSymbol();

   bool isHelperCall = linkage == TR_Helper || linkage == TR_CHelper;
   bool rightToLeft = isHelperCall &&
                      //we want the arguments for induceOSR to be passed from left to right as in any other non-helper call
                      !callNode->getSymbolReference()->isOSRInductionHelper();

   if (rightToLeft)
      {
      from = callNode->getNumChildren() - 1;
      to   = firstArgumentChild;
      step = -1;
      }
   else
      {
      from = firstArgumentChild;
      to   = callNode->getNumChildren() - 1;
      step = 1;
      }

   uint32_t numIntArgRegs = properties.getNumIntArgRegs();
   uint32_t numFloatArgRegs = properties.getNumFloatArgRegs();

   TR::RealRegister::RegNum specialArgReg = TR::RealRegister::NoReg;
   switch (callSymbol->getMandatoryRecognizedMethod())
      {
      // Node: special long args are still only passed in one GPR
      case TR::java_lang_invoke_ComputedCalls_dispatchJ9Method:
         specialArgReg = getProperties().getJ9MethodArgumentRegister();
         // Other args go in memory
         numIntArgRegs   = 0;
         numFloatArgRegs = 0;
         break;
      case TR::java_lang_invoke_ComputedCalls_dispatchVirtual:
         specialArgReg = getProperties().getVTableIndexArgumentRegister();
         break;
      case TR::java_lang_invoke_MethodHandle_invokeWithArgumentsHelper:
         numIntArgRegs   = 0;
         numFloatArgRegs = 0;
         break;
      }
   if (specialArgReg != TR::RealRegister::NoReg)
      {
      TR_UNIMPLEMENTED();
      }

   for (int32_t i = from; (rightToLeft && i >= to) || (!rightToLeft && i <= to); i += step)
      {
      child = callNode->getChild(i);
      childType = child->getDataType();

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            if (numIntegerArgs >= numIntArgRegs)
               numMemArgs++;
            numIntegerArgs++;
            totalSize += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Int64:
            if (numIntegerArgs >= numIntArgRegs)
               numMemArgs++;
            numIntegerArgs++;
            totalSize += 2*TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Float:
            if (numFloatArgs >= numFloatArgRegs)
               numMemArgs++;
            numFloatArgs++;
            totalSize += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Double:
            if (numFloatArgs >= numFloatArgRegs)
               numMemArgs++;
            numFloatArgs++;
            totalSize += 2*TR::Compiler->om.sizeofReferenceAddress();
            break;
         default:
            TR_ASSERT(false, "Argument type %s is not supported\n", childType.toString());
         }
      }

   // From here, down, any new stack allocations will expire / die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (numMemArgs > 0)
      {
      pushToMemory = new (trStackMemory()) TR::ARM64MemoryArgument[numMemArgs];

      argMemReg = cg()->allocateRegister();
      }

   numIntegerArgs = 0;
   numFloatArgs = 0;

   // Helper linkage preserves all argument registers except the return register
   // TODO: C helper linkage does not, this code needs to make sure argument registers are killed in post dependencies
   for (int32_t i = from; (rightToLeft && i >= to) || (!rightToLeft && i <= to); i += step)
      {
      TR::MemoryReference *mref = NULL;
      TR::Register *argRegister;
      TR::InstOpCode::Mnemonic op;

      child = callNode->getChild(i);
      childType = child->getDataType();

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address: // have to do something for GC maps here
            if (child->getDataType() == TR::Address)
               {
               argRegister = pushAddressArg(child);
               }
            else
               {
               argRegister = pushIntegerWordArg(child);
               }
            if (numIntegerArgs < numIntArgRegs)
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  if (argRegister->containsCollectedReference())
                     tempReg = cg()->allocateCollectedReferenceRegister();
                  else
                     tempReg = cg()->allocateRegister();
                  generateMovInstruction(cg(), callNode, tempReg, argRegister);
                  argRegister = tempReg;
                  }
               TR::addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
               }
            else // numIntegerArgs >= numIntArgRegs
               {
               if (childType == TR::Address)
                  {
                  op = TR::InstOpCode::strpostx;
                  }
               else
                  {
                  op = TR::InstOpCode::strpostw;
                  }
               mref = getOutgoingArgumentMemRef(argMemReg, argRegister, op, pushToMemory[argIndex++]);
               }
            numIntegerArgs++;
            break;
         case TR::Int64:
            argRegister = pushLongArg(child);
            if (numIntegerArgs < numIntArgRegs)
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  if (argRegister->containsCollectedReference())
                     tempReg = cg()->allocateCollectedReferenceRegister();
                  else
                     tempReg = cg()->allocateRegister();
                  generateMovInstruction(cg(), callNode, tempReg, argRegister);
                  argRegister = tempReg;
                  }
               TR::addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
               }
            else // numIntegerArgs >= numIntArgRegs
               {
               mref = getOutgoingArgumentMemRef(argMemReg, argRegister, TR::InstOpCode::strpostx, pushToMemory[argIndex++]);
               }
            numIntegerArgs++;
            break;
         case TR::Float:
            argRegister = pushFloatArg(child);
            if (numFloatArgs < numFloatArgRegs)
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempReg = cg()->allocateRegister(TR_FPR);
                  generateTrg1Src1Instruction(cg(), TR::InstOpCode::fmovs, callNode, tempReg, argRegister);
                  argRegister = tempReg;
                  }
               if (numFloatArgs == 0 && resType.isFloatingPoint())
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                     resultReg = cg()->allocateSinglePrecisionRegister();
                  else
                     resultReg = cg()->allocateRegister(TR_FPR);
                  dependencies->addPreCondition(argRegister, TR::RealRegister::v0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::v0);
                  }
               else
                  TR::addDependency(dependencies, argRegister, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
               }
            else // numFloatArgs >= numFloatArgRegs
               {
               mref = getOutgoingArgumentMemRef(argMemReg, argRegister, TR::InstOpCode::vstrposts, pushToMemory[argIndex++]);
               }
            numFloatArgs++;
            break;
         case TR::Double:
            argRegister = pushDoubleArg(child);
            if (numFloatArgs < numFloatArgRegs)
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempReg = cg()->allocateRegister(TR_FPR);
                  generateTrg1Src1Instruction(cg(), TR::InstOpCode::fmovd, callNode, tempReg, argRegister);
                  argRegister = tempReg;
                  }
               if (numFloatArgs == 0 && resType.isFloatingPoint())
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                     resultReg = cg()->allocateSinglePrecisionRegister();
                  else
                     resultReg = cg()->allocateRegister(TR_FPR);
                  dependencies->addPreCondition(argRegister, TR::RealRegister::v0);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::v0);
                  }
               else
                  TR::addDependency(dependencies, argRegister, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
               }
            else // numFloatArgs >= numFloatArgRegs
               {
               mref = getOutgoingArgumentMemRef(argMemReg, argRegister, TR::InstOpCode::vstrpostd, pushToMemory[argIndex++]);
               }
            numFloatArgs++;
            break;
         }
      }

   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR; ++i)
      {
      TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum)i;
      if (properties.getPreserved(realReg))
         continue;
      if (realReg == specialArgReg)
         continue; // already added deps above.  No need to add them here.
      if (!dependencies->searchPreConditionRegister(realReg))
         {
         if (realReg == properties.getIntegerArgumentRegister(0) && callNode->getDataType() == TR::Address)
            {
            dependencies->addPreCondition(cg()->allocateRegister(), TR::RealRegister::x0);
            dependencies->addPostCondition(cg()->allocateCollectedReferenceRegister(), TR::RealRegister::x0);
            }
         else
            {
            // Helper linkage preserves all registers that are not argument registers, so we don't need to spill them.
            if (linkage != TR_Helper)
               TR::addDependency(dependencies, NULL, realReg, TR_GPR, cg());
            }
         }
      }

   if (callNode->getType().isFloatingPoint())
      {
      //add return floating-point register dependency
      TR::addDependency(dependencies, NULL, (TR::RealRegister::RegNum)getProperties().getFloatReturnRegister(), TR_FPR, cg());
      }

   if (numMemArgs > 0)
      {
      TR::RealRegister *sp = cg()->machine()->getRealRegister(properties.getStackPointerRegister());
      generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::subimmx, callNode, argMemReg, sp, totalSize);

      for (argIndex = 0; argIndex < numMemArgs; argIndex++)
         {
         TR::Register *aReg = pushToMemory[argIndex].argRegister;
         generateMemSrc1Instruction(cg(), pushToMemory[argIndex].opCode, callNode, pushToMemory[argIndex].argMemory, aReg);
         cg()->stopUsingRegister(aReg);
         }

      cg()->stopUsingRegister(argMemReg);
      }

   return totalSize;
   }

void TR::ARM64PrivateLinkage::buildDirectCall(TR::Node *callNode,
   TR::SymbolReference *callSymRef,
   TR::RegisterDependencyConditions *dependencies,
   const TR::ARM64LinkageProperties &pp,
   uint32_t argSize)
   {
   TR::Instruction *gcPoint;
   TR::MethodSymbol *callSymbol = callSymRef->getSymbol()->castToMethodSymbol();

      TR::LabelSymbol *label = generateLabelSymbol(cg());
      TR::Snippet *snippet;

      if (callSymRef->isUnresolved())
         {
         snippet = new (trHeapMemory()) TR::ARM64UnresolvedCallSnippet(cg(), callNode, label, argSize);
         }
      else
         {
         snippet = new (trHeapMemory()) TR::ARM64CallSnippet(cg(), callNode, label, argSize);
         snippet->gcMap().setGCRegisterMask(pp.getPreservedRegisterMapForGC());
         }

      cg()->addSnippet(snippet);
      gcPoint = generateImmSymInstruction(cg(), TR::InstOpCode::bl, callNode,
         0,
         dependencies,
         new (trHeapMemory()) TR::SymbolReference(comp()->getSymRefTab(), label),
         snippet);

   gcPoint->ARM64NeedsGCMap(cg(), callSymbol->getLinkageConvention() == TR_Helper ? 0xffffffff : pp.getPreservedRegisterMapForGC());
   }

TR::Register *TR::ARM64PrivateLinkage::buildDirectDispatch(TR::Node *callNode)
   {
   TR::SymbolReference *callSymRef = callNode->getSymbolReference();
   const TR::ARM64LinkageProperties &pp = getProperties();
   TR::RegisterDependencyConditions *dependencies =
      new (trHeapMemory()) TR::RegisterDependencyConditions(
         pp.getNumberOfDependencyGPRegisters(),
         pp.getNumberOfDependencyGPRegisters(), trMemory());

   int32_t argSize = buildArgs(callNode, dependencies);

   buildDirectCall(callNode, callSymRef, dependencies, pp, argSize);
   cg()->machine()->setLinkRegisterKilled(true);

   TR::Register *retReg;
   switch(callNode->getOpCodeValue())
      {
      case TR::icall:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getIntegerReturnRegister());
         break;
      case TR::lcall:
      case TR::acall:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getLongReturnRegister());
         break;
      case TR::fcall:
      case TR::dcall:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getFloatReturnRegister());
         break;
      case TR::call:
         retReg = NULL;
         break;
      default:
         retReg = NULL;
         TR_ASSERT(false, "Unsupported direct call Opcode.");
      }

   callNode->setRegister(retReg);
   return retReg;
   }

void TR::ARM64PrivateLinkage::buildVirtualDispatch(TR::Node *callNode,
   TR::RegisterDependencyConditions *dependencies,
   uint32_t argSize)
   {
   TR_UNIMPLEMENTED();
   }

TR::Register *TR::ARM64PrivateLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   const TR::ARM64LinkageProperties &pp = getProperties();
   TR::RealRegister *sp = cg()->machine()->getRealRegister(pp.getStackPointerRegister());

   TR::RegisterDependencyConditions *dependencies =
      new (trHeapMemory()) TR::RegisterDependencyConditions(
         pp.getNumberOfDependencyGPRegisters(),
         pp.getNumberOfDependencyGPRegisters(), trMemory());

   int32_t argSize = buildArgs(callNode, dependencies);

   buildVirtualDispatch(callNode, dependencies, argSize);
   cg()->machine()->setLinkRegisterKilled(true);

   TR::Register *retReg;
   switch(callNode->getOpCodeValue())
      {
      case TR::icalli:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getIntegerReturnRegister());
         break;
      case TR::lcalli:
      case TR::acalli:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getLongReturnRegister());
         break;
      case TR::fcalli:
      case TR::dcalli:
         retReg = dependencies->searchPostConditionRegister(
                     pp.getFloatReturnRegister());
         break;
      case TR::calli:
         retReg = NULL;
         break;
      default:
         retReg = NULL;
         TR_ASSERT(false, "Unsupported indirect call Opcode.");
      }

   callNode->setRegister(retReg);
   return retReg;
   }
