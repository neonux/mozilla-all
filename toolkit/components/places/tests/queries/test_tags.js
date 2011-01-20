/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Tests bookmark and history queries with tags.  See bug 399799.
 */

// Add your tests here.  Each is an object with a summary string |desc| and a
// method run() that's called to run the test.
var gTests = [

  function tags_getter_setter()
  {
    do_log_info("Tags getter/setter should work correctly");
    do_log_info("Without setting tags, tags getter should return empty array");
    var [query, dummy] = makeQuery();
    do_check_eq(query.tags.length, 0);

    do_log_info("Setting tags to an empty array, tags getter should return "+
          "empty array");
    [query, dummy] = makeQuery([]);
    do_check_eq(query.tags.length, 0);

    do_log_info("Setting a few tags, tags getter should return correct array");
    var tags = ["bar", "baz", "foo"];
    [query, dummy] = makeQuery(tags);
    setsAreEqual(query.tags, tags, true);

    do_log_info("Setting some dupe tags, tags getter return unique tags");
    [query, dummy] = makeQuery(["foo", "foo", "bar", "foo", "baz", "bar"]);
    setsAreEqual(query.tags, ["bar", "baz", "foo"], true);
    run_next_test();
  },

  function invalid_setter_calls()
  {
    do_log_info("Invalid calls to tags setter should fail");
    try {
      var query = PlacesUtils.history.getNewQuery();
      query.tags = null;
      do_throw("Passing null to SetTags should fail");
    }
    catch (exc) {}

    try {
      query = PlacesUtils.history.getNewQuery();
      query.tags = "this should not work";
      do_throw("Passing a string to SetTags should fail");
    }
    catch (exc) {}

    try {
      makeQuery([null]);
      do_throw("Passing one-element array with null to SetTags should fail");
    }
    catch (exc) {}

    try {
      makeQuery([undefined]);
      do_throw("Passing one-element array with undefined to SetTags " +
               "should fail");
    }
    catch (exc) {}

    try {
      makeQuery(["foo", null, "bar"]);
      do_throw("Passing mixture of tags and null to SetTags should fail");
    }
    catch (exc) {}

    try {
      makeQuery(["foo", undefined, "bar"]);
      do_throw("Passing mixture of tags and undefined to SetTags " +
               "should fail");
    }
    catch (exc) {}

    try {
      makeQuery([1, 2, 3]);
      do_throw("Passing numbers to SetTags should fail");
    }
    catch (exc) {}

    try {
      makeQuery(["foo", 1, 2, 3]);
      do_throw("Passing mixture of tags and numbers to SetTags should fail");
    }
    catch (exc) {}

    try {
      var str = Cc["@mozilla.org/supports-string;1"].
                createInstance(Ci.nsISupportsString);
      str.data = "foo";
      query = PlacesUtils.history.getNewQuery();
      query.tags = str;
      do_throw("Passing nsISupportsString to SetTags should fail");
    }
    catch (exc) {}

    try {
      makeQuery([str]);
      do_throw("Passing array of nsISupportsStrings to SetTags should fail");
    }
    catch (exc) {}
    run_next_test();
  },

  function not_setting_tags()
  {
    do_log_info("Not setting tags at all should not affect query URI");
    checkQueryURI();
    run_next_test();
  },

  function empty_array_tags()
  {
    do_log_info("Setting tags with an empty array should not affect query URI");
    checkQueryURI([]);
    run_next_test();
  },

  function set_tags()
  {
    do_log_info("Setting some tags should result in correct query URI");
    checkQueryURI([
      "foo",
      "七難",
      "",
      "いっぱいおっぱい",
      "Abracadabra",
      "１２３",
      "Here's a pretty long tag name with some = signs and 1 2 3s and spaces oh jeez will it work I hope so!",
      "アスキーでございません",
      "あいうえお",
    ]);
    run_next_test();
  },

  function no_tags_tagsAreNot()
  {
    do_log_info("Not setting tags at all but setting tagsAreNot should " +
                "affect query URI");
    checkQueryURI(null, true);
    run_next_test();
  },

  function empty_array_tags_tagsAreNot()
  {
    do_log_info("Setting tags with an empty array and setting tagsAreNot " +
                "should affect query URI");
    checkQueryURI([], true);
    run_next_test();
  },

  function ()
  {
    do_log_info("Setting some tags and setting tagsAreNot should result in " +
                "correct query URI");
    checkQueryURI([
      "foo",
      "七難",
      "",
      "いっぱいおっぱい",
      "Abracadabra",
      "１２３",
      "Here's a pretty long tag name with some = signs and 1 2 3s and spaces oh jeez will it work I hope so!",
      "アスキーでございません",
      "あいうえお",
    ], true);
    run_next_test();
  },

  function tag_to_uri()
  {
    do_log_info("Querying history on tag associated with a URI should return " +
                "that URI");
    doWithVisit(["foo", "bar", "baz"], function (aURI) {
      var [query, opts] = makeQuery(["foo"]);
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["bar"]);
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["baz"]);
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
    });
  },

  function tags_to_uri()
  {
    do_log_info("Querying history on many tags associated with a URI should " +
                "return that URI");
    doWithVisit(["foo", "bar", "baz"], function (aURI) {
      var [query, opts] = makeQuery(["foo", "bar"]);
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["foo", "baz"]);
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["bar", "baz"]);
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["foo", "bar", "baz"]);
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
    });
  },

  function repeated_tag()
  {
    do_log_info("Specifying the same tag multiple times in a history query " +
                "should not matter");
    doWithVisit(["foo", "bar", "baz"], function (aURI) {
      var [query, opts] = makeQuery(["foo", "foo"]);
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["foo", "foo", "foo", "bar", "bar", "baz"]);
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
    });
  },

  function many_tags_no_uri()
  {
    do_log_info("Querying history on many tags associated with a URI and " +
                "tags not associated with that URI should not return that URI");
    doWithVisit(["foo", "bar", "baz"], function (aURI) {
      var [query, opts] = makeQuery(["foo", "bogus"]);
      executeAndCheckQueryResults(query, opts, []);
      [query, opts] = makeQuery(["foo", "bar", "bogus"]);
      executeAndCheckQueryResults(query, opts, []);
      [query, opts] = makeQuery(["foo", "bar", "baz", "bogus"]);
      executeAndCheckQueryResults(query, opts, []);
    });
  },

  function nonexistent_tags()
  {
    do_log_info("Querying history on nonexistent tags should return no results");
    doWithVisit(["foo", "bar", "baz"], function (aURI) {
      var [query, opts] = makeQuery(["bogus"]);
      executeAndCheckQueryResults(query, opts, []);
      [query, opts] = makeQuery(["bogus", "gnarly"]);
      executeAndCheckQueryResults(query, opts, []);
    });
  },

  function tag_to_bookmark()
  {
    do_log_info("Querying bookmarks on tag associated with a URI should " +
                "return that URI");
    doWithBookmark(["foo", "bar", "baz"], function (aURI) {
      var [query, opts] = makeQuery(["foo"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["bar"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["baz"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
    });
  },

  function many_tags_to_bookmark()
  {
    do_log_info("Querying bookmarks on many tags associated with a URI " +
                "should return that URI");
    doWithBookmark(["foo", "bar", "baz"], function (aURI) {
      var [query, opts] = makeQuery(["foo", "bar"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["foo", "baz"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["bar", "baz"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["foo", "bar", "baz"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
    });
  },

  function repeated_tag_to_bookmarks()
  {
    do_log_info("Specifying the same tag multiple times in a bookmark query " +
                "should not matter");
    doWithBookmark(["foo", "bar", "baz"], function (aURI) {
      var [query, opts] = makeQuery(["foo", "foo"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
      [query, opts] = makeQuery(["foo", "foo", "foo", "bar", "bar", "baz"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, [aURI.spec]);
    });
  },

  function many_tags_no_bookmark()
  {
    do_log_info("Querying bookmarks on many tags associated with a URI and " +
          "tags not associated with that URI should not return that URI");
    doWithBookmark(["foo", "bar", "baz"], function (aURI) {
      var [query, opts] = makeQuery(["foo", "bogus"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, []);
      [query, opts] = makeQuery(["foo", "bar", "bogus"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, []);
      [query, opts] = makeQuery(["foo", "bar", "baz", "bogus"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, []);
    });
  },

  function nonexistent_tags_bookmark()
  {
    do_log_info("Querying bookmarks on nonexistent tag should return no results");
    doWithBookmark(["foo", "bar", "baz"], function (aURI) {
      var [query, opts] = makeQuery(["bogus"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, []);
      [query, opts] = makeQuery(["bogus", "gnarly"]);
      opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
      executeAndCheckQueryResults(query, opts, []);
    });
  },

  function tagsAreNot_history()
  {
    do_log_info("Querying history using tagsAreNot should work correctly");
    var urisAndTags = {
      "http://example.com/1": ["foo", "bar"],
      "http://example.com/2": ["baz", "qux"],
      "http://example.com/3": null
    };

    do_log_info("Add visits and tag the URIs");
    for (let [pURI, tags] in Iterator(urisAndTags)) {
      let nsiuri = uri(pURI);
      addVisit(nsiuri);
      if (tags)
        PlacesUtils.tagging.tagURI(nsiuri, tags);
    }

    do_log_info('  Querying for "foo" should match only /2 and /3');
    var [query, opts] = makeQuery(["foo"], true);
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root,
                    ["http://example.com/2", "http://example.com/3"]);

    do_log_info('  Querying for "foo" and "bar" should match only /2 and /3');
    [query, opts] = makeQuery(["foo", "bar"], true);
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root,
                    ["http://example.com/2", "http://example.com/3"]);

    do_log_info('  Querying for "foo" and "bogus" should match only /2 and /3');
    [query, opts] = makeQuery(["foo", "bogus"], true);
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root,
                    ["http://example.com/2", "http://example.com/3"]);

    do_log_info('  Querying for "foo" and "baz" should match only /3');
    [query, opts] = makeQuery(["foo", "baz"], true);
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root,
                    ["http://example.com/3"]);

    do_log_info('  Querying for "bogus" should match all');
    [query, opts] = makeQuery(["bogus"], true);
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root,
                    ["http://example.com/1",
                     "http://example.com/2",
                     "http://example.com/3"]);

    // Clean up.
    for (let [pURI, tags] in Iterator(urisAndTags)) {
      let nsiuri = uri(pURI);
      if (tags)
        PlacesUtils.tagging.untagURI(nsiuri, tags);
    }
    cleanDatabase(run_next_test);
  },

  function tagsAreNot_bookmarks()
  {
    do_log_info("Querying bookmarks using tagsAreNot should work correctly");
    var urisAndTags = {
      "http://example.com/1": ["foo", "bar"],
      "http://example.com/2": ["baz", "qux"],
      "http://example.com/3": null
    };

    do_log_info("Add bookmarks and tag the URIs");
    for (let [pURI, tags] in Iterator(urisAndTags)) {
      let nsiuri = uri(pURI);
      addBookmark(nsiuri);
      if (tags)
        PlacesUtils.tagging.tagURI(nsiuri, tags);
    }

    do_log_info('  Querying for "foo" should match only /2 and /3');
    var [query, opts] = makeQuery(["foo"], true);
    opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root,
                    ["http://example.com/2", "http://example.com/3"]);

    do_log_info('  Querying for "foo" and "bar" should match only /2 and /3');
    [query, opts] = makeQuery(["foo", "bar"], true);
    opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root,
                    ["http://example.com/2", "http://example.com/3"]);

    do_log_info('  Querying for "foo" and "bogus" should match only /2 and /3');
    [query, opts] = makeQuery(["foo", "bogus"], true);
    opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root,
                    ["http://example.com/2", "http://example.com/3"]);

    do_log_info('  Querying for "foo" and "baz" should match only /3');
    [query, opts] = makeQuery(["foo", "baz"], true);
    opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root,
                    ["http://example.com/3"]);

    do_log_info('  Querying for "bogus" should match all');
    [query, opts] = makeQuery(["bogus"], true);
    opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root,
                    ["http://example.com/1",
                     "http://example.com/2",
                     "http://example.com/3"]);

    // Clean up.
    for (let [pURI, tags] in Iterator(urisAndTags)) {
      let nsiuri = uri(pURI);
      if (tags)
        PlacesUtils.tagging.untagURI(nsiuri, tags);
    }
    cleanDatabase(run_next_test);
  },

  function duplicate_tags() {
    do_log_info("Duplicate existing tags (i.e., multiple tag folders with " +
                "same name) should not throw off query results");
    var tagName = "foo";

    do_log_info("Add bookmark and tag it normally");
    addBookmark(TEST_URI);
    PlacesUtils.tagging.tagURI(TEST_URI, [tagName]);

    do_log_info("Manually create tag folder with same name as tag and insert " +
          "bookmark");
    var dupTagId = PlacesUtils.bookmarks.createFolder(PlacesUtils.tagsFolderId,
                                                      tagName,
                                                      Ci.nsINavBookmarksService.DEFAULT_INDEX);
    do_check_true(dupTagId > 0);
    var bmId = PlacesUtils.bookmarks.insertBookmark(dupTagId,
                                                    TEST_URI,
                                                    Ci.nsINavBookmarksService.DEFAULT_INDEX,
                                                    "title");
    do_check_true(bmId > 0);

    do_log_info("Querying for tag should match URI");
    var [query, opts] = makeQuery([tagName]);
    opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root, [TEST_URI.spec]);

    PlacesUtils.tagging.untagURI(TEST_URI, [tagName]);
    cleanDatabase(run_next_test);
  },

  function folder_named_as_tag()
  {
    do_log_info("Regular folders with the same name as tag should not throw " +
                "off query results");
    var tagName = "foo";

    do_log_info("Add bookmark and tag it");
    addBookmark(TEST_URI);
    PlacesUtils.tagging.tagURI(TEST_URI, [tagName]);

    do_log_info("Create folder with same name as tag");
    var folderId = PlacesUtils.bookmarks.createFolder(PlacesUtils.unfiledBookmarksFolderId,
                                                      tagName,
                                                      Ci.nsINavBookmarksService.DEFAULT_INDEX);
    do_check_true(folderId > 0);

    do_log_info("Querying for tag should match URI");
    var [query, opts] = makeQuery([tagName]);
    opts.queryType = opts.QUERY_TYPE_BOOKMARKS;
    queryResultsAre(PlacesUtils.history.executeQuery(query, opts).root, [TEST_URI.spec]);

    PlacesUtils.tagging.untagURI(TEST_URI, [tagName]);
    cleanDatabase(run_next_test);
  },

  function ORed_queries() {
    do_log_info("Multiple queries ORed together should work");
    var urisAndTags = {
      "http://example.com/1": [],
      "http://example.com/2": []
    };

    // Search with lots of tags to make sure tag parameter substitution in SQL
    // can handle it with more than one query.
    for (let i = 0; i < 11; i++) {
      urisAndTags["http://example.com/1"].push("/1 tag " + i);
      urisAndTags["http://example.com/2"].push("/2 tag " + i);
    }

    do_log_info("Add visits and tag the URIs");
    for (let [pURI, tags] in Iterator(urisAndTags)) {
      let nsiuri = uri(pURI);
      addVisit(nsiuri);
      if (tags)
        PlacesUtils.tagging.tagURI(nsiuri, tags);
    }

    do_log_info("Query for /1 OR query for /2 should match both /1 and /2");
    var [query1, opts] = makeQuery(urisAndTags["http://example.com/1"]);
    var [query2, dummy] = makeQuery(urisAndTags["http://example.com/2"]);
    var root = PlacesUtils.history.executeQueries([query1, query2], 2, opts).root;
    queryResultsAre(root, ["http://example.com/1", "http://example.com/2"]);

    do_log_info("Query for /1 OR query on bogus tag should match only /1");
    [query1, opts] = makeQuery(urisAndTags["http://example.com/1"]);
    [query2, dummy] = makeQuery(["bogus"]);
    root = PlacesUtils.history.executeQueries([query1, query2], 2, opts).root;
    queryResultsAre(root, ["http://example.com/1"]);

    do_log_info("Query for /1 OR query for /1 should match only /1");
    [query1, opts] = makeQuery(urisAndTags["http://example.com/1"]);
    [query2, dummy] = makeQuery(urisAndTags["http://example.com/1"]);
    root = PlacesUtils.history.executeQueries([query1, query2], 2, opts).root;
    queryResultsAre(root, ["http://example.com/1"]);

    do_log_info("Query for /1 with tagsAreNot OR query for /2 with tagsAreNot " +
          "should match both /1 and /2");
    [query1, opts] = makeQuery(urisAndTags["http://example.com/1"], true);
    [query2, dummy] = makeQuery(urisAndTags["http://example.com/2"], true);
    root = PlacesUtils.history.executeQueries([query1, query2], 2, opts).root;
    queryResultsAre(root, ["http://example.com/1", "http://example.com/2"]);

    do_log_info("Query for /1 OR query for /2 with tagsAreNot should match " +
          "only /1");
    [query1, opts] = makeQuery(urisAndTags["http://example.com/1"]);
    [query2, dummy] = makeQuery(urisAndTags["http://example.com/2"], true);
    root = PlacesUtils.history.executeQueries([query1, query2], 2, opts).root;
    queryResultsAre(root, ["http://example.com/1"]);

    do_log_info("Query for /1 OR query for /1 with tagsAreNot should match " +
          "both URIs");
    [query1, opts] = makeQuery(urisAndTags["http://example.com/1"]);
    [query2, dummy] = makeQuery(urisAndTags["http://example.com/1"], true);
    root = PlacesUtils.history.executeQueries([query1, query2], 2, opts).root;
    queryResultsAre(root, ["http://example.com/1", "http://example.com/2"]);

    // Clean up.
    for (let [pURI, tags] in Iterator(urisAndTags)) {
      let nsiuri = uri(pURI);
      if (tags)
        PlacesUtils.tagging.untagURI(nsiuri, tags);
    }
    cleanDatabase(run_next_test);
  },

  function lastTestJustCallsTestFinished() {
    // close out our over-arching test
    do_test_finished();
    // and let run_next_test close out its pending test too.
    run_next_test();
  },
];

// The tag keys in query URIs, i.e., "place:tag=foo&!tags=1"
//                                          ---     -----
const QUERY_KEY_TAG      = "tag";
const QUERY_KEY_NOT_TAGS = "!tags";

const TEST_URI = uri("http://example.com/");

///////////////////////////////////////////////////////////////////////////////

/**
 * Adds a bookmark.
 *
 * @param aURI
 *        URI of the page (an nsIURI)
 */
function addBookmark(aURI) {
  var bmId = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                  aURI,
                                                  Ci.nsINavBookmarksService.DEFAULT_INDEX,
                                                  aURI.spec);
  do_log_info("Sanity check: insertBookmark should not fail");
  do_check_true(bmId > 0);
}

/**
 * Adds a visit to history.
 *
 * @param aURI
 *        URI of the page (an nsIURI)
 */
function addVisit(aURI) {
  var visitId = PlacesUtils.history.addVisit(aURI,
                                             Date.now() * 1000,
                                             null,
                                             Ci.nsINavHistoryService.TRANSITION_LINK,
                                             false,
                                             0);
  do_log_info("Sanity check: addVisit should not fail");
  do_check_true(visitId > 0);
}

/**
 * Removes all pages from history and bookmarks.
 */
function cleanDatabase(aCallback) {
  remove_all_bookmarks();
  waitForClearHistory(aCallback);
}

/**
 * Sets up a query with the specified tags, converts it to a URI, and makes sure
 * the URI is what we expect it to be.
 *
 * @param aTags
 *        The query's tags will be set to those in this array
 * @param aTagsAreNot
 *        The query's tagsAreNot property will be set to this
 */
function checkQueryURI(aTags, aTagsAreNot) {
  var pairs = (aTags || []).sort().map(function (t) QUERY_KEY_TAG + "=" + encodeTag(t));
  if (aTagsAreNot)
    pairs.push(QUERY_KEY_NOT_TAGS + "=1");
  var expURI = "place:" + pairs.join("&");
  var [query, opts] = makeQuery(aTags, aTagsAreNot);
  var actualURI = queryURI(query, opts);
  do_log_info("Query URI should be what we expect for the given tags");
  do_check_eq(actualURI, expURI);
}

/**
 * Executes a callback function in a "scoped" database state.  A bookmark
 * is added and tagged before the callback is called, and afterward the
 * database is cleared.
 *
 * @param aTags
 *        A bookmark will be added and tagged with this array of tags
 * @param aCallback
 *        A function that will be called after the bookmark has been tagged
 */
function doWithBookmark(aTags, aCallback) {
  addBookmark(TEST_URI);
  PlacesUtils.tagging.tagURI(TEST_URI, aTags);
  aCallback(TEST_URI);
  PlacesUtils.tagging.untagURI(TEST_URI, aTags);
  cleanDatabase(run_next_test);
}

/**
 * Executes a callback function in a "scoped" database state.  A history visit
 * is added and tagged before the callback is called, and afterward the
 * database is cleared.
 *
 * @param aTags
 *        A history visit will be added and tagged with this array of tags
 * @param aCallback
 *        A function that will be called after the visit has been tagged
 */
function doWithVisit(aTags, aCallback) {
  addVisit(TEST_URI);
  PlacesUtils.tagging.tagURI(TEST_URI, aTags);
  aCallback(TEST_URI);
  PlacesUtils.tagging.untagURI(TEST_URI, aTags);
  cleanDatabase(run_next_test);
}

/**
 * queriesToQueryString() encodes every character in the query URI that doesn't
 * match /[a-zA-Z]/.  There's no simple JavaScript function that does the same,
 * but encodeURIComponent() comes close, only missing some punctuation.  This
 * function takes care of all of that.
 *
 * @param  aTag
 *         A tag name to encode
 * @return A UTF-8 escaped string suitable for inclusion in a query URI
 */
function encodeTag(aTag) {
  return encodeURIComponent(aTag).
         replace(/[-_.!~*'()]/g, //'
                 function (s) "%" + s.charCodeAt(0).toString(16));
}

/**
 * Executes the given query and compares the results to the given URIs.
 * See queryResultsAre().
 *
 * @param aQuery
 *        An nsINavHistoryQuery
 * @param aQueryOpts
 *        An nsINavHistoryQueryOptions
 * @param aExpectedURIs
 *        Array of URIs (as strings) that aResultRoot should contain
 */
function executeAndCheckQueryResults(aQuery, aQueryOpts, aExpectedURIs) {
  var root = PlacesUtils.history.executeQuery(aQuery, aQueryOpts).root;
  root.containerOpen = true;
  queryResultsAre(root, aExpectedURIs);
  root.containerOpen = false;
}

/**
 * Returns new query and query options objects.  The query's tags will be
 * set to aTags.  aTags may be null, in which case setTags() is not called at
 * all on the query.
 *
 * @param  aTags
 *         The query's tags will be set to those in this array
 * @param  aTagsAreNot
 *         The query's tagsAreNot property will be set to this
 * @return [query, queryOptions]
 */
function makeQuery(aTags, aTagsAreNot) {
  aTagsAreNot = !!aTagsAreNot;
  do_log_info("Making a query " +
        (aTags ?
         "with tags " + aTags.toSource() :
         "without calling setTags() at all") +
        " and with tagsAreNot=" +
        aTagsAreNot);
  var query = PlacesUtils.history.getNewQuery();
  query.tagsAreNot = aTagsAreNot;
  if (aTags) {
    query.tags = aTags;
    var uniqueTags = [];
    aTags.forEach(function (t) {
      if (typeof(t) === "string" && uniqueTags.indexOf(t) < 0)
        uniqueTags.push(t);
    });
    uniqueTags.sort();
  }

  do_log_info("Made query should be correct for tags and tagsAreNot");
  if (uniqueTags)
    setsAreEqual(query.tags, uniqueTags, true);
  var expCount = uniqueTags ? uniqueTags.length : 0;
  do_check_eq(query.tags.length, expCount);
  do_check_eq(query.tagsAreNot, aTagsAreNot);

  return [query, PlacesUtils.history.getNewQueryOptions()];
}

/**
 * Ensures that the URIs of aResultRoot are the same as those in aExpectedURIs.
 *
 * @param aResultRoot
 *        The nsINavHistoryContainerResultNode root of an nsINavHistoryResult
 * @param aExpectedURIs
 *        Array of URIs (as strings) that aResultRoot should contain
 */
function queryResultsAre(aResultRoot, aExpectedURIs) {
  var rootWasOpen = aResultRoot.containerOpen;
  if (!rootWasOpen)
    aResultRoot.containerOpen = true;
  var actualURIs = [];
  for (let i = 0; i < aResultRoot.childCount; i++) {
    actualURIs.push(aResultRoot.getChild(i).uri);
  }
  setsAreEqual(actualURIs, aExpectedURIs);
  if (!rootWasOpen)
    aResultRoot.containerOpen = false;
}

/**
 * Converts the given query into its query URI.
 *
 * @param  aQuery
 *         An nsINavHistoryQuery
 * @param  aQueryOpts
 *         An nsINavHistoryQueryOptions
 * @return The query's URI
 */
function queryURI(aQuery, aQueryOpts) {
  return PlacesUtils.history.queriesToQueryString([aQuery], 1, aQueryOpts);
}

/**
 * Ensures that the arrays contain the same elements and, optionally, in the
 * same order.
 */
function setsAreEqual(aArr1, aArr2, aIsOrdered) {
  do_check_eq(aArr1.length, aArr2.length);
  if (aIsOrdered) {
    for (let i = 0; i < aArr1.length; i++) {
      do_check_eq(aArr1[i], aArr2[i]);
    }
  }
  else {
    aArr1.forEach(function (u) do_check_true(aArr2.indexOf(u) >= 0));
    aArr2.forEach(function (u) do_check_true(aArr1.indexOf(u) >= 0));
  }
}

///////////////////////////////////////////////////////////////////////////////

function run_test() {
  // Create an overarching test for our entire test run.  this is balanced
  // by a dummy test as the last test in our test list that just calls
  // do_test_finished().
  do_test_pending();
  run_next_test();
}
