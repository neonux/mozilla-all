/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
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
 * The Original Code is the Mozilla SpiderMonkey bytecode type inference
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Hackett <bhackett@mozilla.com>
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

/* Inline members for javascript type inference. */

#include "jsarray.h"
#include "jsanalyze.h"
#include "jscompartment.h"
#include "jsinfer.h"
#include "jsprf.h"
#include "vm/GlobalObject.h"

#include "vm/Stack-inl.h"

#ifndef jsinferinlines_h___
#define jsinferinlines_h___

/////////////////////////////////////////////////////////////////////
// Types
/////////////////////////////////////////////////////////////////////

namespace js {
namespace types {

/* static */ inline Type
Type::ObjectType(JSObject *obj)
{
    if (obj->hasSingletonType())
        return Type((jsuword) obj | 1);
    return Type((jsuword) obj->type());
}

/* static */ inline Type
Type::ObjectType(TypeObject *obj)
{
    if (obj->singleton)
        return Type((jsuword) obj->singleton | 1);
    return Type((jsuword) obj);
}

/* static */ inline Type
Type::ObjectType(TypeObjectKey *obj)
{
    return Type((jsuword) obj);
}

inline Type
GetValueType(JSContext *cx, const Value &val)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (val.isDouble())
        return Type::DoubleType();
    if (val.isObject())
        return Type::ObjectType(&val.toObject());
    return Type::PrimitiveType(val.extractNonDoubleType());
}

inline TypeFlags
PrimitiveTypeFlag(JSValueType type)
{
    switch (type) {
      case JSVAL_TYPE_UNDEFINED:
        return TYPE_FLAG_UNDEFINED;
      case JSVAL_TYPE_NULL:
        return TYPE_FLAG_NULL;
      case JSVAL_TYPE_BOOLEAN:
        return TYPE_FLAG_BOOLEAN;
      case JSVAL_TYPE_INT32:
        return TYPE_FLAG_INT32;
      case JSVAL_TYPE_DOUBLE:
        return TYPE_FLAG_DOUBLE;
      case JSVAL_TYPE_STRING:
        return TYPE_FLAG_STRING;
      case JSVAL_TYPE_MAGIC:
        return TYPE_FLAG_LAZYARGS;
      default:
        JS_NOT_REACHED("Bad type");
        return 0;
    }
}

inline JSValueType
TypeFlagPrimitive(TypeFlags flags)
{
    switch (flags) {
      case TYPE_FLAG_UNDEFINED:
        return JSVAL_TYPE_UNDEFINED;
      case TYPE_FLAG_NULL:
        return JSVAL_TYPE_NULL;
      case TYPE_FLAG_BOOLEAN:
        return JSVAL_TYPE_BOOLEAN;
      case TYPE_FLAG_INT32:
        return JSVAL_TYPE_INT32;
      case TYPE_FLAG_DOUBLE:
        return JSVAL_TYPE_DOUBLE;
      case TYPE_FLAG_STRING:
        return JSVAL_TYPE_STRING;
      case TYPE_FLAG_LAZYARGS:
        return JSVAL_TYPE_MAGIC;
      default:
        JS_NOT_REACHED("Bad type");
        return (JSValueType) 0;
    }
}

/*
 * Get the canonical representation of an id to use when doing inference.  This
 * maintains the constraint that if two different jsids map to the same property
 * in JS (e.g. 3 and "3"), they have the same type representation.
 */
inline jsid
MakeTypeId(JSContext *cx, jsid id)
{
    JS_ASSERT(!JSID_IS_EMPTY(id));

    /*
     * All integers must map to the aggregate property for index types, including
     * negative integers.
     */
    if (JSID_IS_INT(id))
        return JSID_VOID;

    /*
     * Check for numeric strings, as in js_StringIsIndex, but allow negative
     * and overflowing integers.
     */
    if (JSID_IS_STRING(id)) {
        JSFlatString *str = JSID_TO_FLAT_STRING(id);
        const jschar *cp = str->getCharsZ(cx);
        if (JS7_ISDEC(*cp) || *cp == '-') {
            cp++;
            while (JS7_ISDEC(*cp))
                cp++;
            if (*cp == 0)
                return JSID_VOID;
        }
        return id;
    }

    return JSID_VOID;
}

const char * TypeIdStringImpl(jsid id);

/* Convert an id for printing during debug. */
static inline const char *
TypeIdString(jsid id)
{
#ifdef DEBUG
    return TypeIdStringImpl(id);
#else
    return "(missing)";
#endif
}

/*
 * Structure for type inference entry point functions. All functions which can
 * change type information must use this, and functions which depend on
 * intermediate types (i.e. JITs) can use this to ensure that intermediate
 * information is not collected and does not change.
 *
 * Pins inference results so that intermediate type information, TypeObjects
 * and JSScripts won't be collected during GC. Does additional sanity checking
 * that inference is not reentrant and that recompilations occur properly.
 */
struct AutoEnterTypeInference
{
    JSContext *cx;
    bool oldActiveAnalysis;
    bool oldActiveInference;

    AutoEnterTypeInference(JSContext *cx, bool compiling = false)
        : cx(cx), oldActiveAnalysis(cx->compartment->activeAnalysis),
          oldActiveInference(cx->compartment->activeInference)
    {
        JS_ASSERT_IF(!compiling, cx->compartment->types.inferenceEnabled);
        cx->compartment->activeAnalysis = true;
        cx->compartment->activeInference = true;
    }

    ~AutoEnterTypeInference()
    {
        cx->compartment->activeAnalysis = oldActiveAnalysis;
        cx->compartment->activeInference = oldActiveInference;

        /*
         * If there are no more type inference activations on the stack,
         * process any triggered recompilations. Note that we should not be
         * invoking any scripted code while type inference is running.
         * :TODO: assert this.
         */
        if (!cx->compartment->activeInference) {
            TypeCompartment *types = &cx->compartment->types;
            if (types->pendingNukeTypes)
                types->nukeTypes(cx);
            else if (types->pendingRecompiles)
                types->processPendingRecompiles(cx);
        }
    }
};

/*
 * Structure marking the currently compiled script, for constraints which can
 * trigger recompilation.
 */
struct AutoEnterCompilation
{
    JSContext *cx;
    JSScript *script;

    AutoEnterCompilation(JSContext *cx, JSScript *script)
        : cx(cx), script(script)
    {
        JS_ASSERT(!cx->compartment->types.compiledScript);
        cx->compartment->types.compiledScript = script;
    }

    ~AutoEnterCompilation()
    {
        JS_ASSERT(cx->compartment->types.compiledScript == script);
        cx->compartment->types.compiledScript = NULL;
    }
};

/////////////////////////////////////////////////////////////////////
// Interface functions
/////////////////////////////////////////////////////////////////////

/*
 * These functions check whether inference is enabled before performing some
 * action on the type state. To avoid checking cx->typeInferenceEnabled()
 * everywhere, it is generally preferred to use one of these functions or
 * a type function on JSScript to perform inference operations.
 */

/*
 * Get the default 'new' object for a given standard class, per the currently
 * active global.
 */
inline TypeObject *
GetTypeNewObject(JSContext *cx, JSProtoKey key)
{
    JSObject *proto;
    if (!js_GetClassPrototype(cx, NULL, key, &proto, NULL))
        return NULL;
    return proto->getNewType(cx);
}

/* Get a type object for the immediate allocation site within a native. */
inline TypeObject *
GetTypeCallerInitObject(JSContext *cx, JSProtoKey key)
{
    if (cx->typeInferenceEnabled()) {
        jsbytecode *pc;
        JSScript *script = cx->stack.currentScript(&pc);
        if (script && script->compartment == cx->compartment)
            return script->types.initObject(cx, pc, key);
    }
    return GetTypeNewObject(cx, key);
}

/*
 * When using a custom iterator within the initialization of a 'for in' loop,
 * mark the iterator values as unknown.
 */
inline void
MarkIteratorUnknown(JSContext *cx)
{
    extern void MarkIteratorUnknownSlow(JSContext *cx);

    if (cx->typeInferenceEnabled())
        MarkIteratorUnknownSlow(cx);
}

/*
 * Monitor a javascript call, either on entry to the interpreter or made
 * from within the interpreter.
 */
inline void
TypeMonitorCall(JSContext *cx, const js::CallArgs &args, bool constructing)
{
    extern void TypeMonitorCallSlow(JSContext *cx, JSObject *callee,
                                    const CallArgs &args, bool constructing);

    if (cx->typeInferenceEnabled()) {
        JSObject *callee = &args.callee();
        if (callee->isFunction() && callee->getFunctionPrivate()->isInterpreted())
            TypeMonitorCallSlow(cx, callee, args, constructing);
    }
}

inline bool
TrackPropertyTypes(JSContext *cx, JSObject *obj, jsid id)
{
    return cx->typeInferenceEnabled()
        && !obj->hasLazyType()
        && !obj->type()->unknownProperties();
}

/* Add a possible type for a property of obj. */
inline void
AddTypePropertyId(JSContext *cx, JSObject *obj, jsid id, Type type)
{
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->addPropertyType(cx, id, type);
}

inline void
AddTypePropertyId(JSContext *cx, JSObject *obj, jsid id, const Value &value)
{
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->addPropertyType(cx, id, value);
}

inline void
AddTypeProperty(JSContext *cx, TypeObject *obj, const char *name, Type type)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->addPropertyType(cx, name, type);
}

inline void
AddTypeProperty(JSContext *cx, TypeObject *obj, const char *name, const Value &value)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->addPropertyType(cx, name, value);
}

/* Get the default type object to use for objects with no prototype. */
inline TypeObject *
GetTypeEmpty(JSContext *cx)
{
    return &cx->compartment->types.typeEmpty;
}

/* Alias two properties in the type information for obj. */
inline void
AliasTypeProperties(JSContext *cx, JSObject *obj, jsid first, jsid second)
{
    if (TrackPropertyTypes(cx, obj, first) || TrackPropertyTypes(cx, obj, second))
        obj->type()->aliasProperties(cx, first, second);
}

/* Set one or more dynamic flags on a type object. */
inline void
MarkTypeObjectFlags(JSContext *cx, JSObject *obj, TypeObjectFlags flags)
{
    /*
     * Instantiate a lazy type now if we are setting flags on it which cannot
     * be recovered later.
     */
    if (TrackPropertyTypes(cx, obj, JSID_EMPTY) ||
        (cx->typeInferenceEnabled() && !(flags & OBJECT_FLAG_DETERMINED_MASK))) {
        TypeObject *type = obj->getType(cx);
        if (!type->hasAllFlags(flags))
            type->setFlags(cx, flags);
    }
}

/*
 * Mark all properties of a type object as unknown. If markSetsUnknown is set,
 * scan the entire compartment and mark all type sets containing it as having
 * an unknown object. This is needed for correctness in dealing with mutable
 * __proto__, which can change the type of an object dynamically.
 */
inline void
MarkTypeObjectUnknownProperties(JSContext *cx, TypeObject *obj,
                                bool markSetsUnknown = false)
{
    if (cx->typeInferenceEnabled()) {
        if (!obj->unknownProperties())
            obj->markUnknown(cx);
        if (markSetsUnknown && !obj->setsMarkedUnknown)
            cx->compartment->types.markSetsUnknown(cx, obj);
    }
}

/*
 * Mark any property which has been deleted or configured to be non-writable or
 * have a getter/setter.
 */
inline void
MarkTypePropertyConfigured(JSContext *cx, JSObject *obj, jsid id)
{
    if (cx->typeInferenceEnabled() &&
        !obj->hasLazyType() &&
        !obj->type()->unknownProperties()) {
        obj->type()->markPropertyConfigured(cx, id);
    }
}

/* Mark a global object as having had its slots reallocated. */
inline void
MarkGlobalReallocation(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isGlobal());

    if (obj->hasLazyType()) {
        /* No constraints listening to changes on this object. */
        return;
    }

    if (cx->typeInferenceEnabled() && !obj->type()->unknownProperties())
        obj->type()->markSlotReallocation(cx);
}

/*
 * For an array or object which has not yet escaped and been referenced elsewhere,
 * pick a new type based on the object's current contents.
 */

inline void
FixArrayType(JSContext *cx, JSObject *obj)
{
    if (cx->typeInferenceEnabled())
        cx->compartment->types.fixArrayType(cx, obj);
}

inline void
FixObjectType(JSContext *cx, JSObject *obj)
{
    if (cx->typeInferenceEnabled())
        cx->compartment->types.fixObjectType(cx, obj);
}

/* Interface helpers for JSScript */
extern void TypeMonitorResult(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value &rval);
extern void TypeDynamicResult(JSContext *cx, JSScript *script, jsbytecode *pc, js::types::Type type);

inline bool
UseNewTypeAtEntry(JSContext *cx, StackFrame *fp)
{
    return fp->isConstructing() && cx->typeInferenceEnabled() &&
           fp->prev() && fp->prev()->isScriptFrame() &&
           UseNewType(cx, fp->prev()->script(), fp->prev()->pcQuadratic(cx->stack, fp));
}

/////////////////////////////////////////////////////////////////////
// Script interface functions
/////////////////////////////////////////////////////////////////////

inline JSScript *
TypeScript::script()
{
    /*
     * Each TypeScript is embedded as the 'types' field of a JSScript. They
     * have the same lifetime, the distinction is made for code separation.
     * Obtain the base pointer of the outer JSScript.
     */
    return (JSScript *)((char *)this - offsetof(JSScript, types));
}

inline unsigned
TypeScript::numTypeSets()
{
    return script()->nTypeSets + analyze::TotalSlots(script()) + script()->bindings.countUpvars();
}

inline bool
TypeScript::ensureTypeArray(JSContext *cx)
{
    if (typeArray)
        return true;
    return makeTypeArray(cx);
}

inline TypeSet *
TypeScript::bytecodeTypes(const jsbytecode *pc)
{
    JS_ASSERT(typeArray);

    JSOp op = JSOp(*pc);
    JS_ASSERT(op != JSOP_TRAP);
    JS_ASSERT(js_CodeSpec[op].format & JOF_TYPESET);

    /* All bytecodes with type sets are JOF_ATOM, except JSOP_{GET,CALL}ELEM */
    const jsbytecode *npc = (op == JSOP_GETELEM || op == JSOP_CALLELEM) ? pc : pc + 2;
    JS_ASSERT(npc - pc + 3 == js_CodeSpec[op].length);

    uint16 index = GET_UINT16(npc);
    JS_ASSERT(index < script()->nTypeSets);

    return &typeArray[index];
}

inline TypeSet *
TypeScript::returnTypes()
{
    JS_ASSERT(typeArray);
    return &typeArray[script()->nTypeSets + js::analyze::CalleeSlot()];
}

inline TypeSet *
TypeScript::thisTypes()
{
    JS_ASSERT(typeArray);
    return &typeArray[script()->nTypeSets + js::analyze::ThisSlot()];
}

/*
 * Note: for non-escaping arguments and locals, argTypes/localTypes reflect
 * only the initial type of the variable (e.g. passed values for argTypes,
 * or undefined for localTypes) and not types from subsequent assignments.
 */

inline TypeSet *
TypeScript::argTypes(unsigned i)
{
    JS_ASSERT(typeArray && script()->fun && i < script()->fun->nargs);
    return &typeArray[script()->nTypeSets + js::analyze::ArgSlot(i)];
}

inline TypeSet *
TypeScript::localTypes(unsigned i)
{
    JS_ASSERT(typeArray && i < script()->nfixed);
    return &typeArray[script()->nTypeSets + js::analyze::LocalSlot(script(), i)];
}

inline TypeSet *
TypeScript::upvarTypes(unsigned i)
{
    JS_ASSERT(typeArray && i < script()->bindings.countUpvars());
    return &typeArray[script()->nTypeSets + js::analyze::TotalSlots(script()) + i];
}

inline TypeSet *
TypeScript::slotTypes(unsigned slot)
{
    JS_ASSERT(typeArray && slot < js::analyze::TotalSlots(script()));
    return &typeArray[script()->nTypeSets + slot];
}

inline TypeObject *
TypeScript::standardType(JSContext *cx, JSProtoKey key)
{
    JSObject *proto;
    if (!js_GetClassPrototype(cx, script()->global(), key, &proto, NULL))
        return NULL;
    return proto->getNewType(cx);
}

inline TypeObject *
TypeScript::initObject(JSContext *cx, const jsbytecode *pc, JSProtoKey key)
{
    if (!cx->typeInferenceEnabled() || !script()->hasGlobal())
        return GetTypeNewObject(cx, key);

    uint32 offset = pc - script()->code;
    TypeObject *prev = NULL, *obj = typeObjects;
    while (obj) {
        if (obj->initializerKey == key && obj->initializerOffset == offset) {
            /* Move this to the head of the objects list, maintain LRU order. */
            if (prev) {
                prev->next = obj->next;
                obj->next = typeObjects;
                typeObjects = obj;
            }
            return obj;
        }
        prev = obj;
        obj = obj->next;
    }

    return cx->compartment->types.newInitializerTypeObject(cx, script(), offset, key);
}

inline void
TypeScript::monitor(JSContext *cx, jsbytecode *pc, const js::Value &rval)
{
    if (cx->typeInferenceEnabled())
        TypeMonitorResult(cx, script(), pc, rval);
}

inline void
TypeScript::monitorOverflow(JSContext *cx, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        TypeDynamicResult(cx, script(), pc, Type::DoubleType());
}

inline void
TypeScript::monitorString(JSContext *cx, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        TypeDynamicResult(cx, script(), pc, Type::StringType());
}

inline void
TypeScript::monitorUnknown(JSContext *cx, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        TypeDynamicResult(cx, script(), pc, Type::UnknownType());
}

inline void
TypeScript::monitorAssign(JSContext *cx, jsbytecode *pc,
                          JSObject *obj, jsid id, const js::Value &rval)
{
    if (cx->typeInferenceEnabled() && !obj->hasLazyType()) {
        /*
         * Mark as unknown any object which has had dynamic assignments to
         * non-integer properties at SETELEM opcodes. This avoids making large
         * numbers of type properties for hashmap-style objects. :FIXME: this
         * is too aggressive for things like prototype library initialization.
         */
        uint32 i;
        if (js_IdIsIndex(id, &i))
            return;
        MarkTypeObjectUnknownProperties(cx, obj->type());
    }
}

inline void
TypeScript::setThis(JSContext *cx, Type type)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (!ensureTypeArray(cx))
        return;

    /* Analyze the script regardless if -a was used. */
    bool analyze = cx->hasRunOption(JSOPTION_METHODJIT_ALWAYS) && !script()->isUncachedEval;

    if (!thisTypes()->hasType(type) || analyze) {
        AutoEnterTypeInference enter(cx);

        InferSpew(ISpewOps, "externalType: setThis #%u: %s",
                  script()->id(), TypeString(type));
        thisTypes()->addType(cx, type);

        if (analyze)
            script()->ensureRanInference(cx);
    }
}

inline void
TypeScript::setThis(JSContext *cx, const js::Value &value)
{
    if (cx->typeInferenceEnabled())
        setThis(cx, GetValueType(cx, value));
}

inline void
TypeScript::setNewCalled(JSContext *cx)
{
    if (!cx->typeInferenceEnabled() || script()->calledWithNew)
        return;
    script()->calledWithNew = true;

    /*
     * Determining the 'this' type used when the script is invoked with 'new'
     * happens during the script's prologue, so we don't try to pick it up from
     * dynamic calls. Instead, generate constraints modeling the construction
     * of 'this' when the script is analyzed or reanalyzed after an invoke with
     * 'new', and if 'new' is first invoked after the script has already been
     * analyzed.
     */
    AutoEnterTypeInference enter(cx);
    analyze::ScriptAnalysis *analysis = script()->analysis(cx);
    if (!analysis || !analysis->ranInference())
        return;
    analysis->analyzeTypesNew(cx);
}

inline void
TypeScript::setLocal(JSContext *cx, unsigned local, Type type)
{
    if (!cx->typeInferenceEnabled() || !ensureTypeArray(cx))
        return;
    if (!localTypes(local)->hasType(type)) {
        AutoEnterTypeInference enter(cx);

        InferSpew(ISpewOps, "externalType: setLocal #%u %u: %s",
                  script()->id(), local, TypeString(type));
        localTypes(local)->addType(cx, type);
    }
}

inline void
TypeScript::setLocal(JSContext *cx, unsigned local, const js::Value &value)
{
    if (cx->typeInferenceEnabled()) {
        Type type = GetValueType(cx, value);
        setLocal(cx, local, type);
    }
}

inline void
TypeScript::setArgument(JSContext *cx, unsigned arg, Type type)
{
    if (!cx->typeInferenceEnabled() || !ensureTypeArray(cx))
        return;
    if (!argTypes(arg)->hasType(type)) {
        AutoEnterTypeInference enter(cx);

        InferSpew(ISpewOps, "externalType: setArg #%u %u: %s",
                  script()->id(), arg, TypeString(type));
        argTypes(arg)->addType(cx, type);
    }
}

inline void
TypeScript::setArgument(JSContext *cx, unsigned arg, const js::Value &value)
{
    if (cx->typeInferenceEnabled()) {
        Type type = GetValueType(cx, value);
        setArgument(cx, arg, type);
    }
}

inline void
TypeScript::setUpvar(JSContext *cx, unsigned upvar, const js::Value &value)
{
    if (!cx->typeInferenceEnabled() || !ensureTypeArray(cx))
        return;
    Type type = GetValueType(cx, value);
    if (!upvarTypes(upvar)->hasType(type)) {
        AutoEnterTypeInference enter(cx);

        InferSpew(ISpewOps, "externalType: setUpvar #%u %u: %s",
                  script()->id(), upvar, TypeString(type));
        upvarTypes(upvar)->addType(cx, type);
    }
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

inline void
TypeCompartment::addPending(JSContext *cx, TypeConstraint *constraint, TypeSet *source, Type type)
{
    JS_ASSERT(this == &cx->compartment->types);
    JS_ASSERT(!cx->runtime->gcRunning);

    InferSpew(ISpewOps, "pending: %sC%p%s %s",
              InferSpewColor(constraint), constraint, InferSpewColorReset(),
              TypeString(type));

    if (pendingCount == pendingCapacity)
        growPendingArray(cx);

    PendingWork &pending = pendingArray[pendingCount++];
    pending.constraint = constraint;
    pending.source = source;
    pending.type = type;
}

inline void
TypeCompartment::resolvePending(JSContext *cx)
{
    JS_ASSERT(this == &cx->compartment->types);

    if (resolving) {
        /* There is an active call further up resolving the worklist. */
        return;
    }

    resolving = true;

    /* Handle all pending type registrations. */
    while (pendingCount) {
        const PendingWork &pending = pendingArray[--pendingCount];
        InferSpew(ISpewOps, "resolve: %sC%p%s %s",
                  InferSpewColor(pending.constraint), pending.constraint,
                  InferSpewColorReset(), TypeString(pending.type));
        pending.constraint->newType(cx, pending.source, pending.type);
    }

    resolving = false;
}

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

/*
 * The sets of objects and scripts in a type set grow monotonically, are usually
 * empty, almost always small, and sometimes big.  For empty or singleton sets,
 * the pointer refers directly to the value.  For sets fitting into SET_ARRAY_SIZE,
 * an array of this length is used to store the elements.  For larger sets, a hash
 * table filled to 25%-50% of capacity is used, with collisions resolved by linear
 * probing.  TODO: replace these with jshashtables.
 */
const unsigned SET_ARRAY_SIZE = 8;

/* Get the capacity of a set with the given element count. */
static inline unsigned
HashSetCapacity(unsigned count)
{
    JS_ASSERT(count >= 2);

    if (count <= SET_ARRAY_SIZE)
        return SET_ARRAY_SIZE;

    unsigned log2;
    JS_FLOOR_LOG2(log2, count);
    return 1 << (log2 + 2);
}

/* Compute the FNV hash for the low 32 bits of v. */
template <class T, class KEY>
static inline uint32
HashKey(T v)
{
    uint32 nv = KEY::keyBits(v);

    uint32 hash = 84696351 ^ (nv & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 8) & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 16) & 0xff);
    return (hash * 16777619) ^ ((nv >> 24) & 0xff);
}

/*
 * Insert space for an element into the specified set and grow its capacity if needed.
 * returned value is an existing or new entry (NULL if new).
 */
template <class T, class U, class KEY>
static U **
HashSetInsertTry(JSContext *cx, U **&values, unsigned &count, T key, bool pool)
{
    unsigned capacity = HashSetCapacity(count);
    unsigned insertpos = HashKey<T,KEY>(key) & (capacity - 1);

    /* Whether we are converting from a fixed array to hashtable. */
    bool converting = (count == SET_ARRAY_SIZE);

    if (!converting) {
        while (values[insertpos] != NULL) {
            if (KEY::getKey(values[insertpos]) == key)
                return &values[insertpos];
            insertpos = (insertpos + 1) & (capacity - 1);
        }
    }

    count++;
    unsigned newCapacity = HashSetCapacity(count);

    if (newCapacity == capacity) {
        JS_ASSERT(!converting);
        return &values[insertpos];
    }

    U **newValues = pool
        ? ArenaArray<U*>(cx->compartment->pool, newCapacity)
        : (U **) js::OffTheBooks::malloc_(newCapacity * sizeof(U*));
    if (!newValues) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return NULL;
    }
    PodZero(newValues, newCapacity);

    for (unsigned i = 0; i < capacity; i++) {
        if (values[i]) {
            unsigned pos = HashKey<T,KEY>(KEY::getKey(values[i])) & (newCapacity - 1);
            while (newValues[pos] != NULL)
                pos = (pos + 1) & (newCapacity - 1);
            newValues[pos] = values[i];
        }
    }

    if (values && !pool)
        Foreground::free_(values);
    values = newValues;

    insertpos = HashKey<T,KEY>(key) & (newCapacity - 1);
    while (values[insertpos] != NULL)
        insertpos = (insertpos + 1) & (newCapacity - 1);
    return &values[insertpos];
}

/*
 * Insert an element into the specified set if it is not already there, returning
 * an entry which is NULL if the element was not there.
 */
template <class T, class U, class KEY>
static inline U **
HashSetInsert(JSContext *cx, U **&values, unsigned &count, T key, bool pool)
{
    if (count == 0) {
        JS_ASSERT(values == NULL);
        count++;
        return (U **) &values;
    }

    if (count == 1) {
        U *oldData = (U*) values;
        if (KEY::getKey(oldData) == key)
            return (U **) &values;

        values = pool
            ? ArenaArray<U*>(cx->compartment->pool, SET_ARRAY_SIZE)
            : (U **) js::OffTheBooks::malloc_(SET_ARRAY_SIZE * sizeof(U*));
        if (!values) {
            values = (U **) oldData;
            cx->compartment->types.setPendingNukeTypes(cx);
            return NULL;
        }
        PodZero(values, SET_ARRAY_SIZE);
        count++;

        values[0] = oldData;
        return &values[1];
    }

    if (count <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < count; i++) {
            if (KEY::getKey(values[i]) == key)
                return &values[i];
        }

        if (count < SET_ARRAY_SIZE) {
            count++;
            return &values[count - 1];
        }
    }

    return HashSetInsertTry<T,U,KEY>(cx, values, count, key, pool);
}

/* Lookup an entry in a hash set, return NULL if it does not exist. */
template <class T, class U, class KEY>
static inline U *
HashSetLookup(U **values, unsigned count, T key)
{
    if (count == 0)
        return NULL;

    if (count == 1)
        return (KEY::getKey((U *) values) == key) ? (U *) values : NULL;

    if (count <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < count; i++) {
            if (KEY::getKey(values[i]) == key)
                return values[i];
        }
        return NULL;
    }

    unsigned capacity = HashSetCapacity(count);
    unsigned pos = HashKey<T,KEY>(key) & (capacity - 1);

    while (values[pos] != NULL) {
        if (KEY::getKey(values[pos]) == key)
            return values[pos];
        pos = (pos + 1) & (capacity - 1);
    }

    return NULL;
}

inline bool
TypeSet::hasType(Type type)
{
    if (unknown())
        return true;

    if (type.isUnknown()) {
        return false;
    } else if (type.isPrimitive()) {
        return !!(typeFlags & PrimitiveTypeFlag(type.primitive()));
    } else if (type.isAnyObject()) {
        return !!(typeFlags & TYPE_FLAG_ANYOBJECT);
    } else {
        return !!(typeFlags & TYPE_FLAG_ANYOBJECT) ||
            HashSetLookup<TypeObjectKey*,TypeObjectKey,TypeObjectKey>
                (objectSet, objectCount, type.objectKey()) != NULL;
    }
}

inline void
TypeSet::clearObjects()
{
    if (objectCount >= 2 && !intermediate())
        Foreground::free_(objectSet);
    objectCount = 0;
    objectSet = NULL;
}

inline void
TypeSet::addType(JSContext *cx, Type type)
{
    JS_ASSERT(cx->compartment->activeInference);

    if (unknown())
        return;

    if (type.isUnknown()) {
        typeFlags = TYPE_FLAG_UNKNOWN | (typeFlags & ~baseFlags());
        clearObjects();
    } else if (type.isPrimitive()) {
        TypeFlags flag = PrimitiveTypeFlag(type.primitive());
        if (typeFlags & flag)
            return;

        /* If we add float to a type set it is also considered to contain int. */
        if (flag == TYPE_FLAG_DOUBLE)
            flag |= TYPE_FLAG_INT32;

        typeFlags |= flag;
    } else {
        if (typeFlags & TYPE_FLAG_ANYOBJECT)
            return;
        if (type.isAnyObject())
            goto unknownObject;
        TypeObjectKey *object = type.objectKey();
        TypeObjectKey **pentry = HashSetInsert<TypeObjectKey *,TypeObjectKey,TypeObjectKey>
                                     (cx, objectSet, objectCount, object, intermediate());
        if (!pentry || *pentry)
            return;
        *pentry = object;

        if (type.isTypeObject()) {
            TypeObject *nobject = type.typeObject();
            JS_ASSERT(!nobject->singleton);
            if (nobject->unknownProperties())
                goto unknownObject;
            if (objectCount > 1) {
                nobject->contribution += (objectCount - 1) * (objectCount - 1);
                if (nobject->contribution >= TypeObject::CONTRIBUTION_LIMIT) {
                    InferSpew(ISpewOps, "limitUnknown: %sT%p%s",
                              InferSpewColor(this), this, InferSpewColorReset());
                    goto unknownObject;
                }
            }
        }
    }

    if (false) {
    unknownObject:
        type = Type::AnyObjectType();
        typeFlags |= TYPE_FLAG_ANYOBJECT;
        clearObjects();
    }

    InferSpew(ISpewOps, "addType: %sT%p%s %s",
              InferSpewColor(this), this, InferSpewColorReset(),
              TypeString(type));

    /* Propagate the type to all constraints. */
    TypeConstraint *constraint = constraintList;
    while (constraint) {
        cx->compartment->types.addPending(cx, constraint, this, type);
        constraint = constraint->next;
    }

    cx->compartment->types.resolvePending(cx);
}

inline void
TypeSet::setOwnProperty(JSContext *cx, bool configured)
{
    TypeFlags nflags = TYPE_FLAG_OWN_PROPERTY | (configured ? TYPE_FLAG_CONFIGURED_PROPERTY : 0);

    if ((typeFlags & nflags) == nflags)
        return;

    typeFlags |= nflags;

    /* Propagate the change to all constraints. */
    TypeConstraint *constraint = constraintList;
    while (constraint) {
        constraint->newPropertyState(cx, this);
        constraint = constraint->next;
    }
}

inline unsigned
TypeSet::getObjectCount()
{
    JS_ASSERT(!unknownObject());
    if (objectCount > SET_ARRAY_SIZE)
        return HashSetCapacity(objectCount);
    return objectCount;
}

inline TypeObjectKey *
TypeSet::getObject(unsigned i)
{
    JS_ASSERT(i < getObjectCount());
    if (objectCount == 1) {
        JS_ASSERT(i == 0);
        return (TypeObjectKey *) objectSet;
    }
    return objectSet[i];
}

inline JSObject *
TypeSet::getSingleObject(unsigned i)
{
    TypeObjectKey *key = getObject(i);
    return ((jsuword) key & 1) ? (JSObject *)((jsuword) key ^ 1) : NULL;
}

inline TypeObject *
TypeSet::getTypeObject(unsigned i)
{
    TypeObjectKey *key = getObject(i);
    return (key && !((jsuword) key & 1)) ? (TypeObject *) key : NULL;
}

/////////////////////////////////////////////////////////////////////
// TypeCallsite
/////////////////////////////////////////////////////////////////////

inline
TypeCallsite::TypeCallsite(JSContext *cx, JSScript *script, jsbytecode *pc,
                           bool isNew, unsigned argumentCount)
    : script(script), pc(pc), isNew(isNew), argumentCount(argumentCount),
      thisTypes(NULL), returnTypes(NULL)
{
    /* Caller must check for failure. */
    argumentTypes = ArenaArray<TypeSet*>(cx->compartment->pool, argumentCount);
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

inline const char *
TypeObject::name()
{
#ifdef DEBUG
    return TypeIdString(name_);
#else
    return NULL;
#endif
}

inline TypeObject::TypeObject(jsid name, JSObject *proto, bool isFunction)
{
    PodZero(this);

    this->proto = proto;
    this->isFunction = isFunction;

#ifdef DEBUG
    this->name_ = name;
#endif

    InferSpew(ISpewOps, "newObject: %s", this->name());
}

inline TypeSet *
TypeObject::getProperty(JSContext *cx, jsid id, bool assign)
{
    JS_ASSERT(cx->compartment->activeInference);
    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id));
    JS_ASSERT_IF(!JSID_IS_EMPTY(id), id == MakeTypeId(cx, id));
    JS_ASSERT(!unknownProperties());

    Property **pprop = HashSetInsert<jsid,Property,Property>
                           (cx, propertySet, propertyCount, id, false);
    if (!pprop || (!*pprop && !addProperty(cx, id, pprop)))
        return NULL;

    if (assign)
        (*pprop)->types.setOwnProperty(cx, false);

    return &(*pprop)->types;
}

inline bool
TypeObject::hasProperty(JSContext *cx, jsid id)
{
    JS_ASSERT(cx->compartment->activeInference);
    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_STRING(id));
    JS_ASSERT(id == MakeTypeId(cx, id));
    JS_ASSERT(!unknownProperties());

    return HashSetLookup<jsid,Property,Property>
        (propertySet, propertyCount, id) != NULL;
}

inline unsigned
TypeObject::getPropertyCount()
{
    if (propertyCount > SET_ARRAY_SIZE)
        return HashSetCapacity(propertyCount);
    return propertyCount;
}

inline Property *
TypeObject::getProperty(unsigned i)
{
    if (propertyCount == 1) {
        JS_ASSERT(i == 0);
        return (Property *) propertySet;
    }
    return propertySet[i];
}

inline void
TypeObject::setFlagsFromKey(JSContext *cx, JSProtoKey key)
{
    TypeObjectFlags flags = 0;

    switch (key) {
      case JSProto_Function:
        JS_ASSERT(isFunction);
        /* FALLTHROUGH */

      case JSProto_Object:
        flags = OBJECT_FLAG_NON_DENSE_ARRAY
              | OBJECT_FLAG_NON_PACKED_ARRAY
              | OBJECT_FLAG_NON_TYPED_ARRAY;
        break;

      case JSProto_Array:
        flags = OBJECT_FLAG_NON_TYPED_ARRAY;
        break;

      default:
        /* :XXX: abstract */
        JS_ASSERT(key == JSProto_Int8Array ||
                  key == JSProto_Uint8Array ||
                  key == JSProto_Int16Array ||
                  key == JSProto_Uint16Array ||
                  key == JSProto_Int32Array ||
                  key == JSProto_Uint32Array ||
                  key == JSProto_Float32Array ||
                  key == JSProto_Float64Array ||
                  key == JSProto_Uint8ClampedArray);
        flags = OBJECT_FLAG_NON_DENSE_ARRAY
              | OBJECT_FLAG_NON_PACKED_ARRAY;
        break;
    }

    if (!hasAllFlags(flags))
        setFlags(cx, flags);
}

class AutoTypeRooter : private AutoGCRooter {
  public:
    AutoTypeRooter(JSContext *cx, TypeObject *type
                   JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoGCRooter(cx, TYPE), type(type)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    friend void AutoGCRooter::trace(JSTracer *trc);
    friend void MarkRuntime(JSTracer *trc);

  private:
    TypeObject *type;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} } /* namespace js::types */

inline bool
JSScript::isAboutToBeFinalized(JSContext *cx)
{
    return isCachedEval ||
        (u.object && IsAboutToBeFinalized(cx, u.object)) ||
        (fun && IsAboutToBeFinalized(cx, fun));
}

inline bool
JSScript::ensureRanInference(JSContext *cx)
{
    js::analyze::ScriptAnalysis *analysis = this->analysis(cx);
    if (analysis && !analysis->ranInference()) {
        js::types::AutoEnterTypeInference enter(cx);
        analysis->analyzeTypes(cx);
    }
    return analysis && !analysis->OOM();
}

inline void
js::analyze::ScriptAnalysis::addPushedType(JSContext *cx, uint32 offset, uint32 which,
                                           js::types::Type type)
{
    js::types::TypeSet *pushed = pushedTypes(offset, which);
    pushed->addType(cx, type);
}

#endif // jsinferinlines_h___
