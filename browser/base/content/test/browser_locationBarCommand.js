/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL_1 = "about:mozilla";
const TEST_URL_2 = "about:robots";
const TEST_URL_3 = "http://example.com/browser/browser/base/content/test/alltabslistener.html";

const PROMPT_URL = "chrome://global/content/commonDialog.xul";
const BROWSER_URL = "chrome://browser/content/browser.xul";

let gGoButton = document.getElementById("urlbar-go-button");
let gFocusManager = Cc["@mozilla.org/focus-manager;1"].getService(Ci.nsIFocusManager);

function test() {
  waitForExplicitFinish();
  nextTest();
}

function nextTest() {
  let test = tests.shift();
  if (test) {
    info("Starting test: " + test.name);
    test(nextTest);
  } else
    finish();
}

let tests = [
  function testRightClick(aCallback) {
    testUrlbarCommand(true, TEST_URL_1, { button: 2 }, function(aTab) {
      is(gURLBar.value, TEST_URL_1, "Urlbar still has the value we entered");
      ok(gURLBar.focused, "Urlbar is still focused after click");
      gBrowser.removeTab(aTab);

      aCallback();
    });
  },

  function testLeftClick(aCallback) {
    testUrlbarCommand(true, TEST_URL_1, {}, function(aTab) {
      testLoadCurrent(TEST_URL_1, aTab);
      aCallback();
    });
  },

  function testMetaLeftClick(aCallback) {
    testUrlbarCommand(true, TEST_URL_1, { metaKey: true, ctrlKey: true }, function(aTab) {
      testLoadTab(TEST_URL_1, aTab);
      aCallback();
    });
  },

  function testShiftMetaLeftClick(aCallback) {
    testUrlbarCommand(true, TEST_URL_1, { shiftKey: true, metaKey: true, ctrlKey: true  }, function(aTab) {
      is(gURLBar.value, "", "Urlbar reverted to original value");
      ok(!gURLBar.focused, "Urlbar is no longer focused after urlbar command");
      is(gBrowser.selectedTab, aTab, "Focus did not change to the new tab");

      // Close the original tab first
      gBrowser.removeTab(aTab);

      is(gURLBar.value, TEST_URL_1, "New URL is loaded in new tab");
      gBrowser.removeTab(gBrowser.selectedTab);

      aCallback();
    });
  },

/*
  function testAltLeftClick(aCallback) {
    let tab = gBrowser.selectedTab = gBrowser.addTab();
    gURLBar.value = TEST_URL_3;
    gURLBar.focus();

    let listener = new WindowListener(PROMPT_URL, function (domwindow) {
      dump("\n\n\nwindow listener callback\n\n\n");
  
      // Close the dialog
      domwindow.document.documentElement.cancelDialog();
      Services.wm.removeListener(listener);
      gBrowser.removeTab(tab);
      aCallback();
    });
    dump("\n\n\nbefore listener added\n\n\n");
    Services.wm.addListener(listener);

    EventUtils.synthesizeMouseAtCenter(gGoButton, { altKey: true });
  },*/

  function testShiftLeftClick(aCallback) {
    let tab = gBrowser.selectedTab = gBrowser.addTab();
    gURLBar.value = TEST_URL_1;
    gURLBar.focus();

    let listener = new WindowListener(BROWSER_URL, function (domwindow) {
      is(gFocusManager.focusedElement, null, "There should be no focused element");
    
      domwindow.close()
      Services.wm.removeListener(listener);
      gBrowser.removeTab(tab);
      aCallback();
    });
    Services.wm.addListener(listener);

    EventUtils.synthesizeMouseAtCenter(gGoButton, { shiftKey: true });
  },

  function testReturn(aCallback) {
    testUrlbarCommand(false, TEST_URL_1, {}, function(aTab) {
      testLoadCurrent(TEST_URL_1, aTab);
      aCallback();
    });
  },

  function testAltReturnEmptyTab(aCallback) {
    testUrlbarCommand(false, TEST_URL_1, { altKey: true }, function(aTab) {
      testLoadCurrent(TEST_URL_1, aTab);
      aCallback();
    });
  },

  function testAltReturnDirtyTab(aCallback) {
    testUrlbarCommand(false, TEST_URL_1, { altKey: true }, function(aTab) {
      testLoadTab(TEST_URL_1, aTab);
      aCallback();
    }, TEST_URL_2);
  },

  function testMetaReturn(aCallback) {
    testUrlbarCommand(false, TEST_URL_1, { metaKey: true, ctrlKey: true }, function(aTab) {
      testLoadCurrent(TEST_URL_1, aTab);
      aCallback();
    });
  },

  function testAltReturnPopup(aCallback) {
    // TODO: make popup window, then try to open url with alt+enter - url should open in full browser window
    
    aCallback();
  },

  function testReturnPinned(aCallback) {
    testUrlbarCommand(false, TEST_URL_1, {}, function(aTab) {
      testLoadTab(TEST_URL_1, aTab);
      aCallback();
    }, null, true);    
  }
  
  // TODO: add more pinned tab tests
]

/** 
 * aEvent is an object which may contain the properties:
 *   shiftKey, ctrlKey, altKey, metaKey, accessKey, clickCount, button, type
 *
 * aCallback takes the new tab as a parameter, and it is expected to remove
 *   this tab when it's finished.
 *
 * (optional) aOriginalURL will be loaded in the tab before we interact with
 *   the urlbar.
 */
function testUrlbarCommand(aClick, aURL, aEvent, aCallback, aOriginalURL, aPinTab) {
  let tab = gBrowser.selectedTab = gBrowser.addTab(aOriginalURL);
  if (aPinTab)
    gBrowser.pinTab(tab);

  addPageShowListener(function () {
    gURLBar.value = aURL;
    gURLBar.focus();

    if (aClick)
      EventUtils.synthesizeMouseAtCenter(gGoButton, aEvent);
    else
      EventUtils.synthesizeKey("VK_RETURN", aEvent);

    aCallback(tab);
  });
}

/* Checks that the URL was loaded in the current tab */
function testLoadCurrent(aURL, aTab) {
  info("URL should be loaded in the current tab");

  is(gURLBar.value, aURL, "Urlbar still has the value we entered");
  is(gFocusManager.focusedElement, null, "There should be no focused element");
  is(gFocusManager.focusedWindow, gBrowser.contentWindow, "Content window should be focused");
  is(gBrowser.selectedTab, aTab, "New URL was loaded in the current tab");

  gBrowser.removeTab(aTab);
}

/* Checks that the URL was loaded in a new tab */
function testLoadTab(aURL, aTab) {
  info("URL should be loaded in a focused new tab");

  is(gURLBar.value, aURL, "Urlbar still has the value we entered");
  is(gFocusManager.focusedElement, null, "There should be no focused element");
  is(gFocusManager.focusedWindow, gBrowser.contentWindow, "Content window should be focused");
  isnot(gBrowser.selectedTab, aTab, "New URL was loaded in a new tab");

  // Close the new tab and the original tab
  gBrowser.removeTab(gBrowser.selectedTab);
  gBrowser.removeTab(aTab);
}

function addPageShowListener(func) {
  gBrowser.selectedBrowser.addEventListener("pageshow", function loadListener() {
    gBrowser.selectedBrowser.removeEventListener("pageshow", loadListener, false);
    func();
  });
}

/* borrowed from browser_NetUtil.js */
function WindowListener(aURL, aCallback) {
  this.callback = aCallback;
  this.url = aURL;
}
WindowListener.prototype = {
  onOpenWindow: function(aXULWindow) {
    var domwindow = aXULWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindow);
    var self = this;
    domwindow.addEventListener("load", function() {
      domwindow.removeEventListener("load", arguments.callee, false);

      if (domwindow.document.location.href != self.url)
        return;

      // Allow other window load listeners to execute before passing to callback
      executeSoon(function() {
        self.callback(domwindow);
      });
    }, false);
  },
  onCloseWindow: function(aXULWindow) {},
  onWindowTitleChange: function(aXULWindow, aNewTitle) {}
}
