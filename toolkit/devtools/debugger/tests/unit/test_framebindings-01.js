/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check a frame actor's bindings property.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-stack");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestGlobalClientAndResume(gClient, "test-stack", function(aResponse, aThreadClient) {
      gThreadClient = aThreadClient;
      test_pause_frame();
    });
  });
  do_test_pending();
}

function test_pause_frame()
{
  gThreadClient.addOneTimeListener("paused", function(aEvent, aPacket) {
    let bindings = aPacket.frame.environment.bindings;
    let args = bindings.arguments;
    let vars = bindings.variables;

    do_check_eq(args.length, 6);
    do_check_eq(args[0].aNumber.value, 42);
    do_check_eq(args[1].aBool.value, true);
    do_check_eq(args[2].aString.value, "nasu");
    do_check_eq(args[3].aNull.value.type, "null");
    do_check_eq(args[4].aUndefined.value.type, "undefined");
    do_check_eq(args[5].aObject.value.type, "object");
    do_check_eq(args[5].aObject.value.class, "Object");
    do_check_true(!!args[5].aObject.value.actor);

    do_check_eq(vars.a.value, 1);
    do_check_eq(vars.b.value, true);
    do_check_eq(vars.c.value.type, "object");
    do_check_eq(vars.c.value.class, "Object");
    do_check_true(!!vars.c.value.actor);

    gThreadClient.resume(function() {
      finishClient(gClient);
    });
  });

  gDebuggee.eval("(" + function() {
    function stopMe(aNumber, aBool, aString, aNull, aUndefined, aObject) {
      var a = 1;
      var b = true;
      var c = { a: "a" };
      debugger;
    };
    stopMe(42, true, "nasu", null, undefined, { foo: "bar" });
  } + ")()");
}
