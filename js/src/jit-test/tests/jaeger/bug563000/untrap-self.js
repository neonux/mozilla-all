// |jit-test| debug
setDebug(true);
x = "notset";
function main() {
  /* JSOP_STOP in main. */
  untrap(main, 27);
  x = "success";
}
function failure() { x = "failure"; }

/* JSOP_STOP in main. */
trap(main, 27, "failure()");
main();
assertEq(x, "success");
