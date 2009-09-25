function run_test()
{
  // XXX Work around for the fact that for non-libxul builds loading gfx
  // components doesn't call up the layout initialisation routine. This should
  // be fixed/improved by bug 515595.
  Components.classes["@mozilla.org/layout/xul-boxobject-tree;1"]
            .createInstance(Components.interfaces.nsIBoxObject);

  let rgn = Components.classes["@mozilla.org/gfx/region;1"].createInstance(Components.interfaces.nsIScriptableRegion);
  do_check_true (rgn.getRects() === null)
  rgn.unionRect(0,0,80,60);
  do_check_true (rgn.getRects().toString() == "0,0,80,60")
  rgn.unionRect(90,70,1,1);
  do_check_true (rgn.getRects().toString() == "0,0,80,60,90,70,1,1")
}

