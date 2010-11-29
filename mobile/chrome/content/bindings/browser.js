let Cc = Components.classes;
let Ci = Components.interfaces;

dump("!! remote browser loaded\n")

let WebProgressListener = {
  _notifyFlags: [],
  _calculatedNotifyFlags: 0,

  init: function() {
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_ALL);

    addMessageListener("WebProgress:AddProgressListener", this);
    addMessageListener("WebProgress:RemoveProgressListener", this);
  },

  onStateChange: function onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    let webProgress = Ci.nsIWebProgressListener;
    let notifyFlags = 0;
    if (aStateFlags & webProgress.STATE_IS_REQUEST)
      notifyFlags |= Ci.nsIWebProgress.NOTIFY_STATE_REQUEST;
    if (aStateFlags & webProgress.STATE_IS_DOCUMENT)
      notifyFlags |= Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT;
    if (aStateFlags & webProgress.STATE_IS_NETWORK)
      notifyFlags |= Ci.nsIWebProgress.NOTIFY_STATE_NETWORK;
    if (aStateFlags & webProgress.STATE_IS_WINDOW)
      notifyFlags |= Ci.nsIWebProgress.NOTIFY_STATE_WINDOW;

    let json = {
      windowId: aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      stateFlags: aStateFlags,
      status: aStatus,
      notifyFlags: notifyFlags
    };

    sendAsyncMessage("WebProgress:StateChange", json);
  },

  onProgressChange: function onProgressChange(aWebProgress, aRequest, aCurSelf, aMaxSelf, aCurTotal, aMaxTotal) {
    let json = {
      windowId: aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      curSelf: aCurSelf,
      maxSelf: aMaxSelf,
      curTotal: aCurTotal,
      maxTotal: aMaxTotal
    };

    if (this._calculatedNotifyFlags & Ci.nsIWebProgress.NOTIFY_PROGRESS)
      sendAsyncMessage("WebProgress:ProgressChange", json);
  },

  onLocationChange: function onLocationChange(aWebProgress, aRequest, aLocationURI) {
    let location = aLocationURI ? aLocationURI.spec : "";
    let json = {
      windowId: aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      documentURI: aWebProgress.DOMWindow.document.documentURIObject.spec,
      location: location,
      canGoBack: docShell.canGoBack,
      canGoForward: docShell.canGoForward
    };
    sendAsyncMessage("WebProgress:LocationChange", json);
  },

  onStatusChange: function onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    let json = {
      windowId: aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      status: aStatus,
      message: aMessage
    };

    if (this._calculatedNotifyFlags & Ci.nsIWebProgress.NOTIFY_STATUS)
      sendAsyncMessage("WebProgress:StatusChange", json);
  },

  onSecurityChange: function onSecurityChange(aWebProgress, aRequest, aState) {
    let serialization = SecurityUI.getSSLStatusAsString();

    let json = {
      windowId: aWebProgress.DOMWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
      SSLStatusAsString: serialization,
      state: aState
    };
    sendAsyncMessage("WebProgress:SecurityChange", json);
  },

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
      case "WebProgress:AddProgressListener":
        this._notifyFlags.push(aMessage.json.notifyFlags);
        this._calculatedNotifyFlags |= aMessage.json.notifyFlags;
        break;
      case "WebProgress.RemoveProgressListener":
        let index = this._notifyFlags.indexOf(aMessage.json.notifyFlags);
        if (index != -1) {
          this._notifyFlags.splice(index, 1);
          this._calculatedNotifyFlags = this._notifyFlags.reduce(function(a, b) {
            return a | b;
          }, 0);
        }
        break;
    }
  },

  QueryInterface: function QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports)) {
        return this;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

WebProgressListener.init();


let SecurityUI = {
  getSSLStatusAsString: function() {
    let status = docShell.securityUI.QueryInterface(Ci.nsISSLStatusProvider).SSLStatus;

    if (status) {
      let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                      .getService(Ci.nsISerializationHelper);

      status.QueryInterface(Ci.nsISerializable);
      return serhelper.serializeToString(status);
    }

    return null;
  }
};

let WebNavigation =  {
  _webNavigation: docShell.QueryInterface(Ci.nsIWebNavigation),

  init: function() {
    addMessageListener("WebNavigation:GoBack", this);
    addMessageListener("WebNavigation:GoForward", this);
    addMessageListener("WebNavigation:GotoIndex", this);
    addMessageListener("WebNavigation:LoadURI", this);
    addMessageListener("WebNavigation:Reload", this);
    addMessageListener("WebNavigation:Stop", this);
  },

  receiveMessage: function(message) {
    switch (message.name) {
      case "WebNavigation:GoBack":
        this.goBack(message);
        break;
      case "WebNavigation:GoForward":
        this.goForward(message);
        break;
      case "WebNavigation:GotoIndex":
        this.gotoIndex(message);
        break;
      case "WebNavigation:LoadURI":
        this.loadURI(message);
        break;
      case "WebNavigation:Reload":
        this.reload(message);
        break;
      case "WebNavigation:Stop":
        this.stop(message);
        break;
    }
  },

  goBack: function() {
    this._webNavigation.goBack();
  },

  goForward: function() {
    this._webNavigation.goForward();
  },

  gotoIndex: function(message) {
    this._webNavigation.gotoIndex(message.index);
  },

  loadURI: function(message) {
    let flags = message.json.flags || this._webNavigation.LOAD_FLAGS_NONE;
    this._webNavigation.loadURI(message.json.uri, flags, null, null, null);
  },

  reload: function(message) {
    let flags = message.json.flags || this._webNavigation.LOAD_FLAGS_NONE;
    this._webNavigation.reload(flags);
  },

  stop: function(message) {
    let flags = message.json.flags || this._webNavigation.STOP_ALL;
    this._webNavigation.stop(flags);
  }
};

WebNavigation.init();


let DOMEvents =  {
  init: function() {
    addEventListener("DOMContentLoaded", this, false);
    addEventListener("DOMTitleChanged", this, false);
    addEventListener("DOMLinkAdded", this, false);
    addEventListener("DOMWillOpenModalDialog", this, false);
    addEventListener("DOMModalDialogClosed", this, true);
    addEventListener("DOMWindowClose", this, false);
    addEventListener("DOMPopupBlocked", this, false);
    addEventListener("pageshow", this, false);
    addEventListener("pagehide", this, false);
  },

  handleEvent: function(aEvent) {
    let document = content.document;
    switch (aEvent.type) {
      case "DOMContentLoaded":
        if (document.documentURIObject.spec == "about:blank")
          return;

        sendAsyncMessage("DOMContentLoaded", { });
        break;

      case "pageshow":
      case "pagehide": {
        let util = aEvent.target.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                                            .getInterface(Ci.nsIDOMWindowUtils);

        let json = {
          windowId: util.outerWindowID,
          persisted: aEvent.persisted
        };

        // Clear onload focus to prevent the VKB to be shown unexpectingly
        let focusManager = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);
        focusManager.clearFocus(content);

        sendAsyncMessage(aEvent.type, json);
        break;
      }

      case "DOMPopupBlocked": {
        let util = aEvent.requestingWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                          .getInterface(Ci.nsIDOMWindowUtils);
        let json = {
          windowId: util.outerWindowID,
          popupWindowURI: {
            spec: aEvent.popupWindowURI.spec,
            charset: aEvent.popupWindowURI.originCharset
          },
          popupWindowFeatures: aEvent.popupWindowFeatures,
          popupWindowName: aEvent.popupWindowName
        };

        sendAsyncMessage("DOMPopupBlocked", json);
        break;
      }

      case "DOMTitleChanged":
        sendAsyncMessage("DOMTitleChanged", { title: document.title });
        break;

      case "DOMLinkAdded":
        let target = aEvent.originalTarget;
        if (!target.href || target.disabled)
          return;

        let json = {
          windowId: target.ownerDocument.defaultView.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID,
          href: target.href,
          charset: document.characterSet,
          title: target.title,
          rel: target.rel,
          type: target.type
        };

        sendAsyncMessage("DOMLinkAdded", json);
        break;

      case "DOMWillOpenModalDialog":
      case "DOMModalDialogClosed":
      case "DOMWindowClose":
        let retvals = sendSyncMessage(aEvent.type, { });
        for (rv in retvals) {
          if (rv.preventDefault) {
            aEvent.preventDefault();
            break;
          }
        }
        break;
    }
  }
};

DOMEvents.init();

let ContentScroll =  {
  init: function() {
    addMessageListener("Content:ScrollTo", this);
    addMessageListener("Content:ScrollBy", this);
    addMessageListener("Content:SetCacheViewport", this);
    addMessageListener("Content:SetWindowSize", this);

    addEventListener("scroll", this, false);
    addEventListener("MozScrolledAreaChanged", this, false);
  },

  receiveMessage: function(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Content:ScrollTo":
        content.scrollTo(json.x, json.y);
        break;

      case "Content:ScrollBy":
        content.scrollBy(json.dx, json.dy);
        break;

      case "Content:SetCacheViewport": {
        let displayport = new Rect(json.x, json.y, json.w, json.h);
        if (displayport.isEmpty())
          break;

        let scrollOffset = Util.getScrollOffset(content);
        let cwu = Util.getWindowUtils(content);
        cwu.setResolution(json.scale, json.scale);
        cwu.setDisplayPort(displayport.x - scrollOffset.x, displayport.y - scrollOffset.y,
                           displayport.width, displayport.height);
        break;
      }

      case "Content:SetWindowSize": {
        let cwu = Util.getWindowUtils(content);
        cwu.setCSSViewport(json.width, json.height);
        break;
      }
    }
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "scroll":
        let scrollOffset = Util.getScrollOffset(content);
        sendAsyncMessage("scroll", scrollOffset);
        break;

      case "MozScrolledAreaChanged": {
        let doc = aEvent.originalTarget;
        if (content != doc.defaultView) // We are only interested in root scroll pane changes
          return;

        // XXX need to make some things in Util as its own module!
        let scrollOffset = Util.getScrollOffset(content);

        // Adjust width and height from the incoming event properties so that we
        // ignore changes to width and height contributed by growth in page
        // quadrants other than x > 0 && y > 0.
        let x = aEvent.x + scrollOffset.x;
        let y = aEvent.y + scrollOffset.y;
        let width = aEvent.width + (x < 0 ? x : 0);
        let height = aEvent.height + (y < 0 ? y : 0);

        sendAsyncMessage("MozScrolledAreaChanged", {
          width: width,
          height: height
        });

        break;
      }
    }
  }
};

ContentScroll.init();

let ContentActive =  {
  init: function() {
    addMessageListener("Content:Activate", this);
    addMessageListener("Content:Deactivate", this);
  },

  receiveMessage: function(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Content:Deactivate":
        docShell.isActive = false;
        let utils = Util.getWindowUtils(content);
        utils.setDisplayPort(0,0,0,0);
        break;

      case "Content:Activate":
        docShell.isActive = true;
        break;
    }
  }
};

ContentActive.init();
