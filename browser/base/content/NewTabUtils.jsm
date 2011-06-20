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

XPCOMUtils.defineLazyServiceGetter(this, "gHistoryACSearch",
                                   "@mozilla.org/autocomplete/search;1?name=history",
                                   "nsIAutoCompleteSearch");

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
  BLOCKED_SITES_FILE: "newtab/blockedSites.json",

  // mapping of url -> site
  _sites: null,
  _lastUpdated: 0,

  // mapping of url -> boolean
  _blockedSites: {},
  _blockedSitesLoaded: false,

  _obsInitialized: false,

  _faviconColors: {},

  // pay attention to changes in browser history, specifically deleting/forgetting sites
  // TODO: clean up blockedSites when a site is forgotten 
  initObservers: function NTU_initObservers() {
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

  // aCallback takes a site object as a param
  getSites: function NTU_getSites(aCallback) {
    if (!this._obsInitialized) {
      this.initObservers();
    }

    // Use sites stored in memory if we have them
    if (this._sites) {
      // Refresh sites if it's been more than a day since we updated them
      if (this._lastUpdated + 86400000 < Date.now()) {
        this.refreshSites(aCallback);
        return;
      }    

      // Otherwise, just use the sites we got
      for each (let site in this._sites) {
        aCallback(site);
      }
      return;
    }

    // Otherwise load sites from disk
    this.loadSites(aCallback);
  },

  // aCallback takes a site object as a param
  loadSites: function NTU_loadSites(aCallback) {
    this.loadFromFile(this.SITES_FILE, function(sites) {
      // Users who have run an older build might have a site array stored in their
      // profile instead of an object, so we should check !sites.length
      if (sites && !sites.length) {
        for each (let site in sites) {
          aCallback(site);
        }
        NewTabUtils._sites = sites;

        // If there were saved sites, there may be blocked sites as well
        NewTabUtils.loadBlockedSites();
        return;
      }

      // If we don't have any sites stored on disk, just get them all from history AC    
      NewTabUtils.getSitesFromHistory(aCallback);
    });
  },

  loadBlockedSites: function NTU_loadBlockedSites() {
    this.loadFromFile(this.BLOCKED_SITES_FILE, function(blockedSites) {
      if (blockedSites) {
        NewTabUtils._blockedSites = blockedSites;
      }
      NewTabUtils._blockedSitesLoaded = true;
    });  
  },

  // Get top sites from places autocomplete
  // aCallback takes a site object as a param
  getSitesFromHistory: function NTU_getSitesFromHistory(aCallback) {
    this._sites = {};
    let siteCount = 0;
    let listener = {
      onSearchResult: function(aSearch, aResult) {        
        for (siteCount; siteCount < aResult.matchCount; siteCount++) {
          let url = aResult.getValueAt(siteCount);
          let site = {
            url: url,
            host: Services.io.newURI(url, null, null).host,
            favicon: aResult.getImageAt(siteCount),
            title: aResult.getCommentAt(siteCount)
          };
          aCallback(site);
          NewTabUtils._sites[site.url] = site;
        }

        if (aResult.searchResult == Ci.nsIAutoCompleteResult.RESULT_SUCCESS) {
          NewTabUtils._lastUpdated = Date.now();
          NewTabUtils.saveSites();
        }
      },
      onUpdateSearchResult: function(aSearch, aResult) { }
    }
    gHistoryACSearch.startSearch("", "", null, listener);
  },

  // aCallback takes a site object as a param
  refreshSites: function NTU_refreshSites(aCallback) {
    let sites = {};
    let totalSiteCount = 0;
    // Start by adding pinned sites
    for each (let site in this._sites) {
      if (site.pinned) {
        aCallback(site);
        sites[site.url] = site;
        totalSiteCount++;
      }
    }
    this._sites = sites;
    
    // go fetch new sites to fill up the extra spots if there are some
    let siteCount = 0;
    let listener = {
      onSearchResult: function(aSearch, aResult) {
        for (siteCount; siteCount < aResult.matchCount && totalSiteCount < 12; siteCount++) {
          let url = aResult.getValueAt(siteCount);
          // don't add site that already exists or is in blacklist
          if (NewTabUtils._sites[url] || NewTabUtils._blockedSites[url]) {
            continue;
          }
          let site = {
            url: url,
            host: Services.io.newURI(url, null, null).host,
            favicon: aResult.getImageAt(siteCount),
            title: aResult.getCommentAt(siteCount)
          };
          aCallback(site);
          NewTabUtils._sites[site.url] = site;
          totalSiteCount++;
        }
  
        // Stop when we have 12 sites or when we've run out of sites
        if (totalSiteCount > 11 || aResult.searchResult == Ci.nsIAutoCompleteResult.RESULT_SUCCESS) {
          gHistoryACSearch.stopSearch();
          NewTabUtils._lastUpdated = Date.now();
          NewTabUtils.saveSites();
        }
      },
      onUpdateSearchResult: function(aSearch, aResult) { }
    }
    gHistoryACSearch.startSearch("", "newtab-maxresults", null, listener);
  },

  // adds a new site to _sites
  // aCallback takes the added site as a param
  findNewSite: function NTU_findNewSite(aCallback) {
    let siteCount = 0;
    let listener = {
      onSearchResult: function(aSearch, aResult) {
        for (siteCount; siteCount < aResult.matchCount; siteCount++) {
          let url = aResult.getValueAt(siteCount);
          // don't add site that already exists or is in blacklist
          if (NewTabUtils._sites[url] || NewTabUtils._blockedSites[url]) {
            continue;
          }

          let site = {
            url: url,
            host: Services.io.newURI(url, null, null).host,
            favicon: aResult.getImageAt(siteCount),
            title: aResult.getCommentAt(siteCount)
          };

          // Stop search after we've found a suitable new site
          gHistoryACSearch.stopSearch();
          aCallback(site);

          NewTabUtils._sites[site.url] = site;
          NewTabUtils.saveSites();
          return;
        }
      },
      onUpdateSearchResult: function(aSearch, aResult) { }
    }
    gHistoryACSearch.startSearch("", "newtab-maxresults", null, listener);  
  },

  // aSiteURL is the url of the site to delete from the site array
  removeSite: function NTU_removeSite(aSiteURL) {
    delete this._sites[aSiteURL];
    this.saveSites();

    this._blockedSites[aSiteURL] = true;
    this.saveBlockedSites();
  },

  // aSiteURL is the url of the site to save from the site array
  updatePinnedState: function NTU_updatePinnedState(aSiteURL, pinned) {
    if (pinned) {
      this._sites[aSiteURL].pinned = true;
    } else {
      delete this._sites[aSiteURL].pinned;
    }

    this.saveSites();
  },

  // TODO: this API doesn't necessarily make sense for whatever UI we will choose to use
  // TODO: we should make sure we never have more than 12 sites
  // aCallback takes the added site as a param
  addSite: function NTU_addSite(aSiteURL, aCallback) {
    let siteURI = Services.io.newURI(aSiteURL, null, null);
    function faviconDataCallback(aFaviconURI, aDataLen, aData, aMimeType) {
      let site = {
        url: aSiteURL,
        host: siteURI.host,
        favicion: aFaviconURI.spec
      };
      aCallback(site);
      NewTabUtils._sites[site.url] = site;
    }

    if (!this._faviconService) {
      this._faviconService = Cc["@mozilla.org/browser/favicon-service;1"].
                             getService(Ci.nsIFaviconService);
    }
    this._faviconService.getFaviconURLForPage(siteURI, faviconDataCallback);
    this.saveSites();
  },

  /* ----- File I/O Helper Methods ----- */

  // aCallback takes JSON object made from file contents as a parameter
  // object will be null if the file does not exist
  loadFromFile: function NTU_loadFromFile(aFile, aCallback) {
    let file = FileUtils.getFile("ProfD", aFile.split("/"), true);
    if (!file.exists()) {
      aCallback(null);
      return;
    }
    try {
      let channel = NetUtil.newChannel(file);
      channel.contentType = "application/json";
      NetUtil.asyncFetch(channel, function(aInputStream, aResultCode) {
        if (Components.isSuccessCode(aResultCode)) {
          fileJSON = Cc["@mozilla.org/dom/json;1"].
                     createInstance(Ci.nsIJSON).
                     decodeFromStream(aInputStream, aInputStream.available());
          aCallback(fileJSON);
        } else {
          Cu.reportError("NewTabUtils [loadFromFile]: " + aResultCode);
        }
      });
    } catch (e) {
      Cu.reportError("NewTabUtils [loadFromFile]: " + e);
    }
  },

  saveSites: function NTU_saveSites() {
    this.saveToFile(this.SITES_FILE, this._sites);
  },

  saveBlockedSites: function NTU_saveBlockedSites() {
    this.saveToFile(this.BLOCKED_SITES_FILE, this._blockedSites);
  },

  saveToFile: function NTU_saveToFile(aFile, aObj) {
    let file = FileUtils.getFile("ProfD", aFile.split("/"), true);    
    let fos = FileUtils.openSafeFileOutputStream(file);
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                    createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    try {
      let ins = converter.convertToInputStream(JSON.stringify(aObj));
      NetUtil.asyncCopy(ins, fos, function(aResultCode) {
        if (!Components.isSuccessCode(aResultCode)) {
          Cu.reportError("NewTabUtils [saveToFile]: " + aResultCode);
        }
      });
    } catch (e) {
      Cu.reportError("NewTabUtils [saveToFile]: " + e);
    }
  },
  
  /* ----- Favicon Color Methods ----- */

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
