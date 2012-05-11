/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#ifndef nsThreadManager_h__
#define nsThreadManager_h__

#include "mozilla/Mutex.h"
#include "nsIThreadManager.h"
#include "nsRefPtrHashtable.h"
#include "nsThread.h"

class nsIRunnable;

class nsThreadManager : public nsIThreadManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADMANAGER

  static nsThreadManager *get() { return &sInstance; }
  static bool initialized() { return sInitialized; }

  nsresult Init();

  // Shutdown all threads.  This function should only be called on the main
  // thread of the application process.
  void Shutdown();

  // Called by nsThread to inform the ThreadManager it exists.  This method
  // must be called when the given thread is the current thread.
  void RegisterCurrentThread(nsThread *thread);

  // Called by nsThread to inform the ThreadManager it is going away.  This
  // method must be called when the given thread is the current thread.
  void UnregisterCurrentThread(nsThread *thread);

  // Returns the current thread.  Returns null if OOM or if ThreadManager isn't
  // initialized.
  nsThread *GetCurrentThread();

  // This needs to be public in order to support static instantiation of this
  // class with older compilers (e.g., egcs-2.91.66).
  ~nsThreadManager() {}

  nsThread *getChromeThread() { return mChromeZone.thread; }

private:
  nsThreadManager()
    : mCurThreadIndex(0)
    , mMainPRThread(nsnull)
    , mLock(nsnull)
    , mInitialized(false) {
  }
  
  static nsThreadManager sInstance;
  static bool sInitialized;

  nsRefPtrHashtable<nsPtrHashKey<PRThread>, nsThread> mThreadsByPRThread;
  PRUintn             mCurThreadIndex;  // thread-local-storage index
  nsRefPtr<nsThread>  mMainThread;
  PRThread           *mMainPRThread;
  uintptr_t           mMainThreadStackPosition;
  // This is a pointer in order to allow creating nsThreadManager from
  // the static context in debug builds.
  nsAutoPtr<mozilla::Mutex> mLock;  // protects tables
  bool                mInitialized;
  size_t              mCantLockNewContent;

  struct Zone {
    Zone()
      : thread(NULL)
      , prThread(NULL)
      , threadStackPosition(0)
      , lock(NULL)
      , owner(NULL)
      , depth(0)
      , stalled(false)
      , waiting(false)
      , sticky(false)
      , unlockCount(0)
    {}

    nsRefPtr<nsThread> thread; // XXX leaks
    PRThread *prThread;
    uintptr_t threadStackPosition;
    PRLock *lock;
    PRThread *owner;
    size_t depth;
    bool stalled;
    bool waiting;
    bool sticky;
    size_t unlockCount; // XXX remove -- debugging

    void clearOwner() {
      MOZ_ASSERT(owner);
      owner = NULL;
      depth = 0;
      stalled = false;
      sticky = false;
    }
  };

  struct SavedZone {
    SavedZone(Zone *zone) : zone(zone), depth(0), sticky(false) {}
    Zone *zone;
    size_t depth;
    bool sticky;
  };

  void SaveLock(SavedZone &v);
  void RestoreLock(SavedZone &v, PRThread *current);

  Zone mChromeZone;
  Zone mContentZones[JS_ZONE_CONTENT_LIMIT];
  bool mEverythingLocked;
  PRUint64 mAllocatedBitmask;

  static inline bool HasBit(PRUint64 bitmask, size_t bit)
  {
    MOZ_ASSERT(bit < 64);
    return bitmask & (1 << bit);
  }

  static inline void SetBit(PRUint64 *pbitmask, size_t bit)
  {
    MOZ_ASSERT(bit < 64);
    (*pbitmask) |= (1 << bit);
  }

  static inline void ClearBit(PRUint64 *pbitmask, size_t bit)
  {
    MOZ_ASSERT(bit < 64);
    (*pbitmask) &= ~(1 << bit);
  }

  Zone &getZone(PRInt32 zone) {
    MOZ_ASSERT(zone != JS_ZONE_NONE);
    if (zone == JS_ZONE_CHROME)
      return mChromeZone;
    MOZ_ASSERT(zone < JS_ZONE_CONTENT_LIMIT);
    return mContentZones[zone];
  }
};

#define NS_THREADMANAGER_CLASSNAME "nsThreadManager"
#define NS_THREADMANAGER_CID                       \
{ /* 7a4204c6-e45a-4c37-8ebb-6709a22c917c */       \
  0x7a4204c6,                                      \
  0xe45a,                                          \
  0x4c37,                                          \
  {0x8e, 0xbb, 0x67, 0x09, 0xa2, 0x2c, 0x91, 0x7c} \
}

#endif  // nsThreadManager_h__
