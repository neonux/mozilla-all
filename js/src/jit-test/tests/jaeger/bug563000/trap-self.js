// |jit-test| debug
setDebug(true);
x = "notset";
function main() {
  /* The JSOP_STOP in a. */
  trap(main, 29, "success()");
  x = "failure";
}
function success() { x = "success"; }

main();
assertEq(x, "success");
