#ifdef 0
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
 * The Original Code is New Tab Page code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Taubert <ttaubert@mozilla.com> (Original Author)
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
#endif

// TODO
function Link(aUrl, aTitle) {
  this._url = aUrl;
  this._title = aTitle;
}

Link.unserialize = function (aData) {
  return new Link(aData.url, aData.title);
};

Link.prototype = {
  get url() this._url,
  get title() this._title || this._url,

  block: function Link_block() {
    Links.block(this);
  },

  pin: function Link_pin(aIndex) {
    Links.pin(this, aIndex);
  },

  unpin: function Link_unpin() {
    Links.unpin(this);
  },

  isPinned: function Link_isPinned() {
    return Links.isPinned(this);
  },

  serialize: function Link_serialize() {
    return {url: this.url, title: this.title};
  }
};

// TODO
let Links = {
  getLinks: function Links_getLinks(aCallback) {
    let pinnedLinks = this._pinnedLinks.concat();
    let blockedLinks = this._blockedLinks;

    NewTabUtils.Links.fetchLinks(function (links) {
      // create Link objects
      links = links.map(function (link) {
        return new Link(link.url, link.title);
      });

      // filter blocked and pinned links
      links = links.filter(function (link) {
        return !(link.url in blockedLinks) && !link.isPinned();
      });

      // unserialize all pinned links
      for (let i = 0; i < pinnedLinks.length; i++)
        if (pinnedLinks[i])
          pinnedLinks[i] = Link.unserialize(pinnedLinks[i]);

      // try to fill the gaps between pinned links
      for (let i = 0; i < pinnedLinks.length && links.length; i++)
        if (!pinnedLinks[i])
          pinnedLinks[i] = links.shift();

      // append the remaining links if any
      if (links.length)
        pinnedLinks = pinnedLinks.concat(links);

      aCallback(pinnedLinks);
    });
  },

  block: function Links_block(aLink) {
    this._blockedLinks[aLink.url] = 1;
    NewTabUtils.Storage.set("blockedLinks", this._blockedLinks);

    // make sure we unpin blocked links
    if (!this.unpin(aLink))
      NewTabUtils.Pages.refresh(Page);
  },

  pin: function Links_pin(aLink, aIndex) {
    this._pinnedLinks[aIndex] = aLink.serialize();
    NewTabUtils.Storage.set("pinnedLinks", this._pinnedLinks);
    NewTabUtils.Pages.refresh(Page);
  },

  unpin: function Links_unpin(aLink) {
    let index = this._pinnedIndexOf(aLink);

    if (index > -1) {
      this._pinnedLinks[index] = null;
      NewTabUtils.Storage.set("pinnedLinks", this._pinnedLinks);
      NewTabUtils.Pages.refresh(Page);

      return true;
    }

    return false;
  },

  isPinned: function Links_isPinned(aLink) {
    return this._pinnedIndexOf(aLink) > -1;
  },

  reset: function Links_reset() {
    this._initStorageValues();
  },

  _init: function Links__init() {
    this._initStorageValues();
  },

  _initStorageValues: function Links__initStorageValues() {
    this._pinnedLinks = NewTabUtils.Storage.get("pinnedLinks", []);
    this._blockedLinks = NewTabUtils.Storage.get("blockedLinks", {});
  },

  _pinnedIndexOf: function Links__pinnedIndexOf(aLink) {
    let pinned = this._pinnedLinks;
    let url = aLink.url;

    for (let i = 0; i < pinned.length; i++)
      if (pinned[i] && pinned[i].url == url)
        return i;

    return -1;
  }
};

Links._init();

