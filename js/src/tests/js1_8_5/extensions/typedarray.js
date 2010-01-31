/* -*- Mode: js2; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor:
 *   Vladimir Vukicevic <vladimir@pobox.com>
 */

var gTestfile = 'template.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 532774;
var summary = 'js typed arrays (webgl arrays)';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
    enterFunc ('test');
    printBugNumber(BUGNUMBER);
    printStatus(summary);
    
    var TestPassCount = 0;
    var TestFailCount = 0;
    var TestTodoCount = 0;

    var TODO = 1;

    function check(fun, todo) {
        var thrown = null;
        var success = false;
        try {
            success = fun();
        } catch (x) {
            thrown = x;
        }

        if (thrown)
            success = false;

        if (todo) {
            TestTodoCount++;

            if (success) {
                var ex = new Error;
                print ("=== TODO but PASSED? ===");
                print (ex.stack);
                print ("========================");
            }

            return;
        }

        if (success) {
            TestPassCount++;
        } else {
            TestFailCount++;

            var ex = new Error;
            print ("=== FAILED ===");
            print (ex.stack);
            if (thrown) {
                print ("    threw exception:");
                print (thrown);
            }
            print ("==============");
        }
    }

    function checkThrows(fun, todo) {
        let thrown = false;
        try {
            fun();
        } catch (x) {
            thrown = true;
        }

        check(function() thrown, todo);
    }

    var buf;

    buf = new ArrayBuffer(100);
    check(function() buf);
    check(function() buf.byteLength == 100);

    buf.byteLength = 50;
    check(function() buf.byteLength == 100);

    var zerobuf = new ArrayBuffer(0);
    check(function() zerobuf);
    check(function() zerobuf.byteLength == 0);

    check(function() (new Int32Array(zerobuf)).length == 0);
    checkThrows(function() new Int32Array(zerobuf, 1));

    checkThrows(function() new ArrayBuffer(-100));
    // this is using js_ValueToECMAUInt32, which is giving 0 for "abc"
    checkThrows(function() new ArrayBuffer("abc"), TODO);

    var a = new Int32Array(20);
    check(function() a);
    check(function() a.length == 20);
    check(function() a.byteLength == 80);
    check(function() a.byteOffset == 0);
    check(function() a.buffer);
    check(function() a.buffer.byteLength == 80);

    var b = new Uint8Array(a.buffer, 4, 4);
    check(function() b);
    check(function() b.length == 4);
    check(function() b.byteLength == 4);
    check(function() a.buffer == b.buffer);

    b[0] = 0xaa;
    b[1] = 0xbb;
    b[2] = 0xcc;
    b[3] = 0xdd;

    check(function() a[0] == 0);
    check(function() a[1] != 0);
    check(function() a[2] == 0);

    buf = new ArrayBuffer(4);
    check(function() (new Int8Array(buf)).length == 4);
    check(function() (new Uint8Array(buf)).length == 4);
    check(function() (new Int16Array(buf)).length == 2);
    check(function() (new Uint16Array(buf)).length == 2);
    check(function() (new Int32Array(buf)).length == 1);
    check(function() (new Uint32Array(buf)).length == 1);
    check(function() (new Float32Array(buf)).length == 1);

    buf = new ArrayBuffer(5);
    check(function() buf);
    check(function() buf.byteLength == 5);

    check(function() new Int32Array(buf, 0, 1));
    checkThrows(function() new Int32Array(buf, 0));
    check(function() new Int8Array(buf, 0));

    check(function() (new Int8Array(buf, 3)).byteLength == 2);
    checkThrows(function() new Int8Array(buf, 500));
    checkThrows(function() new Int8Array(buf, 0, 50));
    checkThrows(function() new Float32Array(buf, 500));
    checkThrows(function() new Float32Array(buf, 0, 50));

    var sl = a.slice(5,10);
    check(function() sl.length == 5);
    check(function() sl.buffer == a.buffer);
    check(function() sl.byteLength == 20);
    check(function() sl.byteOffset == 20);

    check(function() a.slice(5,5).length == 0);
    check(function() a.slice(-5).length == 5);
    check(function() a.slice(-100).length == 20);
    check(function() a.slice(0, 2).length == 2);
    check(function() a.slice().length == a.length);
    check(function() a.slice(-7,-5).length == 2);
    check(function() a.slice(-5,-7).length == 0);
    check(function() a.slice(15).length == 5);

    a = new Uint8Array([0xaa, 0xbb, 0xcc]);
    check(function() a.length == 3);
    check(function() a.byteLength == 3);
    check(function() a[1] == 0xbb);

    // not sure if this is supposed to throw or to treat "foo"" as 0.
    checkThrows(function() new Int32Array([0xaa, "foo", 0xbb]), TODO);

    checkThrows(function() new Int32Array(-100));

    a = new Uint8Array(3);
    checkThrows(function() a[5000] = 0);
    checkThrows(function() a["hello"] = 0);
    checkThrows(function() a[-10] = 0);
    check(function() (a[0] = "10") && (a[0] == 10));

    print ("done");

    reportCompare(0, TestFailCount, "typed array test failures");

    exitFunc ('test');
}
