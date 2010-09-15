/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This tests is modifying a file in an unpacked extension
// causes cache invalidation.

// Disables security checking our updates which haven't been signed
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

const Ci = Components.interfaces;
const extDir = gProfD.clone();
extDir.append("extensions");

/**
 * Start the test by installing extensions.
 */
function run_test() {
  do_test_pending();
  let cachePurged = false;

  let obs = AM_Cc["@mozilla.org/observer-service;1"].
    getService(AM_Ci.nsIObserverService);
  obs.addObserver({
    observe: function(aSubject, aTopic, aData) {
      cachePurged = true;
    }
  }, "startupcache-invalidate", false);
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");
  startupManager();

  installAllFiles([do_get_addon("test_bug594058")], function() {
    restartManager();
    do_check_true(cachePurged);
    cachePurged = false;

    // Now, make it look like we've updated the file. First, start the EM
    // so it records the bogus old time, then update the file and restart.
    let extFile = extDir.clone();
    let pastTime = extFile.lastModifiedTime - 5000;
    extFile.append("bug594058@tests.mozilla.org");
    setExtensionModifiedTime(extFile, pastTime);
    let otherFile = extFile.clone();
    otherFile.append("directory");
    otherFile.lastModifiedTime = pastTime;
    otherFile.append("file1");
    otherFile.lastModifiedTime = pastTime;

    restartManager();
    cachePurged = false;

    otherFile.lastModifiedTime = pastTime + 5000;
    restartManager();
    do_check_true(cachePurged);
    cachePurged = false;

    restartManager();
    do_check_true(!cachePurged);

    do_test_finished();
  });  
}
