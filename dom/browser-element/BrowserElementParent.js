/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cu = Components.utils;
let Ci = Components.interfaces;
let Cc = Components.classes;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed";
const BROWSER_FRAMES_ENABLED_PREF = "dom.mozBrowserFramesEnabled";

function debug(msg) {
  //dump("BrowserElementParent - " + msg + "\n");
}

/**
 * BrowserElementParent implements one half of <iframe mozbrowser>.  (The other
 * half is, unsurprisingly, BrowserElementChild.)
 *
 * BrowserElementParentFactory detects when we create a windows or docshell
 * contained inside a <iframe mozbrowser> and creates a BrowserElementParent
 * object for that window.
 *
 * BrowserElementParent injects script to listen for certain events in the
 * child.  We then listen to messages from the child script and take
 * appropriate action here in the parent.
 */

function BrowserElementParentFactory() {
  this._initialized = false;
}

BrowserElementParentFactory.prototype = {
  classID: Components.ID("{ddeafdac-cb39-47c4-9cb8-c9027ee36d26}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  /**
   * Called on app startup, and also when the browser frames enabled pref is
   * changed.
   */
  _init: function() {
    if (this._initialized) {
      return;
    }

    // If the pref is disabled, do nothing except wait for the pref to change.
    // (This is important for tests, if nothing else.)
    if (!this._browserFramesPrefEnabled()) {
      var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
      prefs.addObserver(BROWSER_FRAMES_ENABLED_PREF, this, /* ownsWeak = */ true);
      return;
    }

    debug("_init");
    this._initialized = true;

    // Maps frame elements to BrowserElementParent objects.  We never look up
    // anything in this map; the purpose is to keep the BrowserElementParent
    // alive for as long as its frame element lives.
    this._bepMap = new WeakMap();

    var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.addObserver(this, 'remote-browser-frame-shown', /* ownsWeak = */ true);
    os.addObserver(this, 'in-process-browser-frame-shown', /* ownsWeak = */ true);
  },

  _browserFramesPrefEnabled: function() {
    var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    try {
      return prefs.getBoolPref(BROWSER_FRAMES_ENABLED_PREF);
    }
    catch(e) {
      return false;
    }
  },

  _observeInProcessBrowserFrameShown: function(frameLoader) {
    debug("In-process browser frame shown " + frameLoader);
    this._createBrowserElementParent(frameLoader);
  },

  _observeRemoteBrowserFrameShown: function(frameLoader) {
    debug("Remote browser frame shown " + frameLoader);
    this._createBrowserElementParent(frameLoader);
  },

  _createBrowserElementParent: function(frameLoader) {
    let frameElement = frameLoader.QueryInterface(Ci.nsIFrameLoader).ownerElement;
    this._bepMap.set(frameElement, new BrowserElementParent(frameLoader));
  },

  observe: function(subject, topic, data) {
    switch(topic) {
    case 'app-startup':
      this._init();
      break;
    case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
      if (data == BROWSER_FRAMES_ENABLED_PREF) {
        this._init();
      }
      break;
    case 'remote-browser-frame-shown':
      this._observeRemoteBrowserFrameShown(subject);
      break;
    case 'in-process-browser-frame-shown':
      this._observeInProcessBrowserFrameShown(subject);
      break;
    case 'content-document-global-created':
      this._observeContentGlobalCreated(subject);
      break;
    }
  },
};

function BrowserElementParent(frameLoader) {
  debug("Creating new BrowserElementParent object for " + frameLoader);
  this._screenshotListeners = {};
  this._screenshotReqCounter = 0;

  this._frameElement = frameLoader.QueryInterface(Ci.nsIFrameLoader).ownerElement;
  if (!this._frameElement) {
    debug("No frame element?");
    return;
  }

  this._mm = frameLoader.messageManager;

  // Messages we receive are handed to functions which take a (data) argument,
  // where |data| is the message manager's data object.

  let self = this;
  function addMessageListener(msg, handler) {
    self._mm.addMessageListener('browser-element-api:' + msg, handler.bind(self));
  }

  addMessageListener("hello", this._recvHello);
  addMessageListener("locationchange", this._fireEventFromMsg);
  addMessageListener("loadstart", this._fireEventFromMsg);
  addMessageListener("loadend", this._fireEventFromMsg);
  addMessageListener("titlechange", this._fireEventFromMsg);
  addMessageListener("iconchange", this._fireEventFromMsg);
  addMessageListener("close", this._fireEventFromMsg);
  addMessageListener("get-mozapp-manifest-url", this._sendMozAppManifestURL);
  addMessageListener("keyevent", this._fireKeyEvent);
  addMessageListener("showmodalprompt", this._handleShowModalPrompt);
  addMessageListener('got-screenshot', this._recvGotScreenshot);

  function defineMethod(name, fn) {
    XPCNativeWrapper.unwrap(self._frameElement)[name] = fn.bind(self);
  }

  // Define methods on the frame element.
  defineMethod('getScreenshot', this._getScreenshot);
  defineMethod('setVisible', this._setVisible);

  self._mm.loadFrameScript("chrome://global/content/BrowserElementChild.js",
                           /* allowDelayedLoad = */ true);
}

BrowserElementParent.prototype = {
  get _window() {
    return this._frameElement.ownerDocument.defaultView;
  },

  _sendAsyncMsg: function(msg, data) {
    this._frameElement.QueryInterface(Ci.nsIFrameLoaderOwner)
                      .frameLoader
                      .messageManager
                      .sendAsyncMessage('browser-element-api:' + msg, data);
  },

  _recvHello: function(data) {
    debug("recvHello");
  },

  /**
   * Fire either a vanilla or a custom event, depending on the contents of
   * |data|.
   */
  _fireEventFromMsg: function(data) {
    let name = data.name.substring('browser-element-api:'.length);
    let detail = data.json;

    debug('fireEventFromMsg: ' + name + ', ' + detail);
    let evt = this._createEvent(name, detail,
                                /* cancelable = */ false);
    this._frameElement.dispatchEvent(evt);
  },

  _handleShowModalPrompt: function(data) {
    // Fire a showmodalprmopt event on the iframe.  When this method is called,
    // the child is spinning in a nested event loop waiting for an
    // unblock-modal-prompt message.
    //
    // If the embedder calls preventDefault() on the showmodalprompt event,
    // we'll block the child until event.detail.unblock() is called.
    //
    // Otherwise, if preventDefault() is not called, we'll send the
    // unblock-modal-prompt message to the child as soon as the event is done
    // dispatching.

    let detail = data.json;
    debug('handleShowPrompt ' + JSON.stringify(detail));

    // Strip off the windowID property from the object we send along in the
    // event.
    let windowID = detail.windowID;
    delete detail.windowID;
    debug("Event will have detail: " + JSON.stringify(detail));
    let evt = this._createEvent('showmodalprompt', detail,
                                /* cancelable = */ true);

    let self = this;
    let unblockMsgSent = false;
    function sendUnblockMsg() {
      if (unblockMsgSent) {
        return;
      }
      unblockMsgSent = true;

      // We don't need to sanitize evt.detail.returnValue (e.g. converting the
      // return value of confirm() to a boolean); Gecko does that for us.

      let data = { windowID: windowID,
                   returnValue: evt.detail.returnValue };
      self._sendAsyncMsg('unblock-modal-prompt', data);
    }

    XPCNativeWrapper.unwrap(evt.detail).unblock = function() {
      sendUnblockMsg();
    };

    this._frameElement.dispatchEvent(evt);

    if (!evt.defaultPrevented) {
      // Unblock the inner frame immediately.  Otherwise we'll unblock upon
      // evt.detail.unblock().
      sendUnblockMsg();
    }
  },

  _createEvent: function(evtName, detail, cancelable) {
    // This will have to change if we ever want to send a CustomEvent with null
    // detail.  For now, it's OK.
    if (detail !== undefined && detail !== null) {
      return new this._window.CustomEvent('mozbrowser' + evtName,
                                          { bubbles: true,
                                            cancelable: cancelable,
                                            detail: detail });
    }

    return new this._window.Event('mozbrowser' + evtName,
                                  { bubbles: true,
                                    cancelable: cancelable });
  },

  _sendMozAppManifestURL: function(data) {
    return this._frameElement.getAttribute('mozapp');
  },


  _getScreenshot: function() {
    let id = 'req_' + this._screenshotReqCounter++;
    let req = Services.DOMRequest.createRequest(this._window);
    this._screenshotListeners[id] = req;
    this._sendAsyncMsg('get-screenshot', {id: id});
    return req;
  },

  _recvGotScreenshot: function(data) {
    var req = this._screenshotListeners[data.json.id];
    delete this._screenshotListeners[data.json.id];
    Services.DOMRequest.fireSuccess(req, data.json.screenshot);
  },

  _setVisible: function(visible) {
    this._sendAsyncMsg('set-visible', {visible: visible});
  },

  _fireKeyEvent: function(data) {
    let evt = this._window.document.createEvent("KeyboardEvent");
    evt.initKeyEvent(data.json.type, true, true, this._window,
                     false, false, false, false, // modifiers
                     data.json.keyCode,
                     data.json.charCode);

    this._frameElement.dispatchEvent(evt);
  },
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([BrowserElementParentFactory]);
