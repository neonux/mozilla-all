setDebug(true);
x = "notset";
function main() {
  /* The JSOP_STOP in a. */
  a = { valueOf: function () { trap(main, 59, "success()"); } };
  b = "";
  eval();
  a + b;
  x = "failure";
}
function success() { x = "success"; }

main();
assertEq(x, "success");
