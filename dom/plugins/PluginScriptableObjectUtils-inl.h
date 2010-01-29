/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
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
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
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

#include "PluginScriptableObjectUtils.h"

namespace {

template<class InstanceType>
class VariantTraits;

template<>
class VariantTraits<mozilla::plugins::PluginInstanceParent>
{
public:
  typedef mozilla::plugins::PluginScriptableObjectParent ScriptableObjectType;
};

template<>
class VariantTraits<mozilla::plugins::PluginInstanceChild>
{
public:
  typedef mozilla::plugins::PluginScriptableObjectChild ScriptableObjectType;
};

} /* anonymous namespace */

inline bool
mozilla::plugins::ConvertToVariant(const Variant& aRemoteVariant,
                                   NPVariant& aVariant,
                                   PluginInstanceParent* aInstance)
{
  switch (aRemoteVariant.type()) {
    case Variant::Tvoid_t: {
      VOID_TO_NPVARIANT(aVariant);
      break;
    }

    case Variant::Tnull_t: {
      NULL_TO_NPVARIANT(aVariant);
      break;
    }

    case Variant::Tbool: {
      BOOLEAN_TO_NPVARIANT(aRemoteVariant.get_bool(), aVariant);
      break;
    }

    case Variant::Tint: {
      INT32_TO_NPVARIANT(aRemoteVariant.get_int(), aVariant);
      break;
    }

    case Variant::Tdouble: {
      DOUBLE_TO_NPVARIANT(aRemoteVariant.get_double(), aVariant);
      break;
    }

    case Variant::TnsCString: {
      const nsCString& string = aRemoteVariant.get_nsCString();
      NPUTF8* buffer = reinterpret_cast<NPUTF8*>(strdup(string.get()));
      if (!buffer) {
        NS_ERROR("Out of memory!");
        return false;
      }
      STRINGN_TO_NPVARIANT(buffer, string.Length(), aVariant);
      break;
    }

    case Variant::TPPluginScriptableObjectParent: {
      NS_ASSERTION(aInstance, "Must have an instance!");
      NPObject* object = NPObjectFromVariant(aRemoteVariant);
      if (!object) {
        NS_ERROR("Er, this shouldn't fail!");
        return false;
      }

      const NPNetscapeFuncs* npn = GetNetscapeFuncs(aInstance);
      if (!npn) {
        NS_ERROR("Null netscape funcs!");
        return false;
      }

      npn->retainobject(object);
      OBJECT_TO_NPVARIANT(object, aVariant);
      break;
    }

    case Variant::TPPluginScriptableObjectChild: {
      NS_ASSERTION(!aInstance, "No instance should be given!");
      NS_ASSERTION(PluginModuleChild::current(),
                   "Should be running on child only!");

      NPObject* object = NPObjectFromVariant(aRemoteVariant);
      NS_ASSERTION(object, "Null object?!");

      PluginModuleChild::sBrowserFuncs.retainobject(object);
      OBJECT_TO_NPVARIANT(object, aVariant);
      break;
    }

    default:
      NS_NOTREACHED("Shouldn't get here!");
      return false;
  }

  return true;
}

template <class InstanceType>
bool
mozilla::plugins::ConvertToRemoteVariant(const NPVariant& aVariant,
                                         Variant& aRemoteVariant,
                                         InstanceType* aInstance,
                                         bool aProtectActors)
{
  if (NPVARIANT_IS_VOID(aVariant)) {
    aRemoteVariant = mozilla::void_t();
  }
  else if (NPVARIANT_IS_NULL(aVariant)) {
    aRemoteVariant = mozilla::null_t();
  }
  else if (NPVARIANT_IS_BOOLEAN(aVariant)) {
    aRemoteVariant = NPVARIANT_TO_BOOLEAN(aVariant);
  }
  else if (NPVARIANT_IS_INT32(aVariant)) {
    aRemoteVariant = NPVARIANT_TO_INT32(aVariant);
  }
  else if (NPVARIANT_IS_DOUBLE(aVariant)) {
    aRemoteVariant = NPVARIANT_TO_DOUBLE(aVariant);
  }
  else if (NPVARIANT_IS_STRING(aVariant)) {
    NPString str = NPVARIANT_TO_STRING(aVariant);
    nsCString string(str.UTF8Characters, str.UTF8Length);
    aRemoteVariant = string;
  }
  else if (NPVARIANT_IS_OBJECT(aVariant)) {
    NPObject* object = NPVARIANT_TO_OBJECT(aVariant);

    typename VariantTraits<InstanceType>::ScriptableObjectType* actor =
      aInstance->GetActorForNPObject(object);

    if (!actor) {
      NS_ERROR("Null actor!");
      return false;
    }

    if (aProtectActors) {
      actor->Protect();
    }

    aRemoteVariant = actor;
  }
  else {
    NS_NOTREACHED("Shouldn't get here!");
    return false;
  }

  return true;
}
