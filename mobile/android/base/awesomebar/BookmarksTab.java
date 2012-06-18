/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.res.Resources;
import android.widget.AdapterView;
import android.os.AsyncTask;
import android.content.Context;
import android.widget.ListView;
import android.widget.ImageView;
import android.widget.TextView;
import android.view.View;
import android.app.Activity;
import android.database.Cursor;
import android.util.Log;
import android.widget.SimpleCursorAdapter;
import android.view.ViewGroup;
import android.content.Intent;
import android.widget.LinearLayout;
import android.os.SystemClock;
import android.util.Pair;
import android.widget.TabHost.TabContentFactory;

import java.util.LinkedList;

import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;

public class BookmarksTab extends AwesomeBarTab {
    public static final String LOGTAG = "BOOKMARKS_TAB";
    public static final String TAG = "bookmarks";
    private boolean mInReadingList = false;
    private int mFolderId;
    private String mFolderTitle;
    private ListView mView = null;
    private BookmarksListAdapter mCursorAdapter = null;
    private BookmarksQueryTask mQueryTask = null;

    public int getTitleStringId() {
        return R.string.awesomebar_bookmarks_title;
    }

    public String getTag() {
        return TAG;
    }

    public BookmarksTab(Context context) {
        super(context);
    }

    public TabContentFactory getFactory() {
        return new TabContentFactory() {
             public View createTabContent(String tag) {
                 final ListView list = getListView();
                 list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
                     public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                         handleItemClick(parent, view, position, id);
                     }
                 });
                 return list;
             }
        };
    }

    public ListView getListView() {
        if (mView == null) {
            mView = new ListView(mContext, null, R.style.AwesomeBarList);
            ((Activity)mContext).registerForContextMenu(mView);
            mView.setTag(TAG);

            // We need to add the header before we set the adapter, hence make it null
            mView.setAdapter(null);
            mView.setAdapter(getCursorAdapter());

            BookmarksQueryTask task = getQueryTask();
            task.execute();
        }
        return mView;
    }

    public void destroy() {
        BookmarksListAdapter adapter = getCursorAdapter();
        if (adapter == null) {
            return;
        }

        Cursor cursor = adapter.getCursor();
        if (cursor != null)
            cursor.close();
    }

    public boolean onBackPressed() {
        return false;
    }

    protected BookmarksListAdapter getCursorAdapter() {
        return getCursorAdapter(null);
    }

    protected BookmarksListAdapter getCursorAdapter(Cursor c) {
        if (mCursorAdapter == null) {
            mCursorAdapter = new BookmarksListAdapter(mContext, c);
        } else if (c != null) {
            mCursorAdapter.changeCursor(c);
        } else {
            // do a quick return if just asking for the cached adapter
            return mCursorAdapter;
        }

        LinearLayout headerView = mCursorAdapter.getHeaderView();
        if (headerView == null) {
            headerView = (LinearLayout) getInflater().inflate(R.layout.awesomebar_header_row, null);
            mCursorAdapter.setHeaderView(headerView);
        }

        // Add/Remove header based on the root folder
        if (mView != null) {
            if (mFolderId == Bookmarks.FIXED_ROOT_ID) {
                if (mView.getHeaderViewsCount() == 1) {
                    mView.removeHeaderView(headerView);
                }
            } else {
                if (mView.getHeaderViewsCount() == 0) {
                    mView.addHeaderView(headerView, null, true);
                }
                ((TextView) headerView.findViewById(R.id.title)).setText(mFolderTitle);
            }
        }

        return mCursorAdapter;
    }

    protected BookmarksQueryTask getQueryTask() {
        if (mQueryTask == null) {
            mQueryTask = new BookmarksQueryTask();
        }
        return mQueryTask;
    }

    public void handleItemClick(AdapterView<?> parent, View view, int position, long id) {
        ListView list = getListView();
        if (list == null)
            return;

        int headerCount = list.getHeaderViewsCount();
        // If we tap on the header view, there's nothing to do
        if (headerCount == 1 && position == 0)
            return;

        BookmarksListAdapter adapter = getCursorAdapter();
        if (adapter == null)
            return;

        Cursor cursor = adapter.getCursor();
        if (cursor == null)
            return;

        // The header view takes up a spot in the list
        if (headerCount == 1)
            position--;

        cursor.moveToPosition(position);

        int type = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks.TYPE));
        if (type == Bookmarks.TYPE_FOLDER) {
            // If we're clicking on a folder, update adapter to move to that folder
            int folderId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
            String folderTitle = adapter.getFolderTitle(position);

            adapter.moveToChildFolder(folderId, folderTitle);
            return;
        }

        // Otherwise, just open the URL
        AwesomeBarTabs.OnUrlOpenListener listener = getUrlListener();
        if (listener == null) {
            return;
        }

        listener.onUrlOpen(cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL)));
    }

    private class BookmarksListAdapter extends SimpleCursorAdapter {
        private static final int VIEW_TYPE_ITEM = 0;
        private static final int VIEW_TYPE_FOLDER = 1;
        private static final int VIEW_TYPE_COUNT = 2;

        private LinkedList<Pair<Integer, String>> mParentStack;
        private LinearLayout mBookmarksTitleView;

        public BookmarksListAdapter(Context context, Cursor c) {
            super(context, -1, c, new String[] {}, new int[] {});

            // mParentStack holds folder id/title pairs that allow us to navigate
            // back up the folder heirarchy
            mParentStack = new LinkedList<Pair<Integer, String>>();

            // Add the root folder to the stack
            Pair<Integer, String> rootFolder = new Pair<Integer, String>(Bookmarks.FIXED_ROOT_ID, "");
            mParentStack.addFirst(rootFolder);
        }

        public void refreshCurrentFolder() {
            // Cancel any pre-existing async refresh tasks
            if (mQueryTask != null)
                mQueryTask.cancel(false);

            Pair<Integer, String> folderPair = mParentStack.getFirst();
            mInReadingList = (folderPair.first == Bookmarks.FIXED_READING_LIST_ID);

            mQueryTask = new BookmarksQueryTask(folderPair.first, folderPair.second);
            mQueryTask.execute();
        }

        // Returns false if there is no parent folder to move to
        public boolean moveToParentFolder() {
            // If we're already at the root, we can't move to a parent folder
            if (mParentStack.size() == 1)
                return false;

            mParentStack.removeFirst();
            refreshCurrentFolder();
            return true;
        }

        public void moveToChildFolder(int folderId, String folderTitle) {
            Pair<Integer, String> folderPair = new Pair<Integer, String>(folderId, folderTitle);
            mParentStack.addFirst(folderPair);
            refreshCurrentFolder();
        }

        public int getItemViewType(int position) {
            Cursor c = getCursor();
 
            if (c.moveToPosition(position) &&
                c.getInt(c.getColumnIndexOrThrow(Bookmarks.TYPE)) == Bookmarks.TYPE_FOLDER)
                return VIEW_TYPE_FOLDER;

            // Default to retuning normal item type
            return VIEW_TYPE_ITEM;
        }
 
        @Override
        public int getViewTypeCount() {
            return VIEW_TYPE_COUNT;
        }

        public String getFolderTitle(int position) {
            Cursor c = getCursor();
            if (!c.moveToPosition(position))
                return "";

            String guid = c.getString(c.getColumnIndexOrThrow(Bookmarks.GUID));

            // If we don't have a special GUID, just return the folder title from the DB.
            if (guid == null || guid.length() == 12)
                return c.getString(c.getColumnIndexOrThrow(Bookmarks.TITLE));

            // Use localized strings for special folder names.
            if (guid.equals(Bookmarks.FAKE_DESKTOP_FOLDER_GUID))
                return getResources().getString(R.string.bookmarks_folder_desktop);
            else if (guid.equals(Bookmarks.MENU_FOLDER_GUID))
                return getResources().getString(R.string.bookmarks_folder_menu);
            else if (guid.equals(Bookmarks.TOOLBAR_FOLDER_GUID))
                return getResources().getString(R.string.bookmarks_folder_toolbar);
            else if (guid.equals(Bookmarks.UNFILED_FOLDER_GUID))
                return getResources().getString(R.string.bookmarks_folder_unfiled);
            else if (guid.equals(Bookmarks.READING_LIST_FOLDER_GUID))
                return getResources().getString(R.string.bookmarks_folder_reading_list);

            // If for some reason we have a folder with a special GUID, but it's not one of
            // the special folders we expect in the UI, just return the title from the DB.
            return c.getString(c.getColumnIndexOrThrow(Bookmarks.TITLE));
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            int viewType = getItemViewType(position);
            AwesomeEntryViewHolder viewHolder = null;

            if (convertView == null) {
                if (viewType == VIEW_TYPE_ITEM)
                    convertView = getInflater().inflate(R.layout.awesomebar_row, null);
                else
                    convertView = getInflater().inflate(R.layout.awesomebar_folder_row, null);

                viewHolder = new AwesomeEntryViewHolder();
                viewHolder.titleView = (TextView) convertView.findViewById(R.id.title);
                viewHolder.faviconView = (ImageView) convertView.findViewById(R.id.favicon);

                if (viewType == VIEW_TYPE_ITEM)
                    viewHolder.urlView = (TextView) convertView.findViewById(R.id.url);

                convertView.setTag(viewHolder);
            } else {
                viewHolder = (AwesomeEntryViewHolder) convertView.getTag();
            }

            Cursor cursor = getCursor();
            if (!cursor.moveToPosition(position))
                throw new IllegalStateException("Couldn't move cursor to position " + position);

            if (viewType == VIEW_TYPE_ITEM) {
                updateTitle(viewHolder.titleView, cursor);
                updateUrl(viewHolder.urlView, cursor);
                updateFavicon(viewHolder.faviconView, cursor);
            } else {
                viewHolder.titleView.setText(getFolderTitle(position));
            }

            return convertView;
        }

        public LinearLayout getHeaderView() {
            return mBookmarksTitleView;
        }

        public void setHeaderView(LinearLayout titleView) {
            mBookmarksTitleView = titleView;
        }
    }

    private class BookmarksQueryTask extends AsyncTask<Void, Void, Cursor> {
        public BookmarksQueryTask() {
            mFolderId = Bookmarks.FIXED_ROOT_ID;
            mFolderTitle = "";
        }

        public BookmarksQueryTask(int folderId, String folderTitle) {
            mFolderId = folderId;
            mFolderTitle = folderTitle;
        }

        @Override
        protected Cursor doInBackground(Void... arg0) {
            return BrowserDB.getBookmarksInFolder(getContentResolver(), mFolderId);
        }

        @Override
        protected void onPostExecute(final Cursor cursor) {
            // Hack: force this to the main thread, even though it should already be on it
            GeckoApp.mAppContext.mMainHandler.post(new Runnable() {
                public void run() {
                    // this will update the cursorAdapter to use the new one if it already exists
                    // We need to add the header before we set the adapter, hence make it null
                    mView.setAdapter(null);
                    mView.setAdapter(getCursorAdapter(cursor));
                }
            });
            mQueryTask = null;
        }
    }

    public boolean moveToParentFolder() {
        // If we're not in the bookmarks tab, we have nothing to do. We should
        // also return false if mBookmarksAdapter hasn't been initialized yet.
        BookmarksListAdapter adapter = getCursorAdapter();
        if (adapter == null)
            return false;

        return adapter.moveToParentFolder();
    }

    public boolean isInReadingList() {
        return mInReadingList;
    }
}
