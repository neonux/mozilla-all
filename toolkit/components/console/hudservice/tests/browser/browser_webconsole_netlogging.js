/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Julian Viereck <jviereck@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/HUDService.jsm");

const TEST_NETWORK_REQUEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-network-request.html";

const TEST_DATA_JSON_CONTENT =
  '{ id: "test JSON data", myArray: [ "foo", "bar", "baz", "biff" ] }';

var hud;
var hudId;

function testOpenWebConsole()
{
  HUDService.activateHUDForContext(gBrowser.selectedTab);
  is(HUDService.displaysIndex().length, 1, "WebConsole was opened");

  hudId = HUDService.displaysIndex()[0];
  hud = HUDService.hudWeakReferences[hudId].get();

  testNetworkLogging();
}

function finishTest() {
  hud = null;
  hudId = null;

  let tab = gBrowser.selectedTab;
  HUDService.deactivateHUDForContext(tab);
  executeSoon(function() {
    gBrowser.removeCurrentTab();
    finish();
  });
}

function testNetworkLogging()
{
  var lastFinishedRequest = null;
  HUDService.lastFinishedRequestCallback =
    function requestDoneCallback(aHttpRequest)
    {
      lastFinishedRequest = aHttpRequest;
    }

  let browser = gBrowser.selectedBrowser;
  let loggingGen;
  // This generator function is used to step through the individual, async tests.
  function loggingGeneratorFunc() {
    browser.addEventListener("load", function onLoad () {
      browser.removeEventListener("load", onLoad, true);
      loggingGen.next();
    }, true);
    content.location = TEST_NETWORK_REQUEST_URI;
    yield;

    // Check if page load was logged correctly.
    let httpActivity = lastFinishedRequest;
    isnot(httpActivity, null, "Page load was logged");
    is(httpActivity.url, TEST_NETWORK_REQUEST_URI,
      "Logged network entry is page load");
    is(httpActivity.method, "GET", "Method is correct");
    is(httpActivity.request.body, undefined, "No request body sent");

    // TODO: Figure out why the following test is failing on linux (bug 588533).
    //
    // If not linux, then run the test. On Linux it always fails.
    if (navigator.platform.indexOf("Linux") != 0) {
      ok(httpActivity.response.body.indexOf("<!DOCTYPE HTML>") == 0,
        "Response body's beginning is okay");
    }

    // Start xhr-get test.
    browser.contentWindow.wrappedJSObject.testXhrGet(loggingGen);
    yield;

    // Use executeSoon here as the xhr callback calls loggingGen.next() before
    // the network observer detected that the request is completly done and the
    // HUDService.lastFinishedRequest is set. executeSoon solves that problem.
    executeSoon(function() {
      // Check if xhr-get test was successful.
      httpActivity = lastFinishedRequest;
      isnot(httpActivity, null, "testXhrGet() was logged");
      is(httpActivity.method, "GET", "Method is correct");
      is(httpActivity.request.body, undefined, "No request body was sent");
      is(httpActivity.response.body, TEST_DATA_JSON_CONTENT,
        "Response is correct");
      lastFinishedRequest = null;
      loggingGen.next();
    });
    yield;

    // Start xhr-post test.
    browser.contentWindow.wrappedJSObject.testXhrPost(loggingGen);
    yield;

    executeSoon(function() {
      // Check if xhr-post test was successful.
      httpActivity = lastFinishedRequest;
      isnot(httpActivity, null, "testXhrPost() was logged");
      is(httpActivity.method, "POST", "Method is correct");
      is(httpActivity.request.body, "Hello world!",
        "Request body was logged");
      is(httpActivity.response.body, TEST_DATA_JSON_CONTENT,
        "Response is correct");
      lastFinishedRequest = null
      loggingGen.next();
    });
    yield;

    // Start submit-form test. As the form is submitted, the page is loaded
    // again. Bind to the DOMContentLoaded event to catch when this is done.
    browser.addEventListener("load", function onLoad () {
      browser.removeEventListener("load", onLoad, true);
      loggingGen.next();
    }, true);
    browser.contentWindow.wrappedJSObject.testSubmitForm();
    yield;

    // Check if submitting the form was logged successful.
    httpActivity = lastFinishedRequest;
    isnot(httpActivity, null, "testSubmitForm() was logged");
    is(httpActivity.method, "POST", "Method is correct");
    isnot(httpActivity.request.body.indexOf(
      "Content-Type: application/x-www-form-urlencoded"), -1,
      "Content-Type is correct");
    isnot(httpActivity.request.body.indexOf(
      "Content-Length: 20"), -1, "Content-length is correct");
    isnot(httpActivity.request.body.indexOf(
      "name=foo+bar&age=144"), -1, "Form data is correct");
    ok(httpActivity.response.body.indexOf("<!DOCTYPE HTML>") == 0,
      "Response body's beginning is okay");

    lastFinishedRequest = null

    // All tests are done. Shutdown.
    browser = null;
    lastFinishedRequest = null;
    HUDService.lastFinishedRequestCallback = null;
    finishTest();
  }

  loggingGen = loggingGeneratorFunc();
  loggingGen.next();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();

  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    waitForFocus(testOpenWebConsole, content);
  }, true);

  content.location = "data:text/html,WebConsole network logging tests";
}
