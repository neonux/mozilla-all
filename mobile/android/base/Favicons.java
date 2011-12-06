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

package org.mozilla.gecko;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.provider.Browser;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.Set;

public class Favicons {
    private static final String LOGTAG = "GeckoFavicons";

    public static final long NOT_LOADING = 0;

    private Context mContext;
    private DatabaseHelper mDbHelper;

    private Map<Long,LoadFaviconTask> mLoadTasks;
    private long mNextFaviconLoadId;

    public interface OnFaviconLoadedListener {
        public void onFaviconLoaded(String url, Drawable favicon);
    }

    private class DatabaseHelper extends SQLiteOpenHelper {
        private static final String DATABASE_NAME = "favicon_urls.db";
        private static final String TABLE_NAME = "favicon_urls";
        private static final int DATABASE_VERSION = 1;

        private static final String COLUMN_ID = "_id";
        private static final String COLUMN_FAVICON_URL = "favicon_url";
        private static final String COLUMN_PAGE_URL = "page_url";

        DatabaseHelper(Context context) {
            super(context, DATABASE_NAME, null, DATABASE_VERSION);
            Log.d(LOGTAG, "Creating DatabaseHelper");
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            Log.d(LOGTAG, "Creating database for favicon URLs");

            db.execSQL("CREATE TABLE " + TABLE_NAME + " (" +
                       COLUMN_ID + " INTEGER PRIMARY KEY," +
                       COLUMN_FAVICON_URL + " TEXT NOT NULL," +
                       COLUMN_PAGE_URL + " TEXT UNIQUE NOT NULL" +
                       ");");
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            Log.w(LOGTAG, "Upgrading favicon URLs database from version " +
                  oldVersion + " to " + newVersion + ", which will destroy all old data");

            // Drop table completely
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_NAME);

            // Recreate database
            onCreate(db);
        }

        public String getFaviconUrlForPageUrl(String pageUrl) {
            Log.d(LOGTAG, "Calling getFaviconUrlForPageUrl() for " + pageUrl);

            SQLiteDatabase db = mDbHelper.getReadableDatabase();

            SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
            qb.setTables(TABLE_NAME);

            Cursor c = qb.query(
                db,
                new String[] { COLUMN_FAVICON_URL },
                COLUMN_PAGE_URL + " = ?",
                new String[] { pageUrl },
                null, null, null
            );

            if (!c.moveToFirst()) {
                c.close();
                return null;
            }

            String url = c.getString(c.getColumnIndexOrThrow(COLUMN_FAVICON_URL));
            c.close();
            return url;
        }

        public void setFaviconUrlForPageUrl(String pageUrl, String faviconUrl) {
            Log.d(LOGTAG, "Calling setFaviconUrlForPageUrl() for " + pageUrl);

            SQLiteDatabase db = mDbHelper.getWritableDatabase();

            ContentValues values = new ContentValues();
            values.put(COLUMN_FAVICON_URL, faviconUrl);
            values.put(COLUMN_PAGE_URL, pageUrl);

            db.replace(TABLE_NAME, null, values);
        }
    }

    public Favicons(Context context) {
        Log.d(LOGTAG, "Creating Favicons instance");

        mContext = context;
        mDbHelper = new DatabaseHelper(context);

        mLoadTasks = new HashMap<Long,LoadFaviconTask>();
        mNextFaviconLoadId = 0;
    }

    public long loadFavicon(String pageUrl, String faviconUrl,
            OnFaviconLoadedListener listener) {

        // Handle the case where page url is empty
        if (pageUrl == null || pageUrl.length() == 0) {
            if (listener != null)
                listener.onFaviconLoaded(null, null);
        }

        LoadFaviconTask task = new LoadFaviconTask(pageUrl, faviconUrl, listener);

        long taskId = task.getId();
        mLoadTasks.put(taskId, task);

        task.execute();

        Log.d(LOGTAG, "Calling loadFavicon() with URL = " + pageUrl +
                        " and favicon URL = " + faviconUrl +
                        " (" + taskId + ")");

        return taskId;
    }

    public boolean cancelFaviconLoad(long taskId) {
        Log.d(LOGTAG, "Requesting cancelation of favicon load (" + taskId + ")");

        if (!mLoadTasks.containsKey(taskId))
            return false;

        Log.d(LOGTAG, "Cancelling favicon load (" + taskId + ")");

        LoadFaviconTask task = mLoadTasks.get(taskId);
        return task.cancel(false);
    }

    public void close() {
        Log.d(LOGTAG, "Closing Favicons database");
        mDbHelper.close();

        // Cancel any pending tasks
        Set<Long> taskIds = mLoadTasks.keySet();
        Iterator iter = taskIds.iterator();
        while (iter.hasNext()) {
            long taskId = (Long) iter.next();
            cancelFaviconLoad(taskId);
        }
    }

    private class LoadFaviconTask extends AsyncTask<Void, Void, BitmapDrawable> {
        private long mId;
        private String mPageUrl;
        private String mFaviconUrl;
        private OnFaviconLoadedListener mListener;

        public LoadFaviconTask(String pageUrl, String faviconUrl, OnFaviconLoadedListener listener) {
            mId = ++mNextFaviconLoadId;
            mPageUrl = pageUrl;
            mFaviconUrl = faviconUrl;
            mListener = listener;

            Log.d(LOGTAG, "Creating LoadFaviconTask with URL = " + pageUrl +
                          " and favicon URL = " + faviconUrl);
        }

        // Runs in background thread
        private BitmapDrawable loadFaviconFromDb() {
            Log.d(LOGTAG, "Loading favicon from DB for URL = " + mPageUrl);

            ContentResolver resolver = mContext.getContentResolver();

            Cursor c = resolver.query(Browser.BOOKMARKS_URI,
                                      new String[] { Browser.BookmarkColumns.FAVICON },
                                      Browser.BookmarkColumns.URL + " = ?",
                                      new String[] { mPageUrl },
                                      null);

            if (!c.moveToFirst()) {
                c.close();
                return null;
            }

            int faviconIndex = c.getColumnIndexOrThrow(Browser.BookmarkColumns.FAVICON);
            
            byte[] b = c.getBlob(faviconIndex);
            c.close();
            if (b == null)
                return null;

            Bitmap bitmap = BitmapFactory.decodeByteArray(b, 0, b.length);

            Log.d(LOGTAG, "Loaded favicon from DB successfully for URL = " + mPageUrl);

            return new BitmapDrawable(bitmap);
        }

        // Runs in background thread
        private void saveFaviconToDb(BitmapDrawable favicon) {
            Bitmap bitmap = favicon.getBitmap();

            ByteArrayOutputStream stream = new ByteArrayOutputStream();
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);

            ContentValues values = new ContentValues();
            values.put(Browser.BookmarkColumns.FAVICON, stream.toByteArray());
            values.put(Browser.BookmarkColumns.URL, mPageUrl);

            ContentResolver resolver = mContext.getContentResolver();

            Log.d(LOGTAG, "Saving favicon on browser database for URL = " + mPageUrl);
            resolver.update(Browser.BOOKMARKS_URI,
                            values,
                            Browser.BookmarkColumns.URL + " = ?",
                            new String[] { mPageUrl });


            Log.d(LOGTAG, "Saving favicon URL for URL = " + mPageUrl);
            mDbHelper.setFaviconUrlForPageUrl(mPageUrl, mFaviconUrl);
        }

        // Runs in background thread
        private BitmapDrawable downloadFavicon(URL faviconUrl) {
            Log.d(LOGTAG, "Downloading favicon for URL = " + mPageUrl +
                          " with favicon URL = " + mFaviconUrl);

            // due to android bug 6066, we must download the entire image before using it
            // http://code.google.com/p/android/issues/detail?id=6066
            HttpURLConnection urlConnection = null;
            BufferedInputStream contentStream = null;
            ByteArrayInputStream byteStream = null;
            BitmapDrawable image = null;

            try {
                urlConnection = (HttpURLConnection) faviconUrl.openConnection();
                int length = urlConnection.getContentLength();
                contentStream = new BufferedInputStream(urlConnection.getInputStream(), length);
                byte[] bytes = new byte[length];
                int pos = 0;
                int offset = 0;
                while ((pos = contentStream.read(bytes, offset, length - offset)) > 0)
                    offset += pos;
                if (length == offset) {
                    byteStream = new ByteArrayInputStream(bytes);
                    image = (BitmapDrawable) Drawable.createFromStream(byteStream, "src");
                }
            } catch (Exception e) {
                Log.d(LOGTAG, "Error downloading favicon: " + e);
            } finally {
                if (urlConnection != null)
                    urlConnection.disconnect();
                try {
                    if (contentStream != null)
                        contentStream.close();
                    if (byteStream != null)
                        byteStream.close();
                } catch (IOException e) {
                    Log.d(LOGTAG, "error closing favicon stream");
                }
            }

            if (image != null) {
                Log.d(LOGTAG, "Downloaded favicon successfully for URL = " + mPageUrl);
                saveFaviconToDb(image);
            }

            return image;
        }

        @Override
        protected BitmapDrawable doInBackground(Void... unused) {
            BitmapDrawable image = null;
            URL pageUrl = null;

            if (isCancelled())
                return null;

            // Handle the case of malformed URL
            try {
                pageUrl = new URL(mPageUrl);
            } catch (MalformedURLException e) {
                Log.d(LOGTAG, "The provided URL is not valid: " + e);
                return null;
            }

            URL faviconUrl = null;

            // Handle the case of malformed favicon URL
            try {
                // If favicon is empty, fallback to default favicon URI
                if (mFaviconUrl == null || mFaviconUrl.length() == 0) {
                    faviconUrl = new URL(pageUrl.getProtocol(), pageUrl.getAuthority(), "/favicon.ico");
                    mFaviconUrl = faviconUrl.toString();
                } else {
                    faviconUrl = new URL(mFaviconUrl);
                }
            } catch (MalformedURLException e) {
                Log.d(LOGTAG, "The provided favicon URL is not valid: " + e);
                return null;
            }

            Log.d(LOGTAG, "Favicon URL is now: " + mFaviconUrl);

            if (isCancelled())
                return null;

            String storedFaviconUrl = mDbHelper.getFaviconUrlForPageUrl(mPageUrl);
            if (storedFaviconUrl != null && storedFaviconUrl.equals(mFaviconUrl)) {
                image = loadFaviconFromDb();

                if (isCancelled())
                    return null;

                // If favicon URL is defined but the favicon image is not
                // stored in the database for some reason, we force download.
                if (image == null) {
                    image = downloadFavicon(faviconUrl);
                }
            } else {
                image = downloadFavicon(faviconUrl);
            }

            return image;
        }

        @Override
        protected void onPostExecute(final BitmapDrawable image) {
            Log.d(LOGTAG, "LoadFaviconTask finished for URL = " + mPageUrl +
                          " (" + mId + ")");

            mLoadTasks.remove(mId);

            if (mListener != null) {
                // We want to always run the listener on UI thread
                GeckoApp.mAppContext.runOnUiThread(new Runnable() {
                    public void run() {
                        mListener.onFaviconLoaded(mPageUrl, image);
                    }
                });
            }
        }

        @Override
        protected void onCancelled() {
            Log.d(LOGTAG, "LoadFaviconTask cancelled for URL = " + mPageUrl +
                          " (" + mId + ")");

            mLoadTasks.remove(mId);

            // Note that we don't call the listener callback if the
            // favicon load is cancelled.
        }

        public long getId() {
            return mId;
        }
    }
}
