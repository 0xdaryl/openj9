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

#ifndef J9_X86_SNIPPETDELEGATE_INCL
#define J9_X86_SNIPPETDELEGATE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_SNIPPETDELEGATE_CONNECTOR
#define J9_SNIPPETDELEGATE_CONNECTOR
namespace J9 { namespace X86 { class SnippetDelegate; } }
namespace J9 { typedef J9::X86::SnippetDelegate SnippetDelegateConnector; }
#else
#error J9::X86::SnippetDelegate expected to be a primary connector, but a J9 connector is already defined
#endif

#include "compiler/codegen/J9SnippetDelegate.hpp"
#include "infra/Annotations.hpp"

namespace TR { class Node; }
namespace TR { class X86HelperCallSnippet; }

namespace J9
{

namespace X86
{

class OMR_EXTENSIBLE SnippetDelegate : public J9::SnippetDelegate
   {
protected:

   SnippetDelegate() {}

public:

   static void createMetaDataForCodeAddress(
      TR::X86HelperCallSnippet *snippet,
      uint8_t *cursor);

   static void createMetaDataForLoadaddrArg(
      TR::X86HelperCallSnippet *snippet,
      uint8_t *cursor,
      TR::Node *loadAddrNode);

   static void createMetaDataForCodeAddress(
      TR::X86DataSnippet *snippet,
      uint8_t *cursor);

   };

}

}

#endif
