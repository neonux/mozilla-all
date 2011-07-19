/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test the HOSTNAME() SQL function
var testData = [
  { uri: "http://www.mozilla.com/",
    hostname: "mozilla.com/" },

  { uri: "http://moz.is.awesome.com/",
    hostname: "moz.is.awesome.com/" },

  { uri: "http://moz.port.com:330/",
    hostname: "moz.port.com" },

  { uri: "http://www.mozilla.org/i/love/directories",
    hostname: "mozilla.org/" },
];


var conn = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;

function runHostname(uri) {
  var stmt = conn.createStatement("SELECT HOSTNAME(:uri) AS host");
  try {
    stmt.params.uri = uri;
    stmt.executeStep();
    return stmt.row["host"];
  }
  finally {
    stmt.finalize();
  }
}

function run_test() {
  testData.forEach(function (data)
  {
    do_check_eq(runHostname(data.uri), data.hostname);
  });
}
