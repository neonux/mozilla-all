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
 * The Original Code is tabview Startup Transition (Bug 591705) test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
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

var prefsBranch = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService).
                  getBranch("browser.panorama.");

function animateZoom() prefsBranch.getBoolPref("animate_zoom");

function registerCleanupFunction() {
  prefsBranch.setUserPref("animate_zoom", true);
}

function test() {
  waitForExplicitFinish();
  
  let charsetArg = "charset=" + window.content.document.characterSet;
  let win = window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no",
                              "about:blank", charsetArg, null, null, true);
  
  ok(animateZoom(), "By default, we animate on zoom.");
  prefsBranch.setBoolPref("animate_zoom", false);
  ok(!animateZoom(), "animate_zoom = false");
  
  let onLoad = function() {
    win.removeEventListener("load", onLoad, false);

    // a few shared references
    let tabViewWindow = null;
    let transitioned = 0;

    let onShown = function() {
      win.removeEventListener("tabviewshown", onShown, false);

      ok(!transitioned, "There should be no transitions");
      win.close();

      finish();
    };

    let initCallback = function() {
      tabViewWindow = win.TabView._window;
      function onTransitionEnd(event) {
        transitioned++;
        tabViewWindow.Utils.log(transitioned);
      }
      tabViewWindow.document.addEventListener("transitionend", onTransitionEnd, false);

      win.TabView.show();
    };

    win.addEventListener("tabviewshown", onShown, false);
    win.TabView._initFrame(initCallback);
  }
  win.addEventListener("load", onLoad, false);
}
