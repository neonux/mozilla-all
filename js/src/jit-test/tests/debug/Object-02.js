// Debug.Object referents can be transparent wrappers of objects in the debugger compartment.

var g = newGlobal('new-compartment');
g.f = function (a, b) { return a + "/" + b; };
var dbg = Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
        var f = frame.eval("f").return;
        assertEq(f.call(null, "a", "b").return, "a/b");
        hits++;
    }
};
g.eval("debugger;");
assertEq(hits, 1);
