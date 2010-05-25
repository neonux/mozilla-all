/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that plugins exist and can be enabled and disabled.
var gID = null;

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager(1);
  AddonManager.addAddonListener(AddonListener);
  AddonManager.addInstallListener(InstallListener);

  run_test_1();
}

function get_unix_test_plugin() {
  let plugins = Components.classes["@mozilla.org/file/directory_service;1"].
                getService(Components.interfaces.nsIProperties).
                get("CurProcD", Components.interfaces.nsILocalFile);
  plugins.append("plugins");
  do_check_true(plugins.exists());
  let plugin = plugins.clone();
  // *nix plugin
  plugin.append("libnptest.so");
  if (plugin.exists())
    return plugin;

  return null;
}

// Tests that the test plugin exists
function run_test_1() {
  AddonManager.getAddonsByTypes("plugin", function(addons) {
    do_check_true(addons.length > 0);

    addons.forEach(function(p) {
      if (p.name == "Test Plug-in")
        gID = p.id;
    });

    do_check_neq(gID, null);

    AddonManager.getAddonByID(gID, function(p) {
      do_check_neq(p, null);
      do_check_eq(p.name, "Test Plug-in");
      do_check_eq(p.description, "Plug-in for testing purposes.");
      do_check_eq(p.creator, "");
      do_check_eq(p.version, "1.0.0.0");
      do_check_eq(p.type, "plugin");
      do_check_false(p.userDisabled);
      do_check_false(p.appDisabled);
      do_check_true(p.isActive);
      do_check_true(p.isCompatible);
      do_check_true(p.providesUpdatesSecurely);
      do_check_eq(p.blocklistState, 0);
      do_check_eq(p.permissions, AddonManager.PERM_CAN_DISABLE);
      do_check_eq(p.pendingOperations, 0);

      // Work around the fact that on Linux source builds, if we're using
      // symlinks (i.e. objdir), then Linux will see these as a different scope
      // to non-symlinks.
      let pluginLoc = get_unix_test_plugin();
      let pluginScope = AddonManager.SCOPE_APPLICATION;
      if (pluginLoc && pluginLoc.isSymlink())
        pluginScope = AddonManager.SCOPE_SYSTEM;
      do_check_eq(p.scope, pluginScope);
      do_check_true("isCompatibleWith" in p);
      do_check_true("findUpdates" in p);

      run_test_2(p);
    });
  });
}

// Tests that disabling a plugin works
function run_test_2(p) {
  let test = {};
  test[gID] = [
    ["onDisabling", false],
    "onDisabled"
  ];
  prepare_test(test);

  p.userDisabled = true;

  ensure_test_completed();

  do_check_true(p.userDisabled);
  do_check_false(p.appDisabled);
  do_check_false(p.isActive);

  AddonManager.getAddonByID(gID, function(p) {
    do_check_neq(p, null);
    do_check_true(p.userDisabled);
    do_check_false(p.appDisabled);
    do_check_false(p.isActive);
    do_check_eq(p.name, "Test Plug-in");

    run_test_3(p);
  });
}

// Tests that enabling a plugin works
function run_test_3(p) {
  let test = {};
  test[gID] = [
    ["onEnabling", false],
    "onEnabled"
  ];
  prepare_test(test);

  p.userDisabled = false;

  ensure_test_completed();

  do_check_false(p.userDisabled);
  do_check_false(p.appDisabled);
  do_check_true(p.isActive);

  AddonManager.getAddonByID(gID, function(p) {
    do_check_neq(p, null);
    do_check_false(p.userDisabled);
    do_check_false(p.appDisabled);
    do_check_true(p.isActive);
    do_check_eq(p.name, "Test Plug-in");

    do_test_finished();
  });
}
