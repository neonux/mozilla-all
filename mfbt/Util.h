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

/*
 * Miscellaneous uncategorized functionality.  Please add new functionality to
 * new headers, or to other appropriate existing headers, not here.
 */

#ifndef mozilla_Util_h_
#define mozilla_Util_h_

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Types.h"

#ifdef __cplusplus

namespace mozilla {

/**
 * DebugOnly contains a value of type T, but only in debug builds.  In
 * release builds, it does not contain a value.  This helper is
 * intended to be used along with ASSERT()-style macros, allowing one
 * to write
 *
 *   DebugOnly<bool> check = Func();
 *   ASSERT(check);
 *
 * more concisely than declaring |check| conditional on #ifdef DEBUG,
 * but also without allocating storage space for |check| in release
 * builds.
 *
 * DebugOnly instances can only be coerced to T in debug builds; in
 * release builds, they don't have a value so type coercion is not
 * well defined.
 */
template <typename T>
struct DebugOnly
{
#ifdef DEBUG
    T value;

    DebugOnly() {}
    DebugOnly(const T& other) : value(other) {}
    DebugOnly& operator=(const T& rhs) {
        value = rhs;
        return *this;
    }
    void operator++(int) {
        value++;
    }
    void operator--(int) {
        value--;
    }

    operator T&() { return value; }
    operator const T&() const { return value; }

    T& operator->() { return value; }

#else
    DebugOnly() {}
    DebugOnly(const T&) {}
    DebugOnly& operator=(const T&) { return *this; }
    void operator++(int) {}
    void operator--(int) {}
#endif

    /*
     * DebugOnly must always have a destructor or else it will
     * generate "unused variable" warnings, exactly what it's intended
     * to avoid!
     */
    ~DebugOnly() {}
};

/*
 * This class, and the corresponding macro MOZ_ALIGNOF, figure out how many 
 * bytes of alignment a given type needs.
 */
template<class T>
struct AlignmentFinder
{
private:
  struct Aligner
  {
    char c;
    T t;
  };

public:
  static const int alignment = sizeof(Aligner) - sizeof(T);
};

#define MOZ_ALIGNOF(T) mozilla::AlignmentFinder<T>::alignment

/*
 * Declare the MOZ_ALIGNED_DECL macro for declaring aligned types.
 *
 * For instance,
 *
 *   MOZ_ALIGNED_DECL(char arr[2], 8);
 *
 * will declare a two-character array |arr| aligned to 8 bytes.
 */

#if defined(__GNUC__)
#  define MOZ_ALIGNED_DECL(_type, _align) \
     _type __attribute__((aligned(_align)))
#elif defined(_MSC_VER)
#  define MOZ_ALIGNED_DECL(_type, _align) \
     __declspec(align(_align)) _type
#else
#  warning "We don't know how to align variables on this compiler."
#  define MOZ_ALIGNED_DECL(_type, _align) _type
#endif

/*
 * AlignedElem<N> is a structure whose alignment is guaranteed to be at least N bytes.
 *
 * We support 1, 2, 4, 8, and 16-bit alignment.
 */
template<size_t align>
struct AlignedElem;

/*
 * We have to specialize this template because GCC doesn't like __attribute__((aligned(foo))) where
 * foo is a template parameter.
 */

template<>
struct AlignedElem<1>
{
  MOZ_ALIGNED_DECL(uint8_t elem, 1);
};

template<>
struct AlignedElem<2>
{
  MOZ_ALIGNED_DECL(uint8_t elem, 2);
};

template<>
struct AlignedElem<4>
{
  MOZ_ALIGNED_DECL(uint8_t elem, 4);
};

template<>
struct AlignedElem<8>
{
  MOZ_ALIGNED_DECL(uint8_t elem, 8);
};

template<>
struct AlignedElem<16>
{
  MOZ_ALIGNED_DECL(uint8_t elem, 16);
};

/*
 * This utility pales in comparison to Boost's aligned_storage. The utility
 * simply assumes that uint64_t is enough alignment for anyone. This may need
 * to be extended one day...
 *
 * As an important side effect, pulling the storage into this template is
 * enough obfuscation to confuse gcc's strict-aliasing analysis into not giving
 * false negatives when we cast from the char buffer to whatever type we've
 * constructed using the bytes.
 */
template <size_t nbytes>
struct AlignedStorage
{
    union U {
        char bytes[nbytes];
        uint64_t _;
    } u;

    const void *addr() const { return u.bytes; }
    void *addr() { return u.bytes; }
};

template <class T>
struct AlignedStorage2
{
    union U {
        char bytes[sizeof(T)];
        uint64_t _;
    } u;

    const T *addr() const { return (const T *)u.bytes; }
    T *addr() { return (T *)(void *)u.bytes; }
};

/*
 * Small utility for lazily constructing objects without using dynamic storage.
 * When a Maybe<T> is constructed, it is |empty()|, i.e., no value of T has
 * been constructed and no T destructor will be called when the Maybe<T> is
 * destroyed. Upon calling |construct|, a T object will be constructed with the
 * given arguments and that object will be destroyed when the owning Maybe<T>
 * is destroyed.
 *
 * N.B. GCC seems to miss some optimizations with Maybe and may generate extra
 * branches/loads/stores. Use with caution on hot paths.
 */
template <class T>
class Maybe
{
    AlignedStorage2<T> storage;
    bool constructed;

    T &asT() { return *storage.addr(); }

    explicit Maybe(const Maybe &other);
    const Maybe &operator=(const Maybe &other);

  public:
    Maybe() { constructed = false; }
    ~Maybe() { if (constructed) asT().~T(); }

    bool empty() const { return !constructed; }

    void construct() {
        MOZ_ASSERT(!constructed);
        new(storage.addr()) T();
        constructed = true;
    }

    template <class T1>
    void construct(const T1 &t1) {
        MOZ_ASSERT(!constructed);
        new(storage.addr()) T(t1);
        constructed = true;
    }

    template <class T1, class T2>
    void construct(const T1 &t1, const T2 &t2) {
        MOZ_ASSERT(!constructed);
        new(storage.addr()) T(t1, t2);
        constructed = true;
    }

    template <class T1, class T2, class T3>
    void construct(const T1 &t1, const T2 &t2, const T3 &t3) {
        MOZ_ASSERT(!constructed);
        new(storage.addr()) T(t1, t2, t3);
        constructed = true;
    }

    template <class T1, class T2, class T3, class T4>
    void construct(const T1 &t1, const T2 &t2, const T3 &t3, const T4 &t4) {
        MOZ_ASSERT(!constructed);
        new(storage.addr()) T(t1, t2, t3, t4);
        constructed = true;
    }

    T *addr() {
        MOZ_ASSERT(constructed);
        return &asT();
    }

    T &ref() {
        MOZ_ASSERT(constructed);
        return asT();
    }

    const T &ref() const {
        MOZ_ASSERT(constructed);
        return const_cast<Maybe *>(this)->asT();
    }

    void destroy() {
        ref().~T();
        constructed = false;
    }

    void destroyIfConstructed() {
        if (!empty())
            destroy();
    }
};

/*
 * Safely subtract two pointers when it is known that end >= begin.  This avoids
 * the common compiler bug that if (size_t(end) - size_t(begin)) has the MSB
 * set, the unsigned subtraction followed by right shift will produce -1, or
 * size_t(-1), instead of the real difference.
 */
template <class T>
MOZ_ALWAYS_INLINE size_t
PointerRangeSize(T* begin, T* end)
{
    MOZ_ASSERT(end >= begin);
    return (size_t(end) - size_t(begin)) / sizeof(T);
}

/*
 * Compute the length of an array with constant length.  (Use of this method
 * with a non-array pointer will not compile.)
 *
 * Beware of the implicit trailing '\0' when using this with string constants.
 */
template<typename T, size_t N>
size_t
ArrayLength(T (&arr)[N])
{
    return N;
}

/*
 * Compute the address one past the last element of a constant-length array.
 *
 * Beware of the implicit trailing '\0' when using this with string constants.
 */
template<typename T, size_t N>
T*
ArrayEnd(T (&arr)[N])
{
    return arr + ArrayLength(arr);
}

} /* namespace mozilla */

#endif /* __cplusplus */

#endif  /* mozilla_Util_h_ */
