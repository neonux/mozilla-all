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

// the cache client identifier to separate our cache content from others
const CACHE_CLIENT_ID = "moz-thumb";

// ##########
// Class: Cache
// Singleton providing access to the internal cache.
let Cache = {
  // lazy getter for the cache session
  get _cacheSession() {
    let session = gCacheService.createSession(CACHE_CLIENT_ID,
                                              Ci.nsICache.STORE_ON_DISK, true);

    delete this._cacheSession;
    return this._cacheSession = session;
  },

  // ----------
  // Function: getReadEntry
  // Calls the given <aCallback> with a cache entry opened for reading.
  //
  // Parameters:
  //   aKey - the key identifying the desired cache entry
  //   aCallback - the callback that is called when the cache entry is ready
  getReadEntry: function Cache_getReadEntry(aKey, aCallback) {
    let self = this;

    // try to open the desired cache entry
    this._openCacheEntry(aKey, Ci.nsICache.ACCESS_READ, function (aEntry) {
      if (!self._isValidReadEntry(aEntry))
        aEntry = null;

      aCallback(aEntry);
    });
  },

  // ----------
  // Function: getWriteEntry
  // Calls the given <aCallback> with a cache entry opened for writing.
  //
  // Parameters:
  //   aKey - the key identifying the desired cache entry
  //   aCallback - the callback that is called when the cache entry is ready
  getWriteEntry: function Cache_getWriteEntry(aKey, aCallback) {
    // try to open the desired cache entry
    this._openCacheEntry(aKey, Ci.nsICache.ACCESS_WRITE, aCallback);
  },

  // ----------
  // Function: _isValidReadEntry
  // Returns whether the given cache entry is valid and can be read from.
  //
  // Parameters:
  //   aEntry - the cache entry to verify
  _isValidReadEntry: function Cache__isValidReadEntry(aEntry) {
    if (!aEntry)
      return false;

    let inputStream = aEntry.openInputStream(0);

    // check if the entry actually contains data
    return inputStream && inputStream.available();
  },

  // ----------
  // Function: _openCacheEntry
  // Opens the cache entry identified by the given <aKey>.
  //
  // Parameters:
  //   aKey - the key identifying the desired cache entry
  //   aAccess - the desired access mode
  //   aCallback - the callback to be called when the cache entry was opened
  _openCacheEntry: function Cache__openCacheEntry(aKey, aAccess, aCallback) {
    function onCacheEntryAvailable(aEntry, aAccessGranted, aStatus) {
      let validAccess = (aAccess == aAccessGranted);
      let validStatus = Components.isSuccessCode(aStatus);

      // check if a valid entry was passed and if the access we requested
      // was actually granted
      if (aEntry && !(validAccess && validStatus)) {
        aEntry.close();
        aEntry = null;
      }

      aCallback(aEntry);
    }

    let listener = this._createCacheListener(onCacheEntryAvailable);
    this._cacheSession.asyncOpenCacheEntry(aKey, aAccess, listener);
  },

  // ----------
  // Function: _createCacheListener
  // Returns a cache listener implementing the nsICacheListener interface.
  //
  // Parameters:
  //   aCallback - the callback to be called when the cache entry is available
  _createCacheListener: function Cache__createCacheListener(aCallback) {
    return {
      onCacheEntryAvailable: aCallback,
      QueryInterface: XPCOMUtils.generateQI([Ci.nsICacheListener])
    };
  }
};
