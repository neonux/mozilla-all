/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that resetting the 'New Tage Page' works as expected.
 */
function runTests() {
  // create a new tab page and check its modified state after blocking a site
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8");
  ok(!doc.body.classList.contains("modified"), "page is not modified");

  yield blockCell(cells[4]);
  checkGrid("0,1,2,3,5,6,7,8,");
  ok(doc.body.classList.contains("modified"), "page is modified");

  yield cw.Toolbar.reset(TestRunner.next);
  checkGrid("0,1,2,3,4,5,6,7,8");
  ok(!doc.body.classList.contains("modified"), "page is not modified");
}
