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

#include "mozilla/plugins/PluginModuleParent.h"
#include "mozilla/plugins/BrowserStreamParent.h"

#include "nsNPAPIPlugin.h"

using mozilla::SharedLibrary;

using mozilla::ipc::NPRemoteIdentifier;

using namespace mozilla::plugins;

PR_STATIC_ASSERT(sizeof(NPIdentifier) == sizeof(void*));

// HACKS
PluginModuleParent* PluginModuleParent::Shim::HACK_target;

SharedLibrary*
PluginModuleParent::LoadModule(const char* aFilePath, PRLibrary* aLibrary)
{
    _MOZ_LOG(__FUNCTION__);

    // Block on the child process being launched and initialized.
    PluginModuleParent* parent = new PluginModuleParent(aFilePath);
    parent->mSubprocess.Launch();
    parent->Open(parent->mSubprocess.GetChannel());

    // FIXME/cjones: leaking PluginModuleParents ...
    return parent->mShim;
}


PluginModuleParent::PluginModuleParent(const char* aFilePath) :
    mFilePath(aFilePath),
    mSubprocess(aFilePath),
    ALLOW_THIS_IN_INITIALIZER_LIST(mShim(new Shim(this)))
{
#ifdef DEBUG
    PRBool ok =
#endif
    mValidIdentifiers.Init();
    NS_ASSERTION(ok, "Out of memory!");
}

PluginModuleParent::~PluginModuleParent()
{
    _MOZ_LOG("  (closing Shim ...)");
    delete mShim;
}

PPluginInstanceParent*
PluginModuleParent::AllocPPluginInstance(const nsCString& aMimeType,
                                         const uint16_t& aMode,
                                         const nsTArray<nsCString>& aNames,
                                         const nsTArray<nsCString>& aValues,
                                         NPError* rv)
{
    NS_ERROR("Not reachable!");
    return NULL;
}

bool
PluginModuleParent::DeallocPPluginInstance(PPluginInstanceParent* aActor,
                                           NPError* _retval)
{
    _MOZ_LOG(__FUNCTION__);
    delete aActor;
    *_retval = NPERR_NO_ERROR;
    return true;
}

void
PluginModuleParent::SetPluginFuncs(NPPluginFuncs* aFuncs)
{
    aFuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
    aFuncs->javaClass = nsnull;

    // FIXME/cjones: /should/ dynamically allocate shim trampoline.
    // but here we just HACK
    aFuncs->newp = Shim::NPP_New;
    aFuncs->destroy = NPP_Destroy;
    aFuncs->setwindow = NPP_SetWindow;
    aFuncs->newstream = NPP_NewStream;
    aFuncs->destroystream = NPP_DestroyStream;
    aFuncs->asfile = NPP_StreamAsFile;
    aFuncs->writeready = NPP_WriteReady;
    aFuncs->write = NPP_Write;
    aFuncs->print = NPP_Print;
    aFuncs->event = NPP_HandleEvent;
    aFuncs->urlnotify = NPP_URLNotify;
    aFuncs->getvalue = NPP_GetValue;
    aFuncs->setvalue = NPP_SetValue;
}

#ifdef OS_LINUX
NPError
PluginModuleParent::NP_Initialize(const NPNetscapeFuncs* npnIface,
                                  NPPluginFuncs* nppIface)
{
    _MOZ_LOG(__FUNCTION__);

    mNPNIface = npnIface;

    NPError prv;
    if (!CallNP_Initialize(&prv))
        return NPERR_GENERIC_ERROR;
    else if (NPERR_NO_ERROR != prv)
        return prv;

    SetPluginFuncs(nppIface);
    return NPERR_NO_ERROR;
}
#else
NPError
PluginModuleParent::NP_Initialize(const NPNetscapeFuncs* npnIface)
{
    _MOZ_LOG(__FUNCTION__);

    mNPNIface = npnIface;

    NPError prv;
    if (!CallNP_Initialize(&prv))
        return NPERR_GENERIC_ERROR;
    return prv;
}

NPError
PluginModuleParent::NP_GetEntryPoints(NPPluginFuncs* nppIface)
{
    NS_ASSERTION(nppIface, "Null pointer!");

    SetPluginFuncs(nppIface);
    return NPERR_NO_ERROR;
}
#endif

NPError
PluginModuleParent::NPP_New(NPMIMEType pluginType,
                            NPP instance,
                            uint16_t mode,
                            int16_t argc,
                            char* argn[],
                            char* argv[],
                            NPSavedData* saved)
{
    _MOZ_LOG(__FUNCTION__);

    // create the instance on the other side
    nsTArray<nsCString> names;
    nsTArray<nsCString> values;

    for (int i = 0; i < argc; ++i) {
        names.AppendElement(NullableString(argn[i]));
        values.AppendElement(NullableString(argv[i]));
    }

    NPError prv = NPERR_GENERIC_ERROR;
    nsAutoPtr<PluginInstanceParent> parentInstance(
        new PluginInstanceParent(this, instance, mNPNIface));

    instance->pdata = parentInstance.get();

    if (!CallPPluginInstanceConstructor(parentInstance,
                                        nsDependentCString(pluginType), mode,
                                        names,values, &prv))
        return NPERR_GENERIC_ERROR;

    printf ("[PluginModuleParent] %s: got return value %hd\n", __FUNCTION__,
            prv);

    if (NPERR_NO_ERROR == prv)
        parentInstance.forget();

    return prv;
}

NPError
PluginModuleParent::NPP_Destroy(NPP instance,
                                NPSavedData** save)
{
    // FIXME/cjones:
    //  (1) send a "destroy" message to the child
    //  (2) the child shuts down its instance
    //  (3) remove both parent and child IDs from map
    //  (4) free parent

    _MOZ_LOG(__FUNCTION__);

    PluginInstanceParent* parentInstance =
        static_cast<PluginInstanceParent*>(instance->pdata);

    parentInstance->Destroy();

    NPError prv;
    if (!Shim::HACK_target->CallPPluginInstanceDestructor(parentInstance, &prv)) {
        prv = NPERR_GENERIC_ERROR;
    }
    instance->pdata = nsnull;

    return prv;
 }

bool
PluginModuleParent::EnsureValidNPIdentifier(NPIdentifier aIdentifier)
{
    if (!mValidIdentifiers.GetEntry(aIdentifier)) {
        nsVoidPtrHashKey* newEntry = mValidIdentifiers.PutEntry(aIdentifier);
        if (!newEntry) {
            NS_ERROR("Out of memory?");
            return false;
        }
    }
    return true;
}

NPIdentifier
PluginModuleParent::GetValidNPIdentifier(NPRemoteIdentifier aRemoteIdentifier)
{
    NS_ASSERTION(mValidIdentifiers.IsInitialized(), "Not initialized!");
    if (aRemoteIdentifier &&
        mValidIdentifiers.GetEntry((NPIdentifier)aRemoteIdentifier)) {
        return (NPIdentifier)aRemoteIdentifier;
    }
    return 0;
}

NPError
PluginModuleParent::NPP_NewStream(NPP instance, NPMIMEType type,
                                  NPStream* stream, NPBool seekable,
                                  uint16_t* stype)
{
    return InstCast(instance)->NPP_NewStream(type, stream, seekable,
                                             stype);
}

NPError
PluginModuleParent::NPP_SetWindow(NPP instance, NPWindow* window)
{
     return InstCast(instance)->NPP_SetWindow(window);
}

NPError
PluginModuleParent::NPP_DestroyStream(NPP instance,
                                      NPStream* stream,
                                      NPReason reason)
{
    return InstCast(instance)->NPP_DestroyStream(stream, reason);
}

int32_t
PluginModuleParent::NPP_WriteReady(NPP instance,
                                   NPStream* stream)
{
    return StreamCast(instance, stream)->WriteReady();
}

int32_t
PluginModuleParent::NPP_Write(NPP instance,
                              NPStream* stream,
                              int32_t offset,
                              int32_t len,
                              void* buffer)
{
    return StreamCast(instance, stream)->Write(offset, len, buffer);
}

void
PluginModuleParent::NPP_StreamAsFile(NPP instance,
                                     NPStream* stream,
                                     const char* fname)
{
    StreamCast(instance, stream)->StreamAsFile(fname);
}

void
PluginModuleParent::NPP_Print(NPP instance, NPPrint* platformPrint)
{
    InstCast(instance)->NPP_Print(platformPrint);
}

int16_t
PluginModuleParent::NPP_HandleEvent(NPP instance, void* event)
{
    return InstCast(instance)->NPP_HandleEvent(event);
}

void
PluginModuleParent::NPP_URLNotify(NPP instance, const char* url,
                                  NPReason reason, void* notifyData)
{
    return InstCast(instance)->NPP_URLNotify(url, reason, notifyData);
}

NPError
PluginModuleParent::NPP_GetValue(NPP instance,
                                 NPPVariable variable, void *ret_value)
{
    return InstCast(instance)->NPP_GetValue(variable, ret_value);
}

NPError
PluginModuleParent::NPP_SetValue(NPP instance, NPNVariable variable,
                                 void *value)
{
    return InstCast(instance)->NPP_SetValue(variable, value);
}

bool
PluginModuleParent::AnswerNPN_UserAgent(nsCString* userAgent)
{
    NPP_t dummy = { 0, 0 };
    *userAgent = NullableString(mNPNIface->uagent(&dummy));
    return true;
}

bool
PluginModuleParent::RecvNPN_GetStringIdentifier(const nsCString& aString,
                                                NPRemoteIdentifier* aId)
{
    if (aString.IsVoid()) {
        NS_ERROR("Someone sent over a void string?!");
        return false;
    }

    NPIdentifier ident = _getstringidentifier(aString.BeginReading());
    if (!ident) {
        *aId = 0;
        return true;
    }

    if (!EnsureValidNPIdentifier(ident)) {
        NS_ERROR("Out of memory?");
        return false;
    }

    *aId = (NPRemoteIdentifier)ident;
    return true;
}

bool
PluginModuleParent::RecvNPN_GetIntIdentifier(const int32_t& aInt,
                                             NPRemoteIdentifier* aId)
{
    NPIdentifier ident = _getintidentifier(aInt);
    if (!ident) {
        *aId = 0;
        return true;
    }

    if (!EnsureValidNPIdentifier(ident)) {
        NS_ERROR("Out of memory?");
        return false;
    }

    *aId = (NPRemoteIdentifier)ident;
    return true;
}

bool
PluginModuleParent::RecvNPN_UTF8FromIdentifier(const NPRemoteIdentifier& aId,
                                               NPError *err,
                                               nsCString* aString)
{
    NPIdentifier ident = GetValidNPIdentifier(aId);
    if (!ident) {
        *err = NPERR_INVALID_PARAM;
        return true;
    }

    NPUTF8* val = _utf8fromidentifier(ident);
    if (!val) {
        *err = NPERR_INVALID_PARAM;
        return true;
    }

    aString->Assign(val);
    *err = NPERR_NO_ERROR;
    return true;
}

bool
PluginModuleParent::RecvNPN_IntFromIdentifier(const NPRemoteIdentifier& aId,
                                              NPError* err,
                                              int32_t* aInt)
{
    NPIdentifier ident = GetValidNPIdentifier(aId);
    if (!ident) {
        *err = NPERR_INVALID_PARAM;
        return true;
    }

    *aInt = _intfromidentifier(ident);
    *err = NPERR_NO_ERROR;
    return true;
}

bool
PluginModuleParent::RecvNPN_IdentifierIsString(const NPRemoteIdentifier& aId,
                                               bool* aIsString)
{
    NPIdentifier ident = GetValidNPIdentifier(aId);
    if (!ident) {
        *aIsString = false;
        return true;
    }

    *aIsString = _identifierisstring(ident);
    return true;
}

bool
PluginModuleParent::RecvNPN_GetStringIdentifiers(const nsTArray<nsCString>& aNames,
                                                 nsTArray<NPRemoteIdentifier>* aIds)
{
    NS_ASSERTION(aIds->IsEmpty(), "Non-empty array!");

    PRUint32 count = aNames.Length();
    if (!count) {
        NS_ERROR("No names to get!");
        return false;
    }

    nsAutoTArray<NPUTF8*, 10> buffers;
    nsAutoTArray<NPIdentifier, 10> ids;

    if (!(buffers.SetLength(count) &&
          ids.SetLength(count) &&
          aIds->SetCapacity(count))) {
        NS_ERROR("Out of memory?");
        return false;
    }

    for (PRUint32 index = 0; index < count; index++) {
        buffers[index] = const_cast<NPUTF8*>(aNames[index].BeginReading());
        NS_ASSERTION(buffers[index], "Null pointer should be impossible!");
    }

    _getstringidentifiers(const_cast<const NPUTF8**>(buffers.Elements()),
                          count, ids.Elements());

    for (PRUint32 index = 0; index < count; index++) {
        NPIdentifier& id = ids[index];
        if (id) {
            if (!EnsureValidNPIdentifier(id)) {
                NS_ERROR("Out of memory?");
                return false;
            }
        }
        aIds->AppendElement((NPRemoteIdentifier)id);
    }

    return true;
}

PluginInstanceParent*
PluginModuleParent::InstCast(NPP instance)
{
    PluginInstanceParent* ip =
        static_cast<PluginInstanceParent*>(instance->pdata);
    if (instance != ip->mNPP) {
        NS_RUNTIMEABORT("Corrupted plugin data.");
    }
    return ip;
}

BrowserStreamParent*
PluginModuleParent::StreamCast(NPP instance,
                               NPStream* s)
{
    PluginInstanceParent* ip = InstCast(instance);
    BrowserStreamParent* sp =
        static_cast<BrowserStreamParent*>(static_cast<AStream*>(s->pdata));
    if (sp->mNPP != ip || s != sp->mStream) {
        NS_RUNTIMEABORT("Corrupted plugin stream data.");
    }
    return sp;
}

