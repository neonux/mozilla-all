/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  let links = [];
  for (let i = 0; i < 9; i++)
    links.push({url: "about:blank#" + i, title: "site " + i});

  yield addNewTabPageTab(links);

  pinCell(cells[4]);
  checkCellIsPinned(cells[4], "cell #4 is now pinned");
  yield blockCell(cells[4]);

  checkCellTitle(cells[4], "site 5", "cell #4 is now 'site 5'");
  checkCellIsEmpty(cells[8], "cell #8 is now empty");
}
