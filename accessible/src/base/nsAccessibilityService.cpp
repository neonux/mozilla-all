/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt (jgaunt@netscape.com)
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

// NOTE: alphabetically ordered
#include "nsAccessibilityAtoms.h"
#include "nsAccessibilityService.h"
#include "nsCoreUtils.h"
#include "nsAccUtils.h"
#include "nsApplicationAccessibleWrap.h"
#include "nsARIAGridAccessibleWrap.h"
#include "nsARIAMap.h"
#include "nsIContentViewer.h"
#include "nsCURILoader.h"
#include "nsDocAccessible.h"
#include "nsHTMLImageMapAccessible.h"
#include "nsHTMLLinkAccessible.h"
#include "nsHTMLSelectAccessible.h"
#include "nsHTMLTableAccessibleWrap.h"
#include "nsHTMLTextAccessible.h"
#include "nsHyperTextAccessibleWrap.h"
#include "nsIAccessibilityService.h"
#include "nsIAccessibleProvider.h"

#include "nsIDOMDocument.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMWindow.h"
#include "nsIDOMXULElement.h"
#include "nsIHTMLDocument.h"
#include "nsIDocShell.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIImageFrame.h"
#include "nsILink.h"
#include "nsINameSpaceManager.h"
#include "nsIObserverService.h"
#include "nsIPluginInstance.h"
#include "nsIPresShell.h"
#include "nsISupportsUtils.h"
#include "nsIWebNavigation.h"
#include "nsObjectFrame.h"
#include "nsOuterDocAccessible.h"
#include "nsRootAccessibleWrap.h"
#include "nsTextFragment.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicharUtils.h"
#include "nsIWebProgress.h"
#include "nsNetError.h"
#include "nsDocShellLoadTypes.h"

#ifdef MOZ_XUL
#include "nsXULAlertAccessible.h"
#include "nsXULColorPickerAccessible.h"
#include "nsXULComboboxAccessible.h"
#include "nsXULFormControlAccessible.h"
#include "nsXULListboxAccessibleWrap.h"
#include "nsXULMenuAccessibleWrap.h"
#include "nsXULSliderAccessible.h"
#include "nsXULTabAccessible.h"
#include "nsXULTextAccessible.h"
#include "nsXULTreeGridAccessibleWrap.h"
#endif

// For native window support for object/embed/applet tags
#ifdef XP_WIN
#include "nsHTMLWin32ObjectAccessible.h"
#endif

#ifndef DISABLE_XFORMS_HOOKS
#include "nsXFormsFormControlsAccessible.h"
#include "nsXFormsWidgetsAccessible.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService
////////////////////////////////////////////////////////////////////////////////

nsAccessibilityService *nsAccessibilityService::gAccessibilityService = nsnull;
PRBool nsAccessibilityService::gIsShutdown = PR_TRUE;

nsAccessibilityService::nsAccessibilityService()
{
  // Add observers.
  nsCOMPtr<nsIObserverService> observerService = 
    do_GetService("@mozilla.org/observer-service;1");
  if (!observerService)
    return;

  observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  nsCOMPtr<nsIWebProgress> progress(do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID));
  if (progress) {
    progress->AddProgressListener(static_cast<nsIWebProgressListener*>(this),
                                  nsIWebProgress::NOTIFY_STATE_DOCUMENT);
  }

  // Initialize accessibility.
  nsAccessNodeWrap::InitAccessibility();
}

nsAccessibilityService::~nsAccessibilityService()
{
  NS_ASSERTION(gIsShutdown, "Accessibility wasn't shutdown!");
  gAccessibilityService = nsnull;
}

NS_IMPL_THREADSAFE_ISUPPORTS5(nsAccessibilityService, nsIAccessibilityService, nsIAccessibleRetrieval,
                              nsIObserver, nsIWebProgressListener, nsISupportsWeakReference)


////////////////////////////////////////////////////////////////////////////////
// nsIObserver

NS_IMETHODIMP
nsAccessibilityService::Observe(nsISupports *aSubject, const char *aTopic,
                         const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {

    // Remove observers.
    nsCOMPtr<nsIObserverService> observerService = 
      do_GetService("@mozilla.org/observer-service;1");
    if (observerService) {
      observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
    nsCOMPtr<nsIWebProgress> progress(do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID));
    if (progress)
      progress->RemoveProgressListener(static_cast<nsIWebProgressListener*>(this));

    // Application is going to be closed, shutdown accessibility and mark
    // accessibility service as shutdown to prevent calls of its methods.
    // Don't null accessibility service static member at this point to be safe
    // if someone will try to operate with it.

    NS_ASSERTION(!gIsShutdown, "Accessibility was shutdown already");

    gIsShutdown = PR_TRUE;
    nsAccessNodeWrap::ShutdownAccessibility();
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIWebProgressListener

NS_IMETHODIMP nsAccessibilityService::OnStateChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
  NS_ASSERTION(aStateFlags & STATE_IS_DOCUMENT, "Other notifications excluded");

  if (gIsShutdown || !aWebProgress ||
      (aStateFlags & (STATE_START | STATE_STOP)) == 0) {
    return NS_OK;
  }
  
  nsCAutoString name;
  aRequest->GetName(name);
  if (name.EqualsLiteral("about:blank"))
    return NS_OK;

  if (NS_FAILED(aStatus) && (aStateFlags & STATE_START))
    return NS_OK;

  if (aStateFlags & STATE_START) {
    NS_DISPATCH_RUNNABLEMETHOD_ARG2(ProcessDocLoadEvent, this, aWebProgress,
                                    nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_START)
  } else if (NS_SUCCEEDED(aStatus)) {
    NS_DISPATCH_RUNNABLEMETHOD_ARG2(ProcessDocLoadEvent, this, aWebProgress,
                                    nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE)
  } else { // Failed end load
    NS_DISPATCH_RUNNABLEMETHOD_ARG2(ProcessDocLoadEvent, this, aWebProgress,
                                    nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED)
  }

  return NS_OK;
}

// nsAccessibilityService private
void
nsAccessibilityService::ProcessDocLoadEvent(nsIWebProgress *aWebProgress,
                                            PRUint32 aEventType)
{
  if (gIsShutdown)
    return;

  nsCOMPtr<nsIDOMWindow> domWindow;
  aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
  NS_ENSURE_TRUE(domWindow,);

  if (aEventType == nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_START) {
    nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(domWindow));
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(webNav));
    NS_ENSURE_TRUE(docShell,);
    PRUint32 loadType;
    docShell->GetLoadType(&loadType);
    if (loadType == LOAD_RELOAD_NORMAL ||
        loadType == LOAD_RELOAD_BYPASS_CACHE ||
        loadType == LOAD_RELOAD_BYPASS_PROXY ||
        loadType == LOAD_RELOAD_BYPASS_PROXY_AND_CACHE) {
      aEventType = nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD;
    }
  }
      
  nsCOMPtr<nsIDOMDocument> domDoc;
  domWindow->GetDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDOMNode> docNode = do_QueryInterface(domDoc);
  NS_ENSURE_TRUE(docNode,);

  nsCOMPtr<nsIAccessible> accessible;
  GetAccessibleFor(docNode, getter_AddRefs(accessible));
  nsRefPtr<nsDocAccessible> docAcc = do_QueryObject(accessible);
  NS_ENSURE_TRUE(docAcc,);

  docAcc->FireDocLoadEvents(aEventType);
}

// nsIAccessibilityService
void
nsAccessibilityService::NotifyOfAnchorJumpTo(nsIContent *aTarget)
{
  nsIDocument *document = aTarget->GetCurrentDoc();
  nsCOMPtr<nsIDOMNode> documentNode(do_QueryInterface(document));
  if (!documentNode)
    return;

  nsCOMPtr<nsIDOMNode> targetNode(do_QueryInterface(aTarget));

  nsCOMPtr<nsIAccessible> targetAcc;
  GetAccessibleFor(targetNode, getter_AddRefs(targetAcc));

  // Getting the targetAcc above will have ensured accessible doc creation.
  // XXX Bug 561683
  nsRefPtr<nsDocAccessible> accessibleDoc =
    nsAccessNode::GetDocAccessibleFor(documentNode);
  if (!accessibleDoc)
    return;

  // If the jump target is not accessible then fire an event for nearest
  // accessible in parent chain.
  if (!targetAcc) {
        accessibleDoc->GetAccessibleInParentChain(targetNode, PR_TRUE,
                                                  getter_AddRefs(targetAcc));
        nsCOMPtr<nsIAccessNode> accNode = do_QueryInterface(targetAcc);
        accNode->GetDOMNode(getter_AddRefs(targetNode));
  }

  NS_ASSERTION(targetNode,
      "No accessible in parent chain!? Expect at least a document accessible.");
  if (!targetNode)
    return;

  // XXX note in rare cases the node could go away before we flush the queue,
  // for example if the node becomes inaccessible, or is removed from the DOM.
  accessibleDoc->FireDelayedAccessibleEvent(
                     nsIAccessibleEvent::EVENT_SCROLLING_START,
                     targetNode);
}

// nsIAccessibilityService
nsresult
nsAccessibilityService::FireAccessibleEvent(PRUint32 aEvent,
                                            nsIAccessible *aTarget)
{
  nsEventShell::FireEvent(aEvent, aTarget);
  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP nsAccessibilityService::OnProgressChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress,
  PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP nsAccessibilityService::OnLocationChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, nsIURI *location)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP nsAccessibilityService::OnStatusChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP nsAccessibilityService::OnSecurityChange(nsIWebProgress *aWebProgress,
  nsIRequest *aRequest, PRUint32 state)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}


// nsAccessibilityService private
nsresult
nsAccessibilityService::GetInfo(nsIFrame* aFrame, nsIWeakReference** aShell, nsIDOMNode** aNode)
{
  NS_ASSERTION(aFrame,"Error -- 1st argument (aFrame) is null!!");
  if (!aFrame) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIContent> content = aFrame->GetContent();
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
  if (!content || !node)
    return NS_ERROR_FAILURE;

  *aNode = node;
  NS_IF_ADDREF(*aNode);

  nsCOMPtr<nsIDocument> document = content->GetDocument();
  if (!document)
    return NS_ERROR_FAILURE;

  NS_ASSERTION(document->GetPrimaryShell(),"Error no shells!");

  // do_GetWR only works into a |nsCOMPtr| :-(
  nsCOMPtr<nsIWeakReference> weakShell =
    do_GetWeakReference(document->GetPrimaryShell());
  NS_IF_ADDREF(*aShell = weakShell);

  return NS_OK;
}

// nsAccessibilityService public
nsresult
nsAccessibilityService::GetShellFromNode(nsIDOMNode *aNode, nsIWeakReference **aWeakShell)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  aNode->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)
    return NS_ERROR_INVALID_ARG;

  // ---- Get the pres shell ----
  nsIPresShell *shell = doc->GetPrimaryShell();
  if (!shell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWeakReference> weakRef(do_GetWeakReference(shell));
  
  *aWeakShell = weakRef;
  NS_IF_ADDREF(*aWeakShell);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService

nsresult
nsAccessibilityService::CreateOuterDocAccessible(nsIDOMNode* aDOMNode, 
                                                 nsIAccessible **aOuterDocAccessible)
{
  NS_ENSURE_ARG_POINTER(aDOMNode);
  
  *aOuterDocAccessible = nsnull;

  nsCOMPtr<nsIWeakReference> outerWeakShell;
  GetShellFromNode(aDOMNode, getter_AddRefs(outerWeakShell));
  NS_ENSURE_TRUE(outerWeakShell, NS_ERROR_FAILURE);

  nsOuterDocAccessible *outerDocAccessible =
    new nsOuterDocAccessible(aDOMNode, outerWeakShell);
  NS_ENSURE_TRUE(outerDocAccessible, NS_ERROR_FAILURE);

  NS_ADDREF(*aOuterDocAccessible = outerDocAccessible);

  return NS_OK;
}

// nsAccessibilityService private
already_AddRefed<nsAccessible>
nsAccessibilityService::CreateDocOrRootAccessible(nsIPresShell *aShell,
                                                  nsIDocument* aDocument)
{
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(aDocument));
  NS_ENSURE_TRUE(rootNode, nsnull);

  nsIPresShell *presShell = aShell;
  if (!presShell) {
    presShell = aDocument->GetPrimaryShell();
  }
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(presShell));

  nsCOMPtr<nsISupports> container = aDocument->GetContainer();
  nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(container);
  NS_ENSURE_TRUE(docShell, nsnull);

  nsCOMPtr<nsIContentViewer> contentViewer;
  docShell->GetContentViewer(getter_AddRefs(contentViewer));
  NS_ENSURE_TRUE(contentViewer, nsnull); // Doc was already shut down

  PRUint32 busyFlags;
  docShell->GetBusyFlags(&busyFlags);
  if (busyFlags != nsIDocShell::BUSY_FLAGS_NONE) {
    nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(docShell));
    nsCOMPtr<nsIURI> uri;
    webNav->GetCurrentURI(getter_AddRefs(uri));
    NS_ENSURE_TRUE(uri, nsnull);

    nsCAutoString url;
    uri->GetSpec(url);
    if (url.EqualsLiteral("about:blank")) {
      // No load events for a busy about:blank -- they are often temporary.
      return nsnull;
    }
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
    do_QueryInterface(container);
  NS_ENSURE_TRUE(docShellTreeItem, nsnull);
  
  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  docShellTreeItem->GetParent(getter_AddRefs(parentTreeItem));

  nsRefPtr<nsAccessible> accessible;
  if (parentTreeItem) {
    // We only create root accessibles for the true root, othewise create a
    // doc accessible
    accessible = new nsDocAccessibleWrap(rootNode, weakShell);
  }
  else {
    accessible = new nsRootAccessibleWrap(rootNode, weakShell);
  }

  return accessible.forget();
}

 /**
   * HTML widget creation
   */
nsresult
nsAccessibilityService::CreateHTML4ButtonAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTML4ButtonAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLButtonAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLButtonAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

// nsAccessibilityService private
already_AddRefed<nsAccessible>
nsAccessibilityService::CreateHTMLAccessibleByMarkup(nsIFrame *aFrame,
                                                     nsIWeakReference *aWeakShell,
                                                     nsIDOMNode *aNode)
{
  // This method assumes we're in an HTML namespace.
  nsRefPtr<nsAccessible> accessible;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  nsIAtom *tag = content->Tag();
  if (tag == nsAccessibilityAtoms::legend) {
    accessible = new nsHTMLLegendAccessible(aNode, aWeakShell);
  }
  else if (tag == nsAccessibilityAtoms::option) {
    accessible = new nsHTMLSelectOptionAccessible(aNode, aWeakShell);
  }
  else if (tag == nsAccessibilityAtoms::optgroup) {
    accessible = new nsHTMLSelectOptGroupAccessible(aNode, aWeakShell);
  }
  else if (tag == nsAccessibilityAtoms::ul || tag == nsAccessibilityAtoms::ol ||
           tag == nsAccessibilityAtoms::dl) {
    accessible = new nsHTMLListAccessible(aNode, aWeakShell);
  }
  else if (tag == nsAccessibilityAtoms::a) {

    // Only some roles truly enjoy life as nsHTMLLinkAccessibles, for details
    // see closed bug 494807.
    nsRoleMapEntry *roleMapEntry = nsAccUtils::GetRoleMapEntry(aNode);
    if (roleMapEntry && roleMapEntry->role != nsIAccessibleRole::ROLE_NOTHING
        && roleMapEntry->role != nsIAccessibleRole::ROLE_LINK) {

      accessible = new nsHyperTextAccessibleWrap(aNode, aWeakShell);
    } else {
      accessible = new nsHTMLLinkAccessible(aNode, aWeakShell);
    }
  }
  else if (tag == nsAccessibilityAtoms::dt ||
           (tag == nsAccessibilityAtoms::li && 
            aFrame->GetType() != nsAccessibilityAtoms::blockFrame)) {
    // Normally for li, it is created by the list item frame (in nsBlockFrame)
    // which knows about the bullet frame; however, in this case the list item
    // must have been styled using display: foo
    accessible = new nsHTMLLIAccessible(aNode, aWeakShell, EmptyString());
  }
  else if (tag == nsAccessibilityAtoms::abbr ||
           tag == nsAccessibilityAtoms::acronym ||
           tag == nsAccessibilityAtoms::blockquote ||
           tag == nsAccessibilityAtoms::dd ||
           tag == nsAccessibilityAtoms::form ||
           tag == nsAccessibilityAtoms::h1 ||
           tag == nsAccessibilityAtoms::h2 ||
           tag == nsAccessibilityAtoms::h3 ||
           tag == nsAccessibilityAtoms::h4 ||
           tag == nsAccessibilityAtoms::h5 ||
           tag == nsAccessibilityAtoms::h6 ||
           tag == nsAccessibilityAtoms::q) {

    accessible = new nsHyperTextAccessibleWrap(aNode, aWeakShell);
  }
  else if (tag == nsAccessibilityAtoms::tr) {
    accessible = new nsEnumRoleAccessible(aNode, aWeakShell,
                                            nsIAccessibleRole::ROLE_ROW);
  }
  else if (nsCoreUtils::IsHTMLTableHeader(content)) {
    accessible = new nsHTMLTableHeaderCellAccessibleWrap(aNode, aWeakShell);
  }

  return accessible.forget();
}

nsresult
nsAccessibilityService::CreateHTMLLIAccessible(nsIFrame *aFrame, 
                                               nsIFrame *aBulletFrame,
                                               const nsAString& aBulletText,
                                               nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLLIAccessible(node, weakShell, aBulletText);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHyperTextAccessible(nsIFrame *aFrame, nsIAccessible **aAccessible)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);
  
  *aAccessible = new nsHyperTextAccessibleWrap(node, weakShell);
  NS_ENSURE_TRUE(*aAccessible, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aAccessible);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLCheckboxAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLCheckboxAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aPresShell, nsIAccessible **_retval)
{
  *_retval = new nsHTMLComboboxAccessible(aDOMNode, aPresShell);
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLImageAccessible(nsIFrame *aFrame,
                                                  nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIContent> content = do_QueryInterface(node);
  NS_ENSURE_STATE(content);

  nsCOMPtr<nsIHTMLDocument> htmlDoc =
    do_QueryInterface(content->GetCurrentDoc());

  nsCOMPtr<nsIDOMHTMLMapElement> mapElm;
  if (htmlDoc) {
    nsAutoString mapElmName;
    content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::usemap,
                     mapElmName);

    if (!mapElmName.IsEmpty()) {
      if (mapElmName.CharAt(0) == '#')
        mapElmName.Cut(0,1);
      mapElm = htmlDoc->GetImageMap(mapElmName);
    }
  }

  if (mapElm)
    *aAccessible = new nsHTMLImageMapAccessible(node, weakShell, mapElm);
  else
    *aAccessible = new nsHTMLImageAccessibleWrap(node, weakShell);

  if (!*aAccessible)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aAccessible);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLGenericAccessible(nsIFrame *aFrame, nsIAccessible **aAccessible)
{
  return CreateHyperTextAccessible(aFrame, aAccessible);
}

nsresult
nsAccessibilityService::CreateHTMLGroupboxAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLGroupboxAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLListboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aPresShell, nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(aDOMNode, aPresShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLMediaAccessible(nsIFrame *aFrame,
                                                  nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell),
                        getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, rv);

  *aAccessible = new nsEnumRoleAccessible(node, weakShell,
                                          nsIAccessibleRole::ROLE_GROUPING);
  NS_ENSURE_TRUE(*aAccessible, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aAccessible);
  return NS_OK;
}

/**
  * We can have several cases here. 
  *  1) a text or html embedded document where the contentDocument
  *     variable in the object element holds the content
  *  2) web content that uses a plugin, which means we will
  *     have to go to the plugin to get the accessible content
  *  3) An image or imagemap, where the image frame points back to 
  *     the object element DOMNode
  */
nsresult
nsAccessibilityService::CreateHTMLObjectFrameAccessible(nsObjectFrame *aFrame,
                                                        nsIAccessible **aAccessible)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));

  *aAccessible = nsnull;
  if (aFrame->GetRect().IsEmpty()) {
    return NS_ERROR_FAILURE;
  }
  // 1) for object elements containing either HTML or TXT documents
  nsCOMPtr<nsIDOMDocument> domDoc;
  nsCOMPtr<nsIDOMHTMLObjectElement> obj(do_QueryInterface(node));
  if (obj)
    obj->GetContentDocument(getter_AddRefs(domDoc));
  else
    domDoc = do_QueryInterface(node);
  if (domDoc)
    return CreateOuterDocAccessible(node, aAccessible);

#ifdef XP_WIN
  // 2) for plugins
  nsCOMPtr<nsIPluginInstance> pluginInstance ;
  aFrame->GetPluginInstance(*getter_AddRefs(pluginInstance));
  if (pluginInstance) {
    // Note: pluginPort will be null if windowless.
    HWND pluginPort = nsnull;
    aFrame->GetPluginPort(&pluginPort);

    *aAccessible =
      new nsHTMLWin32ObjectOwnerAccessible(node, weakShell, pluginPort);
    NS_ENSURE_TRUE(*aAccessible, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aAccessible);
    return NS_OK;
  }
#endif

  // 3) for images and imagemaps, or anything else with a child frame
  // we have the object frame, get the image frame
  nsIFrame *frame = aFrame->GetFirstChild(nsnull);
  if (frame)
    return frame->GetAccessible(aAccessible);

  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLRadioButtonAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLRadioButtonAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLSelectOptionAccessible(nsIDOMNode* aDOMNode, 
                                                         nsIAccessible *aParent, 
                                                         nsIWeakReference* aPresShell, 
                                                         nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectOptionAccessible(aDOMNode, aPresShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLTableAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTableAccessibleWrap(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLTableCellAccessible(nsIFrame *aFrame,
                                                      nsIAccessible **aAccessible)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *aAccessible = new nsHTMLTableCellAccessibleWrap(node, weakShell);
  if (!*aAccessible) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aAccessible);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLTextAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  // XXX Don't create ATK objects for these
  *_retval = new nsHTMLTextAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLTextFieldAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLTextFieldAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLLabelAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLLabelAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLHRAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLHRAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLBRAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLBRAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

nsresult
nsAccessibilityService::CreateHTMLCaptionAccessible(nsIFrame *aFrame, nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIWeakReference> weakShell;
  nsresult rv = GetInfo(aFrame, getter_AddRefs(weakShell), getter_AddRefs(node));
  if (NS_FAILED(rv))
    return rv;

  *_retval = new nsHTMLCaptionAccessible(node, weakShell);
  if (! *_retval) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*_retval);
  return NS_OK;
}

// nsAccessibilityService public
nsAccessNode*
nsAccessibilityService::GetCachedAccessNode(nsIDOMNode *aNode, 
                                            nsIWeakReference *aWeakShell)
{
  nsCOMPtr<nsIAccessibleDocument> accessibleDoc =
    nsAccessNode::GetDocAccessibleFor(aWeakShell);

  if (!accessibleDoc)
    return nsnull;

  nsRefPtr<nsDocAccessible> docAccessible =
    nsAccUtils::QueryObject<nsDocAccessible>(accessibleDoc);
  return docAccessible->GetCachedAccessNode(static_cast<void*>(aNode));
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleRetrieval

NS_IMETHODIMP
nsAccessibilityService::GetApplicationAccessible(nsIAccessible **aAccessibleApplication)
{
  NS_ENSURE_ARG_POINTER(aAccessibleApplication);

  NS_IF_ADDREF(*aAccessibleApplication = nsAccessNode::GetApplicationAccessible());
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::GetStringRole(PRUint32 aRole, nsAString& aString)
{
  if ( aRole >= NS_ARRAY_LENGTH(kRoleNames)) {
    aString.AssignLiteral("unknown");
    return NS_OK;
  }

  CopyUTF8toUTF16(kRoleNames[aRole], aString);
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::GetStringStates(PRUint32 aStates, PRUint32 aExtraStates,
                                        nsIDOMDOMStringList **aStringStates)
{
  nsAccessibleDOMStringList *stringStates = new nsAccessibleDOMStringList();
  NS_ENSURE_TRUE(stringStates, NS_ERROR_OUT_OF_MEMORY);

  //states
  if (aStates & nsIAccessibleStates::STATE_UNAVAILABLE)
    stringStates->Add(NS_LITERAL_STRING("unavailable"));
  if (aStates & nsIAccessibleStates::STATE_SELECTED)
    stringStates->Add(NS_LITERAL_STRING("selected"));
  if (aStates & nsIAccessibleStates::STATE_FOCUSED)
    stringStates->Add(NS_LITERAL_STRING("focused"));
  if (aStates & nsIAccessibleStates::STATE_PRESSED)
    stringStates->Add(NS_LITERAL_STRING("pressed"));
  if (aStates & nsIAccessibleStates::STATE_CHECKED)
    stringStates->Add(NS_LITERAL_STRING("checked"));
  if (aStates & nsIAccessibleStates::STATE_MIXED)
    stringStates->Add(NS_LITERAL_STRING("mixed"));
  if (aStates & nsIAccessibleStates::STATE_READONLY)
    stringStates->Add(NS_LITERAL_STRING("readonly"));
  if (aStates & nsIAccessibleStates::STATE_HOTTRACKED)
    stringStates->Add(NS_LITERAL_STRING("hottracked"));
  if (aStates & nsIAccessibleStates::STATE_DEFAULT)
    stringStates->Add(NS_LITERAL_STRING("default"));
  if (aStates & nsIAccessibleStates::STATE_EXPANDED)
    stringStates->Add(NS_LITERAL_STRING("expanded"));
  if (aStates & nsIAccessibleStates::STATE_COLLAPSED)
    stringStates->Add(NS_LITERAL_STRING("collapsed"));
  if (aStates & nsIAccessibleStates::STATE_BUSY)
    stringStates->Add(NS_LITERAL_STRING("busy"));
  if (aStates & nsIAccessibleStates::STATE_FLOATING)
    stringStates->Add(NS_LITERAL_STRING("floating"));
  if (aStates & nsIAccessibleStates::STATE_ANIMATED)
    stringStates->Add(NS_LITERAL_STRING("animated"));
  if (aStates & nsIAccessibleStates::STATE_INVISIBLE)
    stringStates->Add(NS_LITERAL_STRING("invisible"));
  if (aStates & nsIAccessibleStates::STATE_OFFSCREEN)
    stringStates->Add(NS_LITERAL_STRING("offscreen"));
  if (aStates & nsIAccessibleStates::STATE_SIZEABLE)
    stringStates->Add(NS_LITERAL_STRING("sizeable"));
  if (aStates & nsIAccessibleStates::STATE_MOVEABLE)
    stringStates->Add(NS_LITERAL_STRING("moveable"));
  if (aStates & nsIAccessibleStates::STATE_SELFVOICING)
    stringStates->Add(NS_LITERAL_STRING("selfvoicing"));
  if (aStates & nsIAccessibleStates::STATE_FOCUSABLE)
    stringStates->Add(NS_LITERAL_STRING("focusable"));
  if (aStates & nsIAccessibleStates::STATE_SELECTABLE)
    stringStates->Add(NS_LITERAL_STRING("selectable"));
  if (aStates & nsIAccessibleStates::STATE_LINKED)
    stringStates->Add(NS_LITERAL_STRING("linked"));
  if (aStates & nsIAccessibleStates::STATE_TRAVERSED)
    stringStates->Add(NS_LITERAL_STRING("traversed"));
  if (aStates & nsIAccessibleStates::STATE_MULTISELECTABLE)
    stringStates->Add(NS_LITERAL_STRING("multiselectable"));
  if (aStates & nsIAccessibleStates::STATE_EXTSELECTABLE)
    stringStates->Add(NS_LITERAL_STRING("extselectable"));
  if (aStates & nsIAccessibleStates::STATE_PROTECTED)
    stringStates->Add(NS_LITERAL_STRING("protected"));
  if (aStates & nsIAccessibleStates::STATE_HASPOPUP)
    stringStates->Add(NS_LITERAL_STRING("haspopup"));
  if (aStates & nsIAccessibleStates::STATE_REQUIRED)
    stringStates->Add(NS_LITERAL_STRING("required"));
  if (aStates & nsIAccessibleStates::STATE_IMPORTANT)
    stringStates->Add(NS_LITERAL_STRING("important"));
  if (aStates & nsIAccessibleStates::STATE_INVALID)
    stringStates->Add(NS_LITERAL_STRING("invalid"));
  if (aStates & nsIAccessibleStates::STATE_CHECKABLE)
    stringStates->Add(NS_LITERAL_STRING("checkable"));

  //extraStates
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_SUPPORTS_AUTOCOMPLETION)
    stringStates->Add(NS_LITERAL_STRING("autocompletion"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_DEFUNCT)
    stringStates->Add(NS_LITERAL_STRING("defunct"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_SELECTABLE_TEXT)
    stringStates->Add(NS_LITERAL_STRING("selectable text"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_EDITABLE)
    stringStates->Add(NS_LITERAL_STRING("editable"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_ACTIVE)
    stringStates->Add(NS_LITERAL_STRING("active"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_MODAL)
    stringStates->Add(NS_LITERAL_STRING("modal"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_MULTI_LINE)
    stringStates->Add(NS_LITERAL_STRING("multi line"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_HORIZONTAL)
    stringStates->Add(NS_LITERAL_STRING("horizontal"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_OPAQUE)
    stringStates->Add(NS_LITERAL_STRING("opaque"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_SINGLE_LINE)
    stringStates->Add(NS_LITERAL_STRING("single line"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_TRANSIENT)
    stringStates->Add(NS_LITERAL_STRING("transient"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_VERTICAL)
    stringStates->Add(NS_LITERAL_STRING("vertical"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_STALE)
    stringStates->Add(NS_LITERAL_STRING("stale"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_ENABLED)
    stringStates->Add(NS_LITERAL_STRING("enabled"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_SENSITIVE)
    stringStates->Add(NS_LITERAL_STRING("sensitive"));
  if (aExtraStates & nsIAccessibleStates::EXT_STATE_EXPANDABLE)
    stringStates->Add(NS_LITERAL_STRING("expandable"));

  //unknown states
  PRUint32 stringStatesLength = 0;

  stringStates->GetLength(&stringStatesLength);
  if (!stringStatesLength)
    stringStates->Add(NS_LITERAL_STRING("unknown"));

  NS_ADDREF(*aStringStates = stringStates);
  return NS_OK;
}

// nsIAccessibleRetrieval::getStringEventType()
NS_IMETHODIMP
nsAccessibilityService::GetStringEventType(PRUint32 aEventType,
                                           nsAString& aString)
{
  NS_ASSERTION(nsIAccessibleEvent::EVENT_LAST_ENTRY == NS_ARRAY_LENGTH(kEventTypeNames),
               "nsIAccessibleEvent constants are out of sync to kEventTypeNames");

  if (aEventType >= NS_ARRAY_LENGTH(kEventTypeNames)) {
    aString.AssignLiteral("unknown");
    return NS_OK;
  }

  CopyUTF8toUTF16(kEventTypeNames[aEventType], aString);
  return NS_OK;
}

// nsIAccessibleRetrieval::getStringRelationType()
NS_IMETHODIMP
nsAccessibilityService::GetStringRelationType(PRUint32 aRelationType,
                                              nsAString& aString)
{
  if (aRelationType >= NS_ARRAY_LENGTH(kRelationTypeNames)) {
    aString.AssignLiteral("unknown");
    return NS_OK;
  }

  CopyUTF8toUTF16(kRelationTypeNames[aRelationType], aString);
  return NS_OK;
}


/**
  * GetAccessibleFor - get an nsIAccessible from a DOM node
  */

NS_IMETHODIMP
nsAccessibilityService::GetAccessibleFor(nsIDOMNode *aNode,
                                         nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  NS_ENSURE_ARG(aNode);

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  nsCOMPtr<nsIDocument> doc;
  if (content) {
    doc = content->GetDocument();
  }
  else {// Could be document node
    doc = do_QueryInterface(aNode);
  }
  if (!doc)
    return NS_ERROR_FAILURE;

  // We use presentation shell #0 because we assume that is presentation of
  // given node window.
  nsIPresShell *presShell = doc->GetPrimaryShell();

  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(presShell));
  nsRefPtr<nsAccessible> accessible =
    GetAccessible(aNode, presShell, weakShell);

  if (accessible)
    CallQueryInterface(accessible.get(), aAccessible);
  
  return NS_OK;
}

NS_IMETHODIMP
nsAccessibilityService::GetAttachedAccessibleFor(nsIDOMNode *aNode,
                                                 nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG(aNode);
  NS_ENSURE_ARG_POINTER(aAccessible);

  *aAccessible = nsnull;

  nsCOMPtr<nsIDOMNode> relevantNode;
  nsresult rv = GetRelevantContentNodeFor(aNode, getter_AddRefs(relevantNode));
  NS_ENSURE_SUCCESS(rv, rv);

  if (relevantNode != aNode)
    return NS_OK;

  return GetAccessibleFor(aNode, aAccessible);
}

NS_IMETHODIMP
nsAccessibilityService::GetAccessibleInShell(nsIDOMNode *aNode, 
                                             nsIPresShell *aPresShell,
                                             nsIAccessible **aAccessible) 
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;

  NS_ENSURE_ARG(aNode);
  NS_ENSURE_ARG(aPresShell);

  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(aPresShell));
  nsRefPtr<nsAccessible> accessible =
    GetAccessible(aNode, aPresShell, weakShell);

  if (accessible)
    CallQueryInterface(accessible.get(), aAccessible);
  
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService public

already_AddRefed<nsAccessible>
nsAccessibilityService::GetAccessibleInWeakShell(nsIDOMNode *aNode, 
                                                 nsIWeakReference *aWeakShell) 
{
  if (!aNode || !aWeakShell)
    return nsnull;

  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(aWeakShell));
  return GetAccessible(aNode, presShell, aWeakShell);
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService private

PRBool
nsAccessibilityService::InitAccessible(nsAccessible *aAccessible,
                                       nsRoleMapEntry *aRoleMapEntry)
{
  if (!aAccessible)
    return PR_FALSE;

  nsresult rv = aAccessible->Init(); // Add to cache, etc.
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to initialize an accessible!");

    aAccessible->Shutdown();
    return PR_FALSE;
  }

  NS_ASSERTION(aAccessible->IsInCache(),
               "Initialized accessible not in the cache!");

  aAccessible->SetRoleMapEntry(aRoleMapEntry);
  return PR_TRUE;
}

static PRBool HasRelatedContent(nsIContent *aContent)
{
  nsAutoString id;
  if (!aContent || !nsCoreUtils::GetID(aContent, id) || id.IsEmpty()) {
    return PR_FALSE;
  }

  nsIAtom *relationAttrs[] = {nsAccessibilityAtoms::aria_labelledby,
                              nsAccessibilityAtoms::aria_describedby,
                              nsAccessibilityAtoms::aria_owns,
                              nsAccessibilityAtoms::aria_controls,
                              nsAccessibilityAtoms::aria_flowto};
  if (nsCoreUtils::FindNeighbourPointingToNode(aContent, relationAttrs,
                                               NS_ARRAY_LENGTH(relationAttrs))) {
    return PR_TRUE;
  }

  nsIContent *ancestorContent = aContent;
  while ((ancestorContent = ancestorContent->GetParent()) != nsnull) {
    if (ancestorContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_activedescendant)) {
        // ancestor has activedescendant property, this content could be active
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

// nsAccessibilityService public
already_AddRefed<nsAccessible>
nsAccessibilityService::GetAccessible(nsIDOMNode *aNode,
                                      nsIPresShell *aPresShell,
                                      nsIWeakReference *aWeakShell,
                                      PRBool *aIsHidden)
{
  if (!aPresShell || !aWeakShell || gIsShutdown)
    return nsnull;

  NS_ASSERTION(aNode, "GetAccessible() called with no node.");

  if (aIsHidden)
    *aIsHidden = PR_FALSE;

#ifdef DEBUG_A11Y
  // Please leave this in for now, it's a convenient debugging method
  nsAutoString name;
  aNode->GetLocalName(name);
  if (name.LowerCaseEqualsLiteral("h1")) 
    printf("## aaronl debugging tag name\n");

  nsAutoString attrib;
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aNode));
  if (element) {
    element->GetAttribute(NS_LITERAL_STRING("type"), attrib);
    if (attrib.EqualsLiteral("statusbarpanel"))
      printf("## aaronl debugging attribute\n");
  }
#endif

  // Check to see if we already have an accessible for this node in the cache.
  nsAccessNode* cachedAccessNode = GetCachedAccessNode(aNode, aWeakShell);
  if (cachedAccessNode) {
    // XXX: the cache might contain the access node for the DOM node that is not
    // accessible because of wrong cache update. In this case try to create new
    // accessible.
    nsRefPtr<nsAccessible> cachedAccessible =
      nsAccUtils::QueryObject<nsAccessible>(cachedAccessNode);

    if (cachedAccessible)
      return cachedAccessible.forget();
  }

  // No cache entry, so we must create the accessible.
  nsRefPtr<nsAccessible> newAcc;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (!content) {
    // This happens when we're on the document node, which will not QI to an
    // nsIContent.
    nsCOMPtr<nsIDocument> nodeIsDoc = do_QueryInterface(aNode);
    if (!nodeIsDoc) // No content, and not doc node.
      return nsnull;

#ifdef DEBUG
    // XXX: remove me if you don't see an assertion.
    nsCOMPtr<nsIAccessibleDocument> accessibleDoc =
      nsAccessNode::GetDocAccessibleFor(nodeIsDoc);
    NS_ASSERTION(!accessibleDoc,
                 "Trying to create already cached accessible document!");
#endif

    newAcc = CreateDocOrRootAccessible(aPresShell, nodeIsDoc);
    if (InitAccessible(newAcc, nsAccUtils::GetRoleMapEntry(aNode)))
      return newAcc.forget();
    return nsnull;
  }

  // We have a content node.
  if (!content->IsInDoc()) {
    NS_ERROR("Creating accessible for node with no document");
    return nsnull;
  }

  if (content->GetOwnerDoc() != aPresShell->GetDocument()) {
    NS_ERROR("Creating accessible for wrong pres shell");
    return nsnull;
  }

  // Frames can be deallocated when we flush layout, or when we call into code
  // that can flush layout, either directly, or via DOM manipulation, or some
  // CSS styles like :hover. We use the weak frame checks to avoid calling
  // methods on a dead frame pointer.
  nsWeakFrame weakFrame = content->GetPrimaryFrame();

  // Check frame to see if it is hidden.
  if (!weakFrame.GetFrame() ||
      !weakFrame.GetFrame()->GetStyleVisibility()->IsVisible()) {
    if (aIsHidden)
      *aIsHidden = PR_TRUE;

    return nsnull;
  }

  if (weakFrame.GetFrame()->GetContent() != content) {
    // Not the main content for this frame. This happens because <area>
    // elements return the image frame as their primary frame. The main content
    // for the image frame is the image content. If the frame is not an image
    // frame or the node is not an area element then null is returned.
    return GetAreaAccessible(weakFrame.GetFrame(), aNode, aWeakShell);
  }

  // Attempt to create an accessible based on what we know.
  if (content->IsNodeOfType(nsINode::eTEXT)) {
    // --- Create HTML for visible text frames ---
    nsIFrame* f = weakFrame.GetFrame();
    if (f && f->IsEmpty()) {
      nsAutoString renderedWhitespace;
      f->GetRenderedText(&renderedWhitespace, nsnull, nsnull, 0, 1);
      if (renderedWhitespace.IsEmpty()) {
        // Really empty -- nothing is rendered
        if (aIsHidden)
          *aIsHidden = PR_TRUE;

        return nsnull;
      }
    }
    if (weakFrame.IsAlive()) {
      nsCOMPtr<nsIAccessible> newAccessible;
      weakFrame.GetFrame()->GetAccessible(getter_AddRefs(newAccessible));
      if (newAccessible) {
        newAcc = nsAccUtils::QueryObject<nsAccessible>(newAccessible);
        if (InitAccessible(newAcc, nsnull))
          return newAcc.forget();
        return nsnull;
      }
    }

    return nsnull;
  }

  PRBool isHTML = content->IsHTML();
  if (isHTML && content->Tag() == nsAccessibilityAtoms::map) {
    // Create hyper text accessible for HTML map if it is used to group links
    // (see http://www.w3.org/TR/WCAG10-HTML-TECHS/#group-bypass). If the HTML
    // map doesn't have 'name' attribute (or has empty name attribute) then we
    // suppose it is used for links grouping. Otherwise we think it is used in
    // conjuction with HTML image element and in this case we don't create any
    // accessible for it and don't walk into it. The accessibles for HTML area
    // (nsHTMLAreaAccessible) the map contains are attached as children of the
    // appropriate accessible for HTML image (nsHTMLImageAccessible).
    nsAutoString name;
    content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::name, name);
    if (!name.IsEmpty()) {
      if (aIsHidden)
        *aIsHidden = PR_TRUE;

      return nsnull;
    }

    newAcc = new nsHyperTextAccessibleWrap(aNode, aWeakShell);
    if (InitAccessible(newAcc, nsAccUtils::GetRoleMapEntry(aNode)))
      return newAcc.forget();
    return nsnull;
  }

  nsRoleMapEntry *roleMapEntry = nsAccUtils::GetRoleMapEntry(aNode);
  if (roleMapEntry && !nsCRT::strcmp(roleMapEntry->roleString, "presentation") &&
      !content->IsFocusable()) { // For presentation only
    // Only create accessible for role of "presentation" if it is focusable --
    // in that case we need an accessible in case it gets focused, we
    // don't want focus ever to be 'lost'
    return nsnull;
  }

  if (weakFrame.IsAlive() && !newAcc && isHTML) {  // HTML accessibles
    PRBool tryTagNameOrFrame = PR_TRUE;

    nsIAtom *frameType = weakFrame.GetFrame()->GetType();

    PRBool partOfHTMLTable =
      frameType == nsAccessibilityAtoms::tableCaptionFrame ||
      frameType == nsAccessibilityAtoms::tableCellFrame ||
      frameType == nsAccessibilityAtoms::tableRowGroupFrame ||
      frameType == nsAccessibilityAtoms::tableRowFrame;

    if (partOfHTMLTable) {
      // Table-related frames don't get table-related roles
      // unless they are inside a table, but they may still get generic
      // accessibles
      nsIContent *tableContent = content;
      while ((tableContent = tableContent->GetParent()) != nsnull) {
        nsIFrame *tableFrame = tableContent->GetPrimaryFrame();
        if (!tableFrame)
          continue;

        if (tableFrame->GetType() == nsAccessibilityAtoms::tableOuterFrame) {
          nsCOMPtr<nsIDOMNode> tableNode(do_QueryInterface(tableContent));
          nsRefPtr<nsAccessible> tableAccessible =
            GetAccessibleInWeakShell(tableNode, aWeakShell);

          if (tableAccessible) {
            if (!roleMapEntry) {
              PRUint32 role = nsAccUtils::Role(tableAccessible);
              if (role != nsIAccessibleRole::ROLE_TABLE &&
                  role != nsIAccessibleRole::ROLE_TREE_TABLE) {
                // No ARIA role and not in table: override role. For example,
                // <table role="label"><td>content</td></table>
                roleMapEntry = &nsARIAMap::gEmptyRoleMap;
              }
            }

            break;
          }

#ifdef DEBUG
          nsRoleMapEntry *tableRoleMapEntry =
            nsAccUtils::GetRoleMapEntry(tableNode);
          NS_ASSERTION(tableRoleMapEntry &&
                       !nsCRT::strcmp(tableRoleMapEntry->roleString, "presentation"),
                       "No accessible for parent table and it didn't have role of presentation");
#endif

          if (!roleMapEntry && !content->IsFocusable()) {
            // Table-related descendants of presentation table are also
            // presentation if they aren't focusable and have not explicit ARIA
            // role (don't create accessibles for them unless they need to fire
            // focus events).
            return nsnull;
          }

          // otherwise create ARIA based accessible.
          tryTagNameOrFrame = PR_FALSE;
          break;
        }

        if (tableContent->Tag() == nsAccessibilityAtoms::table) {
          // Stop before we are fooled by any additional table ancestors
          // This table cell frameis part of a separate ancestor table.
          tryTagNameOrFrame = PR_FALSE;
          break;
        }
      }

      if (!tableContent)
        tryTagNameOrFrame = PR_FALSE;
    }

    if (roleMapEntry) {
      // Create ARIA grid/treegrid accessibles if node is not of a child or
      // valid child of HTML table and is not a HTML table.
      if ((!partOfHTMLTable || !tryTagNameOrFrame) &&
          frameType != nsAccessibilityAtoms::tableOuterFrame) {

        if (roleMapEntry->role == nsIAccessibleRole::ROLE_TABLE ||
            roleMapEntry->role == nsIAccessibleRole::ROLE_TREE_TABLE) {
          newAcc = new nsARIAGridAccessibleWrap(aNode, aWeakShell);

        } else if (roleMapEntry->role == nsIAccessibleRole::ROLE_GRID_CELL ||
            roleMapEntry->role == nsIAccessibleRole::ROLE_ROWHEADER ||
            roleMapEntry->role == nsIAccessibleRole::ROLE_COLUMNHEADER) {
          newAcc = new nsARIAGridCellAccessibleWrap(aNode, aWeakShell);
        }
      }
    }

    if (!newAcc && tryTagNameOrFrame) {
      // Prefer to use markup (mostly tag name, perhaps attributes) to
      // decide if and what kind of accessible to create.
      // The method creates accessibles for table related content too therefore
      // we do not call it if accessibles for table related content are
      // prevented above.
      newAcc = CreateHTMLAccessibleByMarkup(weakFrame.GetFrame(), aWeakShell,
                                            aNode);

      if (!newAcc) {
        // Do not create accessible object subtrees for non-rendered table
        // captions. This could not be done in
        // nsTableCaptionFrame::GetAccessible() because the descendants of
        // the table caption would still be created. By setting
        // *aIsHidden = PR_TRUE we ensure that no descendant accessibles are
        // created.
        nsIFrame* f = weakFrame.GetFrame();
        if (!f) {
          f = aPresShell->GetRealPrimaryFrameFor(content);
        }
        if (f->GetType() == nsAccessibilityAtoms::tableCaptionFrame &&
           f->GetRect().IsEmpty()) {
          // XXX This is not the ideal place for this code, but right now there
          // is no better place:
          if (aIsHidden)
            *aIsHidden = PR_TRUE;

          return nsnull;
        }

        // Try using frame to do it.
        nsCOMPtr<nsIAccessible> newAccessible;
        f->GetAccessible(getter_AddRefs(newAccessible));
        newAcc = nsAccUtils::QueryObject<nsAccessible>(newAccessible);
      }
    }
  }

  if (!newAcc) {
    // Elements may implement nsIAccessibleProvider via XBL. This allows them to
    // say what kind of accessible to create.
    newAcc = CreateAccessibleByType(aNode, aWeakShell);
  }

  if (!newAcc) {
    // Create generic accessibles for SVG and MathML nodes.
    if (content->GetNameSpaceID() == kNameSpaceID_SVG &&
        content->Tag() == nsAccessibilityAtoms::svg) {
      newAcc = new nsEnumRoleAccessible(aNode, aWeakShell,
                                        nsIAccessibleRole::ROLE_DIAGRAM);
    }
    else if (content->GetNameSpaceID() == kNameSpaceID_MathML &&
             content->Tag() == nsAccessibilityAtoms::math) {
      newAcc = new nsEnumRoleAccessible(aNode, aWeakShell,
                                        nsIAccessibleRole::ROLE_EQUATION);
    }
  }

  if (!newAcc) {
    newAcc = CreateAccessibleForDeckChild(weakFrame.GetFrame(), aNode,
                                          aWeakShell);
  }

  // If no accessible, see if we need to create a generic accessible because
  // of some property that makes this object interesting
  // We don't do this for <body>, <html>, <window>, <dialog> etc. which 
  // correspond to the doc accessible and will be created in any case
  if (!newAcc && content->Tag() != nsAccessibilityAtoms::body && content->GetParent() && 
      ((weakFrame.GetFrame() && weakFrame.GetFrame()->IsFocusable()) ||
       (isHTML && nsCoreUtils::HasClickListener(content)) ||
       HasUniversalAriaProperty(content) || roleMapEntry ||
       HasRelatedContent(content) || nsCoreUtils::IsXLink(content))) {
    // This content is focusable or has an interesting dynamic content accessibility property.
    // If it's interesting we need it in the accessibility hierarchy so that events or
    // other accessibles can point to it, or so that it can hold a state, etc.
    if (isHTML) {
      // Interesting HTML container which may have selectable text and/or embedded objects
      newAcc = new nsHyperTextAccessibleWrap(aNode, aWeakShell);
    }
    else {  // XUL, SVG, MathML etc.
      // Interesting generic non-HTML container
      newAcc = new nsAccessibleWrap(aNode, aWeakShell);
    }
  }

  if (InitAccessible(newAcc, roleMapEntry))
    return newAcc.forget();
  return nsnull;
}

PRBool
nsAccessibilityService::HasUniversalAriaProperty(nsIContent *aContent)
{
  // ARIA attributes that take token values (NMTOKEN, bool) are special cased
  // because of special value "undefined" (see HasDefinedARIAToken).
  return nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_atomic) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_busy) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_controls) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_describedby) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_disabled) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_dropeffect) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_flowto) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_grabbed) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_haspopup) ||
         // purposely ignore aria-hidden; since we use gecko for detecting this anyways
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_invalid) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_label) ||
         aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_labelledby) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_live) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_owns) ||
         nsAccUtils::HasDefinedARIAToken(aContent, nsAccessibilityAtoms::aria_relevant);
}

// nsIAccessibleRetrieval
NS_IMETHODIMP
nsAccessibilityService::GetRelevantContentNodeFor(nsIDOMNode *aNode,
                                                  nsIDOMNode **aRelevantNode)
{
  // The method returns node that is relevant for attached accessible check.
  // Sometimes element that is XBL widget hasn't accessible children in
  // anonymous content. This method check whether given node can be accessible
  // by looking through all nested bindings that given node is anonymous for. If
  // there is XBL widget that deniedes to be accessible for given node then the
  // method returns that XBL widget otherwise it returns given node.

  // For example, the algorithm allows us to handle following cases:
  // 1. xul:dialog element has buttons (like 'ok' and 'cancel') in anonymous
  // content. When node is dialog's button then we dialog's button since button
  // of xul:dialog is accessible anonymous node.
  // 2. xul:texbox has html:input in anonymous content. When given node is
  // html:input elmement then we return xul:textbox since xul:textbox doesn't
  // allow accessible nodes in anonymous content.
  // 3. xforms:input that is hosted in xul document contains xul:textbox
  // element. When given node is html:input or xul:textbox then we return
  // xforms:input element since xforms:input hasn't accessible anonymous
  // children.

  NS_ENSURE_ARG(aNode);
  NS_ENSURE_ARG_POINTER(aRelevantNode);

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (content) {
    // Build stack of binding parents so we can walk it in reverse.
    nsIContent *bindingParent;
    nsCOMArray<nsIContent> bindingsStack;

    for (bindingParent = content->GetBindingParent(); bindingParent != nsnull &&
         bindingParent != bindingParent->GetBindingParent();
         bindingParent = bindingParent->GetBindingParent()) {
      bindingsStack.AppendObject(bindingParent);
    }

    PRInt32 bindingsCount = bindingsStack.Count();
    for (PRInt32 index = bindingsCount - 1; index >= 0 ; index--) {
      bindingParent = bindingsStack[index];
      nsCOMPtr<nsIDOMNode> bindingNode(do_QueryInterface(bindingParent));
      if (bindingNode) {
        // Try to get an accessible by type since XBL widget can be accessible
        // only if it implements nsIAccessibleProvider interface.
        nsCOMPtr<nsIWeakReference> weakShell;
        GetShellFromNode(bindingNode, getter_AddRefs(weakShell));

        // XXX: it's a hack we should try the cache before, otherwise to cache
        // the accessible.
        nsRefPtr<nsAccessible> accessible =
          CreateAccessibleByType(bindingNode, weakShell);

        if (accessible) {
          if (!accessible->GetAllowsAnonChildAccessibles()) {
            NS_ADDREF(*aRelevantNode = bindingNode);
            return NS_OK;
          }
        }
      }
    }
  }

  NS_ADDREF(*aRelevantNode = aNode);
  return NS_OK;
}

already_AddRefed<nsAccessible>
nsAccessibilityService::GetAreaAccessible(nsIFrame *aImageFrame,
                                          nsIDOMNode *aAreaNode,
                                          nsIWeakReference *aWeakShell)
{
  // Check if frame is an image frame, and content is <area>.
  nsIImageFrame *imageFrame = do_QueryFrame(aImageFrame);
  if (!imageFrame)
    return nsnull;

  nsCOMPtr<nsIDOMHTMLAreaElement> areaElmt = do_QueryInterface(aAreaNode);
  if (!areaElmt)
    return nsnull;

  // Try to get image map accessible from the global cache or create it
  // if failed.
  nsRefPtr<nsAccessible> imageAcc;

  nsCOMPtr<nsIDOMNode> imageNode(do_QueryInterface(aImageFrame->GetContent()));
  nsAccessNode *cachedImgAcc = GetCachedAccessNode(imageNode, aWeakShell);
  if (cachedImgAcc)
    imageAcc = nsAccUtils::QueryObject<nsAccessible>(cachedImgAcc);

  if (!imageAcc) {
    nsCOMPtr<nsIAccessible> imageAccessible;
    CreateHTMLImageAccessible(aImageFrame,
                              getter_AddRefs(imageAccessible));

    imageAcc = nsAccUtils::QueryObject<nsAccessible>(imageAccessible);
    if (!InitAccessible(imageAcc, nsnull))
      return nsnull;
  }

  // Make sure <area> accessible children of the image map are cached so
  // that they should be available in global cache.
  imageAcc->EnsureChildren();

  nsAccessNode *cachedAreaAcc = GetCachedAccessNode(aAreaNode, aWeakShell);
  if (!cachedAreaAcc)
    return nsnull;

  nsRefPtr<nsAccessible> areaAcc =
    nsAccUtils::QueryObject<nsAccessible>(cachedAreaAcc);
  return areaAcc.forget();
}

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateAccessibleByType(nsIDOMNode *aNode,
                                               nsIWeakReference *aWeakShell)
{
  nsCOMPtr<nsIAccessibleProvider> accessibleProvider(do_QueryInterface(aNode));
  if (!accessibleProvider)
    return nsnull;

  PRInt32 type;
  nsresult rv = accessibleProvider->GetAccessibleType(&type);
  if (NS_FAILED(rv))
    return nsnull;

  nsRefPtr<nsAccessible> accessible;
  if (type == nsIAccessibleProvider::OuterDoc) {
    accessible = new nsOuterDocAccessible(aNode, aWeakShell);
    return accessible.forget();
  }

  switch (type)
  {
#ifdef MOZ_XUL
    case nsIAccessibleProvider::NoAccessible:
      return nsnull;

    // XUL controls
    case nsIAccessibleProvider::XULAlert:
      accessible = new nsXULAlertAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULButton:
      accessible = new nsXULButtonAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULCheckbox:
      accessible = new nsXULCheckboxAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULColorPicker:
      accessible = new nsXULColorPickerAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULColorPickerTile:
      accessible = new nsXULColorPickerTileAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULCombobox:
      accessible = new nsXULComboboxAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULDropmarker:
      accessible = new nsXULDropmarkerAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULGroupbox:
      accessible = new nsXULGroupboxAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULImage:
    {
      // Don't include nameless images in accessible tree
      nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(aNode));
      if (!elt)
        return nsnull;

      PRBool hasTextEquivalent;
      // Prefer value over tooltiptext
      elt->HasAttribute(NS_LITERAL_STRING("tooltiptext"), &hasTextEquivalent);
      if (!hasTextEquivalent)
        return nsnull;

      accessible = new nsHTMLImageAccessibleWrap(aNode, aWeakShell);
      break;
    }
    case nsIAccessibleProvider::XULLink:
      accessible = new nsXULLinkAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULListbox:
      accessible = new nsXULListboxAccessibleWrap(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULListCell:
      accessible = new nsXULListCellAccessibleWrap(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULListHead:
      accessible = new nsXULColumnsAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULListHeader:
      accessible = new nsXULColumnItemAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULListitem:
      accessible = new nsXULListitemAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULMenubar:
      accessible = new nsXULMenubarAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULMenuitem:
      accessible = new nsXULMenuitemAccessibleWrap(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULMenupopup:
    {
#ifdef MOZ_ACCESSIBILITY_ATK
      // ATK considers this node to be redundant when within menubars, and it makes menu
      // navigation with assistive technologies more difficult
      // XXX In the future we will should this for consistency across the nsIAccessible
      // implementations on each platform for a consistent scripting environment, but
      // then strip out redundant accessibles in the nsAccessibleWrap class for each platform.
      nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
      if (content) {
        nsIContent *parent = content->GetParent();
        if (parent && parent->NodeInfo()->Equals(nsAccessibilityAtoms::menu, kNameSpaceID_XUL)) {
          return nsnull;
        }
      }
#endif
      accessible = new nsXULMenupopupAccessible(aNode, aWeakShell);
      break;
    }
    case nsIAccessibleProvider::XULMenuSeparator:
      accessible = new nsXULMenuSeparatorAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULPane:
      accessible = new nsEnumRoleAccessible(aNode, aWeakShell,
                                            nsIAccessibleRole::ROLE_PANE);
      break;
    case nsIAccessibleProvider::XULProgressMeter:
      accessible = new nsXULProgressMeterAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULStatusBar:
      accessible = new nsXULStatusBarAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULScale:
      accessible = new nsXULSliderAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULRadioButton:
      accessible = new nsXULRadioButtonAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULRadioGroup:
      accessible = new nsXULRadioGroupAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULTab:
      accessible = new nsXULTabAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULTabBox:
      accessible = new nsXULTabBoxAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULTabs:
      accessible = new nsXULTabsAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULText:
      accessible = new nsXULTextAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULTextBox:
      accessible = new nsXULTextFieldAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULThumb:
      accessible = new nsXULThumbAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULTree:
      return CreateAccessibleForXULTree(aNode, aWeakShell);

    case nsIAccessibleProvider::XULTreeColumns:
      accessible = new nsXULTreeColumnsAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULTreeColumnItem:
      accessible = new nsXULColumnItemAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULToolbar:
      accessible = new nsXULToolbarAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULToolbarSeparator:
      accessible = new nsXULToolbarSeparatorAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULTooltip:
      accessible = new nsXULTooltipAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XULToolbarButton:
      accessible = new nsXULToolbarButtonAccessible(aNode, aWeakShell);
      break;
#endif // MOZ_XUL

#ifndef DISABLE_XFORMS_HOOKS
    // XForms elements
    case nsIAccessibleProvider::XFormsContainer:
      accessible = new nsXFormsContainerAccessible(aNode, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsLabel:
      accessible = new nsXFormsLabelAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsOuput:
      accessible = new nsXFormsOutputAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsTrigger:
      accessible = new nsXFormsTriggerAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsInput:
      accessible = new nsXFormsInputAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsInputBoolean:
      accessible = new nsXFormsInputBooleanAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsInputDate:
      accessible = new nsXFormsInputDateAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsSecret:
      accessible = new nsXFormsSecretAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsSliderRange:
      accessible = new nsXFormsRangeAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsSelect:
      accessible = new nsXFormsSelectAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsChoices:
      accessible = new nsXFormsChoicesAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsSelectFull:
      accessible = new nsXFormsSelectFullAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsItemCheckgroup:
      accessible = new nsXFormsItemCheckgroupAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsItemRadiogroup:
      accessible = new nsXFormsItemRadiogroupAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsSelectCombobox:
      accessible = new nsXFormsSelectComboboxAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsItemCombobox:
      accessible = new nsXFormsItemComboboxAccessible(aNode, aWeakShell);
      break;

    case nsIAccessibleProvider::XFormsDropmarkerWidget:
      accessible = new nsXFormsDropmarkerWidgetAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsCalendarWidget:
      accessible = new nsXFormsCalendarWidgetAccessible(aNode, aWeakShell);
      break;
    case nsIAccessibleProvider::XFormsComboboxPopupWidget:
      accessible = new nsXFormsComboboxPopupWidgetAccessible(aNode, aWeakShell);
      break;
#endif

    default:
      return nsnull;
  }

  return accessible.forget();
}

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService (DON'T put methods here)

nsresult
nsAccessibilityService::AddNativeRootAccessible(void *aAtkAccessible,
                                                nsIAccessible **aRootAccessible)
{
#ifdef MOZ_ACCESSIBILITY_ATK
  nsNativeRootAccessibleWrap* rootAccWrap =
    new nsNativeRootAccessibleWrap((AtkObject*)aAtkAccessible);

  *aRootAccessible = static_cast<nsIAccessible*>(rootAccWrap);
  NS_ADDREF(*aRootAccessible);

  nsApplicationAccessible *applicationAcc =
    nsAccessNode::GetApplicationAccessible();
  NS_ENSURE_STATE(applicationAcc);

  applicationAcc->AddRootAccessible(*aRootAccessible);

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

nsresult
nsAccessibilityService::RemoveNativeRootAccessible(nsIAccessible *aRootAccessible)
{
#ifdef MOZ_ACCESSIBILITY_ATK
  void* atkAccessible;
  aRootAccessible->GetNativeInterface(&atkAccessible);

  nsApplicationAccessible *applicationAcc =
    nsAccessNode::GetApplicationAccessible();
  NS_ENSURE_STATE(applicationAcc);

  applicationAcc->RemoveRootAccessible(aRootAccessible);

  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

// Called from layout when the frame tree owned by a node changes significantly
nsresult
nsAccessibilityService::InvalidateSubtreeFor(nsIPresShell *aShell,
                                             nsIContent *aChangeContent,
                                             PRUint32 aChangeType)
{
  NS_ASSERTION(aChangeType == nsIAccessibilityService::FRAME_SIGNIFICANT_CHANGE ||
               aChangeType == nsIAccessibilityService::FRAME_SHOW ||
               aChangeType == nsIAccessibilityService::FRAME_HIDE ||
               aChangeType == nsIAccessibilityService::NODE_SIGNIFICANT_CHANGE ||
               aChangeType == nsIAccessibilityService::NODE_APPEND ||
               aChangeType == nsIAccessibilityService::NODE_REMOVE,
               "Incorrect aEvent passed in");

  NS_ENSURE_ARG_POINTER(aShell);

  nsCOMPtr<nsIAccessibleDocument> accessibleDoc =
    nsAccessNode::GetDocAccessibleFor(aShell->GetDocument());
  nsRefPtr<nsDocAccessible> docAcc = do_QueryObject(accessibleDoc);
  if (docAcc)
    docAcc->InvalidateCacheSubtree(aChangeContent, aChangeType);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// NS_GetAccessibilityService
////////////////////////////////////////////////////////////////////////////////

/**
 * Return accessibility service; creating one if necessary.
 */
nsresult
NS_GetAccessibilityService(nsIAccessibilityService** aResult)
{
   NS_ENSURE_TRUE(aResult, NS_ERROR_NULL_POINTER);
   *aResult = nsnull;
 
  if (!nsAccessibilityService::gAccessibilityService) {
    nsAccessibilityService::gAccessibilityService = new nsAccessibilityService();
    NS_ENSURE_TRUE(nsAccessibilityService::gAccessibilityService, NS_ERROR_OUT_OF_MEMORY);
 
    nsAccessibilityService::gIsShutdown = PR_FALSE;
   }
 
  NS_ADDREF(*aResult = nsAccessibilityService::gAccessibilityService);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsAccessibilityService private (DON'T put methods here)

already_AddRefed<nsAccessible>
nsAccessibilityService::CreateAccessibleForDeckChild(nsIFrame* aFrame,
                                                     nsIDOMNode *aNode,
                                                     nsIWeakReference *aWeakShell)
{
  nsRefPtr<nsAccessible> accessible;

  if (aFrame->GetType() == nsAccessibilityAtoms::boxFrame ||
      aFrame->GetType() == nsAccessibilityAtoms::scrollFrame) {

    nsIFrame* parentFrame = aFrame->GetParent();
    if (parentFrame && parentFrame->GetType() == nsAccessibilityAtoms::deckFrame) {
      // If deck frame is for xul:tabpanels element then the given node has
      // tabpanel accessible.
      nsCOMPtr<nsIContent> parentContent = parentFrame->GetContent();
#ifdef MOZ_XUL
      if (parentContent->NodeInfo()->Equals(nsAccessibilityAtoms::tabpanels,
                                            kNameSpaceID_XUL)) {
        accessible = new nsXULTabpanelAccessible(aNode, aWeakShell);
      } else
#endif
        accessible =
          new nsEnumRoleAccessible(aNode, aWeakShell,
                                   nsIAccessibleRole::ROLE_PROPERTYPAGE);
    }
  }

  return accessible.forget();
}

#ifdef MOZ_XUL
already_AddRefed<nsAccessible>
nsAccessibilityService::CreateAccessibleForXULTree(nsIDOMNode *aNode,
                                                   nsIWeakReference *aWeakShell)
{
  nsCOMPtr<nsITreeBoxObject> treeBoxObj;
  nsCoreUtils::GetTreeBoxObject(aNode, getter_AddRefs(treeBoxObj));
  if (!treeBoxObj)
    return nsnull;

  nsCOMPtr<nsITreeColumns> treeColumns;
  treeBoxObj->GetColumns(getter_AddRefs(treeColumns));
  if (!treeColumns)
    return nsnull;

  nsRefPtr<nsAccessible> accessible;

  PRInt32 count = 0;
  treeColumns->GetCount(&count);
  if (count == 1) // outline of list accessible
    accessible = new nsXULTreeAccessible(aNode, aWeakShell);
  else // table or tree table accessible
    accessible = new nsXULTreeGridAccessibleWrap(aNode, aWeakShell);

  return accessible.forget();
}
#endif
