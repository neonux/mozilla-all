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

let Ci = Components.interfaces;
let Cc = Components.classes;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

let NewTabPage = {
  
  init: function NTP_init() {
    this.getTopSites();
  },

  getTopSites: function NTP_getTopSites() {
    let listener = {
      onSearchResult: function(aSearch, aResult) {
        if (aResult.searchResult != Ci.nsIAutoCompleteResult.RESULT_SUCCESS)
          return;

        for (let i = 0; i < aResult.matchCount; i++) {
          NewTabPage.createSiteItem(aResult.getValueAt(i),
                                    aResult.getCommentAt(i),
                                    aResult.getImageAt(i));
        }
      },
      onUpdateSerachResult: function(aSearch, aResult) { 
      }
    }

    let historyACSearch = Cc["@mozilla.org/autocomplete/search;1?name=history"].
                          getService(Ci.nsIAutoCompleteSearch);
    historyACSearch.startSearch("", "", null, listener);
  },

  createSiteItem: function NTP_createSiteItem(aURL, aTitle, aFavicon) {
    let imageItem = document.createElement("img");
    imageItem.className = "image-item";
    imageItem.setAttribute("src", aFavicon);

    let urlItem = document.createElement("a");
    urlItem.className = "url-item";
    urlItem.href = aURL;
    urlItem.textContent = aTitle || aURL;
  
    let removeButton = document.createElement("button");
    removeButton.className = "remove-button";
    removeButton.setAttribute("siteurl", aURL);
    removeButton.addEventListener("click", this.removeSiteItem, false);
  
    let siteItem = document.createElement("li");
    siteItem.className = "site-item";
    siteItem.setAttribute("siteurl", aURL);
    siteItem.appendChild(imageItem);
    siteItem.appendChild(urlItem);
    siteItem.appendChild(removeButton);
    
    document.getElementById("sites-list").appendChild(siteItem);
  },

  removeSiteItem: function NTP_removeSiteItem(event) {
    let siteURL = event.target.getAttribute("siteurl");
    let browserHistory = Cc["@mozilla.org/browser/global-history;2"].
                       getService(Ci.nsIBrowserHistory);
    browserHistory.removePage(Services.io.newURI(siteURL, null, null));

    let siteItem = document.querySelector(".site-item[siteurl='" + siteURL + "']");
    siteItem.addEventListener("transitionend", function() {
      this.parentNode.removeChild(siteItem);    
    }, false);
    siteItem.setAttribute("removing", true);
  }
}
