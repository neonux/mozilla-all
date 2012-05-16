/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "frontend/ParseNode.h"
#include "frontend/TreeContext.h"

#include "jsatominlines.h"

#include "frontend/TreeContext-inl.h"
#include "vm/ScopeObject-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::frontend;

void
TreeContext::trace(JSTracer *trc)
{
    sc->bindings.trace(trc);
}

bool
frontend::SetStaticLevel(SharedContext *sc, unsigned staticLevel)
{
    /*
     * This is a lot simpler than error-checking every UpvarCookie::set, and
     * practically speaking it leaves more than enough room for upvars.
     */
    if (UpvarCookie::isLevelReserved(staticLevel)) {
        JS_ReportErrorNumber(sc->context, js_GetErrorMessage, NULL,
                             JSMSG_TOO_DEEP, js_function_str);
        return false;
    }
    sc->staticLevel = staticLevel;
    return true;
}

bool
frontend::GenerateBlockId(SharedContext *sc, uint32_t &blockid)
{
    if (sc->blockidGen == JS_BIT(20)) {
        JS_ReportErrorNumber(sc->context, js_GetErrorMessage, NULL,
                             JSMSG_NEED_DIET, "program");
        return false;
    }
    blockid = sc->blockidGen++;
    return true;
}

void
frontend::PushStatement(SharedContext *sc, StmtInfo *stmt, StmtType type, ptrdiff_t top)
{
    stmt->type = type;
    stmt->flags = 0;
    stmt->blockid = sc->blockid();
    SET_STATEMENT_TOP(stmt, top);
    stmt->label = NULL;
    stmt->blockObj = NULL;
    stmt->down = sc->topStmt;
    sc->topStmt = stmt;
    if (STMT_LINKS_SCOPE(stmt)) {
        stmt->downScope = sc->topScopeStmt;
        sc->topScopeStmt = stmt;
    } else {
        stmt->downScope = NULL;
    }
}

void
frontend::PushBlockScope(SharedContext *sc, StmtInfo *stmt, StaticBlockObject &blockObj,
                         ptrdiff_t top)
{
    PushStatement(sc, stmt, STMT_BLOCK, top);
    stmt->flags |= SIF_SCOPE;
    blockObj.setEnclosingBlock(sc->blockChain);
    stmt->downScope = sc->topScopeStmt;
    sc->topScopeStmt = stmt;
    sc->blockChain = &blockObj;
    stmt->blockObj = &blockObj;
}

void
frontend::PopStatementSC(SharedContext *sc)
{
    StmtInfo *stmt = sc->topStmt;
    sc->topStmt = stmt->down;
    if (STMT_LINKS_SCOPE(stmt)) {
        sc->topScopeStmt = stmt->downScope;
        if (stmt->flags & SIF_SCOPE)
            sc->blockChain = stmt->blockObj->enclosingBlock();
    }
}

StmtInfo *
frontend::LexicalLookup(SharedContext *sc, JSAtom *atom, int *slotp, StmtInfo *stmt)
{
    if (!stmt)
        stmt = sc->topScopeStmt;
    for (; stmt; stmt = stmt->downScope) {
        if (stmt->type == STMT_WITH)
            break;

        /* Skip "maybe scope" statements that don't contain let bindings. */
        if (!(stmt->flags & SIF_SCOPE))
            continue;

        StaticBlockObject &blockObj = *stmt->blockObj;
        const Shape *shape = blockObj.nativeLookup(sc->context, AtomToId(atom));
        if (shape) {
            JS_ASSERT(shape->hasShortID());

            if (slotp)
                *slotp = blockObj.stackDepth() + shape->shortid();
            return stmt;
        }
    }

    if (slotp)
        *slotp = -1;
    return stmt;
}

