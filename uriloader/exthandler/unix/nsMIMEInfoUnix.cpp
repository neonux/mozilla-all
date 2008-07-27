/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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

#ifdef NS_HILDON
#include <glib.h>
#include <hildon-uri.h>
#endif


#include "nsMIMEInfoUnix.h"
#include "nsGNOMERegistry.h"
#include "nsIGnomeVFSService.h"
#include "nsPrintfCString.h"
#ifdef MOZ_ENABLE_DBUS
#include "nsDBusHandlerApp.h"
#endif


nsresult
nsMIMEInfoUnix::LoadUriInternal(nsIURI * aURI)
{ 
  nsresult rv = nsGNOMERegistry::LoadURL(aURI);
#ifdef NS_HILDON
  if (NS_FAILED(rv)){
    HildonURIAction *action = hildon_uri_get_default_action(mType.get(), nsnull);
    if (action) {
      nsCAutoString spec;
      aURI->GetAsciiSpec(spec);
      if (hildon_uri_open(spec.get(), action, nsnull))
        rv = NS_OK;
      hildon_uri_action_unref(action);
    }
  }
#endif
  return rv;
}

NS_IMETHODIMP
nsMIMEInfoUnix::GetHasDefaultHandler(PRBool *_retval)
{
  *_retval = PR_FALSE;
  nsCOMPtr<nsIGnomeVFSService> vfs = do_GetService(NS_GNOMEVFSSERVICE_CONTRACTID);
  if (vfs) {
    nsCOMPtr<nsIGnomeVFSMimeApp> app;
    if (NS_SUCCEEDED(vfs->GetAppForMimeType(mType, getter_AddRefs(app))) && app)
      *_retval = PR_TRUE;
  }

  if (*_retval)
    return NS_OK;

#ifdef NS_HILDON
  HildonURIAction *action = hildon_uri_get_default_action(mType.get(), nsnull);
  if (action) {
    *_retval = PR_TRUE;
    hildon_uri_action_unref(action);
    return NS_OK;
  }
#endif
  
  // If we didn't find a VFS handler, fallback.
  return nsMIMEInfoImpl::GetHasDefaultHandler(_retval);
}

nsresult
nsMIMEInfoUnix::LaunchDefaultWithFile(nsIFile *aFile)
{
  nsCAutoString nativePath;
  aFile->GetNativePath(nativePath);

  nsCOMPtr<nsIGnomeVFSService> vfs = do_GetService(NS_GNOMEVFSSERVICE_CONTRACTID);

  if (vfs) {
    nsCOMPtr<nsIGnomeVFSMimeApp> app;
    if (NS_SUCCEEDED(vfs->GetAppForMimeType(mType, getter_AddRefs(app))) && app)
      return app->Launch(nativePath);
  }

  if (!mDefaultApplication)
    return NS_ERROR_FILE_NOT_FOUND;

  return LaunchWithIProcess(mDefaultApplication, nativePath);
}

#ifdef NS_HILDON

NS_IMETHODIMP
nsMIMEInfoUnix::GetPossibleApplicationHandlers(nsIMutableArray ** aPossibleAppHandlers)
{
  if (!mPossibleApplications) {
    mPossibleApplications = do_CreateInstance(NS_ARRAY_CONTRACTID);
    
    if (!mPossibleApplications)
      return NS_ERROR_OUT_OF_MEMORY;

    GSList *actions = hildon_uri_get_actions(mType.get(), nsnull);
    GSList *actionsPtr = actions;
    while (actionsPtr) {
      HildonURIAction *action = (HildonURIAction*)actionsPtr->data;
      actionsPtr = actionsPtr->next;
      nsDBusHandlerApp* app = new nsDBusHandlerApp();
      if (!app){
        hildon_uri_free_actions(actions);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      nsDependentCString method(hildon_uri_action_get_method(action));
      nsDependentCString key(hildon_uri_action_get_service(action));
      nsCString service, objpath, interface;
      app->SetMethod(method);
      app->SetName(NS_ConvertUTF8toUTF16(key));

      if (key.FindChar('.', 0) > 0) {
        service.Assign(key);
        int len = key.Length() + 1;
        objpath.Assign(NS_LITERAL_CSTRING("/") + key);
        objpath.ReplaceChar('.', '/');
        interface.Assign(key);
      } else {
        int len = key.Length() + 11;
        service.Assign(NS_LITERAL_CSTRING("com.nokia.") + key);
        objpath.Assign(NS_LITERAL_CSTRING("/com/nokia/") + key);
        interface.Assign(NS_LITERAL_CSTRING("com.nokia.") + key);  
      }
      
      app->SetService(service);
      app->SetObjectPath(objpath);
      app->SetDBusInterface(interface);
      
      mPossibleApplications->AppendElement(app, PR_FALSE);    
    }
    hildon_uri_free_actions(actions);
  }

  *aPossibleAppHandlers = mPossibleApplications;
  NS_ADDREF(*aPossibleAppHandlers);
  return NS_OK;
}
#endif
