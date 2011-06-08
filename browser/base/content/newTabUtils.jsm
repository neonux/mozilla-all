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

Components.utils.import("resource://gre/modules/Services.jsm");

let NewTabUtils = {

  _initialized: false,
  _topSites: [],
  _lastUpdated: 0,
  _searching: false,

  init: function NTU_addHistoryObserver() {
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
      onDeleteVisits: function() {}
    };

    let hs = Components.classes["@mozilla.org/browser/nav-history-service;1"].
             getService(Components.interfaces.nsINavHistoryService);
    hs.addObserver(observer, false);

    this._initialized = true;
  },

  // Get top sites from places autocomplete
  // aCallback takes an array of sites as a param
  getTopSites: function NTU_getTopSites(aCallback) {
    if (!this._initialized)
      this.init();

    // Use cached sites if they were fetched less than an hour ago.
    if (this._lastUpdated + 3600000 > Date.now()) {
      aCallback(this._topSites);
      return;
    }

    let listener = {
      onSearchResult: function(aSearch, aResult) {
        if (aResult.searchResult != Components.interfaces.nsIAutoCompleteResult.RESULT_SUCCESS) {
          // do something about error condition?
          return;
        }

        let topSites = [];
        for (let i = 0; i < aResult.matchCount; i++) {
          let site = { 
            url: aResult.getValueAt(i),
            title: aResult.getCommentAt(i),
            favicon: aResult.getImageAt(i)
          };
          topSites.push(site);
        }

        aCallback(topSites);        
        NewTabUtils._topSites = topSites;
        NewTabUtils._lastUpdated = Date.now();
        NewTabUtils._searching = false;
      },
      onUpdateSerachResult: function(aSearch, aResult) { }
    }

    let historyACSearch = Components.classes["@mozilla.org/autocomplete/search;1?name=history"].
                          getService(Components.interfaces.nsIAutoCompleteSearch);
    this._searching = true;
    historyACSearch.startSearch("", "", null, listener);
  }
};
