// |jit-test| mjitalways
setDebug(true);

function nop(){}
function caller(obj) {
  assertJit();
  return x;
}
trap(caller, 7, "var x = 'success'; nop()");
assertEq(caller(this), "success");
