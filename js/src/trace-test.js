/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/**
 * A number of the tests in this file depend on the setting of
 * HOTLOOP.  Define some constants up front, so they're easy to grep
 * for.
 */
// The HOTLOOP constant we depend on; only readable from our stats
// object in debug builds.
const haveTracemonkey = !!(this.tracemonkey)
const HOTLOOP = haveTracemonkey ? tracemonkey.HOTLOOP : 2;
// The loop count at which we trace
const RECORDLOOP = HOTLOOP;
// The loop count at which we run the trace
const RUNLOOP = HOTLOOP + 1;

var testName = null;
if ("arguments" in this && arguments.length > 0)
  testName = arguments[0];
var fails = [], passes=[];

function jitstatHandler(f)
{
    if (!haveTracemonkey) {
	return;
    }
    // XXXbz this is a nasty hack, but I can't figure out a way to
    // just use jitstats.tbl here
    f("recorderStarted");
    f("recorderAborted");
    f("traceCompleted");
    f("sideExitIntoInterpreter");
    f("typeMapMismatchAtEntry");
    f("returnToDifferentLoopHeader");
    f("traceTriggered");
    f("globalShapeMismatchAtEntry");
    f("treesTrashed");
    f("slotPromoted");
    f("unstableLoopVariable");
    f("breakLoopExits");
    f("returnLoopExits");
}

function test(f)
{
  if (!testName || testName == f.name) {
    // Collect our jit stats
    var localJITstats = {};
    jitstatHandler(function(prop, local, global) {
                     localJITstats[prop] = tracemonkey[prop];
                   });
    check(f.name, f(), f.expected, localJITstats, f.jitstats);
  }
}

function check(desc, actual, expected, oldJITstats, expectedJITstats)
{
  if (expected == actual) {
    var pass = true;
    jitstatHandler(function(prop) {
                     if (expectedJITstats && prop in expectedJITstats &&
                         expectedJITstats[prop] !=
                           tracemonkey[prop] - oldJITstats[prop]) {
                       pass = false;
                     }
                   });
    if (pass) {
      passes.push(desc);
      return print(desc, ": passed");
    }
  }
  fails.push(desc);
  var expectedStats = "";
  if (expectedJITstats) {
      jitstatHandler(function(prop) {
                       if (prop in expectedJITstats) {
                         if (expectedStats)
                           expectedStats += " ";
                         expectedStats +=
                           prop + ": " + expectedJITstats[prop];
                       }
                     });
  }
  var actualStats = "";
  if (expectedJITstats) {
      jitstatHandler(function(prop) {
                       if (prop in expectedJITstats) {
                         if (actualStats)
                           actualStats += " ";
                         actualStats += prop + ": " + (tracemonkey[prop]-oldJITstats[prop]);
                       }
                     });
  }
  print(desc, ": FAILED: expected", typeof(expected), "(", expected, ")",
	(expectedStats ? " [" + expectedStats + "] " : ""),
	"!= actual",
	typeof(actual), "(", actual, ")",
	(actualStats ? " [" + actualStats + "] " : ""));
}

function ifInsideLoop()
{
  var cond = true, intCond = 5, count = 0;
  for (var i = 0; i < 100; i++) {
    if (cond)
      count++;
    if (intCond)
      count++;
  }
  return count;
}
ifInsideLoop.expected = 200;
test(ifInsideLoop);

function bitwiseAnd_inner(bitwiseAndValue) {
  for (var i = 0; i < 60000; i++)
    bitwiseAndValue = bitwiseAndValue & i;
  return bitwiseAndValue;
}
function bitwiseAnd()
{
  return bitwiseAnd_inner(12341234);
}
bitwiseAnd.expected = 0;
test(bitwiseAnd);

if (!testName || testName == "bitwiseGlobal") {
  bitwiseAndValue = Math.pow(2,32);
  for (var i = 0; i < 60000; i++)
    bitwiseAndValue = bitwiseAndValue & i;
  check("bitwiseGlobal", bitwiseAndValue, 0);
}


function equalInt()
{
  var i1 = 55, one = 1, zero = 0, undef;
  var o1 = { }, o2 = { };
  var s = "5";
  var hits = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  for (var i = 0; i < 5000; i++) {
    if (i1 == 55) hits[0]++;
    if (i1 != 56) hits[1]++;
    if (i1 < 56)  hits[2]++;
    if (i1 > 50)  hits[3]++;
    if (i1 <= 60) hits[4]++;
    if (i1 >= 30) hits[5]++;
    if (i1 == 7)  hits[6]++;
    if (i1 != 55) hits[7]++;
    if (i1 < 30)  hits[8]++;
    if (i1 > 90)  hits[9]++;
    if (i1 <= 40) hits[10]++;
    if (i1 >= 70) hits[11]++;
    if (o1 == o2) hits[12]++;
    if (o2 != null) hits[13]++;
    if (s < 10) hits[14]++;
    if (true < zero) hits[15]++;
    if (undef > one) hits[16]++;
    if (undef < zero) hits[17]++;
  }
  return hits.toString();
}
equalInt.expected = "5000,5000,5000,5000,5000,5000,0,0,0,0,0,0,0,5000,5000,0,0,0";
test(equalInt);

var a;
function setelem()
{
  a = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  a = a.concat(a, a, a);
  var l = a.length;
  for (var i = 0; i < l; i++) {
    a[i] = i;
  }
  return a.toString();
}
setelem.expected = "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83";
test(setelem);

function getelem_inner(a)
{
  var accum = 0;
  var l = a.length;
  for (var i = 0; i < l; i++) {
    accum += a[i];
  }
  return accum;
}
function getelem()
{
  return getelem_inner(a);
}
getelem.expected = 3486;
test(getelem);

globalName = 907;
function name()
{
  var a = 0;
  for (var i = 0; i < 100; i++)
    a = globalName;
  return a;
}
name.expected = 907;
test(name);

var globalInt = 0;
if (!testName || testName == "globalGet") {
  for (var i = 0; i < 500; i++)
    globalInt = globalName + i;
  check("globalGet", globalInt, globalName + 499);
}

if (!testName || testName == "globalSet") {
  for (var i = 0; i < 500; i++)
    globalInt = i;
  check("globalSet", globalInt, 499);
}

function arith()
{
  var accum = 0;
  for (var i = 0; i < 100; i++) {
    accum += (i * 2) - 1;
  }
  return accum;
}
arith.expected = 9800;
test(arith);

function lsh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x1 << n;
  return r;
}
function lsh()
{
  return [lsh_inner(15),lsh_inner(55),lsh_inner(1),lsh_inner(0)];
}
lsh.expected = "32768,8388608,2,1";
test(lsh);

function rsh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x11010101 >> n;
  return r;
}
function rsh()
{
  return [rsh_inner(8),rsh_inner(5),rsh_inner(35),rsh_inner(-1)];
}
rsh.expected = "1114369,8914952,35659808,0";
test(rsh);

function ursh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = -55 >>> n;
  return r;
}
function ursh() {
  return [ursh_inner(8),ursh_inner(33),ursh_inner(0),ursh_inner(1)];
}
ursh.expected = "16777215,2147483620,4294967241,2147483620";
test(ursh);

function doMath_inner(cos)
{
    var s = 0;
    var sin = Math.sin;
    for (var i = 0; i < 200; i++)
        s = -Math.pow(sin(i) + cos(i * 0.75), 4);
    return s;
}
function doMath() {
  return doMath_inner(Math.cos);
}
doMath.expected = -0.5405549555611059;
test(doMath);

function fannkuch() {
   var count = Array(8);
   var r = 8;
   var done = 0;
   while (done < 40) {
      // write-out the first 30 permutations
      done += r;
      while (r != 1) { count[r - 1] = r; r--; }
      while (true) {
         count[r] = count[r] - 1;
         if (count[r] > 0) break;
         r++;
      }
   }
   return done;
}
fannkuch.expected = 41;
test(fannkuch);

function xprop()
{
  a = 0;
  for (var i = 0; i < 20; i++)
    a += 7;
  return a;
}
xprop.expected = 140;
test(xprop);

var a = 2;
function getprop_inner(o2)
{
  var o = {a:5};
  var t = this;
  var x = 0;
  for (var i = 0; i < 20; i++) {
    t = this;
    x += o.a + o2.a + this.a + t.a;
  }
  return x;
}
function getprop() {
  return getprop_inner({a:9});
}
getprop.expected = 360;
test(getprop);

function mod()
{
  var mods = [-1,-1,-1,-1];
  var a = 9.5, b = -5, c = 42, d = (1/0);
  for (var i = 0; i < 20; i++) {
    mods[0] = a % b;
    mods[1] = b % 1;
    mods[2] = c % d;
    mods[3] = c % a;
    mods[4] = b % 0;
  }
  return mods.toString();
}
mod.expected = "4.5,0,42,4,NaN";
test(mod);

function glob_f1() {
  return 1;
}
function glob_f2() {
  return glob_f1();
}
function call()
{
  var q1 = 0, q2 = 0, q3 = 0, q4 = 0, q5 = 0;
  var o = {};
  function f1() {
      return 1;
  }
  function f2(f) {
      return f();
  }
  o.f = f1;
  for (var i = 0; i < 100; ++i) {
      q1 += f1();
      q2 += f2(f1);
      q3 += glob_f1();
      q4 += o.f();
      q5 += glob_f2();
  }
  var ret = [q1, q2, q3, q4, q5];
  return ret;
}
call.expected =  "100,100,100,100,100";
test(call);

function setprop()
{
  var obj = { a:-1 };
  var obj2 = { b:-1, a:-1 };
  for (var i = 0; i < 20; i++) {
    obj2.b = obj.a = i;
  }
  return [obj.a, obj2.a, obj2.b].toString();
}
setprop.expected =  "19,-1,19";
test(setprop);

function testif() {
	var q = 0;
	for (var i = 0; i < 100; i++) {
		if ((i & 1) == 0)
			q++;
		else
			q--;
	}
    return q;
}
testif.expected = "0";
test(testif);

var globalinc = 0;
function testincops(n) {
  var i = 0, o = {p:0}, a = [0];
  n = 100;

  for (i = 0; i < n; i++);
  while (i-- > 0);
  for (i = 0; i < n; ++i);
  while (--i >= 0);

  for (o.p = 0; o.p < n; o.p++) globalinc++;
  while (o.p-- > 0) --globalinc;
  for (o.p = 0; o.p < n; ++o.p) ++globalinc;
  while (--o.p >= 0) globalinc--;

  ++i; // set to 0
  for (a[i] = 0; a[i] < n; a[i]++);
  while (a[i]-- > 0);
  for (a[i] = 0; a[i] < n; ++a[i]);
  while (--a[i] >= 0);

  return [++o.p, ++a[i], globalinc].toString();
}
testincops.expected = "0,0,0";
test(testincops);

function trees() {
  var i = 0, o = [0,0,0];
  for (i = 0; i < 100; ++i) {
    if ((i & 1) == 0) o[0]++;
    else if ((i & 2) == 0) o[1]++;
    else o[2]++;
  }
  return o;
}
trees.expected = "50,25,25";
test(trees);

function unboxint() {
    var q = 0;
    var o = [4];
    for (var i = 0; i < 100; ++i)
	q = o[0] << 1;
    return q;
}
unboxint.expected = "8";
test(unboxint);

function strings()
{
  var a = [], b = -1;
  var s = "abcdefghij", s2 = "a";
  var f = "f";
  var c = 0, d = 0, e = 0, g = 0;
  for (var i = 0; i < 10; i++) {
    a[i] = (s.substring(i, i+1) + s[i] + String.fromCharCode(s2.charCodeAt(0) + i)).concat(i) + i;
    if (s[i] == f)
      c++;
    if (s[i] != 'b')
      d++;
    if ("B" > s2)
      g++; // f already used
    if (s2 < "b")
      e++;
    b = s.length;
  }
  return a.toString() + b + c + d + e + g;
}
strings.expected = "aaa00,bbb11,ccc22,ddd33,eee44,fff55,ggg66,hhh77,iii88,jjj991019100";
test(strings);

function dependentStrings()
{
  var a = [];
  var t = "abcdefghijklmnopqrst";
  for (var i = 0; i < 10; i++) {
    var s = t.substring(2*i, 2*i + 2);
    a[i] = s + s.length;
  }
  return a.join("");
}
dependentStrings.expected = "ab2cd2ef2gh2ij2kl2mn2op2qr2st2";
test(dependentStrings);

function stringConvert()
{
  var a = [];
  var s1 = "F", s2 = "1.3", s3 = "5";
  for (var i = 0; i < 10; i++) {
    a[0] = 1 >> s1;
    a[1] = 10 - s2;
    a[2] = 15 * s3;
    a[3] = s3 | 32;
    a[4] = s2 + 60;
    // a[5] = 9 + s3;
    // a[6] = -s3;
    a[7] = s3 & "7";
    // a[8] = ~s3;
  }
  return a.toString();
}
stringConvert.expected = "1,8.7,75,37,1.360,,,5";
test(stringConvert);

function orTestHelper(a, b, n)
{
  var k = 0;
  for (var i = 0; i < n; i++) {
    if (a || b)
      k += i;
  }
  return k;
}

var orNaNTest1, orNaNTest2;

orNaNTest1 = new Function("return orTestHelper(NaN, NaN, 10);");
orNaNTest1.name = 'orNaNTest1';
orNaNTest1.expected = '0';
orNaNTest2 = new Function("return orTestHelper(NaN, 1, 10);");
orNaNTest2.name = 'orNaNTest2';
orNaNTest2.expected = '45';
test(orNaNTest1);
test(orNaNTest2);

function andTestHelper(a, b, n)
{
  var k = 0;
  for (var i = 0; i < n; i++) {
    if (a && b)
      k += i;
  }
  return k;
}

if (!testName || testName == "truthies") {
  (function () {
     var opsies   = ["||", "&&"];
     var falsies  = [null, undefined, false, NaN, 0, ""];
     var truthies = [{}, true, 1, 42, 1/0, -1/0, "blah"];
     var boolies  = [falsies, truthies];

     // The for each here should abort tracing, so that this test framework
     // relies only on the interpreter while the orTestHelper and andTestHelper
     //  functions get trace-JITed.
     for each (var op in opsies) {
       for (var i in boolies) {
	 for (var j in boolies[i]) {
           var x = uneval(boolies[i][j]);
           for (var k in boolies) {
             for (var l in boolies[k]) {
               var y = uneval(boolies[k][l]);
               var prefix = (op == "||") ? "or" : "and";
               var f = new Function("return " + prefix + "TestHelper(" + x + "," + y + ",10)");
               f.name = prefix + "Test(" + x + "," + y + ")";
               f.expected = eval(x + op + y) ? 45 : 0;
               test(f);
             }
           }
	 }
       }
     }
   })();
}

function nonEmptyStack1Helper(o, farble) {
    var a = [];
    var j = 0;
    for (var i in o)
        a[j++] = i;
    return a.join("");
}

function nonEmptyStack1() {
    return nonEmptyStack1Helper({a:1,b:2,c:3,d:4,e:5,f:6,g:7,h:8}, "hi");
}

nonEmptyStack1.expected = "abcdefgh";
test(nonEmptyStack1);

function nonEmptyStack2()
{
  var a = 0;
  for (var c in {a:1, b:2, c:3}) {
    for (var i = 0; i < 10; i++)
      a += i;
  }
  return String(a);
}
nonEmptyStack2.expected = "135";
test(nonEmptyStack2);

function arityMismatchMissingArg(arg)
{
  for (var a = 0, i = 1; i < 10000; i *= 2) {
    a += i;
  }
  return a;
}
arityMismatchMissingArg.expected = 16383;
test(arityMismatchMissingArg);

function arityMismatchExtraArg()
{
  return arityMismatchMissingArg(1, 2);
}
arityMismatchExtraArg.expected = 16383;
test(arityMismatchExtraArg);

function MyConstructor(i)
{
  this.i = i;
}
MyConstructor.prototype.toString = function() {return this.i + ""};

function newTest()
{
  var a = [];
  for (var i = 0; i < 10; i++)
    a[i] = new MyConstructor(i);
  return a.join("");
}
newTest.expected = "0123456789";
test(newTest);

// The following functions use a delay line of length 2 to change the value
// of the callee without exiting the traced loop. This is obviously tuned to
// match the current HOTLOOP setting of 2.
function shapelessArgCalleeLoop(f, g, h, a)
{
  for (var i = 0; i < 10; i++) {
    f(i, a);
    f = g;
    g = h;
  }
}

function shapelessVarCalleeLoop(f0, g, h, a)
{
  var f = f0;
  for (var i = 0; i < 10; i++) {
    f(i, a);
    f = g;
    g = h;
  }
}

function shapelessLetCalleeLoop(f0, g, h, a)
{
  for (var i = 0; i < 10; i++) {
    let f = f0;
    f(i, a);
    f = g;
    g = h;
  }
}

function shapelessUnknownCalleeLoop(n, f, g, h, a)
{
  for (var i = 0; i < 10; i++) {
    (n || f)(i, a);
    f = g;
    g = h;
  }
}

function shapelessCalleeTest()
{
  var a = [];

  var helper = function (i, a) a[i] = i;
  shapelessArgCalleeLoop(helper, helper, function (i, a) a[i] = -i, a);

  helper = function (i, a) a[10 + i] = i;
  shapelessVarCalleeLoop(helper, helper, function (i, a) a[10 + i] = -i, a);

  helper = function (i, a) a[20 + i] = i;
  shapelessLetCalleeLoop(helper, helper, function (i, a) a[20 + i] = -i, a);

  helper = function (i, a) a[30 + i] = i;
  shapelessUnknownCalleeLoop(null, helper, helper, function (i, a) a[30 + i] = -i, a);

  try {
    helper = {hack: 42};
    shapelessUnknownCalleeLoop(null, helper, helper, helper, a);
  } catch (e) {
    if (e + "" != "TypeError: f is not a function")
      print("shapelessUnknownCalleeLoop: unexpected exception " + e);
  }
  return a.join("");
}
shapelessCalleeTest.expected = "01-2-3-4-5-6-7-8-901-2-3-4-5-6-7-8-9012345678901-2-3-4-5-6-7-8-9";
test(shapelessCalleeTest);

function typeofTest()
{
  var values = ["hi", "hi", "hi", null, 5, 5.1, true, undefined, /foo/, typeofTest, [], {}], types = [];
  for (var i = 0; i < values.length; i++)
    types[i] = typeof values[i];
  return types.toString();
}
typeofTest.expected = "string,string,string,object,number,number,boolean,undefined,object,function,object,object";
test(typeofTest);

function joinTest()
{
  var s = "";
  var a = [];
  for (var i = 0; i < 8; i++)
    a[i] = [String.fromCharCode(97 + i)];
  for (i = 0; i < 8; i++) {
    for (var j = 0; j < 8; j++)
      a[i][1 + j] = j;
  }
  for (i = 0; i < 8; i++)
    s += a[i].join(",");
  return s;
}
joinTest.expected = "a,0,1,2,3,4,5,6,7b,0,1,2,3,4,5,6,7c,0,1,2,3,4,5,6,7d,0,1,2,3,4,5,6,7e,0,1,2,3,4,5,6,7f,0,1,2,3,4,5,6,7g,0,1,2,3,4,5,6,7h,0,1,2,3,4,5,6,7";
test(joinTest);

function arity1(x)
{
  return (x == undefined) ? 1 : 0;
}
function missingArgTest() {
  var q;
  for (var i = 0; i < 10; i++) {
    q = arity1();
  }
  return q;
}
missingArgTest.expected = "1"
test(missingArgTest);

JSON = function () {
    return {
        stringify: function stringify(value, whitelist) {
            switch (typeof(value)) {
              case "object":
                return value.constructor.name;
            }
        }
    };
}();

function missingArgTest2() {
  var testPairs = [
    ["{}", {}],
    ["[]", []],
    ['{"foo":"bar"}', {"foo":"bar"}],
  ]

  var a = [];
  for (var i=0; i < testPairs.length; i++) {
    var s = JSON.stringify(testPairs[i][1])
    a[i] = s;
  }
  return a.join(",");
}
missingArgTest2.expected = "Object,Array,Object";
test(missingArgTest2);

function deepForInLoop() {
  // NB: the number of props set in C is arefully tuned to match HOTLOOP = 2.
  function C(){this.p = 1, this.q = 2}
  C.prototype = {p:1, q:2, r:3, s:4, t:5};
  var o = new C;
  var j = 0;
  var a = [];
  for (var i in o)
    a[j++] = i;
  return a.join("");
}
deepForInLoop.expected = "pqrst";
test(deepForInLoop);

function nestedExit(x) {
    var q = 0;
    for (var i = 0; i < 10; ++i)
	if (x)
	    ++q;
}
function nestedExitLoop() {
    for (var j = 0; j < 10; ++j)
	nestedExit(j < 7);
    return "ok";
}
nestedExitLoop.expected = "ok";
test(nestedExitLoop);

function bitsinbyte(b) {
    var m = 1, c = 0;
    while(m<0x100) {
        if(b & m) c++;
        m <<= 1;
    }
    return 1;
}
function TimeFunc(func) {
    var x,y;
    for(var y=0; y<256; y++) func(y);
}
function nestedExit2() {
    TimeFunc(bitsinbyte);
    return "ok";
}
nestedExit2.expected = "ok";
test(nestedExit2);

function parsingNumbers() {
    var s1 = "123";
    var s1z = "123zzz";
    var s2 = "123.456";
    var s2z = "123.456zzz";

    var e1 = 123;
    var e2 = 123.456;

    var r1, r1z, r2, r2z;

    for (var i = 0; i < 10; i++) {
	r1 = parseInt(s1);
	r1z = parseInt(s1z);
	r2 = parseFloat(s2);
	r2z = parseFloat(s2z);
    }

    if (r1 == e1 && r1z == e1 && r2 == e2 && r2z == e2)
	return "ok";
    return "fail";
}
parsingNumbers.expected = "ok";
test(parsingNumbers);

function matchInLoop() {
    var k = "hi";
    for (var i = 0; i < 10; i++) {
        var result = k.match(/hi/) != null;
    }
    return result;
}
matchInLoop.expected = true;
test(matchInLoop);

function deep1(x) {
    if (x > 90)
	return 1;
    return 2;
}
function deep2() {
    for (var i = 0; i < 100; ++i)
	deep1(i);
    return "ok";
}
deep2.expected = "ok";
test(deep2);

var merge_type_maps_x = 0, merge_type_maps_y = 0;
function merge_type_maps() {
    for (merge_type_maps_x = 0; merge_type_maps_x < 50; ++merge_type_maps_x)
        if ((merge_type_maps_x & 1) == 1)
	    ++merge_type_maps_y;
    return [merge_type_maps_x,merge_type_maps_y].join(",");
}
merge_type_maps.expected = "50,25";
test(merge_type_maps)

function inner_double_outer_int() {
    function f(i) {
	for (var m = 0; m < 20; ++m)
	    for (var n = 0; n < 100; n += i)
		;
	return n;
    }
    return f(.5);
}
inner_double_outer_int.expected = "100";
test(inner_double_outer_int);

function newArrayTest()
{
  var a = [];
  for (var i = 0; i < 10; i++)
    a[i] = new Array();
  return a.map(function(x) x.length).toString();
}
newArrayTest.expected="0,0,0,0,0,0,0,0,0,0";
test(newArrayTest);

function stringSplitTest()
{
  var s = "a,b"
  var a = null;
  for (var i = 0; i < 10; ++i)
    a = s.split(",");
  return a.join();
}
stringSplitTest.expected="a,b";
test(stringSplitTest);

function stringSplitIntoArrayTest()
{
  var s = "a,b"
  var a = [];
  for (var i = 0; i < 10; ++i)
    a[i] = s.split(",");
  return a.join();
}
stringSplitIntoArrayTest.expected="a,b,a,b,a,b,a,b,a,b,a,b,a,b,a,b,a,b,a,b";
test(stringSplitIntoArrayTest);

function forVarInWith() {
    function foo() ({notk:42});
    function bar() ({p:1, q:2, r:3, s:4, t:5});
    var o = foo();
    var a = [];
    with (o) {
        for (var k in bar())
            a[a.length] = k;
    }
    return a.join("");
}
forVarInWith.expected = "pqrst";
test(forVarInWith);

function inObjectTest() {
    var o = {p: 1, q: 2, r: 3, s: 4, t: 5};
    var r = 0;
    for (var i in o) {
        if (!(i in o))
            break;
        if ((i + i) in o)
            break;
        ++r;
    }
    return r;
}
inObjectTest.expected = 5;
test(inObjectTest);

function inArrayTest() {
    var a = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
    for (var i = 0; i < a.length; i++) {
        if (!(i in a))
            break;
    }
    return i;
}
inArrayTest.expected = 10;
test(inArrayTest);

function innerLoopIntOuterDouble() {
    var n = 10000, i=0, j=0, count=0, limit=0;
    for (i = 1; i <= n; ++i) {
	limit = i * 1;
	for (j = 0; j < limit; ++j) {
	    ++count;
	}
    }
    return "" + count;
}
innerLoopIntOuterDouble.expected="50005000";
test(innerLoopIntOuterDouble);

function outerline(){
    var i=0;
    var j=0;

    for (i = 3; i<= 100000; i+=2)
	for (j = 3; j < 1000; j+=2)
	    if ((i & 1) == 1)
		break;
    return "ok";
}
outerline.expected="ok";
test(outerline);

function addAccumulations(f) {
  var a = f();
  var b = f();
  return a() + b();
}

function loopingAccumulator() {
  var x = 0;
  return function () {
    for (var i = 0; i < 10; ++i) {
      ++x;
    }
    return x;
  }
}

function testLoopingAccumulator() {
	var x = addAccumulations(loopingAccumulator);
	return x;
}
testLoopingAccumulator.expected = 20;
test(testLoopingAccumulator);

function testBranchingLoop() {
  var x = 0;
  for (var i=0; i < 100; ++i) {
    if (i == 51) {
      x += 10;
    }
    x++;
  }
  return x;
}
testBranchingLoop.expected = 110;
test(testBranchingLoop);

function testBranchingUnstableLoop() {
  var x = 0;
  for (var i=0; i < 100; ++i) {
    if (i == 51) {
      x += 10.1;
    }
    x++;
  }
  return x;
}
testBranchingUnstableLoop.expected = 110.1;
test(testBranchingUnstableLoop);

function testBranchingUnstableLoopCounter() {
  var x = 0;
  for (var i=0; i < 100; ++i) {
    if (i == 51) {
      i += 1.1;
    }
    x++;    
  }
  return x;
}
testBranchingUnstableLoopCounter.expected = 99;
test(testBranchingUnstableLoopCounter);


function testBranchingUnstableObject() {
  var x = {s: "a"};
  var t = "";
  for (var i=0; i < 100; ++i) {
      if (i == 51)
      {
        x.s = 5;
      }
      t += x.s;
  }
  return t.length;
}
testBranchingUnstableObject.expected = 100;
test(testBranchingUnstableObject);

function testArrayDensityChange() {
  var x = [];
  var count = 0;
  for (var i=0; i < 100; ++i) {
    x[i] = "asdf";
  }
  for (var i=0; i < x.length; ++i) {
      if (i == 51)
      {
        x[199] = "asdf";
      }
      if (x[i])
        count += x[i].length;
  }
  return count;
}
testArrayDensityChange.expected = 404;
test(testArrayDensityChange);

function testDoubleToStr() {
    var x = 0.0;
    var y = 5.5;
    for (var i = 0; i < 200; i++) {
       x += parseFloat(y.toString());
    }
    return x;
}
testDoubleToStr.expected = 5.5*200;
test(testDoubleToStr);

function testNumberToString() {
    var x = new Number(0);
    for (var i = 0; i < 4; i++)
        x.toString();
}
test(testNumberToString);

function testDecayingInnerLoop() {
    var i, j, k = 10;
    for (i = 0; i < 5000; ++i) {
	for (j = 0; j < k; ++j);
	--k;
    }
    return i;
}
testDecayingInnerLoop.expected = 5000;
test(testDecayingInnerLoop);

function testContinue() {
    var i;
    var total = 0;
    for (i = 0; i < 20; ++i) {
	if (i == 11)
	    continue;
	total++;
    }
    return total;
}
testContinue.expected = 19;
test(testContinue);

function testContinueWithLabel() {
    var i = 0;
    var j = 20;
    checkiandj :
    while (i<10) {
	i+=1;
	checkj :
	while (j>10) {
	    j-=1;
	    if ((j%2)==0)
		continue checkj;
	}   
    }
    return i + j;
}
testContinueWithLabel.expected = 20;
test(testContinueWithLabel);

function testDivision() {
    var a = 32768;
    var b;
    while (b !== 1) {
	b = a / 2;
	a = b;
    }
    return a;
}
testDivision.expected = 1;
test(testDivision);

function testDivisionFloat() {
    var a = 32768.0;
    var b;
    while (b !== 1) {
	b = a / 2.0;
	a = b;
    }
    return a === 1.0;
}
testDivisionFloat.expected = true;
test(testDivisionFloat);

function testToUpperToLower() {
    var s = "Hello", s1, s2;
    for (i = 0; i < 100; ++i) {
	s1 = s.toLowerCase();
	s2 = s.toUpperCase();
    }
    return s1 + s2;
}
testToUpperToLower.expected = "helloHELLO";
test(testToUpperToLower);

function testReplace2() {
    var s = "H e l l o", s1;
    for (i = 0; i < 100; ++i) {
	s1 = s.replace(" ", "");
    }
    return s1;
}
testReplace2.expected = "He l l o";
test(testReplace2);

function testBitwise() {
    var x = 10000;
    var y = 123456;
    var z = 987234;
    for (var i = 0; i < 50; i++) {
        x = x ^ y;
        y = y | z;
        z = ~x;
    }
    return x + y + z;
}
testBitwise.expected = -1298;
test(testBitwise);

function testSwitch() {
    var x = 0;
    var ret = 0;
    for (var i = 0; i < 100; ++i) {
        switch (x) {
            case 0:
                ret += 1;
                break;
            case 1:
                ret += 2;
                break;
            case 2:
                ret += 3;
                break;
            case 3:
                ret += 4;
                break;
            default:
                x = 0;
        }
        x++;
    }
    return ret;
}
testSwitch.expected = 226;
test(testSwitch);

function testSwitchString() {
    var x = "asdf";
    var ret = 0;
    for (var i = 0; i < 100; ++i) {
        switch (x) {
        case "asdf":
            x = "asd";
            ret += 1;
            break;
        case "asd":
            x = "as";
            ret += 2;
            break;
        case "as":
            x = "a";
            ret += 3;
            break;
        case "a":
            x = "foo";
            ret += 4;
            break;
        default:
            x = "asdf";
        }
    }
    return ret;
}
testSwitchString.expected = 200;
test(testSwitchString);

function testNegZero1Helper(z) {
    for (let j = 0; j < 5; ++j) { z = -z; }
    return Math.atan2(0, -0) == Math.atan2(0, z);
}

var testNegZero1 = function() { return testNegZero1Helper(0); }
testNegZero1.expected = true;
testNegZero1.name = 'testNegZero1';
testNegZero1Helper(1);
test(testNegZero1);

// No test case, just make sure this doesn't assert. 
function testNegZero2() {
    var z = 0;
    for (let j = 0; j < 5; ++j) { ({p: (-z)}); }
}
testNegZero2();

function testConstSwitch() {
    var x;
    for (var j=0;j<5;++j) { switch(1.1) { case NaN: case 2: } x = 2; }
    return x;
}
testConstSwitch.expected = 2;
test(testConstSwitch);

function testConstSwitch2() {
    var x;
    for (var j = 0; j < 4; ++j) { switch(0/0) { } }
    return "ok";
}
testConstSwitch2.expected = "ok";
test(testConstSwitch2);

function testConstIf() {
    var x;
    for (var j=0;j<5;++j) { if (1.1 || 5) { } x = 2;}
    return x;
}
testConstIf.expected = 2;
test(testConstIf);

function testTypeofHole() {
  var a = new Array(6);
  a[5] = 3;
  for (var i = 0; i < 6; ++i)
    a[i] = typeof a[i];
  return a.join(",");
}
testTypeofHole.expected = "undefined,undefined,undefined,undefined,undefined,number"
test(testTypeofHole);

function testNativeLog() {
  var a = new Array(5);
  for (var i = 0; i < 5; i++) {
    a[i] = Math.log(Math.pow(Math.E, 10));
  }
  return a.join(",");
}
testNativeLog.expected = "10,10,10,10,10";
test(testNativeLog);

function test_JSOP_ARGSUB() {
    function f0() { return arguments[0]; }
    function f1() { return arguments[1]; }
    function f2() { return arguments[2]; }
    function f3() { return arguments[3]; }
    function f4() { return arguments[4]; }
    function f5() { return arguments[5]; }
    function f6() { return arguments[6]; }
    function f7() { return arguments[7]; }
    function f8() { return arguments[8]; }
    function f9() { return arguments[9]; }
    var a = [];
    for (var i = 0; i < 10; i++) {
        a[0] = f0('a');
        a[1] = f1('a','b');
        a[2] = f2('a','b','c');
        a[3] = f3('a','b','c','d');
        a[4] = f4('a','b','c','d','e');
        a[5] = f5('a','b','c','d','e','f');
        a[6] = f6('a','b','c','d','e','f','g');
        a[7] = f7('a','b','c','d','e','f','g','h');
        a[8] = f8('a','b','c','d','e','f','g','h','i');
        a[9] = f9('a','b','c','d','e','f','g','h','i','j');
    }
    return a.join("");
}
test_JSOP_ARGSUB.expected = "abcdefghij";
test(test_JSOP_ARGSUB);

function test_JSOP_ARGCNT() {
    function f0() { return arguments.length; }
    function f1() { return arguments.length; }
    function f2() { return arguments.length; }
    function f3() { return arguments.length; }
    function f4() { return arguments.length; }
    function f5() { return arguments.length; }
    function f6() { return arguments.length; }
    function f7() { return arguments.length; }
    function f8() { return arguments.length; }
    function f9() { return arguments.length; }
    var a = [];
    for (var i = 0; i < 10; i++) {
        a[0] = f0('a');
        a[1] = f1('a','b');
        a[2] = f2('a','b','c');
        a[3] = f3('a','b','c','d');
        a[4] = f4('a','b','c','d','e');
        a[5] = f5('a','b','c','d','e','f');
        a[6] = f6('a','b','c','d','e','f','g');
        a[7] = f7('a','b','c','d','e','f','g','h');
        a[8] = f8('a','b','c','d','e','f','g','h','i');
        a[9] = f9('a','b','c','d','e','f','g','h','i','j');
    }
    return a.join(",");
}
test_JSOP_ARGCNT.expected = "1,2,3,4,5,6,7,8,9,10";
test(test_JSOP_ARGCNT);

function testNativeMax() {
    var out = [], k;
    for (var i = 0; i < 5; ++i) {
        k = Math.max(k, i);
    }
    out.push(k);

    k = 0;
    for (var i = 0; i < 5; ++i) {
        k = Math.max(k, i);
    }
    out.push(k);

    for (var i = 0; i < 5; ++i) {
        k = Math.max(0, -0);
    }
    out.push((1 / k) < 0);
    return out.join(",");
}
testNativeMax.expected = "NaN,4,false";
test(testNativeMax);

function testFloatArrayIndex() {
    var a = [];
    for (var i = 0; i < 10; ++i) {
	a[3] = 5;
	a[3.5] = 7;
    }
    return a[3] + "," + a[3.5];
}
testFloatArrayIndex.expected = "5,7";
test(testFloatArrayIndex);

function testStrict() {
    var n = 10, a = [];
    for (var i = 0; i < 10; ++i) {
	a[0] = (n === 10);
	a[1] = (n !== 10);
	a[2] = (n === null);
	a[3] = (n == null);
    }
    return a.join(",");
}
testStrict.expected = "true,false,false,false";
test(testStrict);

function testSetPropNeitherMissNorHit() {
    for (var j = 0; j < 5; ++j) { if (({}).__proto__ = 1) { } }
    return "ok";
}
testSetPropNeitherMissNorHit.expected = "ok";
test(testSetPropNeitherMissNorHit);

function testPrimitiveConstructorPrototype() {
    var f = function(){};
    f.prototype = false;
    for (let j=0;j<5;++j) { new f; }
    return "ok";
}    
testPrimitiveConstructorPrototype.expected = "ok";
test(testPrimitiveConstructorPrototype);

function testSideExitInConstructor() {
    var FCKConfig = {};
    FCKConfig.CoreStyles =
	{
	    'Bold': { },
	    'Italic': { },
	    'FontFace': { },
	    'Size' :
	    {
		Overrides: [ ]
	    },

	    'Color' :
	    {
		Element: '',
		Styles: {  },
		Overrides: [  ]
	    },
	    'BackColor': {
		Element : '',
		Styles : { 'background-color' : '' }
	    },
	    
	};
    var FCKStyle = function(A) {
	A.Element;
    };
    
    var pass = true;
    for (var s in FCKConfig.CoreStyles) {
	var x = new FCKStyle(FCKConfig.CoreStyles[s]);
	if (!x) pass = false;
    }
    return pass;
}
testSideExitInConstructor.expected = true;
test(testSideExitInConstructor);

function testNot() {
    var a = new Object(), b = null, c = "foo", d = "", e = 5, f = 0, g = 5.5, h = -0, i = true, j = false, k = undefined;
    var r;
    for (var i = 0; i < 10; ++i) {
	r = [!a, !b, !c, !d, !e, !f, !g, !h, !i, !j, !k];
    }
    return r.join(",");
}
testNot.expected = "false,true,false,true,false,true,false,true,false,true,true";
test(testNot);

function doTestDifferingArgc(a, b)
{
    var k = 0;
    for (var i = 0; i < 10; i++)
    {
        k += i;
    }
    return k;
}
function testDifferingArgc()
{
    var x = 0;
    x += doTestDifferingArgc(1, 2);
    x += doTestDifferingArgc(1);
    x += doTestDifferingArgc(1, 2, 3);
    return x;
}
testDifferingArgc.expected = 45*3;
test(testDifferingArgc);

function doTestMoreArgcThanNargs()
{
    var x = 0;
    for (var i = 0; i < 10; i++)
    {
        x = x + arguments[3];
    }
    return x;
}
function testMoreArgcThanNargs()
{
    return doTestMoreArgcThanNargs(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}
testMoreArgcThanNargs.expected = 4*10;
test(testMoreArgcThanNargs);

// Test stack reconstruction after a nested exit
function testNestedExitStackInner(j, counter) {
  ++counter;
  var b = 0;
  for (var i = 1; i <= RUNLOOP; i++) {
    ++b;
    var a;
    // Make sure that once everything has been traced we suddenly switch to
    // a different control flow the first time we run the outermost tree,
    // triggering a side exit.
    if (j < RUNLOOP)
      a = 1;
    else
      a = 0;
    ++b;
    b += a;
  }
  return counter + b;
}
function testNestedExitStackOuter() {
  var counter = 0;
  for (var j = 1; j <= RUNLOOP; ++j) {
    for (var k = 1; k <= RUNLOOP; ++k) {
      counter = testNestedExitStackInner(j, counter);
    }
  }
  return counter;
}
testNestedExitStackOuter.expected = 81;
testNestedExitStackOuter.jitstats = {
    recorderStarted: 5,
    recorderAborted: 2,
    traceTriggered: 9
};
test(testNestedExitStackOuter);

function testHOTLOOPSize() {
    return HOTLOOP > 1;
}
testHOTLOOPSize.expected = true;
test(testHOTLOOPSize);

function testMatchStringObject() {
    var a = new String("foo");
    var b;
    for (i = 0; i < 300; i++) {
	b = a.match(/bar/);
    }
    return b;
}
testMatchStringObject.expected = null;
test(testMatchStringObject);

function innerSwitch(k)
{
    var m = 0;

    switch (k)
    {
    case 0:
        m = 1;
        break;
    }

    return m;
}
function testInnerSwitchBreak()
{
    var r = new Array(5);
    for (var i = 0; i < 5; i++)
    {
        r[i] = innerSwitch(0);
    }

    return r.join(",");
}
testInnerSwitchBreak.expected = "1,1,1,1,1";
test(testInnerSwitchBreak);

function testArrayNaNIndex() 
{
    for (var j = 0; j < 4; ++j) { [this[NaN]]; }
    for (var j = 0; j < 5; ++j) { if([1][-0]) { } }
    return "ok";
}
testArrayNaNIndex.expected = "ok";
test(testArrayNaNIndex);

function innerTestInnerMissingArgs(a,b,c,d)
{
        if (a) {
        } else {
        }
}
function doTestInnerMissingArgs(k)
{
    for (i = 0; i < 10; i++) {
        innerTestInnerMissingArgs(k);
    }
}
function testInnerMissingArgs()
{
    doTestInnerMissingArgs(1);
    doTestInnerMissingArgs(0);
    return 1;
}
testInnerMissingArgs.expected = 1;  //Expected: that we don't crash.
test(testInnerMissingArgs);

function regexpLastIndex()
{
    var n = 0;
    var re = /hi/g;
    var ss = " hi hi hi hi hi hi hi hi hi hi";
    for (var i = 0; i < 10; i++) {
        // re.exec(ss);
        n += (re.lastIndex > 0) ? 3 : 0;
        re.lastIndex = 0;
    }
    return n;
}
regexpLastIndex.expected = 0; // 30;
test(regexpLastIndex);

function testHOTLOOPCorrectness() {
    var b = 0;
    for (var i = 0; i < HOTLOOP; ++i) {
	++b;
    }
    return b;
}
testHOTLOOPCorrectness.expected = HOTLOOP;
testHOTLOOPCorrectness.jitstats = {
    recorderStarted: 1,
    recorderAborted: 0,
    traceTriggered: 0
};
// Change the global shape right before doing the test
this.testHOTLOOPCorrectnessVar = 1;
test(testHOTLOOPCorrectness);

function testRUNLOOPCorrectness() {
    var b = 0;
    for (var i = 0; i < RUNLOOP; ++i) {
	++b;
    }
    return b;
}
testRUNLOOPCorrectness.expected = RUNLOOP;
testRUNLOOPCorrectness.jitstats = {
    recorderStarted: 1,
    recorderAborted: 0,
    traceTriggered: 1
};
// Change the global shape right before doing the test
this.testRUNLOOPCorrectnessVar = 1;
test(testRUNLOOPCorrectness);

function testDateNow() {
    // Accessing global.Date for the first time will change the global shape,
    // so do it before the loop starts; otherwise we have to loop an extra time
    // to pick things up.
    var time = Date.now();
    for (var j = 0; j < RUNLOOP; ++j) {
	time = Date.now();
    }
    return "ok";
}
testDateNow.expected = "ok";
testDateNow.jitstats = {
    recorderStarted: 1,
    recorderAborted: 0,
    traceTriggered: 1
};
test(testDateNow);

function testINITELEM() 
{
    var x;
    for (var i = 0; i < 10; ++i)
	x = { 0: 5, 1: 5 };    
    return x[0] + x[1];
}
testINITELEM.expected = 10;
test(testINITELEM);

function testUndefinedBooleanCmp() 
{
    var t = true, f = false, x = [];
    for (var i = 0; i < 10; ++i) {
	x[0] = t == undefined;
	x[1] = t != undefined;
	x[2] = t === undefined;
	x[3] = t !== undefined;
	x[4] = t < undefined;
	x[5] = t > undefined;
	x[6] = t <= undefined;
	x[7] = t >= undefined;
	x[8] = f == undefined;
	x[9] = f != undefined;
	x[10] = f === undefined;
	x[11] = f !== undefined;
	x[12] = f < undefined;
	x[13] = f > undefined;
	x[14] = f <= undefined;
	x[15] = f >= undefined;
    }
    return x.join(",");
}
testUndefinedBooleanCmp.expected = "false,true,false,true,false,false,false,false,false,true,false,true,false,false,false,false";
test(testUndefinedBooleanCmp);

function testConstantBooleanExpr()
{
    for (var j = 0; j < 3; ++j) { if(true <= true) { } }
    return "ok";
}
testConstantBooleanExpr.expected = "ok";
test(testConstantBooleanExpr);

function testNegativeGETELEMIndex()
{
    for (let i=0;i<3;++i) /x/[-4];
    return "ok";
}
testNegativeGETELEMIndex.expected = "ok";
test(testNegativeGETELEMIndex);

function doTestInvalidCharCodeAt(input)
{
    var q = "";
    for (var i = 0; i < 10; i++)
       q += input.charCodeAt(i); 
    return q;
}
function testInvalidCharCodeAt()
{
    return doTestInvalidCharCodeAt("");
}
testInvalidCharCodeAt.expected = "NaNNaNNaNNaNNaNNaNNaNNaNNaNNaN";
test(testInvalidCharCodeAt);

function FPQuadCmp()
{
    for (let j = 0; j < 3; ++j) { true == 0; }
    return "ok";
}
FPQuadCmp.expected = "ok";
test(FPQuadCmp);

function testDestructuring() {
    var t = 0;
    for (var i = 0; i < HOTLOOP + 1; ++i) {
        var [r, g, b] = [1, 1, 1];
        t += r + g + b;
    }
    return t
}
testDestructuring.expected = (HOTLOOP + 1) * 3;
test(testDestructuring);

function loopWithUndefined1(t, val) {
    var a = new Array(6);
    for (var i = 0; i < 6; i++)
        a[i] = (t > val);
    return a;
}
loopWithUndefined1(5.0, 2);     //compile version with val=int

function testLoopWithUndefined1() {
    return loopWithUndefined1(5.0).join(",");  //val=undefined
};
testLoopWithUndefined1.expected = "false,false,false,false,false,false";
test(testLoopWithUndefined1);

function loopWithUndefined2(t, dostuff, val) {
    var a = new Array(6);
    for (var i = 0; i < 6; i++) {
        if (dostuff) {
            val = 1; 
            a[i] = (t > val);
        } else {
            a[i] = (val == undefined);
        }
    }
    return a;
}
function testLoopWithUndefined2() {
    var a = loopWithUndefined2(5.0, true, 2);
    var b = loopWithUndefined2(5.0, true);
    var c = loopWithUndefined2(5.0, false, 8);
    var d = loopWithUndefined2(5.0, false);
    return [a[0], b[0], c[0], d[0]].join(",");
}
testLoopWithUndefined2.expected = "true,true,false,true";
test(testLoopWithUndefined2);

//test no multitrees assert
function testBug462388() {
    var c = 0, v; for each (let x in ["",v,v,v]) { for (c=0;c<4;++c) { } }
    return true;
}
testBug462388.expected = true;
test(testBug462388);

//test no multitrees assert
function testBug462407() {
    for each (let i in [0, {}, 0, 1.5, {}, 0, 1.5, 0, 0]) { }
    return true;
}
testBug462407.expected = true;
test(testBug462407);

//test no multitrees assert
function testBug463490() {
    function f(a, b, d) {
        for (var i = 0; i < 10; i++) {
            if (d)
                b /= 2;
        }
        return a + b;
    }
    //integer stable loop
    f(2, 2, false);
    //double stable loop
    f(3, 4.5, false);
    //integer unstable branch
    f(2, 2, true);
    return true;
};
testBug463490.expected = true;
test(testBug463490);

// BEGIN MANDELBROT STUFF
// XXXbz I would dearly like to wrap it up into a function to avoid polluting
// the global scope, but the function ends up heavyweight, and then we lose on
// the jit.
load("mandelbrot-results.js");
//function testMandelbrotAll() {
  // Configuration options that affect which codepaths we follow.
  var doImageData = true;
  var avoidSparseArray = true;

  // Control of iteration numbers and sizing.  We'll do
  // scaler * colorNames.length iterations or so before deciding that we
  // don't escape.
  const scaler = 5;
  const numRows = 600;
  const numCols = 600;

  // For now, avoid hitting memory pressure
  gcparam("maxBytes", 1300000000); 
  gcparam("maxMallocBytes", 1300000000); 

  const colorNames = [
    "black",
    "green",
    "blue",
    "red",
    "purple",
    "orange",
    "cyan",
    "yellow",
    "magenta",
    "brown",
    "pink",
    "chartreuse",
    "darkorange",
    "crimson",
    "gray",
    "deeppink",
    "firebrick",
    "lavender",
    "lawngreen",
    "lightsalmon",
    "lime",
    "goldenrod"
  ];
  const threshold = (colorNames.length - 1) * scaler;

  // Now set up our colors
  var colors = [];
  // 3-part for loop (iterators buggy, we will add a separate test for them)
  for (var colorNameIdx = 0; colorNameIdx < colorNames.length; ++colorNameIdx) {
  //for (var colorNameIdx in colorNames) {
    colorNameIdx = parseInt(colorNameIdx);
    colors.push([colorNameIdx, colorNameIdx, colorNameIdx, 0]);
  }

  // Storage for our point data
  var points;

  var scratch = {};
  var scratchZ = {};
  function complexMult(a, b) {
    var newr = a.r * b.r - a.i * b.i;
    var newi = a.r * b.i + a.i * b.r;
    scratch.r = newr;
    scratch.i = newi;
    return scratch;
  }
  function complexAdd(a, b) {
    scratch.r = a.r + b.r;
    scratch.i = a.i + b.i;
    return scratch;
  }
  function abs(a) {
    return Math.sqrt(a.r * a.r + a.i * a.i);
  }

  function escapeAbsDiff(normZ, absC) {
    var absZ = Math.sqrt(normZ);
    return normZ > absZ + absC;
  }

  function escapeNorm2(normZ) {
    return normZ > 4;
  }

  function fuzzyColors(i) {
    return Math.floor(i / scaler) + 1;
  }

  function moddedColors(i) {
    return (i % (colorNames.length - 1)) + 1;
  }

  function computeEscapeSpeedObjects(real, imag) {
    var c = { r: real, i: imag }
    scratchZ.r = scratchZ.i = 0;
    var absC = abs(c);
    for (var i = 0; i < threshold; ++i) {
      scratchZ = complexAdd(c, complexMult(scratchZ, scratchZ));
      if (escape(scratchZ.r * scratchZ.r + scratchZ.i * scratchZ.i,
                 absC)) {
        return colorMap(i);
      }
    }
    return 0;
  }

  function computeEscapeSpeedOneObject(real, imag) {
    // fold in the fact that we start with 0
    var r = real;
    var i = imag;
    var absC = abs({r: real, i: imag});
    for (var j = 0; j < threshold; ++j) {
      var r2 = r * r;
      var i2 = i * i;
      if (escape(r2 + i2, absC)) {
        return colorMap(j);
      }
      i = 2 * r * i + imag;
      r = r2 - i2 + real;
    }
    return 0;
  }

  function computeEscapeSpeedDoubles(real, imag) {
    // fold in the fact that we start with 0
    var r = real;
    var i = imag;
    var absC = Math.sqrt(real * real + imag * imag);
    for (var j = 0; j < threshold; ++j) {
      var r2 = r * r;
      var i2 = i * i;
      if (escape(r2 + i2, absC)) {
        return colorMap(j);
      }
      i = 2 * r * i + imag;
      r = r2 - i2 + real;
    }
    return 0;
  }

  var computeEscapeSpeed = computeEscapeSpeedDoubles;
  var escape = escapeNorm2;
  var colorMap = fuzzyColors;

  function addPointOrig(pointArray, n, i, j) {
    if (!points[n]) {
      points[n] = [];
      points[n].push([i, j, 1, 1]);
    } else {
      var point = points[n][points[n].length-1];
      if (point[0] == i && point[1] == j - point[3]) {
        ++point[3];
      } else {
        points[n].push([i, j, 1, 1]);
      }
    }
  }

  function addPointImagedata(pointArray, n, col, row) {
    var slotIdx = ((row * numCols) + col) * 4;
    pointArray[slotIdx] = colors[n][0];
    pointArray[slotIdx+1] = colors[n][1];
    pointArray[slotIdx+2] = colors[n][2];
    pointArray[slotIdx+3] = colors[n][3];
  }

  function createMandelSet() {
    var realRange = { min: -2.1, max: 1 };
    var imagRange = { min: -1.5, max: 1.5 };

    var addPoint;
    if (doImageData) {
      addPoint = addPointImagedata;
      points = new Array(4*numCols*numRows);
      if (avoidSparseArray) {
        for (var idx = 0; idx < 4*numCols*numRows; ++idx) {
          points[idx] = 0;
        }
      }
    } else {
      addPoint = addPointOrig;
      points = [];
    }
    var realStep = (realRange.max - realRange.min)/numCols;
    var imagStep = (imagRange.min - imagRange.max)/numRows;
    for (var i = 0, curReal = realRange.min;
         i < numCols;
         ++i, curReal += realStep) {
      for (var j = 0, curImag = imagRange.max;
           j < numRows;
           ++j, curImag += imagStep) {
        var n = computeEscapeSpeed(curReal, curImag);
        addPoint(points, n, i, j)
      }
    }
    var result;
    if (doImageData) {
      if (colorMap == fuzzyColors) {
        result = mandelbrotImageDataFuzzyResult;
      } else {
        result = mandelbrotImageDataModdedResult;
      }
    } else {
      result = mandelbrotNoImageDataResult;
    }
    return points.toSource() == result;
  }

  createMandelSet.expected = true;

  const escapeTests = [ escapeAbsDiff ];
  const colorMaps = [ fuzzyColors, moddedColors ];
  const escapeComputations = [ computeEscapeSpeedObjects,
                               computeEscapeSpeedOneObject,
                               computeEscapeSpeedDoubles ];
  // Test all possible escape-speed generation codepaths, using the
  // imageData + sparse array avoidance storage.
  doImageData = true;
  avoidSparseArray = true;
  for (var escapeIdx in escapeTests) {
    escape = escapeTests[escapeIdx];
    for (var colorMapIdx in colorMaps) {
      colorMap = colorMaps[colorMapIdx];
      for (var escapeComputationIdx in escapeComputations) {
        computeEscapeSpeed = escapeComputations[escapeComputationIdx];
        test(createMandelSet);
      }
    }
  }

  // Test all possible storage strategies. Note that we already tested
  // doImageData == true with avoidSparseArray == true.
  escape = escapeAbsDiff;
  colorMap = fuzzyColors; // This part doesn't really matter too much here
  computeEscapeSpeed = computeEscapeSpeedDoubles;

  doImageData = true;
  avoidSparseArray = false; 
  test(createMandelSet);

  escape = escapeNorm2;
  doImageData = false;  // avoidSparseArray doesn't matter here
  test(createMandelSet);
//}
//testMandelbrotAll();
// END MANDELBROT STUFF

function testNewDate()
{
    // Accessing global.Date for the first time will change the global shape,
    // so do it before the loop starts; otherwise we have to loop an extra time
    // to pick things up.
    var start = new Date();
    var time = new Date();
    for (var j = 0; j < RUNLOOP; ++j) {
	time = new Date();
    }
    return time > 0 && time >= start;
}
testNewDate.expected = true;
testNewDate.jitstats = {
    recorderStarted: 1,
    recorderAborted: 0,
    traceTriggered: 1
};
test(testNewDate);

function testArrayPushPop() {
    var a = [], sum1 = 0, sum2 = 0;
    for (var i = 0; i < 10; ++i)
	sum1 += a.push(i);
    for (var i = 0; i < 10; ++i)
	sum2 += a.pop();
    a.push(sum1);
    a.push(sum2);
    return a.join(",");
}
testArrayPushPop.expected = "55,45";
test(testArrayPushPop);

function testResumeOp() {
    var a = [1,"2",3,"4",5,"6",7,"8",9,"10",11,"12",13,"14",15,"16"];
    var x = "";
    while (a.length > 0)
        x += a.pop();
    return x;
}
testResumeOp.expected = "16151413121110987654321";
test(testResumeOp);

function testUndefinedCmp() {
    var a = false;
    for (var j = 0; j < 4; ++j) { if (undefined < false) { a = true; } }
    return a;
}
testUndefinedCmp.expected = false;
test(testUndefinedCmp);

function reallyDeepNestedExit(schedule)
{
    var c = 0, j = 0;
    for (var i = 0; i < 5; i++) {
        for (j = 0; j < 4; j++) {
            c += (schedule[i*4 + j] == 1) ? 1 : 2;
        }
    }
    return c;
}
function testReallyDeepNestedExit()
{
    var c = 0;
    var schedule1 = new Array(5*4);
    var schedule2 = new Array(5*4);
    for (var i = 0; i < 5*4; i++) {
        schedule1[i] = 0;
        schedule2[i] = 0;
    }
    /**
     * First innermost compile: true branch runs through.
     * Second '': false branch compiles new loop edge.
     * First outer compile: expect true branch.
     * Second '': hit false branch.
     */
    schedule1[0*4 + 3] = 1;
    var schedules = [schedule1,
                     schedule2,
                     schedule1,
                     schedule2,
                     schedule2];

    for (var i = 0; i < 5; i++) {
        c += reallyDeepNestedExit(schedules[i]);
    }
    return c;
}
testReallyDeepNestedExit.expected = 198;
test(testReallyDeepNestedExit);

function testRegExpTest() {
    var r = /abc/;
    var flag = false;
    for (var i = 0; i < 10; ++i)
	flag = r.test("abc");
    return flag;
}
testRegExpTest.expected = true;
test(testRegExpTest);

function testNumToString() {
    var r = [];
    var d = 123456789;
    for (var i = 0; i < 10; ++i) {
	r = [
	     d.toString(),
	     (-d).toString(),
	     d.toString(10),
	     (-d).toString(10),
	     d.toString(16),
	     (-d).toString(16),
	     d.toString(36),
	     (-d).toString(36)
        ];
    }
    return r.join(",");
}
testNumToString.expected = "123456789,-123456789,123456789,-123456789,75bcd15,-75bcd15,21i3v9,-21i3v9";
test(testNumToString);

function testSubstring() {
    for (var i = 0; i < 5; ++i) {
        actual = "".substring(5);
    }
    return actual;
}
testSubstring.expected = "";
test(testSubstring);

function testForInLoopChangeIteratorType() {
    for(y in [0,1,2]) y = NaN;
    (function(){
        [].__proto__.u = void 0;
        for (let y in [5,6,7,8])
            y = NaN;
        delete [].__proto__.u;
    })()
    return "ok";
}
testForInLoopChangeIteratorType.expected = "ok";
test(testForInLoopChangeIteratorType);

function testGrowDenseArray() {
    var a = new Array();
    for (var i = 0; i < 10; ++i)
	a[i] |= 5;
    return a.join(",");
}
testGrowDenseArray.expected = "5,5,5,5,5,5,5,5,5,5";
test(testGrowDenseArray);

function testCallProtoMethod() {
    function X() { this.x = 1; }
    X.prototype.getName = function () { return "X"; }

    function Y() { this.x = 2; }
    Y.prototype.getName = function() "Y";

    var a = [new X, new X, new X, new X, new Y];
    var s = '';
    for (var i = 0; i < a.length; i++)
        s += a[i].getName();
    return s;
}
testCallProtoMethod.expected = 'XXXXY';
test(testCallProtoMethod);

function testTypeUnstableForIn() {
    var a = [1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16];
    var x = 0;
    for (var i in a) {
        i = parseInt(i);
        x++;
    }
    return x;
}
testTypeUnstableForIn.expected = 16;
test(testTypeUnstableForIn);

function testAddUndefined() {
    for (var j = 0; j < 3; ++j)
        (0 + void 0) && 0;
}
test(testAddUndefined);

function testStringify() {
    var t = true, f = false, u = undefined, n = 5, d = 5.5, s = "x";
    var a = [];
    for (var i = 0; i < 10; ++i) {
	a[0] = "" + t;
	a[1] = t + "";
	a[2] = "" + f;
	a[3] = f + "";
	a[4] = "" + u;
	a[5] = u + "";
	a[6] = "" + n;
	a[7] = n + "";
	a[8] = "" + d;
	a[9] = d + "";
	a[10] = "" + s;
	a[11] = s + "";
    }
    return a.join(",");
}
testStringify.expected = "true,true,false,false,undefined,undefined,5,5,5.5,5.5,x,x";
test(testStringify);

/* NOTE: Keep this test last, since it screws up all for...in loops after it. */
function testGlobalProtoAccess() {
    return "ok";
}
this.__proto__.a = 3; for (var j = 0; j < 4; ++j) { [a]; }
testGlobalProtoAccess.expected = "ok";
test(testGlobalProtoAccess);

/* Keep these at the end so that we can see the summary after the trace-debug spew. */
print("\npassed:", passes.length && passes.join(","));
print("\nFAILED:", fails.length && fails.join(","));
