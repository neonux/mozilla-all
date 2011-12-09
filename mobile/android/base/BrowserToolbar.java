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

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.Typeface;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.animation.TranslateAnimation;
import android.view.Gravity;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.TextSwitcher;
import android.widget.ViewSwitcher.ViewFactory;

public class BrowserToolbar extends LinearLayout {
    private Button mAwesomeBar;
    private ImageButton mTabs;
    public ImageButton mFavicon;
    public ImageButton mStop;
    public ImageButton mSiteSecurity;
    private AnimationDrawable mProgressSpinner;
    private TextSwitcher mTabsCount;

    final private Context mContext;
    private Handler mHandler;
    private boolean mInflated;
    private int mColor;
    private int mCounterColor;

    private int mDuration;
    private TranslateAnimation mSlideUpIn;
    private TranslateAnimation mSlideUpOut;
    private TranslateAnimation mSlideDownIn;
    private TranslateAnimation mSlideDownOut;

    private int mCount;

    public BrowserToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mInflated = false;
    }

    @Override
    protected void onFinishInflate () {
        super.onFinishInflate();

        // HACK: Without this, the onFinishInflate is called twice
        // This issue is due to a bug when Android inflates a layout with a
        // parent. Fixed in Honeycomb
        if (mInflated)
            return;

        mInflated = true;

        // Get the device's highlight color
        ContextThemeWrapper wrapper = new ContextThemeWrapper(mContext, android.R.style.TextAppearance);
        TypedArray typedArray = wrapper.getTheme().obtainStyledAttributes(new int[] { android.R.attr.textColorHighlight });
        mColor = typedArray.getColor(typedArray.getIndex(0), 0);

        mAwesomeBar = (Button) findViewById(R.id.awesome_bar);
        mAwesomeBar.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                onAwesomeBarSearch();
            }
        });

        Resources resources = getResources();
        
        int padding[] = { mAwesomeBar.getPaddingLeft(),
                          mAwesomeBar.getPaddingTop(),
                          mAwesomeBar.getPaddingRight(),
                          mAwesomeBar.getPaddingBottom() };

        GeckoStateListDrawable states = new GeckoStateListDrawable();
        states.initializeFilter(mColor);
        states.addState(new int[] { android.R.attr.state_pressed }, resources.getDrawable(R.drawable.address_bar_url_pressed));
        states.addState(new int[] { }, resources.getDrawable(R.drawable.address_bar_url_default));
        mAwesomeBar.setBackgroundDrawable(states);

        mAwesomeBar.setPadding(padding[0], padding[1], padding[2], padding[3]);

        mTabs = (ImageButton) findViewById(R.id.tabs);
        mTabs.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                if (Tabs.getInstance().getCount() > 1)
                    showTabs();
                else
                    addTab();
            }
        });
        mTabs.setImageLevel(1);

        mCounterColor = 0x99ffffff;

        mTabsCount = (TextSwitcher) findViewById(R.id.tabs_count);
        mTabsCount.removeAllViews();
        mTabsCount.setFactory(new ViewFactory() {
            public View makeView() {
                TextView text = new TextView(mContext);
                text.setGravity(Gravity.CENTER);
                text.setTextSize(16);
                text.setTextColor(mCounterColor);
                text.setTypeface(text.getTypeface(), Typeface.BOLD);
                return (View) text;
            }
        }); 
        mTabsCount.setText("0");
        mCount = 0;

        mFavicon = (ImageButton) findViewById(R.id.favicon);
        mSiteSecurity = (ImageButton) findViewById(R.id.site_security);
        mProgressSpinner = (AnimationDrawable) resources.getDrawable(R.drawable.progress_spinner);
        
        mStop = (ImageButton) findViewById(R.id.stop);
        mStop.setOnClickListener(new Button.OnClickListener() {
            public void onClick(View v) {
                doStop();
            }
        });

        mHandler = new Handler();
        mSlideUpIn = new TranslateAnimation(0, 0, 30, 0);
        mSlideUpOut = new TranslateAnimation(0, 0, 0, -30);
        mSlideDownIn = new TranslateAnimation(0, 0, -30, 0);
        mSlideDownOut = new TranslateAnimation(0, 0, 0, 30);

        mDuration = 750;
        mSlideUpIn.setDuration(mDuration);
        mSlideUpOut.setDuration(mDuration);
        mSlideDownIn.setDuration(mDuration);
        mSlideDownOut.setDuration(mDuration);
    }

    private void onAwesomeBarSearch() {
        GeckoApp.mAppContext.onEditRequested();
    }

    private void addTab() {
        GeckoApp.mAppContext.addTab();
    }

    private void showTabs() {
        GeckoApp.mAppContext.showTabs();
    }

    private void doStop() {
        GeckoApp.mAppContext.doStop();
    }

    public int getHighlightColor() {
        return mColor;
    }
    
    public void updateTabs(int count) {
        if (mCount > count) {
            mTabsCount.setInAnimation(mSlideDownIn);
            mTabsCount.setOutAnimation(mSlideDownOut);
        } else if (mCount < count) {
            mTabsCount.setInAnimation(mSlideUpIn);
            mTabsCount.setOutAnimation(mSlideUpOut);
        }

        if (count > 1)
            mTabs.setImageLevel(count);
        else
            mTabs.setImageLevel(0);

        mTabsCount.setVisibility(View.VISIBLE);
        mTabsCount.setText(String.valueOf(count));
        mCount = count;

        mHandler.postDelayed(new Runnable() {
            public void run() {
                ((TextView) mTabsCount.getCurrentView()).setTextColor(mColor);
            }
        }, mDuration);

        mHandler.postDelayed(new Runnable() {
            public void run() {
                if (Tabs.getInstance().getCount() == 1) {
                    mTabs.setImageLevel(1);
                    mTabsCount.setVisibility(View.GONE);
                    ((TextView) mTabsCount.getCurrentView()).setTextColor(mCounterColor);
                } else {
                    ((TextView) mTabsCount.getCurrentView()).setTextColor(mCounterColor);
                }
            }
        }, 2 * mDuration);
    }

    public void setProgressVisibility(boolean visible) {
        if (visible) {
            mFavicon.setImageDrawable(mProgressSpinner);
            mProgressSpinner.start();
            setStopVisibility(true);
        } else {
            mProgressSpinner.stop();
            setStopVisibility(false);
            setFavicon(Tabs.getInstance().getSelectedTab().getFavicon());
        }
    }

    public void setStopVisibility(boolean visible) {
        mStop.setVisibility(visible ? View.VISIBLE : View.GONE);
        mSiteSecurity.setVisibility(visible ? View.GONE : View.VISIBLE);
    }

    public void setTitle(CharSequence title) {
        mAwesomeBar.setText(title);
    }

    public void setFavicon(Drawable image) {
        if (Tabs.getInstance().getSelectedTab().isLoading())
            return;

        if (image != null)
            mFavicon.setImageDrawable(image);
        else
            mFavicon.setImageResource(R.drawable.favicon);
    }
    
    public void setSecurityMode(String mode) {
        if (mode.equals("identified") || mode.equals("verified"))
            mSiteSecurity.setImageLevel(1);
        else
            mSiteSecurity.setImageLevel(0);
    }
}
