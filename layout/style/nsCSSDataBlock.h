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
 * The Original Code is nsCSSDataBlock.h.
 *
 * The Initial Developer of the Original Code is L. David Baron.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
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
 * compact representation of the property-value pairs within a CSS
 * declaration, and the code for expanding and compacting it
 */

#ifndef nsCSSDataBlock_h__
#define nsCSSDataBlock_h__

#include "nsCSSStruct.h"
#include "nsCSSProps.h"
#include "nsCSSPropertySet.h"
#include "nsAutoPtr.h"

struct nsRuleData;
class nsCSSExpandedDataBlock;

namespace mozilla {
namespace css {
class Declaration;
}
}

/**
 * An |nsCSSCompressedDataBlock| holds a usually-immutable chunk of
 * property-value data for a CSS declaration block (which we misname a
 * |css::Declaration|).  Mutation is accomplished through
 * |nsCSSExpandedDataBlock| or in some cases via direct slot access.
 *
 * Mutation is forbidden when the reference count is greater than one,
 * since once a style rule has used a compressed data block, mutation of
 * that block is forbidden, and any declarations that want to mutate it
 * need to clone it first.
 */
class nsCSSCompressedDataBlock {
public:
    friend class nsCSSExpandedDataBlock;
    friend class mozilla::css::Declaration;

    /**
     * Do what |nsIStyleRule::MapRuleInfoInto| needs to do for a style
     * rule using this block for storage.
     */
    void MapRuleInfoInto(nsRuleData *aRuleData) const;

    /**
     * Return the location at which the *value* for the property is
     * stored, or null if the block does not contain a value for the
     * property.  This is either an |nsCSSValue*|, |nsCSSRect*|, or an
     * |nsCSSValueList**|, etc.
     *
     * Inefficient (by design).
     *
     * Must not be called for shorthands.
     */
    const void* StorageFor(nsCSSProperty aProperty) const;

    /**
     * A set of slightly more typesafe helpers for the above.  All
     * return null if the value is not present.
     */
    const nsCSSValue* ValueStorageFor(nsCSSProperty aProperty) const {
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] == eCSSType_Value,
                        "type mismatch");
      return static_cast<const nsCSSValue*>(StorageFor(aProperty));
    }
    const nsCSSRect* RectStorageFor(nsCSSProperty aProperty) const {
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] == eCSSType_Rect,
                        "type mismatch");
      return static_cast<const nsCSSRect*>(StorageFor(aProperty));
    }
    const nsCSSValuePair* ValuePairStorageFor(nsCSSProperty aProperty) const {
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] ==
                          eCSSType_ValuePair,
                        "type mismatch");
      return static_cast<const nsCSSValuePair*>(StorageFor(aProperty));
    }
    const nsCSSValueList*const*
    ValueListStorageFor(nsCSSProperty aProperty) const {
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] ==
                          eCSSType_ValueList,
                        "type mismatch");
      return static_cast<const nsCSSValueList*const*>(StorageFor(aProperty));
    }
    const nsCSSValuePairList*const*
    ValuePairListStorageFor(nsCSSProperty aProperty) const {
      NS_ABORT_IF_FALSE(nsCSSProps::kTypeTable[aProperty] ==
                          eCSSType_ValuePairList,
                        "type mismatch");
      return static_cast<const nsCSSValuePairList*const*>(
               StorageFor(aProperty));
    }

    /**
     * Clone this block, or return null on out-of-memory.
     */
    already_AddRefed<nsCSSCompressedDataBlock> Clone() const;

    /**
     * Create a new nsCSSCompressedDataBlock holding no declarations.
     */
    static already_AddRefed<nsCSSCompressedDataBlock> CreateEmptyBlock();

    void AddRef() {
        NS_ASSERTION(mRefCnt == 0 || mRefCnt == 1,
                     "unexpected reference count");
        ++mRefCnt;
    }
    void Release() {
        NS_ASSERTION(mRefCnt == 1 || mRefCnt == 2,
                     "unexpected reference count");
        if (--mRefCnt == 0) {
            Destroy();
        }
    }

    PRBool IsMutable() const {
        NS_ASSERTION(mRefCnt == 1 || mRefCnt == 2,
                     "unexpected reference count");
        return mRefCnt < 2;
    }

private:
    PRInt32 mStyleBits; // the structs for which we have data, according to
                        // |nsCachedStyleData::GetBitForSID|.
    nsAutoRefCnt mRefCnt;

    enum { block_chars = 4 }; // put 4 chars in the definition of the class
                              // to ensure size not inflated by alignment

    void* operator new(size_t aBaseSize, size_t aDataSize) {
        // subtract off the extra size to store |mBlock_|
        return ::operator new(aBaseSize + aDataSize -
                              sizeof(char) * block_chars);
    }

    nsCSSCompressedDataBlock() : mStyleBits(0) {}

    // Only this class (through |Destroy|) or nsCSSExpandedDataBlock (in
    // |Expand|) can delete compressed data blocks.
    ~nsCSSCompressedDataBlock() { }

    /**
     * Delete all the data stored in this block, and the block itself.
     */
    void Destroy();

    char* mBlockEnd; // the byte after the last valid byte
    char mBlock_[block_chars]; // must be the last member!

    char* Block() { return mBlock_; }
    char* BlockEnd() { return mBlockEnd; }
    const char* Block() const { return mBlock_; }
    const char* BlockEnd() const { return mBlockEnd; }
    ptrdiff_t DataSize() const { return BlockEnd() - Block(); }

    // Direct slot access to our values.  See StorageFor above.  Can
    // return null.  Must not be called for shorthand properties.
    void* SlotForValue(nsCSSProperty aProperty) {
      NS_ABORT_IF_FALSE(IsMutable(), "must be mutable");
      return const_cast<void*>(StorageFor(aProperty));
    }
};

class nsCSSExpandedDataBlock {
public:
    nsCSSExpandedDataBlock();
    ~nsCSSExpandedDataBlock();
    /*
     * When setting properties in an |nsCSSExpandedDataBlock|, callers
     * must make the appropriate |AddPropertyBit| call.
     */

    nsCSSFont mFont;
    nsCSSDisplay mDisplay;
    nsCSSMargin mMargin;
    nsCSSList mList;
    nsCSSPosition mPosition;
    nsCSSTable mTable;
    nsCSSColor mColor;
    nsCSSContent mContent;
    nsCSSText mText;
    nsCSSUserInterface mUserInterface;
    nsCSSAural mAural;
    nsCSSPage mPage;
    nsCSSBreaks mBreaks;
    nsCSSXUL mXUL;
    nsCSSSVG mSVG;
    nsCSSColumn mColumn;

    /**
     * Transfer all of the state from the compressed block to this
     * expanded block.  The state of this expanded block must be clear
     * beforehand.
     *
     * The compressed block passed in IS RELEASED by this method and
     * set to null, and thus cannot be used again.  (This is necessary
     * because ownership of sub-objects is transferred to the expanded
     * block in many cases.)
     */
    void Expand(nsRefPtr<nsCSSCompressedDataBlock> *aNormalBlock,
                nsRefPtr<nsCSSCompressedDataBlock> *aImportantBlock);

    /**
     * Allocate a new compressed block and transfer all of the state
     * from this expanded block to the new compressed block, clearing
     * the state of this expanded block.
     */
    void Compress(nsCSSCompressedDataBlock **aNormalBlock,
                  nsCSSCompressedDataBlock **aImportantBlock);

    /**
     * Clear (and thus destroy) the state of this expanded block.
     */
    void Clear();

    /**
     * Clear the data for the given property (including the set and
     * important bits).  Can be used with shorthand properties.
     */
    void ClearProperty(nsCSSProperty aPropID);

    /**
     * Same as ClearProperty, but faster and cannot be used with shorthands.
     */
    void ClearLonghandProperty(nsCSSProperty aPropID);

    void AssertInitialState() {
#ifdef DEBUG
        DoAssertInitialState();
#endif
    }

private:
    /**
     * Compute the size that will be occupied by the result of
     * |Compress|.
     */
    struct ComputeSizeResult {
        PRUint32 normal, important;
    };
    ComputeSizeResult ComputeSize();

    void DoExpand(nsRefPtr<nsCSSCompressedDataBlock> *aBlock,
                  PRBool aImportant);
#ifdef DEBUG
    void DoAssertInitialState();
#endif

    // XXX These could probably be pointer-to-member, if the casting can
    // be done correctly.
    static const size_t kOffsetTable[];

    /*
     * mPropertiesSet stores a bit for every property that is present,
     * to optimize compression of blocks with small numbers of
     * properties (the norm) and to allow quickly checking whether a
     * property is set in this block.
     */
    nsCSSPropertySet mPropertiesSet;
    /*
     * mPropertiesImportant indicates which properties are '!important'.
     */
    nsCSSPropertySet mPropertiesImportant;

public:
    /*
     * Return the storage location within |this| of the value of the
     * property (i.e., either an |nsCSSValue*|, |nsCSSRect*|, or
     * |nsCSSValueList**| (etc.).
     */
    void* PropertyAt(nsCSSProperty aProperty) {
        size_t offset = nsCSSExpandedDataBlock::kOffsetTable[aProperty];
        return reinterpret_cast<void*>(reinterpret_cast<char*>(this) + offset);
    }

    void SetPropertyBit(nsCSSProperty aProperty) {
        mPropertiesSet.AddProperty(aProperty);
    }

    void ClearPropertyBit(nsCSSProperty aProperty) {
        mPropertiesSet.RemoveProperty(aProperty);
    }

    PRBool HasPropertyBit(nsCSSProperty aProperty) {
        return mPropertiesSet.HasProperty(aProperty);
    }

    void SetImportantBit(nsCSSProperty aProperty) {
        mPropertiesImportant.AddProperty(aProperty);
    }

    void ClearImportantBit(nsCSSProperty aProperty) {
        mPropertiesImportant.RemoveProperty(aProperty);
    }

    PRBool HasImportantBit(nsCSSProperty aProperty) {
        return mPropertiesImportant.HasProperty(aProperty);
    }

    void ClearSets() {
        mPropertiesSet.Empty();
        mPropertiesImportant.Empty();
    }
};

#endif /* !defined(nsCSSDataBlock_h__) */
