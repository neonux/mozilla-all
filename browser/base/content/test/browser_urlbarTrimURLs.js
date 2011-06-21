function testVal(originalValue, tragetValue) {
  gURLBar.value = originalValue;
  is(gURLBar.value, tragetValue || originalValue, "original value: " + originalValue);
}

function test() {
  const prefname = "browser.urlbar.trimURLs";

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(prefname);
    URLBarSetURI();
  });

  Services.prefs.setBoolPref(prefname, true);

  testVal("http://mozilla.org/", "mozilla.org");
  testVal("https://mozilla.org/", "https://mozilla.org");
  testVal("http://mözilla.org/", "mözilla.org");
  testVal("http://mozilla.imaginatory/", "mozilla.imaginatory");
  testVal("http://www.mozilla.org/", "www.mozilla.org");
  testVal("http://sub.mozilla.org/", "sub.mozilla.org");
  testVal("http://sub1.sub2.sub3.mozilla.org/", "sub1.sub2.sub3.mozilla.org");
  testVal("http://mozilla.org/file.ext", "mozilla.org/file.ext");
  testVal("http://mozilla.org/sub/", "mozilla.org/sub/");

  testVal("http://ftp.mozilla.org/", "http://ftp.mozilla.org");
  testVal("ftp://ftp.mozilla.org/", "ftp://ftp.mozilla.org");

  testVal("https://user:pass@mozilla.org/", "https://user:pass@mozilla.org");
  testVal("http://user:pass@mozilla.org/", "http://user:pass@mozilla.org");
  testVal("http://sub.mozilla.org:666/", "sub.mozilla.org:666");

  testVal("https://[fe80::222:19ff:fe11:8c76]/file.ext");
  testVal("http://[fe80::222:19ff:fe11:8c76]/", "[fe80::222:19ff:fe11:8c76]");
  testVal("https://user:pass@[fe80::222:19ff:fe11:8c76]:666/file.ext");
  testVal("http://user:pass@[fe80::222:19ff:fe11:8c76]:666/file.ext");

  testVal("mailto:admin@mozilla.org");
  testVal("gopher://mozilla.org/");
  testVal("about:config");

  Services.prefs.setBoolPref(prefname, false);

  testVal("http://mozilla.org/");
}
