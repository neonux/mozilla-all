// |jit-test| debug
// evalWithBindings code can assign to the bindings.
var g = newGlobal('new-compartment');
var dbg = new Debug(g);
var hits = 0;
dbg.hooks = {
    debuggerHandler: function (frame) {
	assertEq(frame.evalWithBindings("for (i = 0; i < 5; i++) {}  i;", {i: 10}).return, 5);
	hits++;
    }
};

g.eval("debugger;");
assertEq("i" in g, false);
assertEq(hits, 1);
