// |jit-test| debug
// Test Debug.Object.prototype.callable.

var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
        assertEq(frame.arguments[0].callable, frame.arguments[1]);
        hits++;
    }
};

g.eval("function f(obj, iscallable) { debugger; }");

g.eval("f({}, false);");
g.eval("f(Function.prototype, true);");
g.eval("f(f, true);");
g.eval("f(Proxy.create({}), false);");
g.eval("f(Proxy.createFunction(f, f), true);");
g.f({}, false);
g.f(Function.prototype, true);
assertEq(hits, 7);
