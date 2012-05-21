/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicharUtils.h"
#include "nsUnicharUtilCIID.h"

#include "nsCRT.h"
#include "nsICaseConversion.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMStrings.h"
#include "nsUTF8Utils.h"
#include "nsUnicodeProperties.h"
#include "nsHashKeys.h"

// We map x -> x, except for upper-case letters,
// which we map to their lower-case equivalents.
static const PRUint8 gASCIIToLower [128] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
};

#define IS_ASCII(u)       ((u) < 0x80)
#define IS_ASCII_UPPER(u) (('A' <= (u)) && ((u) <= 'Z'))
#define IS_ASCII_LOWER(u) (('a' <= (u)) && ((u) <= 'z'))
#define IS_ASCII_ALPHA(u) (IS_ASCII_UPPER(u) || IS_ASCII_LOWER(u))
#define IS_ASCII_SPACE(u) (' ' == (u))

// We want ToLowerCase(PRUint32) and ToLowerCaseASCII(PRUint32) to be fast
// when they're called from within the case-insensitive comparators, so we
// define inlined versions.
static NS_ALWAYS_INLINE PRUint32
ToLowerCase_inline(PRUint32 aChar)
{
  if (IS_ASCII(aChar)) {
    return gASCIIToLower[aChar];
  }

  return mozilla::unicode::GetLowercase(aChar);
}

static NS_ALWAYS_INLINE PRUint32
ToLowerCaseASCII_inline(const PRUint32 aChar)
{
  if (IS_ASCII(aChar)) {
    return gASCIIToLower[aChar];
  }

  return aChar;
}

void
ToLowerCase(nsAString& aString)
{
  PRUnichar *buf = aString.BeginWriting();
  ToLowerCase(buf, buf, aString.Length());
}

void
ToLowerCase(const nsAString& aSource,
            nsAString& aDest)
{
  const PRUnichar *in;
  PRUnichar *out;
  PRUint32 len = NS_StringGetData(aSource, &in);
  NS_StringGetMutableData(aDest, len, &out);
  NS_ASSERTION(out, "Uh...");
  ToLowerCase(in, out, len);
}

PRUint32
ToLowerCaseASCII(const PRUint32 aChar)
{
  return ToLowerCaseASCII_inline(aChar);
}

void
ToUpperCase(nsAString& aString)
{
  PRUnichar *buf = aString.BeginWriting();
  ToUpperCase(buf, buf, aString.Length());
}

void
ToUpperCase(const nsAString& aSource,
            nsAString& aDest)
{
  const PRUnichar *in;
  PRUnichar *out;
  PRUint32 len = NS_StringGetData(aSource, &in);
  NS_StringGetMutableData(aDest, len, &out);
  NS_ASSERTION(out, "Uh...");
  ToUpperCase(in, out, len);
}

#ifdef MOZILLA_INTERNAL_API

PRInt32
nsCaseInsensitiveStringComparator::operator()(const PRUnichar* lhs,
                                              const PRUnichar* rhs,
                                              PRUint32 lLength,
                                              PRUint32 rLength) const
{
  return (lLength == rLength) ? CaseInsensitiveCompare(lhs, rhs, lLength) :
         (lLength > rLength) ? 1 : -1;
}

PRInt32
nsCaseInsensitiveUTF8StringComparator::operator()(const char* lhs,
                                                  const char* rhs,
                                                  PRUint32 lLength,
                                                  PRUint32 rLength) const
{
  return CaseInsensitiveCompare(lhs, rhs, lLength, rLength);
}

PRInt32
nsASCIICaseInsensitiveStringComparator::operator()(const PRUnichar* lhs,
                                                   const PRUnichar* rhs,
                                                   PRUint32 lLength,
                                                   PRUint32 rLength) const
{
  if (lLength != rLength) {
    if (lLength > rLength)
      return 1;
    return -1;
  }

  while (rLength) {
    // we don't care about surrogates here, because we're only
    // lowercasing the ASCII range
    PRUnichar l = *lhs++;
    PRUnichar r = *rhs++;
    if (l != r) {
      l = ToLowerCaseASCII_inline(l);
      r = ToLowerCaseASCII_inline(r);

      if (l > r)
        return 1;
      else if (r > l)
        return -1;
    }
    rLength--;
  }

  return 0;
}

#endif // MOZILLA_INTERNAL_API

PRUint32
ToLowerCase(PRUint32 aChar)
{
  return ToLowerCase_inline(aChar);
}

void
ToLowerCase(const PRUnichar *aIn, PRUnichar *aOut, PRUint32 aLen)
{
  for (PRUint32 i = 0; i < aLen; i++) {
    PRUint32 ch = aIn[i];
    if (NS_IS_HIGH_SURROGATE(ch) && i < aLen - 1 &&
        NS_IS_LOW_SURROGATE(aIn[i + 1])) {
      ch = mozilla::unicode::GetLowercase(SURROGATE_TO_UCS4(ch, aIn[i + 1]));
      NS_ASSERTION(!IS_IN_BMP(ch), "case mapping crossed BMP/SMP boundary!");
      aOut[i++] = H_SURROGATE(ch);
      aOut[i] = L_SURROGATE(ch);
      continue;
    }
    aOut[i] = ToLowerCase(ch);
  }
}

PRUint32
ToUpperCase(PRUint32 aChar)
{
  if (IS_ASCII(aChar)) {
    if (IS_ASCII_LOWER(aChar)) {
      return aChar - 0x20;
    }
    return aChar;
  }

  return mozilla::unicode::GetUppercase(aChar);
}

void
ToUpperCase(const PRUnichar *aIn, PRUnichar *aOut, PRUint32 aLen)
{
  for (PRUint32 i = 0; i < aLen; i++) {
    PRUint32 ch = aIn[i];
    if (NS_IS_HIGH_SURROGATE(ch) && i < aLen - 1 &&
        NS_IS_LOW_SURROGATE(aIn[i + 1])) {
      ch = mozilla::unicode::GetUppercase(SURROGATE_TO_UCS4(ch, aIn[i + 1]));
      NS_ASSERTION(!IS_IN_BMP(ch), "case mapping crossed BMP/SMP boundary!");
      aOut[i++] = H_SURROGATE(ch);
      aOut[i] = L_SURROGATE(ch);
      continue;
    }
    aOut[i] = ToUpperCase(ch);
  }
}

PRUint32
ToTitleCase(PRUint32 aChar)
{
  if (IS_ASCII(aChar)) {
    return ToUpperCase(aChar);
  }

  return mozilla::unicode::GetTitlecaseForLower(aChar);
}

PRInt32
CaseInsensitiveCompare(const PRUnichar *a,
                       const PRUnichar *b,
                       PRUint32 len)
{
  NS_ASSERTION(a && b, "Do not pass in invalid pointers!");

  if (len) {
    do {
      PRUint32 c1 = *a++;
      PRUint32 c2 = *b++;

      // Unfortunately, we need to check for surrogates BEFORE we check
      // for equality, because we could have identical high surrogates
      // but non-identical characters, so we can't just skip them

      // If c1 isn't a surrogate, we don't bother to check c2;
      // in the case where it _is_ a surrogate, we're definitely going to get
      // a mismatch, and don't need to interpret and lowercase it

      if (NS_IS_HIGH_SURROGATE(c1) && len > 1 && NS_IS_LOW_SURROGATE(*a)) {
        c1 = SURROGATE_TO_UCS4(c1, *a++);
        if (NS_IS_HIGH_SURROGATE(c2) && NS_IS_LOW_SURROGATE(*b)) {
          c2 = SURROGATE_TO_UCS4(c2, *b++);
        }
        // If c2 wasn't a surrogate, decrementing len means we'd stop
        // short of the end of string b, but that doesn't actually matter
        // because we're going to find a mismatch and return early
        --len;
      }

      if (c1 != c2) {
        c1 = ToLowerCase_inline(c1);
        c2 = ToLowerCase_inline(c2);
        if (c1 != c2) {
          if (c1 < c2) {
            return -1;
          }
          return 1;
        }
      }
    } while (--len != 0);
  }
  return 0;
}

// Calculates the codepoint of the UTF8 sequence starting at aStr.  Sets aNext
// to the byte following the end of the sequence.
//
// If the sequence is invalid, or if computing the codepoint would take us off
// the end of the string (as marked by aEnd), returns -1 and does not set
// aNext.  Note that this function doesn't check that aStr < aEnd -- it assumes
// you've done that already.
static NS_ALWAYS_INLINE PRUint32
GetLowerUTF8Codepoint(const char* aStr, const char* aEnd, const char **aNext)
{
  // Convert to unsigned char so that stuffing chars into PRUint32s doesn't
  // sign extend.
  const unsigned char *str = (unsigned char*)aStr;

  if (UTF8traits::isASCII(str[0])) {
    // It's ASCII; just convert to lower-case and return it.
    *aNext = aStr + 1;
    return gASCIIToLower[*str];
  }
  if (UTF8traits::is2byte(str[0]) && NS_LIKELY(aStr + 1 < aEnd)) {
    // It's a two-byte sequence, so it looks like
    //  110XXXXX 10XXXXXX.
    // This is definitely in the BMP, so we can store straightaway into a
    // PRUint16.

    PRUint16 c;
    c  = (str[0] & 0x1F) << 6;
    c += (str[1] & 0x3F);

    // we don't go through ToLowerCase here, because we know this isn't
    // an ASCII character so the ASCII fast-path there is useless
    c = mozilla::unicode::GetLowercase(c);

    *aNext = aStr + 2;
    return c;
  }
  if (UTF8traits::is3byte(str[0]) && NS_LIKELY(aStr + 2 < aEnd)) {
    // It's a three-byte sequence, so it looks like
    //  1110XXXX 10XXXXXX 10XXXXXX.
    // This will just barely fit into 16-bits, so store into a PRUint16.

    PRUint16 c;
    c  = (str[0] & 0x0F) << 12;
    c += (str[1] & 0x3F) << 6;
    c += (str[2] & 0x3F);

    c = mozilla::unicode::GetLowercase(c);

    *aNext = aStr + 3;
    return c;
  }
  if (UTF8traits::is4byte(str[0]) && NS_LIKELY(aStr + 3 < aEnd)) {
    // It's a four-byte sequence, so it looks like
    //   11110XXX 10XXXXXX 10XXXXXX 10XXXXXX.

    PRUint32 c;
    c  = (str[0] & 0x07) << 18;
    c += (str[1] & 0x3F) << 12;
    c += (str[2] & 0x3F) << 6;
    c += (str[3] & 0x3F);

    c = mozilla::unicode::GetLowercase(c);

    *aNext = aStr + 4;
    return c;
  }

  // Hm, we don't understand this sequence.
  return -1;
}

PRInt32 CaseInsensitiveCompare(const char *aLeft,
                               const char *aRight,
                               PRUint32 aLeftBytes,
                               PRUint32 aRightBytes)
{
  const char *leftEnd = aLeft + aLeftBytes;
  const char *rightEnd = aRight + aRightBytes;

  while (aLeft < leftEnd && aRight < rightEnd) {
    PRUint32 leftChar = GetLowerUTF8Codepoint(aLeft, leftEnd, &aLeft);
    if (NS_UNLIKELY(leftChar == PRUint32(-1)))
      return -1;

    PRUint32 rightChar = GetLowerUTF8Codepoint(aRight, rightEnd, &aRight);
    if (NS_UNLIKELY(rightChar == PRUint32(-1)))
      return -1;

    // Now leftChar and rightChar are lower-case, so we can compare them.
    if (leftChar != rightChar) {
      if (leftChar > rightChar)
        return 1;
      return -1;
    }
  }

  // Make sure that if one string is longer than the other we return the
  // correct result.
  if (aLeft < leftEnd)
    return 1;
  if (aRight < rightEnd)
    return -1;

  return 0;
}

bool
CaseInsensitiveUTF8CharsEqual(const char* aLeft, const char* aRight,
                              const char* aLeftEnd, const char* aRightEnd,
                              const char** aLeftNext, const char** aRightNext,
                              bool* aErr)
{
  NS_ASSERTION(aLeftNext, "Out pointer shouldn't be null.");
  NS_ASSERTION(aRightNext, "Out pointer shouldn't be null.");
  NS_ASSERTION(aErr, "Out pointer shouldn't be null.");
  NS_ASSERTION(aLeft < aLeftEnd, "aLeft must be less than aLeftEnd.");
  NS_ASSERTION(aRight < aRightEnd, "aRight must be less than aRightEnd.");

  PRUint32 leftChar = GetLowerUTF8Codepoint(aLeft, aLeftEnd, aLeftNext);
  if (NS_UNLIKELY(leftChar == PRUint32(-1))) {
    *aErr = true;
    return false;
  }

  PRUint32 rightChar = GetLowerUTF8Codepoint(aRight, aRightEnd, aRightNext);
  if (NS_UNLIKELY(rightChar == PRUint32(-1))) {
    *aErr = true;
    return false;
  }

  // Can't have an error past this point.
  *aErr = false;

  return leftChar == rightChar;
}

namespace mozilla {

PRUint32
HashUTF8AsUTF16(const char* aUTF8, PRUint32 aLength, bool* aErr)
{
  PRUint32 hash = 0;
  const char* s = aUTF8;
  const char* end = aUTF8 + aLength;

  *aErr = false;

  while (s < end)
  {
    PRUint32 ucs4 = UTF8CharEnumerator::NextChar(&s, end, aErr);
    if (*aErr) {
      return 0;
    }

    if (ucs4 < PLANE1_BASE) {
      hash = AddToHash(hash, ucs4);
    }
    else {
      hash = AddToHash(hash, H_SURROGATE(ucs4), L_SURROGATE(ucs4));
    }
  }

  return hash;
}

} // namespace mozilla
