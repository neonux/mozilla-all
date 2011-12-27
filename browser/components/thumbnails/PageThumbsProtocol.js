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

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Cr = Components.results;
const Ci = Components.interfaces;

Cu.import("resource:///modules/PageThumbs.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

/**
 * Implements the thumbnail protocol handler responsible for moz-page-thumb: URIs.
 */
function Protocol() {
}

Protocol.prototype = {
  /**
   * The scheme used by this protocol.
   */
  get scheme() PageThumbs.scheme,

  /**
   * The default port for this protocol (we don't support ports).
   */
  get defaultPort() -1,

  /**
   * The flags specific to this protocol implementation.
   */
  get protocolFlags() {
    return Ci.nsIProtocolHandler.URI_DANGEROUS_TO_LOAD |
           Ci.nsIProtocolHandler.URI_NORELATIVE |
           Ci.nsIProtocolHandler.URI_NOAUTH;
  },

  /**
   * Creates a new URI object that is suitable for loading by this protocol.
   * @param aSpec The URI string in UTF8 encoding.
   * @param aOriginCharset The charset of the document from which the URI originated.
   * @return The newly created URI.
   */
  newURI: function Proto_newURI(aSpec, aOriginCharset) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  },

  /**
   * Constructs a new channel from the given URI for this protocol handler.
   * @param aUri The URI for which to construct a channel.
   * @return The newly created channel.
   */
  newChannel: function Proto_newChannel(aURI) {
    return new Channel(aURI);
  },

  /**
   * Decides whether to allow a blacklisted port.
   * @return Always false, we'll never allow ports.
   */
  allowPort: function () false,

  classID: Components.ID("{5a4ae9b5-f475-48ae-9dce-0b4c1d347884}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler])
};

let NSGetFactory = XPCOMUtils.generateNSGetFactory([Protocol]);

/**
 * A channel implementation responsible for delivering cached thumbnails.
 */
function Channel(aUri) {
  this._uri = aUri;

  // nsIChannel
  this.originalURI = aUri;

  // nsIHttpChannel
  this._responseHeaders = {"content-type": PageThumbs.contentType};
}

Channel.prototype = {
  /** 
   * Tracks if the channel has been opened, yet.
   */
  _wasOpened: false,

  /**
   * Opens this channel asynchronously.
   * @param aListener The listener that receives the channel data when available.
   * @param aContext A custom context passed to the listener's methods.
   */
  asyncOpen: function Channel_asyncOpen(aListener, aContext) {
    if (this._wasOpened)
      throw Cr.NS_ERROR_ALREADY_OPENED;

    if (this.canceled)
      return;

    this._listener = aListener;
    this._context = aContext;

    this._isPending = true;
    this._wasOpened = true;

    // Try to read the data from the thumbnail cache.
    this._readCache(function (aData) {
      // Update response if there's no data.
      if (!aData) {
        this._responseStatus = 404;
        this._responseText = "Not Found";
      }

      this._startRequest();

      if (!this.canceled) {
        this._addToLoadGroup();

        if (aData)
          this._serveData(aData);

        if (!this.canceled)
          this._stopRequest();
      }
    }.bind(this));
  },

  /**
   * Reads a data stream from the cache entry.
   * @param aCallback The callback the data is passed to.
   */
  _readCache: function Channel_readCache(aCallback) {
    let {url} = parseURI(this._uri);

    // Return early if there's no valid URL given.
    if (!url) {
      aCallback();
      return;
    }

    // Try to get a cache entry.
    PageThumbsCache.getReadEntry(url, function (aEntry) {
      let inputStream = aEntry && aEntry.openInputStream(0);

      // Check if we have a valid entry and if it has any data.
      if (!inputStream || !inputStream.available()) {
        if (aEntry) {
          aEntry.close();
        }
        aCallback();
        return;
      }

      // Read the cache entry's data.
      NetUtil.asyncFetch(inputStream, function (aData, aStatus) {
        // We might have been canceled while waiting.
        if (this.canceled)
          return;

        // Check if we have a valid data stream.
        if (!Components.isSuccessCode(aStatus) || !aData.available())
          aData = null;

        aEntry.close();
        aCallback(aData);
      }.bind(this));
    }.bind(this));
  },

  /**
   * Calls onStartRequest on the channel listener.
   */
  _startRequest: function Channel_startRequest() {
    try {
      this._listener.onStartRequest(this, this._context);
    } catch (e) {
      // The listener might throw if the request has been canceled.
      this.cancel(Cr.NS_BINDING_ABORTED);
    }
  },

  /**
   * Calls onDataAvailable on the channel listener and passes the data stream.
   * @param aData The data to be delivered.
   */
  _serveData: function Channel_serveData(aData) {
    try {
      let available = aData.available();
      this._listener.onDataAvailable(this, this._context, aData, 0, available);
    } catch (e) {
      // The listener might throw if the request has been canceled.
      this.cancel(Cr.NS_BINDING_ABORTED);
    }
  },

  /**
   * Calls onStopRequest on the channel listener.
   */
  _stopRequest: function Channel_stopRequest() {
    try {
      this._listener.onStopRequest(this, this._context, this.status);
    } catch (e) {
      // This might throw but is generally ignored.
    }

    // The request has finished, clean up after ourselves.
    this._cleanup();
  },

  /**
   * Adds this request to the load group, if any.
   */
  _addToLoadGroup: function Channel_addToLoadGroup() {
    if (this.loadGroup)
      this.loadGroup.addRequest(this, this._context);
  },

  /**
   * Removes this request from its load group, if any.
   */
  _removeFromLoadGroup: function Channel_removeFromLoadGroup() {
    if (!this.loadGroup)
      return;

    try {
      this.loadGroup.removeRequest(this, this._context, this.status);
    } catch (e) {
      // This might throw but is ignored.
    }
  },

  /**
   * Cleans up the channel when the request has finished.
   */
  _cleanup: function Channel_cleanup() {
    this._removeFromLoadGroup();
    this.loadGroup = null;

    this._isPending = false;

    delete this._listener;
    delete this._context;
  },

  /* :::::::: nsIChannel ::::::::::::::: */

  contentType: PageThumbs.contentType,
  contentLength: -1,
  owner: null,
  contentCharset: null,
  notificationCallbacks: null,

  get URI() this._uri,
  get securityInfo() null,

  /**
   * Opens this channel synchronously. Not supported.
   */
  open: function Channel_open() {
    // Synchronous data delivery is not implemented.
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  /* :::::::: nsIHttpChannel ::::::::::::::: */

  redirectionLimit: 10,
  requestMethod: "GET",
  allowPipelining: true,
  referrer: null,

  get requestSucceeded() true,

  _responseStatus: 200,
  get responseStatus() this._responseStatus,

  _responseText: "OK",
  get responseStatusText() this._responseText,

  /**
   * Checks if the server sent the equivalent of a "Cache-control: no-cache"
   * response header.
   * @return Always false.
   */
  isNoCacheResponse: function () false,

  /**
   * Checks if the server sent the equivalent of a "Cache-control: no-cache"
   * response header.
   * @return Always false.
   */
  isNoStoreResponse: function () false,

  /**
   * Returns the value of a particular request header. Not implemented.
   */
  getRequestHeader: function Channel_getRequestHeader() {
    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  /**
   * This method is called to set the value of a particular request header.
   * Not implemented.
   */
  setRequestHeader: function Channel_setRequestHeader() {
    if (this._wasOpened)
      throw Cr.NS_ERROR_IN_PROGRESS;
  },

  /**
   * Call this method to visit all request headers. Not implemented.
   */
  visitRequestHeaders: function () {},

  /**
   * Gets the value of a particular response header.
   * @param aHeader The case-insensitive name of the response header to query.
   * @return The header value.
   */
  getResponseHeader: function Channel_getResponseHeader(aHeader) {
    let name = aHeader.toLowerCase();
    if (name in this._responseHeaders)
      return this._responseHeaders[name];

    throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  /**
   * This method is called to set the value of a particular response header.
   * @param aHeader The case-insensitive name of the response header to query.
   * @param aValue The response header value to set.
   */
  setResponseHeader: function Channel_setResponseHeader(aHeader, aValue, aMerge) {
    let name = aHeader.toLowerCase();
    if (!aValue && !aMerge)
      delete this._responseHeaders[name];
    else
      this._responseHeaders[name] = aValue;
  },

  /**
   * Call this method to visit all response headers.
   * @param aVisitor The header visitor.
   */
  visitResponseHeaders: function Channel_visitResponseHeaders(aVisitor) {
    for (let name in this._responseHeaders) {
      let value = this._responseHeaders[name];

      try {
        aVisitor.visitHeader(name, value);
      } catch (e) {
        // The visitor can throw to stop the iteration.
        return;
      }
    }
  },

  /* :::::::: nsIRequest ::::::::::::::: */

  loadFlags: Ci.nsIRequest.LOAD_NORMAL,
  loadGroup: null,

  get name() this._uri.spec,

  _status: Cr.NS_OK,
  get status() this._status,

  _isPending: false,
  isPending: function () this._isPending,

  resume: function () {},
  suspend: function () {},

  /**
   * Cancels this request.
   * @param aStatus The reason for cancelling.
   */
  cancel: function Channel_cancel(aStatus) {
    if (this.canceled)
      return;

    this._isCanceled = true;
    this._status = aStatus;

    this._cleanup();
  },

  /* :::::::: nsIHttpChannelInternal ::::::::::::::: */

  documentURI: null,

  _isCanceled: false,
  get canceled() this._isCanceled,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannel,
                                         Ci.nsIHttpChannel,
                                         Ci.nsIHttpChannelInternal,
                                         Ci.nsIRequest])
};

/**
 * Parses a given uri and extracts all parameters relevant to this protocol.
 * @param aUri The URI to parse.
 * @return The parsed parameters.
 */
function parseURI(aURI) {
  let params = {};
  let [host, query] = aURI.spec.split("?");

  query.split("&").forEach(function (param) {
    let [key, value] = param.split("=").map(decodeURIComponent);
    params[key.toLowerCase()] = value;
  });

  params.algorithm = host.replace(/^.*\/+/, "").replace(/\/+.*$/, "");

  return params;
}
