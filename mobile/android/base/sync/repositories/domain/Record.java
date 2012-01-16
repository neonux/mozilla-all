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
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jason Voll <jvoll@mozilla.com>
 * Richard Newman <rnewman@mozilla.com>
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

package org.mozilla.gecko.sync.repositories.domain;

import java.io.UnsupportedEncodingException;

import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ExtendedJSONObject;

public abstract class Record {
  // TODO: consider immutability, effective immutability, and thread-safety.
  public String guid;
  public String collection;
  public long lastModified;
  public boolean deleted;
  public long androidID;
  public long sortIndex;

  public Record(String guid, String collection, long lastModified, boolean deleted) {
    this.guid         = guid;
    this.collection   = collection;
    this.lastModified = lastModified;
    this.deleted      = deleted;
    this.sortIndex    = 0;
  }

  @Override
  public boolean equals(Object o) {
    if (o == null) {
      return false;
    }

    Record other = (Record) o;

    if (this.guid == null) {
      if (other.guid != null) {
        return false;
      }
    } else {
      if (!this.guid.equals(other.guid)) {
        return false;
      }
    }

    if (this.collection == null) {
      if (other.collection != null) {
        return false;
      }
    } else {
      if (!this.collection.equals(other.collection)) {
        return false;
      }
    }

    if (this.deleted != other.deleted) {
      return false;
    }
    return true;
  }

  public abstract void initFromPayload(CryptoRecord payload);
  public abstract CryptoRecord getPayload();

  public String toJSONString() {
    throw new RuntimeException("Cannot JSONify non-CryptoRecord Records.");
  }

  public byte[] toJSONBytes() {
    try {
      return this.toJSONString().getBytes("UTF-8");
    } catch (UnsupportedEncodingException e) {
      // Can't happen.
      return null;
    }
  }

  protected void checkGUIDs(ExtendedJSONObject payload) {
    String payloadGUID = (String) payload.get("id");
    if (this.guid == null ||
        payloadGUID == null) {
      String detailMessage = "Inconsistency: either envelope or payload GUID missing.";
      throw new IllegalStateException(detailMessage);
    }
    if (!this.guid.equals(payloadGUID)) {
      String detailMessage = "Inconsistency: record has envelope ID " + this.guid + ", payload ID " + payloadGUID;
      throw new IllegalStateException(detailMessage);
    }
  }
}
