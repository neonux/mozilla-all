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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lucas Rocha <lucasr@mozilla.com>
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

package org.mozilla.gecko.db;

import java.io.File;
import java.util.HashMap;
import java.util.UUID;

import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.CommonColumns;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.db.BrowserContract.Images;

import android.content.ContentProvider;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

public class BrowserProvider extends ContentProvider {
    private static final String LOGTAG = "GeckoBrowserProvider";

    static final String DATABASE_NAME = "browser.db";

    static final String TABLE_BOOKMARKS = "bookmarks";
    static final String TABLE_HISTORY = "history";
    static final String TABLE_IMAGES = "images";

    // Bookmark matches
    static final int BOOKMARKS = 100;
    static final int BOOKMARKS_ID = 101;
    static final int BOOKMARKS_FOLDER_ID = 102;

    // History matches
    static final int HISTORY = 200;
    static final int HISTORY_ID = 201;

    // Image matches
    static final int IMAGES = 300;

    static final String DEFAULT_BOOKMARKS_SORT_ORDER = Bookmarks.IS_FOLDER
            + " DESC, " + Bookmarks.POSITION + " ASC, " + Bookmarks._ID
            + " ASC";

    static final String DEFAULT_HISTORY_SORT_ORDER = History.DATE_LAST_VISITED + " DESC";

    static final String TABLE_BOOKMARKS_JOIN_IMAGES = TABLE_BOOKMARKS + " LEFT OUTER JOIN " +
            TABLE_IMAGES + " ON " + qualifyColumnValue(TABLE_BOOKMARKS, Bookmarks.URL) +
            " = " + qualifyColumnValue(TABLE_IMAGES, Images.URL);

    static final String TABLE_HISTORY_JOIN_IMAGES = TABLE_HISTORY + " LEFT OUTER JOIN " +
            TABLE_IMAGES + " ON " + qualifyColumnValue(TABLE_HISTORY, History.URL) +
            " = " + qualifyColumnValue(TABLE_IMAGES, Images.URL);

    static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    static final HashMap<String, String> BOOKMARKS_PROJECTION_MAP = new HashMap<String, String>();
    static final HashMap<String, String> HISTORY_PROJECTION_MAP = new HashMap<String, String>();
    static final HashMap<String, String> IMAGES_PROJECTION_MAP = new HashMap<String, String>();

    private HashMap<String, DatabaseHelper> mDatabasePerProfile;

    static {
        HashMap<String, String> map;

        // Bookmarks
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks", BOOKMARKS);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks/#", BOOKMARKS_ID);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks/folder/#", BOOKMARKS_FOLDER_ID);

        map = BOOKMARKS_PROJECTION_MAP;
        map.put(Bookmarks._ID, qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID));
        map.put(Bookmarks.TITLE, Bookmarks.TITLE);
        map.put(Bookmarks.URL, Bookmarks.URL);
        map.put(Bookmarks.FAVICON, Bookmarks.FAVICON);
        map.put(Bookmarks.THUMBNAIL, Bookmarks.THUMBNAIL);
        map.put(Bookmarks.IS_FOLDER, Bookmarks.IS_FOLDER);
        map.put(Bookmarks.PARENT, Bookmarks.PARENT);
        map.put(Bookmarks.POSITION, Bookmarks.POSITION);
        map.put(Bookmarks.DATE_CREATED, qualifyColumn(TABLE_BOOKMARKS, Bookmarks.DATE_CREATED));
        map.put(Bookmarks.DATE_MODIFIED, qualifyColumn(TABLE_BOOKMARKS, Bookmarks.DATE_MODIFIED));
        map.put(Bookmarks.GUID, qualifyColumn(TABLE_BOOKMARKS, Bookmarks.GUID));

        // History
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "history", HISTORY);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "history/#", HISTORY_ID);

        map = HISTORY_PROJECTION_MAP;
        map.put(History._ID, qualifyColumn(TABLE_HISTORY, History._ID));
        map.put(History.TITLE, History.TITLE);
        map.put(History.URL, History.URL);
        map.put(History.FAVICON, History.FAVICON);
        map.put(History.THUMBNAIL, History.THUMBNAIL);
        map.put(History.VISITS, History.VISITS);
        map.put(History.DATE_LAST_VISITED, History.DATE_LAST_VISITED);
        map.put(History.DATE_CREATED, qualifyColumn(TABLE_HISTORY, History.DATE_CREATED));
        map.put(History.DATE_MODIFIED, qualifyColumn(TABLE_HISTORY, History.DATE_MODIFIED));
        map.put(History.GUID, qualifyColumn(TABLE_HISTORY, History.GUID));

        // Images
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "images", IMAGES);

        map = IMAGES_PROJECTION_MAP;
        map.put(Images.URL, Images.URL);
        map.put(Images.FAVICON, Images.FAVICON);
        map.put(Images.FAVICON_URL, Images.FAVICON_URL);
        map.put(Images.THUMBNAIL, Images.THUMBNAIL);
        map.put(Images.DATE_CREATED, qualifyColumn(TABLE_IMAGES, Images.DATE_CREATED));
        map.put(Images.DATE_MODIFIED, qualifyColumn(TABLE_IMAGES, Images.DATE_MODIFIED));
        map.put(Images.GUID, qualifyColumn(TABLE_IMAGES, Images.GUID));
    }

    static final String qualifyColumn(String table, String column) {
        return table + "." + column + " AS " + column;
    }

    static final String qualifyColumnValue(String table, String column) {
        return table + "." + column;
    }

    // This is available in Android >= 11. Implemented locally to be
    // compatible with older versions.
    public static String concatenateWhere(String a, String b) {
        if (TextUtils.isEmpty(a)) {
            return b;
        }

        if (TextUtils.isEmpty(b)) {
            return a;
        }

        return "(" + a + ") AND (" + b + ")";
    }

    // This is available in Android >= 11. Implemented locally to be
    // compatible with older versions.
    public static String[] appendSelectionArgs(String[] originalValues, String[] newValues) {
        if (originalValues == null || originalValues.length == 0) {
            return newValues;
        }

        if (newValues == null || newValues.length == 0) {
            return originalValues;
        }

        String[] result = new String[originalValues.length + newValues.length];
        System.arraycopy(originalValues, 0, result, 0, originalValues.length);
        System.arraycopy(newValues, 0, result, originalValues.length, newValues.length);

        return result;
    }

    final class DatabaseHelper extends SQLiteOpenHelper {
        static final int DATABASE_VERSION = 1;

        public DatabaseHelper(Context context, String databasePath) {
            super(context, databasePath, null, DATABASE_VERSION);
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            Log.d(LOGTAG, "Creating browser.db: " + db.getPath());

            Log.d(LOGTAG, "Creating " + TABLE_BOOKMARKS + " table");
            db.execSQL("CREATE TABLE " + TABLE_BOOKMARKS + "(" +
                    Bookmarks._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                    Bookmarks.TITLE + " TEXT," +
                    Bookmarks.URL + " TEXT," +
                    Bookmarks.IS_FOLDER + " INTEGER NOT NULL DEFAULT 0," +
                    Bookmarks.PARENT + " INTEGER," +
                    Bookmarks.POSITION + " INTEGER NOT NULL," +
                    Bookmarks.DATE_CREATED + " INTEGER," +
                    Bookmarks.DATE_MODIFIED + " INTEGER," +
                    Bookmarks.GUID + " TEXT" +
                    ");");

            Log.d(LOGTAG, "Creating " + TABLE_HISTORY + " table");
            db.execSQL("CREATE TABLE " + TABLE_HISTORY + "(" +
                    History._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                    History.TITLE + " TEXT," +
                    History.URL + " TEXT NOT NULL," +
                    History.VISITS + " INTEGER NOT NULL DEFAULT 0," +
                    History.DATE_LAST_VISITED + " INTEGER," +
                    History.DATE_CREATED + " INTEGER," +
                    History.DATE_MODIFIED + " INTEGER," +
                    History.GUID + " TEXT" +
                    ");");

            Log.d(LOGTAG, "Creating " + TABLE_IMAGES + " table");
            db.execSQL("CREATE TABLE " + TABLE_IMAGES + " (" +
                    Images.URL + " TEXT UNIQUE NOT NULL," +
                    Images.FAVICON + " BLOB," +
                    Images.FAVICON_URL + " TEXT," +
                    Images.THUMBNAIL + " BLOB," +
                    Images.DATE_CREATED + " INTEGER," +
                    Images.DATE_MODIFIED + " INTEGER," +
                    Images.GUID + " TEXT" +
                    ");");

            db.execSQL("CREATE INDEX images_url_index ON " + TABLE_IMAGES + "("
                    + Images.URL + ")");

            // FIXME: Create default bookmarks here
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            Log.d(LOGTAG, "Upgrading browser.db: " + db.getPath() + " from " +
                    oldVersion + " to " + newVersion);

            // Do nothing for now
        }

        @Override
        public void onOpen(SQLiteDatabase db) {
            Log.d(LOGTAG, "Opening browser.db: " + db.getPath());

            // From Honeycomb on, it's possible to run several db
            // commands in parallel using multiple connections.
            if (Build.VERSION.SDK_INT >= 11)
                db.enableWriteAheadLogging();
        }
    }

    private DatabaseHelper getDatabaseHelperForProfile(String profile) {
        Log.d(LOGTAG, "Getting database helper for profile: " + profile);

        // Each profile has a separate browser.db database. The target
        // profile is provided using a URI query argument in each request
        // to our content provider.

        // Always fallback to default profile if non has been provided
        if (TextUtils.isEmpty(profile)) {
            Log.d(LOGTAG, "No profile provided, using default");
            profile = BrowserContract.DEFAULT_PROFILE;
        }

        DatabaseHelper dbHelper = mDatabasePerProfile.get(profile);

        if (dbHelper == null) {
            synchronized (this) {
                dbHelper = new DatabaseHelper(getContext(), getDatabasePath(profile));
                mDatabasePerProfile.put(profile, dbHelper);
            }
        }

        Log.d(LOGTAG, "Successfully created database helper for profile: " + profile);

        return dbHelper;
    }

    private String getDatabasePath(String profile) {
        Log.d(LOGTAG, "Getting database path for profile: " + profile);

        // On Android releases older than 2.3, it's not possible to use
        // SQLiteOpenHelper with a full path. Fallback to using separate
        // db files per profile in the app directory.
        if (Build.VERSION.SDK_INT <= 8) {
            return "browser-" + profile + ".db";
        }

        File profileDir = GeckoApp.mAppContext.getProfileDir(profile);
        if (profileDir == null) {
            Log.d(LOGTAG, "Couldn't find directory for profile: " + profile);
            return null;
        }

        String databasePath = new File(profileDir, DATABASE_NAME).getAbsolutePath();
        Log.d(LOGTAG, "Successfully created database path for profile: " + databasePath);

        return databasePath;
    }

    private SQLiteDatabase getReadableDatabase(Uri uri) {
        Log.d(LOGTAG, "Getting readable database for URI: " + uri);

        String profile = null;

        if (uri != null)
            profile = uri.getQueryParameter(BrowserContract.PARAM_PROFILE);

        return getDatabaseHelperForProfile(profile).getReadableDatabase();
    }

    private SQLiteDatabase getWritableDatabase(Uri uri) {
        Log.d(LOGTAG, "Getting writable database for URI: " + uri);

        String profile = null;

        if (uri != null)
            profile = uri.getQueryParameter(BrowserContract.PARAM_PROFILE);

        return getDatabaseHelperForProfile(profile).getWritableDatabase();
    }

    @Override
    public boolean onCreate() {
        Log.d(LOGTAG, "Creating BrowserProvider");

        mDatabasePerProfile = new HashMap<String, DatabaseHelper>();

        return true;
    }

    @Override
    public String getType(Uri uri) {
        final int match = URI_MATCHER.match(uri);

        Log.d(LOGTAG, "Getting URI type: " + uri);

        switch (match) {
            case BOOKMARKS:
                Log.d(LOGTAG, "URI is BOOKMARKS: " + uri);
                return Bookmarks.CONTENT_TYPE;
            case BOOKMARKS_ID:
                Log.d(LOGTAG, "URI is BOOKMARKS_ID: " + uri);
                return Bookmarks.CONTENT_ITEM_TYPE;
            case HISTORY:
                Log.d(LOGTAG, "URI is HISTORY: " + uri);
                return History.CONTENT_TYPE;
            case HISTORY_ID:
                Log.d(LOGTAG, "URI is HISTORY_ID: " + uri);
                return History.CONTENT_ITEM_TYPE;
        }

        Log.d(LOGTAG, "URI has unrecognized type: " + uri);

        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        Log.d(LOGTAG, "Calling delete on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int deleted = 0;

        if (Build.VERSION.SDK_INT >= 11) {
            Log.d(LOGTAG, "Beginning delete transaction: " + uri);
            db.beginTransaction();
            try {
                deleted = deleteInTransaction(uri, selection, selectionArgs);
                db.setTransactionSuccessful();
                Log.d(LOGTAG, "Successful delete transaction: " + uri);
            } finally {
                db.endTransaction();
            }
        } else {
            deleted = deleteInTransaction(uri, selection, selectionArgs);
        }

        return deleted;
    }

    public int deleteInTransaction(Uri uri, String selection, String[] selectionArgs) {
        Log.d(LOGTAG, "Calling delete in transaction on URI: " + uri);

        final int match = URI_MATCHER.match(uri);
        int deleted = 0;

        switch (match) {
            case BOOKMARKS_ID:
                Log.d(LOGTAG, "Delete on BOOKMARKS_ID: " + uri);

                selection = concatenateWhere(selection, TABLE_BOOKMARKS + "._id = ?");
                selectionArgs = appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case BOOKMARKS: {
                Log.d(LOGTAG, "Deleting bookmarks: " + uri);
                deleted = deleteBookmarks(uri, selection, selectionArgs);
                deleteUnusedImages(uri);
                break;
        }

        case HISTORY_ID:
            Log.d(LOGTAG, "Delete on HISTORY_ID: " + uri);

            selection = concatenateWhere(selection, TABLE_HISTORY + "._id = ?");
            selectionArgs = appendSelectionArgs(selectionArgs,
                    new String[] { Long.toString(ContentUris.parseId(uri)) });
            // fall through
        case HISTORY: {
            Log.d(LOGTAG, "Deleting history: " + uri);
            deleted = deleteHistory(uri, selection, selectionArgs);
            deleteUnusedImages(uri);
            break;
        }

        default:
            throw new UnsupportedOperationException("Unknown delete URI " + uri);
        }

        Log.d(LOGTAG, "Deleted " + deleted + " rows for URI: " + uri);

        return deleted;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        Log.d(LOGTAG, "Calling insert on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        Uri result = null;

        if (Build.VERSION.SDK_INT >= 11) {
            Log.d(LOGTAG, "Beginning insert transaction: " + uri);
            db.beginTransaction();
            try {
                result = insertInTransaction(uri, values);
                db.setTransactionSuccessful();
                Log.d(LOGTAG, "Successful insert transaction: " + uri);
            } finally {
                db.endTransaction();
            }
        } else {
            result = insertInTransaction(uri, values);
        }

        return result;
    }

    public Uri insertInTransaction(Uri uri, ContentValues values) {
        Log.d(LOGTAG, "Calling insert in transaction on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int match = URI_MATCHER.match(uri);
        long id = -1;

        switch (match) {
            case BOOKMARKS: {
                Log.d(LOGTAG, "Insert on BOOKMARKS: " + uri);

                long now = System.currentTimeMillis();
                values.put(Bookmarks.DATE_CREATED, now);
                values.put(Bookmarks.DATE_MODIFIED, now);

                // Generate GUID for new bookmark
                values.put(Bookmarks.GUID, UUID.randomUUID().toString());

                if (!values.containsKey(Bookmarks.POSITION)) {
                    Log.d(LOGTAG, "Inserting bookmark with no position for URI");
                    values.put(Bookmarks.POSITION, Long.toString(Long.MIN_VALUE));
                }

                String url = values.getAsString(Bookmarks.URL);
                ContentValues imageValues = extractImageValues(values, url);
                Boolean isFolder = values.getAsBoolean(Bookmarks.IS_FOLDER);

                if ((isFolder == null || !isFolder) && imageValues != null
                        && !TextUtils.isEmpty(url)) {
                    Log.d(LOGTAG, "Inserting bookmark image for URL: " + url);
                    updateImage(uri, imageValues, Images.URL + " = ?", new String[] { url });
                }

                Log.d(LOGTAG, "Inserting bookmark in database with URL: " + url);
                id = db.insertOrThrow(TABLE_BOOKMARKS, Bookmarks.TITLE, values);
                break;
            }

            case HISTORY: {
                Log.d(LOGTAG, "Insert on HISTORY: " + uri);

                long now = System.currentTimeMillis();
                values.put(History.DATE_CREATED, now);
                values.put(History.DATE_MODIFIED, now);

                // Generate GUID for new history entry
                values.put(History.GUID, UUID.randomUUID().toString());

                String url = values.getAsString(History.URL);

                ContentValues imageValues = extractImageValues(values,
                        values.getAsString(History.URL));

                if (imageValues != null) {
                    Log.d(LOGTAG, "Inserting history image for URL: " + url);
                    updateImage(uri, imageValues, Images.URL + " = ?", new String[] { url });
                }

                Log.d(LOGTAG, "Inserting history in database with URL: " + url);
                id = db.insertOrThrow(TABLE_HISTORY, History.VISITS, values);
                break;
            }

            default:
                throw new UnsupportedOperationException("Unknown insert URI " + uri);
        }

        Log.d(LOGTAG, "Inserted ID in database: " + id);

        if (id >= 0)
            return ContentUris.withAppendedId(uri, id);

        return null;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        Log.d(LOGTAG, "Calling update on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int updated = 0;

        if (Build.VERSION.SDK_INT >= 11) {
            Log.d(LOGTAG, "Beginning update transaction: " + uri);
            db.beginTransaction();
            try {
                updated = updateInTransaction(uri, values, selection, selectionArgs);
                db.setTransactionSuccessful();
                Log.d(LOGTAG, "Successful update transaction: " + uri);
            } finally {
                db.endTransaction();
            }
        } else {
            updated = updateInTransaction(uri, values, selection, selectionArgs);
        }

        return updated;
    }

    public int updateInTransaction(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        Log.d(LOGTAG, "Calling update in transaction on URI: " + uri);

        int match = URI_MATCHER.match(uri);
        int updated = 0;

        switch (match) {
            case BOOKMARKS_ID:
                Log.d(LOGTAG, "Update on BOOKMARKS_ID: " + uri);

                selection = concatenateWhere(selection, TABLE_BOOKMARKS + "._id = ?");
                selectionArgs = appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case BOOKMARKS: {
                Log.d(LOGTAG, "Updating bookmark: " + uri);
                updated = updateBookmarks(uri, values, selection, selectionArgs);
                break;
            }

            case HISTORY_ID:
                Log.d(LOGTAG, "Update on HISTORY_ID: " + uri);

                selection = concatenateWhere(selection, TABLE_HISTORY + "._id = ?");
                selectionArgs = appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case HISTORY: {
                Log.d(LOGTAG, "Updating history: " + uri);
                updated = updateHistory(uri, values, selection, selectionArgs);
                break;
            }

            case IMAGES: {
                Log.d(LOGTAG, "Update on IMAGES: " + uri);

                String url = values.getAsString(Images.URL);

                if (TextUtils.isEmpty(url))
                    throw new IllegalArgumentException("Images.URL is required");

                updated = updateImage(uri, values, Images.URL + " = ?", new String[] { url });

                break;
            }

            default:
                throw new UnsupportedOperationException("Unknown update URI " + uri);
        }

        Log.d(LOGTAG, "Updated " + updated + " rows for URI: " + uri);

        return updated;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        Log.d(LOGTAG, "Querying with URI: " + uri);

        SQLiteDatabase db = getReadableDatabase(uri);
        final int match = URI_MATCHER.match(uri);

        SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
        String limit = uri.getQueryParameter(BrowserContract.PARAM_LIMIT);

        switch (match) {
            case BOOKMARKS_FOLDER_ID:
            case BOOKMARKS_ID:
            case BOOKMARKS: {
                Log.d(LOGTAG, "Query is on bookmarks: " + uri);

                if (match == BOOKMARKS_ID) {
                    Log.d(LOGTAG, "Query is BOOKMARKS_ID: " + uri);
                    selection = concatenateWhere(selection,
                            qualifyColumnValue(TABLE_BOOKMARKS, Bookmarks._ID) + " = ?");
                    selectionArgs = appendSelectionArgs(selectionArgs,
                            new String[] { Long.toString(ContentUris.parseId(uri)) });
                } else if (match == BOOKMARKS_FOLDER_ID) {
                    Log.d(LOGTAG, "Query is BOOKMARKS_FOLDER_ID: " + uri);
                    selection = concatenateWhere(selection,
                            qualifyColumnValue(TABLE_BOOKMARKS, Bookmarks.PARENT) + " = ?");
                    selectionArgs = appendSelectionArgs(selectionArgs,
                            new String[] { Long.toString(ContentUris.parseId(uri)) });
                }

                if (TextUtils.isEmpty(sortOrder)) {
                    Log.d(LOGTAG, "Using default sort order on query: " + uri);
                    sortOrder = DEFAULT_BOOKMARKS_SORT_ORDER;
                }

                qb.setProjectionMap(BOOKMARKS_PROJECTION_MAP);
                qb.setTables(TABLE_BOOKMARKS_JOIN_IMAGES);

                break;
            }

            case HISTORY_ID:
            case HISTORY: {
                Log.d(LOGTAG, "Query is on history: " + uri);

                if (match == HISTORY_ID) {
                    Log.d(LOGTAG, "Query is HISTORY_ID: " + uri);
                    selection = concatenateWhere(selection,
                            qualifyColumnValue(TABLE_HISTORY, History._ID) + " = ?");
                    selectionArgs = appendSelectionArgs(selectionArgs,
                            new String[] { Long.toString(ContentUris.parseId(uri)) });
                }

                if (TextUtils.isEmpty(sortOrder))
                    sortOrder = DEFAULT_HISTORY_SORT_ORDER;

                qb.setProjectionMap(HISTORY_PROJECTION_MAP);
                qb.setTables(TABLE_HISTORY_JOIN_IMAGES);

                break;
            }

            case IMAGES: {
                Log.d(LOGTAG, "Query is on images: " + uri);

                qb.setProjectionMap(IMAGES_PROJECTION_MAP);
                qb.setTables(TABLE_IMAGES);

                break;
            }

            default:
                throw new UnsupportedOperationException("Unknown query URI " + uri);
        }

        Log.d(LOGTAG, "Finally running the built query: " + uri);
        Cursor cursor = qb.query(db, projection, selection, selectionArgs, null,
                null, sortOrder, limit);
        cursor.setNotificationUri(getContext().getContentResolver(),
                BrowserContract.AUTHORITY_URI);

        return cursor;
    }

    ContentValues extractImageValues(ContentValues values, String url) {
        Log.d(LOGTAG, "Extracting image values for URI: " + url);

        ContentValues imageValues = null;

        if (values.containsKey(Bookmarks.FAVICON)) {
            Log.d(LOGTAG, "Has favicon value on URL: " + url);
            imageValues = new ContentValues();
            imageValues.put(Images.FAVICON,
                    values.getAsByteArray(Bookmarks.FAVICON));
            values.remove(Bookmarks.FAVICON);
        }

        if (values.containsKey(Bookmarks.THUMBNAIL)) {
            Log.d(LOGTAG, "Has favicon value on URL: " + url);
            if (imageValues == null)
                imageValues = new ContentValues();

            imageValues.put(Images.THUMBNAIL,
                    values.getAsByteArray(Bookmarks.THUMBNAIL));
            values.remove(Bookmarks.THUMBNAIL);
        }

        if (imageValues != null && url != null) {
            Log.d(LOGTAG, "Has URL value");
            imageValues.put(Images.URL, url);
        }

        return imageValues;
    }

    int getUrlCount(SQLiteDatabase db, String table, String url) {
        Cursor c = db.query(table, new String[] { "COUNT(*)" },
                CommonColumns.URL + " = ?", new String[] { url }, null, null,
                null);

        int count = 0;

        try {
            if (c.moveToFirst())
                count = c.getInt(0);
        } finally {
            c.close();
        }

        return count;
    }

    int updateBookmarks(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        Log.d(LOGTAG, "Updating bookmarks on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int updated = 0;

        final String[] bookmarksProjection = new String[] {
                Bookmarks._ID, // 0
                Bookmarks.URL, // 1
        };

        Log.d(LOGTAG, "Quering bookmarks to update on URI: " + uri);

        Cursor cursor = db.query(TABLE_BOOKMARKS, bookmarksProjection,
                selection, selectionArgs, null, null, null);

        try {
            values.put(Bookmarks.DATE_MODIFIED, System.currentTimeMillis());

            boolean updatingUrl = values.containsKey(Bookmarks.URL);
            String url = null;

            if (updatingUrl)
                url = values.getAsString(Bookmarks.URL);

            ContentValues imageValues = extractImageValues(values, url);

            while (cursor.moveToNext()) {
                long id = cursor.getLong(0);

                Log.d(LOGTAG, "Updating bookmark with ID: " + id);

                updated += db.update(TABLE_BOOKMARKS, values, "_id = ?",
                        new String[] { Long.toString(id) });

                if (imageValues == null)
                    continue;

                if (!updatingUrl) {
                    url = cursor.getString(1);
                    imageValues.put(Images.URL, url);
                }

                if (!TextUtils.isEmpty(url)) {
                    Log.d(LOGTAG, "Updating bookmark image for URL: " + url);
                    updateImage(uri, imageValues, Images.URL + " = ?", new String[] { url });
                }
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }

        return updated;
    }

    int updateHistory(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        Log.d(LOGTAG, "Updating history on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int updated = 0;

        final String[] historyProjection = new String[] { History._ID, // 0
                History.URL, // 1
        };

        Cursor cursor = db.query(TABLE_HISTORY, historyProjection, selection,
                selectionArgs, null, null, null);

        try {
            values.put(History.DATE_MODIFIED, System.currentTimeMillis());

            boolean updatingUrl = values.containsKey(History.URL);
            String url = null;

            if (updatingUrl)
                url = values.getAsString(History.URL);

            ContentValues imageValues = extractImageValues(values, url);

            while (cursor.moveToNext()) {
                long id = cursor.getLong(0);

                Log.d(LOGTAG, "Updating history entry with ID: " + id);

                updated += db.update(TABLE_HISTORY, values, "_id = ?",
                        new String[] { Long.toString(id) });

                if (imageValues == null)
                    continue;

                if (!updatingUrl) {
                    url = cursor.getString(1);
                    imageValues.put(Images.URL, url);
                }

                if (!TextUtils.isEmpty(url)) {
                    Log.d(LOGTAG, "Updating history image for URL: " + url);
                    updateImage(uri, imageValues, Images.URL + " = ?", new String[] { url });
                }
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }

        return updated;
    }

    int updateImage(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        String url = values.getAsString(Images.URL);

        Log.d(LOGTAG, "Updating image for URL: " + url);

        final SQLiteDatabase db = getWritableDatabase(uri);

        long now = System.currentTimeMillis();

        // Thumbnails update on every page load. We don't want to flood
        // sync with meaningless last modified date. Only update modified
        // date when favicons bits change.
        if (values.containsKey(Images.FAVICON) || values.containsKey(Images.FAVICON_URL))
            values.put(Images.DATE_MODIFIED, now);

        Log.d(LOGTAG, "Trying to update image for URL: " + url);
        int updated = db.update(TABLE_IMAGES, values, selection, selectionArgs);

        if (updated == 0) {
            // Generate GUID for new image
            values.put(Images.GUID, UUID.randomUUID().toString());
            values.put(Images.DATE_CREATED, now);
            values.put(Images.DATE_MODIFIED, now);

            Log.d(LOGTAG, "No update, inserting image for URL: " + url);
            db.insert(TABLE_IMAGES, Images.FAVICON, values);
            updated = 1;
        }

        return updated;
    }

    int deleteHistory(Uri uri, String selection, String[] selectionArgs) {
        Log.d(LOGTAG, "Deleting history entry for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        return db.delete(TABLE_HISTORY, selection, selectionArgs);
    }

    int deleteBookmarks(Uri uri, String selection, String[] selectionArgs) {
        Log.d(LOGTAG, "Deleting bookmarks for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        return db.delete(TABLE_BOOKMARKS, selection, selectionArgs);
    }

    int deleteUnusedImages(Uri uri) {
        Log.d(LOGTAG, "Deleting all unused images for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

        String selection = Images.URL + " NOT IN (SELECT " + Bookmarks.URL +
                " FROM " + TABLE_BOOKMARKS + " WHERE " + Bookmarks.URL +
                " IS NOT NULL) AND " + Images.URL + " NOT IN (SELECT " +
                History.URL + " FROM " + TABLE_HISTORY + " WHERE " +
                History.URL + " IS NOT NULL)";

        return db.delete(TABLE_IMAGES, selection, null);
    }
}