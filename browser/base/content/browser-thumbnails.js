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
   * Map of capture() timeouts assigned to their browsers.
   */
  _timeouts: null,

  /**
   * List of tab events we want to listen for.
   */
  _tabEvents: ["TabClose", "TabSelect", "SSTabRestored"],

  /**
   * Initializes the Thumbnails singleton and registers event handlers.
   */
  init: function Thumbnails_init() {
    addEventListener("unload", this, false);

    gBrowser.addTabsProgressListener(this);

    this._tabEvents.forEach(function (aEvent) {
      gBrowser.tabContainer.addEventListener(aEvent, this, false);
    }, this);

    this._timeouts = new WeakMap();
  },

  /**
   * Uninitializes the Thumbnails singleton and unregisters event handlers.
   */
  uninit: function Thumbnails_uninit() {
    removeEventListener("unload", this, false);

    gBrowser.removeTabsProgressListener(this);

    this._tabEvents.forEach(function (aEvent) {
      gBrowser.tabContainer.removeEventListener(aEvent, this, false);
    }, this);

    this._timeouts = null;
  },

  /**
   * Generic event handler.
   * @param aEvent The event to handle.
   */
  handleEvent: function Thumbnails_handleEvent(aEvent) {
    let browser;
    switch (aEvent.type) {
      case "TabSelect":
      case "SSTabRestored":
        this.delayedCapture(aEvent.target.linkedBrowser);
        break;
      case "TabClose":
        browser = aEvent.target.linkedBrowser;
        if (this._timeouts.has(browser)) {
          clearTimeout(this._timeouts.get(browser));
          this._timeouts.delete(browser);
        }
        break;
      case "unload":
        this.uninit();
        break;
    }
  },

  /**
   * State change progress listener for all tabs.
   * @param aBrowser The browser whose state has changed.
   * @param aWebProgress The nsIWebProgress that fired this notification.
   * @param aRequest The nsIRequest that has changed state.
   * @param aStateFlags Flags indicating the new state.
   * @param aStatus Error status code associated with the state change.
   */
  onStateChange: function Thumbnails_onStateChange(aBrowser, aWebProgress,
                                                   aRequest, aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK)
      this.delayedCapture(aBrowser);
  },

  /**
   * Captures a screenshot of the given browser with a delay.
   * @param aBrowser The browser to take a screenshot of.
   */
  delayedCapture: function Thumbnails_delayedCapture(aBrowser) {
    if (this._timeouts.has(aBrowser))
      clearTimeout(this._timeouts.get(aBrowser));

    let self = this;

    let timeout = setTimeout(function () {
      self._timeouts.delete(aBrowser);
      self.capture(aBrowser);
    }, THUMBNAIL_CAPTURE_DELAY_MS);

    this._timeouts.set(aBrowser, timeout);
  },

  /**
   * Captures a screenshot of the given browser.
   * @param aBrowser The browser to take a screenshot of.
   */
  capture: function Thumbnails_capture(aBrowser) {
    if (!aBrowser.parentNode || !this.shouldCapture(aBrowser))
      return;

    PageThumbs.capture(aBrowser);
  },

  /**
   * Determines whether we should capture a screenshot of the given browser.
   * @param aBrowser The browser to possibly take a screenshot of.
   * @return Whether we should capture a screenshot.
   */
  shouldCapture: function Thumbnails_shouldCapture(aBrowser) {
    // There's no point in taking screenshot of loading pages.
    if (aBrowser.docShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE)
      return false;

    // Don't take screenshots of about: pages.
    if (aBrowser.currentURI.schemeIs("about"))
      return false;

    let channel = aBrowser.docShell.currentDocumentChannel;

    try {
      // If the channel is a nsIHttpChannel get its http status code.
      let httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);

      // Continue only if we have a 2xx status code.
      return Math.floor(httpChannel.responseStatus / 100) == 2;
    } catch (e) {
      // Not a http channel, we just assume a success status code.
      return true;
    }
  }
};
