/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that the undo dialog works as expected.
 */
function runTests() {
  // remove unpinned sites and undo it
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("5");

  yield addNewTabPageTab();
  checkGrid("5p,0,1,2,3,4,6,7,8");

  yield blockCell(cells[4]);
  yield blockCell(cells[4]);
  checkGrid("5p,0,1,2,6,7,8");

  yield undo();
  checkGrid("5p,0,1,2,4,6,7,8");

  // now remove a pinned site and undo it
  yield blockCell(cells[0]);
  checkGrid("0,1,2,4,6,7,8");

  yield undo();
  checkGrid("5p,0,1,2,4,6,7,8");
}

function undo() {
  let target = cw.document.getElementById("newtab-undo");
  EventUtils.synthesizeMouseAtCenter(target, {}, cw);
  whenPagesUpdated();
}
