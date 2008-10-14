/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is worker threads.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
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

#ifndef __NSDOMWORKERXHR_H__
#define __NSDOMWORKERXHR_H__

// Bases
#include "nsIXMLHttpRequest.h"
#include "nsIClassInfo.h"

// Interfaces

// Other includes
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "prlock.h"

// DOMWorker includes
#include "nsDOMWorkerThread.h"

class nsDOMWorkerXHR;
class nsDOMWorkerXHREvent;
class nsDOMWorkerXHRProxy;

class nsDOMWorkerXHREventTarget : public nsIXMLHttpRequestEventTarget
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTTARGET
  NS_DECL_NSIXMLHTTPREQUESTEVENTTARGET

  static const char* const sListenerTypes[];
  static const PRUint32 sMaxXHREventTypes;
  static const PRUint32 sMaxUploadEventTypes;

  static PRUint32 GetListenerTypeFromString(const nsAString& aString);

  virtual nsresult SetEventListener(PRUint32 aType,
                                    nsIDOMEventListener* aListener,
                                    PRBool aOnXListener) = 0;

  virtual nsresult UnsetEventListener(PRUint32 aType,
                                      nsIDOMEventListener* aListener) = 0;

  virtual nsresult HandleWorkerEvent(nsIDOMEvent* aEvent) = 0;

  virtual already_AddRefed<nsIDOMEventListener>
    GetOnXListener(PRUint32 aType) = 0;

protected:
  virtual ~nsDOMWorkerXHREventTarget() { }
};

class nsDOMWorkerXHRUpload : public nsDOMWorkerXHREventTarget,
                             public nsIXMLHttpRequestUpload,
                             public nsIClassInfo
{
  friend class nsDOMWorkerXHR;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMWorkerXHREventTarget::)
  NS_FORWARD_NSIXMLHTTPREQUESTEVENTTARGET(nsDOMWorkerXHREventTarget::)
  NS_DECL_NSIXMLHTTPREQUESTUPLOAD
  NS_DECL_NSICLASSINFO

  nsDOMWorkerXHRUpload(nsDOMWorkerXHR* aWorkerXHR);

  virtual nsresult SetEventListener(PRUint32 aType,
                                    nsIDOMEventListener* aListener,
                                    PRBool aOnXListener);

  virtual nsresult UnsetEventListener(PRUint32 aType,
                                      nsIDOMEventListener* aListener);

  virtual nsresult HandleWorkerEvent(nsIDOMEvent* aEvent);

  virtual already_AddRefed<nsIDOMEventListener>
    GetOnXListener(PRUint32 aType);

protected:
  virtual ~nsDOMWorkerXHRUpload() { }

  nsRefPtr<nsDOMWorkerXHR> mWorkerXHR;
};

class nsDOMWorkerXHR : public nsDOMWorkerXHREventTarget,
                       public nsIXMLHttpRequest,
                       public nsIClassInfo
{
  friend class nsDOMWorkerXHREvent;
  friend class nsDOMWorkerXHRProxy;
  friend class nsDOMWorkerXHRUpload;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXMLHTTPREQUEST
  NS_DECL_NSICLASSINFO

  nsDOMWorkerXHR(nsDOMWorkerThread* aWorker);

  nsresult Init();

  void Cancel();

  virtual nsresult SetEventListener(PRUint32 aType,
                                    nsIDOMEventListener* aListener,
                                    PRBool aOnXListener);

  virtual nsresult UnsetEventListener(PRUint32 aType,
                                      nsIDOMEventListener* aListener);

  virtual nsresult HandleWorkerEvent(nsIDOMEvent* aEvent);

  virtual already_AddRefed<nsIDOMEventListener>
    GetOnXListener(PRUint32 aType);

protected:
  virtual ~nsDOMWorkerXHR();

  PRLock* Lock() {
    return mWorker->Lock();
  }

  nsRefPtr<nsDOMWorkerThread> mWorker;
  nsRefPtr<nsDOMWorkerXHRProxy> mXHRProxy;
  nsRefPtr<nsDOMWorkerXHRUpload> mUpload;

  volatile PRBool mCanceled;
};

#endif /* __NSDOMWORKERXHR_H__ */
