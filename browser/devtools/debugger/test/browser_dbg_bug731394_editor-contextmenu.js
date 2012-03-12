/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 731394: test the debugger source editor default context menu.
 */

const TAB_URL = EXAMPLE_URL + "browser_dbg_script-switching.html";

let gPane = null;
let gTab = null;
let gDebuggee = null;
let gDebugger = null;

function test()
{
  let tempScope = {};
  Cu.import("resource:///modules/source-editor.jsm", tempScope);
  let SourceEditor = tempScope.SourceEditor;

  let contextMenu = null;

  debug_tab_pane(TAB_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gDebuggee = aDebuggee;
    gPane = aPane;
    gDebugger = gPane.debuggerWindow;

    gPane.activeThread.addOneTimeListener("scriptsadded", function() {
      Services.tm.currentThread.dispatch({ run: onScriptsAdded }, 0);
    });
    gDebuggee.firstCall();
  });

  function onScriptsAdded()
  {
    let scripts = gDebugger.DebuggerView.Scripts._scripts;

    is(gDebugger.StackFrames.activeThread.state, "paused",
      "Should only be getting stack frames while paused.");

    is(scripts.itemCount, 2, "Found the expected number of scripts.");

    let editor = gDebugger.editor;

    isnot(editor.getText().indexOf("debugger"), -1,
          "The correct script was loaded initially.");

    contextMenu = gDebugger.document.getElementById("sourceEditorContextMenu");
    ok(contextMenu, "source editor context menupopup");
    ok(editor.readOnly, "editor is read only");

    editor.focus();
    editor.setSelection(0, 10);

    contextMenu.addEventListener("popupshown", function onPopupShown() {
      contextMenu.removeEventListener("popupshown", onPopupShown, false);
      executeSoon(testContextMenu);
    }, false);
    contextMenu.openPopup(editor.editorElement, "overlap", 0, 0, true, false);
  }

  function testContextMenu()
  {
    let document = gDebugger.document;

    ok(document.getElementById("editMenuCommands"),
       "#editMenuCommands found");
    ok(!document.getElementById("editMenuKeys"),
       "#editMenuKeys not found");
    ok(document.getElementById("sourceEditorCommands"),
       "#sourceEditorCommands found");
    ok(document.getElementById("sourceEditorKeys"),
       "#sourceEditorKeys found");

    // Map command ids to their expected disabled state.
    let commands = {"se-cmd-undo": true, "se-cmd-redo": true,
                    "se-cmd-cut": true, "se-cmd-paste": true,
                    "se-cmd-delete": true, "cmd_findAgain": true,
                    "cmd_findPrevious": true, "cmd_find": false,
                    "cmd_gotoLine": false, "cmd_copy": false,
                    "se-cmd-selectAll": false};
    for (let id in commands) {
      let element = document.getElementById(id);
      is(element.hasAttribute("disabled"), commands[id],
         id + " hasAttribute('disabled') check");
    }

    executeSoon(function() {
      contextMenu.hidePopup();
      gDebugger.StackFrames.activeThread.resume(finish);
    });
  }

  registerCleanupFunction(function() {
    removeTab(gTab);
    gPane = null;
    gTab = null;
    gDebuggee = null;
    gDebugger = null;
  });
}
