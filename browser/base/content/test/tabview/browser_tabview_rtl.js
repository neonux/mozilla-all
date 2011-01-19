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
 * The Original Code is tabview RTL support test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan@mozilla.com> (Original Author)
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

let tabViewShownCount = 0;

// ----------
function test() {
  waitForExplicitFinish();

  // verify initial state
  ok(!TabView.isVisible(), "Tab View starts hidden");

  // use the Tab View button to launch it for the first time
  window.addEventListener("tabviewshown", onTabViewLoadedAndShown("ltr"), false);
  toggleTabView();
}

function toggleTabView() {
  let button = document.getElementById("tabview-button");
  ok(!button, "Tab View button not exist by default");
  let tabViewCommand = document.getElementById("Browser:ToggleTabView");
  tabViewCommand.doCommand();
}

// ----------
function onTabViewLoadedAndShown(dir) {
  return function() {
    window.removeEventListener("tabviewshown", arguments.callee, false);
    ok(TabView.isVisible(), "Tab View is visible.");

    let contentWindow = document.getElementById("tab-view").contentWindow;
    let contentDocument = contentWindow.document;
    is(contentDocument.documentElement.getAttribute("dir"), dir,
       "The direction should be set to " + dir.toUpperCase());

    // kick off the series
    window.addEventListener("tabviewhidden", onTabViewHidden(dir), false);
    TabView.toggle();
  };
}

// ---------- 
function onTabViewHidden(dir) {
  return function() {
    window.removeEventListener("tabviewhidden", arguments.callee, false);
    ok(!TabView.isVisible(), "Tab View is hidden.");

    if (dir == "ltr") {
      // Switch to RTL mode
      Services.prefs.setCharPref("intl.uidirection.en-US", "rtl");

      // use the Tab View button to launch it for the second time
      window.addEventListener("tabviewshown", onTabViewLoadedAndShown("rtl"), false);
      toggleTabView();
    } else {
      // Switch to LTR mode
      Services.prefs.clearUserPref("intl.uidirection.en-US");

      finish();
    }
  };
}

