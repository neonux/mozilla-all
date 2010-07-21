// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "PluralForm", function() {
  Cu.import("resource://gre/modules/PluralForm.jsm");
  return PluralForm;
});

XPCOMUtils.defineLazyGetter(this, "PlacesUtils", function() {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
  return PlacesUtils;
});

const TOOLBARSTATE_LOADING  = 1;
const TOOLBARSTATE_LOADED   = 2;

[
  [
    "gHistSvc",
    "@mozilla.org/browser/nav-history-service;1",
    [Ci.nsINavHistoryService, Ci.nsIBrowserHistory]
  ],
  [
    "gFaviconService",
     "@mozilla.org/browser/favicon-service;1",
     [Ci.nsIFaviconService]
  ],
  [
    "gURIFixup",
    "@mozilla.org/docshell/urifixup;1",
    [Ci.nsIURIFixup]
  ],
  [
    "gFocusManager",
    "@mozilla.org/focus-manager;1",
    [Ci.nsIFocusManager]
  ]
].forEach(function (service) {
  let [name, contract, ifaces] = service;
  window.__defineGetter__(name, function () {
    delete window[name];
    window[name] = Cc[contract].getService(ifaces.splice(0, 1)[0]);
    if (ifaces.length)
      ifaces.forEach(function (i) { return window[name].QueryInterface(i); });
    return window[name];
  });
});

var BrowserUI = {
  _edit : null,
  _throbber : null,
  _favicon : null,
  _dialogs: [],

  _domWillOpenModalDialog: function(aBrowser) {
    // We're about to open a modal dialog, make sure the opening
    // tab is brought to the front.

    for (let i = 0; i < Browser.tabs.length; i++) {
      if (Browser._tabs[i].browser == aBrowser) {
        Browser.selectedTab = Browser.tabs[i];
        break;
      }
    }
  },

  _titleChanged: function(aBrowser) {
    var browser = Browser.selectedBrowser;
    if (browser && aBrowser != browser)
      return;

    var url = this.getDisplayURI(browser);
    var caption = browser.contentTitle || url;

    if (Util.isURLEmpty(url))
      caption = "";

    this._setURI(caption);
  },

  /*
   * Dispatched by window.close() to allow us to turn window closes into tabs
   * closes.
   */
  _domWindowClose: function(aBrowser) {
     // Find the relevant tab, and close it.
     let browsers = Browser.browsers;
     for (let i = 0; i < browsers.length; i++) {
      if (browsers[i] == aBrowser) {
        Browser.closeTab(Browser.getTabAtIndex(i));
        return { preventDefault: true };
      }
    }
  },

  _updateButtons: function(aBrowser) {
    let back = document.getElementById("cmd_back");
    let forward = document.getElementById("cmd_forward");

    back.setAttribute("disabled", !aBrowser.canGoBack);
    forward.setAttribute("disabled", !aBrowser.canGoForward);
  },

  _updateToolbar: function _updateToolbar() {
    let icons = document.getElementById("urlbar-icons");
    let mode = icons.getAttribute("mode");
    if (Browser.selectedTab.isLoading() && mode != "loading") {
      icons.setAttribute("mode", "loading");
    }
    else if (mode != "view") {
      icons.setAttribute("mode", "view");
    }
  },

  _tabSelect: function(aEvent) {
    let browser = Browser.selectedBrowser;
    this._titleChanged(browser);
    this._updateToolbar();
    this._updateButtons(browser);
    this._updateIcon(browser.mIconURL);
    this.updateStar();
  },

  showToolbar: function showToolbar(aEdit) {
    this.hidePanel();
    this._editURI(aEdit);
    if (aEdit)
      this.showAutoComplete();
  },

  _toolbarLocked: 0,

  isToolbarLocked: function isToolbarLocked() {
    return this._toolbarLocked;
  },

  lockToolbar: function lockToolbar() {
    this._toolbarLocked++;
    document.getElementById("toolbar-moveable-container").top = "0";
    if (this._toolbarLocked == 1)
      Browser.forceChromeReflow();
  },

  unlockToolbar: function unlockToolbar() {
    if (!this._toolbarLocked)
      return;

    this._toolbarLocked--;
    if (!this._toolbarLocked)
      document.getElementById("toolbar-moveable-container").top = "";
  },

  _setURI: function _setURI(aCaption) {
    if (this.isAutoCompleteOpen())
      this._edit.defaultValue = aCaption;
    else
      this._edit.value = aCaption;
  },

  _editURI: function _editURI(aEdit) {
    var icons = document.getElementById("urlbar-icons");
    if (aEdit && icons.getAttribute("mode") != "edit") {
      icons.setAttribute("mode", "edit");
      this._edit.defaultValue = this._edit.value;

      let urlString = this.getDisplayURI(Browser.selectedBrowser);
      if (Util.isURLEmpty(urlString))
        urlString = "";
      this._edit.value = urlString;

      // This is a workaround for bug 488420, needed to cycle focus for the
      // IME state to be set properly. Testing shows we only really need to
      // do this the first time.
      this._edit.blur();
      gFocusManager.setFocus(this._edit, Ci.nsIFocusManager.FLAG_NOSCROLL);
    }
    else if (!aEdit) {
      this._updateToolbar();
    }
  },

  _closeOrQuit: function _closeOrQuit() {
    // Close active dialog, if we have one. If not then close the application.
    let dialog = this.activeDialog;
    if (dialog) {
      dialog.close();
    } else {
      // Check to see if we should really close the window
      if (Browser.closing())
        window.close();
    }
  },

  get activeDialog() {
    // Return the topmost dialog
    if (this._dialogs.length)
      return this._dialogs[this._dialogs.length - 1];
    return null;
  },

  pushDialog: function pushDialog(aDialog) {
    // If we have a dialog push it on the stack and set the attr for CSS
    if (aDialog) {
      this.lockToolbar();
      this._dialogs.push(aDialog);
      document.getElementById("toolbar-main").setAttribute("dialog", "true");
      Elements.contentShowing.setAttribute("disabled", "true");
    }
  },

  popDialog: function popDialog() {
    if (this._dialogs.length) {
      this._dialogs.pop();
      this.unlockToolbar();
    }

    // If no more dialogs are being displayed, remove the attr for CSS
    if (!this._dialogs.length) {
      document.getElementById("toolbar-main").removeAttribute("dialog");
      Elements.contentShowing.removeAttribute("disabled");
    }
  },

  pushPopup: function pushPopup(aPanel, aElements) {
    this._hidePopup();
    this._popup =  { "panel": aPanel,
                     "elements": (aElements instanceof Array) ? aElements : [aElements] };
    this._dispatchPopupChanged();
  },

  popPopup: function popPopup() {
    this._popup = null;
    this._dispatchPopupChanged();
  },

  _dispatchPopupChanged: function _dispatchPopupChanged() {
    let stack = document.getElementById("stack");
    let event = document.createEvent("Events");
    event.initEvent("PopupChanged", true, false);
    event.popup = this._popup;
    stack.dispatchEvent(event);
  },

  _hidePopup: function _hidePopup() {
    if (!this._popup)
      return;
    let panel = this._popup.panel;
    if (panel.hide)
      panel.hide();
  },

  _isEventInsidePopup: function _isEventInsidePopup(aEvent) {
    if (!this._popup)
      return false;
    let elements = this._popup.elements;
    let targetNode = aEvent ? aEvent.target : null;
    while (targetNode && elements.indexOf(targetNode) == -1)
      targetNode = targetNode.parentNode;
    return targetNode ? true : false;
  },

  switchPane: function switchPane(id) {
    let button = document.getElementsByAttribute("linkedpanel", id)[0];
    if (button)
      button.checked = true;

    this.blurFocusedElement();

    let pane = document.getElementById(id);
    document.getElementById("panel-items").selectedPanel = pane;
  },

  get toolbarH() {
    if (!this._toolbarH) {
      let toolbar = document.getElementById("toolbar-main");
      this._toolbarH = toolbar.boxObject.height;
    }
    return this._toolbarH;
  },

  get sidebarW() {
    if (!this._sidebarW) {
      let sidebar = document.getElementById("browser-controls");
      this._sidebarW = sidebar.boxObject.width;
    }
    return this._sidebarW;
  },

  get starButton() {
    delete this.starButton;
    return this.starButton = document.getElementById("tool-star");
  },

  sizeControls: function(windowW, windowH) {
    // tabs
    document.getElementById("tabs").resize();

    // awesomebar
    let popup = document.getElementById("popup_autocomplete");
    popup.top = this.toolbarH;
    popup.height = windowH - this.toolbarH;
    popup.width = windowW;

    // content navigator helper
    let contentHelper = document.getElementById("content-navigator");
    contentHelper.top = windowH - contentHelper.getBoundingClientRect().height;
  },

  init: function() {
    this._edit = document.getElementById("urlbar-edit");
    this._throbber = document.getElementById("urlbar-throbber");
    this._favicon = document.getElementById("urlbar-favicon");
    this._favicon.addEventListener("error", this, false);

    this._edit.addEventListener("click", this, false);
    this._edit.addEventListener("mousedown", this, false);

    document.getElementById("toolbar-main").ignoreDrag = true;

    let tabs = document.getElementById("tabs");
    tabs.addEventListener("TabSelect", this, true);
    tabs.addEventListener("TabOpen", this, true);

    // listen content messages
    messageManager.addMessageListener("DOMLinkAdded", this);
    messageManager.addMessageListener("DOMTitleChanged", this);
    messageManager.addMessageListener("DOMWillOpenModalDialog", this);
    messageManager.addMessageListener("DOMWindowClose", this);

    messageManager.addMessageListener("Browser:Highlight", this);
    messageManager.addMessageListener("Browser:OpenURI", this);
    messageManager.addMessageListener("Browser:ContextMenu", ContextHelper);
    messageManager.addMessageListener("Browser:SaveAs:Return", this);

    // listening mousedown for automatically dismiss some popups (e.g. larry)
    window.addEventListener("mousedown", this, true);

    // listening escape to dismiss dialog on VK_ESCAPE
    window.addEventListener("keypress", this, true);

    // listening AppCommand to handle special keys
    window.addEventListener("AppCommand", this, true);

    // Push the panel initialization out of the startup path
    // (Using a message because we have no good way to delay-init [Bug 535366])
    messageManager.addMessageListener("DOMContentLoaded", function() {
      // We only want to delay one time
      messageManager.removeMessageListener("DOMContentLoaded", arguments.callee, true);

      // We unhide the panelUI so the XBL and settings can initialize
      Elements.panelUI.hidden = false;

      // Init the views
      ExtensionsView.init();
      DownloadsView.init();
      PreferencesView.init();
      ConsoleView.init();

      // Init the sync system
      WeaveGlue.init();
    });

    FormHelperUI.init();
    FindHelperUI.init();
  },

  uninit: function() {
    ExtensionsView.uninit();
    ConsoleView.uninit();
  },

  update: function(aState) {
    let icons = document.getElementById("urlbar-icons");
    let browser = Browser.selectedBrowser;

    switch (aState) {
      case TOOLBARSTATE_LOADED:
        if (icons.getAttribute("mode") != "edit")
          this._updateToolbar();

        this._updateIcon(browser.mIconURL);
        this.unlockToolbar();
        break;

      case TOOLBARSTATE_LOADING:
        if (icons.getAttribute("mode") != "edit")
          this._updateToolbar();

        browser.mIconURL = "";
        this._updateIcon();
        this.lockToolbar();
        break;
    }
  },

  _updateIcon: function(aIconSrc) {
    this._favicon.src = aIconSrc || "";
    if (Browser.selectedTab.isLoading()) {
      this._throbber.hidden = false;
      this._throbber.setAttribute("loading", "true");
      this._favicon.hidden = true;
    }
    else {
      this._favicon.hidden = false;
      this._throbber.hidden = true;
      this._throbber.removeAttribute("loading");
    }
  },

  getDisplayURI: function(browser) {
    if (!this._URIFixup)
      this._URIFixup = Cc["@mozilla.org/docshell/urifixup;1"].getService(Ci.nsIURIFixup);

    let uri = browser.currentURI;
    try {
      uri = this._URIFixup.createExposableURI(uri);
    } catch (ex) {}

    return uri.spec;
  },

  /* Set the location to the current content */
  updateURI: function() {
    var browser = Browser.selectedBrowser;

    // FIXME: deckbrowser should not fire TabSelect on the initial tab (bug 454028)
    if (!browser.currentURI)
      return;

    // Update the navigation buttons
    this._updateButtons(browser);

    // Check for a bookmarked page
    this.updateStar();

    var urlString = this.getDisplayURI(browser);
    if (Util.isURLEmpty(urlString))
      urlString = "";

    this._setURI(urlString);
  },

  goToURI: function(aURI) {
    aURI = aURI || this._edit.value;
    if (!aURI)
      return;

    // Make sure we're online before attempting to load
    Util.forceOnline();

    // Give the new page lots of room
    Browser.hideSidebars();
    this.closeAutoComplete(true);

    this._edit.value = aURI;

    let fixupFlags = Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    let uri = gURIFixup.createFixupURI(aURI, fixupFlags);

    // We need to keep about: pages opening in new "local" tabs. We also want to spawn
    // new "remote" tabs if opening web pages from a "local" about: page.
    let currentURI = getBrowser().currentURI;
    let useLocal = Util.isLocalScheme(uri.spec);
    let hasLocal = Util.isLocalScheme(currentURI.spec);
    if (useLocal || hasLocal != useLocal) {
      Browser.addTab(uri.spec, true);
    } else {
      let loadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
      getBrowser().loadURIWithFlags(uri.spec, loadFlags, null, null);
    }

    gHistSvc.markPageAsTyped(uri);
  },

  showAutoComplete: function showAutoComplete() {
    if (this.isAutoCompleteOpen())
      return;

    BrowserSearch.updateSearchButtons();

    this._hidePopup();
    this._edit.showHistoryPopup();
  },

  closeAutoComplete: function closeAutoComplete(aResetInput) {
    if (!this.isAutoCompleteOpen())
      return;

    if (aResetInput)
      this._edit.popup.close();
    else
      this._edit.popup.closePopup();
  },

  isAutoCompleteOpen: function isAutoCompleteOpen() {
    return this._edit.popup.popupOpen;
  },

  doButtonSearch: function(button) {
    if (!("engine" in button) || !button.engine)
      return;

    // We don't want the button to look pressed for now
    button.parentNode.selectedItem = null;

    // Give the new page lots of room
    Browser.hideSidebars();
    this.closeAutoComplete(false);

    // Make sure we're online before attempting to load
    Util.forceOnline();

    let submission = button.engine.getSubmission(this._edit.value, null);
    let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

    let currentURI = getBrowser().currentURI;
    let hasLocal = Util.isLocalScheme(currentURI.spec);
    if (hasLocal) {
      // We are in a local browser, so make a new tab and stop the initial load
      let tab = Browser.addTab("about:blank", true);
      tab.browser.stop();
    }

    getBrowser().loadURIWithFlags(submission.uri.spec, flags, null, null, submission.postData);
  },

  updateStar: function() {
    if (PlacesUtils.getMostRecentBookmarkForURI(Browser.selectedBrowser.currentURI) != -1)
      this.starButton.setAttribute("starred", "true");
    else
      this.starButton.removeAttribute("starred");
  },

  newTab: function newTab(aURI, aOwner) {
    aURI = aURI || "about:blank";
    let tab = Browser.addTab(aURI, true, aOwner);

    this.hidePanel();

    if (aURI == "about:blank") {
      // Display awesomebar UI
      this.showToolbar(true);
    }
    else {
      // Give the new page lots of room
      Browser.hideSidebars();
      this.closeAutoComplete(true);
    }

    return tab;
  },

  newOrSelectTab: function newOrSelectTab(aURI, aOwner) {
    let tabs = Browser.tabs;
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].browser.currentURI.spec == aURI) {
        Browser.selectedTab = tabs[i];
        return;
      }
    }
    this.newTab(aURI, aOwner);
  },

  closeTab: function closeTab(aTab) {
    // If no tab is passed in, assume the current tab
    Browser.closeTab(aTab || Browser.selectedTab);
  },

  selectTab: function selectTab(aTab) {
    Browser.selectedTab = aTab;
  },

  undoCloseTab: function undoCloseTab(aIndex) {
    let tab = null;
    let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
    if (ss.getClosedTabCount(window) > (aIndex || 0)) {
      tab = ss.undoCloseTab(window, aIndex || 0);
    }
    return tab;
  },

  isTabsVisible: function isTabsVisible() {
    // The _1, _2 and _3 are to make the js2 emacs mode happy
    let [leftvis,_1,_2,_3] = Browser.computeSidebarVisibility();
    return (leftvis > 0.002);
  },

  showPanel: function showPanel(aPage) {
    Elements.panelUI.left = 0;
    Elements.panelUI.hidden = false;
    Elements.contentShowing.setAttribute("disabled", "true");

    if (aPage != undefined)
      this.switchPane(aPage);
  },

  hidePanel: function hidePanel() {
    if (!this.isPanelVisible())
      return;
    Elements.panelUI.hidden = true;
    Elements.contentShowing.removeAttribute("disabled");
    this.blurFocusedElement();
  },

  isPanelVisible: function isPanelVisible() {
    return (!Elements.panelUI.hidden && Elements.panelUI.left == 0);
  },

  blurFocusedElement: function blurFocusedElement() {
    let focusedElement = document.commandDispatcher.focusedElement;
    if (focusedElement)
      focusedElement.blur();
  },

  switchTask: function switchTask() {
    try {
      let phone = Cc["@mozilla.org/phone/support;1"].createInstance(Ci.nsIPhoneSupport);
      phone.switchTask();
    } catch(e) { }
  },

  handleEscape: function () {
    // Check open dialogs
    let dialog = this.activeDialog;
    if (dialog) {
      dialog.close();
      return;
    }
    
    // Check open popups
    if (this._popup) {
      this._hidePopup();
      return;
    }
      
    // Check open modal elements
    let modalElementsLength = document.getElementsByClassName("modal-block").length;
    if (modalElementsLength > 0) 
      return;

    // Check open panel
    if (this.isPanelVisible()) {
      this.hidePanel();
      return;
    }

    // Only if there are no dialogs, popups, or panels open
    let tab = Browser.selectedTab;
    let browser = tab.browser;

    if (browser.canGoBack) {
      browser.goBack();
    } else if (tab.owner) {
      this.closeTab(tab);
    }
#ifdef ANDROID
    else if (Browser.tabs.length == 1) {
      this.doCommand("cmd_quit");
    } else {
      this.closeTab(tab);
      window.QueryInterface(Ci.nsIDOMChromeWindow).minimize();
    }
#endif
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      // Browser events
      case "TabSelect":
        this._tabSelect(aEvent);
        break;
      case "TabOpen":
      {
        let [tabsVisibility,,,] = Browser.computeSidebarVisibility();
        if (!(tabsVisibility == 1.0) && Browser.selectedTab.chromeTab != aEvent.target)
          NewTabPopup.show(aEvent.target);

        // Workaround to hide the tabstrip if it is partially visible
        // See bug 524469
        if (tabsVisibility > 0.0 && tabsVisibility < 1.0)
          Browser.hideSidebars();

        break;
      }
      // Window events
      case "keypress":
        if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE)
          this.handleEscape();
        break;
      case "AppCommand":
        aEvent.stopPropagation();
        switch (aEvent.command) {
          case "Menu":
            this.doCommand("cmd_menu");
            break;
          case "Search":
            this.doCommand("cmd_openLocation");
            break;
          default:
            break;
        }
        break;
      // URL textbox events
      case "click":
        this.doCommand("cmd_openLocation");
        break;
      case "mousedown":
        if (!this._isEventInsidePopup(aEvent))
          this._hidePopup();

        let selectAll = Services.prefs.getBoolPref("browser.urlbar.doubleClickSelectsAll");
        if (aEvent.detail == 2 && aEvent.button == 0 && selectAll && aEvent.target == this._edit) {
          this._edit.editor.selectAll();
          aEvent.preventDefault();
        }
        break;
      // Favicon events
      case "error":
        this._favicon.src = "";
        break;
    }
  },

  receiveMessage: function receiveMessage(aMessage) {
    let browser = aMessage.target;
    let json = aMessage.json;
    switch (aMessage.name) {
      case "DOMTitleChanged":
        this._titleChanged(browser);
        break;
      case "DOMWillOpenModalDialog":
        return this._domWillOpenModalDialog(browser);
        break;
      case "DOMWindowClose":
        return this._domWindowClose(browser);
        break;
      case "DOMLinkAdded":
        if (Browser.selectedBrowser == browser)
          this._updateIcon(Browser.selectedBrowser.mIconURL);
        break;
      case "Browser:SaveAs:Return":
        if (json.type != Ci.nsIPrintSettings.kOutputFormatPDF)
          return;

        let dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
        let db = dm.DBConnection;
        let stmt = db.createStatement("UPDATE moz_downloads SET endTime = :endTime, state = :state WHERE id = :id");
        stmt.params.endTime = Date.now() * 1000;
        stmt.params.state = Ci.nsIDownloadManager.DOWNLOAD_FINISHED;
        stmt.params.id = json.id;
        stmt.execute();
        stmt.finalize();

        let download = dm.getDownload(json.id);
        try {
          DownloadsView.downloadCompleted(download);
          let element = DownloadsView.getElementForDownload(json.id);
          element.setAttribute("state", Ci.nsIDownloadManager.DOWNLOAD_FINISHED);
          element.setAttribute("endTime", Date.now());
          element.setAttribute("referrer", json.referrer);
          DownloadsView._updateTime(element);
          DownloadsView._updateStatus(element);
        }
        catch(e) {}
        Services.obs.notifyObservers(download, "dl-done", null);
        break;

      case "Browser:Highlight":
        let rects = [];
        for (let i = 0; i < json.rects.length; i++) {
          let rect = json.rects[i];
          rects.push(new Rect(rect.left, rect.top, rect.width, rect.height));
        }
        TapHighlightHelper.show(rects);
        break;

      case "Browser:OpenURI":
        Browser.addTab(json.uri, false, Browser.selectedTab);
    }
  },

  supportsCommand : function(cmd) {
    var isSupported = false;
    switch (cmd) {
      case "cmd_back":
      case "cmd_forward":
      case "cmd_reload":
      case "cmd_forceReload":
      case "cmd_stop":
      case "cmd_go":
      case "cmd_openLocation":
      case "cmd_star":
      case "cmd_bookmarks":
      case "cmd_quit":
      case "cmd_close":
      case "cmd_menu":
      case "cmd_newTab":
      case "cmd_closeTab":
      case "cmd_undoCloseTab":
      case "cmd_actions":
      case "cmd_panel":
      case "cmd_sanitize":
      case "cmd_zoomin":
      case "cmd_zoomout":
      case "cmd_volumeLeft":
      case "cmd_volumeRight":
      case "cmd_lockscreen":
        isSupported = true;
        break;
      default:
        isSupported = false;
        break;
    }
    return isSupported;
  },

  isCommandEnabled : function(cmd) {
    return true;
  },

  doCommand : function(cmd) {
    var browser = getBrowser();
    switch (cmd) {
      case "cmd_back":
        browser.goBack();
        break;
      case "cmd_forward":
        browser.goForward();
        break;
      case "cmd_reload":
        browser.reload();
        break;
      case "cmd_forceReload":
      {
        // Simulate a new page
        browser.lastLocation = null;

        const reloadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_PROXY |
                            Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
        browser.reloadWithFlags(reloadFlags);
        break;
      }
      case "cmd_stop":
        browser.stop();
        break;
      case "cmd_go":
        this.goToURI();
        break;
      case "cmd_openLocation":
        this.showToolbar(true);
        break;
      case "cmd_star":
      {
        var bookmarkURI = browser.currentURI;
        var bookmarkTitle = browser.contentTitle || bookmarkURI.spec;

        let autoClose = false;

        if (PlacesUtils.getMostRecentBookmarkForURI(bookmarkURI) == -1) {
          let bmsvc = PlacesUtils.bookmarks;
          let bookmarkId = bmsvc.insertBookmark(BookmarkList.mobileRoot, bookmarkURI,
                                                bmsvc.DEFAULT_INDEX,
                                                bookmarkTitle);
          this.updateStar();

          // autoclose the bookmark popup
          autoClose = true;
        }

        // Show/hide bookmark popup
        BookmarkPopup.toggle(autoClose);
        break;
      }
      case "cmd_bookmarks":
        BookmarkList.show();
        break;
      case "cmd_quit":
        goQuitApplication();
        break;
      case "cmd_close":
        this._closeOrQuit();
        break;
      case "cmd_menu":
        getIdentityHandler().toggle();
        break;
      case "cmd_newTab":
        this.newTab();
        break;
      case "cmd_closeTab":
        this.closeTab();
        break;
      case "cmd_undoCloseTab":
        this.undoCloseTab();
        break;
      case "cmd_sanitize":
      {
        // disable the button temporarily to indicate something happened
        let button = document.getElementById("prefs-clear-data");
        button.disabled = true;
        setTimeout(function() { button.disabled = false; }, 5000);

        Sanitizer.sanitize();
        break;
      }
      case "cmd_panel":
      {
        if (BrowserUI.isPanelVisible())
          this.hidePanel();
        else
          this.showPanel();
        break;
      }
      case "cmd_zoomin":
        Browser.zoom(-1);
        break;
      case "cmd_zoomout":
        Browser.zoom(1);
        break;
      case "cmd_volumeLeft":
        // Zoom in (portrait) or out (landscape)
        Browser.zoom(Util.isPortrait() ? -1 : 1);
        break;
      case "cmd_volumeRight":
        // Zoom out (portrait) or in (landscape)
        Browser.zoom(Util.isPortrait() ? 1 : -1);
        break;
      case "cmd_lockscreen":
      {
        let locked = Services.prefs.getBoolPref("toolkit.screen.lock");
        Services.prefs.setBoolPref("toolkit.screen.lock", !locked);

        let strings = Elements.browserBundle;
        let alerts = Cc["@mozilla.org/alerts-service;1"].getService(Ci.nsIAlertsService);
        alerts.showAlertNotification(null, strings.getString("alertLockScreen"),
                                     strings.getString("alertLockScreen." + (!locked ? "locked" : "unlocked")), false, "", null);
        break;
      }
    }
  }
};

var TapHighlightHelper = {
  get _overlay() {
    delete this._overlay;
    return this._overlay = document.getElementById("content-overlay");
  },

  show: function show(aRects) {
    let bv = Browser._browserView;
    let union = aRects.reduce(function(a, b) {
      return a.expandToContain(b);
    }, new Rect(0, 0, 0, 0)).map(bv.browserToViewport);

    let vis = Browser.getVisibleRect();
    let canvasArea = vis.intersect(union);

    let overlay = this._overlay;
    overlay.width = canvasArea.width;
    overlay.style.width = canvasArea.width + "px";
    overlay.height = canvasArea.height;
    overlay.style.height = canvasArea.height + "px";

    let ctx = overlay.getContext("2d");
    ctx.save();
    ctx.translate(-canvasArea.left, -canvasArea.top);
    bv.browserToViewportCanvasContext(ctx);

    overlay.style.left = canvasArea.left + "px";
    overlay.style.top = canvasArea.top + "px";
    ctx.fillStyle = "rgba(0, 145, 255, .5)";
    for (let i = aRects.length - 1; i >= 0; i--) {
      let rect = aRects[i];
      ctx.fillRect(rect.left, rect.top, rect.width, rect.height);
    }
    ctx.restore();
    overlay.style.display = "block";
  },

  hide: function hide() {
    this._overlay.style.display = "none";
  }
}

var PageActions = {
  get _loginManager() {
    delete this._loginManager;
    return this._loginManager = Cc["@mozilla.org/login-manager;1"].getService(Ci.nsILoginManager);
  },

  // This is easy for an addon to add his own perm type here
  _permissions: ["popup", "offline-app", "geo"],

  _forEachPermissions: function _forEachPermissions(aHost, aCallback) {
    let pm = Services.perms;
    for (let i = 0; i < this._permissions.length; i++) {
      let type = this._permissions[i];
      if (!pm.testPermission(aHost, type))
        continue;

      let perms = pm.enumerator;
      while (perms.hasMoreElements()) {
        let permission = perms.getNext().QueryInterface(Ci.nsIPermission);
        if (permission.host == aHost.asciiHost && permission.type == type)
          aCallback(type);
      }
    }
  },

  updatePagePermissions: function updatePagePermissions() {
    this.removeItems("preferences");

    let host = Browser.selectedBrowser.currentURI;
    let permissions = [];

    this._forEachPermissions(host, function(aType) {
      permissions.push(aType);
    });

    let lm = this._loginManager;
    if (!lm.getLoginSavingEnabled(host.prePath)) {
      permissions.push("password");
    }

    // Show the clear site preferences button if needed
    if (permissions.length) {
      let title = Elements.browserBundle.getString("pageactions.reset");
      let description = [];
      for each(permission in permissions)
        description.push(Elements.browserBundle.getString("pageactions." + permission));

      let node = this.appendItem("preferences", title, description.join(", "));
      node.addEventListener("click", function(event) {
          PageActions.clearPagePermissions();
          PageActions.removeItem(node);
        },
        false);
    }

    let siteLogins = [];
    let allLogins = lm.findLogins({}, host.prePath, "", null);
    for (let i = 0; i < allLogins.length; i++) {
      let login = allLogins[i];
      if (login.hostname != host.prePath)
        continue;

      siteLogins.push(login);
    }

    // Show only 1 password button for all the saved logins
    if (siteLogins.length) {
      let title = Elements.browserBundle.getString("pageactions.password.forget");
      let node = this.appendItem("preferences", title, "");
      node.addEventListener("click", function(event) {
          for (let i = 0; i < siteLogins.length; i++)
            lm.removeLogin(siteLogins[i]);
  
          PageActions.removeItem(node);
        },
        false);
    }
  },

  clearPagePermissions: function clearPagePermissions() {
    let pm = Services.perms;
    let host = Browser.selectedBrowser.currentURI;
    this._forEachPermissions(host, function(aType) {
      pm.remove(host.asciiHost, aType);
    });

    let lm = this._loginManager;
    if (!lm.getLoginSavingEnabled(host.prePath))
      lm.setLoginSavingEnabled(host.prePath, true);
  },

  _savePageAsPDF: function saveAsPDF() {
    let strings = Elements.browserBundle;
    let picker = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    picker.init(window, strings.getString("pageactions.saveas.pdf"), Ci.nsIFilePicker.modeSave);
    picker.appendFilter("PDF", "*.pdf");
    picker.defaultExtension = "pdf";

    let browser = Browser.selectedBrowser;
    let fileName = getDefaultFileName(browser.contentTitle, browser.documentURI, null, null);
    fileName = fileName.trim();
#ifdef MOZ_PLATFORM_MAEMO
    fileName = fileName.replace(/[\*\:\?]+/g, " ");
#endif
    picker.defaultString = fileName + ".pdf";

    let dm = Cc["@mozilla.org/download-manager;1"].getService(Ci.nsIDownloadManager);
    picker.displayDirectory = dm.defaultDownloadsDirectory;
    let rv = picker.show();
    if (rv == Ci.nsIFilePicker.returnCancel)
      return;

    // We must manually add this to the download system
    let db = dm.DBConnection;

    let stmt = db.createStatement(
      "INSERT INTO moz_downloads (name, source, target, startTime, endTime, state, referrer) " +
      "VALUES (:name, :source, :target, :startTime, :endTime, :state, :referrer)"
    );

    let current = Browser.selectedBrowser.currentURI.spec;
    stmt.params.name = picker.file.leafName;
    stmt.params.source = current;
    stmt.params.target = Services.io.newFileURI(picker.file).spec;
    stmt.params.startTime = Date.now() * 1000;
    stmt.params.endTime = Date.now() * 1000;
    stmt.params.state = Ci.nsIDownloadManager.DOWNLOAD_NOTSTARTED;
    stmt.params.referrer = current;
    stmt.execute();
    stmt.finalize();

    let newItemId = db.lastInsertRowID;
    let download = dm.getDownload(newItemId);
    try {
      DownloadsView.downloadStarted(download);
    }
    catch(e) {}
    Services.obs.notifyObservers(download, "dl-start", null);

    let data = {
      type: Ci.nsIPrintSettings.kOutputFormatPDF,
      id: newItemId,
      referrer: current,
      filePath: picker.file.path
    };

    Browser.selectedBrowser.messageManager.sendAsyncMessage("Browser:SaveAs", data);
  },

  updatePageSaveAs: function updatePageSaveAs() {
    this.removeItems("saveas");

    // Check for local XUL content
    let contentDocument = Browser.selectedBrowser.contentDocument;
    if (contentDocument && contentDocument instanceof XULDocument)
      return;

    let strings = Elements.browserBundle;
    let node = this.appendItem("saveas", strings.getString("pageactions.saveas.pdf"), "");
    node.onclick = function(event) {
      PageActions._savePageAsPDF();
    }
  },

  appendItem: function appendItem(aType, aTitle, aDesc) {
    let container = document.getElementById("pageactions-container");
    let item = document.createElement("pageaction");
    item.setAttribute("title", aTitle);
    item.setAttribute("description", aDesc);
    item.setAttribute("type", aType);
    container.appendChild(item);

    let identityContainer = document.getElementById("identity-container");
    identityContainer.setAttribute("hasmenu", "true");
    return item;
  },

  removeItem: function removeItem(aItem) {
    let container = document.getElementById("pageactions-container");
    container.removeChild(aItem);

    let identityContainer = document.getElementById("identity-container");
    identityContainer.setAttribute("hasmenu", container.hasChildNodes() ? "true" : "false");
  },

  removeItems: function removeItems(aType) {
    let container = document.getElementById("pageactions-container");
    let count = container.childNodes.length;
    for (let i = count - 1; i >= 0; i--) {
      if (aType == "" || container.childNodes[i].getAttribute("type") == aType)
        this.removeItem(container.childNodes[i]);
    }
  }
}

var NewTabPopup = {
  _timeout: 0,
  _tabs: [],

  get box() {
    delete this.box;
    return this.box = document.getElementById("newtab-popup");
  },

  _updateLabel: function() {
    let newtabStrings = Elements.browserBundle.getString("newtabpopup.opened");
    let label = PluralForm.get(this._tabs.length, newtabStrings).replace("#1", this._tabs.length);

    this.box.firstChild.setAttribute("value", label);
  },

  hide: function() {
    if (this._timeout) {
      clearTimeout(this._timeout);
      this._timeout = 0;
    }

    this._tabs = [];
    this.box.hidden = true;
    BrowserUI.popPopup();
  },

  show: function(aTab) {
    BrowserUI.pushPopup(this, this.box);

    this._tabs.push(aTab);
    this._updateLabel();

    this.box.top = aTab.getBoundingClientRect().top + (aTab.getBoundingClientRect().height / 3);
    this.box.hidden = false;

    if (this._timeout)
      clearTimeout(this._timeout);

    this._timeout = setTimeout(function(self) {
      self.hide();
    }, 2000, this);
  },

  selectTab: function() {
    BrowserUI.selectTab(this._tabs.pop());
    this.hide();
  }
}

var BookmarkPopup = {
  get box() {
    delete this.box;
    return this.box = document.getElementById("bookmark-popup");
  },

  _bookmarkPopupTimeout: -1,

  hide : function hide() {
    if (this._bookmarkPopupTimeout != -1) {
      clearTimeout(this._bookmarkPopupTimeout);
      this._bookmarkPopupTimeout = -1;
    }
    this.box.hidden = true;
    BrowserUI.popPopup();
  },

  show : function show(aAutoClose) {
    const margin = 10;

    this.box.hidden = false;

    let [,,,controlsW] = Browser.computeSidebarVisibility();
    this.box.left = window.innerWidth - (this.box.getBoundingClientRect().width + controlsW + margin);
    this.box.top  = BrowserUI.starButton.getBoundingClientRect().top + margin;

    if (aAutoClose) {
      this._bookmarkPopupTimeout = setTimeout(function (self) {
        self._bookmarkPopupTimeout = -1;
        self.hide();
      }, 2000, this);
    }

    // include starButton here, so that click-to-dismiss works as expected
    BrowserUI.pushPopup(this, [this.box, BrowserUI.starButton]);
  },

  toggle : function toggle(aAutoClose) {
    if (this.box.hidden)
      this.show(aAutoClose);
    else
      this.hide();
  }
}

var BookmarkHelper = {
  _panel: null,
  _editor: null,

  edit: function BH_edit(aURI) {
    if (!aURI)
      aURI = getBrowser().currentURI;

    let itemId = PlacesUtils.getMostRecentBookmarkForURI(aURI);
    if (itemId == -1)
      return;

    let title = PlacesUtils.bookmarks.getItemTitle(itemId);
    let tags = PlacesUtils.tagging.getTagsForURI(aURI, {});

    const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    this._editor = document.createElementNS(XULNS, "placeitem");
    this._editor.setAttribute("id", "bookmark-item");
    this._editor.setAttribute("flex", "1");
    this._editor.setAttribute("type", "bookmark");
    this._editor.setAttribute("ui", "manage");
    this._editor.setAttribute("title", title);
    this._editor.setAttribute("uri", aURI.spec);
    this._editor.setAttribute("itemid", itemId);
    this._editor.setAttribute("tags", tags.join(", "));
    this._editor.setAttribute("onclose", "BookmarkHelper.hide()");
    document.getElementById("bookmark-form").appendChild(this._editor);

    let toolbar = document.getElementById("toolbar-main");
    let top = toolbar.top + toolbar.boxObject.height;

    this._panel = document.getElementById("bookmark-container");
    this._panel.top = (top < 0 ? 0 : top);
    this._panel.hidden = false;
    BrowserUI.pushPopup(this, this._panel);

    let self = this;
    Browser.forceChromeReflow();
    self._editor.startEditing();
  },

  save: function BH_save() {
    this._editor.stopEditing(true);
  },

  hide: function BH_hide() {
    BrowserUI.updateStar();

    // Note: the _editor will have already saved the data, if needed, by the time
    // this method is called, since this method is called via the "close" event.
    this._editor.parentNode.removeChild(this._editor);
    this._editor = null;

    this._panel.hidden = true;
    BrowserUI.popPopup();
  },
};

var BookmarkList = {
  _panel: null,
  _bookmarks: null,
  _manageButtton: null,

  get mobileRoot() {
    let items = PlacesUtils.annotations.getItemsWithAnnotation("mobile/bookmarksRoot", {});
    if (!items.length)
      throw "Couldn't find mobile bookmarks root!";

    delete this.mobileRoot;
    return this.mobileRoot = items[0];
  },

  show: function() {
    this._panel = document.getElementById("bookmarklist-container");
    this._panel.width = window.innerWidth;
    this._panel.height = window.innerHeight;
    this._panel.hidden = false;
    BrowserUI.pushDialog(this);

    this._bookmarks = document.getElementById("bookmark-items");
    this._bookmarks.addEventListener("BookmarkRemove", this, true);
    this._bookmarks.manageUI = false;
    this._bookmarks.openFolder();

    this._manageButton = document.getElementById("tool-bookmarks-manage");
    this._manageButton.disabled = (this._bookmarks.items.length == 0);
  },

  close: function() {
    BrowserUI.updateStar();

    if (this._bookmarks.manageUI)
      this.toggleManage();
    this._bookmarks.blur();
    this._bookmarks.removeEventListener("BookmarkRemove", this, true);

    this._panel.hidden = true;
    BrowserUI.popDialog();
  },

  toggleManage: function() {
    this._bookmarks.manageUI = !(this._bookmarks.manageUI);
    this._manageButton.checked = this._bookmarks.manageUI;
  },

  openBookmark: function() {
    let item = this._bookmarks.activeItem;
    if (item.spec) {
      this.close();
      BrowserUI.goToURI(item.spec);
    }
  },

  handleEvent: function(aEvent) {
    if (aEvent.type == "BookmarkRemove") {
      if (this._bookmarks.isRootFolder && this._bookmarks.items.length == 1) {
        this._manageButton.disabled = true;
        this.toggleManage();
      }
    }
  }
};


var FindHelperUI = {
  type: "find",
  commands: {
    next: "cmd_findNext",
    previous: "cmd_findPrevious",
    close: "cmd_findClose"
  },

  init: function findHelperInit() {
    this._textbox = document.getElementById("find-helper-textbox");
    this._container = document.getElementById("content-navigator");

    this._cmdPrevious = document.getElementById(this.commands.previous);
    this._cmdNext = document.getElementById(this.commands.next);

    // Listen for form assistant messages from content
    messageManager.addMessageListener("FindAssist:Show", this);

    // Listen for events where form assistant should be closed
    document.getElementById("tabs").addEventListener("TabSelect", this, true);
    document.getElementById("browsers").addEventListener("URLChanged", this, true);
  },

  receiveMessage: function findHelperReceiveMessage(aMessage) {
    let json = aMessage.json;
    switch(aMessage.name) {
      case "FindAssist:Show":
        if (json.rect)
          this._zoom(Rect.fromRect(json.rect));
        break;
    }
  },

  handleEvent: function findHelperHandleEvent(aEvent) {
    if (aEvent.type == "TabSelect" || aEvent.type == "URLChanged")
      this.hide();
  },

  show: function findHelperShow() {
    Browser._browserView.ignorePageScroll(true);
    this._container.show(this);
    this.search("");
    this._textbox.focus();
  },

  hide: function findHelperHide() {
    Browser._browserView.ignorePageScroll(false);
    this._textbox.value = "";
    this._container.hide(this);
  },

  goToPrevious: function findHelperGoToPrevious() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Previous", { });
  },

  goToNext: function findHelperGoToNext() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Next", { });
  },

  search: function findHelperSearch(aValue) {
    this.updateCommands(aValue);
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FindAssist:Find", { searchString: aValue });
  },

  updateCommands: function findHelperUpdateCommands(aValue) {
    this._cmdPrevious.setAttribute("disabled", aValue == "");
    this._cmdNext.setAttribute("disabled", aValue == "");
  },

  updateFindInPage: function findHelperUpdateFindInPage() {
    PageActions.removeItems("findinpage");
    let title = Elements.browserBundle.getString("pageactions.findInPage");
    let node = PageActions.appendItem("findinpage", title, "");
    node.onclick = function(event) {
      BrowserUI._hidePopup();
      FindHelperUI.show();
    }
  },

  _zoom: function _findHelperZoom(aElementRect) {
    let bv = Browser._browserView;
    let zoomRect = bv.getVisibleRect();

    // Zoom to a specified Rect
    if (aElementRect && bv.allowZoom && Services.prefs.getBoolPref("findhelper.autozoom")) {
      let zoomLevel = Browser._getZoomLevelForRect(bv.browserToViewportRect(aElementRect.clone()));
      zoomLevel = Math.min(Math.max(kBrowserFormZoomLevelMin, zoomLevel), kBrowserFormZoomLevelMax);

      zoomRect = Browser._getZoomRectForPoint(aElementRect.center().x, aElementRect.y, zoomLevel);
      Browser.animatedZoomTo(zoomRect);
    }
  }
};

/**
 * Responsible for navigating forms and filling in information.
 *  - Navigating forms is handled by next and previous commands.
 *  - When an element is focused, the browser view zooms in to the control.
 *  - The caret positionning and the view are sync to keep the type
 *    in text into view for input fields (text/textarea).
 *  - Provides autocomplete box for input fields.
 */
var FormHelperUI = {
  type: "form",
  commands: {
    next: "cmd_formNext",
    previous: "cmd_formPrevious",
    close: "cmd_formClose"
  },

  init: function formHelperInit() {
    this._container = document.getElementById("content-navigator");
    this._autofillContainer = document.getElementById("form-helper-autofill");
    this._cmdPrevious = document.getElementById(this.commands.previous);
    this._cmdNext = document.getElementById(this.commands.next);

    // Listen for form assistant messages from content
    messageManager.addMessageListener("FormAssist:Show", this);
    messageManager.addMessageListener("FormAssist:Hide", this);
    messageManager.addMessageListener("FormAssist:Update", this);
    messageManager.addMessageListener("FormAssist:AutoComplete", this);

    // Listen for events where form assistant should be closed
    document.getElementById("tabs").addEventListener("TabSelect", this, true);
    document.getElementById("browsers").addEventListener("URLChanged", this, true);
  },

  show: function formHelperShow(aElement, aHasPrevious, aHasNext) {
    this._open = true;

    // Update the next/previous commands
    this._cmdPrevious.setAttribute("disabled", !aHasPrevious);
    this._cmdNext.setAttribute("disabled", !aHasNext);

    let lastElement = this._currentElement || null;
    this._currentElement = {
      name: aElement.name,
      value: aElement.value,
      maxLength: aElement.maxLength,
      isAutocomplete: aElement.isAutocomplete,
      list: aElement.choices
    }
    this._updateContainer(lastElement, this._currentElement);

    this._zoom(Rect.fromRect(aElement.rect), Rect.fromRect(aElement.caretRect));
  },

  hide: function formHelperHide() {
    if (!this._open)
      return;

    this._updateContainerForSelect(this._currentElement, null);
    this._open = false;
  },

  handleEvent: function formHelperHandleEvent(aEvent) {
    if (aEvent.type == "TabSelect" || aEvent.type == "URLChanged")
      this.hide();
  },

  receiveMessage: function formHelperReceiveMessage(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "FormAssist:Show":
        // if the user has manually disabled the Form Assistant UI we still
        // want to show a UI for <select /> element but not managed by
        // FormHelperUI
        let enabled = Services.prefs.getBoolPref("formhelper.enabled");
        if (enabled) {
          this.show(json.current, json.hasPrevious, json.hasNext);
        }
        else {
          SelectHelperUI.show(json.current.list);
        }
        break;

      case "FormAssist:Hide":
        this.hide();
        break;

      case "FormAssist:AutoComplete":
        this._updateAutocompleteFor(json.current);
        this._container.contentHasChanged();
        break;

      case "FormAssist:Update":
        this._zoom(null, Rect.fromRect(json.caretRect));
        break;
    }
  },

  goToPrevious: function formHelperGoToPrevious() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:Previous", { });
  },

  goToNext: function formHelperGoToNext() {
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:Next", { });
  },

  doAutoComplete: function formHelperDoAutoComplete(aElement) {
    // Suggestions are only in <label>s. Ignore the rest.
    if (aElement instanceof Ci.nsIDOMXULLabelElement)
      Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:AutoComplete", { value: aElement.value });
  },

  get _open() {
    return (this._container.getAttribute("type") == this.type);
  },

  set _open(aVal) {
    if (aVal == this._open)
      return;

    let bv = Browser._browserView;
    bv.ignorePageScroll(aVal);
    this._container.hidden = !aVal;

    if (aVal) {
      this._zoomStart();
      this._container.show(this);
    } else {
      this._zoomFinish();
      this._currentElement = null;
      this._container.hide(this);
    }

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("FormUI", true, true, window, aVal);
    this._container.dispatchEvent(evt);
  },

  _updateAutocompleteFor: function _formHelperUpdateAutocompleteFor(aElement) {
    let suggestions = this._getAutocompleteSuggestions(aElement);
    this._displaySuggestions(suggestions);
  },

  _displaySuggestions: function _formHelperDisplaySuggestions(aSuggestions) {
    let autofill = this._autofillContainer;
    while (autofill.hasChildNodes())
      autofill.removeChild(autofill.lastChild);

    let fragment = document.createDocumentFragment();
    for (let i = 0; i < aSuggestions.length; i++) {
      let value = aSuggestions[i];
      let button = document.createElement("label");
      button.setAttribute("value", value);
      fragment.appendChild(button);
    }
    autofill.appendChild(fragment);
    autofill.collapsed = !aSuggestions.length;
  },

  /** Retrieve the autocomplete list from the autocomplete service for an element */
  _getAutocompleteSuggestions: function _formHelperGetAutocompleteSuggestions(aElement) {
    if (!aElement.isAutocomplete)
      return [];

    let suggestions = [];

    let autocompleteService = Cc["@mozilla.org/satchel/form-autocomplete;1"].getService(Ci.nsIFormAutoComplete);
    let results = autocompleteService.autoCompleteSearch(aElement.name, aElement.value, aElement, null);
    if (results.matchCount > 0) {
      for (let i = 0; i < results.matchCount; i++) {
        let value = results.getValueAt(i);
        suggestions.push(value);
      }
    }

    return suggestions;
  },

  /** Update the form helper container to reflect new element user is editing. */
  _updateContainer: function _formHelperUpdateContainer(aLastElement, aCurrentElement) {
    this._updateContainerForSelect(aLastElement, aCurrentElement);

    // Setup autofill UI
    this._updateAutocompleteFor(aCurrentElement);
    this._container.contentHasChanged();
  },

  /** Helper for _updateContainer that handles the case where the new element is a select. */
  _updateContainerForSelect: function _formHelperUpdateContainerForSelect(aLastElement, aCurrentElement) {
    let lastHasChoices = aLastElement && (aLastElement.list != null);
    let currentHasChoices = aCurrentElement && (aCurrentElement.list != null);

    if (!lastHasChoices && currentHasChoices) {
      SelectHelperUI.dock(this._container);
      SelectHelperUI.show(aCurrentElement.list);
    } else if (lastHasChoices && currentHasChoices) {
      SelectHelperUI.reset();
      SelectHelperUI.show(aCurrentElement.list);
    } else if (lastHasChoices && !currentHasChoices) {
      SelectHelperUI.hide();
    }
  },

  /** Zoom and move viewport so that element is legible and touchable. */
  _zoom: function _formHelperZoom(aElementRect, aCaretRect) {
    let bv = Browser._browserView;
    let zoomRect = bv.getVisibleRect();

    // Zoom to a specified Rect
    if (aElementRect && bv.allowZoom && Services.prefs.getBoolPref("formhelper.autozoom")) {
      // Zoom to an element by keeping the caret into view
      let zoomLevel = Browser._getZoomLevelForRect(bv.browserToViewportRect(aElementRect.clone()));
      zoomLevel = Math.min(Math.max(kBrowserFormZoomLevelMin, zoomLevel), kBrowserFormZoomLevelMax);

      zoomRect = Browser._getZoomRectForPoint(aElementRect.center().x, aElementRect.y, zoomLevel);
      Browser.animatedZoomTo(zoomRect);
    }

    // Move the view to show the caret if needed
    if (aCaretRect) {
      let caretRect = bv.browserToViewportRect(aCaretRect);
      if (zoomRect.contains(caretRect))
        return;

      let [deltaX, deltaY] = this._getOffsetForCaret(caretRect, zoomRect);
      if (deltaX != 0 || deltaY != 0) {
        Browser.contentScrollboxScroller.scrollBy(deltaX, deltaY);
        bv.onAfterVisibleMove();
      }

      Browser.animatedZoomTo(zoomRect);
    }
  },

  /* Store the current zoom level, and scroll positions to restore them if needed */
  _zoomStart: function _formHelperZoomStart() {
    if (!Services.prefs.getBoolPref("formhelper.restore"))
      return;

    this._restore = {
      zoom: Browser._browserView.getZoomLevel(),
      contentScrollOffset: Browser.getScrollboxPosition(Browser.contentScrollboxScroller),
      pageScrollOffset: Browser.getScrollboxPosition(Browser.pageScrollboxScroller)
    };
  },

  /** Element is no longer selected. Restore zoom level if setting is enabled. */
  _zoomFinish: function _formHelperZoomFinish() {
    if(!Services.prefs.getBoolPref("formhelper.restore"))
      return;

    let restore = this._restore;
    Browser._browserView.setZoomLevel(restore.zoom);
    Browser.contentScrollboxScroller.scrollTo(restore.contentScrollOffset.x, restore.contentScrollOffset.y);
    Browser.pageScrollboxScroller.scrollTo(restore.pageScrollOffset.x, restore.pageScrollOffset.y);
  },

  _getOffsetForCaret: function _formHelperGetOffsetForCaret(aCaretRect, aRect) {
    // Determine if we need to move left or right to bring the caret into view
    let deltaX = 0;
    if (aCaretRect.right > aRect.right)
      deltaX = aCaretRect.right - aRect.right;
    if (aCaretRect.left < aRect.left)
      deltaX = aCaretRect.left - aRect.left;

    // Determine if we need to move up or down to bring the caret into view
    let deltaY = 0;
    if (aCaretRect.bottom > aRect.bottom)
      deltaY = aCaretRect.bottom - aRect.bottom;
    if (aCaretRect.top < aRect.top)
      deltaY = aCaretRect.top - aRect.top;

    return [deltaX, deltaY];
  }
};


/**
 * SelectHelperUI: Provides an interface for making a choice in a list.
 *   Supports simultaneous selection of choices and group headers.
 */
var SelectHelperUI = {
  _list: null,
  _selectedIndexes: null,

  get _panel() {
    delete this._panel;
    return this._panel = document.getElementById("select-container");
  },

  show: function(aList) {
    this._list = aList;

    this._container = document.getElementById("select-list");
    this._container.setAttribute("multiple", aList.multiple ? "true" : "false");

    this._selectedIndexes = this._getSelectedIndexes();
    let firstSelected = null;

    let choices = aList.choices;
    for (let i = 0; i < choices.length; i++) {
      let choice = choices[i];
      let item = document.createElement("option");
      item.setAttribute("label", choice.text);
      choice.disabled ? item.setAttribute("disabled", choice.disabled)
                      : item.removeAttribute("disabled");
      this._container.appendChild(item);

      if (choice.group) {
        item.className = "optgroup";
        continue;
      }

      item.optionIndex = choice.optionIndex;
      item.choiceIndex = i;

      if (choice.inGroup)
        item.className = "in-optgroup";

      if (choice.selected) {
        item.setAttribute("selected", "true");
        firstSelected = firstSelected || item;
      }
    }

    this._panel.hidden = false;

    if (!this._docked)
      BrowserUI.pushPopup(this, this._panel);

    this._scrollElementIntoView(firstSelected);

    this._container.addEventListener("click", this, false);
  },

  dock: function dock(aContainer) {
    aContainer.insertBefore(this._panel, aContainer.lastChild);
    this._panel.style.maxHeight = (window.innerHeight / 1.8) + "px";
    this._docked = true;
  },

  undock: function undock() {
    let rootNode = Elements.stack;
    rootNode.insertBefore(this._panel, rootNode.lastChild);
    this._panel.style.maxHeight = "";
    this._docked = false;
  },

  reset: function() {
    this._updateControl();
    let empty = this._container.cloneNode(false);
    this._container.parentNode.replaceChild(empty, this._container);
    this._container = empty;
    this._list = null;
    this._selectedIndexes = null;
  },

  hide: function() {
    this._container.removeEventListener("click", this, false);
    this._panel.hidden = true;

    if (this._docked)
      this.undock();
    else
      BrowserUI.popPopup();

    this.reset();
  },

  unselectAll: function() {
    let choices = this._list.choices;
    this._forEachOption(function(aItem, aIndex) {
      aItem.selected = false;
      choices[aIndex].selected = false;
    });
  },

  selectByIndex: function(aIndex) {
    let choices = this._list.choices;
    for (let i = 0; i < this._container.childNodes.length; i++) {
      let option = this._container.childNodes[i];
      if (option.optionIndex == aIndex) {
        option.selected = true;
        this._choices[i].selected = true;
        this._scrollElementIntoView(option);
        break;
      }
    }
  },

  _getSelectedIndexes: function() {
    let indexes = [];
    let choices = this._list.choices;
    let choiceLength = choices.length;
    for (let i = 0; i < choiceLength; i++) {
      let choice = choices[i];
      if (choice.selected)
        indexes.push(choice.optionIndex);
    }
    return indexes;
  },

  _scrollElementIntoView: function(aElement) {
    if (!aElement)
      return;

    let index = -1;
    this._forEachOption(
      function(aItem, aIndex) {
        if (aElement.optionIndex == aItem.optionIndex)
          index = aIndex;
      }
    );

    if (index == -1)
      return;

    let scrollBoxObject = this._container.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    let itemHeight = aElement.getBoundingClientRect().height;
    let visibleItemsCount = this._container.boxObject.height / itemHeight;
    if ((index + 1) > visibleItemsCount) {
      let delta = Math.ceil(visibleItemsCount / 2);
      scrollBoxObject.scrollTo(0, ((index + 1) - delta) * itemHeight);
    }
    else {
      scrollBoxObject.scrollTo(0, 0);
    }
  },

  _forEachOption: function(aCallback) {
    let children = this._container.children;
    for (let i = 0; i < children.length; i++) {
      let item = children[i];
      if (!item.hasOwnProperty("optionIndex"))
        continue;
      aCallback(item, i);
    }
  },

  _updateControl: function() {
    let currentSelectedIndexes = this._getSelectedIndexes();

    let isIdentical = (this._selectedIndexes && this._selectedIndexes.length == currentSelectedIndexes.length);
    if (isIdentical) {
      for (let i = 0; i < currentSelectedIndexes.length; i++) {
        if (currentSelectedIndexes[i] != this._selectedIndexes[i]) {
          isIdentical = false;
          break;
        }
      }
    }

    if (isIdentical)
      return;

    this._list.changeCallback ? this._list.changeCallback()
                              : Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceChange", { });
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "click":
        let item = aEvent.target;
        if (item && item.hasOwnProperty("optionIndex")) {
          if (this._list.multiple) {
            // Toggle the item state
            item.selected = !item.selected;
          }
          else {
            this.unselectAll();

            // Select the new one and update the control
            item.selected = true;
          }

          this.onSelect(item.optionIndex, item.selected, !this._list.multiple);
        }
        break;
    }
  },

  onSelect: function(aIndex, aSelected, aClearAll) {
    if (this._list.selectCallback) {
      this._list.selectCallback(aIndex);
      return;
    }

    let json = {
      index: aIndex,
      selected: aSelected,
      clearAll: aClearAll
    };
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceSelect", json);
  }
};

const kXLinkNamespace = "http://www.w3.org/1999/xlink";

var ContextHelper = {
  popupState: null,

  receiveMessage: function ch_receiveMessage(aMessage) {
    this.popupState = aMessage.json;
    this.popupState.browser = aMessage.target;

    let first = null;
    let last = null;
    let commands = document.getElementById("context-commands");
    for (let i=0; i<commands.childElementCount; i++) {
      let command = commands.children[i];
      let types = command.getAttribute("type").split(/\s+/);
      command.removeAttribute("selector");
      if (types.indexOf("image") != -1 && this.popupState.onImage) {
        first = (first ? first : command);
        last = command;
        command.hidden = false;
        continue;
      } else if (types.indexOf("image-loaded") != -1 && this.popupState.onLoadedImage) {
        first = (first ? first : command);
        last = command;
        command.hidden = false;
        continue;
      } else if (types.indexOf("link") != -1 && this.popupState.onSaveableLink) {
        first = (first ? first : command);
        last = command;
        command.hidden = false;
        continue;
      } else if (types.indexOf("callto") != -1 && this.popupState.onVoiceLink) {
        first = (first ? first : command);
        last = command;
        command.hidden = false;
        continue;
      } else if (types.indexOf("mailto") != -1 && this.popupState.onLink && this.popupState.linkProtocol == "mailto") {
        first = (first ? first : command);
        last = command;
        command.hidden = false;
        continue;
      }
      command.hidden = true;
    }

    if (!first) {
      this.popupState = null;
      return;
    }
    
    first.setAttribute("selector", "first-child");
    last.setAttribute("selector", "last-child");

    let label = document.getElementById("context-hint");
    if (this.popupState.onImage)
      label.value = this.popupState.mediaURL;
    if (this.popupState.onLink)
      label.value = this.popupState.linkURL;

    let container = document.getElementById("context-popup");
    container.hidden = false;

    // Make sure the container is at least sized to the content
    let preferredHeight = 0;
    for (let i=0; i<container.childElementCount; i++) {
      preferredHeight += container.children[i].getBoundingClientRect().height;
    }

    let rect = container.getBoundingClientRect();
    let height = Math.min(preferredHeight, 0.75 * window.innerWidth);
    let width = Math.min(rect.width, 0.75 * window.innerWidth);

    container.height = height;
    container.width = width;
    container.top = (window.innerHeight - height) / 2;
    container.left = (window.innerWidth - width) / 2;

    BrowserUI.pushPopup(this, [container]);
  },

  hide: function ch_hide() {
    this.popupState = null;

    let container = document.getElementById("context-popup");
    container.hidden = true;

    BrowserUI.popPopup();
  }
};

var ContextCommands = {
  openInNewTab: function cc_openInNewTab(aEvent) {
    Browser.addTab(ContextHelper.popupState.linkURL, false, Browser.selectedTab);
  },

  saveImage: function cc_saveImage(aEvent) {
    let browser = ContextHelper.popupState.browser;
    saveImageURL(ContextHelper.popupState.mediaURL, null, "SaveImageTitle", false, false, browser.documentURI);
  }
}

function removeBookmarksForURI(aURI) {
  //XXX blargle xpconnect! might not matter, but a method on
  // nsINavBookmarksService that takes an array of items to
  // delete would be faster. better yet, a method that takes a URI!
  let itemIds = PlacesUtils.getBookmarksForURI(aURI);
  itemIds.forEach(PlacesUtils.bookmarks.removeItem);

  BrowserUI.updateStar();
}
