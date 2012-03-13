/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that resetting the 'New Tage Page' works as expected.
 */
function runTests() {
  // create a new tab page and check its state after blocking a site
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();

  checkGrid("0,1,2,3,4,5,6,7,8");

  yield blockCell(cells[4]);
  checkGrid("0,1,2,3,5,6,7,8,");

  yield restore(TestRunner.next);
  checkGrid("0,1,2,3,4,5,6,7,8");
}
