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

let EXPORTED_SYMBOLS = ["PageThumbs", "PageThumbsCache"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

const HTML_NAMESPACE = "http://www.w3.org/1999/xhtml";

/**
 * The default width for page thumbnails.
 */
const THUMBNAIL_WIDTH = 201;

/**
 * The default height for page thumbnails.
 */
const THUMBNAIL_HEIGHT = 127;

/**
 * The default background color for page thumbnails.
 */
const THUMBNAIL_BG_COLOR = "#fff";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gAppShellService",
  "@mozilla.org/appshell/appShellService;1", "nsIAppShellService");

XPCOMUtils.defineLazyServiceGetter(this, "gCacheService",
  "@mozilla.org/network/cache-service;1", "nsICacheService");

/**
 * Singleton providing functionality for capturing web page thumbnails and for
 * accessing them if already cached.
 */
let PageThumbs = {
  /**
   * The scheme to use for thumbnail urls.
   */
  get scheme() "moz-page-thumb",

  /**
   * The thumbnails' image type.
   */
  get contentType() "image/png",

  /**
   * Gets the thumbnail image's url for a given web page's url.
   * @param aUrl The web page's url that is depicted in the thumbnail.
   * @param aWidth The thumbnail's width.
   * @param aHeight The thumbnail's height.
   * @return The thumbnail image's url.
   */
  getThumbnailForUrl:
    function PageThumbs_getThumbnailForUrl(aUrl, aWidth, aHeight) {

    return this.scheme + "://default" +
           "?url=" + encodeURIComponent(aUrl) +
           "&width=" + encodeURIComponent(aWidth) +
           "&height=" + encodeURIComponent(aHeight);
  },

  /**
   * Captures a thumbnail for the given tab.
   * @tab aTab The tab to capture a thumbnail from.
   */
  capture: function PageThumbs_capture(aTab) {
    let contentWindow = aTab.linkedBrowser.contentWindow;
    let [sx, sy, sw, sh, scale] = this._determineCropRectangle(contentWindow);

    let canvas = this._createCanvas();
    let ctx = canvas.getContext("2d");

    // Scale the canvas accordingly.
    ctx.scale(scale, scale);

    // Draw the window contents to the canvas.
    ctx.drawWindow(contentWindow, sx, sy, sw, sh, THUMBNAIL_BG_COLOR,
                   ctx.DRAWWINDOW_DO_NOT_FLUSH);

    // Store the capture thumbnail in the file cache.
    this._store(aTab, canvas);
  },

  /**
   * Stores the image data belonging to the given tab and contained in the
   * given canvas to the file cache.
   * @param aTab The tab whose content is depicted in the thumbnail.
   * @param aCanvas The canvas containing the thumbnail's image data.
   */
  _store: function PageThumbs_store(aTab, aCanvas) {
    let key = aTab.linkedBrowser.currentURI.spec;
    let self = this;

    // Get a writeable cache entry.
    PageThumbsCache.getWriteEntry(key, function (aEntry) {
      if (!aEntry)
        return;

      // Extract image data from the canvas.
      self._readImageData(aCanvas, function (aData) {
        let outputStream = aEntry.openOutputStream(0);

        // Write the image data to the cache entry.
        NetUtil.asyncCopy(aData, outputStream, function (aResult) {
          if (Components.isSuccessCode(aResult))
            aEntry.markValid();

          aEntry.close();
        });
      });
    });
  },

  /**
   * Reads the image data from a given canvas and passes it to the callback.
   * @param aCanvas The canvas to read the image data from.
   * @param aCallback The callbak that the data is passed.
   */
  _readImageData: function PageThumbs_readImageData(aCanvas, aCallback) {
    let dataUri = aCanvas.toDataURL(PageThumbs.contentType, "");
    let uri = Services.io.newURI(dataUri, "UTF8", null);

    NetUtil.asyncFetch(uri, function (aData, aResult) {
      if (Components.isSuccessCode(aResult) && aData && aData.available())
        aCallback(aData);
    });
  },

  /**
   * Determines the crop rectangle for a given content window.
   * @param aWindow The content window.
   * @return An array containing x, y, width, heigh and the scale of the crop
   *         rectangle.
   */
  _determineCropRectangle: function PageThumbs_determineCropRectangle(aWindow) {
    let sx = 0;
    let sy = 0;
    let sw = aWindow.innerWidth;
    let sh = aWindow.innerHeight;

    let scale = Math.max(THUMBNAIL_WIDTH / sw, THUMBNAIL_HEIGHT / sh);
    let scaledWidth = sw * scale;
    let scaledHeight = sh * scale;

    if (scaledHeight > THUMBNAIL_HEIGHT) {
      sy = Math.floor(Math.abs((scaledHeight - THUMBNAIL_HEIGHT) / 2) / scale);
      sh -= 2 * sy;
    }

    if (scaledWidth > THUMBNAIL_WIDTH) {
      sx = Math.floor(Math.abs((scaledWidth - THUMBNAIL_WIDTH) / 2) / scale);
      sw -= 2 * sx;
    }

    return [sx, sy, sw, sh, scale];
  },

  /**
   * Creates a new hidden canvas element.
   * @return The newly created canvas.
   */
  _createCanvas: function PageThumbs_createCanvas() {
    let doc = gAppShellService.hiddenDOMWindow.document;
    let canvas = doc.createElementNS(HTML_NAMESPACE, "canvas");
    canvas.mozOpaque = true;
    canvas.width = THUMBNAIL_WIDTH;
    canvas.height = THUMBNAIL_HEIGHT;
    return canvas;
  }
};

/**
 * A singleton handling the storage of page thumbnails.
 */
let PageThumbsCache = {
  /**
   * Calls the given callback with a cache entry opened for reading.
   * @param aKey The key identifying the desired cache entry.
   * @param aCallback The callback that is called when the cache entry is ready.
   */
  getReadEntry: function Cache_getReadEntry(aKey, aCallback) {
    let self = this;

    // Try to open the desired cache entry.
    this._openCacheEntry(aKey, Ci.nsICache.ACCESS_READ, function (aEntry) {
      if (!self._isValidReadEntry(aEntry))
        aEntry = null;

      aCallback(aEntry);
    });
  },

  /**
   * Calls the given callback with a cache entry opened for writing.
   * @param aKey The key identifying the desired cache entry.
   * @param aCallback The callback that is called when the cache entry is ready.
   */
  getWriteEntry: function Cache_getWriteEntry(aKey, aCallback) {
    // Try to open the desired cache entry.
    this._openCacheEntry(aKey, Ci.nsICache.ACCESS_WRITE, aCallback);
  },

  /**
   * Determines whether the given cache entry is valid and can be read from.
   * @param aEntry The cache entry to verify.
   * @return Whether the cache entry is valid.
   */
  _isValidReadEntry: function Cache_isValidReadEntry(aEntry) {
    if (!aEntry)
      return false;

    let inputStream = aEntry.openInputStream(0);

    // check if the entry actually contains data
    return inputStream && inputStream.available();
  },

  /**
   * Opens the cache entry identified by the given keys
   * @param aKey The key identifying the desired cache entry.
   * @param aAccess The desired access mode.
   * @param aCallback The callback to be called when the cache entry was opened.
   */
  _openCacheEntry: function Cache_openCacheEntry(aKey, aAccess, aCallback) {
    function onCacheEntryAvailable(aEntry, aAccessGranted, aStatus) {
      let validAccess = aAccess == aAccessGranted;
      let validStatus = Components.isSuccessCode(aStatus);

      // Check if a valid entry was passed and if the
      // access we requested was actually granted.
      if (aEntry && !(validAccess && validStatus)) {
        aEntry.close();
        aEntry = null;
      }

      aCallback(aEntry);
    }

    let listener = this._createCacheListener(onCacheEntryAvailable);
    this._session.asyncOpenCacheEntry(aKey, aAccess, listener);
  },

  /**
   * Returns a cache listener implementing the nsICacheListener interface.
   * @param aCallback The callback to be called when the cache entry is available.
   * @return The new cache listener.
   */
  _createCacheListener: function Cache_createCacheListener(aCallback) {
    return {
      onCacheEntryAvailable: aCallback,
      QueryInterface: XPCOMUtils.generateQI([Ci.nsICacheListener])
    };
  }
};

/**
 * Define a lazy getter for the cache session.
 */
XPCOMUtils.defineLazyGetter(PageThumbsCache, "_session", function () {
  return gCacheService.createSession(PageThumbs.scheme,
                                     Ci.nsICache.STORE_ON_DISK, true);
});
