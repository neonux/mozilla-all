/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-on update check correctly fills in the
// %COMPATIBILITY_MODE% token in the update URL.


// The test extension uses an insecure update url.
Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

do_load_httpd_js();
var testserver;
const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.start(4444);

  writeInstallRDFForExtension({
    id: "compatmode-normal@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_updatecompatmode_%COMPATIBILITY_MODE%.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - normal"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "compatmode-strict@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_updatecompatmode_%COMPATIBILITY_MODE%.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - strict"
  }, profileDir);

  writeInstallRDFForExtension({
    id: "compatmode-strict-optin@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_updatecompatmode_%COMPATIBILITY_MODE%.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - strict opt-in",
    strictCompatibility: true
  }, profileDir);

  writeInstallRDFForExtension({
    id: "compatmode-ignore@tests.mozilla.org",
    version: "1.0",
    updateURL: "http://localhost:4444/data/test_updatecompatmode_%COMPATIBILITY_MODE%.rdf",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon - ignore",
  }, profileDir);

  restartManager();
  run_test_1();
}

// Strict compatibility checking disabled.
function run_test_1() {
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  AddonManager.getAddonByID("compatmode-normal@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);
    addon.findUpdates({
      onCompatibilityUpdateAvailable: function() {
        do_throw("Should have not have seen compatibility information");
      },

      onNoUpdateAvailable: function() {
        do_throw("Should have seen an available update");
      },

      onUpdateAvailable: function(addon, install) {
        do_check_eq(install.version, "2.0")
      },

      onUpdateFinished: function() {
        run_test_2();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

// Strict compatibility checking enabled.
function run_test_2() {
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);
  AddonManager.getAddonByID("compatmode-strict@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);
    addon.findUpdates({
      onCompatibilityUpdateAvailable: function() {
        do_throw("Should have not have seen compatibility information");
      },

      onNoUpdateAvailable: function() {
        do_throw("Should have seen an available update");
      },

      onUpdateAvailable: function(addon, install) {
        do_check_eq(install.version, "2.0")
      },

      onUpdateFinished: function() {
        run_test_3();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

// Strict compatibility checking opt-in.
function run_test_3() {
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  AddonManager.getAddonByID("compatmode-strict-optin@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);
    addon.findUpdates({
      onCompatibilityUpdateAvailable: function() {
        do_throw("Should have not have seen compatibility information");
      },

      onUpdateAvailable: function(addon, install) {
        do_throw("Should not have seen an available update");
      },

      onUpdateFinished: function() {
        run_test_4();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}

// Compatibility checking disabled.
function run_test_4() {
  Services.prefs.setBoolPref(COMPATIBILITY_PREF, false);
  AddonManager.getAddonByID("compatmode-ignore@tests.mozilla.org", function(addon) {
    do_check_neq(addon, null);
    addon.findUpdates({
      onCompatibilityUpdateAvailable: function() {
        do_throw("Should have not have seen compatibility information");
      },

      onNoUpdateAvailable: function() {
        do_throw("Should have seen an available update");
      },

      onUpdateAvailable: function(addon, install) {
        do_check_eq(install.version, "2.0")
      },

      onUpdateFinished: function() {
        end_test();
      }
    }, AddonManager.UPDATE_WHEN_USER_REQUESTED);
  });
}
