/*   ***** BEGIN LICENSE BLOCK *****
   - This Source Code Form is subject to the terms of the Mozilla Public License,
   - v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/.
   -
   - Contributor(s):
   -   Jonathan Rietveld <jon.rietveld@gmail.com>
   -
   - ***** END LICENSE BLOCK ***** */

const Ci = Components.interfaces;
var currentPageState;
	
function gotoPref(page) {
  document.getElementById(window.history.state).hidden = true;
  window.history.pushState(page,"Preferences");
  document.getElementById(page).hidden = false;
  document.getElementById("back-btn").disabled = false;
}

function cmd_back() {
  currentPageState = window.history.state;
  document.getElementById("forward-btn").disabled = false;
  window.history.back();
}
	
function cmd_forward() {
  currentPageState = window.history.state;
  window.history.forward();
}

window.onpopstate = function(event) {
  document.getElementById(currentPageState).hidden = true;
  window.history.pushState(event.state,"Preferences");
  document.getElementById(event.state).hidden = false;
}

function init_all() {
  window.history.pushState('landing-content', "Preferences");
}