// |trace-test| TMFLAGS: full,fragprofile,treevis

function testRegExpTest() {
    var r = /abc/;
    var flag = false;
    for (var i = 0; i < 10; ++i)
        flag = r.test("abc");
    return flag;
}
assertEq(testRegExpTest(), true);
