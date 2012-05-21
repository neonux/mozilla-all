/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozSpellI18NManager.h"
#include "mozEnglishWordUtils.h"
#include "mozGenericWordUtils.h"
#include "nsString.h"

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozSpellI18NManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozSpellI18NManager)

NS_INTERFACE_MAP_BEGIN(mozSpellI18NManager)
  NS_INTERFACE_MAP_ENTRY(mozISpellI18NManager)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, mozISpellI18NManager)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(mozSpellI18NManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_0(mozSpellI18NManager)

mozSpellI18NManager::mozSpellI18NManager()
{
  /* member initializers and constructor code */
}

mozSpellI18NManager::~mozSpellI18NManager()
{
  /* destructor code */
}

/* mozISpellI18NUtil GetUtil (in wstring language); */
NS_IMETHODIMP mozSpellI18NManager::GetUtil(const PRUnichar *aLanguage, mozISpellI18NUtil **_retval)
{
 if( NULL == _retval) {
   return NS_ERROR_NULL_POINTER;
 }
 *_retval = NULL;
 nsAutoString lang;
 lang.Assign(aLanguage);
 if(lang.EqualsLiteral("en")){
   *_retval = new mozEnglishWordUtils;
 }
 else{
   *_retval = new mozEnglishWordUtils;   
 }

 NS_IF_ADDREF(*_retval);
 return NS_OK;
}
