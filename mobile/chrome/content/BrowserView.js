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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roy Frostig <rfrostig@mozilla.com>
 *   Stuart Parmenter <stuart@mozilla.com>
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

let Ci = Components.interfaces;

const kBrowserFormZoomLevelMin = 1.0;
const kBrowserFormZoomLevelMax = 2.0;
const kBrowserViewZoomLevelPrecision = 10000;
const kBrowserViewPrefetchBeginIdleWait = 1;    // seconds
const kBrowserViewPrefetchBeginIdleWaitLoading = 10;    // seconds
const kBrowserViewCacheSize = 6;

/**
 * A BrowserView maintains state of the viewport (browser, zoom level,
 * dimensions) and the visible rectangle into the viewport, for every
 * browser it is given (cf setBrowser()).  In updates to the viewport state,
 * a BrowserView (using its TileManager) renders parts of the page quasi-
 * intelligently, with guarantees of having rendered and appended all of the
 * visible browser content (aka the "critical rectangle").
 *
 * State is characterized in large part by two rectangles (and an implicit third):
 * - Viewport: Always rooted at the origin, ie with (left, top) at (0, 0).  The
 *     width and height (right and bottom) of this rectangle are that of the
 *     current viewport, which corresponds more or less to the transformed
 *     browser content (scaled by zoom level).
 * - Visible: Corresponds to the client's viewing rectangle in viewport
 *     coordinates.  Has (top, left) corresponding to position, and width & height
 *     corresponding to the clients viewing dimensions.  Take note that the top
 *     and left of the visible rect are per-browser state, but that the width
 *     and height persist across setBrowser() calls.  This is best explained by
 *     a simple example: user views browser A, pans to position (x0, y0), switches
 *     to browser B, where she finds herself at position (x1, y1), tilts her
 *     device so that visible rectangle's width and height change, and switches
 *     back to browser A.  She expects to come back to position (x0, y0), but her
 *     device remains tilted.
 * - Critical (the implicit one): The critical rectangle is the (possibly null)
 *     intersection of the visible and viewport rectangles.  That is, it is that
 *     region of the viewport which is visible to the user.  We care about this
 *     because it tells us which region must be rendered as soon as it is dirtied.
 *     The critical rectangle is mostly state that we do not keep in BrowserView
 *     but that our TileManager maintains.
 *
 * Example rectangle state configurations:
 *
 *
 *        +-------------------------------+
 *        |A                              |
 *        |                               |
 *        |                               |
 *        |                               |
 *        |        +----------------+     |
 *        |        |B,C             |     |
 *        |        |                |     |
 *        |        |                |     |
 *        |        |                |     |
 *        |        +----------------+     |
 *        |                               |
 *        |                               |
 *        |                               |
 *        |                               |
 *        |                               |
 *        +-------------------------------+
 *
 *
 * A = viewport ; at (0, 0)
 * B = visible  ; at (x, y) where x > 0, y > 0
 * C = critical ; at (x, y)
 *
 *
 *
 *        +-------------------------------+
 *        |A                              |
 *        |                               |
 *        |                               |
 *        |                               |
 *   +----+-----------+                   |
 *   |B   .C          |                   |
 *   |    .           |                   |
 *   |    .           |                   |
 *   |    .           |                   |
 *   +----+-----------+                   |
 *        |                               |
 *        |                               |
 *        |                               |
 *        |                               |
 *        |                               |
 *        +-------------------------------+
 *
 *
 * A = viewport ; at (0, 0)
 * B = visible  ; at (x, y) where x < 0, y > 0
 * C = critical ; at (0, y)
 *
 *
 * Maintaining per-browser state is a little bit of a hack involving attaching
 * an object as the obfuscated dynamic JS property of the browser object, that
 * hopefully no one but us will touch.  See BrowserView.Util.getViewportStateFromBrowser()
 * for the property name.
 */
function BrowserView(container, visibleRectFactory) {
  Util.bindAll(this);
  this.init(container, visibleRectFactory);
}


// -----------------------------------------------------------
// Util/convenience functions.
//
// These are mostly for use by BrowserView itself, but if you find them handy anywhere
// else, feel free.
//

BrowserView.Util = {
  visibleRectToCriticalRect: function visibleRectToCriticalRect(visibleRect, browserViewportState) {
    return visibleRect.intersect(browserViewportState.viewportRect);
  },

  createBrowserViewportState: function createBrowserViewportState() {
    return new BrowserView.BrowserViewportState(new Rect(0, 0, 1, 1), 0, 0, 1);
  },

  getViewportStateFromBrowser: function getViewportStateFromBrowser(browser) {
    return browser.__BrowserView__vps;
  },

  /**
   * Calling this is likely to cause a reflow of the browser's document.  Use
   * wisely.
   */
  getBrowserDimensions: function getBrowserDimensions(browser) {
    let cdoc = browser.contentDocument;
    if (cdoc instanceof SVGDocument) {
      let rect = cdoc.rootElement.getBoundingClientRect();
      return [Math.ceil(rect.width), Math.ceil(rect.height)];
    }

    // These might not exist yet depending on page load state
    let body = cdoc.body || {};
    let html = cdoc.documentElement || {};
    let w = Math.max(body.scrollWidth || 1, html.scrollWidth);
    let h = Math.max(body.scrollHeight || 1, html.scrollHeight);

    return [w, h];
  },

  getContentScrollOffset: function getContentScrollOffset(browser) {
    let cwu = BrowserView.Util.getBrowserDOMWindowUtils(browser);
    let scrollX = {};
    let scrollY = {};
    cwu.getScrollXY(false, scrollX, scrollY);

    return new Point(scrollX.value, scrollY.value);
  },

  getBrowserDOMWindowUtils: function getBrowserDOMWindowUtils(browser) {
    return browser.contentWindow
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);
  },

  getNewBatchOperationState: function getNewBatchOperationState() {
    return {
      viewportSizeChanged: false,
      dirtyAll: false
    };
  },

  initContainer: function initContainer(container, visibleRect) {
    container.style.width = visibleRect.width  + 'px';
    container.style.height = visibleRect.height + 'px';
    container.style.overflow = '-moz-hidden-unscrollable';
  },

  resizeContainerToViewport: function resizeContainerToViewport(container, viewportRect) {
    container.style.width = viewportRect.width  + 'px';
    container.style.height = viewportRect.height + 'px';
  },

  ensureMozScrolledAreaEvent: function ensureMozScrolledAreaEvent(aBrowser, aWidth, aHeight) {
    let message = {};
    message.target = aBrowser;
    message.name = "Browser:MozScrolledAreaChanged";
    message.json = { width: aWidth, height: aHeight };

    Browser._browserView.updateScrolledArea(message);
  }
};

BrowserView.prototype = {

  // -----------------------------------------------------------
  // Public instance methods
  //

  init: function init(container, visibleRectFactory) {
    this._batchOps = [];
    this._container = container;
    this._browser = null;
    this._browserViewportState = null;
    this._renderMode = 0;
    this._offscreenDepth = 0;

    let cacheSize = kBrowserViewCacheSize;
    try {
      cacheSize = gPrefService.getIntPref("tile.cache.size");
    } catch(e) {}

    if (cacheSize == -1) {
      let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
      let device = sysInfo.get("device");
      switch (device) {
#ifdef MOZ_PLATFORM_MAEMO
        case "Nokia N900":
          cacheSize = 26;
          break;
        case "Nokia N8xx":
          // N8xx has half the memory of N900 and crashes with higher numbers
          cacheSize = 10;
          break;
#endif
        default:
          // Use a minimum number of tiles sice we don't know the device
          cacheSize = 6;
      }
    }

    this._tileManager = new TileManager(this._appendTile, this._removeTile, this, cacheSize);
    this._visibleRectFactory = visibleRectFactory;

    this._idleServiceObserver = new BrowserView.IdleServiceObserver(this);
    this._idleService = Cc["@mozilla.org/widget/idleservice;1"].getService(Ci.nsIIdleService);
    this._idleService.addIdleObserver(this._idleServiceObserver, kBrowserViewPrefetchBeginIdleWait);
    this._idleServiceWait = kBrowserViewPrefetchBeginIdleWait;

    let self = this;
    messageManager.addMessageListener("Browser:MozScrolledAreaChanged", this);
    messageManager.addMessageListener("Browser:MozAfterPaint", this);
    messageManager.addMessageListener("Browser:PageScroll", this);
  },

  uninit: function uninit() {
    this.setBrowser(null, null);
    this._idleService.removeIdleObserver(this._idleServiceObserver, this._idleServiceWait);
  },

  /** When aggressive, spend more time rendering tiles. */
  setAggressive: function setAggressive(aggro) {
    let wait = aggro ? kBrowserViewPrefetchBeginIdleWait : kBrowserViewPrefetchBeginIdleWaitLoading;
    this._idleService.removeIdleObserver(this._idleServiceObserver, this._idleServiceWait);
    this._idleService.addIdleObserver(this._idleServiceObserver, wait);
    this._idleServiceWait = wait;
  },

  getVisibleRect: function getVisibleRect() {
    return this._visibleRectFactory();
  },

  /**
   * @return [width, height]
   */
  getViewportDimensions: function getViewportDimensions() {
    let bvs = this._browserViewportState;
    if (!bvs)
      throw "Cannot get viewport dimensions when no browser is set";

    return [bvs.viewportRect.right, bvs.viewportRect.bottom];
  },

  setZoomLevel: function setZoomLevel(zoomLevel) {
    let bvs = this._browserViewportState;
    if (!bvs)
      return;

    let newZoomLevel = this.clampZoomLevel(zoomLevel);
    if (newZoomLevel != bvs.zoomLevel) {
      let browserW = this.viewportToBrowser(bvs.viewportRect.right);
      let browserH = this.viewportToBrowser(bvs.viewportRect.bottom);
      bvs.zoomLevel = newZoomLevel; // side-effect: now scale factor in transformations is newZoomLevel
      bvs.viewportRect.right  = this.browserToViewport(browserW);
      bvs.viewportRect.bottom = this.browserToViewport(browserH);
      this._viewportChanged(true, true);

      if (this._browser) {
        let event = document.createEvent("Events");
        event.initEvent("ZoomChanged", true, false);
        this._browser.dispatchEvent(event);
      }
    }
  },

  getZoomLevel: function getZoomLevel() {
    let bvs = this._browserViewportState;
    if (!bvs)
      return undefined;

    return bvs.zoomLevel;
  },

  clampZoomLevel: function clampZoomLevel(zl) {
    let bounded = Math.min(Math.max(ZoomManager.MIN, zl), ZoomManager.MAX);

    let bvs = this._browserViewportState;
    if (bvs) {
      let md = bvs.metaData;
      if (md && md.minZoom)
        bounded = Math.max(bounded, md.minZoom);
      if (md && md.maxZoom)
        bounded = Math.min(bounded, md.maxZoom);
    }

    let rounded = Math.round(bounded * kBrowserViewZoomLevelPrecision) / kBrowserViewZoomLevelPrecision;
    return rounded || 1.0;
  },

  beginOffscreenOperation: function beginOffscreenOperation(rect) {
    if (this._offscreenDepth == 0) {
      let vis = this.getVisibleRect();
      rect = rect || vis;
      let zoomRatio = vis.width / rect.width;
      let viewBuffer = Elements.viewBuffer;
      viewBuffer.width = vis.width;
      viewBuffer.height = vis.height;

      this._tileManager.renderRectToCanvas(rect, viewBuffer, zoomRatio, zoomRatio, false);
      viewBuffer.style.display = "block";
      window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils).processUpdates();
      this.pauseRendering();
    }
    this._offscreenDepth++;
  },

  commitOffscreenOperation: function commitOffscreenOperation() {
    this._offscreenDepth--;
    if (this._offscreenDepth == 0) {
      this.resumeRendering();
      Elements.viewBuffer.style.display = "none";
    }
  },

  beginBatchOperation: function beginBatchOperation() {
    this._batchOps.push(BrowserView.Util.getNewBatchOperationState());
    this.pauseRendering();
  },

  commitBatchOperation: function commitBatchOperation() {
    let bops = this._batchOps;
    if (bops.length == 0)
      return;

    let opState = bops.pop();

    // XXX If stack is not empty, this just assigns opState variables to the next one
    // on top. Why then have a stack of these booleans?
    this._viewportChanged(opState.viewportSizeChanged, opState.dirtyAll);
    this.resumeRendering();
  },

  discardBatchOperation: function discardBatchOperation() {
    let bops = this._batchOps;
    bops.pop();
    this.resumeRendering();
  },

  discardAllBatchOperations: function discardAllBatchOperations() {
    let bops = this._batchOps;
    while (bops.length > 0)
      this.discardBatchOperation();
  },

  /**
   * Calls to this function need to be one-to-one with calls to
   * resumeRendering()
   */
  pauseRendering: function pauseRendering() {
    this._renderMode++;
    if (this._renderMode == 1 && this._browser) {
      let event = document.createEvent("Events");
      event.initEvent("RenderStateChanged", true, false);
      event.isRendering = false;
      this._browser.dispatchEvent(event);
    }
  },

  /**
   * Calls to this function need to be one-to-one with calls to
   * pauseRendering()
   */
  resumeRendering: function resumeRendering(renderNow) {
    if (this._renderMode > 0)
      this._renderMode--;

    if (renderNow || this._renderMode == 0)
      this.renderNow();

    if (this._renderMode == 0 && this._browser) {
      let event = document.createEvent("Events");
      event.initEvent("RenderStateChanged", true, false);
      event.isRendering = true;
      this._browser.dispatchEvent(event);
    }
  },

  /**
   * Called while rendering is paused to allow update of critical area
   */
  renderNow: function renderNow() {
    this._tileManager.criticalRectPaint();
  },

  isRendering: function isRendering() {
    return (this._renderMode == 0);
  },

  onAfterVisibleMove: function onAfterVisibleMove() {
    let vs = this._browserViewportState;
    let vr = this.getVisibleRect();

    vs.visibleX = vr.left;
    vs.visibleY = vr.top;

    let cr = BrowserView.Util.visibleRectToCriticalRect(vr, vs);

    this._tileManager.criticalMove(cr, this.isRendering());
  },

  /**
   * Swap out the current browser and browser viewport state with a new pair.
   */
  setBrowser: function setBrowser(browser, browserViewportState) {
    if (browser && !browserViewportState) {
      throw "Cannot set non-null browser with null BrowserViewportState";
    }

    let oldBrowser = this._browser;
    let browserChanged = (oldBrowser !== browser);

    if (oldBrowser) {
      oldBrowser.setAttribute("type", "content");
      oldBrowser.messageManager.sendAsyncMessage("Browser:Blur", {});
    }

    this._browser = browser;
    this._browserViewportState = browserViewportState;

    if (browser) {
      browser.setAttribute("type", "content-primary");
      browser.messageManager.sendAsyncMessage("Browser:Focus", {});

      this.beginBatchOperation();

      if (browserChanged)
        this._viewportChanged(true, true);

      this.commitBatchOperation();
    }
  },

  getBrowser: function getBrowser() {
    return this._browser;
  },

  receiveMessage: function receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "Browser:MozAfterPaint":
        this.updateDirtyTiles(aMessage);
        break;
      case "Browser:PageScroll":
        this.updatePageScroll(aMessage);
        break;
      case "Browser:MozScrolledAreaChanged":
        this.updateScrolledArea(aMessage);
        break;
    }
    
    return {};
  },

  updateDirtyTiles: function updateDirtyTiles(aMessage) {
    let browser = aMessage.target;
    if (browser != this._browser)
      return;
    
    let rects = aMessage.json.rects;

    let tm = this._tileManager;
    let vs = this._browserViewportState;

    let dirtyRects = [];
    // loop backwards to avoid xpconnect penalty for .length
    for (let i = rects.length - 1; i >= 0; --i) {
      let r = Rect.fromRect(rects[i]);
      r = this.browserToViewportRect(r);
      r.expandToIntegers();

      r.restrictTo(vs.viewportRect);
      if (!r.isEmpty())
        dirtyRects.push(r);
    }

    tm.dirtyRects(dirtyRects, this.isRendering(), true);
  },

  /** If browser scrolls, pan content to new scroll area. */
  updatePageScroll: function updatePageScroll(aMessage) {
    if (aMessage.target != this._browser || this._ignorePageScroll)
      return;

    // XXX shouldn't really make calls to Browser
    let json = aMessage.json;
    Browser.scrollContentToBrowser(json.scrollX, json.scrollY);
  },

  _ignorePageScroll: false,
  ignorePageScroll: function ignorePageScroll(aIgnoreScroll) {
    this._ignorePageScroll = aIgnoreScroll;
  },

  updateScrolledArea: function updateScrolledArea(aMessage) {
    let browser = aMessage.target;
    if (!browser)
      throw "MozScrolledAreaChanged: Could not find browser";

    let json = aMessage.json;
    let tab = Browser.getTabForBrowser(browser);
    let bvs = tab.browserViewportState;

    let vis = this.getVisibleRect();
    let viewport = bvs.viewportRect;
    let oldRight = viewport.right;
    let oldBottom = viewport.bottom;
    viewport.right  = bvs.zoomLevel * json.width;
    viewport.bottom = bvs.zoomLevel * json.height;

    if (browser == this._browser) {
      // Page has now loaded enough to allow zooming.
      let sizeChanged = oldRight != viewport.right || oldBottom != viewport.bottom;
      this._viewportChanged(sizeChanged, false);
      this.updateDefaultZoom();
      if (vis.right > viewport.right || vis.bottom > viewport.bottom) {
        // Content has shrunk outside of the visible rectangle.
        // XXX for some reason scroller doesn't know it is outside its bounds
        Browser.contentScrollboxScroller.scrollBy(0, 0);
        this.onAfterVisibleMove();
      }
    }
  },

  /** Call when default zoom level may change. */
  updateDefaultZoom: function updateDefaultZoom() {
    let bvs = this._browserViewportState;
    if (!bvs)
      return false;

    let isDefault = (bvs.zoomLevel == bvs.defaultZoomLevel);
    bvs.defaultZoomLevel = this.getDefaultZoomLevel();
    if (isDefault)
      this.setZoomLevel(bvs.defaultZoomLevel);
    return isDefault;
  },

  isDefaultZoom: function isDefaultZoom() {
    let bvs = this._browserViewportState;
    if (!bvs)
      return true;
    return bvs.zoomLevel == bvs.defaultZoomLevel;
  },

  getDefaultZoomLevel: function getDefaultZoomLevel() {
    let bvs = this._browserViewportState;
    if (!bvs)
      return 0;

    let pageZoom = this.getPageZoomLevel();

    // If pageZoom is "almost" 100%, zoom in to exactly 100% (bug 454456).
    let granularity = gPrefService.getIntPref("browser.ui.zoom.pageFitGranularity");
    let threshold = 1 - 1 / granularity;
    if (threshold < pageZoom && pageZoom < 1)
      pageZoom = 1;

    let md = bvs.metaData;
    if (md && md.defaultZoom)
      return Math.max(pageZoom, this.clampZoomLevel(md.defaultZoom));

    return pageZoom;
  },

  getPageZoomLevel: function getPageZoomLevel() {
    let bvs = this._browserViewportState;  // browser exists, so bvs must as well
    let browserW = this.viewportToBrowser(bvs.viewportRect.right);
    return this.clampZoomLevel(this.getVisibleRect().width / browserW);
  },

  zoom: function zoom(aDirection) {
    let bvs = this._browserViewportState;
    if (!bvs)
      throw "No browser is set";

    if (aDirection == 0)
      return;

    var zoomDelta = 0.05; // 1/20
    if (aDirection >= 0)
      zoomDelta *= -1;

    this.setZoomLevel(bvs.zoomLevel + zoomDelta);
  },

  get allowZoom() {
    let bvs = this._browserViewportState;
    if (!bvs || !bvs.metaData)
      return true;
    return bvs.metaData.allowZoom;
  },

  //
  // MozAfterPaint events do not guarantee to inform us of all
  // invalidated paints (See
  // https://developer.mozilla.org/en/Gecko-Specific_DOM_Events#Important_notes
  // for details on what the event *does* guarantee).  This is only an
  // issue when the same current <browser> is used to navigate to a
  // new page.  Unless a zoom was issued during the page transition
  // (e.g. a call to setZoomLevel() or something of that nature), we
  // aren't guaranteed that we've actually invalidated the entire
  // page.  We don't want to leave bits of the previous page in the
  // view of the new one, so this method exists as a way for Browser
  // to inform us that the page is changing, and that we really ought
  // to invalidate everything.  Ideally, we wouldn't have to rely on
  // this being called, and we would get proper invalidates for the
  // whole page no matter what is or is not visible.
  //
  // Note that calling this function isn't necessary in almost all
  // cases, but should be done for correctness.  Most of the time, one
  // of the following two conditions is satisfied.  Either
  //
  //   (1) Pages have different widths so the Browser calls a
  //       updateDefaultZoom() which forces a dirtyAll, or
  //   (2) MozAfterPaint does indeed inform us of dirtyRects covering
  //       the entire page (everything that could possibly become
  //       visible).
  /**
   * Invalidates the entire page by throwing away any cached graphical
   * portions of the view and refusing to allow a updateDefaultZoom() until
   * the next explicit update of the viewport dimensions.
   *
   * This method should be called when the <browser> last set by
   * setBrowser() is about to navigate to a new page.
   */
  invalidateEntireView: function invalidateEntireView() {
    if (this._browserViewportState) {
      this._viewportChanged(false, true);
    }
  },

  /**
   * Render a rectangle within the browser viewport to the destination canvas
   * under the given scale.
   *
   * @param destCanvas The destination canvas into which the image is rendered.
   * @param destWidth Destination width
   * @param destHeight Destination height
   * @param srcRect [optional] The source rectangle in BrowserView coordinates.
   * This defaults to the visible rect rooted at the x,y of the critical rect.
   */
  renderToCanvas: function renderToCanvas(destCanvas, destWidth, destHeight, srcRect) {
    let bvs = this._browserViewportState;
    if (!bvs) {
      throw "Browser viewport state null in call to renderToCanvas (probably no browser set on BrowserView).";
    }

    if (!srcRect) {
      let vr = this.getVisibleRect();
      vr.x = bvs.viewportRect.left;
      vr.y = bvs.viewportRect.top;
      srcRect = vr;
    }

    let scalex = (destWidth / srcRect.width) || 1;
    let scaley = (destHeight / srcRect.height) || 1;

    srcRect.restrictTo(bvs.viewportRect);
    this._tileManager.renderRectToCanvas(srcRect, destCanvas, scalex, scaley);
  },

  viewportToBrowser: function viewportToBrowser(x) {
    let bvs = this._browserViewportState;
    if (!bvs)
      throw "No browser is set";

    return x / bvs.zoomLevel;
  },

  browserToViewport: function browserToViewport(x) {
    let bvs = this._browserViewportState;
    if (!bvs)
      throw "No browser is set";

    return x * bvs.zoomLevel;
  },

  viewportToBrowserRect: function viewportToBrowserRect(rect) {
    let f = this.viewportToBrowser(1.0);
    return rect.scale(f, f);
  },

  browserToViewportRect: function browserToViewportRect(rect) {
    let f = this.browserToViewport(1.0);
    return rect.scale(f, f);
  },

  browserToViewportCanvasContext: function browserToViewportCanvasContext(ctx) {
    let f = this.browserToViewport(1.0);
    ctx.scale(f, f);
  },

  forceContainerResize: function forceContainerResize() {
    let bvs = this._browserViewportState;
    if (bvs)
      BrowserView.Util.resizeContainerToViewport(this._container, bvs.viewportRect);
  },

  /**
   * Force any pending viewport changes to occur.  Batch operations will still be on the
   * stack so commitBatchOperation is still necessary afterwards.
   */
  forceViewportChange: function forceViewportChange() {
    let bops = this._batchOps;
    if (bops.length > 0) {
      let opState = bops[bops.length - 1];
      this._applyViewportChanges(opState.viewportSizeChanged, opState.dirtyAll);
      opState.viewportSizeChanged = false;
      opState.dirtyAll = false;
    }
  },

  // -----------------------------------------------------------
  // Private instance methods
  //

  _viewportChanged: function _viewportChanged(viewportSizeChanged, dirtyAll) {
    let bops = this._batchOps;
    if (bops.length > 0) {
      let opState = bops[bops.length - 1];

      if (viewportSizeChanged)
        opState.viewportSizeChanged = viewportSizeChanged;
      if (dirtyAll)
        opState.dirtyAll = dirtyAll;

      return;
    }

    this._applyViewportChanges(viewportSizeChanged, dirtyAll);
  },

  _applyViewportChanges: function _applyViewportChanges(viewportSizeChanged, dirtyAll) {
    let bvs = this._browserViewportState;
    if (bvs) {
      BrowserView.Util.resizeContainerToViewport(this._container, bvs.viewportRect);

      let vr = this.getVisibleRect();
      this._tileManager.viewportChangeHandler(bvs.viewportRect,
                                              BrowserView.Util.visibleRectToCriticalRect(vr, bvs),
                                              viewportSizeChanged,
                                              dirtyAll);

      let rects = vr.subtract(bvs.viewportRect);
      this._tileManager.clearRects(rects);
    }
  },

  _appendTile: function _appendTile(tile) {
    let canvas = tile.getContentImage();

    //canvas.style.position = "absolute";
    //canvas.style.left = tile.x + "px";
    //canvas.style.top  = tile.y + "px";

    // XXX The above causes a trace abort, and this function is called back in the tight
    // render-heavy loop in TileManager, so even though what we do below isn't so proper
    // and takes longer on the Platform/C++ emd, it's better than causing a trace abort
    // in our tight loop.
    //
    // But this also overwrites some style already set on the canvas in Tile constructor.
    // Hack fail...
    //
    canvas.setAttribute("style", "position: absolute; left: " + tile.boundRect.left + "px; " + "top: " + tile.boundRect.top + "px;");

    this._container.appendChild(canvas);
  },

  _removeTile: function _removeTile(tile) {
    let canvas = tile.getContentImage();

    this._container.removeChild(canvas);
  }

};


// -----------------------------------------------------------
// Helper structures
//

/**
 * A BrowserViewportState maintains viewport state information that is unique to each
 * browser.  It does not hold *all* viewport state maintained by BrowserView.  For
 * instance, it does not maintain width and height of the visible rectangle (but it
 * does keep the top and left coordinates (cf visibleX, visibleY)), since those are not
 * characteristic of the current browser in view.
 */
BrowserView.BrowserViewportState = function(viewportRect, visibleX, visibleY, zoomLevel) {
  this.init(viewportRect, visibleX, visibleY, zoomLevel);
};

BrowserView.BrowserViewportState.prototype = {

  init: function init(viewportRect, visibleX, visibleY, zoomLevel) {
    this.viewportRect = viewportRect;
    this.visibleX     = visibleX;
    this.visibleY     = visibleY;
    this.zoomLevel    = zoomLevel;
    this.defaultZoomLevel = 1;
  },

  toString: function toString() {
    let props = ["\tviewportRect=" + this.viewportRect.toString(),
                 "\tvisibleX="     + this.visibleX,
                 "\tvisibleY="     + this.visibleY,
                 "\tzoomLevel="    + this.zoomLevel];

    return "[BrowserViewportState] {\n" + props.join(",\n") + "\n}";
  }

};


/**
 * nsIObserver that implements a callback for the nsIIdleService, which starts
 * and stops the BrowserView's TileManager's prefetch crawl according to user
 * idleness.
 */
BrowserView.IdleServiceObserver = function IdleServiceObserver(browserView) {
  this._browserView = browserView;
  this._idle = false;
  this._paused = false;
};

BrowserView.IdleServiceObserver.prototype = {
  /** No matter what idle is, make sure prefetching is not active. */
  pause: function pause() {
    this._paused = true;
    this._updateTileManager();
  },

  /** Prefetch tiles in idle mode. */
  resume: function resume() {
    this._paused = false;
    this._updateTileManager();
  },

  /** Idle event handler. */
  observe: function observe(aSubject, aTopic, aUserIdleTime) {
    this._idle = (aTopic == "idle") ? true : false;
    this._updateTileManager();
  },

  _updateTileManager: function _updateTileManager() {
    let bv = this._browserView;
    bv._tileManager.setPrefetch(this._idle && !this._paused);
  }
};
