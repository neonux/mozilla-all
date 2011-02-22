# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Tab View
#
# The Initial Developer of the Original Code is Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Raymond Lee <raymond@appcoast.com>
#   Ian Gilman <ian@iangilman.com>
#   Tim Taubert <tim.taubert@gmx.de>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

let TabView = {
  _deck: null,
  _window: null,
  _firstUseExperienced: false,
  _browserKeyHandlerInitialized: false,
  VISIBILITY_IDENTIFIER: "tabview-visibility",

  // ----------
  get windowTitle() {
    delete this.windowTitle;
    let brandBundle = document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");
    let title = gNavigatorBundle.getFormattedString("tabView2.title", [brandShortName]);
    return this.windowTitle = title;
  },

  // ----------
  get firstUseExperienced() {
    return this._firstUseExperienced;
  },

  // ----------
  set firstUseExperienced(val) {
    if (val != this._firstUseExperienced)
      Services.prefs.setBoolPref("browser.panorama.experienced_first_run", val);
  },

  // ----------
  init: function TabView_init() {
    if (!Services.prefs.prefHasUserValue("browser.panorama.experienced_first_run") ||
        !Services.prefs.getBoolPref("browser.panorama.experienced_first_run")) {
      Services.prefs.addObserver(
        "browser.panorama.experienced_first_run", this, false);
    } else {
      this._firstUseExperienced = true;

      if ((gBrowser.tabs.length - gBrowser.visibleTabs.length) > 0)
        this._setBrowserKeyHandlers();

      // ___ visibility
      let sessionstore =
        Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
      let data = sessionstore.getWindowValue(window, this.VISIBILITY_IDENTIFIER);

      if (data && data == "true") {
        this.show();
      } else {
        let self = this;

        // if a tab is changed from hidden to unhidden and the iframe is not 
        // initialized, load the iframe and setup the tab.
        this._tabShowEventListener = function (event) {
          if (!self._window)
            self._initFrame(function() {
              self._window.UI.onTabSelect(gBrowser.selectedTab);
            });
        };
        gBrowser.tabContainer.addEventListener(
          "TabShow", this._tabShowEventListener, true);
      }
    }
  },

  // ----------
  // Observes topic changes.
  observe: function TabView_observe(subject, topic, data) {
    if (topic == "nsPref:changed") {
      Services.prefs.removeObserver(
        "browser.panorama.experienced_first_run", this);
      this._firstUseExperienced = true;
      this._addToolbarButton();
    }
  },

  // ----------
  // Uninitializes TabView.
  uninit: function TabView_uninit() {
    if (!this._firstUseExperienced) {
      Services.prefs.removeObserver(
        "browser.panorama.experienced_first_run", this);
    }
    if (this._tabShowEventListener) {
      gBrowser.tabContainer.removeEventListener(
        "TabShow", this._tabShowEventListener, true);
    }
  },

  // ----------
  // Creates the frame and calls the callback once it's loaded. 
  // If the frame already exists, calls the callback immediately. 
  _initFrame: function TabView__initFrame(callback) {
    if (this._window) {
      if (typeof callback == "function")
        callback();
    } else {
      // ___ find the deck
      this._deck = document.getElementById("tab-view-deck");

      // ___ create the frame
      let iframe = document.createElement("iframe");
      iframe.id = "tab-view";
      iframe.setAttribute("transparent", "true");
      iframe.flex = 1;

      if (typeof callback == "function")
        window.addEventListener("tabviewframeinitialized", callback, false);

      iframe.setAttribute("src", "chrome://browser/content/tabview.html");
      this._deck.appendChild(iframe);
      this._window = iframe.contentWindow;

      if (this._tabShowEventListener) {
        gBrowser.tabContainer.removeEventListener(
          "TabShow", this._tabShowEventListener, true);
        this._tabShowEventListener = null;
      }

      this._setBrowserKeyHandlers();
    }
  },

  // ----------
  getContentWindow: function TabView_getContentWindow() {
    return this._window;
  },

  // ----------
  isVisible: function() {
    return (this._deck ? this._deck.selectedIndex == 1 : false);
  },

  // ----------
  show: function() {
    if (this.isVisible())
      return;
    
    this._initFrame(function() {
      let event = document.createEvent("Events");
      event.initEvent("tabviewshow", false, false);
      dispatchEvent(event);
    });
  },

  // ----------
  hide: function() {
    if (!this.isVisible())
      return;

    let event = document.createEvent("Events");
    event.initEvent("tabviewhide", false, false);
    dispatchEvent(event);
  },

  // ----------
  toggle: function() {
    if (this.isVisible())
      this.hide();
    else 
      this.show();
  },
  
  getActiveGroupName: function TabView_getActiveGroupName() {
    // We get the active group this way, instead of querying
    // GroupItems.getActiveGroupItem() because the tabSelect event
    // will not have happened by the time the browser tries to
    // update the title.
    let activeTab = window.gBrowser.selectedTab;
    if (activeTab._tabViewTabItem && activeTab._tabViewTabItem.parent){
      let groupName = activeTab._tabViewTabItem.parent.getTitle();
      if (groupName)
        return groupName;
    }
    return null;
  },  

  // ----------
  updateContextMenu: function(tab, popup) {
    let separator = document.getElementById("context_tabViewNamedGroups");
    let isEmpty = true;

    while (popup.firstChild && popup.firstChild != separator)
      popup.removeChild(popup.firstChild);

    let self = this;
    this._initFrame(function() {
      let activeGroup = tab._tabViewTabItem.parent;
      let groupItems = self._window.GroupItems.groupItems;

      groupItems.forEach(function(groupItem) {
        // if group has title, it's not hidden and there is no active group or
        // the active group id doesn't match the group id, a group menu item
        // would be added.
        if (groupItem.getTitle().length > 0 && !groupItem.hidden &&
            (!activeGroup || activeGroup.id != groupItem.id)) {
          let menuItem = self._createGroupMenuItem(groupItem);
          popup.insertBefore(menuItem, separator);
          isEmpty = false;
        }
      });
      separator.hidden = isEmpty;
    });
  },

  // ----------
  _createGroupMenuItem: function TabView__createGroupMenuItem(groupItem) {
    let menuItem = document.createElement("menuitem")
    menuItem.setAttribute("label", groupItem.getTitle());
    menuItem.setAttribute(
      "oncommand", 
      "TabView.moveTabTo(TabContextMenu.contextTab,'" + groupItem.id + "')");

    return menuItem;
  },

  // ----------
  moveTabTo: function TabView_moveTabTo(tab, groupItemId) {
    if (this._window) {
      this._window.GroupItems.moveTabToGroupItem(tab, groupItemId);
    } else {
      let self = this;
      this._initFrame(function() {
        self._window.GroupItems.moveTabToGroupItem(tab, groupItemId);
      });
    }
  },

  // ----------
  enableSearch: function TabView_enableSearch(event) {
    if (this._window)
      this._window.UI.enableSearch(event);
  },

  // ----------
  // Adds new key commands to the browser, for invoking the Tab Candy UI
  // and for switching between groups of tabs when outside of the Tab Candy UI.
  _setBrowserKeyHandlers: function TabView__setBrowserKeyHandlers() {
    if (this._browserKeyHandlerInitialized)
      return;

    this._browserKeyHandlerInitialized = true;

    let self = this;
    window.addEventListener("keypress", function(event) {
      if (self.isVisible() ||
          (gBrowser.tabs.length - gBrowser.visibleTabs.length) == 0)
        return;

      let charCode = event.charCode;
      // Control (+ Shift) + `
      if (event.ctrlKey && !event.metaKey && !event.altKey &&
          (charCode == 96 || charCode == 126)) {
        event.stopPropagation();
        event.preventDefault();

        self._initFrame(function() {
          let groupItems = self._window.GroupItems;
          let tabItem = groupItems.getNextGroupItemTab(event.shiftKey);
          if (!tabItem)
            return;

          // Switch to the new tab, and close the old group if it's now empty.
          let oldGroupItem = groupItems.getActiveGroupItem();
          window.gBrowser.selectedTab = tabItem.tab;
          oldGroupItem.closeIfEmpty();
        });
      }
    }, true);
  },

  // ----------
  // Prepares the tab view for undo close tab.
  prepareUndoCloseTab: function(blankTabToRemove) {
    if (this._window) {
      this._window.UI.restoredClosedTab = true;

      if (blankTabToRemove)
        blankTabToRemove._tabViewTabIsRemovedAfterRestore = true;
    }
  },

  // ----------
  // Cleans up the tab view after undo close tab.
  afterUndoCloseTab: function () {
    if (this._window)
      this._window.UI.restoredClosedTab = false;
  },

  // ----------
  // On move to group pop showing.
  moveToGroupPopupShowing: function TabView_moveToGroupPopupShowing(event) {
    // there are hidden tabs so initialize the iframe and update the context menu
    if ((gBrowser.tabs.length - gBrowser.visibleTabs.length) > 0)
      this.updateContextMenu(TabContextMenu.contextTab, event.target);
  },

  // ----------
  // Function: _addToolbarButton
  // Adds the TabView button to the TabsToolbar.
  _addToolbarButton: function TabView__addToolbarButton() {
    let buttonId = "tabview-button";

    if (document.getElementById(buttonId))
      return;

    let toolbar = document.getElementById("TabsToolbar");
    let currentSet = toolbar.currentSet.split(",");

    let alltabsPos = currentSet.indexOf("alltabs-button");
    if (-1 == alltabsPos)
      return;

    currentSet[alltabsPos] += "," + buttonId;
    currentSet = currentSet.join(",");
    toolbar.currentSet = currentSet;
    toolbar.setAttribute("currentset", currentSet);
    document.persist(toolbar.id, "currentset");
  }
};
