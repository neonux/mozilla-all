Function.prototype.prototype = function () {};
var x;
for (var i = 0; i < HOTLOOP + 2; i++)
    x = new Function.prototype;
assertEq(toString.call(x), "[object Object]");
assertEq(Object.getPrototypeOf(x), Function.prototype.prototype);
