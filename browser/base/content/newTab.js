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
    
    let desaturate = false;
    if (desaturate) {
      document.body.setAttribute("desaturate", "true");
      document.body.addEventListener("mouseover", this.colorPage, false);
    }
  },

  getTopSites: function NTP_getTopSites() {
    NewTabUtils.getTopSites(function(topSites) {  
      let table = document.getElementById("sites-table");
      let currentRow;
  
      topSites.forEach(function(aSite, i) {        
        if (i % 3 == 0)
          currentRow = table.insertRow(-1);

        let siteItem = NewTabPage.createSiteItem(aSite);
        currentRow.appendChild(siteItem);
      });
    });
  },

  // aSite is an object with url, title, favicon properites
  createSiteItem: function NTP_createSiteItem(aSite) {
    let imageItem = document.createElement("img");
    imageItem.className = "image-item";
    imageItem.setAttribute("src", aSite.favicon);

    let imageContainer = document.createElement("div");
    imageContainer.className = "image-container";
    imageContainer.appendChild(imageItem);

    let uri = Services.io.newURI(aSite.url, null, null);

    let urlItem = document.createElement("a");
    urlItem.className = "url-item";    
    urlItem.href = uri.prePath;

    let host = uri.host;
    if (host.length > 30) {
      let ellipsis = Services.prefs.getComplexValue(
                      "intl.ellipsis", Ci.nsIPrefLocalizedString).data;
      host = host.substr(0,30) + ellipsis;   
    }
    urlItem.textContent = host;
  
    let removeButton = document.createElement("button");
    removeButton.className = "remove-button";
    removeButton.setAttribute("siteurl", aSite.url);
    removeButton.addEventListener("click", this.removeSiteItem, false);
  
    let siteItem = document.createElement("td");
    siteItem.className = "site-item";
    siteItem.setAttribute("siteurl", aSite.url);
    siteItem.appendChild(removeButton);
    siteItem.appendChild(imageContainer);
    siteItem.appendChild(urlItem);

    imageItem.addEventListener("load", this.computeSiteColor, false);
    
    siteItem.addEventListener("mouseover", this.colorSite, false);
    siteItem.addEventListener("mouseout", this.uncolorSite, false);
    
    return siteItem;
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
  },

  computeSiteColor: function NTP_computeSiteColor(event) {
    let imageItem = event.target;
  
    let canvas = document.createElement("canvas");
    canvas.height = 16;
    canvas.width = 16;

    let context = canvas.getContext("2d");
    context.drawImage(imageItem, 0, 0);
    
    let data = context.getImageData(0, 0, 16, 16).data;
    
    let colorCount = {};
    // data is an array of a series of 4 one-byte values representing the rgba values of each pixel
    for (let i = 0; i < data.length; i += 4) {
      // ignore transparent pixels
      if (data[i+3] == 0)
        continue;
      
      let color = data[i] + "," + data[i+1] + "," + data[i+2];
      // ignore white - that makes for a boring background!
      if (color == "255,255,255")
        continue;
      colorCount[color] = colorCount[color] ?  colorCount[color] + 1 : 1;
    }

    // get most common color
    let maxCount = 0;
    let siteColor = "";
    for (let color in colorCount) {
      if (colorCount[color] > maxCount) {
        maxCount = colorCount[color];
        siteColor = color;
      }
    }

    let container = imageItem.parentNode;    
    //container.setAttribute("sitecolor", siteColor);
    container.style.backgroundImage =
      "-moz-linear-gradient(top, rgba(" + siteColor + ",0.1), rgba(" + siteColor + ",0.3))";
    container.style.border = "1px solid rgba(" + siteColor + ",0.9)";
  }

  /*colorPage: function NTP_colorPage(event) {
    document.body.removeAttribute("desaturate");
    let containers = document.getElementsByClassName("image-container");
    for (let i = 0; i < containers.length; i++) {
      let container = containers[i];
      let siteColor = container.getAttribute("sitecolor");
      container.style.backgroundImage =
        "-moz-linear-gradient(top, rgba(" + siteColor + ",0.1), rgba(" + siteColor + ",0.3))";
      container.style.borderColor = "rgba(" + siteColor + ",0.9)";
    }
  }*/
}
