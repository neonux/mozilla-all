/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function getTabLabel(aTab) {
  return aTab.label;
}

function waitForTabsRelabeled(aTab, aCallback) {
  let callback = function () {
    aTab.parentNode.removeEventListener("TabsRelabeled", callback, true);
    aCallback();
  };
  aTab.parentNode.addEventListener("TabsRelabeled", callback, true);
}

function addTabsAndWaitForRelabeled(aBaseTab, aCount, aCallback, aAddedTabs) {
  if (!aAddedTabs)
    aAddedTabs = [];
  let callback = function () {
    aBaseTab.parentNode.removeEventListener("TabsRelabeled", callback, true);
    if (aCount > 1)
      addTabsAndWaitForRelabeled(aBaseTab, aCount - 1, aCallback, aAddedTabs);
    else
      aCallback(aAddedTabs);
  };
  aBaseTab.parentNode.addEventListener("TabsRelabeled", callback, true);
  aAddedTabs.push(gBrowser.addTab());
}

function removeTabsAndWaitForRelabeled(aBaseTab, aTabs, aCallback) {
  let callback = function () {
    aBaseTab.parentNode.removeEventListener("TabsRelabeled", callback, true);
    if (aTabs.length > 0)
      removeTabsAndWaitForRelabeled(aBaseTab, aTabs, aCallback);
    else
      aCallback();
  };
  aBaseTab.parentNode.addEventListener("TabsRelabeled", callback, true);
  gBrowser.removeTab(aTabs.pop());
}

function setTitleForTab(aTab, aTitle, aCallback) {
  waitForTabsRelabeled(aTab, aCallback);
  aTab.linkedBrowser.contentDocument.title = aTitle;
}

var TESTS = [
function test_about_blank() {
  let tab1 = gBrowser.selectedTab;
  addTabsAndWaitForRelabeled(tab1, 2, test_blank);
  function test_blank(aTabs) {
    let [tab2, tab3] = aTabs;
    is(getTabLabel(tab1), "New Tab", "Should be unchanged");
    is(getTabLabel(tab2), "New Tab", "Should be unchanged");
    is(getTabLabel(tab3), "New Tab", "Should be unchanged");
    removeTabsAndWaitForRelabeled(tab1, [tab2, tab3], runNextTest);
  }
},

function test_two_tabs() {
  let tab1 = gBrowser.selectedTab;
  addTabsAndWaitForRelabeled(tab1, 1, setup1);
  let tab2;

  function setup1 (aTabs) {
    tab2 = aTabs[0];
    setTitleForTab(tab1, "Foo - Bar - Baz", setup2);
  }
  function setup2 () {
    setTitleForTab(tab2, "Foo - Baz - Baz", setupComplete);
  }
  function setupComplete () {
    is(getTabLabel(tab1), "Bar - Baz", "Should remove exactly two tokens");
    is(getTabLabel(tab2), "Baz - Baz", "Should remove exactly two tokens");
    removeTabsAndWaitForRelabeled(tab1, [tab2], runNextTest);
  }
},

function test_direct_tests() {
  let useFunc = gBrowser.selectedTab.parentNode._getChopsForSet;
  let sortFunc = function (aTab, bTab) {
    let aLabel = aTab._originalTitle;
    let bLabel = bTab._originalTitle;
    return (aLabel < bLabel) ? -1 : (aLabel > bLabel) ? 1 : 0;
  };
  let testGroups = [
    [
      [
        "Foo - Bar - Baz",
        "Foo - Baz - Baz",
        "Foo - Baz - Baz",
        "Foo - Baz - Qux"
      ],
      [
        [6, 6],
        [6, 6, 6],
        [6, 12, 12, 12]
      ]
    ],
    [
      [
        "http://example.com/foo.html",
        "http://example.com/foo/bar.html",
        "Browse - ftp://example.com/pub/",
        "Browse - ftp://example.com/pub/src/"
      ],
      [
        [27, 31],
        [27, 31, 0],
        [27, 31, 19, 19]
      ]
    ],
    [
      [
        "'Zilla and the Foxes - Singles - Musical Monkey",
        "'Zilla and the Foxes - Biography - Musical Monkey",
        "'Zilla and the Foxes - Musical Monkey",
        "'Zilla and the Foxes - Interviews - Musical Monkey"
      ],
      [
        [23, 23],
        [23, 23, 0],
        [23, 23, 0, 23]
      ]
    ]
  ];
  for (let i = 0; i < testGroups.length; i++) {
    let rawInputSet = testGroups[i][0];
    let inputSet = [];
    for (let j = 0; j < rawInputSet.length; j++)
      inputSet[j] = {_originalTitle: rawInputSet[j]};
    inputSet.sort(sortFunc);
    for (let j = 1; j < inputSet.length; j++) {
      let correct = testGroups[i][1][j - 1];
      let output = useFunc(inputSet.slice(0, j + 1));
      is(output.toSource(), correct.toSource(),
        "Direct test group " + (i + 1) + ", stage " + j);
    }
  }
  runNextTest();
}
];

var gTestStart = null;

function runNextTest() {
  if (gTestStart)
    info("Test part took " + (Date.now() - gTestStart) + "ms");

  if (TESTS.length == 0) {
    finish();
    return;
  }

  info("Running " + TESTS[0].name);
  gTestStart = Date.now();
  TESTS.shift()();
};

function test() {
  waitForExplicitFinish();
  runNextTest();
}
