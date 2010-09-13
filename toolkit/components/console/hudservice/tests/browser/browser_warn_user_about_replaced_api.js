/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is DevTools test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Dahl <ddahl@mozilla.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/HUDService.jsm");

const TEST_REPLACED_API_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console-replaced-api.html";

function log(aMsg)
{
  dump("*** WebConsoleTest: " + aMsg + "\n");
}

function testOpenWebConsole()
{
  HUDService.activateHUDForContext(gBrowser.selectedTab);
  is(HUDService.displaysIndex().length, 1, "WebConsole was opened");

  hudId = HUDService.displaysIndex()[0];
  hud = HUDService.getHeadsUpDisplay(hudId);

  HUDService.logWarningAboutReplacedAPI(hudId);
}

function testWarning()
{
  const successMsg = "Found the warning message";
  const errMsg = "Could not find the warning message about the replaced API";

  var display = HUDService.getDisplayByURISpec(content.location.href);
  var outputNode = display.querySelectorAll(".hud-output-node")[0];
  executeSoon(function () {
    testLogEntry(outputNode, "disabled", { success: successMsg, err: errMsg });
  });
}

function testLogEntry(aOutputNode, aMatchString, aSuccessErrObj)
{
  var message = aOutputNode.textContent.indexOf(aMatchString);
  if (message > -1) {
    ok(true, aSuccessErrObj.success);
  return;
  }
  ok(false, aSuccessErrObj.err);
}

function finishTest() {
  hud = null;
  hudId = null;

  executeSoon(function() {
    finish();
  });
}

let hud, hudId, tab, browser, filterBox, outputNode;
let win = gBrowser.selectedBrowser;

tab = gBrowser.selectedTab;
browser = gBrowser.getBrowserForTab(tab);

content.location.href = TEST_REPLACED_API_URI;

function test() {
  waitForExplicitFinish();
  browser.addEventListener("DOMContentLoaded", function onLoad(event) {
    browser.removeEventListener("DOMContentLoaded", onLoad, false);
    executeSoon(function (){
      testOpenWebConsole();
      executeSoon(function (){
        testWarning();
      });
    });
  }, false);
  finishTest();
}
