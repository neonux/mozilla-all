#ifdef 0
/*
 * This Source Code is subject to the terms of the Mozilla Public License
 * version 2.0 (the "License"). You can obtain a copy of the License at
 * http://mozilla.org/MPL/2.0/.
 */
#endif

/**
 * A singleton that takes care of keeping thumbnails of web pages up-to-date
 * automatically.
 */
let gBrowserThumbnails = {
  /**
   * The delay in ms to wait before taking a screenshot of a page.
   */
  _captureDelayMS: 2000,

  /**
   * Map of capture() timeouts assigned to their browsers.
   */
  _timeouts: null,

  /**
   * Cache for the PageThumbs module.
   */
  _pageThumbs: null,

  /**
   * List of tab events we want to listen for.
   */
  _tabEvents: ["TabClose", "TabSelect", "SSTabRestored"],

  /**
   * Initializes the Thumbnails singleton and registers event handlers.
   */
  init: function Thumbnails_init() {
    gBrowser.addTabsProgressListener(this);

    this._tabEvents.forEach(function (aEvent) {
      gBrowser.tabContainer.addEventListener(aEvent, this, false);
    }, this);

    this._timeouts = new WeakMap();

    XPCOMUtils.defineLazyModuleGetter(this, "_pageThumbs",
      "resource:///modules/PageThumbs.jsm", "PageThumbs");
  },

  /**
   * Uninitializes the Thumbnails singleton and unregisters event handlers.
   */
  uninit: function Thumbnails_uninit() {
    gBrowser.removeTabsProgressListener(this);

    this._tabEvents.forEach(function (aEvent) {
      gBrowser.tabContainer.removeEventListener(aEvent, this, false);
    }, this);

    this._timeouts = null;
    this._pageThumbs = null;
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
    }, this._captureDelayMS);

    this._timeouts.set(aBrowser, timeout);
  },

  /**
   * Captures a screenshot of the given browser.
   * @param aBrowser The browser to take a screenshot of.
   */
  capture: function Thumbnails_capture(aBrowser) {
    if (!aBrowser.parentNode || !this.shouldCapture(aBrowser))
      return;

    let canvas = this._pageThumbs.capture(aBrowser.contentWindow);
    this._pageThumbs.store(aBrowser.currentURI.spec, canvas);
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
