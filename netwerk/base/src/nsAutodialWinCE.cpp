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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Meredith <smeredith@netscape.com>
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

#include <objbase.h>

#ifdef WINCE_WINDOWS_MOBILE
#include <connmgr.h>
#endif

#include "nsAutodialWinCE.h"

#include "nsCOMPtr.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"


// pulled from the header so that we do not get multiple define errors during link
static const GUID ras_DestNetInternet =
        { 0x436ef144, 0xb4fb, 0x4863, { 0xa0, 0x41, 0x8f, 0x90, 0x5a, 0x62, 0xc5, 0x72 } };

nsRASAutodial::nsRASAutodial()
{
}

nsRASAutodial::~nsRASAutodial()
{
}

nsresult
nsRASAutodial::Init()
{
  return NS_OK;
}

nsresult nsRASAutodial::DialDefault(const PRUnichar* /* hostName */)
{
#ifdef WINCE_WINDOWS_MOBILE
  HANDLE connectionHandle;

  // Make the connection to the new network
  CONNMGR_CONNECTIONINFO conn_info;
  memset(&conn_info, 0, sizeof(conn_info));
  
  conn_info.cbSize      = sizeof(conn_info);
  conn_info.dwParams    = CONNMGR_PARAM_GUIDDESTNET;
  conn_info.dwPriority  = CONNMGR_PRIORITY_USERINTERACTIVE;
  conn_info.guidDestNet = ras_DestNetInternet;
  conn_info.bExclusive  = FALSE;
  conn_info.bDisabled   = FALSE;
  
  DWORD status;
  HRESULT result = ConnMgrEstablishConnectionSync(&conn_info, 
						  &connectionHandle, 
						  60000,
						  &status);
  if (result != S_OK)
    return NS_ERROR_FAILURE;

  PRInt32 defaultCacheTime = 1;    // 1 second according to msdn
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefs) {
    PRInt32 t;
    if (NS_SUCCEEDED(prefs->GetIntPref("network.autodial.cacheTime", &t)))
	defaultCacheTime = t;
  }

  ConnMgrReleaseConnection(connectionHandle, defaultCacheTime);
  
  if (status != CONNMGR_STATUS_CONNECTED)
    return NS_ERROR_FAILURE;

  return NS_OK;
#else
  return NS_ERROR_FAILURE;
#endif
}

PRBool
nsRASAutodial::ShouldDialOnNetworkError()
{
#ifdef WINCE_WINDOWS_MOBILE
  return PR_TRUE;
#else
  return PR_FALSE;
#endif
}
