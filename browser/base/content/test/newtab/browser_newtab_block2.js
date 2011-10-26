/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  let links = [];
  for (let i = 0; i < 10; i++)
    links.push({url: "about:blank#" + i, title: "site " + i});

  yield addNewTabPageTab(links);

  pinCell(cells[8]);
  checkCellIsPinned(cells[8], "cell #8 is now pinned");
  yield blockCell(cells[7]);

  checkCellTitle(cells[7], "site 9", "cell #7 is now 'site 9'");
}
