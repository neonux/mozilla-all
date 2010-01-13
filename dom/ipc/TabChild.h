/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
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
 * The Original Code is Mozilla Content App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_tabs_TabChild_h
#define mozilla_tabs_TabChild_h

#include "mozilla/dom/PIFrameEmbeddingChild.h"
#include "nsIWebNavigation.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIWebBrowserChrome2.h"
#include "nsIEmbeddingSiteWindow2.h"
#include "nsIWebBrowserChromeFocus.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIInterfaceRequestor.h"
#include "nsIWindowProvider.h"

namespace mozilla {
namespace dom {

class TabChild;

class ContentListener : public nsIDOMEventListener
{
public:
  ContentListener(TabChild* aTabChild) : mTabChild(aTabChild) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
protected:
  TabChild* mTabChild;
};

class TabChild : public PIFrameEmbeddingChild,
                 public nsIWebBrowserChrome2,
                 public nsIEmbeddingSiteWindow2,
                 public nsIWebBrowserChromeFocus,
                 public nsIInterfaceRequestor,
                 public nsIWindowProvider
{
public:
    TabChild();
    virtual ~TabChild();
    bool destroyWidget();
    nsresult Init();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROME2
    NS_DECL_NSIEMBEDDINGSITEWINDOW
    NS_DECL_NSIEMBEDDINGSITEWINDOW2
    NS_DECL_NSIWEBBROWSERCHROMEFOCUS
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWINDOWPROVIDER

    virtual bool RecvcreateWidget(const MagicWindowHandle& parentWidget);
    virtual bool RecvloadURL(const nsCString& uri);
    virtual bool Recvmove(const PRUint32& x,
                          const PRUint32& y,
                          const PRUint32& width,
                          const PRUint32& height);
    virtual bool Recvactivate();
    virtual bool RecvsendMouseEvent(const nsString& aType,
                                    const PRInt32&  aX,
                                    const PRInt32&  aY,
                                    const PRInt32&  aButton,
                                    const PRInt32&  aClickCount,
                                    const PRInt32&  aModifiers,
                                    const bool&     aIgnoreRootScrollFrame);
    virtual bool RecvactivateFrameEvent(const nsString& aType, const bool& capture);
    virtual mozilla::ipc::PDocumentRendererChild* AllocPDocumentRenderer(
            const PRInt32& x,
            const PRInt32& y,
            const PRInt32& w,
            const PRInt32& h,
            const nsString& bgcolor,
            const PRUint32& flags,
            const bool& flush);
    virtual bool DeallocPDocumentRenderer(PDocumentRendererChild* actor);
    virtual bool RecvPDocumentRendererConstructor(
            mozilla::ipc::PDocumentRendererChild *__a,
            const PRInt32& x,
            const PRInt32& y,
            const PRInt32& w,
            const PRInt32& h,
            const nsString& bgcolor,
            const PRUint32& flags,
            const bool& flush);

private:
    nsCOMPtr<nsIWebNavigation> mWebNav;

    DISALLOW_EVIL_CONSTRUCTORS(TabChild);
};

}
}

#endif // mozilla_tabs_TabChild_h
