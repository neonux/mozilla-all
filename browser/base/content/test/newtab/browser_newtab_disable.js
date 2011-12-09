/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * These tests make sure that the 'New Tab Page' feature can be disabled if the
 * decides not to use it.
 */
function runTests() {
  // create a new tab page and hide it.
  setLinks("0,1,2,3,4,5,6,7,8");
  setPinnedLinks("");

  yield addNewTabPageTab();
  ok(!doc.body.classList.contains("disabled"), "page is not disabled");

  cw.Toolbar.hide();
  ok(doc.body.classList.contains("disabled"), "page is disabled");

  let oldDocument = doc;

  // create a second new tage page and make sure it's disabled. enable it
  // again and check if the former page gets enabled as well.
  yield addNewTabPageTab();
  ok(doc.body.classList.contains("disabled"), "page is disabled");

  cw.Toolbar.show();
  ok(!doc.body.classList.contains("disabled"), "page is not disabled");
  ok(!oldDocument.body.classList.contains("disabled"), "old page is not disabled");
}
