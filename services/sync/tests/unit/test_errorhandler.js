/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/status.js");

Svc.DefaultPrefs.set("registerEngines", "");
Cu.import("resource://services-sync/service.js");

function run_test() {
  initTestLogging("Trace");

  Log4Moz.repository.getLogger("Sync.Service").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Sync.SyncScheduler").level = Log4Moz.Level.Trace;

  run_next_test();
}

function sync_httpd_setup() {
  let global = new ServerWBO("global", {
    syncID: Service.syncID,
    storageVersion: STORAGE_VERSION,
    engines: {clients: {version: Clients.version,
                        syncID: Clients.syncID}}
  });
  let clientsColl = new ServerCollection({}, true);

  // Tracking info/collections.
  let collectionsHelper = track_collections_helper();
  let upd = collectionsHelper.with_updated_collection;

  let handler_401 = httpd_handler(401, "Unauthorized");
  return httpd_setup({
    "/1.1/johndoe/storage/meta/global": upd("meta", global.handler()),
    "/1.1/johndoe/info/collections": collectionsHelper.handler,
    "/1.1/johndoe/storage/crypto/keys":
      upd("crypto", (new ServerWBO("keys")).handler()),
    "/1.1/johndoe/storage/clients": upd("clients", clientsColl.handler()),

    "/1.1/janedoe/storage/meta/global": handler_401,
    "/1.1/janedoe/info/collections": handler_401,
  });
}

function setUp() {
  Service.username = "johndoe";
  Service.password = "ilovejane";
  Service.passphrase = "abcdeabcdeabcdeabcdeabcdea";
  Service.clusterURL = "http://localhost:8080/";

  generateNewKeys();
  let serverKeys = CollectionKeys.asWBO("crypto", "keys");
  serverKeys.encrypt(Service.syncKeyBundle);
  return serverKeys.upload(Service.cryptoKeysURL);
}

add_test(function test_401_logout() {
  let server = sync_httpd_setup();
  setUp();

  // By calling sync, we ensure we're logged in.
  Service.sync();
  do_check_eq(Status.sync, SYNC_SUCCEEDED);
  do_check_true(Service.isLoggedIn);

  // Make sync fail due to login rejected.
  Service.username = "janedoe";
  Service.sync();
  
  do_check_eq(Status.login, LOGIN_FAILED_LOGIN_REJECTED);
  do_check_false(Service.isLoggedIn);

  // Clean up.
  Service.startOver();
  server.stop(run_next_test);
});

add_test(function test_credentials_changed_logout() {
  let server = sync_httpd_setup();
  setUp();

  // By calling sync, we ensure we're logged in.
  Service.sync();
  do_check_eq(Status.sync, SYNC_SUCCEEDED);
  do_check_true(Service.isLoggedIn);

  // Make sync fail due to changed credentials. We simply re-encrypt
  // the keys with a different Sync Key, without changing the local one.
  let newSyncKeyBundle = new SyncKeyBundle(PWDMGR_PASSPHRASE_REALM, Service.username);
  newSyncKeyBundle.keyStr = "23456234562345623456234562";
  let keys = CollectionKeys.asWBO();
  keys.encrypt(newSyncKeyBundle);
  keys.upload(Service.cryptoKeysURL);
  Service.sync();
  
  do_check_eq(Status.sync, CREDENTIALS_CHANGED);
  do_check_false(Service.isLoggedIn);

  // Clean up.
  Service.startOver();
  server.stop(run_next_test);
});

add_test(function test_shouldIgnoreError() {
  Status.login = MASTER_PASSWORD_LOCKED;
  Status.sync = LOGIN_FAILED_NETWORK_ERROR;

  // Error ignored since master password locked.
  do_check_true(ErrorHandler.shouldIgnoreError());

  Status.login = LOGIN_FAILED_LOGIN_REJECTED;
  Status.sync = LOGIN_FAILED_NETWORK_ERROR

  // Error ignored due to network error.
  do_check_true(ErrorHandler.shouldIgnoreError());

  Status.login = LOGIN_FAILED_LOGIN_REJECTED;
  Status.sync = NO_SYNC_NODE_FOUND;

  // Error not ignored.
  do_check_false(ErrorHandler.shouldIgnoreError());

  run_next_test();
});
