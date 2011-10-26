/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  let links = [];
  for (let i = 0; i < 9; i++)
    links.push({url: "about:blank#" + i, title: "site " + i});

  yield addNewTabPageTab(links);

  pinCell(cells[6]);
  checkCellIsPinned(cells[6], "cell #6 is now pinned");

  pinCell(cells[8]);
  checkCellIsPinned(cells[8], "cell #8 is now pinned");

  yield blockCell(cells[0]);
  checkCellIsEmpty(cells[7], "cell #7 is now empty");

  yield blockCell(cells[0]);
  checkCellIsEmpty(cells[5], "cell #5 is now empty");

  yield unpinCell(cells[8]);
  checkCellTitle(cells[5], "site 8", "cell #5 is now 'site 8'");
  checkCellIsEmpty(cells[7], "cell #7 is still empty");

  pinCell(cells[5]);
  checkCellIsPinned(cells[5], "cell #5 is now pinned");

  yield blockCell(cells[4]);
  checkCellIsEmpty(cells[4], "cell #4 is now empty");

  yield unpinCell(cells[5]);
  checkCellTitle(cells[4], "site 8", "cell #4 is now 'site 8'");

  yield unpinCell(cells[6]);
  checkCellTitle(cells[5], "site 6", "cell #6 is now 'site 6'");
}
