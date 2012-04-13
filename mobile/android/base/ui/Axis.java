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
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick Walton <pcwalton@mozilla.com>
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

package org.mozilla.gecko.ui;

import java.util.Map;
import android.util.Log;
import org.json.JSONArray;
import org.mozilla.gecko.FloatUtils;

/**
 * This class represents the physics for one axis of movement (i.e. either
 * horizontal or vertical). It tracks the different properties of movement
 * like displacement, velocity, viewport dimensions, etc. pertaining to
 * a particular axis.
 */
public abstract class Axis {
    private static final String LOGTAG = "GeckoAxis";

    private static final String PREF_SCROLLING_FRICTION_SLOW = "ui.scrolling.friction_slow";
    private static final String PREF_SCROLLING_FRICTION_FAST = "ui.scrolling.friction_fast";
    private static final String PREF_SCROLLING_VELOCITY_THRESHOLD = "ui.scrolling.velocity_threshold";
    private static final String PREF_SCROLLING_MAX_EVENT_ACCELERATION = "ui.scrolling.max_event_acceleration";
    private static final String PREF_SCROLLING_OVERSCROLL_DECEL_RATE = "ui.scrolling.overscroll_decel_rate";
    private static final String PREF_SCROLLING_OVERSCROLL_SNAP_LIMIT = "ui.scrolling.overscroll_snap_limit";
    private static final String PREF_SCROLLING_MIN_SCROLLABLE_DISTANCE = "ui.scrolling.min_scrollable_distance";

    // This fraction of velocity remains after every animation frame when the velocity is low.
    private static float FRICTION_SLOW;
    // This fraction of velocity remains after every animation frame when the velocity is high.
    private static float FRICTION_FAST;
    // Below this velocity (in pixels per frame), the friction starts increasing from FRICTION_FAST
    // to FRICTION_SLOW.
    private static float VELOCITY_THRESHOLD;
    // The maximum velocity change factor between events, per ms, in %.
    // Direction changes are excluded.
    private static float MAX_EVENT_ACCELERATION;

    // The rate of deceleration when the surface has overscrolled.
    private static float OVERSCROLL_DECEL_RATE;
    // The percentage of the surface which can be overscrolled before it must snap back.
    private static float SNAP_LIMIT;

    // The minimum amount of space that must be present for an axis to be considered scrollable,
    // in pixels.
    private static float MIN_SCROLLABLE_DISTANCE;

    private static float getFloatPref(Map<String, Integer> prefs, String prefName, int defaultValue) {
        Integer value = (prefs == null ? null : prefs.get(prefName));
        return (float)(value == null || value < 0 ? defaultValue : value) / 1000f;
    }

    private static int getIntPref(Map<String, Integer> prefs, String prefName, int defaultValue) {
        Integer value = (prefs == null ? null : prefs.get(prefName));
        return (value == null || value < 0 ? defaultValue : value);
    }

    public static void addPrefNames(JSONArray prefs) {
        prefs.put(PREF_SCROLLING_FRICTION_FAST);
        prefs.put(PREF_SCROLLING_FRICTION_SLOW);
        prefs.put(PREF_SCROLLING_VELOCITY_THRESHOLD);
        prefs.put(PREF_SCROLLING_MAX_EVENT_ACCELERATION);
        prefs.put(PREF_SCROLLING_OVERSCROLL_DECEL_RATE);
        prefs.put(PREF_SCROLLING_OVERSCROLL_SNAP_LIMIT);
        prefs.put(PREF_SCROLLING_MIN_SCROLLABLE_DISTANCE);
    }

    public static void setPrefs(Map<String, Integer> prefs) {
        FRICTION_SLOW = getFloatPref(prefs, PREF_SCROLLING_FRICTION_SLOW, 850);
        FRICTION_FAST = getFloatPref(prefs, PREF_SCROLLING_FRICTION_FAST, 970);
        VELOCITY_THRESHOLD = getIntPref(prefs, PREF_SCROLLING_VELOCITY_THRESHOLD, 10);
        MAX_EVENT_ACCELERATION = getFloatPref(prefs, PREF_SCROLLING_MAX_EVENT_ACCELERATION, 12);
        OVERSCROLL_DECEL_RATE = getFloatPref(prefs, PREF_SCROLLING_OVERSCROLL_DECEL_RATE, 40);
        SNAP_LIMIT = getFloatPref(prefs, PREF_SCROLLING_OVERSCROLL_SNAP_LIMIT, 300);
        MIN_SCROLLABLE_DISTANCE = getFloatPref(prefs, PREF_SCROLLING_MIN_SCROLLABLE_DISTANCE, 500);
        Log.i(LOGTAG, "Prefs: " + FRICTION_SLOW + "," + FRICTION_FAST + "," + VELOCITY_THRESHOLD + ","
                + MAX_EVENT_ACCELERATION + "," + OVERSCROLL_DECEL_RATE + "," + SNAP_LIMIT + "," + MIN_SCROLLABLE_DISTANCE);
    }

    static {
        // set the scrolling parameters to default values on startup
        setPrefs(null);
    }

    // The number of milliseconds per frame assuming 60 fps
    private static final float MS_PER_FRAME = 1000.0f / 60.0f;

    private enum FlingStates {
        STOPPED,
        PANNING,
        FLINGING,
    }

    private enum Overscroll {
        NONE,
        MINUS,      // Overscrolled in the negative direction
        PLUS,       // Overscrolled in the positive direction
        BOTH,       // Overscrolled in both directions (page is zoomed to smaller than screen)
    }

    private final SubdocumentScrollHelper mSubscroller;

    private float mFirstTouchPos;           /* Position of the first touch event on the current drag. */
    private float mTouchPos;                /* Position of the most recent touch event on the current drag. */
    private float mLastTouchPos;            /* Position of the touch event before touchPos. */
    private float mVelocity;                /* Velocity in this direction; pixels per animation frame. */
    public boolean mScrollingDisabled;      /* Whether movement on this axis is locked. */
    private boolean mDisableSnap;           /* Whether overscroll snapping is disabled. */
    private float mDisplacement;

    private FlingStates mFlingState;        /* The fling state we're in on this axis. */

    protected abstract float getOrigin();
    protected abstract float getViewportLength();
    protected abstract float getPageLength();

    Axis(SubdocumentScrollHelper subscroller) {
        mSubscroller = subscroller;
    }

    private float getViewportEnd() {
        return getOrigin() + getViewportLength();
    }

    void startTouch(float pos) {
        mVelocity = 0.0f;
        mScrollingDisabled = false;
        mFirstTouchPos = mTouchPos = mLastTouchPos = pos;
    }

    float panDistance(float currentPos) {
        return currentPos - mFirstTouchPos;
    }

    void setScrollingDisabled(boolean disabled) {
        mScrollingDisabled = disabled;
    }

    void saveTouchPos() {
        mLastTouchPos = mTouchPos;
    }

    void updateWithTouchAt(float pos, float timeDelta) {
        float newVelocity = (mTouchPos - pos) / timeDelta * MS_PER_FRAME;

        // If there's a direction change, or current velocity is very low,
        // allow setting of the velocity outright. Otherwise, use the current
        // velocity and a maximum change factor to set the new velocity.
        boolean curVelocityIsLow = Math.abs(mVelocity) < 1.0f;
        boolean directionChange = (mVelocity > 0) != (newVelocity > 0);
        if (curVelocityIsLow || (directionChange && !FloatUtils.fuzzyEquals(newVelocity, 0.0f))) {
            mVelocity = newVelocity;
        } else {
            float maxChange = Math.abs(mVelocity * timeDelta * MAX_EVENT_ACCELERATION);
            mVelocity = Math.min(mVelocity + maxChange, Math.max(mVelocity - maxChange, newVelocity));
        }

        mTouchPos = pos;
    }

    boolean overscrolled() {
        return getOverscroll() != Overscroll.NONE;
    }

    private Overscroll getOverscroll() {
        boolean minus = (getOrigin() < 0.0f);
        boolean plus = (getViewportEnd() > getPageLength());
        if (minus && plus) {
            return Overscroll.BOTH;
        } else if (minus) {
            return Overscroll.MINUS;
        } else if (plus) {
            return Overscroll.PLUS;
        } else {
            return Overscroll.NONE;
        }
    }

    // Returns the amount that the page has been overscrolled. If the page hasn't been
    // overscrolled on this axis, returns 0.
    private float getExcess() {
        switch (getOverscroll()) {
        case MINUS:     return -getOrigin();
        case PLUS:      return getViewportEnd() - getPageLength();
        case BOTH:      return getViewportEnd() - getPageLength() - getOrigin();
        default:        return 0.0f;
        }
    }

    /*
     * Returns true if the page is zoomed in to some degree along this axis such that scrolling is
     * possible and this axis has not been scroll locked while panning. Otherwise, returns false.
     */
    private boolean scrollable() {
        return getViewportLength() <= getPageLength() - MIN_SCROLLABLE_DISTANCE &&
               !mScrollingDisabled;
    }

    /*
     * Returns the resistance, as a multiplier, that should be taken into account when
     * tracking or pinching.
     */
    float getEdgeResistance() {
        float excess = getExcess();
        if (excess > 0.0f) {
            // excess can be greater than viewport length, but the resistance
            // must never drop below 0.0
            return Math.max(0.0f, SNAP_LIMIT - excess / getViewportLength());
        }
        return 1.0f;
    }

    /* Returns the velocity. If the axis is locked, returns 0. */
    float getRealVelocity() {
        return scrollable() ? mVelocity : 0f;
    }

    void startPan() {
        mFlingState = FlingStates.PANNING;
    }

    void startFling(boolean stopped) {
        mDisableSnap = mSubscroller.scrolling();

        if (stopped) {
            mFlingState = FlingStates.STOPPED;
        } else {
            mFlingState = FlingStates.FLINGING;
        }
    }

    /* Advances a fling animation by one step. */
    boolean advanceFling() {
        if (mFlingState != FlingStates.FLINGING) {
            return false;
        }
        if (mSubscroller.scrolling() && !mSubscroller.lastScrollSucceeded()) {
            // if the subdocument stopped scrolling, it's because it reached the end
            // of the subdocument. we don't do overscroll on subdocuments, so there's
            // no point in continuing this fling.
            return false;
        }

        float excess = getExcess();
        if (mDisableSnap || FloatUtils.fuzzyEquals(excess, 0.0f)) {
            // If we aren't overscrolled, just apply friction.
            if (Math.abs(mVelocity) >= VELOCITY_THRESHOLD) {
                mVelocity *= FRICTION_FAST;
            } else {
                float t = mVelocity / VELOCITY_THRESHOLD;
                mVelocity *= FloatUtils.interpolate(FRICTION_SLOW, FRICTION_FAST, t);
            }
        } else {
            // Otherwise, decrease the velocity linearly.
            float elasticity = 1.0f - excess / (getViewportLength() * SNAP_LIMIT);
            if (getOverscroll() == Overscroll.MINUS) {
                mVelocity = Math.min((mVelocity + OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);
            } else { // must be Overscroll.PLUS
                mVelocity = Math.max((mVelocity - OVERSCROLL_DECEL_RATE) * elasticity, 0.0f);
            }
        }

        return true;
    }

    void stopFling() {
        mVelocity = 0.0f;
        mFlingState = FlingStates.STOPPED;
    }

    // Performs displacement of the viewport position according to the current velocity.
    void displace() {
        if (!mSubscroller.scrolling() && !scrollable())
            return;

        if (mFlingState == FlingStates.PANNING)
            mDisplacement += (mLastTouchPos - mTouchPos) * getEdgeResistance();
        else
            mDisplacement += mVelocity;
    }

    float resetDisplacement() {
        float d = mDisplacement;
        mDisplacement = 0.0f;
        return d;
    }
}
