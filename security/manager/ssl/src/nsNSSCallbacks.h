/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSNSSCALLBACKS_H_
#define _NSNSSCALLBACKS_H_

#include "pk11func.h"
#include "nspr.h"
#include "ocspt.h"
#include "nsIStreamLoader.h"
#include "mozilla/CondVar.h"
#include "mozilla/Mutex.h"

char* PR_CALLBACK
PK11PasswordPrompt(PK11SlotInfo *slot, PRBool retry, void* arg);

void PR_CALLBACK HandshakeCallback(PRFileDesc *fd, void *client_data);

SECStatus RegisterMyOCSPAIAInfoCallback();
SECStatus UnregisterMyOCSPAIAInfoCallback();

class nsHTTPListener : public nsIStreamLoaderObserver
{
private:
  // For XPCOM implementations that are not a base class for some other
  // class, it is good practice to make the destructor non-virtual and
  // private.  Then the only way to delete the object is via Release.
  ~nsHTTPListener();

public:
  nsHTTPListener();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER

  nsCOMPtr<nsIStreamLoader> mLoader;

  nsresult mResultCode;

  bool mHttpRequestSucceeded;
  PRUint16 mHttpResponseCode;
  nsCString mHttpResponseContentType;

  const PRUint8* mResultData; // not owned, refers to mLoader
  PRUint32 mResultLen;
  
  mozilla::Mutex mLock;
  mozilla::CondVar mCondition;
  volatile bool mWaitFlag;
  
  bool mResponsibleForDoneSignal;
  void send_done_signal();

  // no nsCOMPtr. When I use it, I get assertions about
  //   loadgroup not being thread safe.
  // So, let's use a raw pointer and ensure we only create and destroy
  // it on the network thread ourselves.
  nsILoadGroup *mLoadGroup;
  PRThread *mLoadGroupOwnerThread;
  void FreeLoadGroup(bool aCancelLoad);
};

class nsNSSHttpServerSession
{
public:
  nsCString mHost;
  PRUint16 mPort;  

  static SECStatus createSessionFcn(const char *host,
                                    PRUint16 portnum,
                                    SEC_HTTP_SERVER_SESSION *pSession);
};

class nsNSSHttpRequestSession
{
protected:
  PRInt32 mRefCount;

public:
  static SECStatus createFcn(SEC_HTTP_SERVER_SESSION session,
                             const char *http_protocol_variant,
                             const char *path_and_query_string,
                             const char *http_request_method, 
                             const PRIntervalTime timeout, 
                             SEC_HTTP_REQUEST_SESSION *pRequest);

  SECStatus setPostDataFcn(const char *http_data, 
                           const PRUint32 http_data_len,
                           const char *http_content_type);

  SECStatus addHeaderFcn(const char *http_header_name, 
                         const char *http_header_value);

  SECStatus trySendAndReceiveFcn(PRPollDesc **pPollDesc,
                                 PRUint16 *http_response_code, 
                                 const char **http_response_content_type, 
                                 const char **http_response_headers, 
                                 const char **http_response_data, 
                                 PRUint32 *http_response_data_len);

  SECStatus cancelFcn();
  SECStatus freeFcn();

  void AddRef();
  void Release();

  nsCString mURL;
  nsCString mRequestMethod;
  
  bool mHasPostData;
  nsCString mPostData;
  nsCString mPostContentType;
  
  PRIntervalTime mTimeoutInterval;
  
  nsCOMPtr<nsHTTPListener> mListener;
  
protected:
  nsNSSHttpRequestSession();
  ~nsNSSHttpRequestSession();

  SECStatus internal_send_receive_attempt(bool &retryable_error,
                                          PRPollDesc **pPollDesc,
                                          PRUint16 *http_response_code,
                                          const char **http_response_content_type,
                                          const char **http_response_headers,
                                          const char **http_response_data,
                                          PRUint32 *http_response_data_len);
};

class nsNSSHttpInterface
{
public:
  static SECStatus createSessionFcn(const char *host,
                                    PRUint16 portnum,
                                    SEC_HTTP_SERVER_SESSION *pSession)
  {
    return nsNSSHttpServerSession::createSessionFcn(host, portnum, pSession);
  }

  static SECStatus keepAliveFcn(SEC_HTTP_SERVER_SESSION session,
                                PRPollDesc **pPollDesc)
  {
    // Not yet implemented, however, Necko does transparent keep-alive 
    // anyway, when enabled in Necko's prefs.
    return SECSuccess;
  }

  static SECStatus freeSessionFcn(SEC_HTTP_SERVER_SESSION session)
  {
    delete static_cast<nsNSSHttpServerSession*>(session);
    return SECSuccess;
  }

  static SECStatus createFcn(SEC_HTTP_SERVER_SESSION session,
                             const char *http_protocol_variant,
                             const char *path_and_query_string,
                             const char *http_request_method, 
                             const PRIntervalTime timeout, 
                             SEC_HTTP_REQUEST_SESSION *pRequest)
  {
    return nsNSSHttpRequestSession::createFcn(session, http_protocol_variant,
                                     path_and_query_string, http_request_method, 
                                     timeout, pRequest);
  }

  static SECStatus setPostDataFcn(SEC_HTTP_REQUEST_SESSION request, 
                                  const char *http_data, 
                                  const PRUint32 http_data_len,
                                  const char *http_content_type)
  {
    return static_cast<nsNSSHttpRequestSession*>(request)
            ->setPostDataFcn(http_data, http_data_len, http_content_type);
  }

  static SECStatus addHeaderFcn(SEC_HTTP_REQUEST_SESSION request,
                                const char *http_header_name, 
                                const char *http_header_value)
  {
    return static_cast<nsNSSHttpRequestSession*>(request)
            ->addHeaderFcn(http_header_name, http_header_value);
  }

  static SECStatus trySendAndReceiveFcn(SEC_HTTP_REQUEST_SESSION request, 
                                        PRPollDesc **pPollDesc,
                                        PRUint16 *http_response_code, 
                                        const char **http_response_content_type, 
                                        const char **http_response_headers, 
                                        const char **http_response_data, 
                                        PRUint32 *http_response_data_len)
  {
    return static_cast<nsNSSHttpRequestSession*>(request)
            ->trySendAndReceiveFcn(pPollDesc, http_response_code, http_response_content_type, 
                     http_response_headers, http_response_data, http_response_data_len);
  }

  static SECStatus cancelFcn(SEC_HTTP_REQUEST_SESSION request)
  {
    return static_cast<nsNSSHttpRequestSession*>(request)
            ->cancelFcn();
  }

  static SECStatus freeFcn(SEC_HTTP_REQUEST_SESSION request)
  {
    return static_cast<nsNSSHttpRequestSession*>(request)
            ->freeFcn();
  }

  static void initTable();
  static SEC_HttpClientFcn sNSSInterfaceTable;

  void registerHttpClient();
  void unregisterHttpClient();
};

#endif // _NSNSSCALLBACKS_H_



