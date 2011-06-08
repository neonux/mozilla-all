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
Cu.import("resource://gre/modules/NewTabUtils.jsm");

let NewTabPage = {

  init: function NTP_init() {
    this.getTopSites();
    
    let desaturate = true;
    if (desaturate) {
      document.body.setAttribute("desaturate", "true");
      document.body.addEventListener("mouseover", function() {
        this.removeAttribute("desaturate");
      }, false);
      document.addEventListener("focus", function() {
        this.body.removeAttribute("desaturate");
      }, false);
    }
  },

  getTopSites: function NTP_getTopSites() {
    NewTabUtils.getTopSites(function(topSites) {
      topSites.forEach(function(aSite) {
        NewTabPage.createSiteItem(aSite);
      });
    });
  },

  // site is an object with url, title, favicon properites
  createSiteItem: function NTP_createSiteItem(aSite) {
    let imageItem = document.createElement("img");
    imageItem.className = "image-item";
    imageItem.setAttribute("src", aSite.favicon);

    let urlItem = document.createElement("a");
    urlItem.className = "url-item";
    urlItem.href = aSite.url;
    urlItem.textContent = aSite.title || aSite.url;
  
    let removeButton = document.createElement("button");
    removeButton.className = "remove-button";
    removeButton.setAttribute("siteurl", aSite.url);
    removeButton.addEventListener("click", this.removeSiteItem, false);
  
    let siteItem = document.createElement("li");
    siteItem.className = "site-item";
    siteItem.setAttribute("siteurl", aSite.url);
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
