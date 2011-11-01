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
// Class: CacheChannel
// A http channel implementation that delivers a thumbnail specified by the
// given URI from the image cache.
let CacheChannel = {
  // ----------
  // Function: extend
  // Extends the given channel with our custom functionality.
  //
  // Parameters:
  //   aChannel - the channel to extend
  //   aEntry - the cache entry we read from
  extend: function CC_extend(aChannel, aEntry) {
    for (let name in this)
      if (name != "extend")
        aChannel[name] = this[name];

    try {
      // set the content-length header stored in the entry's meta data
      let contentLength = aEntry.getMetaDataElement("content-length");
      aChannel.setResponseHeader("content-length", contentLength);
    } catch (e) {
      // throws if the meta data element doesn't exist, ignore
    }

    aChannel._entry = aEntry;
    aChannel._serve();
  },

  // ----------
  // Function: _serve
  // Serves data from the cache entry to the channel listener.
  _serve: function CC__serve() {
    let self = this;

    this._readEntry(function (aDataStream) {
      self._dataStream = aDataStream;

      if (!self._startRequest())
        return;

      self._addToLoadGroup();

      if (self._serveData(aDataStream))
        self._stopRequest();
    });
  },

  // ----------
  // Function: _readEntry
  // Reads a data stream from the cache entry.
  //
  // Parameters:
  //   aCallback - the callback the data is passed to
  _readEntry: function CC__readEntry(aCallback) {
    let self = this;
    let inputStream = this._entry.openInputStream(0);

    gNetUtil.asyncFetch(inputStream, function (aDataStream, aStatus, aRequest) {
      inputStream.close();

      // we might have been canceled while waiting
      if (self.canceled)
        return;

      // check if we have a valid data stream
      if (!Components.isSuccessCode(aStatus) || !aDataStream.available()) {
        self.cancel(Cr.NS_ERROR_UNEXPECTED);
        return;
      }

      aCallback(aDataStream);
    });
  }
};
