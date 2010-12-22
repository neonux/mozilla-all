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
 * The Original Code is third party utility code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@mozilla.com)
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

#include "ThirdPartyUtil.h"
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#include "nsIHttpChannelInternal.h"
#include "nsIDOMWindow.h"
#include "nsILoadContext.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsThreadUtils.h"

NS_IMPL_ISUPPORTS1(ThirdPartyUtil, mozIThirdPartyUtil)

nsresult
ThirdPartyUtil::Init()
{
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_NOT_AVAILABLE);

  nsresult rv;
  mTLDService = do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID, &rv);
  return rv;
}

// Get the base domain for aHostURI; e.g. for "www.bbc.co.uk", this would be
// "bbc.co.uk". Only properly-formed URI's are tolerated, though a trailing
// dot may be present. If aHostURI is an IP address, an alias such as
// 'localhost', an eTLD such as 'co.uk', or the empty string, aBaseDomain will
// be the exact host. The result of this function should only be used in exact
// string comparisons, since substring comparisons will not be valid for the
// special cases elided above.
nsresult
ThirdPartyUtil::GetBaseDomain(nsIURI* aHostURI,
                              nsCString& aBaseDomain)
{
  // Get the base domain. this will fail if the host contains a leading dot,
  // more than one trailing dot, or is otherwise malformed.
  nsresult rv = mTLDService->GetBaseDomain(aHostURI, 0, aBaseDomain);
  if (rv == NS_ERROR_HOST_IS_IP_ADDRESS ||
      rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
    // aHostURI is either an IP address, an alias such as 'localhost', an eTLD
    // such as 'co.uk', or the empty string. Uses the normalized host in such
    // cases.
    rv = aHostURI->GetAsciiHost(aBaseDomain);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // aHostURI (and thus aBaseDomain) may be the string '.'. If so, fail.
  if (aBaseDomain.Length() == 1 && aBaseDomain.Last() == '.')
    return NS_ERROR_INVALID_ARG;

  // Reject any URIs without a host that aren't file:// URIs. This makes it the
  // only way we can get a base domain consisting of the empty string, which
  // means we can safely perform foreign tests on such URIs where "not foreign"
  // means "the involved URIs are all file://".
  if (aBaseDomain.IsEmpty()) {
    PRBool isFileURI = PR_FALSE;
    aHostURI->SchemeIs("file", &isFileURI);
    NS_ENSURE_TRUE(isFileURI, NS_ERROR_INVALID_ARG);
  }

  return NS_OK;
}

// Determine if aFirstDomain is a different base domain to aSecondURI; or, if
// the concept of base domain does not apply, determine if the two hosts are not
// string-identical.
nsresult
ThirdPartyUtil::IsThirdPartyInternal(const nsCString& aFirstDomain,
                                     nsIURI* aSecondURI,
                                     PRBool* aResult)
{
  NS_ASSERTION(aSecondURI, "null URI!");

  // Get the base domain for aSecondURI.
  nsCString secondDomain;
  nsresult rv = GetBaseDomain(aSecondURI, secondDomain);
  if (NS_FAILED(rv))
    return rv;

  // Check strict equality.
  *aResult = aFirstDomain != secondDomain;
  return NS_OK;
}

// Get the URI associated with a window.
already_AddRefed<nsIURI>
ThirdPartyUtil::GetURIFromWindow(nsIDOMWindow* aWin)
{
  nsCOMPtr<nsIScriptObjectPrincipal> scriptObjPrin = do_QueryInterface(aWin);
  NS_ENSURE_TRUE(scriptObjPrin, NULL);

  nsIPrincipal* prin = scriptObjPrin->GetPrincipal();
  NS_ENSURE_TRUE(prin, NULL);

  nsCOMPtr<nsIURI> result;
  prin->GetURI(getter_AddRefs(result));
  return result.forget();
}

// Determine if aFirstURI is third party with respect to aSecondURI. See docs
// for mozIThirdPartyUtil.
NS_IMETHODIMP
ThirdPartyUtil::IsThirdPartyURI(nsIURI* aFirstURI,
                                nsIURI* aSecondURI,
                                PRBool* aResult)
{
  NS_ENSURE_ARG(aFirstURI);
  NS_ENSURE_ARG(aSecondURI);
  NS_ASSERTION(aResult, "null outparam pointer");

  nsCString firstHost;
  nsresult rv = GetBaseDomain(aFirstURI, firstHost);
  if (NS_FAILED(rv))
    return rv;

  return IsThirdPartyInternal(firstHost, aSecondURI, aResult);
}

// Determine if any URI of the window hierarchy of aWindow is foreign with
// respect to aSecondURI. See docs for mozIThirdPartyUtil.
NS_IMETHODIMP
ThirdPartyUtil::IsThirdPartyWindow(nsIDOMWindow* aWindow,
                                   nsIURI* aURI,
                                   PRBool* aResult)
{
  NS_ENSURE_ARG(aWindow);
  NS_ASSERTION(aResult, "null outparam pointer");

  PRBool result;

  // Get the URI of the window, and its base domain.
  nsCOMPtr<nsIURI> currentURI = GetURIFromWindow(aWindow);
  NS_ENSURE_TRUE(currentURI, NS_ERROR_INVALID_ARG);

  nsCString bottomDomain;
  nsresult rv = GetBaseDomain(currentURI, bottomDomain);
  if (NS_FAILED(rv))
    return rv;

  if (aURI) {
    // Determine whether aURI is foreign with respect to currentURI.
    rv = IsThirdPartyInternal(bottomDomain, aURI, &result);
    if (NS_FAILED(rv))
      return rv;

    if (result) {
      *aResult = true;
      return NS_OK;
    }
  }

  nsCOMPtr<nsIDOMWindow> current = aWindow, parent;
  nsCOMPtr<nsIURI> parentURI;
  do {
    rv = current->GetParent(getter_AddRefs(parent));
    NS_ENSURE_SUCCESS(rv, rv);

    if (SameCOMIdentity(parent, current)) {
      // We're at the topmost content window. We already know the answer.
      *aResult = false;
      return NS_OK;
    }

    parentURI = GetURIFromWindow(parent);
    NS_ENSURE_TRUE(parentURI, NS_ERROR_INVALID_ARG);

    rv = IsThirdPartyInternal(bottomDomain, parentURI, &result);
    if (NS_FAILED(rv))
      return rv;

    if (result) {
      *aResult = true;
      return NS_OK;
    }

    current = parent;
    currentURI = parentURI;
  } while (1);

  NS_NOTREACHED("should've returned");
  return NS_ERROR_UNEXPECTED;
}

// Determine if the URI associated with aChannel or any URI of the window
// hierarchy associated with the channel is foreign with respect to aSecondURI.
// See docs for mozIThirdPartyUtil.
NS_IMETHODIMP 
ThirdPartyUtil::IsThirdPartyChannel(nsIChannel* aChannel,
                                    nsIURI* aURI,
                                    PRBool* aResult)
{
  NS_ENSURE_ARG(aChannel);
  NS_ASSERTION(aResult, "null outparam pointer");

  nsresult rv;
  PRBool doForce = false;
  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
    do_QueryInterface(aChannel);
  if (httpChannelInternal) {
    rv = httpChannelInternal->GetForceAllowThirdPartyCookie(&doForce);
    NS_ENSURE_SUCCESS(rv, rv);

    // If aURI was not supplied, and we're forcing, then we're by definition
    // not foreign. If aURI was supplied, we still want to check whether it's
    // foreign with respect to the channel URI. (The forcing only applies to
    // whatever window hierarchy exists above the channel.)
    if (doForce && !aURI) {
      *aResult = false;
      return NS_OK;
    }
  }

  // Obtain the URI from the channel, and its base domain.
  nsCOMPtr<nsIURI> channelURI;
  aChannel->GetURI(getter_AddRefs(channelURI));
  NS_ENSURE_TRUE(channelURI, NS_ERROR_INVALID_ARG);

  nsCString channelDomain;
  rv = GetBaseDomain(channelURI, channelDomain);
  if (NS_FAILED(rv))
    return rv;

  if (aURI) {
    // Determine whether aURI is foreign with respect to channelURI.
    PRBool result;
    rv = IsThirdPartyInternal(channelDomain, aURI, &result);
    if (NS_FAILED(rv))
     return rv;

    // If it's foreign, or we're forcing, we're done.
    if (result || doForce) {
      *aResult = result;
      return NS_OK;
    }
  }

  // Find the associated window and its parent window.
  nsCOMPtr<nsILoadContext> ctx;
  NS_QueryNotificationCallbacks(aChannel, ctx);
  if (!ctx) return NS_ERROR_INVALID_ARG;

  // If there is no window, the consumer kicking off the load didn't provide one
  // to the channel. This is limited to loads of certain types of resources. If
  // those loads require cookies, the forceAllowThirdPartyCookie property should
  // be set on the channel.
  nsCOMPtr<nsIDOMWindow> ourWin, parentWin;
  ctx->GetAssociatedWindow(getter_AddRefs(ourWin));
  if (!ourWin) return NS_ERROR_INVALID_ARG;

  ourWin->GetParent(getter_AddRefs(parentWin));
  NS_ENSURE_TRUE(parentWin, NS_ERROR_INVALID_ARG);

  if (SameCOMIdentity(ourWin, parentWin)) {
    // Check whether this is the document channel for this window (representing
    // a load of a new page). This covers the case of a freshly kicked-off load
    // (e.g. the user typing something in the location bar, or clicking on a
    // bookmark), where the window's URI hasn't yet been set, and will be bogus.
    // This is a bit of a nasty hack, but we will hopefully flag these channels
    // better later.
    nsLoadFlags flags;
    rv = aChannel->GetLoadFlags(&flags);
    NS_ENSURE_SUCCESS(rv, rv);

    if (flags & nsIChannel::LOAD_DOCUMENT_URI) {
      // We only need to compare aURI to the channel URI -- the window's will be
      // bogus. We already know the answer.
      *aResult = false;
      return NS_OK;
    }
  }

  // Check the window hierarchy. This covers most cases for an ordinary page
  // load from the location bar.
  return IsThirdPartyWindow(ourWin, channelURI, aResult);
}

