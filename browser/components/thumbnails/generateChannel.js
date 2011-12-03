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

// ##########
// Class: GenerateChannel
// A http channel implementation that creates a thumbnail for the requested URI
// delivers it and stores it in the cache.
let GenerateChannel = {
  // ----------
  // Function: extend
  // Extends the given channel with our custom functionality.
  //
  // Parameters:
  //   aChannel - the channel to extend
  extend: function GC_extend(aChannel) {
    for (let name in this)
      if (name != "extend")
        aChannel[name] = this[name];

    aChannel._serve();
  },

  // ----------
  // Function: _serve
  // Serves the thumbnails's data and writes it to the entry's output stream.
  _serve: function GC__serve() {
    let self = this;

    Cache.getWriteEntry(this._cacheKey, function (aEntry) {
      // store the entry so that _cleanup can close it
      self._entry = aEntry;

      // we might've been canceled while waiting for the entry
      if (self.canceled)
        return;

      // for some reason we don't have a valid entry
      if (!aEntry) {
        self.cancel(Cr.NS_ERROR_UNEXPECTED);
        return;
      }

      // the listener might cancel when starting the request
      if (!self._startRequest())
        return;

      self._addToLoadGroup();

      // create our thumbnail and wait for its data
      self._createThumbnail(function (aDataStream, aStatusCode) {
        // store thumbnails in the cache only on success
        if (self._isSuccessStatusCode(aStatusCode))
          self._prepareCacheEntry(aEntry, aDataStream);

        // serve the image data to the listener
        if (self._serveData(aDataStream))
          self._stopRequest();
      });
    });
  },

  // ----------
  // Function: _isSuccessStatusCode
  // Returns whether the given status code denotes a success.
  //
  // Parameters:
  //   aStatusCode - the http status code to check
  _isSuccessStatusCode: function GC__isSuccessStatusCode(aStatusCode) {
    // return true for all 2xx status codes
    return Math.floor(aStatusCode / 100) == 2;
  },

  // ----------
  // Function: _createThumbnail
  // Creates a thumbnail and calls the given callback with a data stream and a
  // http status code.
  //
  // Parameters:
  //   aCallback - the callback to be called with the thumbnail's data stream
  //               and its http status code
  _createThumbnail: function GC__createThumbnail(aCallback) {
    let self = this;
    let {url, width, height} = parseUri(this._uri);

    // create a new thumbnail
    Thumbnailer.create(url, width, height, function (aDataStream, aStatusCode) {
      if (aDataStream && aDataStream.available())
        // valid thumbnail data
        aCallback(aDataStream, aStatusCode);
      else
        // failed to get the thumbnail data
        self.cancel(Cr.NS_ERROR_UNEXPECTED);
    });
  },

  // ----------
  // Function: _prepareCacheEntry
  // Prepares a given cache entry so that we can write the thumbnail data
  // to its output stream.
  //
  // Parameters:
  //   aEntry - the entry we prepare to be written to
  //   aDataStream - the data stream that is written to the given entry
  _prepareCacheEntry: function GC__prepareCacheEntry(aEntry, aDataStream) {
    // create a tee listener to simultaneously deliver data to the
    // stream listener and to the cache entry's stream
    this._useWriteEntryTeeListener(aEntry);

    // store content-length header
    let available = "" + aDataStream.available();
    aEntry.setMetaDataElement("content-length", available);
  },

  // ----------
  // Function: _useWriteEntryTeeListener
  // Replaces the current listener with a TeeListener implementation that
  // simultaneously delivers the entry and writes it to the cache entry.
  //
  // Parameters:
  //   aEntry - the entry to whose data stream we write simultaneously
  _useWriteEntryTeeListener: function GC__useWriteEntryTeeListener(aEntry) {
    let tee = Cc["@mozilla.org/network/stream-listener-tee;1"]
              .createInstance(Ci.nsIStreamListenerTee);

    let outputStream = aEntry.openOutputStream(0);
    tee.init(this._listener, outputStream);

    this._listener = tee;
  }
};
