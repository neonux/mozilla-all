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

// TODO
function CacheChannel(aUri, aEntry) {
  this._entry = aEntry;

  try {
    let contentLength = aEntry.getMetaDataElement("content-length");
    this.setResponseHeader("content-length", contentLength);
  } catch (e) {
    // throws if the meta data element doesn't exist, ignore
  }

  this._setup(aUri);
}

CacheChannel.prototype = extend(BaseChannel, {
  asyncOpen: function CCH_asyncOpen(aListener, aContext) {
    if (this._wasOpened)
      throw Cr.NS_ERROR_ALREADY_OPENED;

    if (this.canceled)
      return;

    this._listener = aListener;
    this._context = aContext;

    this._isPending = true;
    this._wasOpened = true;

    this._serveEntry();
  },

  cancel: function CCH__cancel(aStatus) {
    if (this.canceled)
      return;

    BaseChannel.cancel.call(this, aStatus);

    this._cleanup();
  },

  _serveEntry: function CCH__serveEntry() {
    let self = this;
    let listener = this._listener;
    let context = this._context;

    this._readEntry(function (aDataStream) {
      self._dataStream = aDataStream;

      if (self._startRequest()) {
        self._addToLoadGroup();

        if (self._serveData(aDataStream))
          self._stopRequest();
      }
    });
  },

  _readEntry: function CCH__readEntry(aCallback) {
    let self = this;

    this._readEntryData(function (aDataStream) {
      if (self.canceled)
        return;

      // failed to get the thumbnail data
      if (!aDataStream) {
        self.cancel(Cr.NS_ERROR_UNEXPECTED);
        return;
      }

      aCallback(aDataStream);
    });
  },

  _readEntryData: function CCH__readEntryData(aCallback) {
    let inputStream = this._entry.openInputStream(0);

    gNetUtil.asyncFetch(inputStream, function (aDataStream, aStatus, aRequest) {
      inputStream.close();

      if (!Components.isSuccessCode(aStatus) || !aDataStream.available())
        aDataStream = null;

      aCallback(aDataStream);
    });
  }
});
