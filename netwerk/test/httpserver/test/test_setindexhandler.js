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
 * The Original Code is httpd.js code.
 *
 * The Initial Developer of the Original Code is
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// Make sure setIndexHandler works as expected

var srv, serverBasePath;

function run_test()
{
  srv = createServer();
  serverBasePath = do_get_cwd();
  srv.registerDirectory("/", serverBasePath);
  srv.setIndexHandler(myIndexHandler);
  srv.start(4444);

  runHttpTests(tests, testComplete(srv));
}


var tests = [];
var test;

test = new Test("http://localhost:4444/",
                init, startCustomIndexHandler, stopCustomIndexHandler);
tests.push(test);
function init(ch)
{
  ch.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE; // important!
}
function startCustomIndexHandler(ch, cx)
{
  do_check_eq(ch.getResponseHeader("Content-Length"), "10");
  srv.setIndexHandler(null);
}
function stopCustomIndexHandler(ch, cx, status, data)
{
  do_check_true(Components.isSuccessCode(status));
  do_check_eq(String.fromCharCode.apply(null, data), "directory!");
}

test = new Test("http://localhost:4444/",
                init, startDefaultIndexHandler, stopDefaultIndexHandler);
tests.push(test);
function startDefaultIndexHandler(ch, cx)
{
  do_check_eq(ch.responseStatus, 200);
}
function stopDefaultIndexHandler(ch, cx, status, data)
{
  do_check_true(Components.isSuccessCode(status));
}

// PATH HANDLERS

function myIndexHandler(metadata, response)
{
  var dir = metadata.getProperty("directory");
  do_check_true(dir != null);
  do_check_true(dir instanceof Ci.nsIFile);
  do_check_true(dir.equals(serverBasePath));

  response.write("directory!");
}
