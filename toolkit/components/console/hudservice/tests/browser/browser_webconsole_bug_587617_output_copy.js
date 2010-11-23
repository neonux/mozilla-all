/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *  Patrick Walton <pcwalton@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded() {
  browser.removeEventListener("load", tabLoaded, true);
  openConsole();

  // See bugs 574036, 586386 and 587617.

  hudId = HUDService.displaysIndex()[0];
  let HUD = HUDService.hudReferences[hudId].HUDBox;
  let filterBox = HUD.querySelector(".hud-filter-box");
  outputNode = HUD.querySelector(".hud-output-node");
  let selection = getSelection();
  let jstermInput = HUD.querySelector(".jsterm-input-node");
  let console = browser.contentWindow.wrappedJSObject.console;
  let contentSelection = browser.contentWindow.wrappedJSObject.getSelection();

  let make_selection = function () {
    let controller =
      top.document.commandDispatcher.
      getControllerForCommand("cmd_copy");
    is(controller.isCommandEnabled("cmd_copy"), false, "cmd_copy is disabled");

    console.log("Hello world!");

    let range = top.document.createRange();
    let selectedNode = outputNode.querySelector(".hud-group > label:last-child");
    range.selectNode(selectedNode);
    selection.addRange(range);

    selectedNode.focus();

    goUpdateCommand("cmd_copy");

    controller = top.document.commandDispatcher.
      getControllerForCommand("cmd_copy");
    is(controller.isCommandEnabled("cmd_copy"), true, "cmd_copy is enabled");

    waitForClipboard(selectedNode.textContent, clipboard_setup,
      clipboard_copy_done, clipboard_copy_done);
    };

    let clipboard_setup = function () {
      goDoCommand("cmd_copy");
    };

    let clipboard_copy_done = function () {
      selection.removeAllRanges();
      finishTest();
    };

    // Check if we first need to clear any existing selections.
    if (selection.rangeCount > 0 || contentSelection.rangeCount > 0 ||
        jstermInput.selectionStart != jstermInput.selectionEnd) {
      if (jstermInput.selectionStart != jstermInput.selectionEnd) {
        jstermInput.selectionStart = jstermInput.selectionEnd = 0;
      }

      if (selection.rangeCount > 0) {
        selection.removeAllRanges();
      }

      if (contentSelection.rangeCount > 0) {
        contentSelection.removeAllRanges();
      }

      goUpdateCommand("cmd_copy");
        make_selection();
    }
    else {
      make_selection();
    }
}
