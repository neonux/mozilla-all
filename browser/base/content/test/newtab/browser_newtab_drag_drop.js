/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that dragging and dropping sites works as expected.
 * Sites contained in the grid need to shift around to indicate the result
 * of the drag-and-drop operation. If the grid is full and we're dragging
 * a new site into it another one gets pushed out.
 */
function runTests() {
  // test a simple drag-and-drop scenario
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8");

  yield simulateDrop(cells[1], cells[0]);
  checkGrid("1,0p,2,3,4,5,6,7,8");

  // drag a cell to its current cell and make sure it's not pinned afterwards
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8");

  yield simulateDrop(cells[0], cells[0]);
  checkGrid("0,1,2,3,4,5,6,7,8");

  // ensure that pinned pages aren't moved if that's not necessary
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",1,2");

  yield addNewTabPageTab();
  checkGrid("0,1p,2p,3,4,5,6,7,8");

  yield simulateDrop(cells[3], cells[0]);
  checkGrid("3,1p,2p,0p,4,5,6,7,8");

  // pinned sites should always be moved around as blocks. if a pinned site is
  // moved around, neighboring pinned are affected as well
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1");

  yield addNewTabPageTab();
  checkGrid("0p,1p,2,3,4,5,6,7,8");

  yield simulateDrop(cells[0], cells[2]);
  checkGrid("2p,0p,1p,3,4,5,6,7,8");

  // pinned sites should not be pushed out of the grid (unless there are only
  // pinned ones left on the grid)
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7p,8p");

  yield simulateDrop(cells[8], cells[2]);
  checkGrid("0,1,3,4,5,6,7p,8p,2p");

  // make sure that pinned sites are re-positioned correctly
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1,2,,,5");

  yield addNewTabPageTab();
  checkGrid("0p,1p,2p,3,4,5p,6,7,8");

  yield simulateDrop(cells[4], cells[0]);
  checkGrid("3,1p,2p,4,0p,5p,6,7,8");

  // drag a new site onto the very first cell
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7p,8p");

  yield simulateDrop(cells[0]);
  checkGrid("99p,0,1,2,3,4,5,7p,8p");

  // drag a new site onto the grid and make sure that pinned cells don't get
  // pushed out
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,7,8");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7p,8p");

  yield simulateDrop(cells[7]);
  checkGrid("0,1,2,3,4,5,7p,99p,8p");

  // drag a new site beneath a pinned cell and make sure the pinned cell is
  // not moved
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks(",,,,,,,,8");

  yield addNewTabPageTab();
  checkGrid("0,1,2,3,4,5,6,7,8p");

  yield simulateDrop(cells[7]);
  checkGrid("0,1,2,3,4,5,6,99p,8p");

  // drag a new site onto a block of pinned sites and make sure they're shifted
  // around accordingly
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("0,1,2,,,,,,");

  yield addNewTabPageTab();
  checkGrid("0p,1p,2p");

  yield simulateDrop(cells[1]);
  checkGrid("0p,99p,1p,2p,3,4,5,6,7");
}
