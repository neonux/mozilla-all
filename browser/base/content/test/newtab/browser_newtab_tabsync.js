/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function runTests() {
  // setup mock sites
  let sites = [];

  for (let i = 0; i < 9; i++)
    sites.push({url: "about:blank#" + i, title: "site " + i});

  NewTabPageTesting.mockSites(sites);
  yield addNewTabPageTab(TestRunner.next);

  let cells = ntp.grid.cells;

  yield addNewTabPageTab(TestRunner.next);

  let cells2 = ntp.grid.cells;

  yield NewTabPageTesting.removeCell(cells[4], TestRunner.next);
  is(cells[4].getSite().title, "site 5", "cell #4 is now 'site 5'");
  is(cells2[4].getSite().title, "site 5", "cell #4-2 is now also 'site 5'");

  cells[4].pin();
  ok(cells[4].isPinned(), "cell #4 is now pinned");
  ok(cells2[4].isPinned(), "cell #4-2 is now pinned");

  cells[4].unpin();
  ok(cells[4].isPinned(), "cell #4 is now unpinned");
  ok(cells2[4].isPinned(), "cell #4-2 is now unpinned");

  // TODO test complete enable/disable
}
