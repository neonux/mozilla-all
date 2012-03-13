/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that all changes that are made to a specific
 * 'New Tab Page' are synchronized with all other open 'New Tab Pages'
 * automatically. All about:newtab pages should always be in the same
 * state.
 */
function runTests() {
  setLinks("0,1,2,3,4,5,6,7,8,9");
  setPinnedLinks(",1");

  yield addNewTabPageTab();
  checkGrid("0,1p,2,3,4,5,6,7,8");

  let oldCw = cw;

  // create the new tab page
  yield addNewTabPageTab();
  checkGrid("0,1p,2,3,4,5,6,7,8");

  // unpin a cell
  yield unpinCell(cells[1]);
  checkGrid("0,1,2,3,4,5,6,7,8");
  checkGrid("0,1,2,3,4,5,6,7,8", oldCw.gGrid.sites);

  // remove a cell
  yield blockCell(cells[1]);
  checkGrid("0,2,3,4,5,6,7,8,9");
  checkGrid("0,2,3,4,5,6,7,8,9", oldCw.gGrid.sites);

  // insert a new cell by dragging
  yield simulateDrop(cells[1]);
  checkGrid("0,99p,2,3,4,5,6,7,8");
  checkGrid("0,99p,2,3,4,5,6,7,8", oldCw.gGrid.sites);

  // drag a cell around
  yield simulateDrop(cells[1], cells[2]);
  checkGrid("0,2p,99p,3,4,5,6,7,8");
  checkGrid("0,2p,99p,3,4,5,6,7,8", oldCw.gGrid.sites);

  // reset the new tab page
  yield restore(TestRunner.next);
  checkGrid("0,1,2,3,4,5,6,7,8");
  checkGrid("0,1,2,3,4,5,6,7,8", oldCw.gGrid.sites);
}
