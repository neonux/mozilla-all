// defines for prompt - button position
const ACCEPT = 0;
const ACCEPT_FUZZ = 1;
const DECLINE = 2;

/* Set if testLocationProvider is registered as geolocation provider.
 * To register, copy testLocationProvider.js to components directory 
 * and remove from directory when done. 
 */
var TEST_PROVIDER = 0; 

// set if there should be a delay before prompt is accepted
var DELAYED_PROMPT = 0;

var prompt_delay = timeout * 2;

// the prompt that was registered at runtime
var old_prompt;

// whether the prompt was accepted
var prompted = 0;

// which prompt option to select when prompt is fired
var promptOption;

// number of position changes for testLocationProvider to make
var num_pos_changes = 3;

 // based on testLocationProvider's interval
var timer_interval = 500;
var slack = 500;

// time needed for provider to make position changes
var timeout = num_pos_changes * timer_interval + slack;

function check_geolocation(location) {

  ok(location, "Check to see if this location is non-null");

  ok("latitude" in location, "Check to see if there is a latitude");
  ok("longitude" in location, "Check to see if there is a longitude");
  ok("altitude" in location, "Check to see if there is a altitude");
  ok("accuracy" in location, "Check to see if there is a accuracy");
  ok("altitudeAccuracy" in location, "Check to see if there is a alt accuracy");
  ok("heading" in location, "Check to see if there is a heading");
  ok("velocity" in location, "Check to see if there is a velocity");
  ok("timestamp" in location, "Check to see if there is a timestamp");

}

//TODO: test for fuzzed location when this is implemented
function check_fuzzed_geolocation(location) {
  check_geolocation(location);
}

function check_no_geolocation(location) {
   ok(!location, "Check to see if this location is null");
}

function checkFlags(flags, value, isExact) {
  for(var i = 0; i < flags.length; i++)
    ok(isExact ? flags[i] == value : flags[i] >= value, "ensure callbacks called " + value + " times");
}

function success_callback(position) {
  if(prompted == 0)
    ok(0, "Should not call success callback before prompt accepted");
  if(position == null)
    ok(1, "No geolocation available");
  else {
    switch(promptOption) {
      case ACCEPT:
        check_geolocation(position);
        break;
      case ACCEPT_FUZZ:
        check_fuzzed_geolocation(position);
        break;
      case DECLINE:
        check_no_geolocation(position);
        break;
      default:
        break;
    }
  }
}

function geolocation_prompt(request) {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
	prompted = 1;
  switch(promptOption) {
    case ACCEPT:
      request.allow();
      break;
    case ACCEPT_FUZZ:
      request.allowButFuzz();
      break;
    case DECLINE:
      request.cancel();
      break;
    default:
      break;
  }
  return 1;
}

function delayed_prompt(request) {
  setTimeout(geolocation_prompt, prompt_delay, request);
}

function attachPrompt() {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  var geolocationService = Components.classes["@mozilla.org/geolocation/service;1"]
                           .getService(Components.interfaces.nsIGeolocationService);
  old_prompt = geolocationService.prompt;

  if(DELAYED_PROMPT)
    geolocationService.prompt = delayed_prompt;
  else
    geolocationService.prompt = geolocation_prompt;
}

function removePrompt() {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  var geolocationService = Components.classes["@mozilla.org/geolocation/service;1"]
                           .getService(Components.interfaces.nsIGeolocationService);
  geolocationService.prompt = old_prompt;
}
