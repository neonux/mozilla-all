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

// ##########
// Class: Page
// This singleton represents the whole 'New Tab Page' and takes care of
// initializing all its components.
let Page = {
  // ----------
  // Function: init
  // Initializes the page.
  //
  // Parameters:
  //   aToolbarSelector - the query selector for the page toolbar
  //   aGridSelector - the query selector for the grid
  init: function Page_init(aToolbarSelector, aGridSelector) {
    Toolbar.init(aToolbarSelector);
    this._gridSelector = aGridSelector;

    // check if the new tab feature is enabled
    if (this.isEnabled())
      this._init();
    else
      document.body.classList.add("disabled");

    // register pref observer
    Services.prefs.addObserver(PREF_NEWTAB_ENABLED, this, true);
  },

  // ----------
  // Function: isEnabled
  // Returns whether the 'New Tage Page' is enabled.
  isEnabled: function Page_isEnabled() {
    return Services.prefs.getBoolPref(PREF_NEWTAB_ENABLED, true);
  },

  // ----------
  // Function: update
  // Updates the whole page and the grid when the storage has changed.
  update: function Page_update() {
    this.checkIfModified();
    Grid.refresh();
  },

  // ----------
  // Function: checkIfModified
  // Checks if the page is modified and sets the CSS class accordingly
  checkIfModified: function Page_checkIfModified() {
    let classes = document.body.classList;

    // the page is considered modified only if sites have been removed
    if (BlockedLinks.isEmpty())
      classes.remove("modified");
    else
      classes.add("modified");
  },

  // ----------
  // Function: observe
  // This function is called by the pref observer and notifies every opened
  // page that the 'disabled' status has been toggled.
  observe: function Page_observe() {
    let classes = document.body.classList;

    if (this.isEnabled()) {
      classes.remove("disabled");
      this._init();
    } else {
      classes.add("disabled");
    }
  },

  // ----------
  // Function: _init
  // Internally initializes the page. This is run only when/if the feature
  // is/gets enabled.
  _init: function Page__init() {
    if (this._initialized)
      return;

    this._initialized = true;

    // add ourselves to the list of pages to receive update notifications
    Pages.register(this);

    let self = this;

    // listen for 'unload' to unregister this page
    function unload() NewTabUtils.Pages.unregister(self);
    addEventListener("unload", unload, false);

    // check if the grid is modified
    this.checkIfModified();

    // initialize and render the grid
    Grid.init(this._gridSelector);

    // initialize the drop target shim
    DropTargetShim.init();

    // workaround to prevent a delay on MacOSX due to a slow drop animation
    let doc = document.documentElement;
    doc.addEventListener("dragover", this._dragover, false);
    doc.addEventListener("drop", this._drop, false);
  },

  _dragover: function Page__dragover(aEvent) {
    aEvent.preventDefault();
  },

  _drop: function Page__drop(aEvent) {
    aEvent.preventDefault();
    aEvent.stopPropagation();
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};
