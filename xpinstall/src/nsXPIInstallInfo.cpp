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
 * The Original Code is Mozilla XPInstall.
 *
 * The Initial Developer of the Original Code is
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
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
 * ***** END LICENSE BLOCK *****
 */

#include "nsXPIInstallInfo.h"

NS_IMPL_ISUPPORTS1(nsXPIInstallInfo, nsIXPIInstallInfo)

nsXPIInstallInfo::nsXPIInstallInfo(nsIDOMWindow *aOriginatingWindow,
                                   nsIURI *aOriginatingURI,
                                   nsXPITriggerInfo *aTriggerInfo,
                                   PRUint32 aChromeType)
  : mOriginatingWindow(aOriginatingWindow), mOriginatingURI(aOriginatingURI),
    mTriggerInfo(aTriggerInfo), mChromeType(aChromeType)
{
}

nsXPIInstallInfo::~nsXPIInstallInfo()
{
    delete mTriggerInfo;
}

/* [noscript, notxpcom] attribute triggerInfoPtr triggerInfo; */
NS_IMETHODIMP
nsXPIInstallInfo::GetTriggerInfo(nsXPITriggerInfo * *aTriggerInfo)
{
    *aTriggerInfo = mTriggerInfo;
    return NS_OK;
}

NS_IMETHODIMP
nsXPIInstallInfo::SetTriggerInfo(nsXPITriggerInfo * aTriggerInfo)
{
    mTriggerInfo = aTriggerInfo;
    return NS_OK;
}

/* readonly attribute nsIDOMWindow originatingWindow; */
NS_IMETHODIMP
nsXPIInstallInfo::GetOriginatingWindow(nsIDOMWindow * *aOriginatingWindow)
{
    NS_IF_ADDREF(*aOriginatingWindow = mOriginatingWindow);
    return NS_OK;
}

/* readonly attribute nsIURI uri; */
NS_IMETHODIMP
nsXPIInstallInfo::GetOriginatingURI(nsIURI * *aOriginatingURI)
{
    NS_IF_ADDREF(*aOriginatingURI = mOriginatingURI);
    return NS_OK;
}

/* readonly attribute PRUint32 type; */
NS_IMETHODIMP
nsXPIInstallInfo::GetChromeType(PRUint32 *aChromeType)
{
    *aChromeType = mChromeType;
    return NS_OK;
}
