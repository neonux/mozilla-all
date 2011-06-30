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

  // Called on page load
  init: function NTP_init() {
    let boomarksContainer = document.getElementById("bookmarks-container");
    NewTabUtils.getBookmarkSites(function(aSite) {
      let siteItem = NewTabPage.createSiteItem(aSite);
      boomarksContainer.appendChild(siteItem);
    });

    let sitesContainer = document.getElementById("sites-container");
    NewTabUtils.getTopSites(function(aSite) {
      let siteItem = NewTabPage.createSiteItem(aSite);
      sitesContainer.appendChild(siteItem);
    });

    boomarksContainer.addEventListener("dragover", function(event) {
      if (event.dataTransfer.types.contains("application/x-moz-node")) {
        event.preventDefault();
      }
    }, false);

    boomarksContainer.addEventListener("drop", function(event) {
      if (event.dataTransfer.types.contains("application/x-moz-node")) {
        event.preventDefault();
        let siteItem = event.dataTransfer.mozGetDataAt("application/x-moz-node", 0);
        // Don't do anything if the site is already a bookmark
        if (siteItem.hasAttribute("bookmark")) {
          return;
        }

        document.getElementById("bookmarks-container").appendChild(siteItem);
        siteItem.setAttribute("bookmark", true);

        let siteURL = siteItem.getAttribute("url");
        NewTabUtils.moveSiteToBookmarks(siteURL);
      }      
    }, false);
  },

  // aSite is a Site object from NewTabUtils.jsm
  // bookmark is true if this site is a bookmark
  createSiteItem: function NTP_createSiteItem(aSite) {
    let imageItem = document.createElement("img");
    imageItem.className = "image-item";
    imageItem.setAttribute("src", aSite.favicon);

    let imageContainer = document.createElement("div");
    imageContainer.className = "image-container";
    imageContainer.appendChild(imageItem);
    imageContainer.addEventListener("click", function(event) {
      document.location = aSite.url;
    }, false);

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
    
    let title = aSite.title || aSite.url;
    if (title.length > 40) {
      let ellipsis = Services.prefs.getComplexValue("intl.ellipsis", Ci.nsIPrefLocalizedString).data;
      title = title.substr(0,40).trim() + ellipsis;   
    }
    urlItem.textContent = title;

    let removeButton = document.createElement("button");
    removeButton.className = "remove-button";
    removeButton.setAttribute("url", aSite.url);
    removeButton.addEventListener("click", this.removeSiteItem, false);

    let siteItem = document.createElement("div");
    siteItem.setAttribute("url", aSite.url);
    siteItem.className = "site-item";
    siteItem.appendChild(removeButton);
    siteItem.appendChild(imageContainer);
    siteItem.appendChild(urlItem);

    if (aSite.isBookmark) {
      siteItem.setAttribute("bookmark", true);
    }

    // Set up drag and drop for site items
    siteItem.setAttribute("draggable", true);

    siteItem.addEventListener("dragstart", function(event) {
      event.dataTransfer.mozSetDataAt("application/x-moz-node", siteItem, 0);
      event.dataTransfer.setDragImage(imageContainer, 16, 16);
    }, false);

    return siteItem;
  },

  removeSiteItem: function NTP_removeSiteItem(event) {
    let siteURL = event.target.getAttribute("url");
    let siteItem = document.querySelector(".site-item[url='" + siteURL + "']");
    let bookmark = siteItem.hasAttribute("bookmark");
    if (bookmark) {
      NewTabUtils.removeBookmarkSite(siteURL);
    } else {
      NewTabUtils.removeTopSite(siteURL);
    }

    siteItem.addEventListener("transitionend", function() {
      this.parentNode.removeChild(siteItem);
      if (!bookmark) {
        NewTabUtils.getNewSiteFromHistory(function(aSite) {
          let container = document.getElementById("sites-container");
          let siteItem = NewTabPage.createSiteItem(aSite);
          container.appendChild(siteItem);
        });
      }
    }, false);
    siteItem.setAttribute("removing", true);
  },

  colorBackground: function NTP_colorIconBackground(aElmt, aColor) {
    aElmt.style.backgroundImage =
      "-moz-linear-gradient(top, rgba(" + aColor + ",0.1), rgba(" + aColor + ",0.3))";
    aElmt.style.borderColor = "rgba(" + aColor + ",0.9)";
  },

  hideSection: function NTP_hideSection(event) {
    let section = event.target.parentNode;
    section.addEventListener("transitionend", function() {
      section.setAttribute("hidden", true);
    }, false);
    section.setAttribute("hiding", true);
  }
}
