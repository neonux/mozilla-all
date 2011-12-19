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
* Tim Taubert <ttaubert@mozilla.com> (Original Author)
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

/**
 * The delay in ms to wait before taking a screenshot of a page.
 */
const THUMBNAIL_CAPTURE_DELAY_MS = "2000";

XPCOMUtils.defineLazyModuleGetter(this, "PageThumbs",
  "resource:///modules/PageThumbs.jsm");

/**
 * A singleton that takes care of keeping thumbnails of web pages up-to-date
 * automatically.
 */
let Thumbnails = {
  /**
   * Map of capture() timeouts assigned to their tabs.
   */
  _timeouts: null,

  /**
   * Initializes the Thumbnails singleton and registers event handlers.
   */
  init: function Thumbnails_init() {
    addEventListener("unload", this, false);

    gBrowser.addEventListener("load", this, true);
    gBrowser.tabContainer.addEventListener("TabSelect", this, false);
    gBrowser.tabContainer.addEventListener("SSTabRestored", this, false);

    this._timeouts = new WeakMap();
  },

  /**
   * Uninitializes the Thumbnails singleton and unregisters event handlers.
   */
  uninit: function Thumbnails_uninit() {
    removeEventListener("unload", this, false);

    gBrowser.removeEventListener("load", this, true);
    gBrowser.tabContainer.removeEventListener("TabSelect", this, false);
    gBrowser.tabContainer.removeEventListener("SSTabRestored", this, false);

    this._timeouts = null;
  },

  /**
   * Generic event handler.
   * @param aEvent The event to handle.
   */
  handleEvent: function Thumbnails_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "load":
        let tab = this._determineTabForLoadEvent(aEvent);
        if (tab)
          this.delayedCapture(tab);
        break;
      case "TabSelect":
      case "SSTabRestored":
        this.delayedCapture(aEvent.target);
        break;
      case "unload":
        this.uninit();
        break;
    }
  },

  /**
   * Captures a screenshot of the given tab with a delay.
   * @param aTab The tab to take a screenshot of.
   */
  delayedCapture: function Thumbnails_delayedCapture(aTab) {
    if (this._timeouts.has(aTab))
      clearTimeout(this._timeouts.get(aTab));

    let self = this;

    let timeout = setTimeout(function () {
      self._timeouts.delete(aTab);
      self.capture(aTab);
    }, THUMBNAIL_CAPTURE_DELAY_MS);

    this._timeouts.set(aTab, timeout);
  },

  /**
   * Captures a screenshot of the given tab.
   * @param aTab The tab to take a screenshot of.
   */
  capture: function Thumbnails_capture(aTab) {
    if (!aTab.parentNode || !this.shouldCapture(aTab))
      return;

    PageThumbs.capture(aTab);
  },

  /**
   * Determines whether we should capture a screenshot of the given tab.
   * @param aTab The tab to possibly take a screenshot of.
   * @return Whether we should capture a screenshot.
   */
  shouldCapture: function Thumbnails_shouldCapture(aTab) {
    // There's no point in taking screenshot of loading pages.
    if (aTab.hasAttribute("busy"))
      return false;

    let browser = aTab.linkedBrowser;

    // Don't take screenshots of about: pages.
    if (browser.currentURI.schemeIs("about"))
      return false;

    let channel = browser.docShell.currentDocumentChannel;

    try {
      // If the channel is a nsIHttpChannel get its http status code.
      let httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);

      // Continue only if we have a 2xx status code.
      return Math.floor(httpChannel.responseStatus / 100) == 2;
    } catch (e) {
      // Not a http channel, we just assume a success status code.
      return true;
    }
  },

  /**
   * Determines the tab that a given load event comes from.
   * @param aEvent The load event.
   * @return The tab the load event comes from.
   */
  _determineTabForLoadEvent:
    function Thumbnails_determineTabForLoadEvent(aEvent) {

    let doc = aEvent.originalTarget;
    if (doc instanceof HTMLDocument) {
      let win = doc.defaultView;
      while (win.frameElement)
        win = win.frameElement.ownerDocument.defaultView;

      return gBrowser._getTabForContentWindow(win);
    }
  }
};
