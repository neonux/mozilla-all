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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <mwu@mozilla.com>
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

import java.io.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

import org.mozilla.gecko.gfx.InputConnectionHandler;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import android.os.*;
import android.app.*;
import android.text.*;
import android.text.style.*;
import android.view.*;
import android.view.inputmethod.*;
import android.content.*;
import android.R;

import android.text.method.TextKeyListener;
import android.text.method.KeyListener;

import android.util.*;


public class GeckoInputConnection
    extends BaseInputConnection
    implements TextWatcher, InputConnectionHandler
{
    private class ChangeNotification {
        public String mText;
        public int mStart;
        public int mEnd;
        public int mNewEnd;

        ChangeNotification(String text, int start, int oldEnd, int newEnd) {
            mText = text;
            mStart = start;
            mEnd = oldEnd;
            mNewEnd = newEnd;
        }

        ChangeNotification(int start, int end) {
            mText = null;
            mStart = start;
            mEnd = end;
            mNewEnd = 0;
        }
    }

    public GeckoInputConnection (View targetView) {
        super(targetView, true);
        mQueryResult = new SynchronousQueue<String>();

        mEditableFactory = Editable.Factory.getInstance();
        initEditable("");
        mIMEState = IME_STATE_DISABLED;
        mIMETypeHint = "";
        mIMEActionHint = "";
    }

    @Override
    public boolean beginBatchEdit() {
        Log.d("GeckoAppJava", "IME: beginBatchEdit");
        mBatchMode = true;
        return true;
    }

    @Override
    public boolean commitCompletion(CompletionInfo text) {
        Log.d("GeckoAppJava", "IME: commitCompletion");

        return commitText(text.getText(), 1);
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        Log.d("GeckoAppJava", "IME: commitText");

        setComposingText(text, newCursorPosition);
        finishComposingText();

        return true;
    }

    @Override
    public boolean deleteSurroundingText(int leftLength, int rightLength) {
        Log.d("GeckoAppJava", "IME: deleteSurroundingText");
        if (leftLength == 0 && rightLength == 0)
            return true;

        /* deleteSurroundingText is supposed to ignore the composing text,
            so we cancel any pending composition, delete the text, and then
            restart the composition */

        if (mComposing) {
            // Cancel current composition
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(0, 0, 0, 0, 0, 0, null));
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_END, 0, 0));
        }

        // Select text to be deleted
        int delStart, delLen;
        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_SELECTION, 0, 0));
        try {
            mQueryResult.take();
        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: deleteSurroundingText interrupted", e);
            return false;
        }
        delStart = mSelectionStart > leftLength ?
                    mSelectionStart - leftLength : 0;
        delLen = mSelectionStart + rightLength - delStart;
        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_SET_SELECTION, delStart, delLen));

        // Restore composition / delete text
        if (mComposing) {
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_BEGIN, 0, 0));
            if (mComposingText.length() > 0) {
                /* IME_SET_TEXT doesn't work well with empty strings */
                GeckoAppShell.sendEventToGecko(
                    new GeckoEvent(0, mComposingText.length(),
                                   GeckoEvent.IME_RANGE_RAWINPUT,
                                   GeckoEvent.IME_RANGE_UNDERLINE, 0, 0,
                                   mComposingText.toString()));
            }
        } else {
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_DELETE_TEXT, 0, 0));
        }

        // Temporarily disable text change notifications which confuse some IMEs (SlideIT, for example)
        // in the middle of text update.
        // They will be re-enabled on the next setComposingText
        disableChangeNotifications();

        return true;
    }

    @Override
    public boolean endBatchEdit() {
        Log.d("GeckoAppJava", "IME: endBatchEdit");

        mBatchMode = false;

        if (!mBatchChanges.isEmpty()) {
            InputMethodManager imm = (InputMethodManager)
                GeckoApp.mAppContext.getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                for (ChangeNotification n : mBatchChanges) {
                    if (n.mText != null)
                        notifyTextChange(imm, n.mText, n.mStart, n.mEnd, n.mNewEnd);
                    else
                        notifySelectionChange(imm, n.mStart, n.mEnd);
                }
            }
            mBatchChanges.clear();
        }
        return true;
    }

    @Override
    public boolean finishComposingText() {
        Log.d("GeckoAppJava", "IME: finishComposingText");

        if (mComposing) {
            // Set style to none
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(0, mComposingText.length(),
                               GeckoEvent.IME_RANGE_RAWINPUT, 0, 0, 0,
                               mComposingText));
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_END, 0, 0));
            mComposing = false;
            mComposingText = "";

            if (!mBatchMode) {
                // Make sure caret stays at the same position
                GeckoAppShell.sendEventToGecko(
                    new GeckoEvent(GeckoEvent.IME_SET_SELECTION,
                                   mCompositionStart + mCompositionSelStart, 0));
            }
        }
        return true;
    }

    @Override
    public int getCursorCapsMode(int reqModes) {
        Log.d("GeckoAppJava", "IME: getCursorCapsMode");

        return 0;
    }

    @Override
    public Editable getEditable() {
        Log.w("GeckoAppJava", "IME: getEditable called from " +
            Thread.currentThread().getStackTrace()[0].toString());

        return null;
    }

    @Override
    public boolean performContextMenuAction(int id) {
        Log.d("GeckoAppJava", "IME: performContextMenuAction");

        // First we need to ask Gecko to tell us the full contents of the
        // text field we're about to operate on.
        String text;
        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_TEXT, 0, Integer.MAX_VALUE));
        try {
            text = mQueryResult.take();
        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: performContextMenuAction interrupted", e);
            return false;
        }

        switch (id) {
            case R.id.selectAll:
                setSelection(0, text.length());
                break;
            case R.id.cut:
                // Fill the clipboard
                GeckoAppShell.setClipboardText(text);
                // If GET_TEXT returned an empty selection, we'll select everything
                if (mSelectionLength <= 0)
                    GeckoAppShell.sendEventToGecko(
                        new GeckoEvent(GeckoEvent.IME_SET_SELECTION, 0, text.length()));
                GeckoAppShell.sendEventToGecko(
                    new GeckoEvent(GeckoEvent.IME_DELETE_TEXT, 0, 0));
                break;
            case R.id.paste:
                commitText(GeckoAppShell.getClipboardText(), 1);
                break;
            case R.id.copy:
                // If there is no selection set, we must be doing "Copy All",
                // otherwise, we need to get the selection from Gecko
                if (mSelectionLength > 0) {
                    GeckoAppShell.sendEventToGecko(
                        new GeckoEvent(GeckoEvent.IME_GET_SELECTION, 0, 0));
                    try {
                        text = mQueryResult.take();
                    } catch (InterruptedException e) {
                        Log.e("GeckoAppJava", "IME: performContextMenuAction interrupted", e);
                        return false;
                    }
                }
                GeckoAppShell.setClipboardText(text);
                break;
        }
        return true;
    }

    @Override
    public ExtractedText getExtractedText(ExtractedTextRequest req, int flags) {
        if (req == null)
            return null;

        // Bail out here if gecko isn't running, otherwise we deadlock
        // below when waiting for the reply to IME_GET_SELECTION.
        if (!GeckoApp.checkLaunchState(GeckoApp.LaunchState.GeckoRunning))
            return null;

        Log.d("GeckoAppJava", "IME: getExtractedText");

        ExtractedText extract = new ExtractedText();
        extract.flags = 0;
        extract.partialStartOffset = -1;
        extract.partialEndOffset = -1;

        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_SELECTION, 0, 0));
        try {
            mQueryResult.take();
        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: getExtractedText interrupted", e);
            return null;
        }
        extract.selectionStart = mSelectionStart;
        extract.selectionEnd = mSelectionStart + mSelectionLength;

        // bug 617298 - IME_GET_TEXT sometimes gives the wrong result due to
        // a stale cache. Use a set of three workarounds:
        // 1. Sleep for 20 milliseconds and hope the child updates us with the new text.
        //    Very evil and, consequentially, most effective.
        try {
            Thread.sleep(20);
        } catch (InterruptedException e) {}

        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_TEXT, 0, Integer.MAX_VALUE));
        try {
            extract.startOffset = 0;
            extract.text = mQueryResult.take();

            // 2. Make a guess about what the text actually is
            if (mComposing && extract.selectionEnd > extract.text.length())
                extract.text = extract.text.subSequence(0, Math.min(extract.text.length(), mCompositionStart)) + mComposingText;

            // 3. If all else fails, make sure our selection indexes make sense
            extract.selectionStart = Math.min(extract.selectionStart, extract.text.length());
            extract.selectionEnd = Math.min(extract.selectionEnd, extract.text.length());

            if ((flags & GET_EXTRACTED_TEXT_MONITOR) != 0)
                mUpdateRequest = req;
            return extract;

        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: getExtractedText interrupted", e);
            return null;
        }
    }

    @Override
    public CharSequence getTextAfterCursor(int length, int flags) {
        Log.d("GeckoAppJava", "IME: getTextAfterCursor");

        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_SELECTION, 0, 0));
        try {
            mQueryResult.take();
        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: getTextBefore/AfterCursor interrupted", e);
            return null;
        }

        /* Compatible with both positive and negative length
            (no need for separate code for getTextBeforeCursor) */
        int textStart = mSelectionStart;
        int textLength = length;

        if (length < 0) {
          textStart += length;
          textLength = -length;
          if (textStart < 0) {
            textStart = 0;
            textLength = mSelectionStart;
          }
        }

        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_TEXT, textStart, textLength));
        try {
            return mQueryResult.take();
        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: getTextBefore/AfterCursor: Interrupted!", e);
            return null;
        }
    }

    @Override
    public CharSequence getTextBeforeCursor(int length, int flags) {
        Log.d("GeckoAppJava", "IME: getTextBeforeCursor");

        return getTextAfterCursor(-length, flags);
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        Log.d("GeckoAppJava", "IME: setComposingText");

        enableChangeNotifications();

        // Set new composing text
        mComposingText = text != null ? text.toString() : "";

        if (!mComposing) {
            // Get current selection
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_GET_SELECTION, 0, 0));
            try {
                mQueryResult.take();
            } catch (InterruptedException e) {
                Log.e("GeckoAppJava", "IME: setComposingText interrupted", e);
                return false;
            }

            if (mComposingText.length() == 0) {
                // Empty composing text is usually sent by IME to delete the selection (for example, ezKeyboard)
                if (mSelectionLength > 0)
                    GeckoAppShell.sendEventToGecko(new GeckoEvent(GeckoEvent.IME_DELETE_TEXT, 0, 0));

                // Some IMEs such as iWnn sometimes call with empty composing 
                // text.  (See bug 664364)
                // If composing text is empty, ignore this and don't start
                // compositing.
                return true;
            }

            // Make sure we are in a composition
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_BEGIN, 0, 0));
            mComposing = true;
            mCompositionStart = mSelectionLength >= 0 ?
                mSelectionStart : mSelectionStart + mSelectionLength;
        }

        // Set new selection
        // New selection should be within the composition
        mCompositionSelStart = newCursorPosition > 0 ? mComposingText.length() : 0;
        mCompositionSelLen = 0;

        // Handle composition text styles
        if (text != null && text instanceof Spanned) {
            Spanned span = (Spanned) text;
            int spanStart = 0, spanEnd = 0;
            boolean pastSelStart = false, pastSelEnd = false;

            do {
                int rangeType = GeckoEvent.IME_RANGE_CONVERTEDTEXT;
                int rangeStyles = 0, rangeForeColor = 0, rangeBackColor = 0;

                // Find next offset where there is a style transition
                spanEnd = span.nextSpanTransition(spanStart + 1, text.length(),
                    CharacterStyle.class);

                // We need to count the selection as a transition
                if (mCompositionSelLen >= 0) {
                    if (!pastSelStart && spanEnd >= mCompositionSelStart) {
                        spanEnd = mCompositionSelStart;
                        pastSelStart = true;
                    } else if (!pastSelEnd && spanEnd >=
                            mCompositionSelStart + mCompositionSelLen) {
                        spanEnd = mCompositionSelStart + mCompositionSelLen;
                        pastSelEnd = true;
                        rangeType = GeckoEvent.IME_RANGE_SELECTEDRAWTEXT;
                    }
                } else {
                    if (!pastSelEnd && spanEnd >=
                            mCompositionSelStart + mCompositionSelLen) {
                        spanEnd = mCompositionSelStart + mCompositionSelLen;
                        pastSelEnd = true;
                    } else if (!pastSelStart &&
                            spanEnd >= mCompositionSelStart) {
                        spanEnd = mCompositionSelStart;
                        pastSelStart = true;
                        rangeType = GeckoEvent.IME_RANGE_SELECTEDRAWTEXT;
                    }
                }
                // Empty range, continue
                if (spanEnd <= spanStart)
                    continue;

                // Get and iterate through list of span objects within range
                CharacterStyle styles[] = span.getSpans(
                    spanStart, spanEnd, CharacterStyle.class);

                for (CharacterStyle style : styles) {
                    if (style instanceof UnderlineSpan) {
                        // Text should be underlined
                        rangeStyles |= GeckoEvent.IME_RANGE_UNDERLINE;

                    } else if (style instanceof ForegroundColorSpan) {
                        // Text should be of a different foreground color
                        rangeStyles |= GeckoEvent.IME_RANGE_FORECOLOR;
                        rangeForeColor =
                            ((ForegroundColorSpan)style).getForegroundColor();

                    } else if (style instanceof BackgroundColorSpan) {
                        // Text should be of a different background color
                        rangeStyles |= GeckoEvent.IME_RANGE_BACKCOLOR;
                        rangeBackColor =
                            ((BackgroundColorSpan)style).getBackgroundColor();
                    }
                }

                // Add range to array, the actual styles are
                //  applied when IME_SET_TEXT is sent
                GeckoAppShell.sendEventToGecko(
                    new GeckoEvent(spanStart, spanEnd - spanStart,
                                   rangeType, rangeStyles,
                                   rangeForeColor, rangeBackColor));

                spanStart = spanEnd;
            } while (spanStart < text.length());
        } else {
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(0, text == null ? 0 : text.length(),
                               GeckoEvent.IME_RANGE_RAWINPUT,
                               GeckoEvent.IME_RANGE_UNDERLINE, 0, 0));
        }

        // Change composition (treating selection end as where the caret is)
        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(mCompositionSelStart + mCompositionSelLen, 0,
                           GeckoEvent.IME_RANGE_CARETPOSITION, 0, 0, 0,
                           mComposingText));

        return true;
    }

    @Override
    public boolean setComposingRegion(int start, int end) {
        Log.d("GeckoAppJava", "IME: setComposingRegion(start=" + start + ", end=" + end + ")");
        if (start < 0 || end < start)
            return true;

        CharSequence text = null;
        if (start == mCompositionStart && end - start == mComposingText.length()) {
            // Use mComposingText to avoid extra call to Gecko
            text = mComposingText;
        }

        finishComposingText();

        if (text == null && start < end) {
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_GET_TEXT, start, end - start));
            try {
                text = mQueryResult.take();
            } catch (InterruptedException e) {
                Log.e("GeckoAppJava", "IME: setComposingRegion interrupted", e);
                return false;
            }
        }

        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_SET_SELECTION, start, end - start));

        // Call setComposingText with the same text to start composition and let Gecko know about new composing region
        setComposingText(text, 1);

        return true;
    }

    @Override
    public boolean setSelection(int start, int end) {
        Log.d("GeckoAppJava", "IME: setSelection");

        if (mComposing) {
            /* Translate to fake selection positions */
            start -= mCompositionStart;
            end -= mCompositionStart;

            if (start < 0)
                start = 0;
            else if (start > mComposingText.length())
                start = mComposingText.length();

            if (end < 0)
                end = 0;
            else if (end > mComposingText.length())
                end = mComposingText.length();

            mCompositionSelStart = start;
            mCompositionSelLen = end - start;
        } else {
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_SET_SELECTION,
                               start, end - start));
        }
        return true;
    }

    public boolean onKeyDel() {
        // Some IMEs don't update us on deletions
        // In that case we are not updated when a composition
        // is destroyed, and Bad Things happen

        if (!mComposing)
            return false;

        if (mComposingText.length() > 0) {
            mComposingText = mComposingText.substring(0,
                mComposingText.length() - 1);
            if (mComposingText.length() > 0)
                return false;
        }

        commitText(null, 1);
        return true;
    }

    public void notifyTextChange(InputMethodManager imm, String text,
                                 int start, int oldEnd, int newEnd) {
        Log.d("GeckoAppShell", String.format("IME: notifyTextChange: text=%s s=%d ne=%d oe=%d",
                                              text, start, newEnd, oldEnd));
        if (!mChangeNotificationsEnabled)
            return;

        if (mBatchMode) {
            mBatchChanges.add(new ChangeNotification(text, start, oldEnd, newEnd));
            return;
        }

        mNumPendingChanges = Math.max(mNumPendingChanges - 1, 0);

        // If there are pending changes, that means this text is not the most up-to-date version
        // and we'll step on ourselves if we change the editable right now.
        View v = GeckoApp.mAppContext.getLayerController().getView();

        if (mNumPendingChanges == 0 && !text.contentEquals(mEditable))
            setEditable(text);

        if (mUpdateRequest == null)
            return;

        mUpdateExtract.flags = 0;

        // We update from (0, oldEnd) to (0, newEnd) because some Android IMEs
        // assume that updates start at zero, according to jchen.
        mUpdateExtract.partialStartOffset = 0;
        mUpdateExtract.partialEndOffset = oldEnd;

        // Faster to not query for selection
        mUpdateExtract.selectionStart = newEnd;
        mUpdateExtract.selectionEnd = newEnd;

        mUpdateExtract.text = text.substring(0, newEnd);
        mUpdateExtract.startOffset = 0;

        imm.updateExtractedText(v, mUpdateRequest.token, mUpdateExtract);
    }

    public void notifySelectionChange(InputMethodManager imm,
                                      int start, int end) {
        Log.d("GeckoAppJava", String.format("IME: notifySelectionChange: s=%d e=%d", start, end));

        if (!mChangeNotificationsEnabled)
            return;

        if (mBatchMode) {
            mBatchChanges.add(new ChangeNotification(start, end));
            return;
        }

        View v = GeckoApp.mAppContext.getLayerController().getView();
        if (mComposing)
            imm.updateSelection(v,
                                mCompositionStart + mCompositionSelStart,
                                mCompositionStart + mCompositionSelStart + mCompositionSelLen,
                                mCompositionStart,
                                mCompositionStart + mComposingText.length());
        else
            imm.updateSelection(v, start, end, -1, -1);

        // We only change the selection if we are relatively sure that the text we have is
        // up-to-date.  Bail out if we are stil expecting changes.
        if (mNumPendingChanges > 0)
            return;

        int maxLen = mEditable.length();
        Selection.setSelection(mEditable,
                               Math.min(start, maxLen),
                               Math.min(end, maxLen));
    }

    public void reset() {
        mComposing = false;
        mComposingText = "";
        mUpdateRequest = null;
        mNumPendingChanges = 0;
        mBatchMode = false;
        mBatchChanges.clear();
    }

    // TextWatcher
    public void onTextChanged(CharSequence s, int start, int before, int count)
    {
         Log.d("GeckoAppShell", String.format("IME: onTextChanged: t=%s s=%d b=%d l=%d",
                                              s, start, before, count));

        mNumPendingChanges++;
        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_SET_SELECTION, start, before));

        if (count == 0) {
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_DELETE_TEXT, 0, 0));
        } else {
            // Start and stop composition to force UI updates.
            finishComposingText();
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_BEGIN, 0, 0));

            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(0, count,
                               GeckoEvent.IME_RANGE_RAWINPUT, 0, 0, 0,
                               s.subSequence(start, start + count).toString()));

            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_END, 0, 0));

            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_SET_SELECTION, start + count, 0));
        }

        // Block this thread until all pending events are processed
        GeckoAppShell.geckoEventSync();
    }

    public void afterTextChanged(Editable s)
    {
    }

    public void beforeTextChanged(CharSequence s, int start, int count, int after)
    {
    }

    private void disableChangeNotifications() {
        mChangeNotificationsEnabled = false;
    }

    private void enableChangeNotifications() {
        mChangeNotificationsEnabled = true;
    }


    public InputConnection onCreateInputConnection(EditorInfo outAttrs)
    {
        Log.d("GeckoAppJava", "IME: handleCreateInputConnection called");

        outAttrs.inputType = InputType.TYPE_CLASS_TEXT;
        outAttrs.imeOptions = EditorInfo.IME_ACTION_NONE;
        outAttrs.actionLabel = null;
        mKeyListener = TextKeyListener.getInstance();

        if (mIMEState == IME_STATE_PASSWORD)
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_PASSWORD;
        else if (mIMETypeHint.equalsIgnoreCase("url"))
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_URI;
        else if (mIMETypeHint.equalsIgnoreCase("email"))
            outAttrs.inputType |= InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
        else if (mIMETypeHint.equalsIgnoreCase("search"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_SEARCH;
        else if (mIMETypeHint.equalsIgnoreCase("tel"))
            outAttrs.inputType = InputType.TYPE_CLASS_PHONE;
        else if (mIMETypeHint.equalsIgnoreCase("number") ||
                 mIMETypeHint.equalsIgnoreCase("range"))
            outAttrs.inputType = InputType.TYPE_CLASS_NUMBER;
        else if (mIMETypeHint.equalsIgnoreCase("datetime") ||
                 mIMETypeHint.equalsIgnoreCase("datetime-local"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME |
                                 InputType.TYPE_DATETIME_VARIATION_NORMAL;
        else if (mIMETypeHint.equalsIgnoreCase("date"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME |
                                 InputType.TYPE_DATETIME_VARIATION_DATE;
        else if (mIMETypeHint.equalsIgnoreCase("time"))
            outAttrs.inputType = InputType.TYPE_CLASS_DATETIME |
                                 InputType.TYPE_DATETIME_VARIATION_TIME;

        if (mIMEActionHint.equalsIgnoreCase("go"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_GO;
        else if (mIMEActionHint.equalsIgnoreCase("done"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE;
        else if (mIMEActionHint.equalsIgnoreCase("next"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_NEXT;
        else if (mIMEActionHint.equalsIgnoreCase("search"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_SEARCH;
        else if (mIMEActionHint.equalsIgnoreCase("send"))
            outAttrs.imeOptions = EditorInfo.IME_ACTION_SEND;
        else if (mIMEActionHint != null && mIMEActionHint.length() != 0)
            outAttrs.actionLabel = mIMEActionHint;

        if (mIMELandscapeFS == false)
            outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_EXTRACT_UI;

        reset();
        return this;
    }

    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        switch (event.getAction()) {
            case KeyEvent.ACTION_DOWN:
                return processKeyDown(keyCode, event, true);
            case KeyEvent.ACTION_UP:
                return processKeyUp(keyCode, event, true);
            case KeyEvent.ACTION_MULTIPLE:
                return onKeyMultiple(keyCode, event.getRepeatCount(), event);
        }
        return false;
    }

    public boolean onKeyDown(int keyCode, KeyEvent event) {
        return processKeyDown(keyCode, event, false);
    }

    private boolean processKeyDown(int keyCode, KeyEvent event, boolean isPreIme) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_MENU:
            case KeyEvent.KEYCODE_BACK:
            case KeyEvent.KEYCODE_VOLUME_UP:
            case KeyEvent.KEYCODE_VOLUME_DOWN:
            case KeyEvent.KEYCODE_SEARCH:
                return false;
            case KeyEvent.KEYCODE_DEL:
                // See comments in GeckoInputConnection.onKeyDel
                if (onKeyDel()) {
                    return true;
                }
                break;
            case KeyEvent.KEYCODE_ENTER:
                if ((event.getFlags() & KeyEvent.FLAG_EDITOR_ACTION) != 0 &&
                    mIMEActionHint.equalsIgnoreCase("next"))
                    event = new KeyEvent(event.getAction(), KeyEvent.KEYCODE_TAB);
                break;
            default:
                break;
        }

        if (isPreIme && mIMEState != IME_STATE_DISABLED &&
            (event.getMetaState() & KeyEvent.META_ALT_ON) == 0)
            // Let active IME process pre-IME key events
            return false;

        View v = GeckoApp.mAppContext.getLayerController().getView();

        // KeyListener returns true if it handled the event for us.
        if (mIMEState == IME_STATE_DISABLED ||
            keyCode == KeyEvent.KEYCODE_ENTER ||
            keyCode == KeyEvent.KEYCODE_DEL ||
            (event.getFlags() & KeyEvent.FLAG_SOFT_KEYBOARD) != 0 ||
            !mKeyListener.onKeyDown(v, mEditable, keyCode, event))
            GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
        return true;
    }

    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return processKeyUp(keyCode, event, false);
    }

    private boolean processKeyUp(int keyCode, KeyEvent event, boolean isPreIme) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BACK:
            case KeyEvent.KEYCODE_SEARCH:
            case KeyEvent.KEYCODE_MENU:
                return false;
            default:
                break;
        }

        if (isPreIme && mIMEState != IME_STATE_DISABLED &&
            (event.getMetaState() & KeyEvent.META_ALT_ON) == 0)
            // Let active IME process pre-IME key events
            return false;
        View v = GeckoApp.mAppContext.getLayerController().getView();

        if (mIMEState == IME_STATE_DISABLED ||
            keyCode == KeyEvent.KEYCODE_ENTER ||
            keyCode == KeyEvent.KEYCODE_DEL ||
            (event.getFlags() & KeyEvent.FLAG_SOFT_KEYBOARD) != 0 ||
            !mKeyListener.onKeyUp(v, mEditable, keyCode, event))
            GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
        return true;
    }

    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        GeckoAppShell.sendEventToGecko(new GeckoEvent(event));
        return true;
    }

    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        View v = GeckoApp.mAppContext.getLayerController().getView();
        switch (keyCode) {
            case KeyEvent.KEYCODE_MENU:
                InputMethodManager imm = (InputMethodManager)
                    v.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
                imm.toggleSoftInputFromWindow(v.getWindowToken(),
                                              imm.SHOW_FORCED, 0);
                return true;
            default:
                break;
        }
        return false;
    }


    public void notifyIME(int type, int state) {

        View v = GeckoApp.mAppContext.getLayerController().getView();

        Log.d("GeckoAppJava", "notifyIME");

        if (v == null)
            return;

        Log.d("GeckoAppJava", "notifyIME v!= null");

        switch (type) {
        case NOTIFY_IME_RESETINPUTSTATE:

        Log.d("GeckoAppJava", "notifyIME = reset");
            // Composition event is already fired from widget.
            // So reset IME flags.
            reset();

            // Don't use IMEStateUpdater for reset.
            // Because IME may not work showSoftInput()
            // after calling restartInput() immediately.
            // So we have to call showSoftInput() delay.
            InputMethodManager imm = (InputMethodManager) v.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                // no way to reset IME status directly
                IMEStateUpdater.resetIME();
            } else {
                imm.restartInput(v);
            }

            // keep current enabled state
            IMEStateUpdater.enableIME();
            break;

        case NOTIFY_IME_CANCELCOMPOSITION:
        Log.d("GeckoAppJava", "notifyIME = cancel");
            IMEStateUpdater.resetIME();
            break;

        case NOTIFY_IME_FOCUSCHANGE:
        Log.d("GeckoAppJava", "notifyIME = focus");
            IMEStateUpdater.resetIME();
            break;
        }
    }

    public void notifyIMEEnabled(int state, String typeHint,
                                        String actionHint, boolean landscapeFS)
    {
        View v = GeckoApp.mAppContext.getLayerController().getView();

        if (v == null)
            return;

        /* When IME is 'disabled', IME processing is disabled.
           In addition, the IME UI is hidden */
        mIMEState = state;
        mIMETypeHint = typeHint;
        mIMEActionHint = actionHint;
        mIMELandscapeFS = landscapeFS;
        IMEStateUpdater.enableIME();
    }


    public void notifyIMEChange(String text, int start, int end, int newEnd) {
        View v = GeckoApp.mAppContext.getLayerController().getView();

        if (v == null)
            return;

        InputMethodManager imm = (InputMethodManager) v.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm == null)
            return;

        Log.d("GeckoAppJava", String.format("IME: notifyIMEChange: t=%s s=%d ne=%d oe=%d",
                                            text, start, newEnd, end));

        if (newEnd < 0)
            notifySelectionChange(imm, start, end);
        else
            notifyTextChange(imm, text, start, end, newEnd);
    }


    public void returnIMEQueryResult(String result, int selectionStart, int selectionLength) {
        mSelectionStart = selectionStart;
        mSelectionLength = selectionLength;
        try {
            mQueryResult.put(result);
        } catch (InterruptedException e) {}
    }

    static private final Timer mIMETimer = new Timer();

    static private final int NOTIFY_IME_RESETINPUTSTATE = 0;
    static private final int NOTIFY_IME_SETOPENSTATE = 1;
    static private final int NOTIFY_IME_CANCELCOMPOSITION = 2;
    static private final int NOTIFY_IME_FOCUSCHANGE = 3;


    /* Delay updating IME states (see bug 573800) */
    private static final class IMEStateUpdater extends TimerTask
    {
        static private IMEStateUpdater instance;
        private boolean mEnable, mReset;

        static private IMEStateUpdater getInstance() {
            if (instance == null) {
                instance = new IMEStateUpdater();
                mIMETimer.schedule(instance, 200);
            }
            return instance;
        }

        static public synchronized void enableIME() {
            getInstance().mEnable = true;
        }

        static public synchronized void resetIME() {
            getInstance().mReset = true;
        }

        public void run() {
            Log.d("GeckoAppJava", "IME: run()");
            synchronized(IMEStateUpdater.class) {
                instance = null;
            }

            View v = GeckoApp.mAppContext.getLayerController().getView();
            Log.d("GeckoAppJava", "IME: v="+v);

            InputMethodManager imm = (InputMethodManager) v.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm == null)
                return;

            if (mReset)
                imm.restartInput(v);

            if (!mEnable)
                return;

            if (mIMEState != IME_STATE_DISABLED &&
                mIMEState != IME_STATE_PLUGIN)
                imm.showSoftInput(v, 0);
            else
                imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
        }
    }

    public void setEditable(String contents)
    {
        mEditable.removeSpan(this);
        mEditable.replace(0, mEditable.length(), contents);
        mEditable.setSpan(this, 0, contents.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE);
        Selection.setSelection(mEditable, contents.length());
    }

    public void initEditable(String contents)
    {
        mEditable = mEditableFactory.newEditable(contents);
        mEditable.setSpan(this, 0, contents.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE);
        Selection.setSelection(mEditable, contents.length());
    }

    // Is a composition active?
    boolean mComposing;
    // Composition text when a composition is active
    String mComposingText = "";
    // Start index of the composition within the text body
    int mCompositionStart;
    /* During a composition, we should not alter the real selection,
        therefore we keep our own offsets to emulate selection */
    // Start of fake selection, relative to start of composition
    int mCompositionSelStart;
    // Length of fake selection
    int mCompositionSelLen;
    // Number of in flight changes
    int mNumPendingChanges;

    // IME stuff
    public static final int IME_STATE_DISABLED = 0;
    public static final int IME_STATE_ENABLED = 1;
    public static final int IME_STATE_PASSWORD = 2;
    public static final int IME_STATE_PLUGIN = 3;

    KeyListener mKeyListener;
    Editable mEditable;
    Editable.Factory mEditableFactory;
    static int mIMEState;
    static String mIMETypeHint;
    static String mIMEActionHint;
    static boolean mIMELandscapeFS;

    private boolean mBatchMode;
    private boolean mChangeNotificationsEnabled = true;

    private CopyOnWriteArrayList<ChangeNotification> mBatchChanges =
        new CopyOnWriteArrayList<ChangeNotification>();

    ExtractedTextRequest mUpdateRequest;
    final ExtractedText mUpdateExtract = new ExtractedText();

    int mSelectionStart, mSelectionLength;
    SynchronousQueue<String> mQueryResult;
}

