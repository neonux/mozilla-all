/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/* 
*/

#ifndef nsDocLoader_h__
#define nsDocLoader_h__

#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIRequestObserver.h"
#include "nsWeakReference.h"
#include "nsILoadGroup.h"
#include "nsCOMArray.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsIChannel.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIChannelEventSink.h"
#include "nsISecurityEventSink.h"
#include "nsISupportsPriority.h"
#include "nsCOMPtr.h"
#include "pldhash.h"
#include "prclist.h"
#include "nsAutoPtr.h"

struct nsRequestInfo;
struct nsListenerInfo;

/****************************************************************************
 * nsDocLoader implementation...
 ****************************************************************************/

#define NS_THIS_DOCLOADER_IMPL_CID                    \
 { /* b4ec8387-98aa-4c08-93b6-6d23069c06f2 */         \
     0xb4ec8387,                                      \
     0x98aa,                                          \
     0x4c08,                                          \
     {0x93, 0xb6, 0x6d, 0x23, 0x06, 0x9c, 0x06, 0xf2} \
 }

class nsDocLoader : public nsIDocumentLoader, 
                    public nsIRequestObserver,
                    public nsSupportsWeakReference,
                    public nsIProgressEventSink,
                    public nsIWebProgress,
                    public nsIInterfaceRequestor,
                    public nsIChannelEventSink,
                    public nsISecurityEventSink,
                    public nsISupportsPriority
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_THIS_DOCLOADER_IMPL_CID)

    nsDocLoader();

    virtual nsresult Init();

    static already_AddRefed<nsDocLoader> GetAsDocLoader(nsISupports* aSupports);
    // Needed to deal with ambiguous inheritance from nsISupports...
    static nsISupports* GetAsSupports(nsDocLoader* aDocLoader) {
        return static_cast<nsIDocumentLoader*>(aDocLoader);
    }

    // Add aDocLoader as a child to the docloader service.
    static nsresult AddDocLoaderAsChildOfRoot(nsDocLoader* aDocLoader);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOCUMENTLOADER
    
    // nsIProgressEventSink
    NS_DECL_NSIPROGRESSEVENTSINK

    NS_DECL_NSISECURITYEVENTSINK

    // nsIRequestObserver methods: (for observing the load group)
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIWEBPROGRESS

    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSISUPPORTSPRIORITY

    // Implementation specific methods...

    // Remove aChild from our childlist.  This nulls out the child's mParent
    // pointer.
    nsresult RemoveChildLoader(nsDocLoader *aChild);
    // Add aChild to our child list.  This will set aChild's mParent pointer to
    // |this|.
    nsresult AddChildLoader(nsDocLoader* aChild);
    nsDocLoader* GetParent() const { return mParent; }

protected:
    virtual ~nsDocLoader();

    virtual nsresult SetDocLoaderParent(nsDocLoader * aLoader);

    bool IsBusy();

    void Destroy();
    virtual void DestroyChildren();

    nsIDocumentLoader* ChildAt(PRInt32 i) {
        return static_cast<nsDocLoader*>(mChildList[i]);
    }

    nsIDocumentLoader* SafeChildAt(PRInt32 i) {
        return static_cast<nsDocLoader*>(mChildList.SafeElementAt(i));
    }

    void FireOnProgressChange(nsDocLoader* aLoadInitiator,
                              nsIRequest *request,
                              PRInt64 aProgress,
                              PRInt64 aProgressMax,
                              PRInt64 aProgressDelta,
                              PRInt64 aTotalProgress,
                              PRInt64 aMaxTotalProgress);

    // This should be at least 2 long since we'll generally always
    // have the current page and the global docloader on the ancestor
    // list.  But to deal with frames it's better to make it a bit
    // longer, and it's always a stack temporary so there's no real
    // reason not to.
    typedef nsAutoTArray<nsRefPtr<nsDocLoader>, 8> WebProgressList;
    void GatherAncestorWebProgresses(WebProgressList& aList);

    void FireOnStateChange(nsIWebProgress *aProgress,
                           nsIRequest* request,
                           PRInt32 aStateFlags,
                           nsresult aStatus);

    // The guts of FireOnStateChange, but does not call itself on our ancestors.
    // The arguments that are const are const so that we can detect cases when
    // DoFireOnStateChange wants to propagate changes to the next web progress
    // at compile time.  The ones that are not, are references so that such
    // changes can be propagated.
    void DoFireOnStateChange(nsIWebProgress * const aProgress,
                             nsIRequest* const request,
                             PRInt32 &aStateFlags,
                             const nsresult aStatus);

    void FireOnStatusChange(nsIWebProgress *aWebProgress,
                            nsIRequest *aRequest,
                            nsresult aStatus,
                            const PRUnichar* aMessage);

    void FireOnLocationChange(nsIWebProgress* aWebProgress,
                              nsIRequest* aRequest,
                              nsIURI *aUri);

    bool RefreshAttempted(nsIWebProgress* aWebProgress,
                            nsIURI *aURI,
                            PRInt32 aDelay,
                            bool aSameURI);

    // this function is overridden by the docshell, it is provided so that we
    // can pass more information about redirect state (the normal OnStateChange
    // doesn't get the new channel).
    // @param aRedirectFlags The flags being sent to OnStateChange that
    //                       indicate the type of redirect.
    // @param aStateFlags    The channel flags normally sent to OnStateChange.
    virtual void OnRedirectStateChange(nsIChannel* aOldChannel,
                                       nsIChannel* aNewChannel,
                                       PRUint32 aRedirectFlags,
                                       PRUint32 aStateFlags) {}

    void doStartDocumentLoad();
    void doStartURLLoad(nsIRequest *request);
    void doStopURLLoad(nsIRequest *request, nsresult aStatus);
    void doStopDocumentLoad(nsIRequest *request, nsresult aStatus);

    // Inform a parent docloader that aChild is about to call its onload
    // handler.
    bool ChildEnteringOnload(nsIDocumentLoader* aChild) {
        // It's ok if we're already in the list -- we'll just be in there twice
        // and then the RemoveObject calls from ChildDoneWithOnload will remove
        // us.
        return mChildrenInOnload.AppendObject(aChild);
    }

    // Inform a parent docloader that aChild is done calling its onload
    // handler.
    void ChildDoneWithOnload(nsIDocumentLoader* aChild) {
        mChildrenInOnload.RemoveObject(aChild);
        DocLoaderIsEmpty(PR_TRUE);
    }        

protected:
    // IMPORTANT: The ownership implicit in the following member
    // variables has been explicitly checked and set using nsCOMPtr
    // for owning pointers and raw COM interface pointers for weak
    // (ie, non owning) references. If you add any members to this
    // class, please make the ownership explicit (pinkerton, scc).
  
    nsCOMPtr<nsIRequest>       mDocumentRequest;       // [OWNER] ???compare with document

    nsDocLoader*               mParent;                // [WEAK]

    nsVoidArray                mListenerInfoList;

    nsCOMPtr<nsILoadGroup>        mLoadGroup;
    // We hold weak refs to all our kids
    nsVoidArray                   mChildList;

    // The following member variables are related to the new nsIWebProgress 
    // feedback interfaces that travis cooked up.
    PRInt32 mProgressStateFlags;

    PRInt64 mCurrentSelfProgress;
    PRInt64 mMaxSelfProgress;

    PRInt64 mCurrentTotalProgress;
    PRInt64 mMaxTotalProgress;

    PLDHashTable mRequestInfoHash;
    PRInt64 mCompletedTotalProgress;

    PRCList mStatusInfoList;

    /*
     * This flag indicates that the loader is loading a document.  It is set
     * from the call to LoadDocument(...) until the OnConnectionsComplete(...)
     * notification is fired...
     */
    bool mIsLoadingDocument;

    /* Flag to indicate that we're in the process of restoring a document. */
    bool mIsRestoringDocument;

    /* Flag to indicate that we're in the process of flushing layout
       under DocLoaderIsEmpty() and should not do another flush. */
    bool mDontFlushLayout;

    /* Flag to indicate whether we should consider ourselves as currently
       flushing layout for the purposes of IsBusy. For example, if Stop has
       been called then IsBusy should return false even if we are still
       flushing. */
    bool mIsFlushingLayout;

private:
    // A list of kids that are in the middle of their onload calls and will let
    // us know once they're done.  We don't want to fire onload for "normal"
    // DocLoaderIsEmpty calls (those coming from requests finishing in our
    // loadgroup) unless this is empty.
    nsCOMArray<nsIDocumentLoader> mChildrenInOnload;
    
    // DocLoaderIsEmpty should be called whenever the docloader may be empty.
    // This method is idempotent and does nothing if the docloader is not in
    // fact empty.  This method _does_ make sure that layout is flushed if our
    // loadgroup has no active requests before checking for "real" emptiness if
    // aFlushLayout is true.
    void DocLoaderIsEmpty(bool aFlushLayout);

    nsListenerInfo *GetListenerInfo(nsIWebProgressListener* aListener);

    PRInt64 GetMaxTotalProgress();

    nsresult AddRequestInfo(nsIRequest* aRequest);
    void RemoveRequestInfo(nsIRequest* aRequest);
    nsRequestInfo *GetRequestInfo(nsIRequest* aRequest);
    void ClearRequestInfoHash();
    PRInt64 CalculateMaxProgress();
///    void DumpChannelInfo(void);

    // used to clear our internal progress state between loads...
    void ClearInternalProgress(); 
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsDocLoader, NS_THIS_DOCLOADER_IMPL_CID)

#endif /* nsDocLoader_h__ */
