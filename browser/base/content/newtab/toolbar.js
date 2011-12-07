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

// ##########
// Class: Toolbar
// This singleton represents the page's toolbar that allows to enable/disable
// the 'New Tab Page' feature and to reset the whole page.
let Toolbar = {
  // ----------
  // Function: init
  // Initializes the toolbar.
  //
  // Parameters:
  //   aSelector - the query selector of the toolbar
  init: function Toolbar_init(aSelector) {
    this._node = document.querySelector(aSelector);

    // retrieve all buttons
    let btnShow = this._node.firstElementChild;
    let btnHide = btnShow.nextElementSibling;
    let btnReset = btnHide.nextElementSibling;

    // listen for 'click' events on the toolbar buttons
    btnShow.addEventListener("click", this.show.bind(this), false);
    btnHide.addEventListener("click", this.hide.bind(this), false);
    btnReset.addEventListener("click", this.reset.bind(this), false);
  },

  // ----------
  // Function: show
  // Enables the 'New Tab Page' feature.
  show: function Toolbar_show() {
    Services.prefs.setBoolPref(PREF_NEWTAB_ENABLED, true);
  },

  // ----------
  // Function: hide
  // Disables the 'New Tab Page' feature.
  hide: function Toolbar_hide() {
    Services.prefs.setBoolPref(PREF_NEWTAB_ENABLED, false);
  },

  // ----------
  // Function: hide
  // Resets the whole page and forces it to re-render its content.
  //
  // Parameters:
  //   aCallback - the callback to call when the page has been reset
  reset: function Toolbar_reset(aCallback) {
    if (aCallback && typeof aCallback != "function")
      aCallback = null;

    let node = Grid.node;

    // animate the page reset
    Animations.fadeOut(node, function () {
      NewTabUtils.reset(function () {
        Pages.update();
        Animations.fadeIn(node, aCallback);
      });
    });
  }
};

