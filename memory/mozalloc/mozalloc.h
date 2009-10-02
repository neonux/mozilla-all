/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_mozalloc_h
#define mozilla_mozalloc_h


/*
 * https://bugzilla.mozilla.org/show_bug.cgi?id=427099
 */

/* 
 * NB: this header depends on the symbols malloc(), free(), and
 * std::bad_alloc.  But because this header is used in situations
 * where malloc/free have different visibility, we rely on code
 * including this header to provide the declarations of malloc/free.
 * I.e., we don't #include <stdlib.h> or <new> on purpose.
 */

#if defined(XP_WIN) || (defined(XP_OS2) && defined(__declspec))
#  define MOZALLOC_EXPORT __declspec(dllexport)
#elif defined(HAVE_VISIBILITY_ATTRIBUTE)
/* Make sure symbols are still exported even if we're wrapped in a
 * |visibility push(hidden)| blanket. */
#  define MOZALLOC_EXPORT __attribute__ ((visibility ("default")))
#else
#  define MOZALLOC_EXPORT
#endif


#if defined(NS_ALWAYS_INLINE)
#  define MOZALLOC_INLINE NS_ALWAYS_INLINE inline
#elif defined(HAVE_FORCEINLINE)
#  define MOZALLOC_INLINE __forceinline
#else
#  define MOZALLOC_INLINE inline
#endif


#if defined(__cplusplus)
extern "C" {
#endif /* ifdef __cplusplus */


/* 
 * If we don't have these system functions, but do have jemalloc
 * replacements, go ahead and declare them independently of jemalloc.
 * Trying to #include the jemalloc header causes redeclaration of some
 * system functions with different visibility.
 */
/* FIXME/cjones: make something like the following work with jemalloc */
#if 0
#if !defined(HAVE_POSIX_MEMALIGN) && defined(HAVE_JEMALLOC_POSIX_MEMALIGN)
MOZALLOC_IMPORT int posix_memalign(void **, size_t, size_t)
    NS_WARN_UNUSED_RESULT;
#endif
#endif


/*
 * Each pair of declarations below is analogous to a "standard"
 * allocation function, except that the out-of-memory handling is made
 * explicit.  The |moz_x| versions will never return a NULL pointer;
 * if memory is exhausted, they abort.  The |moz_| versions may return
 * NULL pointers if memory is exhausted: their return value must be
 * checked.
 *
 * All these allocation functions are *guaranteed* to return a pointer
 * to memory allocated in such a way that that memory can be freed by
 * passing that pointer to whatever function the symbol |free()|
 * resolves to at link time.
 */

MOZALLOC_EXPORT void* moz_xmalloc(size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT
void* moz_malloc(size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;


MOZALLOC_EXPORT void* moz_xcalloc(size_t nmemb, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT void* moz_calloc(size_t nmemb, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;


MOZALLOC_EXPORT void* moz_xrealloc(void* ptr, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT void* moz_realloc(void* ptr, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;


MOZALLOC_EXPORT char* moz_xstrdup(const char* str)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT char* moz_strdup(const char* str)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;


#if defined(HAVE_STRNDUP)
MOZALLOC_EXPORT char* moz_xstrndup(const char* str, size_t strsize)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT char* moz_strndup(const char* str, size_t strsize)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;
#endif /* if defined(HAVE_STRNDUP) */


#if defined(HAVE_POSIX_MEMALIGN)
MOZALLOC_EXPORT int moz_xposix_memalign(void **ptr, size_t alignment, size_t size)
    NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT int moz_posix_memalign(void **ptr, size_t alignment, size_t size)
    NS_WARN_UNUSED_RESULT;
#endif /* if defined(HAVE_POSIX_MEMALIGN) */


#if defined(HAVE_MEMALIGN)
MOZALLOC_EXPORT void* moz_xmemalign(size_t boundary, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT void* moz_memalign(size_t boundary, size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;
#endif /* if defined(HAVE_MEMALIGN) */


#if defined(HAVE_VALLOC)
MOZALLOC_EXPORT void* moz_xvalloc(size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;

MOZALLOC_EXPORT void* moz_valloc(size_t size)
    NS_ATTR_MALLOC NS_WARN_UNUSED_RESULT;
#endif /* if defined(HAVE_VALLOC) */


#ifdef __cplusplus
} /* extern "C" */
#endif /* ifdef __cplusplus */


#ifdef __cplusplus

/*
 * We implement the default operators new/delete as part of
 * libmozalloc, replacing their definitions in libstdc++.  The
 * operator new* definitions in libmozalloc will never return a NULL
 * pointer.
 *
 * Each operator new immediately below returns a pointer to memory
 * that can be delete'd by any of
 *
 *   (1) the matching infallible operator delete immediately below
 *   (2) the matching "fallible" operator delete further below
 *   (3) the matching system |operator delete(void*, std::nothrow)|
 *   (4) the matching system |operator delete(void*) throw(std::bad_alloc)|
 *
 * NB: these are declared |throw(std::bad_alloc)|, though they will never
 * throw that exception.  This declaration is consistent with the rule
 * that |::operator new() throw(std::bad_alloc)| will never return NULL.
 */

MOZALLOC_INLINE
void* operator new(size_t size) throw(std::bad_alloc)
{
    return moz_xmalloc(size);
}

MOZALLOC_INLINE
void* operator new[](size_t size) throw(std::bad_alloc)
{
    return moz_xmalloc(size);
}

MOZALLOC_INLINE
void operator delete(void* ptr) throw()
{
    return free(ptr);
}

MOZALLOC_INLINE
void operator delete[](void* ptr) throw()
{
    return free(ptr);
}


/*
 * We also add a new allocator variant: "fallible operator new."
 * Unlike libmozalloc's implementations of the standard nofail
 * allocators, this allocator is allowed to return NULL.  It can be used
 * as follows
 *
 *   Foo* f = new (mozilla::fallible) Foo(...);
 *
 * operator delete(fallible) is defined for completeness only.
 *
 * Each operator new below returns a pointer to memory that can be
 * delete'd by any of
 *
 *   (1) the matching "fallible" operator delete below
 *   (2) the matching infallible operator delete above
 *   (3) the matching system |operator delete(void*, std::nothrow)|
 *   (4) the matching system |operator delete(void*) throw(std::bad_alloc)|
 */

namespace mozilla {

struct MOZALLOC_EXPORT fallible_t { };

} /* namespace mozilla */

MOZALLOC_INLINE
void* operator new(size_t size, const mozilla::fallible_t&) throw()
{
    return malloc(size);
}

MOZALLOC_INLINE
void* operator new[](size_t size, const mozilla::fallible_t&) throw()
{
    return malloc(size);
}

MOZALLOC_INLINE
void operator delete(void* ptr, const mozilla::fallible_t&) throw()
{
    free(ptr);
}

MOZALLOC_INLINE
void operator delete[](void* ptr, const mozilla::fallible_t&) throw()
{
    free(ptr);
}

#endif  /* ifdef __cplusplus */


#endif /* ifndef mozilla_mozalloc_h */
