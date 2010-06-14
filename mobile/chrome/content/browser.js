// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Aleks Totic <a@totic.org>
 *   Johnathan Nightingale <johnath@mozilla.com>
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Taras Glek <tglek@mozilla.com>
 *   Roy Frostig <rfrostig@mozilla.com>
 *   Ben Combee <bcombee@mozilla.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
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

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

function getBrowser() {
  return Browser.selectedBrowser;
}

const kDefaultBrowserWidth = 800;

// how many milliseconds before the mousedown and the overlay of an element
const kTapOverlayTimeout = 200;

// Override sizeToContent in the main window. It breaks things (bug 565887)
window.sizeToContent = function() {
  Components.utils.reportError("window.sizeToContent is not allowed in this window");
}

#ifdef MOZ_CRASH_REPORTER
XPCOMUtils.defineLazyServiceGetter(this, "CrashReporter",
  "@mozilla.org/xre/app-info;1", "nsICrashReporter");
#endif

const endl = '\n';

function debug() {
  let bv = Browser._browserView;
  let tc = bv._tileManager._tileCache;
  let scrollbox = document.getElementById("content-scrollbox")
                .boxObject.QueryInterface(Ci.nsIScrollBoxObject);

  let x = {};
  let y = {};
  let w = {};
  let h = {};
  scrollbox.getPosition(x, y);
  scrollbox.getScrolledSize(w, h);
  let container = document.getElementById("tile-container");
  let [x, y] = [x.value, y.value];
  let [w, h] = [w.value, h.value];
  if (bv) {
    dump('----------------------DEBUG!-------------------------\n');
    dump(bv._browserViewportState.toString() + endl);

    dump(endl);

    dump('location from Browser: ' + Browser.selectedBrowser.contentWindow.location + endl);
    dump('location from BV     : ' + bv.getBrowser().contentWindow.location + endl);

    dump(endl + endl);

    let cr = bv._tileManager._criticalRect;
    dump('criticalRect from BV: ' + (cr ? cr.toString() : null) + endl);
    dump('visibleRect from BV : ' + bv.getVisibleRect().toString() + endl);
    dump('visibleRect from foo: ' + Browser.getVisibleRect().toString() + endl);

    dump('bv batchops depth:    ' + bv._batchOps.length + endl);
    dump('renderpause depth:    ' + bv._renderMode + endl);

    dump(endl);

    dump('window.innerWidth : ' + window.innerWidth  + endl);
    dump('window.innerHeight: ' + window.innerHeight + endl);

    dump(endl);

    dump('container width,height from BV: ' + bv._container.style.width + ', '
                                            + bv._container.style.height + endl);
    dump('container width,height via DOM: ' + container.style.width + ', '
                                            + container.style.height + endl);

    dump(endl);

    dump('scrollbox position    : ' + x + ', ' + y + endl);
    dump('scrollbox scrolledsize: ' + w + ', ' + h + endl);


    let sb = document.getElementById("content-scrollbox");
    dump('container location:     ' + Math.round(container.getBoundingClientRect().left) + " " +
                                      Math.round(container.getBoundingClientRect().top) + endl);

    dump(endl);

    let mouseModule = ih._modules[0];
    dump('ih grabber  : ' + ih._grabber           + endl);
    dump('ih grabdepth: ' + ih._grabDepth         + endl);
    dump('ih listening: ' + !ih._ignoreEvents     + endl);
    dump('ih suppress : ' + ih._suppressNextClick + endl);
    dump('mouseModule : ' + mouseModule           + endl);

    dump(endl);

    dump('tilecache capacity: ' + bv._tileManager._tileCache.getCapacity() + endl);
    dump('tilecache size    : ' + bv._tileManager._tileCache.size          + endl);
    dump('tilecache iBound  : ' + bv._tileManager._tileCache.iBound        + endl);
    dump('tilecache jBound  : ' + bv._tileManager._tileCache.jBound        + endl);

    dump('-----------------------------------------------------\n');
  }
}

function debugTile(i, j) {
  let bv = Browser._browserView;
  let tc = bv._tileManager._tileCache;
  let t  = tc.getTile(i, j);

  dump('------ DEBUGGING TILE (' + i + ',' + j + ') --------\n');

  dump('in bounds: ' + tc.inBounds(i, j) + endl);
  dump('occupied : ' + !!tc.lookup(i, j) + endl);
  if (t)
  {
  dump('toString : ' + t.toString(true) + endl);
  dump('free     : ' + t.free + endl);
  dump('dirtyRect: ' + t._dirtyTileCanvasRect + endl);

  let len = tc._tilePool.length;
  for (let k = 0; k < len; ++k)
    if (tc._tilePool[k] === t)
      dump('found in tilePool at index ' + k + endl);
  }

  dump('------------------------------------\n');
}

function onDebugKeyPress(ev) {
  let bv = Browser._browserView;

  if (!ev.ctrlKey)
    return;

  // use capitals so we require SHIFT here too

  const a = 65;   // debug all critical tiles
  const b = 66;   // dump an ASCII graphic of the tile map
  const c = 67;
  const d = 68;  // debug dump
  const e = 69;
  const f = 70;  // free memory by clearing a tab.
  const g = 71;
  const h = 72;
  const i = 73;  // toggle info click mode
  const j = 74;
  const k = 75;
  const l = 76;  // restart lazy crawl
  const m = 77;  // fix mouseout
  const n = 78;
  const o = 79;
  const p = 80;  // debug tiles in pool order
  const q = 81;  // toggle orientation
  const r = 82;  // reset visible rect
  const s = 83;
  const t = 84;  // debug given list of tiles separated by space
  const u = 85;
  const v = 86;
  const w = 87;
  const x = 88;
  const y = 89;
  const z = 90;  // set zoom level to 1

  if (window.tileMapMode) {
    function putChar(ev, col, row) {
      let tile = tc.getTile(col, row);
      switch (ev.charCode) {
      case h: // held tiles
        dump(tile ? (tile.free ? '*' : 'h') : ' ');
        break;
      case d: // dirty tiles
        dump(tile ? (tile.isDirty() ? 'd' : '*') : ' ');
        break;
      case o: // occupied tileholders
        dump(tc.lookup(col, row) ? 'o' : ' ');
        break;
      }
    }

    let tc = Browser._browserView._tileManager._tileCache;
    let col, row;

    dump(endl);

    dump('  ');
    for (col = 0; col < tc.iBound; ++col)
      dump(col % 10);

    dump(endl);

    for (row = 0; row < tc.jBound; ++row) {

      dump((row % 10) + ' ');

      for (col = 0; col < tc.iBound; ++col) {
        putChar(ev, col, row);
      }

      dump(endl);
    }
    dump(endl + endl);

    for (let ii = 0; ii < tc._tilePool.length; ++ii) {
      let tile = tc._tilePool[ii];
      putChar(ev, tile.i, tile.j);
    }

    dump(endl + endl);

    window.tileMapMode = false;
    return;
  }

  switch (ev.charCode) {
  case f:
    var result = Browser.sacrificeTab();
    if (result)
      dump("Freed a tab\n");
    else
      dump("There are no tabs left to free\n");
    break;

  case r:
    bv.onAfterVisibleMove();
    //bv.setVisibleRect(Browser.getVisibleRect());

  case d:
    debug();

    break;
  case l:
    bv._tileManager.restartLazyCrawl(bv._tileManager._criticalRect);

    break;
  case b:
    window.tileMapMode = true;
    break;
  case t:
    let ijstrs = window.prompt('row,col plz').split(' ');
    for each (let ijstr in ijstrs) {
      let [i, j] = ijstr.split(',').map(function (x) { return parseInt(x); });
      debugTile(i, j);
    }

    break;
  case a:
    let cr = bv._tileManager._criticalRect;
    dump('>>>>>> critical rect is ' + (cr ? cr.toString() : cr) + '\n');
    if (cr) {
      let starti = cr.left  >> kTileExponentWidth;
      let endi   = cr.right >> kTileExponentWidth;

      let startj = cr.top    >> kTileExponentHeight;
      let endj   = cr.bottom >> kTileExponentHeight;

      for (var jj = startj; jj <= endj; ++jj)
        for (var ii = starti; ii <= endi; ++ii)
          debugTile(ii, jj);
    }

    break;
  case i:
    window.infoMode = !window.infoMode;
    break;
  case m:
    Util.dumpLn("renderMode:", bv._renderMode);
    Util.dumpLn("batchOps:",bv._batchOps.length);
    bv.resumeRendering();
    break;
  case p:
    let tc = bv._tileManager._tileCache;
    dump('************* TILE POOL ****************\n');
    for (let ii = 0, len = tc._tilePool.length; ii < len; ++ii) {
      if (window.infoMode)
        debugTile(tc._tilePool[ii].i, tc._tilePool[ii].j);
      else
        dump(tc._tilePool[ii].i + ',' + tc._tilePool[ii].j + '\n');
    }
    dump('****************************************\n');
    break;
#ifndef MOZ_PLATFORM_MAEMO
  case q:
    if (Util.isPortrait())
      window.top.resizeTo(800,480);
    else
      window.top.resizeTo(480,800);
    break;
#endif
  case z:
    bv.setZoomLevel(1.0);
    break;
  default:
    break;
  }
}
window.infoMode = false;
window.tileMapMode = false;

var ih = null;

var Browser = {
  _tabs : [],
  _selectedTab : null,
  windowUtils: window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils),
  contentScrollbox: null,
  contentScrollboxScroller: null,
  controlsScrollbox: null,
  controlsScrollboxScroller: null,
  pageScrollbox: null,
  pageScrollboxScroller: null,
  styles: {},

  startup: function startup() {
    var self = this;

    try {
      messageManager.loadFrameScript("chrome://browser/content/Util.js", true);
      messageManager.loadFrameScript("chrome://browser/content/content.js", true);
    } catch (e) {
      // XXX whatever is calling startup needs to dump errors!
      dump("###########" + e + "\n");
    }

    let needOverride = Util.needHomepageOverride();
    if (needOverride == "new profile")
      this.initNewProfile();

    let container = document.getElementById("tile-container");
    let bv = this._browserView = new BrowserView(container, Browser.getVisibleRect);

    /* handles dispatching clicks on tiles into clicks in content or zooms */
    container.customClicker = new ContentCustomClicker(bv);
    container.customKeySender = new ContentCustomKeySender(bv);

    /* scrolling box that contains tiles */
    let contentScrollbox = this.contentScrollbox = document.getElementById("content-scrollbox");
    this.contentScrollboxScroller = contentScrollbox.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    contentScrollbox.customDragger = new Browser.MainDragger(bv);

    /* horizontally scrolling box that holds the sidebars as well as the contentScrollbox */
    let controlsScrollbox = this.controlsScrollbox = document.getElementById("controls-scrollbox");
    this.controlsScrollboxScroller = controlsScrollbox.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    controlsScrollbox.customDragger = {
      isDraggable: function isDraggable(target, content) { return false; },
      dragStart: function dragStart(cx, cy, target, scroller) {},
      dragStop: function dragStop(dx, dy, scroller) { return false; },
      dragMove: function dragMove(dx, dy, scroller) { return false; }
    };

    /* vertically scrolling box that contains the url bar, notifications, and content */
    let pageScrollbox = this.pageScrollbox = document.getElementById("page-scrollbox");
    this.pageScrollboxScroller = pageScrollbox.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    pageScrollbox.customDragger = controlsScrollbox.customDragger;

    // during startup a lot of viewportHandler calls happen due to content and window resizes
    bv.beginBatchOperation();

    let stylesheet = document.styleSheets[0];
    for each (let style in ["viewport-width", "viewport-height", "window-width", "window-height", "toolbar-height"]) {
      let index = stylesheet.insertRule("." + style + " {}", stylesheet.cssRules.length);
      this.styles[style] = stylesheet.cssRules[index].style;
    }

    function resizeHandler(e) {
      if (e.target != window)
        return;

      // XXX is this code right here actually needed?
      let w = window.innerWidth;
      let h = window.innerHeight;
      let maximize = (document.documentElement.getAttribute("sizemode") == "maximized");
      if (maximize && w > screen.width)
        return;

      bv.beginBatchOperation();

      let toolbarHeight = Math.round(document.getElementById("toolbar-main").getBoundingClientRect().height);
      let scaledDefaultH = (kDefaultBrowserWidth * (h / w));
      let scaledScreenH = (window.screen.width * (h / w));
      let dpiScale = gPrefService.getIntPref("zoom.dpiScale") / 100;

      Browser.styles["viewport-width"].width = (w / dpiScale) + "px";
      Browser.styles["viewport-height"].height = (h / dpiScale) + "px";
      Browser.styles["window-width"].width = w + "px";
      Browser.styles["window-height"].height = h + "px";
      Browser.styles["toolbar-height"].height = toolbarHeight + "px";

      // Cause a resize of the viewport if the current browser holds a XUL document
      let browser = Browser.selectedBrowser;
      if (browser.contentDocument instanceof XULDocument)
        BrowserView.Util.ensureMozScrolledAreaEvent(browser, w, h);

      // Tell the UI to resize the browser controls
      BrowserUI.sizeControls(w, h);

      // XXX During the launch, the resize of the window arrive after we add
      // the first tab, so we need to force the viewport state for this case
      // see bug 558840
      let bvs = bv._browserViewportState;
      if (bvs.viewportRect.width == 1 && bvs.viewportRect.height == 1) {
        bvs.viewportRect.width = window.innerWidth;
        bvs.viewportRect.height = window.innerHeight;
      }

      bv.updateDefaultZoom();
      if (bv.isDefaultZoom())
        // XXX this should really only happen on browser startup, not every resize
        Browser.hideSidebars();
      bv.onAfterVisibleMove();

      for (let i = Browser._tabs.length - 1; i >= 0; i--)
        Browser._tabs[i].updateViewportSize();

      bv.commitBatchOperation();
      
      let curEl = document.activeElement;
      if (curEl && curEl.scrollIntoView)
        curEl.scrollIntoView(false);
    }
    window.addEventListener("resize", resizeHandler, false);

    function fullscreenHandler() {
      if (!window.fullScreen)
        document.getElementById("toolbar-main").setAttribute("fullscreen", "true");
      else
        document.getElementById("toolbar-main").removeAttribute("fullscreen");
    }
    window.addEventListener("fullscreen", fullscreenHandler, false);

    function notificationHandler() {
      // Let the view know that the layout might have changed
      Browser.forceChromeReflow();
      bv.onAfterVisibleMove();
    }
    let notifications = document.getElementById("notifications");
    notifications.addEventListener("AlertActive", notificationHandler, false);
    notifications.addEventListener("AlertClose", notificationHandler, false);

    // Add context helper to the content area only
    container.addEventListener("contextmenu", ContextHelper, false);

    // initialize input handling
    ih = new InputHandler(container);

    BrowserUI.init();

    window.controllers.appendController(this);
    window.controllers.appendController(BrowserUI);

    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.addObserver(gXPInstallObserver, "addon-install-blocked", false);
    os.addObserver(gSessionHistoryObserver, "browser:purge-session-history", false);
    os.addObserver(FormSubmitObserver, "formsubmit", false);

    // clear out tabs the user hasn't touched lately on memory crunch
    os.addObserver(MemoryObserver, "memory-pressure", false);

    // search engine changes
    os.addObserver(BrowserSearch, "browser-search-engine-modified", false);

    window.QueryInterface(Ci.nsIDOMChromeWindow).browserDOMWindow = new nsBrowserAccess();

    let browsers = document.getElementById("browsers");
    browsers.addEventListener("command", this._handleContentCommand, true);
    browsers.addEventListener("DOMUpdatePageReport", gPopupBlockerObserver.onUpdatePageReport, false);

    // Login Manager
    Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);

    // Make sure we're online before attempting to load
    Util.forceOnline();

    // Command line arguments/initial homepage
    let whereURI = this.getHomePage();
    if (needOverride == "new profile")
        whereURI = "about:firstrun";

    // If this is an intial window launch (was a nsICommandLine passed via window params)
    // we execute some logic to load the initial launch page
    if (window.arguments && window.arguments[0] &&
        window.arguments[0] instanceof Ci.nsICommandLine) {
      try {
        var cmdLine = window.arguments[0];

        // Check for and use a single commandline parameter
        if (cmdLine.length == 1) {
          // Assume the first arg is a URI if it is not a flag
          var uri = cmdLine.getArgument(0);
          if (uri != "" && uri[0] != '-') {
            whereURI = cmdLine.resolveURI(uri);
            if (whereURI)
              whereURI = whereURI.spec;
          }
        }

        // Check for the "url" flag
        var uriFlag = cmdLine.handleFlagWithParam("url", false);
        if (uriFlag) {
          whereURI = cmdLine.resolveURI(uriFlag);
          if (whereURI)
            whereURI = whereURI.spec;
        }
      } catch (e) {}
    }

    this.addTab(whereURI, true);

    // JavaScript Error Console
    if (gPrefService.getBoolPref("browser.console.showInPanel")){
      let button = document.getElementById("tool-console");
      button.hidden = false;
    }

    bv.commitBatchOperation();

    // If some add-ons were disabled during during an application update, alert user
    if (gPrefService.prefHasUserValue("extensions.disabledAddons")) {
      let addons = gPrefService.getCharPref("extensions.disabledAddons").split(",");
      if (addons.length > 0) {
        let disabledStrings = Elements.browserBundle.getString("alertAddonsDisabled");
        let label = PluralForm.get(addons.length, disabledStrings).replace("#1", addons.length);

        let alerts = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
        alerts.showAlertNotification(URI_GENERIC_ICON_XPINSTALL, Elements.browserBundle.getString("alertAddons"),
                                     label, false, "", null);
      }
      gPrefService.clearUserPref("extensions.disabledAddons");
    }

    // Force commonly used border-images into the image cache
    ImagePreloader.cache();

    messageManager.addMessageListener("FennecViewportMetadata", this);
    messageManager.addMessageListener("Browser:MozApplicationManifest", OfflineApps);

    this._pluginObserver = new PluginObserver(bv);

    // broadcast a UIReady message so add-ons know we are finished with startup
    let event = document.createEvent("Events");
    event.initEvent("UIReady", true, false);
    window.dispatchEvent(event);
  },

  _waitingToClose: false,
  closing: function closing() {
    // If we are already waiting for the close prompt, don't show another
    if (this._waitingToClose)
      return false;

    // Prompt if we have multiple tabs before closing window
    let numTabs = this._tabs.length;
    if (numTabs > 1) {
      let shouldPrompt = gPrefService.getBoolPref("browser.tabs.warnOnClose");
      if (shouldPrompt) {
        let prompt = Cc["@mozilla.org/embedcomp/prompt-service;1"].getService(Ci.nsIPromptService);
  
        // Default to true: if it were false, we wouldn't get this far
        let warnOnClose = { value: true };

        let messageBase = Elements.browserBundle.getString("tabs.closeWarning");
        let message = PluralForm.get(numTabs, messageBase).replace("#1", numTabs);

        let title = Elements.browserBundle.getString("tabs.closeWarningTitle");
        let closeText = Elements.browserBundle.getString("tabs.closeButton");
        let checkText = Elements.browserBundle.getString("tabs.closeWarningPromptMe");
        let buttons = (prompt.BUTTON_TITLE_IS_STRING * prompt.BUTTON_POS_0) +
                      (prompt.BUTTON_TITLE_CANCEL * prompt.BUTTON_POS_1);

        this._waitingToClose = true;
        let pressed = prompt.confirmEx(window, title, message, buttons, closeText, null, null, checkText, warnOnClose);
        this._waitingToClose = false;

        // Don't set the pref unless they press OK and it's false
        let reallyClose = (pressed == 0);
        if (reallyClose && !warnOnClose.value)
          gPrefService.setBoolPref("browser.tabs.warnOnClose", false);

        // If we don't want to close, return now. If we are closing, continue with other housekeeping.
        if (!reallyClose)
          return false;
      }
    }

    // Figure out if there's at least one other browser window around.
    let lastBrowser = true;
    let e = gWindowMediator.getEnumerator("navigator:browser");
    while (e.hasMoreElements() && lastBrowser) {
      let win = e.getNext();
      if (win != window && win.toolbar.visible)
        lastBrowser = false;
    }
    if (!lastBrowser)
      return true;

    // Let everyone know we are closing the last browser window
    let closingCanceled = Cc["@mozilla.org/supports-PRBool;1"].createInstance(Ci.nsISupportsPRBool);
    gObserverService.notifyObservers(closingCanceled, "browser-lastwindow-close-requested", null);
    if (closingCanceled.data)
      return false;
  
    gObserverService.notifyObservers(null, "browser-lastwindow-close-granted", null);
    return true;
  },

  shutdown: function shutdown() {
    this._browserView.uninit();
    BrowserUI.uninit();
    this._pluginObserver.stop();

    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.removeObserver(gXPInstallObserver, "addon-install-blocked");
    os.removeObserver(gSessionHistoryObserver, "browser:purge-session-history");
    os.removeObserver(MemoryObserver, "memory-pressure");
    os.removeObserver(BrowserSearch, "browser-search-engine-modified");
    os.removeObserver(FormSubmitObserver, "formsubmit");

    window.controllers.removeController(this);
    window.controllers.removeController(BrowserUI);
  },

  initNewProfile: function initNewProfile() {
  },

  getHomePage: function () {
    let url = "about:home";
    try {
      url = gPrefService.getComplexValue("browser.startup.homepage", Ci.nsIPrefLocalizedString).data;
    } catch (e) { }

    return url;
  },

  get browsers() {
    return this._tabs.map(function(tab) { return tab.browser; });
  },

  scrollContentToTop: function scrollContentToTop() {
    this.contentScrollboxScroller.scrollTo(0, 0);
    this.pageScrollboxScroller.scrollTo(0, 0);
    this._browserView.onAfterVisibleMove();
  },

  /** Let current browser's scrollbox know about where content has been panned. */
  scrollBrowserToContent: function scrollBrowserToContent() {
    let browser = this.selectedBrowser;
    if (browser) {
      let scroll = Browser.getScrollboxPosition(Browser.contentScrollboxScroller);
      let windowUtils = BrowserView.Util.getBrowserDOMWindowUtils(browser);
      browser.contentWindow.scrollTo(scroll.x, scroll.y);
    }
  },

  /** Update viewport to location of browser's scrollbars. */
  scrollContentToBrowser: function scrollContentToBrowser() {
    let pos = BrowserView.Util.getContentScrollOffset(this.selectedBrowser);
    if (pos.y != 0)
      Browser.hideTitlebar();

    Browser.contentScrollboxScroller.scrollTo(pos.x, pos.y);
    this._browserView.onAfterVisibleMove();
  },

  hideSidebars: function scrollSidebarsOffscreen() {
    let container = this.contentScrollbox;
    let rect = container.getBoundingClientRect();
    this.controlsScrollboxScroller.scrollBy(Math.round(rect.left), 0);
    this._browserView.onAfterVisibleMove();
  },

  hideTitlebar: function hideTitlebar() {
    let container = this.contentScrollbox;
    let rect = container.getBoundingClientRect();
    this.pageScrollboxScroller.scrollBy(0, Math.round(rect.top));
    this.tryUnfloatToolbar();
    this._browserView.onAfterVisibleMove();
  },

  /**
   * Return the currently active <browser> object
   */
  get selectedBrowser() {
    return this._selectedTab.browser;
  },

  get tabs() {
    return this._tabs;
  },

  getTabForDocument: function(aDocument) {
    let tabs = this._tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser.contentDocument == aDocument)
        return tabs[i];
    }
    return null;
  },

  getTabForBrowser: function getTabForBrowser(aBrowser) {
    let tabs = this._tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser == aBrowser)
        return tabs[i];
    }
    return null;
  },

  getTabAtIndex: function getTabAtIndex(index) {
    if (index > this._tabs.length || index < 0)
      return null;
    return this._tabs[index];
  },

  getTabFromChrome: function getTabFromChrome(chromeTab) {
    for (var t = 0; t < this._tabs.length; t++) {
      if (this._tabs[t].chromeTab == chromeTab)
        return this._tabs[t];
    }
    return null;
  },

  addTab: function(uri, bringFront) {
    let newTab = new Tab();
    this._tabs.push(newTab);

    if (bringFront)
      this.selectedTab = newTab;

    newTab.load(uri);

    let event = document.createEvent("Events");
    event.initEvent("TabOpen", true, false);
    newTab.chromeTab.dispatchEvent(event);

    return newTab;
  },

  closeTab: function(tab) {
    if (tab instanceof XULElement)
      tab = this.getTabFromChrome(tab);

    if (!tab)
      return;

    let tabIndex = this._tabs.indexOf(tab);

    let nextTab = this._selectedTab;
    if (this._selectedTab == tab) {
      nextTab = this.getTabAtIndex(tabIndex + 1) || this.getTabAtIndex(tabIndex - 1);
      if (!nextTab)
        return;
    }

    let event = document.createEvent("Events");
    event.initEvent("TabClose", true, false);
    tab.chromeTab.dispatchEvent(event);

    this.selectedTab = nextTab;

    tab.destroy();
    this._tabs.splice(tabIndex, 1);
  },

  get selectedTab() {
    return this._selectedTab;
  },

  set selectedTab(tab) {
    let bv = this._browserView;

    if (tab instanceof XULElement)
      tab = this.getTabFromChrome(tab);

    if (!tab || this._selectedTab == tab)
      return;

    if (this._selectedTab) {
      this._selectedTab.contentScrollOffset = this.getScrollboxPosition(this.contentScrollboxScroller);
      this._selectedTab.pageScrollOffset = this.getScrollboxPosition(this.pageScrollboxScroller);

      // Make sure we leave the toolbar in an unlocked state
      if (this._selectedTab.isLoading())
        BrowserUI.unlockToolbar();
    }

    let isFirstTab = this._selectedTab == null;
    let lastTab = this._selectedTab;
    this._selectedTab = tab;

    // Lock the toolbar if the new tab is still loading
    if (this._selectedTab.isLoading())
      BrowserUI.lockToolbar();

    tab.ensureBrowserExists();

    bv.beginBatchOperation();

    bv.setBrowser(tab.browser, tab.browserViewportState);
    bv.forceContainerResize();
    bv.updateDefaultZoom();

    document.getElementById("tabs").selectedTab = tab.chromeTab;

    if (!isFirstTab) {
      // Update all of our UI to reflect the new tab's location
      BrowserUI.updateURI();
      getIdentityHandler().checkIdentity();

      let event = document.createEvent("Events");
      event.initEvent("TabSelect", true, false);
      event.lastTab = lastTab;
      tab.chromeTab.dispatchEvent(event);
    }

    tab.lastSelected = Date.now();

    // XXX incorrect behavior if page was scrolled by tab in the background.
    if (tab.contentScrollOffset) {
      let { x: scrollX, y: scrollY } = tab.contentScrollOffset;
      Browser.contentScrollboxScroller.scrollTo(scrollX, scrollY);
    }
    if (tab.pageScrollOffset) {
      let { x: pageScrollX, y: pageScrollY } = tab.pageScrollOffset;
      Browser.pageScrollboxScroller.scrollTo(pageScrollX, pageScrollY);
    }

    bv.setAggressive(!tab._loading);

    bv.commitBatchOperation();
  },

  supportsCommand: function(cmd) {
    var isSupported = false;
    switch (cmd) {
      case "cmd_fullscreen":
        isSupported = true;
        break;
      default:
        isSupported = false;
        break;
    }
    return isSupported;
  },

  isCommandEnabled: function(cmd) {
    return true;
  },

  doCommand: function(cmd) {
    switch (cmd) {
      case "cmd_fullscreen":
        window.fullScreen = !window.fullScreen;
        break;
    }
  },

  getNotificationBox: function getNotificationBox() {
    return document.getElementById("notifications");
  },

  removeTransientNotificationsForTab: function removeTransientNotificationsForTab(aTab) {
    let notificationBox = this.getNotificationBox();
    let notifications = notificationBox.allNotifications;
    for (let n = notifications.length - 1; n >= 0; n--) {
      let notification = notifications[n];
      if (notification._chromeTab != aTab.chromeTab)
        continue;

      if (notification.persistence)
        notification.persistence--;
      else if (Date.now() > notification.timeout)
        notificationBox.removeNotification(notification);
    }
  },

  /** Returns true iff a tab's browser has been destroyed to free up memory. */
  sacrificeTab: function sacrificeTab() {
    let tabToClear = this._tabs.reduce(function(prevTab, currentTab) {
      if (currentTab == Browser.selectedTab || !currentTab.browser) {
        return prevTab;
      } else {
        return (prevTab && prevTab.lastSelected <= currentTab.lastSelected) ? prevTab : currentTab;
      }
    }, null);

    if (tabToClear) {
      tabToClear.saveState();
      tabToClear._destroyBrowser();
      return true;
    } else {
      return false;
    }
  },

  /**
   * Handle command event bubbling up from content.  This allows us to do chrome-
   * privileged things based on buttons in, e.g., unprivileged error pages.
   * Obviously, care should be taken not to trust events that web pages could have
   * synthesized.
   */
  _handleContentCommand: function _handleContentCommand(aEvent) {
    // Don't trust synthetic events
    if (!aEvent.isTrusted)
      return;

    var ot = aEvent.originalTarget;
    var errorDoc = ot.ownerDocument;

    // If the event came from an ssl error page, it is probably either the "Add
    // Exception…" or "Get me out of here!" button
    if (/^about:certerror\?e=nssBadCert/.test(errorDoc.documentURI)) {
      if (ot == errorDoc.getElementById("temporaryExceptionButton") ||
          ot == errorDoc.getElementById("permanentExceptionButton")) {
        try {
          // add a new SSL exception for this URL
          let uri = gIOService.newURI(errorDoc.location.href, null, null);
          let sslExceptions = new SSLExceptions();

          if (ot == errorDoc.getElementById("permanentExceptionButton")) {
            sslExceptions.addPermanentException(uri);
          } else {
            sslExceptions.addTemporaryException(uri);
          }
        } catch (e) {
          dump("EXCEPTION handle content command: " + e + "\n" );
        }

        // automatically reload after the exception was added
        errorDoc.location.reload();
      }
      else if (ot == errorDoc.getElementById('getMeOutOfHereButton')) {
        // Get the start page from the *default* pref branch, not the user's
        var defaultPrefs = Cc["@mozilla.org/preferences-service;1"]
                          .getService(Ci.nsIPrefService).getDefaultBranch(null);
        var url = "about:blank";
        try {
          url = defaultPrefs.getCharPref("browser.startup.homepage");
          // If url is a pipe-delimited set of pages, just take the first one.
          if (url.indexOf("|") != -1)
            url = url.split("|")[0];
        } catch (e) { /* Fall back on about blank */ }

        Browser.selectedBrowser.loadURI(url, null, null, false);
      }
    }
    else if (/^about:neterror\?e=netOffline/.test(errorDoc.documentURI)) {
      if (ot == errorDoc.getElementById("errorTryAgain")) {
        // Make sure we're online before attempting to load
        Util.forceOnline();
      }
    }
  },

  /**
   * Compute the sidebar percentage visibility.
   *
   * @param [optional] dx
   * @param [optional] dy an offset distance at which to perform the visibility
   * computation
   * @return [leftVisibility, rightVisiblity, leftTotalWidth, rightTotalWidth]
   */
  computeSidebarVisibility: function computeSidebarVisibility(dx, dy) {
    function visibility(bar, visrect) {
      let w = bar.width;
      bar.restrictTo(visrect);
      return bar.width / w;
    }

    if (!dx) dx = 0;
    if (!dy) dy = 0;

    let leftbarCBR = document.getElementById('tabs-container').getBoundingClientRect();
    let ritebarCBR = document.getElementById('browser-controls').getBoundingClientRect();

    let leftbar = new Rect(Math.round(leftbarCBR.left) - dx, 0, Math.round(leftbarCBR.width), 1);
    let ritebar = new Rect(Math.round(ritebarCBR.left) - dx, 0, Math.round(ritebarCBR.width), 1);
    let leftw = leftbar.width;
    let ritew = ritebar.width;

    let visrect = new Rect(0, 0, window.innerWidth, 1);

    let leftvis = visibility(leftbar, visrect);
    let ritevis = visibility(ritebar, visrect);

    return [leftvis, ritevis, leftw, ritew];
  },

  /**
   * Compute the horizontal distance needed to scroll in order to snap the
   * sidebars into place.
   *
   * Visibility is computed by creating dummy rectangles for the sidebar and the
   * visible rect.  Sidebar rectangles come from getBoundingClientRect(), so
   * they are in absolute client coordinates (and since we're in a scrollbox,
   * this means they are positioned relative to the window, which is anchored at
   * (0, 0) regardless of the scrollbox's scroll position.  The rectangles are
   * made to have a top of 0 and a height of 1, since these do not affect how we
   * compute visibility (we care only about width), and using rectangles allows
   * us to use restrictTo(), which comes in handy.
   *
   * @return scrollBy dx needed to make snap happen
   */
  snapSidebars: function snapSidebars() {
    let [leftvis, ritevis, leftw, ritew] = Browser.computeSidebarVisibility();

    let snappedX = 0;

    if (leftvis != 0 && leftvis != 1) {
      if (leftvis >= 0.6666) {
        snappedX = -((1 - leftvis) * leftw);
      } else {
        snappedX = leftvis * leftw;
      }
    }
    else if (ritevis != 0 && ritevis != 1) {
      if (ritevis >= 0.6666) {
        snappedX = (1 - ritevis) * ritew;
      } else {
        snappedX = -ritevis * ritew;
      }
    }

    return Math.round(snappedX);
  },

  tryFloatToolbar: function tryFloatToolbar(dx, dy) {
    if (this.floatedWhileDragging)
      return;

    let [leftvis, ritevis, leftw, ritew] = Browser.computeSidebarVisibility(dx, dy);
    if (leftvis > 0 || ritevis > 0) {
      BrowserUI.lockToolbar();
      this.floatedWhileDragging = true;
    }
  },

  tryUnfloatToolbar: function tryUnfloatToolbar(dx, dy) {
    if (!this.floatedWhileDragging)
      return true;

    let [leftvis, ritevis, leftw, ritew] = Browser.computeSidebarVisibility(dx, dy);
    if (leftvis == 0 && ritevis == 0) {
      BrowserUI.unlockToolbar();
      this.floatedWhileDragging = false;
      return true;
    }
    return false;
  },

  /** Zoom one step in (negative) or out (positive). */
  zoom: function zoom(aDirection) {
    let bv = this._browserView;
    if (!bv.allowZoom)
      return;

    let zoomLevel = bv.getZoomLevel();

    let zoomValues = ZoomManager.zoomValues;
    var i = zoomValues.indexOf(ZoomManager.snap(zoomLevel)) + (aDirection < 0 ? 1 : -1);
    if (i >= 0 && i < zoomValues.length)
      zoomLevel = zoomValues[i];

    zoomLevel = Math.max(zoomLevel, bv.getPageZoomLevel());

    let center = this.getVisibleRect().center().map(bv.viewportToBrowser);
    this.setVisibleRect(this._getZoomRectForPoint(center.x, center.y, zoomLevel));
  },

  /**
   * Find the needed zoom level for zooming on an element
   */
  _getZoomLevelForElement: function _getZoomLevelForElement(element) {
    const margin = 15;

    let bv = this._browserView;
    let elRect = bv.browserToViewportRect(Browser.getBoundingContentRect(element));

    let vis = bv.getVisibleRect();
    return bv.clampZoomLevel(bv.getZoomLevel() * vis.width / (elRect.width + margin * 2));
  },

  /**
   * Find an appropriate zoom rect for an element, if it exists.
   * @return Rect in viewport coordinates
   * */
  _getZoomRectForElement: function _getZoomRectForElement(element, elementY) {
    let bv = this._browserView;
    let oldZoomLevel = bv.getZoomLevel();
    let zoomLevel = this._getZoomLevelForElement(element);
    let zoomRatio = oldZoomLevel / zoomLevel;

    // Don't zoom in a marginal amount, but be more lenient for the first zoom.
    // > 2/3 means operation increases the zoom level by less than 1.5
    // > 9/10 means operation increases the zoom level by less than 1.1
    let zoomTolerance = (bv.isDefaultZoom()) ? .9 : .6666;
    if (zoomRatio >= zoomTolerance) {
      return null;
    } else {
      let elRect = this.getBoundingContentRect(element);
      return this._getZoomRectForPoint(elRect.center().x, elementY, zoomLevel);
    }
  },

  /**
   * Find a good zoom rectangle for point that is specified in browser coordinates.
   * @return Rect in viewport coordinates
   */
  _getZoomRectForPoint: function _getZoomRectForPoint(x, y, zoomLevel) {
    let bv = this._browserView;
    let vis = bv.getVisibleRect();
    x = bv.browserToViewport(x);
    y = bv.browserToViewport(y);

    zoomLevel = Math.min(ZoomManager.MAX, zoomLevel);
    let zoomRatio = zoomLevel / bv.getZoomLevel();
    let newVisW = vis.width / zoomRatio, newVisH = vis.height / zoomRatio;
    let result = new Rect(x - newVisW / 2, y - newVisH / 2, newVisW, newVisH);

    // Make sure rectangle doesn't poke out of viewport
    return result.translateInside(bv._browserViewportState.viewportRect);
  },

  setVisibleRect: function setVisibleRect(rect) {
    let bv = this._browserView;
    let vis = bv.getVisibleRect();
    let zoomRatio = vis.width / rect.width;
    let zoomLevel = bv.getZoomLevel() * zoomRatio;
    let scrollX = rect.left * zoomRatio;
    let scrollY = rect.top * zoomRatio;

    // The order of operations below is important for artifacting and for performance. Important
    // side effects of functions are noted below.

    // Hardware scrolling happens immediately when scrollTo is called.  Hide to prevent artifacts.
    bv.beginOffscreenOperation(rect);

    // We must scroll to the correct area before TileManager is informed of the change
    // so that only one render is done. Ensures setZoomLevel puts it off.
    bv.beginBatchOperation();

    // Critical rect changes when controls are hidden. Must hide before tilemanager viewport.
    this.hideSidebars();
    this.hideTitlebar();

    bv.setZoomLevel(zoomLevel);

    // Ensure container is big enough for scroll values.
    bv.forceContainerResize();
    this.forceChromeReflow();
    this.contentScrollboxScroller.scrollTo(scrollX, scrollY);
    bv.onAfterVisibleMove();

    // Inform tile manager, which happens to render new tiles too. Must call in case a batch
    // operation was in progress before zoom.
    bv.forceViewportChange();

    bv.commitBatchOperation();
    bv.commitOffscreenOperation();
  },

  zoomToPoint: function zoomToPoint(cX, cY) {
    let [elementX, elementY] = this.transformClientToBrowser(cX, cY);
    let zoomRect = null;
    let element = this.elementFromPoint(elementX, elementY);
    let bv = this._browserView;
    if (element)
      zoomRect = this._getZoomRectForElement(element, elementY);
    if (!zoomRect && bv.isDefaultZoom())
      zoomRect = this._getZoomRectForPoint(elementX, elementY, bv.getZoomLevel() * 2);

    if (zoomRect)
      this.setVisibleRect(zoomRect);

    return zoomRect;
  },

  zoomFromPoint: function zoomFromPoint(cX, cY) {
    let bv = this._browserView;
    if (!bv.isDefaultZoom()) {
      let zoomLevel = bv.getDefaultZoomLevel();
      let [elementX, elementY] = this.transformClientToBrowser(cX, cY);
      let zoomRect = this._getZoomRectForPoint(elementX, elementY, zoomLevel);
      this.setVisibleRect(zoomRect);
    }
  },

  getContentClientRects: function getContentClientRects(contentElem) {
    // XXX don't copy getBoundingContentRect
    let browser = Browser._browserView.getBrowser();

    if (!browser)
      return null;

    let offset = BrowserView.Util.getContentScrollOffset(browser);
    let nativeRects = contentElem.getClientRects();

    // step out of iframes and frames, offsetting scroll values
    let rect;
    let cw = browser.contentWindow;
    for (let frame = contentElem.ownerDocument.defaultView; frame != cw; frame = frame.parent) {
      // adjust client coordinates' origin to be top left of iframe viewport
      rect = frame.frameElement.getBoundingClientRect();
      let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
      let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
      offset.add(rect.left + parseInt(left), rect.top + parseInt(top));
    }

    let result = [];
    let r;
    for (let i = nativeRects.length - 1; i >= 0; i--) {
      r = nativeRects[i];
      result.push(new Rect(r.left + offset.x, r.top + offset.y, r.width, r.height));
    }
    return result;
  },

  getBoundingContentRect: function getBoundingContentRect(contentElem) {
    let document = contentElem.ownerDocument;
    while(document.defaultView.frameElement)
      document = document.defaultView.frameElement.ownerDocument;

    let tab = Browser.getTabForDocument(document);
    if (!tab || !tab.browser)
      return null;

    let browser = tab.browser;
    let offset = BrowserView.Util.getContentScrollOffset(browser);

    let r = contentElem.getBoundingClientRect();

    // step out of iframes and frames, offsetting scroll values
    for (let frame = contentElem.ownerDocument.defaultView; frame != browser.contentWindow; frame = frame.parent) {
      // adjust client coordinates' origin to be top left of iframe viewport
      let rect = frame.frameElement.getBoundingClientRect();
      let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
      let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
      offset.add(rect.left + parseInt(left), rect.top + parseInt(top));
    }

    return new Rect(r.left + offset.x, r.top + offset.y, r.width, r.height);
  },

  /**
   * Transform x and y from client coordinates to BrowserView coordinates.
   */
  clientToBrowserView: function clientToBrowserView(x, y) {
    let container = document.getElementById("tile-container");
    let containerBCR = container.getBoundingClientRect();

    let x0 = Math.round(containerBCR.left);
    let y0;
    if (arguments.length > 1)
      y0 = Math.round(containerBCR.top);

    return (arguments.length > 1) ? [x - x0, y - y0] : (x - x0);
  },

  browserViewToClient: function browserViewToClient(x, y) {
    let container = document.getElementById("tile-container");
    let containerBCR = container.getBoundingClientRect();

    let x0 = Math.round(-containerBCR.left);
    let y0;
    if (arguments.length > 1)
      y0 = Math.round(-containerBCR.top);

    return (arguments.length > 1) ? [x - x0, y - y0] : (x - x0);
  },

  browserViewToClientRect: function browserViewToClientRect(rect) {
    let container = document.getElementById("tile-container");
    let containerBCR = container.getBoundingClientRect();
    return rect.clone().translate(Math.round(containerBCR.left), Math.round(containerBCR.top));
  },

  /**
   * turn client coordinates into page-relative ones (adjusted for
   * zoom and page position)
   */
  transformClientToBrowser: function transformClientToBrowser(cX, cY) {
    return this.clientToBrowserView(cX, cY).map(this._browserView.viewportToBrowser);
  },

  /**
   * @param x,y Browser coordinates
   * @return Element at position, null if no active browser or no element found
   */
  elementFromPoint: function elementFromPoint(x, y) {
    let browser = this._browserView.getBrowser();
    if (!browser)
      return null;

    // browser's elementFromPoint expect browser-relative client coordinates.
    // subtract browser's scroll values to adjust
    let cwu = BrowserView.Util.getBrowserDOMWindowUtils(browser);
    let scrollX = {}, scrollY = {};
    cwu.getScrollXY(false, scrollX, scrollY);
    x = x - scrollX.value;
    y = y - scrollY.value;
    let elem = ElementTouchHelper.getClosest(cwu, x, y);

    // step through layers of IFRAMEs and FRAMES to find innermost element
    while (elem && (elem instanceof HTMLIFrameElement || elem instanceof HTMLFrameElement)) {
      // adjust client coordinates' origin to be top left of iframe viewport
      let win = elem.ownerDocument.defaultView;
      let left = win.getComputedStyle(elem, "").borderLeftWidth;
      let top = win.getComputedStyle(elem, "").borderTopWidth;

      let rect = elem.getBoundingClientRect();
      x = Math.max(0, x - (rect.left + parseInt(left)));
      y = Math.max(0, y - (rect.top + parseInt(top)));

      let windowUtils = elem.contentDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
      elem = ElementTouchHelper.getClosest(windowUtils, x, y);
    }

    return elem;
  },

  /**
   * Return the visible rect in coordinates with origin at the (left, top) of
   * the tile container, i.e. BrowserView coordinates.
   */
  getVisibleRect: function getVisibleRect() {
    let stack = document.getElementById("tile-stack");
    let container = document.getElementById("tile-container");
    let containerBCR = container.getBoundingClientRect();

    let x = Math.round(-containerBCR.left);
    let y = Math.round(-containerBCR.top);
    let w = window.innerWidth;
    let h = stack.getBoundingClientRect().height;

    return new Rect(x, y, w, h);
  },

  /**
   * Convenience function for getting the scrollbox position off of a
   * scrollBoxObject interface.  Returns the actual values instead of the
   * wrapping objects.
   *
   * @param scroller a scrollBoxObject on which to call scroller.getPosition()
   */
  getScrollboxPosition: function getScrollboxPosition(scroller) {
    let x = {};
    let y = {};
    scroller.getPosition(x, y);
    return new Point(x.value, y.value);
  },

  forceChromeReflow: function forceChromeReflow() {
    let dummy = getComputedStyle(document.documentElement, "").width;
  },

  receiveMessage: function receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "FennecViewportMetadata":
        let tab = Browser.getTabForBrowser(aMessage.target);
        tab.updateViewportMetadata(aMessage.json);
        break;
    }
  }
};

Browser.MainDragger = function MainDragger(browserView) {
  this.bv = browserView;
  this.draggedFrame = null;
  this.contentScrollbox = null;
};

Browser.MainDragger.prototype = {
  isDraggable: function isDraggable(target, scroller) { return true; },

  dragStart: function dragStart(clientX, clientY, target, scroller) {
    // Make sure pausing occurs before any early returns.
    this.bv.pauseRendering();

    // XXX shouldn't know about observer
    // adding pause in pauseRendering isn't so great, because tiles will hardly ever prefetch while
    // loading state is going (and already, the idle timer is bigger during loading so it doesn't fit
    // into the aggressive flag).
    this.bv._idleServiceObserver.pause();

    let [x, y] = Browser.transformClientToBrowser(clientX, clientY);
    let element = Browser.elementFromPoint(x, y);

    this.draggedFrame = null;
    this.contentScrollbox = null;

    // Check if we are in a scrollable HTML element
    let htmlElement = element;
    if (htmlElement && htmlElement instanceof HTMLElement) {
      let win = htmlElement.ownerDocument.defaultView;
      for (; htmlElement; htmlElement = htmlElement.parentNode) {
        try {
          let cs = win.getComputedStyle(htmlElement, null);
          let overflowX = cs.getPropertyValue("overflow-x");
          let overflowY = cs.getPropertyValue("overflow-y");

          let scrollableY = overflowY != "hidden" && overflowY != "visible" && htmlElement.clientHeight < htmlElement.scrollHeight;
          let scrollableX = overflowX != "hidden" && overflowX != "visible" && htmlElement.clientWidth  < htmlElement.scrollWidth
            && !(htmlElement instanceof HTMLSelectElement); // Bug 295977

          if (scrollableX || scrollableY) {
            this.contentScrollbox = this._createDivScrollBox(htmlElement);
            return;
          }
        } catch(e) {}
      }
    }

    // Check if we are in XUL land
    let xulElement = element;
    if (xulElement && xulElement instanceof XULElement) {
      for (; xulElement; xulElement = xulElement.parentNode) {
        if (xulElement.localName == "treechildren") {
          this.contentScrollbox = this._createTreeScrollBox(xulElement.parentNode);
          return;
        }
        let wrapper = xulElement.wrappedJSObject;
        let scrollable = false;
        try {
          scrollable = (wrapper.scrollBoxObject != null) || (wrapper.boxObject.QueryInterface(Ci.nsIScrollBoxObject));
        } catch(e) {}
        if (scrollable) {
          this.contentScrollbox = wrapper.scrollBoxObject || wrapper.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
          return;
        }
      }
    }

    if (element)
      this.draggedFrame = element.ownerDocument.defaultView;
  },

  dragStop: function dragStop(dx, dy, scroller) {
    this.draggedFrame = null;
    this.dragMove(Browser.snapSidebars(), 0, scroller);

    Browser.tryUnfloatToolbar();

    this.bv.resumeRendering();

    // XXX shouldn't know about observer
    this.bv._idleServiceObserver.resume();
  },

  dragMove: function dragMove(dx, dy, scroller) {
    let elem = this.draggedFrame;
    let doffset = new Point(dx, dy);
    let render = false;

    // First calculate any panning to take sidebars out of view
    let panOffset = this._panControlsAwayOffset(doffset);

    // do HTML overflow or XUL panning
    if (this.contentScrollbox && !doffset.isZero()) {
      this._panScrollbox(this.contentScrollbox, doffset);
      render = true;
    }

    // Do all iframe panning
    if (elem) {
      while (elem.frameElement && !doffset.isZero()) {
        this._panFrame(elem, doffset);
        elem = elem.frameElement;
        render = true;
      }
    }

    // Do content panning
    this._panScroller(Browser.contentScrollboxScroller, doffset);

    // Any leftover panning in doffset would bring controls into view. Add to sidebar
    // away panning for the total scroll offset.
    doffset.add(panOffset);
    Browser.tryFloatToolbar(doffset.x, 0);
    this._panScroller(Browser.controlsScrollboxScroller, doffset);
    this._panScroller(Browser.pageScrollboxScroller, doffset);

    this.bv.onAfterVisibleMove();

    if (render)
      this.bv.renderNow();

    return !doffset.equals(dx, dy);
  },

  /**
  * builds a minimal implementation of scrollBoxObject for div
  */
  _createDivScrollBox: function(div) {
    let sbo = {
      getScrolledSize: function(width, height) {
        width.value = div.scrollWidth;
        height.value = div.scrollHeight;
      },
  
      getPosition: function(x, y) {
        x.value = div.scrollLeft;
        y.value = div.scrollTop;
      },
  
      scrollBy: function(dx, dy) {
        div.scrollTop += dy;
        div.scrollLeft += dx;
      }
   }
   return sbo;
  },

 /**
  * builds a minimal implementation of scrollBoxObject for trees
  */
  _createTreeScrollBox: function(tree) {
    let treeBox = tree.boxObject.QueryInterface(Ci.nsITreeBoxObject);
    let sbo = {
      pageLength: treeBox.getPageLength(),
      rowHeight: treeBox.rowHeight,
      rowWidth: treeBox.rowWidth,
      rowCount: treeBox.view.rowCount,
      targetY: treeBox.getFirstVisibleRow() * treeBox.rowHeight,
      getScrolledSize: function(width, height) {
        width.value = this.rowWidth;
        height.value = this.rowHeight * this.rowCount;
      },

      getPosition: function(x, y) {
        x.value = treeBox.horizontalPosition;
        y.value = this.targetY;
      },

      scrollBy: function(dx, dy) {
        this.targetY += dy;
        if (this.targetY < 0)
          this.targetY = 0;
        let targetRow = Math.floor(this.targetY / this.rowHeight);
        if ((targetRow + this.pageLength) > this.rowCount) {
          targetRow = this.rowCount - this.pageLength;
          this.targetY = targetRow * this.rowHeight;
        }
        treeBox.scrollToRow(targetRow);
        treeBox.scrollToHorizontalPosition(treeBox.horizontalPosition + dx);
      }
    }
    return sbo;
  },

  /**
   * pans a scrollbox, updating doffset
   */
  _panScrollbox: function(sbo, doffset) {
    let origX = {}, origY = {}, newX = {}, newY = {};

    sbo.getPosition(origX, origY);
    sbo.scrollBy(doffset.x, doffset.y);
    sbo.getPosition(newX, newY);

    doffset.subtract(newX.value - origX.value, newY.value - origY.value);
  },

  /** Return offset that pans controls away from screen. Updates doffset with leftovers. */
  _panControlsAwayOffset: function(doffset) {
    let x = 0, y = 0, rect;

    rect = Rect.fromRect(Browser.pageScrollbox.getBoundingClientRect()).map(Math.round);
    if (doffset.x < 0 && rect.right < window.innerWidth)
      x = Math.max(doffset.x, rect.right - window.innerWidth);
    if (doffset.x > 0 && rect.left > 0)
      x = Math.min(doffset.x, rect.left);

    let height = document.getElementById("tile-stack").getBoundingClientRect().height;
    rect = Rect.fromRect(Browser.contentScrollbox.getBoundingClientRect()).map(Math.round);
    if (doffset.y < 0 && rect.bottom < height)
      y = Math.max(doffset.y, rect.bottom - height);
    if (doffset.y > 0 && rect.top > 0)
      y = Math.min(doffset.y, rect.top);

    doffset.subtract(x, y);
    return new Point(x, y);
  },

  /** Pan scroller by the given amount. Updates doffset with leftovers. */
  _panScroller: function _panScroller(scroller, doffset) {
    let { x: x0, y: y0 } = Browser.getScrollboxPosition(scroller);
    scroller.scrollBy(doffset.x, doffset.y);
    let { x: x1, y: y1 } = Browser.getScrollboxPosition(scroller);
    doffset.subtract(x1 - x0, y1 - y0);
  },

  /** Pan frame by the given amount. Updates doffset with leftovers. */
  _panFrame: function _panFrame(frame, doffset) {
    let origX = {}, origY = {}, newX = {}, newY = {};
    let windowUtils = frame.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);

    windowUtils.getScrollXY(false, origX, origY);
    frame.scrollBy(doffset.x, doffset.y);
    windowUtils.getScrollXY(false, newX, newY);

    doffset.subtract(newX.value - origX.value, newY.value - origY.value);
  }
};

function nsBrowserAccess()
{
}

nsBrowserAccess.prototype = {
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIBrowserDOMWindow) || aIID.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  },

  _getBrowser: function _getBrowser(aURI, aOpener, aWhere, aContext) {
    let isExternal = (aContext == Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL);
    if (isExternal && aURI && aURI.schemeIs("chrome"))
      return null;

    let loadflags = isExternal ?
                      Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL :
                      Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    let location;
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_DEFAULTWINDOW) {
      switch (aContext) {
        case Ci.nsIBrowserDOMWindow.OPEN_EXTERNAL :
          aWhere = gPrefService.getIntPref("browser.link.open_external");
          break;
        default : // OPEN_NEW or an illegal value
          aWhere = gPrefService.getIntPref("browser.link.open_newwindow");
      }
    }

    let browser;
    if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWWINDOW) {
      let url = aURI ? aURI.spec : "about:blank";
      let newWindow = openDialog("chrome://browser/content/browser.xul", "_blank",
                                 "all,dialog=no", url, null, null, null);
      browser = newWindow.Browser.selectedBrowser;
    } else {
      if (aWhere == Ci.nsIBrowserDOMWindow.OPEN_NEWTAB)
        browser = Browser.addTab("about:blank", true).browser;
      else // OPEN_CURRENTWINDOW and illegal values
        browser = Browser.selectedBrowser;
    }
    
    try {
      let referrer;
      if (aURI) {
        if (aOpener) {
          location = aOpener.location;
          referrer = gIOService.newURI(location, null, null);
        }
        browser.loadURIWithFlags(aURI.spec, loadflags, referrer, null, null);
      }
      browser.focus();
    } catch(e) { }

    return browser;
  },

  openURI: function(aURI, aOpener, aWhere, aContext) {
    let browser = this._getBrowser(aURI, aOpener, aWhere, aContext);
    return browser ? browser.contentWindow : null;
  },

  openURIInFrame: function(aURI, aOpener, aWhere, aContext) {
    let browser = this._getBrowser(aURI, aOpener, aWhere, aContext);
    return browser ? browser.QueryInterface(Ci.nsIFrameLoaderOwner) : null;
  },

  isTabContentWindow: function(aWindow) {
    return Browser.browsers.some(function (browser) browser.contentWindow == aWindow);
  }
};

const BrowserSearch = {
  observe: function bs_observe(aSubject, aTopic, aData) {
    if (aTopic != "browser-search-engine-modified")
      return;

    switch (aData) {
      case "engine-added":
      case "engine-removed":
        // force a rebuild of the prefs list, if needed
        // XXX this is inefficient, shouldn't have to rebuild the entire list
        if (ExtensionsView._list)
          ExtensionsView.getAddonsFromLocal();

        // fall through
      case "engine-changed":
        // XXX we should probably also update the ExtensionsView list here once
        // that's efficient, since the icon can change (happen during an async
        // installs from the web)

        // blow away our cache
        this._engines = null;
        break;
      case "engine-current":
        // Not relevant
        break;
    }
  },

  get searchService() {
    delete this.searchService;
    return this.searchService = Cc["@mozilla.org/browser/search-service;1"].getService(Ci.nsIBrowserSearchService);
  },

  get engines() {
    if (this._engines)
      return this._engines;
    return this._engines = this.searchService.getVisibleEngines({ });
  },

  updatePageSearchEngines: function() {
    PageActions.removeItems("search");

    let items = Browser.selectedBrowser.searchEngines;
    if (!items.length)
      return;

    // XXX limit to the first search engine for now
    let kMaxSearchEngine = 1;
    for (let i = 0; i < kMaxSearchEngine; i++) {
      let engine = items[i];
      let item = PageActions.appendItem("search",
                                        Elements.browserBundle.getString("pageactions.search.addNew"),
                                        engine.title);

      item.engine = engine;
      item.onclick = function() {
        BrowserSearch.addPermanentSearchEngine(item.engine);
        PageActions.removeItem(item);
      };
    }
  },

  addPermanentSearchEngine: function (aEngine) {
    let iconURL = BrowserUI._favicon.src;
    this.searchService.addEngine(aEngine.href, Ci.nsISearchEngine.DATA_XML, iconURL, false);

    this._engines = null;
  },

  updateSearchButtons: function() {
    let container = document.getElementById("search-buttons");
    if (this._engines && container.hasChildNodes())
      return;

    // Clean the previous search engines button
    while (container.hasChildNodes())
      container.removeChild(container.lastChild);

    let engines = this.engines;
    for (let e = 0; e < engines.length; e++) {
      let button = document.createElement("radio");
      let engine = engines[e];
      button.id = engine.name;
      button.setAttribute("label", engine.name);
      button.className = "searchengine";
      if (engine.iconURI)
        button.setAttribute("src", engine.iconURI.spec);
      container.appendChild(button);
      button.engine = engine;
    }
  }
}

/** Watches for mouse events in chrome and sends them to content. */
function ContentCustomClicker(browserView) {
  this._browserView = browserView;
  this._overlay = document.getElementById("content-overlay");
  this._overlayTimeout = 0;
  this._width = 0;
  this._height = 0;
}

ContentCustomClicker.prototype = {
    /** Dispatch a mouse event with chrome client coordinates. */
    _dispatchMouseEvent: function _dispatchMouseEvent(element, name, cX, cY) {
      let browser = this._browserView.getBrowser();
      if (browser) {
        let [x, y] = Browser.transformClientToBrowser(cX, cY);
        let cwu = BrowserView.Util.getBrowserDOMWindowUtils(browser);
        let scrollX = {}, scrollY = {};
        cwu.getScrollXY(false, scrollX, scrollY);

        // the element can be out of the cX/cY point because of the touch radius
        // ignore the redirection if the element is a HTMLHtmlElement (bug 562981)
        let rect = Browser.getBoundingContentRect(element);
        if (!rect.isEmpty() && !(element instanceof HTMLHtmlElement) &&
            ((x < rect.left || (x > rect.left + rect.width)) || (y < rect.top || (y > rect.top + rect.height)))) {

          let point = rect.center();
          x = point.x;
          y = point.y;
        }

        x = x - scrollX.value;
        y = y - scrollY.value;
        cwu.sendMouseEvent(name, x, y, 0, 1, 0, true);
      }
    },

    /** Returns a node if selecting this node causes a focus. */
    _getFocusable: function _getFocusable(node) {
      if (node && node.mozMatchesSelector("*:link,*:visited,*:link *,*:visited *,*[role=button],button,input,option,select,textarea,label"))
        return node;
      return null;
    },

    _showCanvas: function _showCanvas(cX, cY) {
      // This code is sensitive to performance. Please profile changes you make to
      // keep this running fast.
      let bv = this._browserView;
      let overlay = this._overlay;
      let ctx = overlay.getContext("2d");
      let [elementX, elementY] = Browser.transformClientToBrowser(cX, cY);
      let element = this._getFocusable(Browser.elementFromPoint(elementX, elementY));
      if (!element)
        return;

      let rects = Browser.getContentClientRects(element);
      let union = rects.reduce(function(a, b) {
        return a.expandToContain(b);
      }, new Rect(0, 0, 0, 0)).map(bv.browserToViewport);

      let vis = Browser.getVisibleRect();
      let canvasArea = vis.intersect(union);
      this._ensureSize(canvasArea.width, canvasArea.height);

      ctx.save();
      ctx.translate(-canvasArea.left, -canvasArea.top);
      bv.browserToViewportCanvasContext(ctx);

      overlay.style.left = canvasArea.left + "px";
      overlay.style.top = canvasArea.top + "px";
      ctx.fillStyle = "rgba(0, 145, 255, .5)";
      let rect;
      let i;
      for (i = rects.length - 1; i >= 0; i--) {
        rect = rects[i];
        ctx.fillRect(rect.left, rect.top, rect.width, rect.height);
      }
      ctx.restore();
      overlay.style.display = "block";
    },

    /** Stop highlighting current element. */
    _hideCanvas: function _hideCanvas() {
      let overlay = this._overlay;
      overlay.style.display = "none";
      overlay.getContext("2d").clearRect(0, 0, this._width, this._height);

      if (this._overlayTimeout) {
        clearTimeout(this._overlayTimeout);
        this._overlayTimeout = 0;
      }
    },

    /** Make sure canvas is at least width x height. */
    _ensureSize: function _ensureSize(width, height) {
      if (this._width <= width) {
        this._width = width;
        this._overlay.width = width;
      }
      if (this._height <= height) {
        this._height = height;
        this._overlay.height = height;
      }
    },

    mouseDown: function mouseDown(cX, cY) {
      if (!this._overlayTimeout)
        this._overlayTimeout = setTimeout(function(self) { self._showCanvas(cX, cY); }, kTapOverlayTimeout, this);
    },

    mouseUp: function mouseUp(cX, cY) {
    },

    panBegin: function panBegin() {
      this._hideCanvas();
    },

    singleClick: function singleClick(cX, cY, modifiers) {
      this._hideCanvas();

      let [elementX, elementY] = Browser.transformClientToBrowser(cX, cY);
      let element = Browser.elementFromPoint(elementX, elementY);
      if (modifiers == 0) {
        if (element instanceof HTMLOptionElement)
          element = element.parentNode;

        if (gPrefService.getBoolPref("formhelper.enabled")) {
          if (FormHelper.canShowUIFor(element) && FormHelper.open(element))
            return;
        }
        else if (SelectHelper.canShowUIFor(element)) {
          SelectHelper.show(element);
          return;
        }

        gFocusManager.setFocus(element, Ci.nsIFocusManager.FLAG_NOSCROLL);

        let self = this;
        Util.executeSoon(function() {
          self._dispatchMouseEvent(element, "mousedown", cX, cY);
          self._dispatchMouseEvent(element, "mouseup", cX, cY);
        });
      }
      else if (modifiers == Ci.nsIDOMNSEvent.CONTROL_MASK) {
        let uri = Util.getHrefForElement(element);
        if (uri)
          Browser.addTab(uri, false);
      }
    },

    doubleClick: function doubleClick(cX1, cY1, cX2, cY2) {
      this._hideCanvas();

      const kDoubleClickRadius = 32;

      let maxRadius = kDoubleClickRadius * Browser._browserView.getZoomLevel();
      let isClickInRadius = (Math.abs(cX1 - cX2) < maxRadius && Math.abs(cY1 - cY2) < maxRadius);
      if (isClickInRadius && !Browser.zoomToPoint(cX1, cY1))
        Browser.zoomFromPoint(cX1, cY1);
    },

    toString: function toString() {
      return "[ContentCustomClicker] { }";
    }
};

/** Watches for mouse events in chrome and sends them to content. */
function ContentCustomKeySender(browserView) {
  this._browserView = browserView;
}

ContentCustomKeySender.prototype = {
  /** Dispatch a mouse event with chrome client coordinates. */
  dispatchKeyEvent: function _dispatchKeyEvent(event) {
    let browser = this._browserView.getBrowser();
    if (browser) {
      let fl = browser.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader;
      try {
        fl.sendCrossProcessKeyEvent(event.type, event.keyCode, event.charCode, event.modifiers);
      } catch (e) {}
    }
  },

  toString: function toString() {
    return "[ContentCustomClicker] { }";
  }
};

/** Watches for mouse click in content and redirect them to the best found target **/
const ElementTouchHelper = {
  get radius() {
    delete this.radius;
    return this.radius = { "top": gPrefService.getIntPref("browser.ui.touch.top"),
                           "right": gPrefService.getIntPref("browser.ui.touch.right"),
                           "bottom": gPrefService.getIntPref("browser.ui.touch.bottom"),
                           "left": gPrefService.getIntPref("browser.ui.touch.left")
                         };
  },

  get weight() {
    delete this.weight;
    return this.weight = { "visited": gPrefService.getIntPref("browser.ui.touch.weight.visited")
                         };
  },

  /* Retrieve the closest element to a point by looking at borders position */
  getClosest: function getClosest(aWindowUtils, aX, aY) {
    let target = aWindowUtils.elementFromPoint(aX, aY,
                                               true,   /* ignore root scroll frame*/
                                               false); /* don't flush layout */

    let nodes = aWindowUtils.nodesFromRect(aX, aY, this.radius.top,
                                                   this.radius.right,
                                                   this.radius.bottom,
                                                   this.radius.left, true, false);

    // return early if the click is just over a clickable element
    if (this._isElementClickable(target, nodes))
      return target;

    let threshold = Number.POSITIVE_INFINITY;
    for (let i = 0; i < nodes.length; i++) {
      let current = nodes[i];
      if (!current.mozMatchesSelector || !this._isElementClickable(current))
        continue;

      let rect = current.getBoundingClientRect();
      let distance = this._computeDistanceFromRect(aX, aY, rect);

      // increase a little bit the weight for already visited items
      if (current && current.mozMatchesSelector("*:visited"))
        distance *= (this.weight.visited / 100);

      if (distance < threshold) {
        target = current;
        threshold = distance;
      }
    }

    return target;
  },

  _els: Cc["@mozilla.org/eventlistenerservice;1"].getService(Ci.nsIEventListenerService),
  _clickableEvents: ["mousedown", "mouseup", "click"],
  _hasMouseListener: function _hasMouseListener(aElement) {
    let els = this._els;
    let listeners = els.getListenerInfoFor(aElement, {});
    for (let i = 0; i < listeners.length; i++) {
      if (this._clickableEvents.indexOf(listeners[i].type) != -1)
        return true;
    }
  },

  _isElementClickable: function _isElementClickable(aElement, aElementsInRect) {
    let isClickable = this._hasMouseListener(aElement);

    // If possible looks in the parents node to find a target
    if (aElement && !isClickable && aElementsInRect) {
      let parentNode = aElement.parentNode;
      let count = aElementsInRect.length;
      for (let i = 0; i < count && parentNode; i++) {
        if (aElementsInRect[i] != parentNode)
          continue;

        isClickable = this._hasMouseListener(parentNode);
        if (isClickable)
          break;

        parentNode = parentNode.parentNode;
      }
    }

    return aElement && (isClickable || aElement.mozMatchesSelector("*:link,*:visited,*[role=button],button,input,select,label"));
  },

  _computeDistanceFromRect: function _computeDistanceFromRect(aX, aY, aRect) {
    let x = 0, y = 0;
    let xmost = aRect.left + aRect.width;
    let ymost = aRect.top + aRect.height;

    // compute horizontal distance from left/right border depending if X is
    // before/inside/after the element's rectangle
    if (aRect.left < aX && aX < xmost)
      x = Math.min(xmost - aX, aX - aRect.left);
    else if (aX < aRect.left)
      x = aRect.left - aX;
    else if (aX > xmost)
      x = aX - xmost;

    // compute vertical distance from top/bottom border depending if Y is
    // above/inside/below the element's rectangle
    if (aRect.top < aY && aY < ymost)
      y = Math.min(ymost - aY, aY - aRect.top);
    else if (aY < aRect.top)
      y = aRect.top - aY;
    if (aY > ymost)
      y = aY - ymost;

    return Math.sqrt(Math.pow(x, 2) + Math.pow(y, 2));
  }
};

/**
 * Utility class to handle manipulations of the identity indicators in the UI
 */
function IdentityHandler() {
  this._staticStrings = {};
  this._staticStrings[this.IDENTITY_MODE_DOMAIN_VERIFIED] = {
    encryption_label: Elements.browserBundle.getString("identity.encrypted2")
  };
  this._staticStrings[this.IDENTITY_MODE_IDENTIFIED] = {
    encryption_label: Elements.browserBundle.getString("identity.encrypted2")
  };
  this._staticStrings[this.IDENTITY_MODE_UNKNOWN] = {
    encryption_label: Elements.browserBundle.getString("identity.unencrypted2")
  };

  this._cacheElements();
}

IdentityHandler.prototype = {

  // Mode strings used to control CSS display
  IDENTITY_MODE_IDENTIFIED       : "verifiedIdentity", // High-quality identity information
  IDENTITY_MODE_DOMAIN_VERIFIED  : "verifiedDomain",   // Minimal SSL CA-signed domain verification
  IDENTITY_MODE_UNKNOWN          : "unknownIdentity",  // No trusted identity information

  // Cache the most recent SSLStatus and Location seen in checkIdentity
  _lastStatus : null,
  _lastLocation : null,

  /**
   * Build out a cache of the elements that we need frequently.
   */
  _cacheElements: function() {
    this._identityBox = document.getElementById("identity-box");
    this._identityPopup = document.getElementById("identity-container");
    this._identityPopupContentBox = document.getElementById("identity-popup-content-box");
    this._identityPopupContentHost = document.getElementById("identity-popup-content-host");
    this._identityPopupContentOwner = document.getElementById("identity-popup-content-owner");
    this._identityPopupContentSupp = document.getElementById("identity-popup-content-supplemental");
    this._identityPopupContentVerif = document.getElementById("identity-popup-content-verifier");
    this._identityPopupEncLabel = document.getElementById("identity-popup-encryption-label");
  },

  /**
   * Helper to parse out the important parts of _lastStatus (of the SSL cert in
   * particular) for use in constructing identity UI strings
   */
  getIdentityData: function() {
    var result = {};
    var status = this._lastStatus.QueryInterface(Ci.nsISSLStatus);
    var cert = status.serverCert;

    // Human readable name of Subject
    result.subjectOrg = cert.organization;

    // SubjectName fields, broken up for individual access
    if (cert.subjectName) {
      result.subjectNameFields = {};
      cert.subjectName.split(",").forEach(function(v) {
        var field = v.split("=");
        this[field[0]] = field[1];
      }, result.subjectNameFields);

      // Call out city, state, and country specifically
      result.city = result.subjectNameFields.L;
      result.state = result.subjectNameFields.ST;
      result.country = result.subjectNameFields.C;
    }

    // Human readable name of Certificate Authority
    result.caOrg =  cert.issuerOrganization || cert.issuerCommonName;
    result.cert = cert;

    return result;
  },

  /**
   * Determine the identity of the page being displayed by examining its SSL cert
   * (if available) and, if necessary, update the UI to reflect this.
   */
  checkIdentity: function() {
    let state = Browser.selectedTab.getIdentityState();
    let location = getBrowser().contentWindow.location;
    let currentStatus = getBrowser().securityUI.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus;

    this._lastStatus = currentStatus;
    this._lastLocation = {};
    try {
      // make a copy of the passed in location to avoid cycles
      this._lastLocation = { host: location.host, hostname: location.hostname, port: location.port };
    } catch (ex) { }

    if (state & Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL)
      this.setMode(this.IDENTITY_MODE_IDENTIFIED);
    else if (state & Ci.nsIWebProgressListener.STATE_SECURE_HIGH)
      this.setMode(this.IDENTITY_MODE_DOMAIN_VERIFIED);
    else
      this.setMode(this.IDENTITY_MODE_UNKNOWN);
  },

  /**
   * Return the eTLD+1 version of the current hostname
   */
  getEffectiveHost: function() {
    // Cache the eTLDService if this is our first time through
    if (!this._eTLDService)
      this._eTLDService = Cc["@mozilla.org/network/effective-tld-service;1"]
                         .getService(Ci.nsIEffectiveTLDService);
    try {
      return this._eTLDService.getBaseDomainFromHost(this._lastLocation.hostname);
    } catch (e) {
      // If something goes wrong (e.g. hostname is an IP address) just fail back
      // to the full domain.
      return this._lastLocation.hostname;
    }
  },

  /**
   * Update the UI to reflect the specified mode, which should be one of the
   * IDENTITY_MODE_* constants.
   */
  setMode: function(newMode) {
    this._identityBox.setAttribute("mode", newMode);
    this.setIdentityMessages(newMode);

    // Update the popup too, if it's open
    if (!this._identityPopup.hidden)
      this.setPopupMessages(newMode);
  },

  /**
   * Set up the messages for the primary identity UI based on the specified mode,
   * and the details of the SSL cert, where applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setIdentityMessages: function(newMode) {
    let strings = Elements.browserBundle;

    if (newMode == this.IDENTITY_MODE_DOMAIN_VERIFIED) {
      var iData = this.getIdentityData();

      // We need a port number for all lookups.  If one hasn't been specified, use
      // the https default
      var lookupHost = this._lastLocation.host;
      if (lookupHost.indexOf(':') < 0)
        lookupHost += ":443";

      // Cache the override service the first time we need to check it
      if (!this._overrideService)
        this._overrideService = Cc["@mozilla.org/security/certoverride;1"].getService(Ci.nsICertOverrideService);

      // Verifier is either the CA Org, for a normal cert, or a special string
      // for certs that are trusted because of a security exception.
      var tooltip = strings.getFormattedString("identity.identified.verifier",
                                               [iData.caOrg]);

      // Check whether this site is a security exception. XPConnect does the right
      // thing here in terms of converting _lastLocation.port from string to int, but
      // the overrideService doesn't like undefined ports, so make sure we have
      // something in the default case (bug 432241).
      if (this._overrideService.hasMatchingOverride(this._lastLocation.hostname,
                                                    (this._lastLocation.port || 443),
                                                    iData.cert, {}, {}))
        tooltip = strings.getString("identity.identified.verified_by_you");
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();
      tooltip = strings.getFormattedString("identity.identified.verifier",
                                           [iData.caOrg]);
    }
    else {
      tooltip = strings.getString("identity.unknown.tooltip");
    }

    // Push the appropriate strings out to the UI
    this._identityBox.tooltipText = tooltip;
  },

  /**
   * Set up the title and content messages for the identity message popup,
   * based on the specified mode, and the details of the SSL cert, where
   * applicable
   *
   * @param newMode The newly set identity mode.  Should be one of the IDENTITY_MODE_* constants.
   */
  setPopupMessages: function(newMode) {
    this._identityPopup.setAttribute("mode", newMode);
    this._identityPopupContentBox.className = newMode;

    // Set the static strings up front
    this._identityPopupEncLabel.textContent = this._staticStrings[newMode].encryption_label;

    // Initialize the optional strings to empty values
    var supplemental = "";
    var verifier = "";

    let strings = Elements.browserBundle;

    if (newMode == this.IDENTITY_MODE_DOMAIN_VERIFIED) {
      var iData = this.getIdentityData();
      var host = this.getEffectiveHost();
      var owner = strings.getString("identity.ownerUnknown2");
      verifier = this._identityBox.tooltipText;
      supplemental = "";
    }
    else if (newMode == this.IDENTITY_MODE_IDENTIFIED) {
      // If it's identified, then we can populate the dialog with credentials
      iData = this.getIdentityData();
      host = this.getEffectiveHost();
      owner = iData.subjectOrg;
      verifier = this._identityBox.tooltipText;

      // Build an appropriate supplemental block out of whatever location data we have
      if (iData.city)
        supplemental += iData.city + " ";
      if (iData.state && iData.country)
        supplemental += strings.getFormattedString("identity.identified.state_and_country",
                                                   [iData.state, iData.country]);
      else if (iData.state) // State only
        supplemental += iData.state;
      else if (iData.country) // Country only
        supplemental += iData.country;
    }
    else {
      // These strings will be hidden in CSS anyhow
      host = "";
      owner = "";
    }

    // Push the appropriate strings out to the UI
    this._identityPopupContentHost.textContent = host;
    this._identityPopupContentOwner.textContent = owner;
    this._identityPopupContentSupp.textContent = supplemental;
    this._identityPopupContentVerif.textContent = verifier;

    // Update the search engines results
    BrowserSearch.updatePageSearchEngines();

    // Update the per site permissions results
    PageActions.updatePagePermissions();

    PageActions.updatePageSaveAs();
  },

  show: function ih_show() {
    // dismiss any dialog which hide the identity popup
    while (BrowserUI.activeDialog)
      BrowserUI.activeDialog.close();

    this._identityPopup.hidden = false;
    this._identityPopup.top = BrowserUI.toolbarH;
    this._identityPopup.focus();

    this._identityBox.setAttribute("open", "true");

    // Update the popup strings
    this.setPopupMessages(this._identityBox.getAttribute("mode") || this.IDENTITY_MODE_UNKNOWN);

    BrowserUI.pushPopup(this, [this._identityPopup, this._identityBox, Elements.toolbarContainer]);
    BrowserUI.lockToolbar();
  },

  hide: function ih_hide() {
    this._identityPopup.hidden = true;
    this._identityBox.removeAttribute("open");

    BrowserUI.popPopup();
    BrowserUI.unlockToolbar();
  },

  toggle: function ih_toggle() {
    if (this._identityPopup.hidden)
      this.show();
    else
      this.hide();
  },

  /**
   * Click handler for the identity-box element in primary chrome.
   */
  handleIdentityButtonEvent: function(event) {
    event.stopPropagation();

    if ((event.type == "click" && event.button != 0) ||
        (event.type == "keypress" && event.charCode != KeyEvent.DOM_VK_SPACE &&
         event.keyCode != KeyEvent.DOM_VK_RETURN))
      return; // Left click, space or enter only

    this.toggle();
  }
};

var gIdentityHandler;

/**
 * Returns the singleton instance of the identity handler class.  Should always be
 * used instead of referencing the global variable directly or creating new instances
 */
function getIdentityHandler() {
  if (!gIdentityHandler)
    gIdentityHandler = new IdentityHandler();
  return gIdentityHandler;
}


/**
 * Handler for blocked popups, triggered by DOMUpdatePageReport events in browser.xml
 */
const gPopupBlockerObserver = {
  _kIPM: Ci.nsIPermissionManager,

  onUpdatePageReport: function onUpdatePageReport(aEvent)
  {
    var cBrowser = Browser.selectedBrowser;
    if (aEvent.originalTarget != cBrowser)
      return;

    if (!cBrowser.pageReport)
      return;

    let pm = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
    let result = pm.testExactPermission(Browser.selectedBrowser.currentURI, "popup");
    if (result == Ci.nsIPermissionManager.DENY_ACTION)
      return;

    // Only show the notification again if we've not already shown it. Since
    // notifications are per-browser, we don't need to worry about re-adding
    // it.
    if (!cBrowser.pageReport.reported) {
      if(gPrefService.getBoolPref("privacy.popups.showBrowserMessage")) {
        var brandBundle = document.getElementById("bundle_brand");
        var brandShortName = brandBundle.getString("brandShortName");
        var message;
        var popupCount = cBrowser.pageReport.length;

        let strings = Elements.browserBundle;
        if (popupCount > 1)
          message = strings.getFormattedString("popupWarningMultiple", [brandShortName, popupCount]);
        else
          message = strings.getFormattedString("popupWarning", [brandShortName]);

        var notificationBox = Browser.getNotificationBox();
        var notification = notificationBox.getNotificationWithValue("popup-blocked");
        if (notification) {
          notification.label = message;
        }
        else {
          var buttons = [
            {
              label: strings.getString("popupButtonAllowOnce"),
              accessKey: null,
              callback: function() { gPopupBlockerObserver.showPopupsForSite(); }
            },
            {
              label: strings.getString("popupButtonAlwaysAllow2"),
              accessKey: null,
              callback: function() { gPopupBlockerObserver.allowPopupsForSite(); }
            },
            {
              label: strings.getString("popupButtonNeverWarn2"),
              accessKey: null,
              callback: function() { gPopupBlockerObserver.denyPopupsForSite(); }
            }
          ];

          const priority = notificationBox.PRIORITY_WARNING_MEDIUM;
          notificationBox.appendNotification(message, "popup-blocked",
                                             "",
                                             priority, buttons);
        }
      }
      // Record the fact that we've reported this blocked popup, so we don't
      // show it again.
      cBrowser.pageReport.reported = true;
    }
  },

  allowPopupsForSite: function allowPopupsForSite(aEvent) {
    var currentURI = Browser.selectedBrowser.currentURI;
    var pm = Cc["@mozilla.org/permissionmanager;1"].getService(this._kIPM);
    pm.add(currentURI, "popup", this._kIPM.ALLOW_ACTION);

    Browser.getNotificationBox().removeCurrentNotification();
  },

  denyPopupsForSite: function denyPopupsForSite(aEvent) {
    var currentURI = Browser.selectedBrowser.currentURI;
    var pm = Cc["@mozilla.org/permissionmanager;1"].getService(this._kIPM);
    pm.add(currentURI, "popup", this._kIPM.DENY_ACTION);

    Browser.getNotificationBox().removeCurrentNotification();
  },

  showPopupsForSite: function showPopupsForSite() {
    let uri = Browser.selectedBrowser.currentURI;
    let pageReport = Browser.selectedBrowser.pageReport;
    if (pageReport) {
      for (let i = 0; i < pageReport.length; ++i) {
        var popupURIspec = pageReport[i].popupWindowURI.spec;

        // Sometimes the popup URI that we get back from the pageReport
        // isn't useful (for instance, netscape.com's popup URI ends up
        // being "http://www.netscape.com", which isn't really the URI of
        // the popup they're trying to show).  This isn't going to be
        // useful to the user, so we won't create a menu item for it.
        if (popupURIspec == "" || popupURIspec == "about:blank" ||
            popupURIspec == uri.spec)
          continue;

        let popupFeatures = pageReport[i].popupWindowFeatures;
        let popupName = pageReport[i].popupWindowName;

        Browser.addTab(popupURIspec, false);
      }
    }
  }
};

const gXPInstallObserver = {
  observe: function xpi_observer(aSubject, aTopic, aData)
  {
    var brandBundle = document.getElementById("bundle_brand");
    switch (aTopic) {
      case "addon-install-blocked":
        var installInfo = aSubject.QueryInterface(Ci.amIWebInstallInfo);
        var host = installInfo.originatingURI.host;
        var brandShortName = brandBundle.getString("brandShortName");
        var notificationName, messageString, buttons;
        var strings = Elements.browserBundle;
        var enabled = true;
        try {
          enabled = gPrefService.getBoolPref("xpinstall.enabled");
        }
        catch (e) {}
        if (!enabled) {
          notificationName = "xpinstall-disabled";
          if (gPrefService.prefIsLocked("xpinstall.enabled")) {
            messageString = strings.getString("xpinstallDisabledMessageLocked");
            buttons = [];
          }
          else {
            messageString = strings.getFormattedString("xpinstallDisabledMessage",
                                                             [brandShortName, host]);
            buttons = [{
              label: strings.getString("xpinstallDisabledButton"),
              accessKey: null,
              popup: null,
              callback: function editPrefs() {
                gPrefService.setBoolPref("xpinstall.enabled", true);
                return false;
              }
            }];
          }
        }
        else {
          notificationName = "xpinstall";
          messageString = strings.getFormattedString("xpinstallPromptWarning",
                                                           [brandShortName, host]);

          buttons = [{
            label: strings.getString("xpinstallPromptAllowButton"),
            accessKey: null,
            popup: null,
            callback: function() {
              // Kick off the install
              installInfo.install();
              return false;
            }
          }];
        }

        var nBox = Browser.getNotificationBox();
        if (!nBox.getNotificationWithValue(notificationName)) {
          const priority = nBox.PRIORITY_WARNING_MEDIUM;
          const iconURL = "chrome://mozapps/skin/update/update.png";
          nBox.appendNotification(messageString, notificationName, iconURL, priority, buttons);
        }
        break;
    }
  }
};

const gSessionHistoryObserver = {
  observe: function sho_observe(subject, topic, data) {
    if (topic != "browser:purge-session-history")
      return;

    let back = document.getElementById("cmd_back");
    back.setAttribute("disabled", "true");
    let forward = document.getElementById("cmd_forward");
    forward.setAttribute("disabled", "true");

    let urlbar = document.getElementById("urlbar-edit");
    if (urlbar) {
      // Clear undo history of the URL bar
      urlbar.editor.transactionManager.clear();
    }
  }
};

var FormSubmitObserver = {
  notify: function notify(aFormElement, aWindow, aActionURI, aCancelSubmit) {
    let doc = aWindow.content.top.document;
    let tab = Browser.getTabForDocument(doc);
    if (tab)
      tab.browser.lastLocation = null;
  },

  QueryInterface : function(aIID) {
    if (!aIID.equals(Ci.nsIFormSubmitObserver) &&
        !aIID.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};


var MemoryObserver = {
  observe: function mo_observe() {
    let memory = Cc["@mozilla.org/xpcom/memory-service;1"].getService(Ci.nsIMemory);
    do {
      Browser.windowUtils.garbageCollect();
    } while (memory.isLowMemory() && Browser.sacrificeTab());
  }
};

function getNotificationBox(aWindow) {
  return Browser.getNotificationBox();
}

function importDialog(parent, src, arguments) {
  // load the dialog with a synchronous XHR
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance();
  xhr.open("GET", src, false);
  xhr.overrideMimeType("text/xml");
  xhr.send(null);
  if (!xhr.responseXML)
    return null;

  let currentNode;
  let nodeIterator = xhr.responseXML.createNodeIterator(xhr.responseXML,
                                                    NodeFilter.SHOW_TEXT,
                                                    null,
                                                    false);
  while (currentNode = nodeIterator.nextNode()) {
    let trimmed = currentNode.nodeValue.replace(/^\s\s*/, "").replace(/\s\s*$/, "");
    if (!trimmed.length)
      currentNode.parentNode.removeChild(currentNode);
  }

  let doc = xhr.responseXML.documentElement;

  var dialog  = null;

  // we need to insert before select-container if we want it to show correctly
  let selectContainer = document.getElementById("select-container");
  let parent = selectContainer.parentNode;

  // emit DOMWillOpenModalDialog event
  let event = document.createEvent("Events");
  event.initEvent("DOMWillOpenModalDialog", true, false);
  let dispatcher = parent || getBrowser();
  dispatcher.dispatchEvent(event);

  // create a full-screen semi-opaque box as a background
  let back = document.createElement("box");
  back.setAttribute("class", "modal-block");
  dialog = back.appendChild(document.importNode(doc, true));
  parent.insertBefore(back, selectContainer);

  dialog.arguments = arguments;
  dialog.parent = parent;
  return dialog;
}

function showDownloadManager(aWindowContext, aID, aReason) {
  BrowserUI.showPanel("downloads-container");
  // TODO: select the download with aID
}

var AlertsHelper = {
  _timeoutID: -1,
  _listener: null,
  _cookie: "",
  _clickable: false,

  showAlertNotification: function ah_show(aImageURL, aTitle, aText, aTextClickable, aCookie, aListener) {
    this._clickable = aTextClickable || false;
    this._listener = aListener || null;
    this._cookie = aCookie || "";

    document.getElementById("alerts-image").setAttribute("src", aImageURL);
    document.getElementById("alerts-title").value = aTitle;
    document.getElementById("alerts-text").textContent = aText;

    let container = document.getElementById("alerts-container");
    container.hidden = false;

    let rect = container.getBoundingClientRect();
    container.top = window.innerHeight - (rect.height + 20);
    container.left = window.innerWidth - (rect.width + 20);

    let timeout = gPrefService.getIntPref("alerts.totalOpenTime");
    let self = this;
    this._timeoutID = setTimeout(function() { self._timeoutAlert(); }, timeout);
  },

  _timeoutAlert: function ah__timeoutAlert() {
    this._timeoutID = -1;
    let container = document.getElementById("alerts-container");
    container.hidden = true;

    if (this._listener)
      this._listener.observe(null, "alertfinished", this._cookie);

    // TODO: add slide to UI
  },

  click: function ah_click(aEvent) {
    if (this._clickable && this._listener)
      this._listener.observe(null, "alertclickcallback", this._cookie);

    if (this._timeoutID != -1) {
      clearTimeout(this._timeoutID);
      this._timeoutAlert();
    }
  }
};

function ProgressController(tab) {
  this._tab = tab;

  // Properties used to cache security state used to update the UI
  this.state = null;
  this._hostChanged = false; // onLocationChange will flip this bit
}

ProgressController.prototype = {
  get browser() {
    return this._tab.browser;
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    // ignore notification that aren't about the main document (iframes, etc)
    if (aWebProgress.windowId != this._tab.browser.contentWindowId && this._tab.browser.contentWindowId)
      return;

    // If you want to observe other state flags, be sure they're listed in the
    // Tab._createBrowser's call to addProgressListener
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_START)
        this._networkStart();
      else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP)
        this._networkStop();
    }

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
#ifdef MOZ_CRASH_REPORTER
        if (aRequest instanceof Ci.nsIChannel && CrashReporter.enabled)
          CrashReporter.annotateCrashReport("URL", aRequest.URI.spec);
#endif
      }
      else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        this._documentStop();
      }
    }
  },

  /** This method is called to indicate progress changes for the currently loading page. */
  onProgressChange: function onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
    // To use this method, add NOTIFY_PROGRESS to the flags in Tab._createBrowser
  },

  /** This method is called to indicate a change to the current location. */
  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocationURI) {
    // ignore notification that aren't about the main document (iframes, etc)
    if (aWebProgress.windowId != this._tab.browser.contentWindowId)
      return;

    let spec = aLocationURI ? aLocationURI.spec : "";
    let location = spec.split("#")[0]; // Ignore fragment identifier changes.

    this._hostChanged = true;

    if (location != this.browser.lastLocation) {
      this.browser.lastLocation = location;
      Browser.removeTransientNotificationsForTab(this._tab);
      this._tab.resetZoomLevel();

      if (this._tab == Browser.selectedTab) {
        BrowserUI.updateURI();

        // We're about to have new page content, so scroll the content area
        // to the top so the new paints will draw correctly.
        // (background tabs are delayed scrolled to top in _documentStop)
        Browser.scrollContentToTop();
      }
    }
  },

  /**
   * This method is called to indicate a status changes for the currently
   * loading page.  The message is already formatted for display.
   */
  onStatusChange: function onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    // To use this method, add NOTIFY_STATUS to the flags in Tab._createBrowser
  },

  /** This method is called when the security state of the browser changes. */
  onSecurityChange: function onSecurityChange(aWebProgress, aRequest, aState) {
    // Don't need to do anything if the data we use to update the UI hasn't changed
    if (this.state == aState && !this._hostChanged)
      return;

    this._hostChanged = false;
    this.state = aState;

    if (this._tab == Browser.selectedTab) {
      getIdentityHandler().checkIdentity();
    }
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  _networkStart: function _networkStart() {
    this._tab.startLoading();

    if (this._tab == Browser.selectedTab) {
      BrowserUI.update(TOOLBARSTATE_LOADING);

      // We should at least show something in the URLBar until
      // the load has progressed further along
      if (this._tab.browser.currentURI.spec == "about:blank")
        BrowserUI.updateURI();
    }

    // broadcast a URLChanged message for consumption by InputHandler
    let event = document.createEvent("Events");
    event.initEvent("URLChanged", true, false);
    this.browser.dispatchEvent(event);
  },

  _networkStop: function _networkStop() {
    this._tab.endLoading();

    if (this._tab == Browser.selectedTab) {
      BrowserUI.update(TOOLBARSTATE_LOADED);
      this.browser.docShell.isOffScreenBrowser = true;
    }

    if (this.browser.currentURI.spec != "about:blank")
      this._tab.updateThumbnail();
  },

  _documentStop: function _documentStop() {
    if (this._tab == Browser.selectedTab) {
      // XXX Sometimes MozScrollSizeChange has not occurred, so the scroll pane will not
      // be resized yet. We are assuming this event is on the queue, so scroll the pane
      // "soon."
      Util.executeSoon(function() {
        let scroll = Browser.getScrollboxPosition(Browser.contentScrollboxScroller);
        if (scroll.isZero())
          Browser.scrollContentToBrowser();
      });
    }
    else {
      let scroll = BrowserView.Util.getContentScrollOffset(this._tab.browser);
      this._tab.contentScrollOffset = new Point(scroll.x, scroll.y);

      // If the document content is scrolled to the top, make sure the URLbar is in view.
      // If this were the selected tab, onLocationChange would scroll to top.
      if (scroll.isZero())
        this._tab.pageScrollOffset = new Point(0, 0);
    }
  }
};

var OfflineApps = {
  get _pm() {
    delete this._pm;
    return this._pm = Cc["@mozilla.org/permissionmanager;1"].getService(Ci.nsIPermissionManager);
  },

  offlineAppRequested: function(aRequest) {
    if (!gPrefService.getBoolPref("browser.offline-apps.notify"))
      return;

    let currentURI = gIOService.newURI(aRequest.location, aRequest.charset, null);

    // don't bother showing UI if the user has already made a decision
    if (this._pm.testExactPermission(currentURI, "offline-app") != Ci.nsIPermissionManager.UNKNOWN_ACTION)
      return;

    try {
      if (gPrefService.getBoolPref("offline-apps.allow_by_default")) {
        // all pages can use offline capabilities, no need to ask the user
        return;
      }
    } catch(e) {
      // this pref isn't set by default, ignore failures
    }

    let host = currentURI.asciiHost;
    let notificationID = "offline-app-requested-" + host;
    let notificationBox = Browser.getNotificationBox();

    let notification = notificationBox.getNotificationWithValue(notificationID);
    let strings = Elements.browserBundle;
    if (notification) {
      notification.documents.push(aRequest);
    } else {
      let buttons = [{
        label: strings.getString("offlineApps.allow"),
        accessKey: null,
        callback: function() {
          for (let i = 0; i < notification.documents.length; i++)
            OfflineApps.allowSite(notification.documents[i]);
        }
      },{
        label: strings.getString("offlineApps.never"),
        accessKey: null,
        callback: function() {
          for (let i = 0; i < notification.documents.length; i++)
            OfflineApps.disallowSite(notification.documents[i]);
        }
      },{
        label: strings.getString("offlineApps.notNow"),
        accessKey: null,
        callback: function() { /* noop */ }
      }];

      const priority = notificationBox.PRIORITY_INFO_LOW;
      let message = strings.getFormattedString("offlineApps.available", [host]);
      notification = notificationBox.appendNotification(message, notificationID,
                                                        "", priority, buttons);
      notification.documents = [aRequest];
    }
  },

  allowSite: function(aRequest) {
    let currentURI = gIOService.newURI(aRequest.location, aRequest.charset, null);
    this._pm.add(currentURI, "offline-app", Ci.nsIPermissionManager.ALLOW_ACTION);

    // When a site is enabled while loading, manifest resources will start
    // fetching immediately.  This one time we need to do it ourselves.
    this._startFetching(aRequest);
  },

  disallowSite: function(aRequest) {
    let currentURI = gIOService.newURI(aRequest.location, aRequest.charset, null);
    this._pm.add(currentURI, "offline-app", Ci.nsIPermissionManager.DENY_ACTION);
  },

  _startFetching: function(aRequest) {
    let currentURI = gIOService.newURI(aRequest.location, aRequest.charset, null);
    let manifestURI = gIOService.newURI(aRequest.manifest, aRequest.charset, currentURI);

    let updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"].getService(Ci.nsIOfflineCacheUpdateService);
    updateService.scheduleUpdate(manifestURI, currentURI);
  },

  receiveMessage: function receiveMessage(aMessage) {
    if (aMessage.name == "Browser:MozApplicationManifest") {
      this.offlineAppRequested(aMessage.json);
    }
  }
};

function Tab() {
  this._id = null;
  this._browser = null;
  this._browserViewportState = null;
  this._state = null;
  this._listener = null;
  this._loading = false;
  this._chromeTab = null;
  this._resizeAndPaint = Util.bind(this._resizeAndPaint, this);

  // Set to 0 since new tabs that have not been viewed yet are good tabs to
  // toss if app needs more memory.
  this.lastSelected = 0;

  this.create();
}

Tab.prototype = {
  get browser() {
    return this._browser;
  },

  get browserViewportState() {
    return this._browserViewportState;
  },

  get chromeTab() {
    return this._chromeTab;
  },

  /**
   * Throttles redraws to once every 2 seconds while loading the page, zooming to fit page if
   * user hasn't started zooming.
   */
  _resizeAndPaint: function _resizeAndPaint() {
    let bv = Browser._browserView;

    bv.commitBatchOperation();

    // kick ourselves off 2s later while we're still loading
    bv.beginBatchOperation();
    this._loadingTimeout = setTimeout(this._resizeAndPaint, 2000);
  },

  _startResizeAndPaint: function _startResizeAndPaint() {
    if (this._loadingTimeout)
      throw "Already have a loading timeout";

    Browser._browserView.beginBatchOperation();
    this._loadingTimeout = setTimeout(this._resizeAndPaint, 2000);
  },

  _stopResizeAndPaint: function _stopResizeAndPaint() {
    if (!this._loadingTimeout)
      throw "No loading timeout!";

    clearTimeout(this._loadingTimeout);
    delete this._loadingTimeout;
    Browser._browserView.commitBatchOperation();
  },

  /** Update browser styles when the viewport metadata changes. */
  updateViewportMetadata: function updateViewportMetadata(metaData) {
    let browser = this._browser;
    if (!browser)
      return;

    this._browserViewportState.metaData = metaData;

    // Remove any previous styles.
    browser.className = "";
    browser.style.removeProperty("width");
    browser.style.removeProperty("height");

    // Add classes for auto-sizing viewports.
    if (metaData.autoSize) {
      if (metaData.defaultZoom == 1.0) {
        browser.classList.add("window-width");
        browser.classList.add("window-height");
      } else {
        browser.classList.add("viewport-width");
        browser.classList.add("viewport-height");
      }
    }
    this.updateViewportSize();
  },

  /** Update browser size when the metadata or the window size changes. */
  updateViewportSize: function updateViewportSize() {
    let browser = this._browser;
    if (!browser)
      return;

    let metaData = this._browserViewportState.metaData || {};
    if (!metaData.autoSize) {
      let screenW = window.innerWidth;
      let screenH = window.innerHeight;
      let viewportW = metaData.width;
      let viewportH = metaData.height;

      // If (scale * width) < device-width, increase the width (bug 561413).
      let maxInitialZoom = metaData.defaultZoom || metaData.maxZoom;
      if (maxInitialZoom && viewportW)
        viewportW = Math.max(viewportW, screenW / maxInitialZoom);

      let validW = viewportW > 0;
      let validH = viewportH > 0;

      if (validW && !validH) {
        viewportH = viewportW * (screenH / screenW);
      } else if (!validW && validH) {
        viewportW = viewportH * (screenW / screenH);
      } else {
        viewportW = kDefaultBrowserWidth;
        viewportH = kDefaultBrowserWidth * (screenH / screenW);
      }

      browser.style.width = viewportW + "px";
      browser.style.height = viewportH + "px";
    }

    // Some documents are not firing MozScrolledAreaChanged and/or fired it for
    // sub-documents only
    let doc = browser.contentDocument;
    if (doc instanceof XULDocument || doc.body instanceof HTMLFrameSetElement) {
       let [width, height] = BrowserView.Util.getBrowserDimensions(browser);
       BrowserView.Util.ensureMozScrolledAreaEvent(browser, width, height);
    }
  },

  /** Returns tab's identity state for updating security UI. */
  getIdentityState: function getIdentityState() {
    return this._listener.state;
  },

  startLoading: function startLoading() {
    if (this._loading) throw "Already Loading!";

    this._loading = true;

    if (!this._loadingTimeout) {
      let bv = Browser._browserView;

      this._startResizeAndPaint();
      if (this == Browser.selectedTab) {
        bv.invalidateEntireView();
        bv.setAggressive(false);
        // Sync up browser so previous and forward scroll positions are set. This is a good time to do
        // this because the resulting invalidation is irrelevant.
        bv.ignorePageScroll(true);
        Browser.scrollBrowserToContent();
      }
    }
  },

  endLoading: function endLoading() {
    if (!this._loading) throw "Not Loading!";

    this.setIcon(this._browser.mIconURL);
    this._loading = false;

    if (this == Browser.selectedTab) {
      let bv = Browser._browserView;
      bv.ignorePageScroll(false);
      bv.setAggressive(true);
    }

    this._stopResizeAndPaint();

    // if this tab was sacrificed previously, restore its state
    this.restoreState();
  },

  isLoading: function isLoading() {
    return this._loading;
  },

  load: function load(uri) {
    this._browser.setAttribute("src", uri);
  },

  create: function create() {
    // Initialize a viewport state for BrowserView
    this._browserViewportState = BrowserView.Util.createBrowserViewportState();

    this._chromeTab = document.getElementById("tabs").addTab();
    this._createBrowser();
  },

  destroy: function destroy() {
    document.getElementById("tabs").removeTab(this._chromeTab);
    this._chromeTab = null;
    this._destroyBrowser();
  },

  /** Create browser if it doesn't already exist. */
  ensureBrowserExists: function ensureBrowserExists() {
    if (!this._browser) {
      this._createBrowser();
      this.browser.contentDocument.location = this._state._url;
    }
  },

  _createBrowser: function _createBrowser() {
    if (this._browser)
      throw "Browser already exists";

    // Create the browser using the current width the dynamically size the height
    let browser = this._browser = document.createElement("browser");

    browser.setAttribute("style", "overflow: -moz-hidden-unscrollable; visibility: hidden;");
    browser.setAttribute("type", "content");
    browser.setAttribute("remote", "false");

    // Append the browser to the document, which should start the page load
    document.getElementById("browsers").appendChild(browser);

    // stop about:blank from loading
    browser.stop();

    // Attach a separate progress listener to the browser
    let flags = Ci.nsIWebProgress.NOTIFY_LOCATION |
                Ci.nsIWebProgress.NOTIFY_SECURITY |
                Ci.nsIWebProgress.NOTIFY_STATE_NETWORK |
                Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT;
    this._listener = new ProgressController(this);
    browser.webProgress.addProgressListener(this._listener, flags);
  },

  _destroyBrowser: function _destroyBrowser() {
    if (this._browser) {
      var browser = this._browser;
      browser.removeProgressListener(this._listener);

      this._browser = null;
      this._listener = null;
      this._loading = false;

      try { // this will throw if we're not loading
        this._stopResizeAndPaint();
      } catch(ex) {}

      Util.executeSoon(function() {
        document.getElementById("browsers").removeChild(browser);
      });
    }
  },

  /** Serializes as much state as possible of the current content.  */
  saveState: function saveState() {
    let state = { };

    var browser = this._browser;
    var doc = browser.contentDocument;
    state._url = doc.location.href;
    state._scroll = BrowserView.Util.getContentScrollOffset(this.browser);
    if (doc instanceof HTMLDocument) {
      var tags = ["input", "textarea", "select"];

      for (var t = 0; t < tags.length; t++) {
        var elements = doc.getElementsByTagName(tags[t]);
        for (var e = 0; e < elements.length; e++) {
          var element = elements[e];
          var id;
          if (element.id)
            id = "#" + element.id;
          else if (element.name)
            id = "$" + element.name;

          if (id)
            state[id] = element.value;
        }
      }
    }

    this._state = state;
  },

  /** Restores serialized content from saveState.  */
  restoreState: function restoreState() {
    let state = this._state;
    if (!state)
      return;

    let doc = this._browser.contentDocument;

    for (var item in state) {
      var elem = null;
      if (item.charAt(0) == "#") {
        elem = doc.getElementById(item.substring(1));
      } else if (item.charAt(0) == "$") {
        var list = doc.getElementsByName(item.substring(1));
        if (list.length)
          elem = list[0];
      }

      if (elem)
        elem.value = state[item];
    }

    this.browser.contentWindow.scrollX = state._scroll.x;
    this.browser.contentWindow.scrollY = state._scroll.y;

    this._state = null;
  },

  /* Set up the initial zoom level. While the bvs.defaultZoomLevel of a tab is
     equal to bvs.zoomLevel this mean that not user action has happended and
     we can safely alter the zoom on a window resize or on a page load
  */
  resetZoomLevel: function resetZoomLevel() {
    let bvs = this._browserViewportState;
    bvs.defaultZoomLevel = bvs.zoomLevel;
  },

  updateThumbnail: function updateThumbnail() {
    if (!this._browser)
      return;

    let bv = Browser._browserView;
    let browserView = (Browser.selectedBrowser == this._browser && bv.isDefaultZoom()) ? Browser._browserView 
                                                                                       : null;
    this._chromeTab.updateThumbnail(this._browser, browserView);
  },

  setIcon: function setIcon(aURI) {
    let faviconURI = null;
    if (aURI) {
      try {
        faviconURI = gIOService.newURI(aURI, null, null);
      }
      catch (e) {
        faviconURI = null;
      }
    }

    if (!faviconURI || faviconURI.schemeIs("javascript") || gFaviconService.isFailedFavicon(faviconURI)) {
      try {
        // Use documentURIObject in the favicon construction so that we
        // do the right thing with about:-style error pages.  Bug 515188
        faviconURI = gIOService.newURI(this._browser.contentDocument.documentURIObject.prePath + "/favicon.ico", null, null);
        gFaviconService.setAndLoadFaviconForPage(this._browser.currentURI, faviconURI, true);
      }
      catch (e) {
        faviconURI = null;
      }
      if (faviconURI && gFaviconService.isFailedFavicon(faviconURI))
        faviconURI = null;
    }

    this._browser.mIconURL = faviconURI ? faviconURI.spec : "";
  },

  toString: function() {
    return "[Tab " + (this._browser ? this._browser.contentDocument.location.toString() : "(no browser)") + "]";
  }
};

var ImagePreloader = {
  cache: function ip_cache() {
    // Preload images used in border-image CSS
    let images = ["button-active", "button-default",
                  "buttondark-active", "buttondark-default",
                  "toggleon-active", "toggleon-inactive",
                  "toggleoff-active", "toggleoff-inactive",
                  "toggleleft-active", "toggleleft-inactive",
                  "togglemiddle-active", "togglemiddle-inactive",
                  "toggleright-active", "toggleright-inactive",
                  "toggleboth-active", "toggleboth-inactive",
                  "toggledarkleft-active", "toggledarkleft-inactive",
                  "toggledarkmiddle-active", "toggledarkmiddle-inactive",
                  "toggledarkright-active", "toggledarkright-inactive",
                  "toggledarkboth-active", "toggledarkboth-inactive",
                  "toolbarbutton-active", "toolbarbutton-default",
                  "addons-active", "addons-default",
                  "downloads-active", "downloads-default",
                  "preferences-active", "preferences-default",
                  "settings-active", "settings-open"];

    let size = screen.width > 400 ? "-64" : "-36";
    for (let i = 0; i < images.length; i++) {
      let image = new Image();
      image.src = "chrome://browser/skin/images/" + images[i] + size + ".png";
    }
  }
}

const nsIObjectLoadingContent = Ci.nsIObjectLoadingContent_MOZILLA_1_9_2_BRANCH || Ci.nsIObjectLoadingContent;

/**
 * Allows fast-path embed rendering by letting objects know where to absolutely
 * render on the screen.
 */
function PluginObserver(bv) {
  this._emptyRect = new Rect(0, 0, 0, 0);
  this._contentShowing = document.getElementById("observe_contentShowing");
  this._bv = bv;
  this._started = false;
  this._isRendering = false;

  let disabled = gPrefService.getBoolPref("plugin.disable");
  if (!disabled)
    this.start();
}

PluginObserver.prototype = {
  // When calculating critical rect, subtract N pixels around popup boxes
  POPUP_PADDING: 4,

  /** Starts flash objects fast path. */
  start: function() {
    if (this._started)
      return;
    this._started = true;

    document.getElementById("tabs-container").addEventListener("TabSelect", this, false);
    this._contentShowing.addEventListener("broadcast", this, false);
    let browsers = document.getElementById("browsers");
    browsers.addEventListener("RenderStateChanged", this, false);
    gObserverService.addObserver(this, "plugin-changed-event", false);
    Elements.stack.addEventListener("PopupChanged", this, false);

    let browser = Browser.selectedBrowser;
    if (browser) {
      browser.addEventListener("ZoomChanged", this, false);
      browser.addEventListener("MozAfterPaint", this, false);
    }
  },

  /** Stops listening for events. */
  stop: function() {
    if (!this._started)
      return;
    this._started = false;

    document.getElementById("tabs-container").removeEventListener("TabSelect", this, false);
    this._contentShowing.removeEventListener("broadcast", this, false);
    let browsers = document.getElementById("browsers");
    browsers.removeEventListener("RenderStateChanged", this, false);
    gObserverService.removeObserver(this, "plugin-changed-event");
    Elements.stack.removeEventListener("PopupChanged", this, false);

    let browser = Browser.selectedBrowser;
    if (browser) {
      browser.removeEventListener("ZoomChanged", this, false);
      browser.removeEventListener("MozAfterPaint", this, false);
    }
  },

  /** Observe listens for plugin change events and maintains an embed cache. */
  observe: function observe(subject, topic, data) {
    if (topic == "plugin-changed-event")
      this.updateCurrentBrowser();
  },

  /** Update flash objects */
  handleEvent: function handleEvent(ev) {
    if (ev.type == "TabSelect") {
      if (ev.lastTab) {
        let browser = ev.lastTab.browser;
        let oldDoc = browser.contentDocument;

        browser.removeEventListener("ZoomChanged", this, false);
        browser.removeEventListener("MozAfterPaint", this, false);
        this.updateEmbedRegions(this.getPluginNodes(oldDoc), this._emptyRect);
      }

      let browser = Browser.selectedBrowser;
      browser.addEventListener("ZoomChanged", this, false);
      browser.addEventListener("MozAfterPaint", this, false);
    }

    this.updateCurrentBrowser();
  },

  /** Find all embedded objects in document and its frames */
  getPluginNodes: function getPluginNodes(doc) {
    let docs = Util.getAllDocuments(doc);
    let result = [];

    let i;
    let plugins;
    for (i = 0; i < docs.length; i++) {
      plugins = docs[i].querySelectorAll("embed,object");
      result.push.apply(result, Array.prototype.slice.call(plugins));
    }

    return result;
  },

  /** Update the current browser's flash objects. */
  updateCurrentBrowser: function updateCurrentBrowser() {
    let doc = Browser.selectedTab.browser.contentDocument;

    let rect = this.getCriticalRect();
    if (rect == this._emptyRect && !this._isRendering)
      return;

    if (this._isRendering) {
      // Update immediately if not just starting to render
      if (rect == this._emptyRect)
        this._isRendering = false;
      this.updateEmbedRegions(this.getPluginNodes(doc), rect);
    } else {
      // Wait a moment so that any chrome redraws occur first.
      let self = this;
      setTimeout(function() {
        self._isRendering = true;
        // Recalculate critical rect so we don't render when we ought not to.
        self.updateEmbedRegions(self.getPluginNodes(doc), self.getCriticalRect());
      }, 0);
    }
  },

  /** More accurate version of finding the current visible region. Returns client coords. */
  getCriticalRect: function getCriticalRect() {
    let bv = this._bv;
    if (Browser.selectedTab._loading)
      return this._emptyRect;
    if (!bv.isRendering())
      return this._emptyRect;
    if (Elements.contentShowing.hasAttribute("disabled"))
      return this._emptyRect;

    let vs = bv._browserViewportState;
    let vr = bv.getVisibleRect();
    let crit = BrowserView.Util.visibleRectToCriticalRect(vr, vs);
    crit = Browser.browserViewToClientRect(crit);

    if (BrowserUI.isToolbarLocked()) {
      let urlbarRect = Elements.toolbarContainer.getBoundingClientRect();
      // Subtract urlbar area from critical rect area.  In general subtracting a rect from another
      // results in a region that can be described by a union of rects.  Luckily in this case,
      // we can cheat because the resulting area is still just one rect.
      crit.top = Math.max(Math.round(urlbarRect.height) + 1, crit.top);
      // XXX we add 1 to the height so that flash overlays don't leak into URL bar (otherwise
      // you may see a strip of junk if you pop up the identity panel over flash)
    }

    let popup = BrowserUI._popup;
    if (popup) {
      let p = this.POPUP_PADDING;
      let elements = BrowserUI._popup.elements;
      for (let i = elements.length - 1; i >= 0; i--) {
        let popupRect = Rect.fromRect(elements[i].getBoundingClientRect()).expandToIntegers();
        // XXX Our CSS shadows don't seem to be included in getBoundingClientRect.  Compensate
        // with some padding; may need to be changed depending on the theme. Otherwise, flash
        // can "leak into" the popup.
        popupRect.setBounds(popupRect.left - p, popupRect.top - p, popupRect.right + p, popupRect.bottom + p);
        let areaRects = crit.subtract(popupRect);
        if (areaRects.length == 1) {
          // Yay, critical region is still just a rect!
          crit = areaRects[0];
        } else if (areaRects.length > 1) {
          // Critical region is a union of rects. Give up.
          return this._emptyRect;
        }
      }
    }

    return crit;
  },

  /**
   * Tell embedded objects where to absolutely render, using crit for clipping.
   * @param crit Specified in client coordinates
   */
  updateEmbedRegions: function updateEmbedRegions(objects, crit) {
    let bv = this._bv;
    let oprivate, r, dest, clip;
    for (let i = objects.length - 1; i >= 0; i--) {
      r = bv.browserToViewportRect(Browser.getBoundingContentRect(objects[i]));
      dest = Browser.browserViewToClientRect(r);
      clip = dest.intersect(crit).translate(-dest.left, -dest.top);
      oprivate = objects[i].QueryInterface(nsIObjectLoadingContent);
      try {
        oprivate.setAbsoluteScreenPosition(Browser.contentScrollbox, dest, clip);
      } catch(e) {};
    }
  }
};
