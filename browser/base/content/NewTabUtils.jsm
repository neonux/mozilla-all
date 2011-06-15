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
 * The Original Code is New Tab Page code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Margaret Leibovic <margaret.leibovic@gmail.com> (Original Author)
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

var EXPORTED_SYMBOLS = ["NewTabUtils"];

let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "FileUtils", function() {
  Cu.import("resource://gre/modules/FileUtils.jsm");
  return FileUtils;
});

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

let NewTabUtils = {

  SITES_FILE: "newtab/sites.json",

  _obsInitialized: false,

  // an array of sites when initialized
  _sites: [],
  _lastUpdated: 0,

  _savedSitesLoaded: false,

  _callbacks: [],
  _searchingForSites: false,

  _faviconColors: {},

  // pay attention to changes in browser history, specifically deleting/forgetting sites
  initObservers: function NTU_addHistoryObserver() {
    // Invalidate the cache if the user explicitly deletes sites.
    let observer = {
      onBeginUpdateBatch: function() {},
      onEndUpdateBatch: function() {},
      onClearHistory: function() {
        NewTabUtils._lastUpdated = 0;
      },
      onVisit: function() {},
      onTitleChanged: function() {},
      onBeforeDeleteURI: function() {},
      onDeleteURI: function() {
        NewTabUtils._lastUpdated = 0;
      },
      onPageChanged: function() {},
      onDeleteVisits: function() {
        NewTabUtils._lastUpdated = 0;
      }
    };

    let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
             getService(Ci.nsINavHistoryService);
    hs.addObserver(observer, false);

    this._obsInitialized = true;
  },

  // aCallback takes an ordered array of sites as a param
  getSites: function NTU_getSites(aCallback) {
    if (!this._obsInitialized) {
      this.initObservers();
    }

    // Use sites stored in memory if we have them
    if (this._sites.length) {
      // Refresh sites if it's been more than a day since we updated them
      if (this._lastUpdated + 86400000 < Date.now()) {
        this.refreshSites(aCallback);
        return;
      }    

      // Otherwise, just use the sites we got
      aCallback(this._sites);
      return;
    }

    // Load sites from disk if we have some stored there
    if (this.savedSitesExist) {
      this.loadSavedSites(aCallback);
      return;
    }

    // If we don't have any stored data about sites, just get them all from history AC    
    this.getSitesFromHistory(aCallback);
  },

  get savedSitesExist() {
    if (this._savedSitesExist) {
      return true;
    }

    let file = FileUtils.getFile("ProfD", this.SITES_FILE.split("/"), true);
    this._savedSitesExist = file.exists();
    return this._savedSitesExist;
  },

  saveSites: function NTU_saveSites() {
    let file = FileUtils.getFile("ProfD", this.SITES_FILE.split("/"), true);    
    let fos = FileUtils.openSafeFileOutputStream(file);
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    try {
      let ins = converter.convertToInputStream(JSON.stringify(this._sites));
      NetUtil.asyncCopy(ins, fos, function(aResultCode) {
        if (Components.isSuccessCode(aResultCode)) {
          NewTabUtils._lastSaved = Date.now();    
        } else {
          Cu.reportError("NewTabUtils [saveSites]: " + aResultCode);
        }
      });
    } catch (e) {
      Cu.reportError("NewTabUtils [saveSites]: " + e);
    }
  },

  // loadSavedSites assumes a file of saved sites exists and will report an error if it does not
  // aCallback takes an ordered array of sites as a param
  loadSavedSites: function NTU_loadSavedSites(aCallback) {
    let file = FileUtils.getFile("ProfD", this.SITES_FILE.split("/"), true);
    if (!file.exists()) {
      Cu.reportError("NewTabUtils [loadSavedSites]: file should exist but it does not");
      return;
    }

    try {
      let channel = NetUtil.newChannel(file);
      channel.contentType = "application/json";

      NetUtil.asyncFetch(channel, function(aInputStream, aResultCode) {
        if (Components.isSuccessCode(aResultCode)) {
          NewTabUtils._sites = Cc["@mozilla.org/dom/json;1"].
                               createInstance(Ci.nsIJSON).
                               decodeFromStream(aInputStream, aInputStream.available());
          aCallback(NewTabUtils._sites);
        } else {
          Cu.reportError("NewTabUtils [loadSavedSites]: " + aResultCode);
        }
      });
    } catch (e) {
      Cu.reportError("NewTabUtils [loadSavedSites]: " + e);
    }
  },

  // Get top sites from places autocomplete
  // aCallback takes an ordered array of sites as a param
  getSitesFromHistory: function NTU_getSitesFromHistory(aCallback) {
    // Don't start multiple autocomplete searches
    if (this._searchingForSites) {
      this._callbacks.push(aCallback);
      return;   
    }
    this._searchingForSites = true;

    let listener = {
      onSearchResult: function(aSearch, aResult) {
        if (aResult.searchResult != Ci.nsIAutoCompleteResult.RESULT_SUCCESS) {
          return;
        }
        let sites = [];
        for (let i = 0; i < aResult.matchCount; i++) {
          let url = aResult.getValueAt(i);
          let site = {
            url: url,
            host: Services.io.newURI(url, null, null).host,
            favicon: aResult.getImageAt(i),
            title: aResult.getCommentAt(i)
          };
          sites.push(site);
        }

        aCallback(sites);
        while (NewTabUtils._callbacks.length) {
          let callback = NewTabUtils._callbacks.shift();
          callback(sites);
        }
  
        NewTabUtils._sites = sites;
        NewTabUtils._searchingForSites = false;

        NewTabUtils._lastUpdated = Date.now();
        NewTabUtils.saveSites();
      },
      onUpdateSerachResult: function(aSearch, aResult) { }
    }

    let historyACSearch = Cc["@mozilla.org/autocomplete/search;1?name=history"].
                          getService(Ci.nsIAutoCompleteSearch);
    historyACSearch.startSearch("", "", null, listener);
  },

  // aCallback takes an ordered array of sites as a param
  refreshSites: function NTU_refreshSites(aCallback) {
    // check sites to see which ones the user wants to keep
    
    // go fetch new sites to fill up the extra spots if there are some
    
    // make sure we don't refill with blacklisted sites
    
    aCallback(this._sites);
  },

  // aSiteIndex is the index of the site to save from the site array
  keepSite: function NTU_keepSite(aSiteIndex) {
    this._sites[aSiteIndex].keep = true;
    this.saveSites();
  },

  unKeepSite: function NTU_unKeepSite(aSiteIndex) {
    delete this._sites[aSiteIndex].keep;
    this.saveSites();
  },

  // aSiteIndex is the index of the site to delete from the site array
  removeSite: function NTU_removeSite(aSiteIndex) {
    this._sites.splice(aSiteIndex, 1);
    this.saveSites();
  },

  // TODO: this API doesn't necessarily make sense for whatever UI we will choose to use
  // aCallback takes the added site as a param
  addSite: function NTU_addSite(aSiteURL, aCallback) {
    let siteURI = Services.io.newURI(aSiteURL, null, null);
    function faviconDataCallback(aFaviconURI, aDataLen, aData, aMimeType) {
      let site = {
        url: aSiteURL,
        host: siteURI.host,
        favicion: aFaviconURI.spec
      };
      NewTabUtils._sites.push(site);
      aCallback(site);
    }

    if (!this._faviconService) {
      this._faviconService = Cc["@mozilla.org/browser/favicon-service;1"].
                             getService(Ci.nsIFaviconService);
    }
    this._faviconService.getFaviconURLForPage(siteURI, faviconDataCallback);
    this.saveSites();
  },

  getCachedFaviconColor: function NTU_getCachedFaviconColor(aFaviconImg) {
    return this._faviconColors[aFaviconImg.src];
  },

  getFaviconColor: function NTU_getFaviconColor(aFaviconImg, aDocument) {
    let canvas = aDocument.createElement("canvas");
    canvas.height = canvas.width = 16;

    let context = canvas.getContext("2d");
    context.drawImage(aFaviconImg, 0, 0);

    // keep track of how many times a color appears in the image
    let colorCount = {};
    let maxCount = 0;
    let faviconColor = "";

    // data is an array of a series of 4 one-byte values representing the rgba values of each pixel
    let data = context.getImageData(0, 0, 16, 16).data;
    for (let i = 0; i < data.length; i += 4) {
      // ignore transparent pixels
      if (data[i+3] == 0) {
        continue;
      }

      let color = data[i] + "," + data[i+1] + "," + data[i+2];
      // ignore white
      if (color == "255,255,255") {
        continue;
      }

      colorCount[color] = colorCount[color] ?  colorCount[color] + 1 : 1;

      // keep track of the color that appears the most times
      if (colorCount[color] > maxCount) {
        maxCount = colorCount[color];
        faviconColor = color;
      }
    }

    this._faviconColors[aFaviconImg.src] = faviconColor;
    return faviconColor;
  }
};
