function test() {
  waitForExplicitFinish();

  Services.ww.registerNotification(function (aSubject, aTopic, aData) {
    if (aTopic == "domwindowopened") {
      Services.ww.unregisterNotification(arguments.callee);

      ok(true, "duplicateTabIn opened a new window");

      aSubject.addEventListener("load", function () {
        executeSoon(function () {
          aSubject.close();
          finish();
        });
      }, false);
    }
  });

  duplicateTabIn(gBrowser.selectedTab, "window");
}
