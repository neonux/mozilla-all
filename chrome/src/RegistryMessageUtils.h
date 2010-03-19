/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net>
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

#ifndef RegistryMessageUtils_h__
#define RegistryMessageUtils_h__

#include "IPC/IPCMessageUtils.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

#ifndef MOZILLA_INTERNAL_API
#include "nsStringAPI.h"
#else
#include "nsString.h"
#endif

namespace IPC {

inline void WriteURI(Message* aMsg, nsIURI* aURI)
{
  nsCString spec;
  if (aURI)
    aURI->GetSpec(spec);
  WriteParam(aMsg, spec);
}

inline bool ReadURI(const Message* aMsg, void** aIter, nsIURI* *aURI)
{
  *aURI = nsnull;
  
  nsCString spec;
  if (!ReadParam(aMsg, aIter, &spec))
    return false;

  if (spec.Length()) {
    nsresult rv = NS_NewURI(aURI, spec);
    NS_ENSURE_SUCCESS(rv, false);
  }

  return true;
}
  
template <>
struct ParamTraits<ChromePackage>
{
  typedef ChromePackage paramType;
  
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.package);
    WriteURI(aMsg, aParam.baseURI);
    WriteParam(aMsg, aParam.flags);
  }
  
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    nsCString package;
    nsCOMPtr<nsIURI> uri;
    PRUint32 flags;
    
    if (ReadParam(aMsg, aIter, &package) &&
        ReadURI(aMsg, aIter, getter_AddRefs(uri)) &&
        ReadParam(aMsg, aIter, &flags)) {
      aResult->package = package;
      aResult->baseURI = uri;
      aResult->flags = flags;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    nsCString spec;
    aParam.baseURI->GetSpec(spec);
    aLog->append(StringPrintf(L"[%s, %s, %u]", aParam.package.get(),
                             spec.get(), aParam.flags));
  }
};

template <>
struct ParamTraits<ChromeResource>
{
  typedef ChromeResource paramType;
  
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.package);
    WriteURI(aMsg, aParam.resolvedURI);
  }
  
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    nsCString package;
    nsCOMPtr<nsIURI> uri;
    
    if (ReadParam(aMsg, aIter, &package) &&
        ReadURI(aMsg, aIter, getter_AddRefs(uri))) {
      aResult->package = package;
      aResult->resolvedURI = uri;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    nsCString spec;
    aParam.resolvedURI->GetSpec(spec);
    aLog->append(StringPrintf(L"[%s, %s, %u]", aParam.package.get(),
                             spec.get()));
  }
};

}

#endif // RegistryMessageUtils_h__
