/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  let links = [];
  for (let i = 0; i < 10; i++)
    links.push({url: "about:blank#" + i, title: "site " + i});

  yield addNewTabPageTab(links);

  checkCellTitle(cells[4], "site 4", "cell #4 is 'site 4'");
  yield blockCell(cells[4]);

  checkCellTitle(cells[4], "site 5", "cell #4 is now 'site 5'");
  checkCellTitle(cells[8], "site 9", "cell #8 is now 'site 9'");
  yield blockCell(cells[4]);

  checkCellTitle(cells[4], "site 6", "cell #4 is now 'site 6'");
  checkCellIsEmpty(cells[8], "cell #8 is now empty");
  yield blockCell(cells[4]);

  checkCellTitle(cells[4], "site 7", "cell #4 is now 'site 7'");
  checkCellIsEmpty(cells[7], "cell #7 is now empty");
  checkCellIsEmpty(cells[8], "cell #8 is still empty");
}
