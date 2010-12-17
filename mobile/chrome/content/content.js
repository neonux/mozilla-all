// This stays here because otherwise it's hard to tell if there's a parsing error
dump("###################################### content loaded\n");

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

let gFocusManager = Cc["@mozilla.org/focus-manager;1"]
  .getService(Ci.nsIFocusManager);

let XULDocument = Ci.nsIDOMXULDocument;
let HTMLHtmlElement = Ci.nsIDOMHTMLHtmlElement;
let HTMLIFrameElement = Ci.nsIDOMHTMLIFrameElement;
let HTMLFrameElement = Ci.nsIDOMHTMLFrameElement;

// Blindly copied from Safari documentation for now.
const kViewportMinScale  = 0;
const kViewportMaxScale  = 10;
const kViewportMinWidth  = 200;
const kViewportMaxWidth  = 10000;
const kViewportMinHeight = 223;
const kViewportMaxHeight = 10000;

const kReferenceDpi = 240; // standard "pixel" size used in some preferences

/** Watches for mouse click in content and redirect them to the best found target **/
const ElementTouchHelper = {
  get radius() {
    let prefs = Services.prefs;
    delete this.radius;
    return this.radius = { "top": prefs.getIntPref("browser.ui.touch.top"),
                           "right": prefs.getIntPref("browser.ui.touch.right"),
                           "bottom": prefs.getIntPref("browser.ui.touch.bottom"),
                           "left": prefs.getIntPref("browser.ui.touch.left")
                         };
  },

  get weight() {
    delete this.weight;
    return this.weight = { "visited": Services.prefs.getIntPref("browser.ui.touch.weight.visited")
                         };
  },

  /* Retrieve the closest element to a point by looking at borders position */
  getClosest: function getClosest(aWindowUtils, aX, aY) {
    let dpiRatio = aWindowUtils.displayDPI / kReferenceDpi;

    let target = aWindowUtils.elementFromPoint(aX, aY,
                                               true,   /* ignore root scroll frame*/
                                               false); /* don't flush layout */

    // return early if the click is just over a clickable element
    if (this._isElementClickable(target))
      return target;

    let nodes = aWindowUtils.nodesFromRect(aX, aY, this.radius.top * dpiRatio,
                                                   this.radius.right * dpiRatio,
                                                   this.radius.bottom * dpiRatio,
                                                   this.radius.left * dpiRatio, true, false);

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

  _isElementClickable: function _isElementClickable(aElement) {
    const selector = "a,:link,:visited,[role=button],button,input,select,textarea,label";
    for (let elem = aElement; elem; elem = elem.parentNode) {
      if (this._hasMouseListener(elem))
        return true;
      if (elem.mozMatchesSelector && elem.mozMatchesSelector(selector))
        return true;
    }
    return false;
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
    return false;
  }
};


/**
 * @param x,y Browser coordinates
 * @return Element at position, null if no active browser or no element found
 */
function elementFromPoint(x, y) {
  // browser's elementFromPoint expect browser-relative client coordinates.
  // subtract browser's scroll values to adjust
  let cwu = Util.getWindowUtils(content);
  let scroll = Util.getScrollOffset(content);
  x = x - scroll.x;
  y = y - scroll.y;
  let elem = ElementTouchHelper.getClosest(cwu, x, y);

  // step through layers of IFRAMEs and FRAMES to find innermost element
  while (elem && (elem instanceof HTMLIFrameElement || elem instanceof HTMLFrameElement)) {
    // adjust client coordinates' origin to be top left of iframe viewport
    let rect = elem.getBoundingClientRect();
    x -= rect.left;
    y -= rect.top;
    let windowUtils = elem.contentDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    elem = ElementTouchHelper.getClosest(windowUtils, x, y);
  }

  return elem;
}

function getBoundingContentRect(aElement) {
  if (!aElement)
    return new Rect(0, 0, 0, 0);

  let document = aElement.ownerDocument;
  while(document.defaultView.frameElement)
    document = document.defaultView.frameElement.ownerDocument;

  let offset = Util.getScrollOffset(content);
  let r = aElement.getBoundingClientRect();

  // step out of iframes and frames, offsetting scroll values
  for (let frame = aElement.ownerDocument.defaultView; frame != content; frame = frame.parent) {
    // adjust client coordinates' origin to be top left of iframe viewport
    let rect = frame.frameElement.getBoundingClientRect();
    let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
    let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
    offset.add(rect.left + parseInt(left), rect.top + parseInt(top));
  }

  return new Rect(r.left + offset.x, r.top + offset.y, r.width, r.height);
}

function getContentClientRects(aElement) {
  let offset = Util.getScrollOffset(content);
  let nativeRects = aElement.getClientRects();
  // step out of iframes and frames, offsetting scroll values
  for (let frame = aElement.ownerDocument.defaultView; frame != content; frame = frame.parent) {
    // adjust client coordinates' origin to be top left of iframe viewport
    let rect = frame.frameElement.getBoundingClientRect();
    let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
    let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
    offset.add(rect.left + parseInt(left), rect.top + parseInt(top));
  }

  let result = [];
  for (let i = nativeRects.length - 1; i >= 0; i--) {
    let r = nativeRects[i];
    result.push({ left: r.left + offset.x,
                  top: r.top + offset.y,
                  width: r.width,
                  height: r.height
                });
  }
  return result;
};


/**
 * Responsible for sending messages about security, location, and page load state.
 * @param loadingController Object with methods startLoading and stopLoading
 */
function ProgressController(loadingController) {
  this._webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
  this._overrideService = null;
  this._hostChanged = false;
  this._state = null;
  this._loadingController = loadingController || this._defaultLoadingController;
}

ProgressController.prototype = {
  // Default loading callbacks do nothing
  _defaultLoadingController: {
    startLoading: function() {},
    stopLoading: function() {}
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    // ignore notification that aren't about the main document (iframes, etc)
    let win = aWebProgress.DOMWindow;
    if (win != win.parent)
      return;

    // If you want to observe other state flags, be sure they're listed in the
    // Tab._createBrowser's call to addProgressListener
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
      if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
        this._loadingController.startLoading();
      }
      else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
        this._loadingController.stopLoading();
      }
    }
  },

  /** This method is called to indicate progress changes for the currently loading page. */
  onProgressChange: function onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
  },

  /** This method is called to indicate a change to the current location. */
  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocationURI) {
  },

  /**
   * This method is called to indicate a status changes for the currently
   * loading page.  The message is already formatted for display.
   */
  onStatusChange: function onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
  },

  /** This method is called when the security state of the browser changes. */
  onSecurityChange: function onSecurityChange(aWebProgress, aRequest, aState) {
  },

  QueryInterface: function QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports)) {
        return this;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  start: function start() {
    let flags = Ci.nsIWebProgress.NOTIFY_STATE_NETWORK;
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, flags);
  },

  stop: function stop() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this);
  }
};


/** Can't think of a good description of this class.  It probably does too much? */
function Content() {
  addMessageListener("Browser:Blur", this);
  addMessageListener("Browser:KeyEvent", this);
  addMessageListener("Browser:MouseDown", this);
  addMessageListener("Browser:MouseOver", this);
  addMessageListener("Browser:MouseUp", this);
  addMessageListener("Browser:SaveAs", this);
  addMessageListener("Browser:ZoomToPoint", this);
  addMessageListener("Browser:MozApplicationCache:Fetch", this);

  if (Util.isParentProcess())
    addEventListener("DOMActivate", this, true);

  addEventListener("MozApplicationManifest", this, false);
  addEventListener("command", this, false);

  this._progressController = new ProgressController(this);
  this._progressController.start();

  this._formAssistant = new FormAssistant();
}

Content.prototype = {
  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "DOMActivate": {
        // In a local tab, open remote links in new tabs.
        let target = aEvent.originalTarget;
        let href = Util.getHrefForElement(target);
        if (/^http(s?):/.test(href)) {
          aEvent.preventDefault();
          sendAsyncMessage("Browser:OpenURI", { uri: href,
                                                referrer: target.ownerDocument.documentURIObject.spec,
                                                bringFront: true });
        }
        break;
      }

      case "MozApplicationManifest": {
        let doc = aEvent.originalTarget;
        sendAsyncMessage("Browser:MozApplicationManifest", {
          location: doc.documentURIObject.spec,
          manifest: doc.documentElement.getAttribute("manifest"),
          charset: doc.characterSet
        });
        break;
      }
      case "command": {
        // Don't trust synthetic events
        if (!aEvent.isTrusted)
          return;
    
        let ot = aEvent.originalTarget;
        let errorDoc = ot.ownerDocument;
    
        // If the event came from an ssl error page, it is probably either the "Add
        // Exception…" or "Get me out of here!" button
        if (/^about:certerror\?e=nssBadCert/.test(errorDoc.documentURI)) {
          let perm = errorDoc.getElementById("permanentExceptionButton");
          let temp = errorDoc.getElementById("temporaryExceptionButton"); 
          if (ot == temp || ot == perm) {
            let action = (ot == perm ? "permanent" : "temporary");
            sendAsyncMessage("Browser:CertException", { url: errorDoc.location.href, action: action });
          }
          else if (ot == errorDoc.getElementById("getMeOutOfHereButton")) {
            sendAsyncMessage("Browser:CertException", { url: errorDoc.location.href, action: "leave" });
          }
        }
        else if (/^about:neterror\?e=netOffline/.test(errorDoc.documentURI)) {
          if (ot == errorDoc.getElementById("errorTryAgain")) {
            // Make sure we're online before attempting to load
            Util.forceOnline();
          }
        }
        break;
      }
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    let x = json.x;
    let y = json.y;
    let modifiers = json.modifiers;

    switch (aMessage.name) {
      case "Browser:Blur":
        gFocusManager.clearFocus(content);
        break;

      case "Browser:KeyEvent":
        let utils = Util.getWindowUtils(content);
        let defaultAction;
        if (!Util.isParentProcess())
          defaultAction = utils.sendKeyEvent(json.type, json.keyCode, json.charCode, modifiers);
        if (defaultAction && json.type == "keypress") {
          const masks = Ci.nsIDOMNSEvent;
          sendAsyncMessage("Browser:KeyPress", {
            ctrlKey: json.modifiers & masks.CONTROL_MASK,
            shiftKey: json.modifiers & masks.SHIFT_MASK,
            metaKey: json.modifiers & masks.META_MASK,
            keyCode: json.keyCode,
            charCode: json.charCode
          });
        }
        break;

      case "Browser:MouseDown": {
        let element = elementFromPoint(x, y);
        if (!element)
          return;

        ContextHandler.messageId = json.messageId;

        let event = content.document.createEvent("PopupEvents");
        event.initEvent("contextmenu", true, true);
        element.dispatchEvent(event);
        break;
      }

      case "Browser:MouseOver": {
        let element = elementFromPoint(x, y);
        if (!element)
          return;

        // Sending a mousemove force the dispatching of mouseover/mouseout
        this._sendMouseEvent("mousemove", element, x, y);

        // XXX Could we replace all this javascript code by something CSS based
        // using a mix on some -moz-focus-ring/-moz-focus-inner rules
        let highlightRects = null;
        if (element.mozMatchesSelector("*:-moz-any-link, *[role=button],button,input,option,select,textarea,label"))
          highlightRects = getContentClientRects(element);
        else if (element.mozMatchesSelector("*:-moz-any-link *"))
          highlightRects = getContentClientRects(element.parentNode);

        if (highlightRects)
          sendAsyncMessage("Browser:Highlight", { rects: highlightRects, messageId: json.messageId });
        break;
      }

      case "Browser:MouseUp": {
        this._formAssistant.focusSync = true;
        let element = elementFromPoint(x, y);
        if (modifiers == Ci.nsIDOMNSEvent.CONTROL_MASK) {
          let uri = Util.getHrefForElement(element);
          if (uri)
            sendAsyncMessage("Browser:OpenURI", { uri: uri,
                                                  referrer: element.ownerDocument.documentURIObject.spec });
        } else if (!this._formAssistant.open(element)) {
          sendAsyncMessage("FindAssist:Hide", { });
          this._sendMouseEvent("mousemove", element, x, y);
          this._sendMouseEvent("mousedown", element, x, y);
          this._sendMouseEvent("mouseup", element, x, y);
        }
        ContextHandler.reset();
        this._formAssistant.focusSync = false;
        break;
      }

      case "Browser:SaveAs":
        if (json.type != Ci.nsIPrintSettings.kOutputFormatPDF)
          return;

        let printSettings = Cc["@mozilla.org/gfx/printsettings-service;1"]
                              .getService(Ci.nsIPrintSettingsService)
                              .newPrintSettings;
        printSettings.printSilent = true;
        printSettings.showPrintProgress = false;
        printSettings.printBGImages = true;
        printSettings.printBGColors = true;
        printSettings.printToFile = true;
        printSettings.toFileName = json.filePath;
        printSettings.printFrameType = Ci.nsIPrintSettings.kFramesAsIs;
        printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;

        //XXX we probably need a preference here, the header can be useful
        printSettings.footerStrCenter = "";
        printSettings.footerStrLeft   = "";
        printSettings.footerStrRight  = "";
        printSettings.headerStrCenter = "";
        printSettings.headerStrLeft   = "";
        printSettings.headerStrRight  = "";

        let listener = {
          onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
            if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
              sendAsyncMessage("Browser:SaveAs:Return", { type: json.type, id: json.id, referrer: json.referrer });
            }
          },
          onProgressChange : function(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {},

          // stubs for the nsIWebProgressListener interfaces which nsIWebBrowserPrint doesn't use.
          onLocationChange : function() { throw "Unexpected onLocationChange"; },
          onStatusChange   : function() { throw "Unexpected onStatusChange";   },
          onSecurityChange : function() { throw "Unexpected onSecurityChange"; }
        };

        let webBrowserPrint = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebBrowserPrint);
        webBrowserPrint.print(printSettings, listener);
        break;

      case "Browser:ZoomToPoint": {
        let rect = null;
        let element = elementFromPoint(x, y);
        let win = element.ownerDocument.defaultView;
        while (element && win.getComputedStyle(element,null).display == "inline")
          element = element.parentNode;
        if (element)
          rect = getBoundingContentRect(element);
        sendAsyncMessage("Browser:ZoomToPoint:Return", { x: x, y: y, rect: rect });
        break;
      }

      case "Browser:MozApplicationCache:Fetch": {
        let currentURI = Services.io.newURI(json.location, json.charset, null);
        let manifestURI = Services.io.newURI(json.manifest, json.charset, currentURI);
        let updateService = Cc["@mozilla.org/offlinecacheupdate-service;1"]
                            .getService(Ci.nsIOfflineCacheUpdateService);
        updateService.scheduleUpdate(manifestURI, currentURI, content);
        break;
      }
    }
  },

  _sendMouseEvent: function _sendMouseEvent(aName, aElement, aX, aY) {
    // the element can be out of the aX/aY point because of the touch radius
    if (!(aElement instanceof HTMLHtmlElement)) {
      let isTouchClick = true;
      let rects = getContentClientRects(aElement);
      for (let i = 0; i < rects.length; i++) {
        let rect = rects[i];
        if ((aX > rect.left && aX < (rect.left + rect.width)) &&
            (aY > rect.top && aY < (rect.top + rect.height))) {
          isTouchClick = false;
          break;
        }
      }

      if (isTouchClick) {
        let rect = rects[0];
        let point = (new Rect(rect.left, rect.top, rect.width, rect.height)).center();
        aX = point.x;
        aY = point.y;
      }
    }

    let scrollOffset = Util.getScrollOffset(content);
    let windowUtils = Util.getWindowUtils(content);
    windowUtils.sendMouseEventToWindow(aName, aX - scrollOffset.x, aY - scrollOffset.y, 0, 1, 0, true);
  },

  startLoading: function startLoading() {
    this._loading = true;
  },

  stopLoading: function stopLoading() {
    this._loading = false;
  },
};

let contentObject = new Content();

let ViewportHandler = {
  init: function init() {
    addEventListener("DOMWindowCreated", this, false);
    addEventListener("DOMMetaAdded", this, false);
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("pageshow", this, false);
  },

  handleEvent: function handleEvent(aEvent) {
    let target = aEvent.originalTarget;
    let isRootDocument = (target == content.document || target.ownerDocument == content.document);
    if (!isRootDocument)
      return;

    switch (aEvent.type) {
      case "DOMWindowCreated":
        this.resetMetadata();
        break;

      case "DOMMetaAdded":
        if (target.name == "viewport")
          this.updateMetadata();
        break;

      case "DOMContentLoaded":
      case "pageshow":
        this.updateMetadata();
        break;
    }
  },

  resetMetadata: function resetMetadata() {
    sendAsyncMessage("Browser:ViewportMetadata", null);
  },

  updateMetadata: function updateMetadata() {
    sendAsyncMessage("Browser:ViewportMetadata", this.getViewportMetadata());
  },

  /**
   * Returns an object with the page's preferred viewport properties:
   *   defaultZoom (optional float): The initial scale when the page is loaded.
   *   minZoom (optional float): The minimum zoom level.
   *   maxZoom (optional float): The maximum zoom level.
   *   width (optional int): The CSS viewport width in px.
   *   height (optional int): The CSS viewport height in px.
   *   autoSize (boolean): Resize the CSS viewport when the window resizes.
   *   allowZoom (boolean): Let the user zoom in or out.
   *   autoScale (boolean): Adjust the viewport properties to account for display density.
   */
  getViewportMetadata: function getViewportMetadata() {
    let doctype = content.document.doctype;
    if (doctype && /(WAP|WML|Mobile)/.test(doctype.publicId))
      return { defaultZoom: 1, autoSize: true, allowZoom: true, autoScale: true };

    let windowUtils = Util.getWindowUtils(content);
    let handheldFriendly = windowUtils.getDocumentMetadata("HandheldFriendly");
    if (handheldFriendly == "true")
      return { defaultZoom: 1, autoSize: true, allowZoom: true, autoScale: true };

    if (content.document instanceof XULDocument)
      return { defaultZoom: 1, autoSize: true, allowZoom: false, autoScale: false };

    // HACK: Since we can't set the scale in local tabs (bug 597081), we force
    // them to device-width and scale=1 so they will lay out reasonably.
    if (Util.isParentProcess())
      return { defaultZoom: 1, autoSize: true, allowZoom: false, autoScale: false };

    // viewport details found here
    // http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariHTMLRef/Articles/MetaTags.html
    // http://developer.apple.com/safari/library/documentation/AppleApplications/Reference/SafariWebContent/UsingtheViewport/UsingtheViewport.html

    // Note: These values will be NaN if parseFloat or parseInt doesn't find a number.
    // Remember that NaN is contagious: Math.max(1, NaN) == Math.min(1, NaN) == NaN.
    let scale = parseFloat(windowUtils.getDocumentMetadata("viewport-initial-scale"));
    let minScale = parseFloat(windowUtils.getDocumentMetadata("viewport-minimum-scale"));
    let maxScale = parseFloat(windowUtils.getDocumentMetadata("viewport-maximum-scale"));

    let widthStr = windowUtils.getDocumentMetadata("viewport-width");
    let heightStr = windowUtils.getDocumentMetadata("viewport-height");
    let width = Util.clamp(parseInt(widthStr), kViewportMinWidth, kViewportMaxWidth);
    let height = Util.clamp(parseInt(heightStr), kViewportMinHeight, kViewportMaxHeight);

    let allowZoomStr = windowUtils.getDocumentMetadata("viewport-user-scalable");
    let allowZoom = !/^(0|no|false)$/.test(allowZoomStr); // WebKit allows 0, "no", or "false"

    scale = Util.clamp(scale, kViewportMinScale, kViewportMaxScale);
    minScale = Util.clamp(minScale, kViewportMinScale, kViewportMaxScale);
    maxScale = Util.clamp(maxScale, kViewportMinScale, kViewportMaxScale);

    // If initial scale is 1.0 and width is not set, assume width=device-width
    let autoSize = (widthStr == "device-width" ||
                    (!widthStr && (heightStr == "device-height" || scale == 1.0)));

    return {
      defaultZoom: scale,
      minZoom: minScale,
      maxZoom: maxScale,
      width: width,
      height: height,
      autoSize: autoSize,
      allowZoom: allowZoom,
      autoScale: true
    };
  }
};

ViewportHandler.init();


const kXLinkNamespace = "http://www.w3.org/1999/xlink";

var ContextHandler = {
  _types: [],

  _getLinkURL: function ch_getLinkURL(aLink) {
    let href = aLink.href;
    if (href)
      return href;

    href = aLink.getAttributeNS(kXLinkNamespace, "href");
    if (!href || !href.match(/\S/)) {
      // Without this we try to save as the current doc,
      // for example, HTML case also throws if empty
      throw "Empty href";
    }

    return Util.makeURLAbsolute(aLink.baseURI, href);
  },

  _getURI: function ch_getURI(aURL) {
    try {
      return Util.makeURI(aURL);
    } catch (ex) { }

    return null;
  },

  _getProtocol: function ch_getProtocol(aURI) {
    if (aURI)
      return aURI.scheme;
    return null;
  },

  init: function ch_init() {
    addEventListener("contextmenu", this, false);
    addEventListener("pagehide", this, false);
    addMessageListener("Browser:ContextCommand", this, false);
    this.popupNode = null;
  },

  reset: function ch_reset() {
    this.popupNode = null;
  },

  handleEvent: function ch_handleEvent(aEvent) {
    switch (aEvent.type) {
      case "contextmenu":
        this.onContextMenu(aEvent);
        break;
      case "pagehide":
        this.reset();
        break;
    }
  },

  onContextMenu: function ch_onContextMenu(aEvent) {
    if (aEvent.getPreventDefault())
      return;

    let state = {
      types: [],
      label: "",
      linkURL: "",
      linkTitle: "",
      linkProtocol: null,
      mediaURL: ""
    };

    let popupNode = this.popupNode = aEvent.originalTarget;

    // Do checks for nodes that never have children.
    if (popupNode.nodeType == Ci.nsIDOMNode.ELEMENT_NODE) {
      // See if the user clicked on an image.
      if (popupNode instanceof Ci.nsIImageLoadingContent && popupNode.currentURI) {
        state.types.push("image");
        state.label = state.mediaURL = popupNode.currentURI.spec;
      } else if (popupNode instanceof Ci.nsIDOMHTMLMediaElement) {
        state.label = state.mediaURL = (popupNode.currentSrc || popupNode.src);
        state.types.push((popupNode.paused || popupNode.ended) ? "media-paused" : "media-playing");
        if (popupNode instanceof Ci.nsIDOMHTMLVideoElement)
          state.types.push("video");
      }
    }

    let elem = popupNode;
    while (elem) {
      if (elem.nodeType == Ci.nsIDOMNode.ELEMENT_NODE) {
        // Link?
        if ((elem instanceof Ci.nsIDOMHTMLAnchorElement && elem.href) ||
            (elem instanceof Ci.nsIDOMHTMLAreaElement && elem.href) ||
            elem instanceof Ci.nsIDOMHTMLLinkElement ||
            elem.getAttributeNS(kXLinkNamespace, "type") == "simple") {

          // Target is a link or a descendant of a link.
          state.types.push("link");
          state.label = state.linkURL = this._getLinkURL(elem);
          state.linkTitle = popupNode.textContent || popupNode.title;
          state.linkProtocol = this._getProtocol(this._getURI(state.linkURL));
          break;
        }
      }

      elem = elem.parentNode;
    }

    for (let i = 0; i < this._types.length; i++)
      if (this._types[i].handler(state, popupNode))
        state.types.push(this._types[i].name);

    state.messageId = this.messageId;

    sendAsyncMessage("Browser:ContextMenu", state);
  },

  receiveMessage: function ch_receiveMessage(aMessage) {
    let node = this.popupNode;
    let command = aMessage.json.command;

    switch (command) {
      case "play":
      case "pause":
        if (node instanceof Ci.nsIDOMHTMLMediaElement)
          node[command]();
        break;

      case "fullscreen":
        if (node instanceof Ci.nsIDOMHTMLVideoElement) {
          node.pause();
          Cu.import("resource:///modules/video.jsm");
          Video.fullScreenSourceElement = node;
          sendAsyncMessage("Browser:FullScreenVideo:Start");
        }
        break;
    }
  },

  /**
   * For add-ons to add new types and data to the ContextMenu message.
   *
   * @param aName A string to identify the new type.
   * @param aHandler A function that takes a state object and a target element.
   *    If aHandler returns true, then aName will be added to the list of types.
   *    The function may also modify the state object.
   */
  registerType: function registerType(aName, aHandler) {
    this._types.push({name: aName, handler: aHandler});
  }
};

ContextHandler.init();

ContextHandler.registerType("mailto", function(aState, aElement) {
  return aState.linkProtocol == "mailto";
});

ContextHandler.registerType("callto", function(aState, aElement) {
  let protocol = aState.linkProtocol;
  return protocol == "tel" || protocol == "callto" || protocol == "sip" || protocol == "voipto";
});

ContextHandler.registerType("link-saveable", function(aState, aElement) {
  let protocol = aState.linkProtocol;
  return (protocol && protocol != "mailto" && protocol != "javascript" && protocol != "news" && protocol != "snews");
});

ContextHandler.registerType("link-openable", function(aState, aElement) {
  let protocol = aState.linkProtocol;
  return (protocol && protocol != "mailto" && protocol != "javascript" && protocol != "news" && protocol != "snews");
});

ContextHandler.registerType("link-shareable", function(aState, aElement) {
  return Util.isShareableScheme(aState.linkProtocol);
});

["image", "video"].forEach(function(aType) {
  ContextHandler.registerType(aType+"-shareable", function(aState, aElement) {
    if (aState.types.indexOf(aType) == -1)
      return false;

    let protocol = ContextHandler._getProtocol(ContextHandler._getURI(aState.mediaURL));
    return Util.isShareableScheme(protocol);
  });
});

ContextHandler.registerType("image-loaded", function(aState, aElement) {
  if (aState.types.indexOf("image") != -1) {
    let request = aElement.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
    if (request && (request.imageStatus & request.STATUS_SIZE_AVAILABLE))
      return true;
  }
  return false;
});

var FormSubmitObserver = {
  init: function init(){
    addMessageListener("Browser:TabOpen", this);
    addMessageListener("Browser:TabClose", this);
  },

  receiveMessage: function findHandlerReceiveMessage(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Browser:TabOpen":
        Services.obs.addObserver(this, "formsubmit", false);
        break;
      case "Browser:TabClose":
        Services.obs.removeObserver(this, "formsubmit", false);
        break;
    }
  },

  notify: function notify(aFormElement, aWindow, aActionURI, aCancelSubmit) {
    // Do not notify unless this is the window where the submit occurred
    if (aWindow == content)
      // We don't need to send any data along
      sendAsyncMessage("Browser:FormSubmit", {});
  },

  QueryInterface : function(aIID) {
    if (!aIID.equals(Ci.nsIFormSubmitObserver) &&
        !aIID.equals(Ci.nsISupportsWeakReference) &&
        !aIID.equals(Ci.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

FormSubmitObserver.init();

var FindHandler = {
  get _fastFind() {
    delete this._fastFind;
    this._fastFind = Cc["@mozilla.org/typeaheadfind;1"].createInstance(Ci.nsITypeAheadFind);
    this._fastFind.init(docShell);
    return this._fastFind;
  },

  init: function findHandlerInit() {
    addMessageListener("FindAssist:Find", this);
    addMessageListener("FindAssist:Next", this);
    addMessageListener("FindAssist:Previous", this);
  },

  receiveMessage: function findHandlerReceiveMessage(aMessage) {
    let findResult = Ci.nsITypeAheadFind.FIND_NOTFOUND;
    let json = aMessage.json;
    switch (aMessage.name) {
      case "FindAssist:Find":
        findResult = this._fastFind.find(json.searchString, false);
        break;

      case "FindAssist:Previous":
        findResult = this._fastFind.findAgain(true, false);
        break;

      case "FindAssist:Next":
        findResult = this._fastFind.findAgain(false, false);
        break;
    }

    if (findResult == Ci.nsITypeAheadFind.FIND_NOTFOUND) {
      sendAsyncMessage("FindAssist:Show", { rect: null , result: findResult });
      return;
    }

    let selection = this._fastFind.currentWindow.getSelection();
    if (!selection.rangeCount || selection.isCollapsed) {
      // The selection can be into an input or a textarea element
      let nodes = content.document.querySelectorAll("input[type='text'], textarea");
      for (let i = 0; i < nodes.length; i++) {
        let node = nodes[i];
        if (node instanceof Ci.nsIDOMNSEditableElement && node.editor) {
          selection = node.editor.selectionController.getSelection(Ci.nsISelectionController.SELECTION_NORMAL);
          if (selection.rangeCount && !selection.isCollapsed)
            break;
        }
      }
    }

    let scroll = Util.getScrollOffset(content);
    for (let frame = this._fastFind.currentWindow; frame != content; frame = frame.parent) {
      let rect = frame.frameElement.getBoundingClientRect();
      let left = frame.getComputedStyle(frame.frameElement, "").borderLeftWidth;
      let top = frame.getComputedStyle(frame.frameElement, "").borderTopWidth;
      scroll.add(rect.left + parseInt(left), rect.top + parseInt(top));
    }

    let rangeRect = selection.getRangeAt(0).getBoundingClientRect();
    let rect = new Rect(scroll.x + rangeRect.left, scroll.y + rangeRect.top, rangeRect.width, rangeRect.height);

    // Ensure the potential "scroll" event fired during a search as already fired
    let timer = new Util.Timeout(function() {
      sendAsyncMessage("FindAssist:Show", { rect: rect.isEmpty() ? null: rect , result: findResult });
    });
    timer.once(0);
  }
};

FindHandler.init();
