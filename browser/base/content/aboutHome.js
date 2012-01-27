/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is aboutHome.xhtml.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (original author)
 *   Mihai Sucan <mihai.sucan@gmail.com>
 *   Frank Yan <fyan@mozilla.com>
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

// If a definition requires additional params, check that the final search url
// is handled correctly by the engine.
const SEARCH_ENGINES = {
  "Google": {
    params: "source=hp&channel=np"
  }
};

// The process of adding a new default snippet involves:
//   * add a new entity to aboutHome.dtd
//   * add a <span/> for it in aboutHome.xhtml
//   * add an entry here in the proper ordering (based on spans)
// The <a/> part of the snippet will be linked to the corresponding url.
const DEFAULT_SNIPPETS_URLS = [
  "http://www.mozilla.com/firefox/features/?WT.mc_ID=default1"
, "https://addons.mozilla.org/firefox/?src=snippet&WT.mc_ID=default2"
];

const SNIPPETS_UPDATE_INTERVAL_MS = 86400000; // 1 Day.

let gSearchEngine;

function onLoad(event)
{
  setupSearchEngine();
  document.getElementById("searchText").focus();

  loadSnippets();
}


function onSearchSubmit(aEvent)
{
  let searchTerms = document.getElementById("searchText").value;
  if (gSearchEngine && searchTerms.length > 0) {
    const SEARCH_TOKENS = {
      "_searchTerms_": encodeURIComponent(searchTerms)
    }
    let url = gSearchEngine.searchUrl;
    for (let key in SEARCH_TOKENS) {
      url = url.replace(key, SEARCH_TOKENS[key]);
    }
    window.location.href = url;
  }

  aEvent.preventDefault();
}


function setupSearchEngine()
{
  gSearchEngine = JSON.parse(localStorage["search-engine"]);

  if (!gSearchEngine)
    return;

  document.getElementById("searchSubmit").value = gSearchEngine.name;

  // Look for extended information.
  let searchEngineInfo = SEARCH_ENGINES[gSearchEngine.name];
  if (searchEngineInfo) {
    for (let prop in searchEngineInfo)
      gSearchEngine[prop] = searchEngineInfo[prop];
  }

  // Enqueue additional params if required by the engine definition.
  if (gSearchEngine.params)
    gSearchEngine.searchUrl += "&" + gSearchEngine.params;

}

function loadSnippets()
{
  // Check last snippets update.
  let lastUpdate = localStorage["snippets-last-update"];
  let updateURL = localStorage["snippets-update-url"];
  if (updateURL && (!lastUpdate ||
                    Date.now() - lastUpdate > SNIPPETS_UPDATE_INTERVAL_MS)) {
    // Even if fetching should fail we don't want to spam the server, thus
    // set the last update time regardless its results.  Will retry tomorrow.
    localStorage["snippets-last-update"] = Date.now();

    // Try to update from network.
    let xhr = new XMLHttpRequest();
    xhr.open('GET', updateURL, true);
    xhr.onerror = function (event) {
      showSnippets();
    };
    xhr.onload = function (event)
    {
      if (xhr.status == 200) {
        localStorage["snippets"] = xhr.responseText;
      }
      showSnippets();
    };
    xhr.send(null);
  } else {
    showSnippets();
  }
}

function showSnippets()
{
  let snippetsElt = document.getElementById("snippets");
  let snippets = localStorage["snippets"];
  // If there are remotely fetched snippets, try to to show them.
  if (snippets) {
    // Injecting snippets can throw if they're invalid XML.
    try {
      snippetsElt.innerHTML = snippets;
      // Scripts injected by innerHTML are inactive, so we have to relocate them
      // through DOM manipulation to activate their contents.
      Array.forEach(snippetsElt.getElementsByTagName("script"), function(elt) {
        let relocatedScript = document.createElement("script");
        relocatedScript.type = "text/javascript;version=1.8";
        relocatedScript.text = elt.text;
        elt.parentNode.replaceChild(relocatedScript, elt);
      });
      return;
    } catch (ex) {
      // Bad content, continue to show default snippets.
    }
  }

  // Show default snippets otherwise.
  let defaultSnippetsElt = document.getElementById("defaultSnippets");
  let entries = defaultSnippetsElt.querySelectorAll("span");
  // Choose a random snippet.  Assume there is always at least one.
  let randIndex = Math.floor(Math.random() * entries.length);
  let entry = entries[randIndex];
  // Inject url in the eventual link.
  if (DEFAULT_SNIPPETS_URLS[randIndex]) {
    let links = entry.getElementsByTagName("a");
    // Default snippets can have only one link, otherwise something is messed
    // up in the translation.
    if (links.length == 1) {
      links[0].href = DEFAULT_SNIPPETS_URLS[randIndex];
    }
  }
  // Move the default snippet to the snippets element.
  snippetsElt.appendChild(entry);
}
