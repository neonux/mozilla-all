/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu>
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

#ifndef mozilla_Assertions_h_
#define mozilla_Assertions_h_

#include "mozilla/Types.h"

/*
 * XXX: we're cheating here in order to avoid creating object files
 * for mfbt /just/ to provide a function like FatalError() to be used
 * by MOZ_ASSERT().  (It'll happen eventually, but for just ASSERT()
 * it isn't worth the pain.)  JS_Assert(), although unfortunately
 * named, is part of SpiderMonkey's stable, external API, so this
 * isn't quite as bad as it seems.
 *
 * Once mfbt needs object files, this unholy union with JS_Assert()
 * will be broken.
 *
 * JS_Assert is present even in release builds, for the benefit of applications
 * that build DEBUG and link against a non-DEBUG SpiderMonkey library.
 */
#ifdef __cplusplus
extern "C" {
#endif

extern MFBT_API(void)
JS_Assert(const char* s, const char* file, int ln);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*
 * MOZ_ASSERT() is a "strong" assertion of state, like libc's
 * assert().  If a MOZ_ASSERT() fails in a debug build, the process in
 * which it fails will stop running in a loud and dramatic way.
 */
#ifdef DEBUG
#  define MOZ_ASSERT(expr_)                                      \
     ((expr_) ? ((void)0) : JS_Assert(#expr_, __FILE__, __LINE__))
#else
#  define MOZ_ASSERT(expr_) ((void)0)
#endif /* DEBUG */

#endif /* mozilla_Assertions_h_ */
