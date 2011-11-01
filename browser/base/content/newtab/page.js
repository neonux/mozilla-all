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

// TODO
let Page = {
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

  lock: function Page_lock() {
    document.body.classList.add("locked");
  },

  unlock: function Page_unlock() {
    document.body.classList.remove("locked");
  },

  isLocked: function Page_isLocked() {
    return document.body.classList.contains("locked");
  },

  isEnabled: function Page_isEnabled() {
    return Services.prefs.getBoolPref(PREF_NEWTAB_ENABLED, true);
  },

  modify: function Page_modify() {
    document.body.classList.add("modified");
  },

  reset: function Page_reset() {
    document.body.classList.remove("modified");
  },

  isModified: function Page_isModified() {
    let blockedLinks = NewTabUtils.Storage.get("blockedLinks", {});
    return Object.keys(blockedLinks).length > 0;
  },

  refresh: function Page_refresh() {
    Links.reset();
    Grid.refresh();
  },

  observe: function Page_observe() {
    let classes = document.body.classList;

    if (this.isEnabled()) {
      classes.remove("disabled");
      this._init();
    } else {
      classes.add("disabled");
    }
  },

  _init: function Page__init() {
    if (this._initialized)
      return;

    this._initialized = true;

    NewTabUtils.Pages.register(this);

    let self = this;

    // listen for 'unload' to unregister this page
    addEventListener("unload", function () {
      NewTabUtils.Pages.unregister(self);
    }, false);

    // check if the grid has been modified
    if (this.isModified())
      this.modify();

    Grid.init(this._gridSelector);
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference])
};
