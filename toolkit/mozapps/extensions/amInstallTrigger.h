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
 * The Original Code is the Extension Manager.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Townsend <dtownsend@oxymoronical.com>
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

#include "jscntxt.h"
#include "amIInstallTrigger.h"
#include "nsIDOMWindowInternal.h"
#include "nsIURI.h"
#include "amIWebInstaller.h"
#include "nsCOMPtr.h"

#define AM_InstallTrigger_CID \
 {0xfcfcdf1e, 0xe9ef, 0x4141, {0x90, 0xd8, 0xd5, 0xff, 0x84, 0xc1, 0x7c, 0xce}}
#define AM_INSTALLTRIGGER_CONTRACTID "@mozilla.org/addons/installtrigger;1"

class amInstallTrigger : public amIInstallTrigger
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_AMIINSTALLTRIGGER

  amInstallTrigger();

private:
  ~amInstallTrigger();

  JSContext* GetJSContext();
  already_AddRefed<nsIDOMWindowInternal> GetOriginatingWindow(JSContext* cx);
  already_AddRefed<nsIURI> GetOriginatingURI(nsIDOMWindowInternal* aWindow);

  nsCOMPtr<amIWebInstaller> mManager;
};
