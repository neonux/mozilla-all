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
 *   Ben Turner <bent.mozilla@gmail.com>
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

#ifndef dom_plugins_PluginScriptableObjectChild_h
#define dom_plugins_PluginScriptableObjectChild_h 1

#include "mozilla/plugins/PPluginScriptableObjectChild.h"

#include "npruntime.h"

namespace mozilla {
namespace plugins {

class PluginInstanceChild;
class PluginScriptableObjectChild;

struct ChildNPObject : NPObject
{
  PluginScriptableObjectChild* parent;
  bool invalidated;
};

class PluginScriptableObjectChild : public PPluginScriptableObjectChild
{
  friend class PluginInstanceChild;

public:
  PluginScriptableObjectChild();
  virtual ~PluginScriptableObjectChild();

  virtual bool
  AnswerInvalidate();

  virtual bool
  AnswerHasMethod(const NPRemoteIdentifier& aId,
                  bool* aHasMethod);

  virtual bool
  AnswerInvoke(const NPRemoteIdentifier& aId,
               const nsTArray<Variant>& aArgs,
               Variant* aResult,
               bool* aSuccess);

  virtual bool
  AnswerInvokeDefault(const nsTArray<Variant>& aArgs,
                      Variant* aResult,
                      bool* aSuccess);

  virtual bool
  AnswerHasProperty(const NPRemoteIdentifier& aId,
                    bool* aHasProperty);

  virtual bool
  AnswerGetProperty(const NPRemoteIdentifier& aId,
                    Variant* aResult,
                    bool* aSuccess);

  virtual bool
  AnswerSetProperty(const NPRemoteIdentifier& aId,
                    const Variant& aValue,
                    bool* aSuccess);

  virtual bool
  AnswerRemoveProperty(const NPRemoteIdentifier& aId,
                       bool* aSuccess);

  virtual bool
  AnswerEnumerate(nsTArray<NPRemoteIdentifier>* aProperties,
                  bool* aSuccess);

  virtual bool
  AnswerConstruct(const nsTArray<Variant>& aArgs,
                  Variant* aResult,
                  bool* aSuccess);

  void
  Initialize(PluginInstanceChild* aInstance,
             NPObject* aObject);

  NPObject*
  GetObject()
  {
    return mObject;
  }

  static const NPClass*
  GetClass()
  {
    return &sNPClass;
  }

  PluginInstanceChild*
  GetInstance()
  {
    return mInstance;
  }

  bool
  Evaluate(NPString* aScript,
           NPVariant* aResult);

private:
  static NPObject*
  ScriptableAllocate(NPP aInstance,
                     NPClass* aClass);

  static void
  ScriptableInvalidate(NPObject* aObject);

  static void
  ScriptableDeallocate(NPObject* aObject);

  static bool
  ScriptableHasMethod(NPObject* aObject,
                      NPIdentifier aName);

  static bool
  ScriptableInvoke(NPObject* aObject,
                   NPIdentifier aName,
                   const NPVariant* aArgs,
                   uint32_t aArgCount,
                   NPVariant* aResult);

  static bool
  ScriptableInvokeDefault(NPObject* aObject,
                          const NPVariant* aArgs,
                          uint32_t aArgCount,
                          NPVariant* aResult);

  static bool
  ScriptableHasProperty(NPObject* aObject,
                        NPIdentifier aName);

  static bool
  ScriptableGetProperty(NPObject* aObject,
                        NPIdentifier aName,
                        NPVariant* aResult);

  static bool
  ScriptableSetProperty(NPObject* aObject,
                        NPIdentifier aName,
                        const NPVariant* aValue);

  static bool
  ScriptableRemoveProperty(NPObject* aObject,
                           NPIdentifier aName);

  static bool
  ScriptableEnumerate(NPObject* aObject,
                      NPIdentifier** aIdentifiers,
                      uint32_t* aCount);

  static bool
  ScriptableConstruct(NPObject* aObject,
                      const NPVariant* aArgs,
                      uint32_t aArgCount,
                      NPVariant* aResult);

private:
  PluginInstanceChild* mInstance;
  NPObject* mObject;

  static const NPClass sNPClass;
};

} /* namespace plugins */
} /* namespace mozilla */

#endif /* dom_plugins_PluginScriptableObjectChild_h */
