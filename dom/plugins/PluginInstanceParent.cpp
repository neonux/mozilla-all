/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Chris Jones <jones.chris.g@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "PluginInstanceParent.h"

#include "BrowserStreamParent.h"
#include "PluginModuleParent.h"
#include "PluginStreamParent.h"
#include "StreamNotifyParent.h"

#include "npfunctions.h"
#include "nsAutoPtr.h"

using namespace mozilla::plugins;

PluginInstanceParent::PluginInstanceParent(PluginModuleParent* parent,
                                           NPP npp,
                                           const NPNetscapeFuncs* npniface)
  : mParent(parent),
    mNPP(npp),
    mNPNIface(npniface)
{
}

PluginInstanceParent::~PluginInstanceParent()
{
}

PBrowserStreamParent*
PluginInstanceParent::AllocPBrowserStream(const nsCString& url,
                                          const uint32_t& length,
                                          const uint32_t& lastmodified,
                                          const PStreamNotifyParent* notifyData,
                                          const nsCString& headers,
                                          const nsCString& mimeType,
                                          const bool& seekable,
                                          NPError* rv,
                                          uint16_t *stype)
{
    NS_RUNTIMEABORT("Not reachable");
    return NULL;
}

bool
PluginInstanceParent::AnswerPBrowserStreamDestructor(PBrowserStreamParent* stream,
                                                     const NPError& reason,
                                                     const bool& artificial)
{
    if (!artificial) {
        static_cast<BrowserStreamParent*>(stream)->NPN_DestroyStream(reason);
    }
    return true;
}

bool
PluginInstanceParent::DeallocPBrowserStream(PBrowserStreamParent* stream,
                                            const NPError& reason,
                                            const bool& artificial)
{
    delete stream;
    return true;
}

PPluginStreamParent*
PluginInstanceParent::AllocPPluginStream(const nsCString& mimeType,
                                         const nsCString& target,
                                         NPError* result)
{
    return new PluginStreamParent(this, mimeType, target, result);
}

bool
PluginInstanceParent::DeallocPPluginStream(PPluginStreamParent* stream,
                                           const NPError& reason,
                                           const bool& artificial)
{
    if (!artificial) {
        static_cast<PluginStreamParent*>(stream)->NPN_DestroyStream(reason);
    }
    delete stream;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVjavascriptEnabledBool(
                                                       bool* value,
                                                       NPError* result)
{
    NPBool v;
    *result = mNPNIface->getvalue(mNPP, NPNVjavascriptEnabledBool, &v);
    *value = v;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVisOfflineBool(bool* value,
                                                           NPError* result)
{
    NPBool v;
    *result = mNPNIface->getvalue(mNPP, NPNVisOfflineBool, &v);
    *value = v;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVWindowNPObject(
                                        PPluginScriptableObjectParent** value,
                                        NPError* result)
{
    // TODO NPRuntime
    *value = NULL;
    *result = NPERR_GENERIC_ERROR;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVPluginElementNPObject(
                                        PPluginScriptableObjectParent** value,
                                        NPError* result)
{
    // TODO NPRuntime
    *value = NULL;
    *result = NPERR_GENERIC_ERROR;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetValue_NPNVprivateModeBool(bool* value,
                                                             NPError* result)
{
    NPBool v;
    *result = mNPNIface->getvalue(mNPP, NPNVprivateModeBool, &v);
    *value = v;
    return true;
}

bool
PluginInstanceParent::AnswerNPN_GetURL(const nsCString& url,
                                       const nsCString& target,
                                       NPError* result)
{
    *result = mNPNIface->geturl(mNPP,
                                NullableStringGet(url),
                                NullableStringGet(target));
    return true;
}

bool
PluginInstanceParent::AnswerNPN_PostURL(const nsCString& url,
                                        const nsCString& target,
                                        const nsCString& buffer,
                                        const bool& file,
                                        NPError* result)
{
    *result = mNPNIface->posturl(mNPP, url.get(), NullableStringGet(target),
                                 buffer.Length(), buffer.get(), file);
    return true;
}

PStreamNotifyParent*
PluginInstanceParent::AllocPStreamNotify(const nsCString& url,
                                         const nsCString& target,
                                         const bool& post,
                                         const nsCString& buffer,
                                         const bool& file,
                                         NPError* result)
{
    StreamNotifyParent* notifyData = new StreamNotifyParent();

    if (!post) {
        *result = mNPNIface->geturlnotify(mNPP,
                                          NullableStringGet(url),
                                          NullableStringGet(target),
                                          notifyData);
    }
    else {
        *result = mNPNIface->posturlnotify(mNPP,
                                           NullableStringGet(url),
                                           NullableStringGet(target),
                                           buffer.Length(), buffer.get(),
                                           file, notifyData);
    }
    // TODO: what if this method fails?
    return notifyData;
}

bool
PluginInstanceParent::DeallocPStreamNotify(PStreamNotifyParent* notifyData,
                                           const NPReason& reason)
{
    delete notifyData;
    return true;
}

NPError
PluginInstanceParent::NPP_SetWindow(NPWindow* aWindow)
{
    _MOZ_LOG(__FUNCTION__);
    NS_ENSURE_TRUE(aWindow, NPERR_GENERIC_ERROR);

    NPError prv;
    if (!CallNPP_SetWindow(*aWindow, &prv))
        return NPERR_GENERIC_ERROR;
    return prv;
}

NPError
PluginInstanceParent::NPP_GetValue(NPPVariable aVariable,
                                   void* _retval)
{
    _MOZ_LOG(__FUNCTION__);

    switch (aVariable) {

#ifdef OS_LINUX
        // FIXME/cjones: HACK ALERT! should forward to child
        case NPPVpluginNeedsXEmbed:
            (*(PRBool*)_retval) = PR_TRUE;
            return NPERR_NO_ERROR;
#endif

        case NPPVpluginScriptableNPObject: {
            PPluginScriptableObjectParent* actor;
            NPError rv;
            if (!CallNPP_GetValue_NPPVpluginScriptableNPObject(&actor, &rv)) {
                return NPERR_GENERIC_ERROR;
            }

            if (NPERR_NO_ERROR != rv) {
                return rv;
            }

            const NPNetscapeFuncs* npn = mParent->GetNetscapeFuncs();
            if (!npn) {
                NS_WARNING("No netscape functions?!");
                return NPERR_GENERIC_ERROR;
            }

            NPObject* object =
                reinterpret_cast<PluginScriptableObjectParent*>(actor)->
                    GetObject();
            NS_ASSERTION(object, "This shouldn't ever be null!");

            (*(NPObject**)_retval) = npn->retainobject(object);
            return NPERR_NO_ERROR;
        }

        // TODO: more values
        default:
            return NPERR_GENERIC_ERROR;
    }
}

int16_t
PluginInstanceParent::NPP_HandleEvent(void* event)
{
    _MOZ_LOG(__FUNCTION__);

    int16_t handled;
    if (!CallNPP_HandleEvent(*reinterpret_cast<NPEvent*>(event),
                             &handled)) {
        return 0;               // no good way to handle errors here...
    }

    return handled;
}

NPError
PluginInstanceParent::NPP_NewStream(NPMIMEType type, NPStream* stream,
                                    NPBool seekable, uint16_t* stype)
{
    _MOZ_LOG(__FUNCTION__);
        
    BrowserStreamParent* bs = new BrowserStreamParent(this, stream);

    NPError err;
    // TODO are any of these strings nullable?
    if (!CallPBrowserStreamConstructor(bs,
                                       nsCString(stream->url),
                                       stream->end,
                                       stream->lastmodified,
                                       static_cast<PStreamNotifyParent*>(stream->notifyData),
                                       nsCString(stream->headers),
                                       nsCString(type), seekable, &err, stype))
        return NPERR_GENERIC_ERROR;

    if (NPERR_NO_ERROR != err)
        CallPBrowserStreamDestructor(bs, NPERR_GENERIC_ERROR, true);

    return err;
}

NPError
PluginInstanceParent::NPP_DestroyStream(NPStream* stream, NPReason reason)
{
    _MOZ_LOG(__FUNCTION__);

    AStream* s = static_cast<AStream*>(stream->pdata);
    if (s->IsBrowserStream()) {
        BrowserStreamParent* sp =
            static_cast<BrowserStreamParent*>(s);
        if (sp->mNPP != this)
            NS_RUNTIMEABORT("Mismatched plugin data");

        CallPBrowserStreamDestructor(sp, reason, false);
        return NPERR_NO_ERROR;
    }
    else {
        PluginStreamParent* sp =
            static_cast<PluginStreamParent*>(s);
        if (sp->mInstance != this)
            NS_RUNTIMEABORT("Mismatched plugin data");

        CallPPluginStreamDestructor(sp, reason, false);
        return NPERR_NO_ERROR;
    }
}

PPluginScriptableObjectParent*
PluginInstanceParent::AllocPPluginScriptableObject()
{
    nsAutoPtr<PluginScriptableObjectParent>* object =
        mScriptableObjects.AppendElement();
    NS_ENSURE_TRUE(object, nsnull);

    *object = new PluginScriptableObjectParent();
    NS_ENSURE_TRUE(*object, nsnull);

    return object->get();
}

bool
PluginInstanceParent::DeallocPPluginScriptableObject(
                                         PPluginScriptableObjectParent* aObject)
{
    PluginScriptableObjectParent* object =
        reinterpret_cast<PluginScriptableObjectParent*>(aObject);

    PRUint32 count = mScriptableObjects.Length();
    for (PRUint32 index = 0; index < count; index++) {
        if (mScriptableObjects[index] == object) {
            mScriptableObjects.RemoveElementAt(index);
            return true;
        }
    }
    NS_NOTREACHED("An actor we don't know about?!");
    return false;
}

bool
PluginInstanceParent::AnswerPPluginScriptableObjectConstructor(
                                          PPluginScriptableObjectParent* aActor)
{
    const NPNetscapeFuncs* npn = mParent->GetNetscapeFuncs();
    if (!npn) {
        NS_WARNING("No netscape function pointers?!");
        return false;
    }

    ParentNPObject* object = reinterpret_cast<ParentNPObject*>(
        npn->createobject(mNPP, PluginScriptableObjectParent::GetClass()));
    if (!object) {
        return false;
    }

    reinterpret_cast<PluginScriptableObjectParent*>(aActor)->Initialize(
        const_cast<PluginInstanceParent*>(this), object);
    return true;
}

void
PluginInstanceParent::NPP_URLNotify(const char* url, NPReason reason,
                                    void* notifyData)
{
    _MOZ_LOG(__FUNCTION__);

    PStreamNotifyParent* streamNotify =
        static_cast<PStreamNotifyParent*>(notifyData);
    CallPStreamNotifyDestructor(streamNotify, reason);
}
