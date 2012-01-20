/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Utils_h
#define Utils_h

#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>

/**
 * On architectures that are little endian and that support unaligned reads,
 * we can use direct type, but on others, we want to have a special class
 * to handle conversion and alignment issues.
 */
#if defined(__i386__) || defined(__x86_64__)
typedef uint16_t le_uint16;
typedef uint32_t le_uint32;
#else

/**
 * Template that allows to find an unsigned int type from a (computed) bit size
 */
template <int s> struct UInt { };
template <> struct UInt<16> { typedef uint16_t Type; };
template <> struct UInt<32> { typedef uint32_t Type; };

/**
 * Template to read 2 n-bit sized words as a 2*n-bit sized word, doing
 * conversion from little endian and avoiding alignment issues.
 */
template <typename T>
class le_to_cpu
{
public:
  operator typename UInt<16 * sizeof(T)>::Type() const
  {
    return (b << (sizeof(T) * 8)) | a;
  }
private:
  T a, b;
};

/**
 * Type definitions
 */
typedef le_to_cpu<unsigned char> le_uint16;
typedef le_to_cpu<le_uint16> le_uint32;
#endif

/**
 * AutoCloseFD is a RAII wrapper for POSIX file descriptors
 */
class AutoCloseFD
{
public:
  AutoCloseFD(): fd(-1) { }
  AutoCloseFD(int fd): fd(fd) { }
  ~AutoCloseFD()
  {
    if (fd != -1)
      close(fd);
  }

  operator int() const
  {
    return fd;
  }

  int forget()
  {
    int _fd = fd;
    fd = -1;
    return _fd;
  }

  bool operator ==(int other) const
  {
    return fd == other;
  }

  int operator =(int other)
  {
    if (fd != -1)
      close(fd);
    fd = other;
    return fd;
  }

private:
  int fd;
};

/**
 * MappedPtr is a RAII wrapper for mmap()ed memory. It can be used as
 * a simple void * or unsigned char *.
 */
class MappedPtr
{
public:
  MappedPtr(void *buf, size_t length): buf(buf), length(length) { }
  MappedPtr(): buf(MAP_FAILED), length(0) { }

  void Init(void *b, size_t len) {
    buf = b;
    length = len;
  }

  ~MappedPtr()
  {
    if (buf != MAP_FAILED)
      munmap(buf, length);
  }

  operator void *() const
  {
    return buf;
  }

  operator unsigned char *() const
  {
    return reinterpret_cast<unsigned char *>(buf);
  }

  bool operator ==(void *ptr) const {
    return buf == ptr;
  }

  bool operator ==(unsigned char *ptr) const {
    return buf == ptr;
  }

  void *operator +(off_t offset) const
  {
    return reinterpret_cast<char *>(buf) + offset;
  }

  /**
   * Returns whether the given address is within the mapped range
   */
  bool Contains(void *ptr) const
  {
    return (ptr >= buf) && (ptr < reinterpret_cast<char *>(buf) + length);
  }

private:
  void *buf;
  size_t length;
};

/**
 * UnsizedArray is a way to access raw arrays of data in memory.
 *
 *   struct S { ... };
 *   UnsizedArray<S> a(buf);
 *   UnsizedArray<S> b; b.Init(buf);
 *
 * This is roughly equivalent to
 *   const S *a = reinterpret_cast<const S *>(buf);
 *   const S *b = NULL; b = reinterpret_cast<const S *>(buf);
 *
 * An UnsizedArray has no known length, and it's up to the caller to make
 * sure the accessed memory is mapped and makes sense.
 */
template <typename T>
class UnsizedArray
{
public:
  typedef size_t idx_t;

  /**
   * Constructors and Initializers
   */
  UnsizedArray(): contents(NULL) { }
  UnsizedArray(const void *buf): contents(reinterpret_cast<const T *>(buf)) { }

  void Init(const void *buf)
  {
    // ASSERT(operator bool())
    contents = reinterpret_cast<const T *>(buf);
  }

  /**
   * Returns the nth element of the array
   */
  const T &operator[](const idx_t index) const
  {
    // ASSERT(operator bool())
    return contents[index];
  }

  /**
   * Returns whether the array points somewhere
   */
  operator bool() const
  {
    return contents != NULL;
  }
private:
  const T *contents;
};

/**
 * Array, like UnsizedArray, is a way to access raw arrays of data in memory.
 * Unlike UnsizedArray, it has a known length, and is enumerable with an
 * iterator.
 *
 *   struct S { ... };
 *   Array<S> a(buf, len);
 *   UnsizedArray<S> b; b.Init(buf, len);
 *
 * In the above examples, len is the number of elements in the array. It is
 * also possible to initialize an Array with the buffer size:
 *
 *   Array<S> c; c.InitSize(buf, size);
 *
 * It is also possible to initialize an Array in two steps, only providing
 * one data at a time:
 *
 *   Array<S> d;
 *   d.Init(buf);
 *   d.Init(len); // or d.InitSize(size);
 *
 */
template <typename T>
class Array: public UnsizedArray<T>
{
public:
  typedef typename UnsizedArray<T>::idx_t idx_t;

  /**
   * Constructors and Initializers
   */
  Array(): UnsizedArray<T>(), length(0) { }
  Array(const void *buf, const idx_t length)
  : UnsizedArray<T>(buf), length(length) { }

  void Init(const void *buf)
  {
    UnsizedArray<T>::Init(buf);
  }

  void Init(const idx_t len)
  {
    // ASSERT(length != 0)
    length = len;
  }

  void InitSize(const idx_t size)
  {
    Init(size / sizeof(T));
  }

  void Init(const void *buf, const idx_t len)
  {
    UnsizedArray<T>::Init(buf);
    Init(len);
  }

  void InitSize(const void *buf, const idx_t size)
  {
    UnsizedArray<T>::Init(buf);
    InitSize(size);
  }

  /**
   * Returns the nth element of the array
   */
  const T &operator[](const idx_t index) const
  {
    // ASSERT(index < length)
    // ASSERT(operator bool())
    return UnsizedArray<T>::operator[](index);
  }

  /**
   * Returns the number of elements in the array
   */
  idx_t numElements() const
  {
    return length;
  }

  /**
   * Returns whether the array points somewhere and has at least one element.
   */
  operator bool() const
  {
    return (length > 0) && UnsizedArray<T>::operator bool();
  }

  /**
   * Iterator for an Array. Use is similar to that of STL const_iterators:
   *
   *   struct S { ... };
   *   Array<S> a(buf, len);
   *   for (Array<S>::iterator it = a.begin(); it < a.end(); ++it) {
   *     // Do something with *it.
   *   }
   */
  class iterator
  {
  public:
    iterator(): item(NULL) { }

    const T &operator *() const
    {
      return *item;
    }

    const T *operator ->() const
    {
      return item;
    }

    const T &operator ++()
    {
      return *(++item);
    }

    bool operator<(const iterator &other) const
    {
      return item < other.item;
    }
  protected:
    friend class Array<T>;
    iterator(const T &item): item(&item) { }

  private:
    const T *item;
  };

  /**
   * Returns an iterator pointing at the beginning of the Array
   */
  iterator begin() const {
    if (length)
      return iterator(UnsizedArray<T>::operator[](0));
    return iterator();
  }

  /**
   * Returns an iterator pointing past the end of the Array
   */
  iterator end() const {
    if (length)
      return iterator(UnsizedArray<T>::operator[](length));
    return iterator();
  }
private:
  idx_t length;
};

/**
 * Transforms a pointer-to-function to a pointer-to-object pointing at the
 * same address.
 */
template <typename T>
void *FunctionPtr(T func)
{
  union {
    void *ptr;
    T func;
  } f;
  f.func = func;
  return f.ptr;
}

#endif /* Utils_h */
 
