/*
 * Bug 436069 - Fennec browser-chrome tests to verify correct navigation into the
 *              differents part of the awesome panel
 */

let testURL_01 = chromeRoot + "browser_blank_01.html";

let gTests = [];
let gCurrentTest = null;
let Panels = [AllPagesList, HistoryList, BookmarkList];

function test() {
  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();

  // Start the tests
  setTimeout(runNextTest, 200);
}

//------------------------------------------------------------------------------
// Iterating tests by shifting test out one by one as runNextTest is called.
function runNextTest() {
  // Run the next test until all tests completed
  if (gTests.length > 0) {
    gCurrentTest = gTests.shift();
    info(gCurrentTest.desc);
    gCurrentTest.run();
  }
  else {
    // Close the awesome panel just in case
    BrowserUI.activePanel = null;
    finish();
  }
}

//------------------------------------------------------------------------------
// Case: Test awesome bar collapsed state
gTests.push({
  desc: "Test awesome bar collapsed state",

  run: function() {
    window.addEventListener("NavigationPanelShown", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, false);
      gCurrentTest.onPopupShown();
    }, false);

    AllPagesList.doCommand();
  },

  onPopupShown: function() {
    is(BrowserUI.activePanel, AllPagesList, "AllPagesList should be visible");
    ok(!BrowserUI._edit.collapsed, "The urlbar edit element is visible");
    ok(BrowserUI._title.collapsed, "The urlbar title element is not visible");

    window.addEventListener("NavigationPanelHidden", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, false);
      gCurrentTest.onPopupHidden();
    }, false);

    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  },

  onPopupHidden: function() {
    is(BrowserUI.activePanel, null, "AllPagesList should be dismissed");
    ok(BrowserUI._edit.collapsed, "The urlbar edit element is not visible");
    ok(!BrowserUI._title.collapsed, "The urlbar title element is visible");

    runNextTest();
  }
});


//------------------------------------------------------------------------------
// Case: Test typing a character should dismiss the awesome header
gTests.push({
  desc: "Test typing a character should dismiss the awesome header",

  run: function() {
    window.addEventListener("NavigationPanelShown", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, true);
      gCurrentTest.onPopupReady();
    }, true);

    AllPagesList.doCommand();
  },

  onPopupReady: function() {
    is(BrowserUI.activePanel == AllPagesList, true, "AllPagesList should be visible");

    let awesomeHeader = document.getElementById("awesome-header");
    is(awesomeHeader.hidden, false, "Awesome header should be visible");

    BrowserUI._edit.addEventListener("onsearchbegin", function(aEvent) {
      if (BrowserUI._edit.value == "")
        return;

      BrowserUI._edit.removeEventListener(aEvent.type, arguments.callee, true);
      let awesomeHeader = document.getElementById("awesome-header");
      is(awesomeHeader.hidden, true, "Awesome header should be hidden");
      gCurrentTest.onKeyPress();
    }, true);
    EventUtils.synthesizeKey("A", {}, window);
  },

  onKeyPress: function(aKey, aHidden) {
    window.addEventListener("NavigationPanelHidden", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, false);
      let awesomeHeader = document.getElementById("awesome-header");
      is(awesomeHeader.hidden, false, "Awesome header should be visible");
      runNextTest();
    }, false);

    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  }
});

//------------------------------------------------------------------------------
// Case: Test typing a character should open the awesome bar
gTests.push({
  desc: "Test typing a character should open the All Pages List",

  run: function() {
    window.addEventListener("NavigationPanelShown", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, false);
      gCurrentTest.onPopupReady();
    }, false);
    BookmarkList.doCommand();
  },

  onPopupReady: function() {
    BrowserUI._edit.addEventListener("onsearchbegin", function(aEvent) {
      BrowserUI._edit.removeEventListener(aEvent.type, arguments.callee, false);
      gCurrentTest.onSearchBegin();
    }, false);
    EventUtils.synthesizeKey("I", {}, window);
  },

  onSearchBegin: function() {
    let awesomeHeader = document.getElementById("awesome-header");
    is(awesomeHeader.hidden, true, "Awesome header should be hidden");
    is(BrowserUI.activePanel == AllPagesList, true, "AllPagesList should be opened on a keydown");
    is(BrowserUI._edit.readOnly, false, "urlbar should not be readonly after an input");

    window.addEventListener("NavigationPanelHidden", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, false);
      gCurrentTest.onPopupHidden();
    }, false);

    EventUtils.synthesizeKey("VK_ESCAPE", {}, window);
  },

  onPopupHidden: function() {
    is(BrowserUI.activePanel == null, true, "VK_ESCAPE should have dismissed the awesome panel");
    runNextTest();
  }
});

//------------------------------------------------------------------------------
// Case: Test opening the awesome panel and checking the urlbar readonly state
gTests.push({
  desc: "Test opening the awesome panel and checking the urlbar readonly state",

  run: function() {
    is(BrowserUI._edit.readOnly, true, "urlbar input textbox should be readonly");

    window.addEventListener("NavigationPanelShown", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, true);
      gCurrentTest.onPopupReady();
    }, true);

    AllPagesList.doCommand();
  },

  onPopupReady: function() {
    is(Elements.urlbarState.getAttribute("mode"), "edit", "bcast_urlbarState mode attribute should be equal to 'edit'");

    let edit = BrowserUI._edit;
    is(edit.readOnly, true, "urlbar input textbox be readonly once it is open in landscape");

    let urlString = BrowserUI.getDisplayURI(Browser.selectedBrowser);
    if (Util.isURLEmpty(urlString))
      urlString = "";

    Panels.forEach(function(aPanel) {
      aPanel.doCommand();
      is(BrowserUI.activePanel, aPanel, "The panel " + aPanel.panel.id + " should be selected");
      is(edit.readOnly, true, "urlbar input textbox be readonly once it is open in landscape");
      edit.click();
      is(edit.readOnly, false, "urlbar input textbox should not be readonly once it is open in landscape and click again");

      is(edit.value, urlString, "urlbar value should be equal to the page uri");
    });

    setTimeout(function() {
      BrowserUI.activePanel = null;
      runNextTest();
    }, 0);
  }
});

//------------------------------------------------------------------------------
// Case: Test opening the awesome panel and checking the urlbar selection
gTests.push({
  desc: "Test opening the awesome panel and checking the urlbar selection",

  run: function() {
    info("is nav panel open: " + BrowserUI.isAutoCompleteOpen())
    BrowserUI.closeAutoComplete(true);
    info("opening new tab")
    this._currentTab = BrowserUI.newTab(testURL_01);

    // Need to wait until the page is loaded
    messageManager.addMessageListener("pageshow",
    function(aMessage) {
      info("got a pageshow: " + gCurrentTest._currentTab.browser.currentURI.spec)
      if (gCurrentTest._currentTab.browser.currentURI.spec != "about:blank") {
        info("got the right pageshow")
        messageManager.removeMessageListener(aMessage.name, arguments.callee);
        gCurrentTest.onPageReady();
      }
    });
  },

  onPageReady: function() {
    window.addEventListener("NavigationPanelShown", function(aEvent) {
      info("nav panel is open")
      window.removeEventListener(aEvent.type, arguments.callee, false);
      gCurrentTest.onPopupReady();
    }, false);

    info("opening nav panel")
    AllPagesList.doCommand();
  },

  onPopupReady: function() {
    let edit = BrowserUI._edit;

    Panels.forEach(function(aPanel) {
      aPanel.doCommand();
      ok(edit.selectionStart ==  edit.selectionEnd, "urlbar text should not be selected on a simple show");
      edit.click();
      ok(edit.selectionStart == 0 && edit.selectionEnd == edit.textLength, "urlbar text should be selected on a click");

    });

    let oldClickSelectsAll = edit.clickSelectsAll;
    edit.clickSelectsAll = false;
    Panels.forEach(function(aPanel) {
      aPanel.doCommand();
      ok(edit.selectionStart == edit.selectionEnd, "urlbar text should not be selected on a simple show");
      edit.click();
      ok(edit.selectionStart == edit.selectionEnd, "urlbar text should not be selected on a click");
    });
    edit.clickSelectsAll = oldClickSelectsAll;

    BrowserUI.closeTab(this._currentTab);

    BrowserUI.activePanel = null;
    runNextTest();
  }
});         //------------------------------------------------------------------------------
// Case: Test opening the awesome panel and checking the urlbar selection
gTests.push({
  desc: "Test context clicks on awesome panel",

  _panelIndex : 0,
  _contextOpts : [
    ["link-openable", "link-shareable"],
    ["link-openable", "link-shareable"],
    ["edit-bookmark", "link-shareable", "link-openable"],
  ],

  clearContextTypes: function clearContextTypes() {
    if (ContextHelper.popupState)
      ContextHelper.hide();
  },

  checkContextTypes: function checkContextTypes(aTypes) {
    let commandlist = document.getElementById("context-commands");
  
    for (let i=0; i<commandlist.childNodes.length; i++) {
      let command = commandlist.childNodes[i];
      if (aTypes.indexOf(command.getAttribute("type")) > -1) {
        // command should be visible
        if(command.hidden == true)
          return false;
      } else {
        if(command.hidden == false)
          return false;
      }
    }
    return true;
  },

  run: function() {
    window.addEventListener("NavigationPanelShown", function(aEvent) {
      window.removeEventListener(aEvent.type, arguments.callee, false);
      gCurrentTest.onPopupReady();
    }, false);

    AllPagesList.doCommand();
  },

  onPopupReady: function() {
    let self = this;
    if(self._panelIndex < Panels.length) {
      let panel = Panels[self._panelIndex];
      panel.doCommand();

      self.clearContextTypes();      

      EventUtils.synthesizeMouse(panel.panel, panel.panel.width / 2, panel.panel.height / 2, { type: "mousedown" });
      setTimeout(function() {
        EventUtils.synthesizeMouse(panel.panel, panel.panel.width / 2, panel.panel.height / 2, { type: "mouseup" });
        ok(self.checkContextTypes(self._contextOpts[self._panelIndex]), "Correct context menu shown for panel");
        self.clearContextTypes();
  
        self._panelIndex++;
        self.onPopupReady();
      }, 500);
    } else {
      runNextTest();
    }
  }
});


