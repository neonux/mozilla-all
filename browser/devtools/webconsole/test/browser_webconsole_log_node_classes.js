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
 *  Patrick Walton <pcwalton@mozilla.com>
 *  Julian Viereck <jviereck@mozilla.com>
 *  Mihai Sucan <mihai.sucan@gmail.com>
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

// Tests that console logging via the console API produces nodes of the correct
// CSS classes.

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(consoleOpened);
  }, true);
}

function consoleOpened(aHud) {
  let console = content.console;
  outputNode = aHud.outputNode;

  ok(console, "console exists");
  console.log("I am a log message");
  console.error("I am an error");
  console.info("I am an info message");
  console.warn("I am a warning  message");

  waitForSuccess({
    name: "console.warn displayed",
    validatorFn: function()
    {
      return aHud.outputNode.textContent.indexOf("a warning") > -1;
    },
    successFn: testLogNodeClasses,
    failureFn: finishTest,
  });
}

function testLogNodeClasses() {
  let domLogEntries = outputNode.childNodes;

  let count = outputNode.childNodes.length;
  ok(count > 0, "LogCount: " + count);

  let klasses = ["hud-log",
                 "hud-warn",
                 "hud-info",
                 "hud-error",
                 "hud-exception",
                 "hud-network"];

  function verifyClass(classList) {
    let len = klasses.length;
    for (var i = 0; i < len; i++) {
      if (classList.contains(klasses[i])) {
        return true;
      }
    }
    return false;
  }

  for (var i = 0; i < count; i++) {
    let classList = domLogEntries[i].classList;
    ok(verifyClass(classList),
       "Log Node class verified: " + domLogEntries[i].getAttribute("class"));
  }

  finishTest();
}

