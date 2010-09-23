Cu.import("resource://services-sync/engines/tabs.js");
Cu.import("resource://services-sync/engines/clients.js");
Cu.import("resource://services-sync/util.js");

function fakeSvcWinMediator() {
  // actions on windows are captured in logs
  let logs = [];
  delete Svc.WinMediator;
  Svc.WinMediator = {
    getEnumerator: function() {
      return {
        cnt: 2,
        hasMoreElements: function() this.cnt-- > 0,
        getNext: function() {
          let elt = {addTopics: [], remTopics: []};
          logs.push(elt);
          return {
            addEventListener: function(topic) {
              elt.addTopics.push(topic);
            },
            removeEventListener: function(topic) {
              elt.remTopics.push(topic);
            }
          };
        }
      };
    }
  };
  return logs;
}

function fakeSvcSession() {
  // actions on Session are captured in logs
  let logs = [];
  delete Svc.Session;
  Svc.Session = {
    setTabValue: function(target, prop, value) {
      logs.push({target: target, prop: prop, value: value});
    }
  };
  return logs;
}

function run_test() {
  let engine = new TabEngine();
  engine._store.wipe();

  _("Verify we have an empty tracker to work with");
  let tracker = engine._tracker;
  do_check_eq([id for (id in tracker.changedIDs)].length, 0);

  let logs;

  _("Test listeners are registered on windows");
  logs = fakeSvcWinMediator();
  Svc.Obs.notify("weave:engine:start-tracking");
  do_check_eq(logs.length, 2);
  for each (let log in logs) {
    do_check_eq(log.addTopics.length, 5);
    do_check_true(log.addTopics.indexOf("pageshow") >= 0);
    do_check_true(log.addTopics.indexOf("TabOpen") >= 0);
    do_check_true(log.addTopics.indexOf("TabClose") >= 0);
    do_check_true(log.addTopics.indexOf("TabSelect") >= 0);
    do_check_true(log.addTopics.indexOf("unload") >= 0);
    do_check_eq(log.remTopics.length, 0);
  }

  _("Test listeners are unregistered on windows");
  logs = fakeSvcWinMediator();
  Svc.Obs.notify("weave:engine:stop-tracking");
  do_check_eq(logs.length, 2);
  for each (let log in logs) {
    do_check_eq(log.addTopics.length, 0);
    do_check_eq(log.remTopics.length, 5);
    do_check_true(log.remTopics.indexOf("pageshow") >= 0);
    do_check_true(log.remTopics.indexOf("TabOpen") >= 0);
    do_check_true(log.remTopics.indexOf("TabClose") >= 0);
    do_check_true(log.remTopics.indexOf("TabSelect") >= 0);
    do_check_true(log.remTopics.indexOf("unload") >= 0);
  }

  _("Test tab listener");
  logs = fakeSvcSession();
  let idx = 0;
  for each (let evttype in ["TabOpen", "TabClose", "TabSelect"]) {
    tracker.onTab({type: evttype , originalTarget: evttype});
    do_check_eq([id for (id in tracker.changedIDs)].length, 1);
    do_check_eq(logs.length, idx+1);
    do_check_eq(logs[idx].target, evttype);
    do_check_eq(logs[idx].prop, "weaveLastUsed");
    do_check_true(typeof logs[idx].value == "number");
    idx++;
  }
  tracker.onTab({type: "pageshow", originalTarget: "pageshow"});
  do_check_eq([id for (id in tracker.changedIDs)].length, 1);
  do_check_eq(logs.length, idx); // test that setTabValue isn't called
}
