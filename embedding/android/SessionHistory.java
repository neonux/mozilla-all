/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *   Vivien Nicolas <vnicolas@mozilla.com>
 *   Kartikaya Gupta <kgupta@mozilla.com>
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

package org.mozilla.gecko;

import java.util.Stack;

import android.content.*;
import android.database.sqlite.SQLiteDatabase;
import android.os.AsyncTask;
import android.util.Log;

class SessionHistory
{
    private final GeckoApp mApp;
    private final DatabaseHelper mDbHelper;
    private final Stack<HistoryEntry> mHistory;
    private SQLiteDatabase mDb;

    SessionHistory(GeckoApp app) {
        mApp = app;
        mDbHelper = new DatabaseHelper(app);
        mHistory = new Stack<HistoryEntry>();
    }

    void add(HistoryEntry entry) {
        new HistoryEntryTask().execute(entry);
    }

    void searchRequested(Intent searchIntent) {
        if (!mHistory.empty()) {
            searchIntent.putExtra(AwesomeBar.CURRENT_URL_KEY, mHistory.peek().mUri);
        }
    }

    boolean doReload() {
        if (mHistory.empty())
            return false;
        GeckoEvent e = new GeckoEvent("session-reload", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    boolean doBack() {
        if (mHistory.size() <= 1) {
            return false;
        }
        mHistory.pop();
        GeckoEvent e = new GeckoEvent("session-back", "");
        GeckoAppShell.sendEventToGecko(e);
        return true;
    }

    void cleanup() {
        if (mDb != null) {
            mDb.close();
        }
    }

    static class HistoryEntry {
        public final String mUri;
        public final String mTitle;

        public HistoryEntry(String uri, String title) {
            mUri = uri;
            mTitle = title;
        }
    }

    private class HistoryEntryTask extends AsyncTask<HistoryEntry, Void, Void> {
        protected Void doInBackground(HistoryEntry... entries) {
            HistoryEntry entry = entries[0];
            Log.d("GeckoApp", "adding uri=" + entry.mUri + ", title=" + entry.mTitle + " to history");
            ContentValues values = new ContentValues();
            values.put("url", entry.mUri);
            values.put("title", entry.mTitle);
            if (mHistory.empty() || !mHistory.peek().mUri.equals(entry.mUri))
                mHistory.push(entry);
            mDb = mDbHelper.getWritableDatabase();
            long id = mDb.insertWithOnConflict("moz_places", null, values, SQLiteDatabase.CONFLICT_REPLACE);
            values = new ContentValues();
            values.put("place_id", id);
            mDb.insertWithOnConflict("moz_historyvisits", null, values, SQLiteDatabase.CONFLICT_REPLACE);
            return null;
        }
    }
}
