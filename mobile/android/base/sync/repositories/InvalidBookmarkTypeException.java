/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import org.mozilla.gecko.sync.SyncException;

public class InvalidBookmarkTypeException extends SyncException {

  private static final long serialVersionUID = -6098516814844387449L;

  public InvalidBookmarkTypeException(Exception e) {
    super(e);
  }

}
