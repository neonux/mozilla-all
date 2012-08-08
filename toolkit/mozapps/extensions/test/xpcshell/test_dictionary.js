/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that bootstrappable add-ons can be used without restarts.
Components.utils.import("resource://gre/modules/Services.jsm");

// Enable loading extensions from the user scopes
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_USER);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");
const userExtDir = gProfD.clone();
userExtDir.append("extensions2");
userExtDir.append(gAppInfo.ID);
registerDirectory("XREUSysExt", userExtDir.parent);

Components.utils.import("resource://testing-common/httpd.js");
var testserver;

/**
 * This object is both a factory and an mozISpellCheckingEngine implementation (so, it
 * is de-facto a service). It's also an interface requestor that gives out
 * itself when asked for mozISpellCheckingEngine.
 */
var HunspellEngine = {
  dictionaryDirs: [],
  listener: null,
  
  QueryInterface: function hunspell_qi(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIFactory) ||
        iid.equals(Components.interfaces.mozISpellCheckingEngine))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  createInstance: function hunspell_ci(outer, iid) {
    if (outer)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },
  lockFactory: function hunspell_lockf(lock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  addDirectory: function hunspell_addDirectory(dir) {
    this.dictionaryDirs.push(dir);
    if (this.listener)
      this.listener("addDirectory");
  },

  removeDirectory: function hunspell_addDirectory(dir) {
    this.dictionaryDirs.splice(this.dictionaryDirs.indexOf(dir), 1);
    if (this.listener)
      this.listener("removeDirectory");
  },

  getInterface: function hunspell_gi(iid) {
    if (iid.equals(Components.interfaces.mozISpellCheckingEngine))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  contractID: "@mozilla.org/spellchecker/engine;1",
  classID: Components.ID("{6f3c63bc-a4fd-449b-9a58-a2d9bd972cce}"),

  activate: function hunspell_activate() {
    this.origClassID = Components.manager.nsIComponentRegistrar
      .contractIDToCID(this.contractID);
    this.origFactory = Components.manager
      .getClassObject(Components.classes[this.contractID],
                      Components.interfaces.nsIFactory);

    Components.manager.nsIComponentRegistrar
      .unregisterFactory(this.origClassID, this.origFactory);
    Components.manager.nsIComponentRegistrar.registerFactory(this.classID,
      "Test hunspell", this.contractID, this);
  },
  
  deactivate: function hunspell_deactivate() {
    Components.manager.nsIComponentRegistrar.unregisterFactory(this.classID, this);
    Components.manager.nsIComponentRegistrar.registerFactory(this.origClassID,
      "Hunspell", this.contractID, this.origFactory);
  },

  isDictionaryEnabled: function hunspell_isDictionaryEnabled(name) {
    return this.dictionaryDirs.some(function(dir) {
      var dic = dir.clone();
      dic.append(name);
      return dic.exists();
    });
  }
};

function run_test() {
  do_test_pending();

  // Create and configure the HTTP server.
  testserver = new HttpServer();
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.start(4444);

  startupManager();

  run_test_1();
}

// Tests that installing doesn't require a restart
function run_test_1() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  HunspellEngine.activate();

  AddonManager.getInstallForFile(do_get_addon("test_dictionary"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.type, "dictionary");
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Test Dictionary");
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
    do_check_true(install.addon.hasResource("install.rdf"));
    do_check_false(install.addon.hasResource("bootstrap.js"));
    do_check_eq(install.addon.operationsRequiringRestart &
                AddonManager.OP_NEEDS_RESTART_INSTALL, 0);
    do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    let addon = install.addon;
    prepare_test({
      "ab-CD@dictionaries.addons.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], function() {
      do_check_true(addon.hasResource("install.rdf"));
      HunspellEngine.listener = function(aEvent) {
        HunspellEngine.listener = null;
        do_check_eq(aEvent, "addDirectory");
        check_test_1();
      };
    });
    install.install();
  });
}

function check_test_1() {
  AddonManager.getAllInstalls(function(installs) {
    // There should be no active installs now since the install completed and
    // doesn't require a restart.
    do_check_eq(installs.length, 0);

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
      do_check_neq(b1, null);
      do_check_eq(b1.version, "1.0");
      do_check_false(b1.appDisabled);
      do_check_false(b1.userDisabled);
      do_check_true(b1.isActive);
      do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
      do_check_true(b1.hasResource("install.rdf"));
      do_check_false(b1.hasResource("bootstrap.js"));
      do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

      let dir = do_get_addon_root_uri(profileDir, "ab-CD@dictionaries.addons.mozilla.org");

      AddonManager.getAddonsWithOperationsByTypes(null, function(list) {
        do_check_eq(list.length, 0);

        run_test_2();
      });
    });
  });
}

// Tests that disabling doesn't require a restart
function run_test_2() {
  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    prepare_test({
      "ab-CD@dictionaries.addons.mozilla.org": [
        ["onDisabling", false],
        "onDisabled"
      ]
    });

    do_check_eq(b1.operationsRequiringRestart &
                AddonManager.OP_NEEDS_RESTART_DISABLE, 0);
    b1.userDisabled = true;
    ensure_test_completed();

    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_true(b1.userDisabled);
    do_check_false(b1.isActive);
    do_check_false(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
    do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(newb1) {
      do_check_neq(newb1, null);
      do_check_eq(newb1.version, "1.0");
      do_check_false(newb1.appDisabled);
      do_check_true(newb1.userDisabled);
      do_check_false(newb1.isActive);

      run_test_3();
    });
  });
}

// Test that restarting doesn't accidentally re-enable
function run_test_3() {
  shutdownManager();
  do_check_false(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  startupManager(false);
  do_check_false(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_true(b1.userDisabled);
    do_check_false(b1.isActive);

    run_test_4();
  });
}

// Tests that enabling doesn't require a restart
function run_test_4() {
  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    prepare_test({
      "ab-CD@dictionaries.addons.mozilla.org": [
        ["onEnabling", false],
        "onEnabled"
      ]
    });

    do_check_eq(b1.operationsRequiringRestart &
                AddonManager.OP_NEEDS_RESTART_ENABLE, 0);
    b1.userDisabled = false;
    ensure_test_completed();

    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
    do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(newb1) {
      do_check_neq(newb1, null);
      do_check_eq(newb1.version, "1.0");
      do_check_false(newb1.appDisabled);
      do_check_false(newb1.userDisabled);
      do_check_true(newb1.isActive);

      run_test_5();
    });
  });
}

// Tests that a restart shuts down and restarts the add-on
function run_test_5() {
  shutdownManager();
  do_check_false(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");
  startupManager(false);
  do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_false(isExtensionInAddonsList(profileDir, b1.id));

    run_test_7();
  });
}

// Tests that uninstalling doesn't require a restart
function run_test_7() {
  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    prepare_test({
      "ab-CD@dictionaries.addons.mozilla.org": [
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });

    do_check_eq(b1.operationsRequiringRestart &
                AddonManager.OP_NEEDS_RESTART_UNINSTALL, 0);
    b1.uninstall();

    check_test_7();
  });
}

function check_test_7() {
  ensure_test_completed();
  do_check_false(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    do_check_eq(b1, null);

    restartManager();

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(newb1) {
      do_check_eq(newb1, null);

      run_test_8();
    });
  });
}

// Test that a bootstrapped extension dropped into the profile loads properly
// on startup and doesn't cause an EM restart
function run_test_8() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("ab-CD@dictionaries.addons.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_dictionary"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir = dir.parent;
  dir.append("dictionaries");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  dir.append("ab-CD.dic");
  zip.extract("dictionaries/ab-CD.dic", dir);
  zip.close();

  startupManager(false);

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
    do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    run_test_9();
  });
}

// Test that items detected as removed during startup get removed properly
function run_test_9() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("ab-CD@dictionaries.addons.mozilla.org");
  dir.remove(true);
  startupManager(false);

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    do_check_eq(b1, null);
    do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    run_test_12();
  });
}


// Tests that bootstrapped extensions are correctly loaded even if the app is
// upgraded at the same time
function run_test_12() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("ab-CD@dictionaries.addons.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_dictionary"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir = dir.parent;
  dir.append("dictionaries");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  dir.append("ab-CD.dic");
  zip.extract("dictionaries/ab-CD.dic", dir);
  zip.close();

  startupManager(true);

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
    do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

    b1.uninstall();
    restartManager();

    run_test_16();
  });
}


// Tests that bootstrapped extensions don't get loaded when in safe mode
function run_test_16() {
  installAllFiles([do_get_addon("test_dictionary")], function() {
    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
      // Should have installed and started
      do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

      shutdownManager();

      // Should have stopped
      do_check_false(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

      gAppInfo.inSafeMode = true;
      startupManager(false);

      AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
        // Should still be stopped
        do_check_false(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
        do_check_false(b1.isActive);

        shutdownManager();
        gAppInfo.inSafeMode = false;
        startupManager(false);

        // Should have started
        do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

        AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
          b1.uninstall();

          run_test_17();
        });
      });
    });
  });
}

// Check that a bootstrapped extension in a non-profile location is loaded
function run_test_17() {
  shutdownManager();

  let dir = userExtDir.clone();
  dir.append("ab-CD@dictionaries.addons.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_dictionary"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir = dir.parent;
  dir.append("dictionaries");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  dir.append("ab-CD.dic");
  zip.extract("dictionaries/ab-CD.dic", dir);
  zip.close();

  startupManager();

  AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
    // Should have installed and started
    do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_true(b1.isActive);

    // From run_test_21
    dir = userExtDir.clone();
    dir.append("ab-CD@dictionaries.addons.mozilla.org");
    dir.remove(true);

    restartManager();

    run_test_23();
  });
}

// Tests that installing from a URL doesn't require a restart
function run_test_23() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  let url = "http://localhost:4444/addons/test_dictionary.xpi";
  AddonManager.getInstallForURL(url, function(install) {
    ensure_test_completed();

    do_check_neq(install, null);

    prepare_test({ }, [
      "onDownloadStarted",
      "onDownloadEnded"
    ], function() {
      do_check_eq(install.type, "dictionary");
      do_check_eq(install.version, "1.0");
      do_check_eq(install.name, "Test Dictionary");
      do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
      do_check_true(install.addon.hasResource("install.rdf"));
      do_check_false(install.addon.hasResource("bootstrap.js"));
      do_check_eq(install.addon.operationsRequiringRestart &
                  AddonManager.OP_NEEDS_RESTART_INSTALL, 0);
      do_check_not_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

      let addon = install.addon;
      prepare_test({
        "ab-CD@dictionaries.addons.mozilla.org": [
          ["onInstalling", false],
          "onInstalled"
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], function() {
        do_check_true(addon.hasResource("install.rdf"));
        check_test_23();
      });
    });
    install.install();
  }, "application/x-xpinstall");
}

function check_test_23() {
  AddonManager.getAllInstalls(function(installs) {
    // There should be no active installs now since the install completed and
    // doesn't require a restart.
    do_check_eq(installs.length, 0);

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
      do_check_neq(b1, null);
      do_check_eq(b1.version, "1.0");
      do_check_false(b1.appDisabled);
      do_check_false(b1.userDisabled);
      do_check_true(b1.isActive);
      do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
      do_check_true(b1.hasResource("install.rdf"));
      do_check_false(b1.hasResource("bootstrap.js"));
      do_check_in_crash_annotation("ab-CD@dictionaries.addons.mozilla.org", "1.0");

      let dir = do_get_addon_root_uri(profileDir, "ab-CD@dictionaries.addons.mozilla.org");

      AddonManager.getAddonsWithOperationsByTypes(null, function(list) {
        do_check_eq(list.length, 0);

        restartManager();
        AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
          b1.uninstall();
          restartManager();

          testserver.stop(run_test_25);
        });
      });
    });
  });
}

// Tests that updating from a bootstrappable add-on to a normal add-on calls
// the uninstall method
function run_test_25() {
  installAllFiles([do_get_addon("test_dictionary")], function() {
    HunspellEngine.listener = function(aEvent) {
      HunspellEngine.listener = null;
      do_check_eq(aEvent, "addDirectory");
      do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  
      installAllFiles([do_get_addon("test_dictionary_2")], function() {
        // Needs a restart to complete this so the old version stays running
        do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  
        AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
          do_check_neq(b1, null);
          do_check_eq(b1.version, "1.0");
          do_check_true(b1.isActive);
          do_check_true(hasFlag(b1.pendingOperations, AddonManager.PENDING_UPGRADE));
  
          restartManager();
  
          do_check_false(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));
  
          AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
            do_check_neq(b1, null);
            do_check_eq(b1.version, "2.0");
            do_check_true(b1.isActive);
            do_check_eq(b1.pendingOperations, AddonManager.PENDING_NONE);
  
            run_test_26();
          });
        });
      });
    };
  });
}

// Tests that updating from a normal add-on to a bootstrappable add-on calls
// the install method
function run_test_26() {
  installAllFiles([do_get_addon("test_dictionary")], function() {
    // Needs a restart to complete this
    do_check_false(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

    AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
      do_check_neq(b1, null);
      do_check_eq(b1.version, "2.0");
      do_check_true(b1.isActive);
      do_check_true(hasFlag(b1.pendingOperations, AddonManager.PENDING_UPGRADE));

      restartManager();

      do_check_true(HunspellEngine.isDictionaryEnabled("ab-CD.dic"));

      AddonManager.getAddonByID("ab-CD@dictionaries.addons.mozilla.org", function(b1) {
        do_check_neq(b1, null);
        do_check_eq(b1.version, "1.0");
        do_check_true(b1.isActive);
        do_check_eq(b1.pendingOperations, AddonManager.PENDING_NONE);

        HunspellEngine.deactivate();

        do_test_finished();
      });
    });
  });
}

