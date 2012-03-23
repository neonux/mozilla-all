/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ObjectImpl_inl_h___
#define ObjectImpl_inl_h___

#include "mozilla/Assertions.h"

#include "jscell.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"

#include "js/TemplateLib.h"

#include "ObjectImpl.h"

namespace js {

static JS_ALWAYS_INLINE void
Debug_SetSlotRangeToCrashOnTouch(HeapSlot *vec, size_t len)
{
#ifdef DEBUG
    Debug_SetValueRangeToCrashOnTouch((Value *) vec, len);
#endif
}

static JS_ALWAYS_INLINE void
Debug_SetSlotRangeToCrashOnTouch(HeapSlot *begin, HeapSlot *end)
{
#ifdef DEBUG
    Debug_SetValueRangeToCrashOnTouch((Value *) begin, end - begin);
#endif
}

} // namespace js

inline bool
js::ObjectImpl::isExtensible() const
{
    return !lastProperty()->hasObjectFlag(BaseShape::NOT_EXTENSIBLE);
}

inline bool
js::ObjectImpl::isDenseArray() const
{
    bool result = hasClass(&ArrayClass);
    MOZ_ASSERT_IF(result, elements != emptyObjectElements);
    return result;
}

inline bool
js::ObjectImpl::isSlowArray() const
{
    bool result = hasClass(&SlowArrayClass);
    MOZ_ASSERT_IF(result, elements != emptyObjectElements);
    return result;
}

inline bool
js::ObjectImpl::isArray() const
{
    return isSlowArray() || isDenseArray();
}

inline uint32_t
js::ObjectImpl::getDenseArrayInitializedLength()
{
    MOZ_ASSERT(isDenseArray());
    return getElementsHeader()->initializedLength;
}

inline js::HeapSlotArray
js::ObjectImpl::getDenseArrayElements()
{
    MOZ_ASSERT(isDenseArray());
    return HeapSlotArray(elements);
}

inline const js::Value &
js::ObjectImpl::getDenseArrayElement(unsigned idx)
{
    MOZ_ASSERT(isDenseArray() && idx < getDenseArrayInitializedLength());
    return elements[idx];
}

inline void
js::ObjectImpl::getSlotRangeUnchecked(size_t start, size_t length,
                                      HeapSlot **fixedStart, HeapSlot **fixedEnd,
                                      HeapSlot **slotsStart, HeapSlot **slotsEnd)
{
    MOZ_ASSERT(!isDenseArray());
    MOZ_ASSERT(start + length >= start);

    size_t fixed = numFixedSlots();
    if (start < fixed) {
        if (start + length < fixed) {
            *fixedStart = &fixedSlots()[start];
            *fixedEnd = &fixedSlots()[start + length];
            *slotsStart = *slotsEnd = NULL;
        } else {
            size_t localCopy = fixed - start;
            *fixedStart = &fixedSlots()[start];
            *fixedEnd = &fixedSlots()[start + localCopy];
            *slotsStart = &slots[0];
            *slotsEnd = &slots[length - localCopy];
        }
    } else {
        *fixedStart = *fixedEnd = NULL;
        *slotsStart = &slots[start - fixed];
        *slotsEnd = &slots[start - fixed + length];
    }
}

inline void
js::ObjectImpl::getSlotRange(size_t start, size_t length,
                             HeapSlot **fixedStart, HeapSlot **fixedEnd,
                             HeapSlot **slotsStart, HeapSlot **slotsEnd)
{
    MOZ_ASSERT(slotInRange(start + length, SENTINEL_ALLOWED));
    getSlotRangeUnchecked(start, length, fixedStart, fixedEnd, slotsStart, slotsEnd);
}

inline bool
js::ObjectImpl::hasContiguousSlots(size_t start, size_t count) const
{
    /*
     * Check that the range [start, start+count) is either all inline or all
     * out of line.
     */
    MOZ_ASSERT(slotInRange(start + count, SENTINEL_ALLOWED));
    return start + count <= numFixedSlots() || start >= numFixedSlots();
}

inline void
js::ObjectImpl::invalidateSlotRange(size_t start, size_t length)
{
#ifdef DEBUG
    MOZ_ASSERT(!isDenseArray());

    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRange(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);
    Debug_SetSlotRangeToCrashOnTouch(fixedStart, fixedEnd);
    Debug_SetSlotRangeToCrashOnTouch(slotsStart, slotsEnd);
#endif /* DEBUG */
}

inline void
js::ObjectImpl::initializeSlotRange(size_t start, size_t length)
{
    /*
     * No bounds check, as this is used when the object's shape does not
     * reflect its allocated slots (updateSlotsForSpan).
     */
    HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
    getSlotRangeUnchecked(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);

    JSCompartment *comp = compartment();
    size_t offset = start;
    for (HeapSlot *sp = fixedStart; sp < fixedEnd; sp++)
        sp->init(comp, this->asObjectPtr(), offset++, UndefinedValue());
    for (HeapSlot *sp = slotsStart; sp < slotsEnd; sp++)
        sp->init(comp, this->asObjectPtr(), offset++, UndefinedValue());
}

inline bool
js::ObjectImpl::isNative() const
{
    return lastProperty()->isNative();
}

inline js::HeapSlot &
js::ObjectImpl::nativeGetSlotRef(unsigned slot)
{
    MOZ_ASSERT(isNative());
    MOZ_ASSERT(slot < slotSpan());
    return getSlotRef(slot);
}

inline const js::Value &
js::ObjectImpl::nativeGetSlot(unsigned slot) const
{
    MOZ_ASSERT(isNative());
    MOZ_ASSERT(slot < slotSpan());
    return getSlot(slot);
}

inline void
js::ObjectImpl::setSlot(unsigned slot, const js::Value &value)
{
    MOZ_ASSERT(slotInRange(slot));
    getSlotRef(slot).set(this->asObjectPtr(), slot, value);
}

inline void
js::ObjectImpl::initSlot(unsigned slot, const js::Value &value)
{
    MOZ_ASSERT(getSlot(slot).isUndefined() || getSlot(slot).isMagic(JS_ARRAY_HOLE));
    MOZ_ASSERT(slotInRange(slot));
    initSlotUnchecked(slot, value);
}

inline void
js::ObjectImpl::initSlotUnchecked(unsigned slot, const js::Value &value)
{
    getSlotAddressUnchecked(slot)->init(this->asObjectPtr(), slot, value);
}

inline void
js::ObjectImpl::setFixedSlot(unsigned slot, const js::Value &value)
{
    MOZ_ASSERT(slot < numFixedSlots());
    fixedSlots()[slot].set(this->asObjectPtr(), slot, value);
}

inline void
js::ObjectImpl::initFixedSlot(unsigned slot, const js::Value &value)
{
    MOZ_ASSERT(slot < numFixedSlots());
    fixedSlots()[slot].init(this->asObjectPtr(), slot, value);
}

inline uint32_t
js::ObjectImpl::slotSpan() const
{
    if (inDictionaryMode())
        return lastProperty()->base()->slotSpan();
    return lastProperty()->slotSpan();
}

inline size_t
js::ObjectImpl::numDynamicSlots() const
{
    return dynamicSlotsCount(numFixedSlots(), slotSpan());
}

inline js::Class *
js::ObjectImpl::getClass() const
{
    return lastProperty()->getObjectClass();
}

inline JSClass *
js::ObjectImpl::getJSClass() const
{
    return Jsvalify(getClass());
}

inline bool
js::ObjectImpl::hasClass(const Class *c) const
{
    return getClass() == c;
}

inline const js::ObjectOps *
js::ObjectImpl::getOps() const
{
    return &getClass()->ops;
}

inline bool
js::ObjectImpl::isDelegate() const
{
    return lastProperty()->hasObjectFlag(BaseShape::DELEGATE);
}

inline bool
js::ObjectImpl::inDictionaryMode() const
{
    return lastProperty()->inDictionary();
}

/* static */ inline size_t
js::ObjectImpl::dynamicSlotsCount(size_t nfixed, size_t span)
{
    if (span <= nfixed)
        return 0;
    span -= nfixed;
    if (span <= SLOT_CAPACITY_MIN)
        return SLOT_CAPACITY_MIN;

    size_t slots = RoundUpPow2(span);
    MOZ_ASSERT(slots >= span);
    return slots;
}

inline size_t
js::ObjectImpl::sizeOfThis() const
{
    return arenaHeader()->getThingSize();
}

/* static */ inline void
js::ObjectImpl::readBarrier(ObjectImpl *obj)
{
#ifdef JSGC_INCREMENTAL
    JSCompartment *comp = obj->compartment();
    if (comp->needsBarrier()) {
        MOZ_ASSERT(!comp->rt->gcRunning);
        JSObject *tmp = obj->asObjectPtr();
        MarkObjectUnbarriered(comp->barrierTracer(), &tmp, "read barrier");
        JS_ASSERT(tmp == obj->asObjectPtr());
    }
#endif
}

inline void
js::ObjectImpl::privateWriteBarrierPre(void **old)
{
#ifdef JSGC_INCREMENTAL
    JSCompartment *comp = compartment();
    if (comp->needsBarrier()) {
        if (*old && getClass()->trace)
            getClass()->trace(comp->barrierTracer(), this->asObjectPtr());
    }
#endif
}

inline void
js::ObjectImpl::privateWriteBarrierPost(void **old)
{
}

/* static */ inline void
js::ObjectImpl::writeBarrierPre(ObjectImpl *obj)
{
#ifdef JSGC_INCREMENTAL
    /*
     * This would normally be a null test, but TypeScript::global uses 0x1 as a
     * special value.
     */
    if (uintptr_t(obj) < 32)
        return;

    JSCompartment *comp = obj->compartment();
    if (comp->needsBarrier()) {
        MOZ_ASSERT(!comp->rt->gcRunning);
        JSObject *tmp = obj->asObjectPtr();
        MarkObjectUnbarriered(comp->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == obj->asObjectPtr());
    }
#endif
}

/* static */ inline void
js::ObjectImpl::writeBarrierPost(ObjectImpl *obj, void *addr)
{
}

#endif /* ObjectImpl_inl_h__ */
