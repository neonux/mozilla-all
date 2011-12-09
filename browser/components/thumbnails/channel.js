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

// ##########
// Class: Channel
// A basic http channel implementation that extends itself depending on whether
// we deliver a cached thumbnail or need to generate a new one.
function Channel(aUri) {
  this._uri = aUri;

  // nsIChannel
  this.originalURI = aUri;

  // nsIHttpChannel
  this._requestHeaders = {};
  this._responseHeaders = {"content-type": CONTENT_TYPE};
}

Channel.prototype = {
  _wasOpened: false,
  _wasStarted: false,
  _isCanceled: false,

  // ----------
  // Function: asyncOpen
  // Opens this channel asynchronously.
  //
  // Parameters:
  //   aListener - the listener that receives the channel data when available
  //   aContext - a custom context passed to the listener's methods
  asyncOpen: function CH_asyncOpen(aListener, aContext) {
    if (this._wasOpened)
      throw Cr.NS_ERROR_ALREADY_OPENED;

    if (this.canceled)
      return;

    this._listener = aListener;
    this._context = aContext;

    this._isPending = true;
    this._wasOpened = true;

    let {refresh} = Utils.parseUri(this.URI);
    this._cacheKey = this._getCacheKey();

    // the url specifies that we should refresh the thumbnail. so let's
    // create a new thumbnail for the given url and store it in the cache
    // regardless of whether it's already cached.
    if (refresh) {
      GenerateChannel.extend(this);
      return;
    }

    let self = this;

    // query the cache and retrieve our entry
    Cache.getReadEntry(this._cacheKey, function (aEntry) {
      let inputStream = aEntry && aEntry.openInputStream(0);

      if (aEntry && inputStream.available())
        // entry found, deliver from cache
        CacheChannel.extend(self, aEntry);
      else
        // entry not found, create a thumbnail and deliver it when ready
        GenerateChannel.extend(self);
    });
  },

  // ----------
  // Function: _getCacheKey
  // Returns the cache key to use for the given thumbnail URI. This puts params
  // in a fixed order and removes params that must not be included in the key.
  _getCacheKey: function BCH__getCacheKey() {
    let keys = ["algorithm", "url", "width", "height"];
    let params = Utils.parseUri(this.URI);

    let pairs = keys.map(function (key) {
      let value = encodeURIComponent(params[key] || "");
      return encodeURIComponent(key) + "=" + value;
    });

    return pairs.join("&");
  },

  // ----------
  // Function: _startRequest
  // Calls onStartRequest on the channel listener.
  _startRequest: function BCH__startRequest() {
    try {
      this._listener.onStartRequest(this, this._context);
    } catch (e) {
      // the listener might throw if the request has been canceled
      this.cancel(Cr.NS_BINDING_ABORTED);
    }

    return !this.canceled;
  },

  // ----------
  // Function: _serveData
  // Calls onDataAvailable on the channel listener and passes the data stream.
  //
  // Parameters:
  //   aUri - the URI belonging to this channel
  _serveData: function BCH__serveData(aDataStream) {
    try {
      let available = aDataStream.available();
      this._listener.onDataAvailable(this, this._context, aDataStream, 0, available);
    } catch (e) {
      // the listener might throw if the request has been canceled
      this.cancel(Cr.NS_BINDING_ABORTED);
    }

    return !this.canceled;
  },

  // ----------
  // Function: _stopRequest
  // Calls onStopRequest on the channel listener.
  _stopRequest: function BCH__stopRequest() {
    try {
      this._listener.onStopRequest(this, this._context, this.status);
    } catch (e) {
      // this might throw but is generally ignored
    }

    // the request has finished, clean up after ourselves
    this._cleanup();
  },

  // ----------
  // Function: _addToLoadGroup
  // Adds this request to the load group, if any.
  _addToLoadGroup: function BCH__addToLoadGroup() {
    if (this.loadGroup)
      this.loadGroup.addRequest(this, this._context);
  },

  // ----------
  // Function: _removeFromLoadGroup
  // Removes this request from the load group, if any.
  _removeFromLoadGroup: function BCH__removeFromLoadGroup() {
    if (!this.loadGroup)
      return;

    try {
      this.loadGroup.removeRequest(this, this._context, this.status);
    } catch (e) {
      // this might throw and is ignored
    }
  },

  // ----------
  // Function: _cleanup
  // Cleans up the channel when the request has finished.
  _cleanup: function BCH__cleanup() {
    this._removeFromLoadGroup();
    this.loadGroup = null;

    this._isPending = false;

    if (this._entry)
      this._entry.close();

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

  // ----------
  // Function: open
  // Opens this channel synchronously.
  open: function BCH_open() {
    // synchronous data delivery is not implemented
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

  // ----------
  // Function: isNoCacheResponse
  // Returns true if the server sent the equivalent of a
  // "Cache-control: no-cache" response header. Always false.
  isNoCacheResponse: function () false,

  // ----------
  // Function: isNoStoreResponse
  // Returns true if the server sent the equivalent of a
  // "Cache-control: no-cache" response header. Always false.
  isNoStoreResponse: function () false,

  // ----------
  // Function: getRequestHeader
  // Returns the value of a particular request header.
  //
  // Parameters:
  //   aHeader - the case-insensitive name of the request header to query
  getRequestHeader: function BCH_getRequestHeader(aHeader) {
    aHeader = aHeader.toLowerCase();
    let headers = this._requestHeaders;

    if (aHeader in headers)
      return headers[aHeader];

    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  // ----------
  // Function: setRequestHeader
  // This method is called to set the value of a particular request header.
  //
  // Parameters:
  //   aHeader - the case-insensitive name of the request header to query
  //   aValue - the request header value to set
  setRequestHeader: function BCH_setRequestHeader(aHeader, aValue, aMerge) {
    if (aMerge)
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;

    this._requestHeaders[aHeader.toLowerCase()] = aValue;
  },

  // ----------
  // Function: visitRequestHeaders
  // Call this method to visit all request headers.
  //
  // Parameters:
  //   aVisitor - the header visitor
  visitRequestHeaders: function BCH_visitRequestHeaders(aVisitor) {
    this._visitHeaders(this._requestHeaders, aVisitor);
  },

  // ----------
  // Function: getResponseHeader
  // Returns the value of a particular response header.
  //
  // Parameters:
  //   aHeader - the case-insensitive name of the response header to query
  getResponseHeader: function BCH_getResponseHeader(aHeader) {
    aHeader = aHeader.toLowerCase();
    let headers = this._responseHeaders;

    if (aHeader in headers)
      return headers[aHeader];

    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  // ----------
  // Function: setResponseHeader
  // This method is called to set the value of a particular response header.
  //
  // Parameters:
  //   aHeader - the case-insensitive name of the response header to query
  //   aValue - the response header value to set
  setResponseHeader: function BCH_setResponseHeader(aHeader, aValue, aMerge) {
    if (aMerge)
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;

    this._responseHeaders[aHeader.toLowerCase()] = aValue;
  },

  // ----------
  // Function: visitResponseHeaders
  // Call this method to visit all response headers.
  //
  // Parameters:
  //   aVisitor - the header visitor
  visitResponseHeaders: function BCH_visitResponseHeaders(aVisitor) {
    this._visitHeaders(this._responseHeaders, aVisitor);
  },

  // ----------
  // Function: _visitHeaders
  // Generic function to visit an array of headers.
  //
  // Parameters:
  //   aHeaders - the headers to visit
  //   aVisitor - the header visitor
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

  // ----------
  // Function: cancel
  // Cancels this request.
  //
  // Parameters:
  //   aStatus - the reason for cancelling
  cancel: function BCH_cancel(aStatus) {
    if (this.canceled)
      return;

    this._isCanceled = true;
    this._status = aStatus;

    this._cleanup();
  },

  // nsIHttpChannelInternal
  documentURI: null,

  get canceled() this._isCanceled,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannel,
                                         Ci.nsIHttpChannel,
                                         Ci.nsIHttpChannelInternal,
                                         Ci.nsIRequest])
};
