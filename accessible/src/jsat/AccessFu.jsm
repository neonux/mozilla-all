/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

var EXPORTED_SYMBOLS = ['AccessFu'];

Cu.import('resource://gre/modules/Services.jsm');

Cu.import('resource://gre/modules/accessibility/Presenters.jsm');
Cu.import('resource://gre/modules/accessibility/VirtualCursorController.jsm');

const ACCESSFU_DISABLE = 0;
const ACCESSFU_ENABLE = 1;
const ACCESSFU_AUTO = 2;

var AccessFu = {
  /**
   * Attach chrome-layer accessibility functionality to the given chrome window.
   * If accessibility is enabled on the platform (currently Android-only), then
   * a special accessibility mode is started (see startup()).
   * @param {ChromeWindow} aWindow Chrome window to attach to.
   * @param {boolean} aForceEnabled Skip platform accessibility check and enable
   *  AccessFu.
   */
  attach: function attach(aWindow) {
    if (this.chromeWin)
      // XXX: only supports attaching to one window now.
      throw new Error('Only one window could be attached to AccessFu');

    dump('AccessFu attach!! ' + Services.appinfo.OS + '\n');
    this.chromeWin = aWindow;
    this.presenters = [];

    this.prefsBranch = Cc['@mozilla.org/preferences-service;1']
      .getService(Ci.nsIPrefService).getBranch('accessibility.');
    this.prefsBranch.addObserver('accessfu', this, false);

    let accessPref = ACCESSFU_DISABLE;
    try {
      accessPref = this.prefsBranch.getIntPref('accessfu');
    } catch (x) {
    }

    if (this.amINeeded(accessPref))
      this.enable();
  },

  /**
   * Start AccessFu mode, this primarily means controlling the virtual cursor
   * with arrow keys.
   */
  enable: function enable() {
    dump('AccessFu enable');
    this.addPresenter(new VisualPresenter());

    // Implicitly add the Android presenter on Android.
    if (Services.appinfo.OS == 'Android')
      this.addPresenter(new AndroidPresenter());

    VirtualCursorController.attach(this.chromeWin);

    Services.obs.addObserver(this, 'accessible-event', false);
    this.chromeWin.addEventListener('DOMActivate', this, true);
    this.chromeWin.addEventListener('resize', this, true);
    this.chromeWin.addEventListener('scroll', this, true);
    this.chromeWin.addEventListener('TabOpen', this, true);
  },

  /**
   * Disable AccessFu and return to default interaction mode.
   */
  disable: function disable() {
    dump('AccessFu disable');

    this.presenters.forEach(function(p) { p.detach(); });
    this.presenters = [];

    VirtualCursorController.detach();

    Services.obs.removeObserver(this, 'accessible-event');
    this.chromeWin.removeEventListener('DOMActivate', this, true);
    this.chromeWin.removeEventListener('resize', this, true);
    this.chromeWin.removeEventListener('scroll', this, true);
    this.chromeWin.removeEventListener('TabOpen', this, true);
  },

  amINeeded: function(aPref) {
    switch (aPref) {
      case ACCESSFU_ENABLE:
        return true;
      case ACCESSFU_AUTO:
        if (Services.appinfo.OS == 'Android') {
          let msg = Cc['@mozilla.org/android/bridge;1'].
            getService(Ci.nsIAndroidBridge).handleGeckoMessage(
              JSON.stringify(
                { gecko: {
                    type: 'Accessibility:IsEnabled',
                    eventType: 1,
                    text: []
                  }
                }));
          return JSON.parse(msg).enabled;
        }
      default:
        return false;
    }
  },

  addPresenter: function addPresenter(presenter) {
    this.presenters.push(presenter);
    presenter.attach(this.chromeWin);
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case 'TabOpen':
      {
        let browser = aEvent.target.linkedBrowser || aEvent.target;
        // Store the new browser node. We will need to check later when a new
        // content document is attached if it has been attached to this new tab.
        // If it has, than we will need to send a 'loading' message along with
        // the usual 'newdoc' to presenters.
        this._pendingDocuments[browser] = true;
        this.presenters.forEach(function(p) { p.tabStateChanged(null, 'newtab'); });
        break;
      }
      case 'DOMActivate':
      {
        let activatedAcc = getAccessible(aEvent.originalTarget);
        let state = {};
        activatedAcc.getState(state, {});

        // Checkable objects will have a state changed event that we will use
        // instead of this hackish DOMActivate. We will also know the true
        // action that was taken.
        if (state.value & Ci.nsIAccessibleStates.STATE_CHECKABLE)
          return;

        this.presenters.forEach(function(p) {
                                  p.actionInvoked(activatedAcc, 'click');
                                });
        break;
      }
      case 'scroll':
      case 'resize':
      {
        this.presenters.forEach(function(p) { p.viewportChanged(); });
        break;
      }
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case 'nsPref:changed':
        if (aData == 'accessfu') {
          if (this.amINeeded(this.prefsBranch.getIntPref('accessfu')))
            this.enable();
          else
            this.disable();
        }
        break;
      case 'accessible-event':
        let event;
        try {
          event = aSubject.QueryInterface(Ci.nsIAccessibleEvent);
          this.handleAccEvent(event);
        } catch (ex) {
          dump(ex);
          return;
        }
    }
  },

  handleAccEvent: function handleAccEvent(aEvent) {
    switch (aEvent.eventType) {
      case Ci.nsIAccessibleEvent.EVENT_VIRTUALCURSOR_CHANGED:
        {
          let pivot = aEvent.accessible.
            QueryInterface(Ci.nsIAccessibleCursorable).virtualCursor;
          let event = aEvent.
            QueryInterface(Ci.nsIAccessibleVirtualCursorChangeEvent);

          let newContext = this.getNewContext(event.oldAccessible,
                                              pivot.position);
          this.presenters.forEach(
            function(p) {
              p.pivotChanged(pivot.position, newContext);
            });
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_STATE_CHANGE:
        {
          let event = aEvent.QueryInterface(Ci.nsIAccessibleStateChangeEvent);
          if (event.state == Ci.nsIAccessibleStates.STATE_CHECKED &&
              !(event.isExtraState())) {
            this.presenters.forEach(
              function(p) {
                p.actionInvoked(aEvent.accessible,
                                event.isEnabled() ? 'check' : 'uncheck');
              }
            );
          }
          else if (event.state == Ci.nsIAccessibleStates.STATE_BUSY &&
                   !(event.isExtraState()) && event.isEnabled()) {
            let role = event.accessible.role;
            if ((role == Ci.nsIAccessibleRole.ROLE_DOCUMENT ||
                 role == Ci.nsIAccessibleRole.ROLE_APPLICATION)) {
              // An existing document has changed to state "busy", this means
              // something is loading. Send a 'loading' message to presenters.
              this.presenters.forEach(
                function(p) {
                  p.tabStateChanged(event.accessible, 'loading');
                }
              );
            }
          }
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_REORDER:
        {
          let acc = aEvent.accessible;
          if (acc.childCount) {
            let docAcc = acc.getChildAt(0);
            if (this._pendingDocuments[aEvent.DOMNode]) {
              // This is a document in a new tab. Check if it is
              // in a BUSY state (i.e. loading), and inform presenters.
              // We need to do this because a state change event will not be
              // fired when an object is created with the BUSY state.
              // If this is not a new tab, don't bother because we sent 'loading'
              // when the previous doc changed its state to BUSY.
              let state = {};
              docAcc.getState(state, {});
              if (state.value & Ci.nsIAccessibleStates.STATE_BUSY &&
                  this.isNotChromeDoc(docAcc))
                this.presenters.forEach(
                  function(p) { p.tabStateChanged(docAcc, 'loading'); }
                );
              delete this._pendingDocuments[aEvent.DOMNode];
            }
            if (this.isBrowserDoc(docAcc))
              // A new top-level content document has been attached
              this.presenters.forEach(
                function(p) { p.tabStateChanged(docAcc, 'newdoc'); }
              );
          }
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_COMPLETE:
        {
          if (this.isNotChromeDoc(aEvent.accessible)) {
            this.presenters.forEach(
              function(p) {
                p.tabStateChanged(aEvent.accessible, 'loaded');
              }
            );
          }
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_DOCUMENT_LOAD_STOPPED:
        {
          this.presenters.forEach(
            function(p) {
              p.tabStateChanged(aEvent.accessible, 'loadstopped');
            }
          );
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_DOCUMENT_RELOAD:
        {
          this.presenters.forEach(
            function(p) {
              p.tabStateChanged(aEvent.accessible, 'reload');
            }
          );
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_FOCUS:
        {
          if (this.isBrowserDoc(aEvent.accessible)) {
            // The document recieved focus, call tabSelected to present current tab.
            this.presenters.forEach(
              function(p) { p.tabSelected(aEvent.accessible); });
          }
          break;
        }
      case Ci.nsIAccessibleEvent.EVENT_TEXT_INSERTED:
      case Ci.nsIAccessibleEvent.EVENT_TEXT_REMOVED:
      {
        if (aEvent.isFromUserInput) {
          // XXX support live regions as well.
          let event = aEvent.QueryInterface(Ci.nsIAccessibleTextChangeEvent);
          let isInserted = event.isInserted();
          let textIface = aEvent.accessible.QueryInterface(Ci.nsIAccessibleText);

          let text = '';
          try {
            text = textIface.
              getText(0, Ci.nsIAccessibleText.TEXT_OFFSET_END_OF_TEXT);
          } catch (x) {
            // XXX we might have gotten an exception with of a
            // zero-length text. If we did, ignore it (bug #749810).
            if (textIface.characterCount)
              throw x;
          }

          this.presenters.forEach(
            function(p) {
              p.textChanged(isInserted, event.start, event.length, text, event.modifiedText);
            }
          );
        }
        break;
      }
      default:
        break;
    }
  },

  /**
   * Check if accessible is a top-level content document (i.e. a child of a XUL
   * browser node).
   * @param {nsIAccessible} aDocAcc the accessible to check.
   * @return {boolean} true if this is a top-level content document.
   */
  isBrowserDoc: function isBrowserDoc(aDocAcc) {
    let parent = aDocAcc.parent;
    if (!parent)
      return false;

    let domNode = parent.DOMNode;
    if (!domNode)
      return false;

    const ns = 'http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul';
    return (domNode.localName == 'browser' && domNode.namespaceURI == ns);
  },

  /**
   * Check if document is not a local "chrome" document, like about:home.
   * @param {nsIDOMDocument} aDocument the document to check.
   * @return {boolean} true if this is not a chrome document.
   */
  isNotChromeDoc: function isNotChromeDoc(aDocument) {
    let location = aDocument.DOMNode.location;
    if (!location)
      return false;

    return location.protocol != "about:";
  },

  getNewContext: function getNewContext(aOldObject, aNewObject) {
    let newLineage = [];
    let oldLineage = [];

    let parent = aNewObject;
    while ((parent = parent.parent))
      newLineage.push(parent);

    if (aOldObject) {
      parent = aOldObject;
      while ((parent = parent.parent))
        oldLineage.push(parent);
    }

//    newLineage.reverse();
//    oldLineage.reverse();

    let i = 0;
    let newContext = [];

    while (true) {
      let newAncestor = newLineage.pop();
      let oldAncestor = oldLineage.pop();

      if (newAncestor == undefined)
        break;

      if (newAncestor != oldAncestor)
        newContext.push(newAncestor);
      i++;
    }

    return newContext;
  },

  // A hash of documents that don't yet have an accessible tree.
  _pendingDocuments: {}
};

function getAccessible(aNode) {
  try {
    return Cc['@mozilla.org/accessibleRetrieval;1'].
      getService(Ci.nsIAccessibleRetrieval).getAccessibleFor(aNode);
  } catch (e) {
    return null;
  }
}
