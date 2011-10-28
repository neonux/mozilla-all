/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let gWin;
let gTests = [
  { desc: "Check that the home tab exists",
    run: function () {
      is(gWin.gBrowser.tabs.length, 1, "Window only has 1 tab");
      ok(gWin.gBrowser.selectedTab.isHomeTab, "The selected tab is the home tab");
      runNextTest();
    }
  }
];

registerCleanupFunction(function() {
  // Add back user pref to disable home tab
  Services.prefs.setBoolPref("browser.hometab.enabled", false);
  gWin.close();
});

function test() {
  waitForExplicitFinish();

  // Remove user pref that was added by the test harness
  Services.prefs.clearUserPref("browser.hometab.enabled");

  gWin = OpenBrowserWindow();
  gWin.addEventListener("load", function onWindowLoad() {
    gWin.removeEventListener("load", onWindowLoad, false);
    executeSoon(runNextTest);
  }, false);
}

function runNextTest() {
  if (gTests.length) {
    let test = gTests.shift();
    info(test.desc);
    test.run();
  } else {
    finish();
  }
}
