#ifdef 0
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
 * The Original Code is Thumbnails code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Taubert <ttaubert@mozilla.com> (Original Author)
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
#endif

let BaseChannel = {
  _wasOpened: false,
  _wasStarted: false,
  _isCanceled: false,

  _setup: function BCH__setup(aUri) {
    this._uri = aUri;

    // nsIChannel
    this.originalURI = aUri;

    // nsIHttpChannel
    this._requestHeaders = {};
    this._responseHeaders = {"content-type": CONTENT_TYPE};
  },

  _startRequest: function CCH__startRequest() {
    try {
      this._listener.onStartRequest(this, this._context);
    } catch (e) {
      // the listener might throw if the request has been canceled
      this.cancel(Cr.NS_BINDING_ABORTED);
    }

    return !this.canceled;
  },

  _serveData: function CCH__serveData(aDataStream) {
    try {
      let available = aDataStream.available();
      this._listener.onDataAvailable(this, this._context, aDataStream, 0, available);
    } catch (e) {
      // the listener might throw if the request has been canceled
      this.cancel(Cr.NS_BINDING_ABORTED);
    }

    return !this.canceled;
  },

  _stopRequest: function CCH__stopRequest() {
    try {
      this._listener.onStopRequest(this, this._context, this.status);
    } catch (e) {
      // this might throw but is generally ignored
    }

    this._cleanup();
  },

  _addToLoadGroup: function CCH__addToLoadGroup() {
    if (this.loadGroup)
      this.loadGroup.addRequest(this, this._context);
  },

  _removeFromLoadGroup: function CCH__removeFromLoadGroup() {
    if (!this.loadGroup)
      return;

    try {
      this.loadGroup.removeRequest(this, this._context, this.status);
    } catch (e) {
      // this might throw and is ignored
    }
  },

  _cleanup: function CCH__cleanup() {
    this._removeFromLoadGroup();
    this.loadGroup = null;

    this._isPending = false;

    delete this._listener;
    delete this._context;
    delete this._entry;
  },

  // nsIChannel
  contentType: CONTENT_TYPE,
  contentLength: -1,
  owner: null,
  contentCharset: null,
  notificationCallbacks: null,

  get URI() this._uri,
  get securityInfo() null,

  asyncOpen: function BCH_asyncOpen(aListener, aContext) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  open: function BCH_open() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  // nsIHttpChannel
  redirectionLimit: 10,
  requestMethod: "GET",
  allowPipelining: true,
  referrer: null,

  get requestSucceeded() true,
  get responseStatus() 200,
  get responseStatusText() "OK",

  isNoCacheResponse: function () false,
  isNoStoreResponse: function () false,

  getRequestHeader: function BCH_getRequestHeader(aHeader) {
    aHeader = aHeader.toLowerCase();
    let headers = this._requestHeaders;

    if (aHeader in headers)
      return headers[aHeader];

    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  setRequestHeader: function BCH_setRequestHeader(aHeader, aValue, aMerge) {
    if (aMerge)
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;

    this._requestHeaders[aHeader.toLowerCase()] = aValue;
  },

  visitRequestHeaders: function BCH_visitRequestHeaders(aVisitor) {
    this._visitHeaders(this._requestHeaders, aVisitor);
  },

  getResponseHeader: function BCH_getResponseHeader(aHeader) {
    aHeader = aHeader.toLowerCase();
    let headers = this._responseHeaders;

    if (aHeader in headers)
      return headers[aHeader];

    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  setResponseHeader: function BCH_setResponseHeader(aHeader, aValue, aMerge) {
    if (aMerge)
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;

    this._responseHeaders[aHeader.toLowerCase()] = aValue;
  },

  visitResponseHeaders: function BCH_visitResponseHeaders(aVisitor) {
    this._visitHeaders(this._responseHeaders, aVisitor);
  },

  _visitHeaders: function BCH__visitHeaders(aHeaders, aVisitor) {
    for (let name in aHeaders) {
      let value = aHeaders[name];

      try {
        aVisitor.visitHeader(name, value);
      } catch (e) {
        // the visitor throws to stop the iteration
        return;
      }
    }
  },

  // nsIRequest
  _status: Cr.NS_OK,
  _isPending: false,

  loadFlags: Ci.nsIRequest.LOAD_NORMAL,
  loadGroup: null,

  get name() this._uri.spec,
  get status() this._status,

  isPending: function () this._isPending,
  resume: function () {},
  suspend: function () {},

  cancel: function BCH_cancel(aStatus) {
    if (this.canceled)
      return;

    this._isCanceled = true;
    this._status = aStatus;
  },

  // nsIHttpChannelInternal
  documentURI: null,

  get canceled() this._isCanceled,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannel,
                                         Ci.nsIHttpChannel,
                                         Ci.nsIHttpChannelInternal,
                                         Ci.nsIRequest])
};
