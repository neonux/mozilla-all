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

// TODO
function GenerateChannel(aUri) {
  this._setup(aUri);
}

GenerateChannel.prototype = extend(BaseChannel, {
  asyncOpen: function GCH_asyncOpen(aListener, aContext) {
    if (this._wasOpened)
      throw Cr.NS_ERROR_ALREADY_OPENED;

    if (this.canceled)
      return;

    this._listener = aListener;
    this._context = aContext;

    this._isPending = true;
    this._wasOpened = true;

    let self = this;

    Cache.getWriteEntry(this.name, function (aEntry) {
      // we might've been canceled while waiting for the entry
      if (self.canceled)
        return;

      // for some reason we don't have a valid entry
      if (!aEntry) {
        self.cancel(Cr.NS_ERROR_UNEXPECTED);
        return;
      }

      self._createAndServeThumbnail(aEntry);
    });
  },

  _createAndServeThumbnail: function GCH__createAndServeThumbnail(aEntry) {
    if (!this._startRequest())
      return;

    this._addToLoadGroup();

    let self = this;
    let {url, width, height} = new URLParser(this._uri).parse();

    // create a new thumbnail
    Thumbnailer.create(url, width, height, function (aDataStream, aStatusCode) {
      // failed to get the thumbnail data
      if (!aDataStream || !aDataStream.available()) {
        self.cancel(Cr.NS_ERROR_UNEXPECTED);
        return;
      }

      // store thumbnails in the cache only for 2xx status codes
      if (Math.floor(aStatusCode / 100) == 2)
        self._prepareCacheEntry(aEntry, aDataStream);

      if (self._serveData(aDataStream))
        self._stopRequest();
    });
  },

  _prepareCacheEntry: function GHC__prepareCacheEntry(aEntry, aDataStream) {
    // create a tee listener to simultaneously deliver data to the
    // stream listener and to the cache entry's stream
    this._useWriteEntryTeeListener(aEntry);

    // store content-length header
    let available = "" + aDataStream.available();
    aEntry.setMetaDataElement("content-length", available);
  },

  _useWriteEntryTeeListener: function GCH__useWriteEntryTeeListener(aEntry) {
    let tee = Cc["@mozilla.org/network/stream-listener-tee;1"]
              .createInstance(Ci.nsIStreamListenerTee);

    let outputStream = aEntry.openOutputStream(0);
    tee.init(this._listener, outputStream);

    this._listener = tee;
  }
});

// TODO
function URLParser(aUri) {
  this._uri = aUri;
}

URLParser.prototype = {
  parse: function UP_parse() {
    if (this._parsed)
      return this._parsed;

    return this._parsed = this._parseParams();
  },

  _parseParams: function UP_parseParams() {
    let params = {};
    let query = this._uri.spec.split("?")[1] || "";

    query.split("&").forEach(function (param) {
      let [key, value] = param.split("=").map(decodeURIComponent);
      params[key.toLowerCase()] = value;
    });

    return params;
  }
};
