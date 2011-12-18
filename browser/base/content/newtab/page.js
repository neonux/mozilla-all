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
 * The Original Code is New Tab Page code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Taubert <ttaubert@mozilla.com> (Original Author)
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
 * This singleton represents the whole 'New Tab Page' and takes care of
 * initializing all its components.
 */
let gPage = {
  /**
   * Cached value that tells whether the New Tab Page feature is enabled.
   */
  _enabled: null,

  /**
   * Initializes the page.
   * @param aToolbarSelector The query selector for the page toolbar.
   * @param aGridSelector The query selector for the grid.
   */
  init: function Page_init(aToolbarSelector, aGridSelector) {
    gToolbar.init(aToolbarSelector);
    this._gridSelector = aGridSelector;

    // Check if the new tab feature is enabled.
    if (this.isEnabled())
      this._init();
    else
      this.setDisabled(true);
  },

  /**
   * Checks whether the 'New Tab Page' is enabled.
   * @return Whether this feature is enabled.
   */
  isEnabled: function Page_isEnabled() {
    if (this._enabled === null)
      this._enabled = Services.prefs.getBoolPref(PREF_NEWTAB_ENABLED, true);

    return this._enabled;
  },

  /**
   * Updates the whole page and the grid when the storage has changed.
   */
  update: function Page_update() {
    this.checkIfModified();
    gGrid.refresh();
  },

  /**
   * Checks if the page is modified and sets the CSS class accordingly
   */
  checkIfModified: function Page_checkIfModified() {
    // The page is considered modified only if sites have been removed.
    this._setModified(!gBlockedLinks.isEmpty());
  },

  /**
   * Sets the disabled status of the New Tab Page.
   * @param aValue Whether the page is disabled or not.
   */
  setDisabled: function Page_setDisabled(aValue) {
    let nodes = [gGrid.node];
    let buttons = document.querySelectorAll(".toolbar-button");
    for (let i = 0; i < buttons.length; i++)
      nodes.push(buttons[i]);

    // Set the nodes' states.
    nodes.forEach(function (aNode) {
      if (aNode) {
        if (aValue)
          aNode.setAttribute("page-disabled", "true");
        else
          aNode.removeAttribute("page-disabled");
      }
    }, this);

    // Update the cached value.
    this._enabled = !aValue;

    // Initialize the whole page if we haven't done that, yet.
    if (aValue)
      this._init();
  },

  /**
   * Internally initializes the page. This runs only when/if the feature
   * is/gets enabled.
   */
  _init: function Page_init() {
    if (this._initialized)
      return;

    this._initialized = true;

    // Add ourselves to the list of pages to receive update notifications.
    gAllPages.register(this);

    let self = this;

    // Listen for 'unload' to unregister this page.
    function unload() gAllPages.unregister(self);
    addEventListener("unload", unload, false);

    // Check if the grid is modified.
    this.checkIfModified();

    // Initialize and render the grid.
    gGrid.init(this._gridSelector);

    // Initialize the drop target shim.
    gDropTargetShim.init();

    // Workaround to prevent a delay on MacOSX due to a slow drop animation.
    let doc = document.documentElement;
    doc.addEventListener("dragover", this._onDragOver, false);
    doc.addEventListener("drop", this._onDrop, false);
  },

  /**
   * Sets the modified status of the New Tab Page.
   * @param aValue Whether the page is modified or not.
   */
  _setModified: function Page_setModified(aValue) {
    let node = document.getElementById("toolbar-button-reset");

    if (aValue)
      node.setAttribute("modified", "true");
    else
      node.removeAttribute("modified");
  },

  /**
   * Handles the 'dragover' event. Workaround to prevent a delay on MacOSX
   * due to a slow drop animation.
   * @param aEvent The 'dragover' event.
   */
  _onDragOver: function Page_onDragOver(aEvent) {
    aEvent.preventDefault();
  },

  /**
   * Handles the 'drop' event. Workaround to prevent a delay on MacOSX due to
   * a slow drop animation.
   * @param aEvent The 'drop' event.
   */
  _onDrop: function Page_onDrop(aEvent) {
    aEvent.preventDefault();
    aEvent.stopPropagation();
  }
};
