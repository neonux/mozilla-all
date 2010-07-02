_("Make sure Utils.deepEquals correctly finds items that are deeply equal");
Cu.import("resource://services-sync/util.js");

function run_test() {
  let data = '[NaN, undefined, null, true, false, Infinity, 0, 1, "a", "b", {a: 1}, {a: "a"}, [{a: 1}], [{a: true}], {a: 1, b: 2}, [1, 2], [1, 2, 3]]';
  _("Generating two copies of data:", data);
  let d1 = eval(data);
  let d2 = eval(data);

  d1.forEach(function(a) {
    _("Testing", a, typeof a, JSON.stringify([a]));
    let numMatch = 0;

    d2.forEach(function(b) {
      if (Utils.deepEquals(a, b)) {
        numMatch++;
        _("Found a match", b, typeof b, JSON.stringify([b]));
      }
    });

    let expect = 1;
    if (isNaN(a) && typeof a == "number") {
      expect = 0;
      _("Checking NaN should result in no matches");
    }

    _("Making sure we found the correct # match:", expect);
    _("Actual matches:", numMatch);
    do_check_eq(numMatch, expect);
  });

  _("Make sure adding undefined properties doesn't affect equalness");
  let a = {};
  let b = { a: undefined };
  do_check_true(Utils.deepEquals(a, b));
  a.b = 5;
  do_check_false(Utils.deepEquals(a, b));
  b.b = 5;
  do_check_true(Utils.deepEquals(a, b));
  a.c = undefined;
  do_check_true(Utils.deepEquals(a, b));
  b.d = undefined;
  do_check_true(Utils.deepEquals(a, b));
}
