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
   * Initializes the page.
   * @param aToolbarSelector The query selector for the page toolbar.
   * @param aGridSelector The query selector for the grid.
   */
  init: function Page_init(aToolbarSelector, aGridSelector) {
    gToolbar.init(aToolbarSelector);
    this._gridSelector = aGridSelector;

    // Add ourselves to the list of pages to receive notifications.
    gAllPages.register(this);

    // Listen for 'unload' to unregister this page.
    function unload() gAllPages.unregister(self);
    addEventListener("unload", unload, false);

    // Check if the new tab feature is enabled.
    if (gAllPages.enabled)
      this._init();
    else
      this._updateAttributes(false);
  },

  /**
   * Listens for notifications specific to this page.
   */
  observe: function Page_observe() {
    let enabled = gAllPages.enabled;
    this._updateAttributes(enabled);

    // Initialize the whole page if we haven't done that, yet.
    if (enabled)
      this._init();
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
   * Internally initializes the page. This runs only when/if the feature
   * is/gets enabled.
   */
  _init: function Page_init() {
    if (this._initialized)
      return;

    this._initialized = true;

    let self = this;

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
   * Updates the 'page-disabled' attributes of the respective DOM nodes.
   * @param aValue Whether to set or remove attributes.
   */
  _updateAttributes: function Page_updateAttributes(aValue) {
    let nodes = document.querySelectorAll("#grid, #vbox, #toolbar, .toolbar-button");

    // Set the nodes' states.
    for (let i = 0; i < nodes.length; i++) {
      let node = nodes[i];
      if (aValue)
        node.removeAttribute("page-disabled");
      else
        node.setAttribute("page-disabled", "true");
    }
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
    if (gDrag.isValid(aEvent))
      aEvent.preventDefault();
  },

  /**
   * Handles the 'drop' event. Workaround to prevent a delay on MacOSX due to
   * a slow drop animation.
   * @param aEvent The 'drop' event.
   */
  _onDrop: function Page_onDrop(aEvent) {
    if (gDrag.isValid(aEvent)) {
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }
  }
};
