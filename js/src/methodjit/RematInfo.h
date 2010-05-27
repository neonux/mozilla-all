/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if !defined jsjaeger_remat_h__ && defined JS_METHODJIT
#define jsjaeger_remat_h__

#include "assembler/assembler/MacroAssembler.h"

/*
 * Describes how to rematerialize a value during compilation.
 */
struct RematInfo {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    /* Physical location. */
    enum PhysLoc {
        /* Backing bits are in memory. */
        PhysLoc_Memory = 0,

        /* Backed by another entry in the stack. */
        PhysLoc_Copy,

        /* Backing bits are known at compile time. */
        PhysLoc_Constant,

        /* Backing bits are in a register. */
        PhysLoc_Register
    };

    void setRegister(RegisterID reg) {
        reg_ = reg;
        location_ = PhysLoc_Register;
        synced_ = false;
    }

    void setMemory() {
        synced_ = true;
        location_ = PhysLoc_Memory;
    }

    void setConstant() { location_ = PhysLoc_Constant; }

    bool isCopy() { return location_ == PhysLoc_Copy; }
    bool isConstant() { return location_ == PhysLoc_Constant; }
    bool inRegister() { return location_ == PhysLoc_Register; }
    bool inMemory() { return location_ == PhysLoc_Memory; }
    RegisterID reg() { return reg_; }

    void unsync() { synced_ = false; }
    bool synced() { return synced_; }
    bool needsSync() { return !inMemory() && !synced(); }

    /* Set if location is PhysLoc_Register. */
    RegisterID reg_;

    /* Remat source. */
    PhysLoc    location_;

    /* Whether or not the value has been synced to memory. */
    bool       synced_;
};

#endif

