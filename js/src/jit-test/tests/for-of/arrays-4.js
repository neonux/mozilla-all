// Nested for-of loops on the same array get distinct iterators.

var a = [1, 2, 3];
var s = '';
for (var x of a)
    for (var y of a)
        s += '' + x + y + ',';
assertEq(s, '11,12,13,21,22,23,31,32,33,');
