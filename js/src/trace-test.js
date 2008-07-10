function test(desc, expected, actual)
{
  if (expected == actual)
    return print(desc, ": passed");
  print(desc, ": FAILED: ", typeof(expected), "(", expected, ") != ",
	typeof(actual), "(", actual, ")");
}

function ifInsideLoop()
{
  var cond = true, count = 0;
  for (var i = 0; i < 5000; i++) {
    if (cond)
      count++;
  }
  return count;
}
test("tracing if", ifInsideLoop(), 5000);

function bitwiseAnd(bitwiseAndValue) {
  for (var i = 0; i < 60000; i++)
    bitwiseAndValue = bitwiseAndValue & i;
  return bitwiseAndValue;
}
test("bitwise and with arg/var", bitwiseAnd(12341234), 0)

bitwiseAndValue = Math.pow(2,32);
for (var i = 0; i < 60000; i++)
  bitwiseAndValue = bitwiseAndValue & i;
test("bitwise on undeclared globals", bitwiseAndValue, 0);

function equalInt()
{
  var i1 = 55, eq = 0;
  for (var i = 0; i < 5000; i++) {
    if (i1 == 55)
      eq++;
  }
  return eq;
}
test("int equality", equalInt(), 5000);

function setelem(a)
{
  var l = a.length;
  for (var i = 0; i < l; i++) {
    a[i] = i;
  }
  return a;
}

var a = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
a = a.concat(a, a, a);
setelem(a)
test("setelem", a, "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83");

function getelem(a)
{
  var accum = 0;
  var l = a.length;
  for (var i = 0; i < l; i++) {
    accum += a[i];
  }
  return accum;
}
test("getelem", getelem(a), 3486);

globalName = 907;
function name()
{
  var a = 0;
  for (var i = 0; i < 100; i++)
    a = globalName;
  return a;
}
test("undeclared globals from function", name(), 907);

function arith()
{
  var accum = 0;
  for (var i = 0; i < 100; i++) {
    accum += (i * 2) - 1;
  }
  return accum;
}
test("basic arithmetic", arith(), 9800);

function lsh(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x1 << n;
  return r;
}
test("lsh(20)", lsh(20), 1048576);
test("lsh(0)", lsh(0), 1); // crashes on second call
// test("lsh(55)", lsh(55), 8388608); // crashes on second call

function rsh(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x11010101 >> n;
  return r;
}
test("rsh(35)", rsh(35), 35659808);
// test("rsh(-1)", rsh(-1), x);

function ursh(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = -55 >>> n;
  return r;
}
test("ursh(8)", ursh(8), 16777215);
// test("ursh(33)", ursh(33), 2147483620);

for (var i = 0; i < 500; i++)
  globalName;
// can't store anywhere yet, so just a crash-test
// update when we fix local var setting
test("get undeclared global at top level", true, true);

var globalInt = 0;
for (var i = 0; i < 500; i++)
  globalInt = i;
test("setting global variable", globalInt, 500);

