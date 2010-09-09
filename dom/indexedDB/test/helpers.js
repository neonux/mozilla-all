/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();
function runTest()
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  let uri = window.document.documentURIObject;
  Components.classes["@mozilla.org/permissionmanager;1"]
            .getService(Components.interfaces.nsIPermissionManager)
            .add(uri, "indexedDB",
                 Components.interfaces.nsIPermissionManager.ALLOW_ACTION);

  SimpleTest.waitForExplicitFinish();
  testGenerator.next();
}

function finishTest()
{
  SimpleTest.executeSoon(function() {
    testGenerator.close();
    SimpleTest.finish();
  });
}

function grabEventAndContinueHandler(event)
{
  testGenerator.send(event);
}

function continueToNextStep()
{
  SimpleTest.executeSoon(function() {
    testGenerator.next();
  });
}

function errorHandler(event)
{
  ok(false, "indexedDB error (" + event.code + "): " + event.message);
  finishTest();
}

function unexpectedSuccessHandler()
{
  ok(false, "Got success, but did not expect it!");
  finishTest();
}

function ExpectError(code)
{
  this._code = code;
}
ExpectError.prototype = {
  handleEvent: function(event)
  {
    is(this._code, event.code, "Expected error was thrown.");
    grabEventAndContinueHandler(event);
  }
};
