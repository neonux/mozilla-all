/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Newman <b{enjam,newma}n@mozilla.com> (original author)
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

#ifndef mozilla_jsipc_ContextWrapperParent_h__
#define mozilla_jsipc_ContextWrapperParent_h__

#include "mozilla/jsipc/PContextWrapperParent.h"
#include "mozilla/jsipc/ObjectWrapperParent.h"
#include "mozilla/jsipc/CPOWTypes.h"

#include "mozilla/dom/ContentParent.h"

#include "jsapi.h"
#include "nsAutoJSValHolder.h"

namespace mozilla {
namespace jsipc {

using mozilla::dom::ContentParent;
    
class ContextWrapperParent
    : public PContextWrapperParent
{
public:

    ContextWrapperParent(ContentParent* cpp)
        : mContent(cpp)
        , mGlobal(NULL)
    {}

    JSBool GetGlobalJSObject(JSContext* cx, JSObject** globalp) {
        if (!mGlobal)
            return JS_FALSE;
        mGlobalHolder.Hold(cx);
        mGlobalHolder = *globalp = mGlobal->GetJSObject(cx);
        return JS_TRUE;
    }

    ObjectWrapperParent* GetGlobalObjectWrapper() const {
        return mGlobal;
    }

    bool RequestRunToCompletion() {
        return mContent->RequestRunToCompletion();
    }

private:

    ContentParent* mContent;
    ObjectWrapperParent* mGlobal;
    nsAutoJSValHolder mGlobalHolder;

    PObjectWrapperParent* AllocPObjectWrapper(const bool&) {
        return new ObjectWrapperParent();
    }

    bool RecvPObjectWrapperConstructor(PObjectWrapperParent* actor,
                                       const bool& makeGlobal)
    {
        if (makeGlobal) {
            mGlobalHolder.Release();
            mGlobal = static_cast<ObjectWrapperParent*>(actor);
        }
        return true;
    }

    bool DeallocPObjectWrapper(PObjectWrapperParent* actor)
    {
        if (mGlobal &&
            mGlobal == static_cast<ObjectWrapperParent*>(actor)) {
            mGlobalHolder.Release();
            mGlobal = NULL;
        }
        delete actor;
        return true;
    }

};

}}

#endif
