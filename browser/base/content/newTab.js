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
Cu.import("resource:///modules/NewTabUtils.jsm");

let NewTabPage = {

  init: function NTP_init() {
    this.getTopSites();
  },

  getTopSites: function NTP_getTopSites() {
    // sites is an ordered array of sites
    NewTabUtils.getSites(function(site) {
      let container = document.getElementById("sites-container");
      let siteItem = NewTabPage.createSiteItem(site);
      container.appendChild(siteItem);
    });
  },

  // aSite is an object with url, host, favicon, saved properites
  createSiteItem: function NTP_createSiteItem(aSite) {
    let imageItem = document.createElement("img");
    imageItem.className = "image-item";
    imageItem.setAttribute("src", aSite.favicon);
    
    let imageContainer = document.createElement("div");
    imageContainer.className = "image-container";
    imageContainer.appendChild(imageItem);
    
    let imageContainerLink = document.createElement("a");
    imageContainerLink.className = "image-container-link";
    imageContainerLink.href = aSite.url;
    imageContainerLink.appendChild(imageContainer);

    let color = NewTabUtils.getCachedFaviconColor(imageItem);
    if (color) {
      this.colorBackground(imageContainer, color);
    } else {
      imageItem.addEventListener("load", function() {
        color = NewTabUtils.getFaviconColor(this, document);
        NewTabPage.colorBackground(imageContainer, color);
      }, false);
    }

    let urlItem = document.createElement("a");
    urlItem.className = "url-item";    
    urlItem.href = aSite.url;
    urlItem.textContent = aSite.title || aSite.url;

    let removeButton = document.createElement("button");
    removeButton.className = "remove-button";
    removeButton.setAttribute("url", aSite.url);
    removeButton.addEventListener("click", this.removeSiteItem, false);

    let pinButton = document.createElement("button");
    pinButton.className = "pin-button";
    pinButton.setAttribute("url", aSite.url);
    pinButton.addEventListener("click", this.updatePinnedState, false);
    if (aSite.pinned) {
      pinButton.setAttribute("pinned", true);
    }

    let siteItem = document.createElement("div");
    siteItem.setAttribute("url", aSite.url);
    siteItem.className = "site-item";
    siteItem.appendChild(pinButton);
    siteItem.appendChild(removeButton);
    siteItem.appendChild(imageContainerLink);
    siteItem.appendChild(urlItem);

    return siteItem;
  },

  removeSiteItem: function NTP_removeSiteItem(event) {
    let siteURL = event.target.getAttribute("url");
    NewTabUtils.removeSite(siteURL);

    let siteItem = document.querySelector(".site-item[url='" + siteURL + "']");
    siteItem.addEventListener("transitionend", function() {
      this.parentNode.removeChild(siteItem);    
      NewTabUtils.findNewSite(function(aSite) {
        let container = document.getElementById("sites-container");
        let siteItem = NewTabPage.createSiteItem(aSite);
        container.appendChild(siteItem);
      });
    }, false);
    siteItem.setAttribute("removing", true);
  },

  updatePinnedState: function NTP_updatePinnedState(event) {
    let saveButton = event.target;
    let pinned = !saveButton.hasAttribute("pinned");
    if (pinned) {
      saveButton.setAttribute("pinned", true);
    } else {
      saveButton.removeAttribute("pinned");
    }

    let siteURL = saveButton.getAttribute("url");
    NewTabUtils.updatePinnedState(siteURL, pinned);
  },

  colorBackground: function NTP_colorIconBackground(aElmt, aColor) {
    aElmt.style.backgroundImage =
      "-moz-linear-gradient(top, rgba(" + aColor + ",0.1), rgba(" + aColor + ",0.3))";
    aElmt.style.borderColor = "rgba(" + aColor + ",0.9)";
  }
}
