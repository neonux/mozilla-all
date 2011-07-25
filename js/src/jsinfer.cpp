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

#include "jsapi.h"
#include "jsautooplen.h"
#include "jsbit.h"
#include "jsbool.h"
#include "jsdate.h"
#include "jsexn.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinfer.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jscntxt.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jstl.h"
#include "jsiter.h"

#include "methodjit/MethodJIT.h"
#include "methodjit/Retcon.h"

#include "jsatominlines.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"
#include "vm/Stack-inl.h"

#ifdef JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

using namespace js;
using namespace js::types;
using namespace js::analyze;

static inline jsid
id_prototype(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
}

static inline jsid
id_arguments(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.argumentsAtom);
}

static inline jsid
id_length(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
}

static inline jsid
id___proto__(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.protoAtom);
}

static inline jsid
id_constructor(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.constructorAtom);
}

static inline jsid
id_caller(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.callerAtom);
}

static inline jsid
id_toString(JSContext *cx)
{
    return ATOM_TO_JSID(cx->runtime->atomState.toStringAtom);
}

static inline jsid
id_toSource(JSContext *cx)
{
    return ATOM_TO_JSID(cx->runtime->atomState.toSourceAtom);
}

#ifdef DEBUG
const char *
types::TypeIdStringImpl(jsid id)
{
    if (JSID_IS_VOID(id))
        return "(index)";
    if (JSID_IS_EMPTY(id))
        return "(new)";
    static char bufs[4][100];
    static unsigned which = 0;
    which = (which + 1) & 3;
    PutEscapedString(bufs[which], 100, JSID_TO_FLAT_STRING(id), 0);
    return bufs[which];
}
#endif

/////////////////////////////////////////////////////////////////////
// Logging
/////////////////////////////////////////////////////////////////////

static bool InferSpewActive(SpewChannel channel)
{
    static bool active[SPEW_COUNT];
    static bool checked = false;
    if (!checked) {
        checked = true;
        PodArrayZero(active);
        const char *env = getenv("INFERFLAGS");
        if (!env)
            return false;
        if (strstr(env, "ops"))
            active[ISpewOps] = true;
        if (strstr(env, "result"))
            active[ISpewResult] = true;
        if (strstr(env, "full")) {
            for (unsigned i = 0; i < SPEW_COUNT; i++)
                active[i] = true;
        }
    }
    return active[channel];
}

#ifdef DEBUG

static bool InferSpewColorable()
{
    /* Only spew colors on xterm-color to not screw up emacs. */
    const char *env = getenv("TERM");
    if (!env)
        return false;
    return strcmp(env, "xterm-color") == 0;
}

const char *
types::InferSpewColorReset()
{
    if (!InferSpewColorable())
        return "";
    return "\x1b[0m";
}

const char *
types::InferSpewColor(TypeConstraint *constraint)
{
    /* Type constraints are printed out using foreground colors. */
    static const char *colors[] = { "\x1b[31m", "\x1b[32m", "\x1b[33m",
                                    "\x1b[34m", "\x1b[35m", "\x1b[36m",
                                    "\x1b[37m" };
    if (!InferSpewColorable())
        return "";
    return colors[DefaultHasher<TypeConstraint *>::hash(constraint) % 7];
}

const char *
types::InferSpewColor(TypeSet *types)
{
    /* Type sets are printed out using bold colors. */
    static const char *colors[] = { "\x1b[1;31m", "\x1b[1;32m", "\x1b[1;33m",
                                    "\x1b[1;34m", "\x1b[1;35m", "\x1b[1;36m",
                                    "\x1b[1;37m" };
    if (!InferSpewColorable())
        return "";
    return colors[DefaultHasher<TypeSet *>::hash(types) % 7];
}

const char *
types::TypeString(Type type)
{
    if (type.isPrimitive()) {
        switch (type.primitive()) {
          case JSVAL_TYPE_UNDEFINED:
            return "void";
          case JSVAL_TYPE_NULL:
            return "null";
          case JSVAL_TYPE_BOOLEAN:
            return "bool";
          case JSVAL_TYPE_INT32:
            return "int";
          case JSVAL_TYPE_DOUBLE:
            return "float";
          case JSVAL_TYPE_STRING:
            return "string";
          case JSVAL_TYPE_MAGIC:
            return "lazyargs";
          default:
            JS_NOT_REACHED("Bad type");
            return "";
        }
    }
    if (type.isUnknown())
        return "unknown";
    if (type.isAnyObject())
        return " object";
    if (type.isSingleObject()) {
        static char bufs[4][40];
        static unsigned which = 0;
        which = (which + 1) & 3;
        JS_snprintf(bufs[which], 40, "<0x%p>", (void *) type.singleObject());
        return bufs[which];
    }
    return type.typeObject()->name();
}

void
types::InferSpew(SpewChannel channel, const char *fmt, ...)
{
    if (!InferSpewActive(channel))
        return;

    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[infer] ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
}

bool
types::TypeHasProperty(JSContext *cx, TypeObject *obj, jsid id, const Value &value)
{
    /*
     * Check the correctness of the type information in the object's property
     * against an actual value.
     */
    if (cx->typeInferenceEnabled() && !obj->unknownProperties() && !value.isUndefined()) {
        id = MakeTypeId(cx, id);

        /* Watch for properties which inference does not monitor. */
        if (id == id___proto__(cx) || id == id_constructor(cx) || id == id_caller(cx))
            return true;

        /*
         * If we called in here while resolving a type constraint, we may be in the
         * middle of resolving a standard class and the type sets will not be updated
         * until the outer TypeSet::add finishes.
         */
        if (cx->compartment->types.pendingCount)
            return true;

        Type type = GetValueType(cx, value);

        AutoEnterTypeInference enter(cx);

        /*
         * We don't track types for properties inherited from prototypes which
         * haven't yet been accessed during analysis of the inheriting object.
         * Don't do the property instantiation now.
         */
        TypeSet *types = obj->maybeGetProperty(cx, id);
        if (!types)
            return true;

        /*
         * If the types inherited from prototypes are not being propagated into
         * this set (because we haven't analyzed code which accesses the
         * property), skip.
         */
        if (!types->hasPropagatedProperty())
            return true;

        if (!types->hasType(type)) {
            TypeFailure(cx, "Missing type in object %s %s: %s",
                        obj->name(), TypeIdString(id), TypeString(type));
        }
    }
    return true;
}

#endif

void
types::TypeFailure(JSContext *cx, const char *fmt, ...)
{
    char msgbuf[1024]; /* Larger error messages will be truncated */
    char errbuf[1024];

    va_list ap;
    va_start(ap, fmt);
    JS_vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
    va_end(ap);

    JS_snprintf(msgbuf, sizeof(msgbuf), "[infer failure] %s", errbuf);

    /*
     * If the INFERFLAGS environment variable is set to 'result' or 'full', 
     * this dumps the current type state of all scripts and type objects 
     * to stdout.
     */
    cx->compartment->types.print(cx);

    /* Always active, even in release builds */
    JS_Assert(msgbuf, __FILE__, __LINE__);
    
    *((int*)NULL) = 0;  /* Should never be reached */
}

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

TypeSet *
TypeSet::make(JSContext *cx, const char *name)
{
    JS_ASSERT(cx->compartment->activeInference);

    TypeSet *res = ArenaNew<TypeSet>(cx->compartment->pool);
    if (!res) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return NULL;
    }

    InferSpew(ISpewOps, "typeSet: %sT%p%s intermediate %s",
              InferSpewColor(res), res, InferSpewColorReset(),
              name);
    res->setIntermediate();

    return res;
}

inline void
TypeSet::add(JSContext *cx, TypeConstraint *constraint, bool callExisting)
{
    if (!constraint) {
        /* OOM failure while constructing the constraint. */
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    JS_ASSERT(cx->compartment->activeInference);

    InferSpew(ISpewOps, "addConstraint: %sT%p%s %sC%p%s %s",
              InferSpewColor(this), this, InferSpewColorReset(),
              InferSpewColor(constraint), constraint, InferSpewColorReset(),
              constraint->kind());

    JS_ASSERT(constraint->next == NULL);
    constraint->next = constraintList;
    constraintList = constraint;

    if (!callExisting)
        return;

    if (flags & TYPE_FLAG_UNKNOWN) {
        cx->compartment->types.addPending(cx, constraint, this, Type::UnknownType());
        cx->compartment->types.resolvePending(cx);
        return;
    }

    for (TypeFlags flag = 1; flag < TYPE_FLAG_ANYOBJECT; flag <<= 1) {
        if (flags & flag) {
            Type type = Type::PrimitiveType(TypeFlagPrimitive(flag));
            cx->compartment->types.addPending(cx, constraint, this, type);
        }
    }

    if (flags & TYPE_FLAG_ANYOBJECT) {
        cx->compartment->types.addPending(cx, constraint, this, Type::AnyObjectType());
        cx->compartment->types.resolvePending(cx);
        return;
    }

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        TypeObjectKey *object = getObject(i);
        if (object)
            cx->compartment->types.addPending(cx, constraint, this, Type::ObjectType(object));
    }

    cx->compartment->types.resolvePending(cx);
}

void
TypeSet::print(JSContext *cx)
{
    if (flags & TYPE_FLAG_OWN_PROPERTY)
        printf(" [own]");
    if (flags & TYPE_FLAG_CONFIGURED_PROPERTY)
        printf(" [configured]");

    if (isDefiniteProperty())
        printf(" [definite:%d]", definiteSlot());

    if (baseFlags() == 0 && !baseObjectCount()) {
        printf(" missing");
        return;
    }

    if (flags & TYPE_FLAG_UNKNOWN)
        printf(" unknown");
    if (flags & TYPE_FLAG_ANYOBJECT)
        printf(" object");

    if (flags & TYPE_FLAG_UNDEFINED)
        printf(" void");
    if (flags & TYPE_FLAG_NULL)
        printf(" null");
    if (flags & TYPE_FLAG_BOOLEAN)
        printf(" bool");
    if (flags & TYPE_FLAG_INT32)
        printf(" int");
    if (flags & TYPE_FLAG_DOUBLE)
        printf(" float");
    if (flags & TYPE_FLAG_STRING)
        printf(" string");
    if (flags & TYPE_FLAG_LAZYARGS)
        printf(" lazyargs");

    uint32 objectCount = baseObjectCount();
    if (objectCount) {
        printf(" object[%u]", objectCount);

        unsigned count = getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            TypeObjectKey *object = getObject(i);
            if (object)
                printf(" %s", TypeString(Type::ObjectType(object)));
        }
    }
}

/////////////////////////////////////////////////////////////////////
// TypeSet constraints
/////////////////////////////////////////////////////////////////////

/* Standard subset constraint, propagate all types from one set to another. */
class TypeConstraintSubset : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintSubset(TypeSet *target)
        : TypeConstraint("subset"), target(target)
    {
        JS_ASSERT(target);
    }

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        /* Basic subset constraint, move all types to the target. */
        target->addType(cx, type);
    }
};

void
TypeSet::addSubset(JSContext *cx, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintSubset>(cx->compartment->pool, target));
}

/* Constraints for reads/writes on object properties. */
class TypeConstraintProp : public TypeConstraint
{
public:
    JSScript *script;
    jsbytecode *pc;

    /*
     * If assign is true, the target is used to update a property of the object.
     * If assign is false, the target is assigned the value of the property.
     */
    bool assign;
    TypeSet *target;

    /* Property being accessed. */
    jsid id;

    TypeConstraintProp(JSScript *script, jsbytecode *pc,
                       TypeSet *target, jsid id, bool assign)
        : TypeConstraint("prop"), script(script), pc(pc),
          assign(assign), target(target), id(id)
    {
        JS_ASSERT(script && pc && target);
    }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addGetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                        TypeSet *target, jsid id)
{
    add(cx, ArenaNew<TypeConstraintProp>(cx->compartment->pool, script, pc, target, id, false));
}

void
TypeSet::addSetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                        TypeSet *target, jsid id)
{
    add(cx, ArenaNew<TypeConstraintProp>(cx->compartment->pool, script, pc, target, id, true));
}

/*
 * Constraints for updating the 'this' types of callees on CALLPROP/CALLELEM.
 * These are derived from the types on the properties themselves, rather than
 * those pushed in the 'this' slot at the call site, which allows us to retain
 * correlations between the type of the 'this' object and the associated
 * callee scripts at polymorphic call sites.
 */
class TypeConstraintCallProp : public TypeConstraint
{
public:
    JSScript *script;
    jsbytecode *callpc;

    /* Property being accessed. */
    jsid id;

    TypeConstraintCallProp(JSScript *script, jsbytecode *callpc, jsid id)
        : TypeConstraint("callprop"), script(script), callpc(callpc), id(id)
    {
        JS_ASSERT(script && callpc);
    }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addCallProperty(JSContext *cx, JSScript *script, jsbytecode *pc, jsid id)
{
    /*
     * For calls which will go through JSOP_NEW, don't add any constraints to
     * modify the 'this' types of callees. The initial 'this' value will be
     * outright ignored.
     */
    jsbytecode *callpc = script->analysis(cx)->getCallPC(pc);
    UntrapOpcode untrap(cx, script, callpc);
    if (JSOp(*callpc) == JSOP_NEW)
        return;

    add(cx, ArenaNew<TypeConstraintCallProp>(cx->compartment->pool, script, callpc, id));
}

/*
 * Constraints for watching call edges as they are discovered and invoking native
 * function handlers, adding constraints for arguments, receiver objects and the
 * return value, and updating script foundOffsets.
 */
class TypeConstraintCall : public TypeConstraint
{
public:
    /* Call site being tracked. */
    TypeCallsite *callsite;

    TypeConstraintCall(TypeCallsite *callsite)
        : TypeConstraint("call"), callsite(callsite)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addCall(JSContext *cx, TypeCallsite *site)
{
    add(cx, ArenaNew<TypeConstraintCall>(cx->compartment->pool, site));
}

/* Constraints for arithmetic operations. */
class TypeConstraintArith : public TypeConstraint
{
public:
    /* Type set receiving the result of the arithmetic. */
    TypeSet *target;

    /* For addition operations, the other operand. */
    TypeSet *other;

    TypeConstraintArith(TypeSet *target, TypeSet *other)
        : TypeConstraint("arith"), target(target), other(other)
    {
        JS_ASSERT(target);
    }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addArith(JSContext *cx, TypeSet *target, TypeSet *other)
{
    add(cx, ArenaNew<TypeConstraintArith>(cx->compartment->pool, target, other));
}

/* Subset constraint which transforms primitive values into appropriate objects. */
class TypeConstraintTransformThis : public TypeConstraint
{
public:
    JSScript *script;
    TypeSet *target;

    TypeConstraintTransformThis(JSScript *script, TypeSet *target)
        : TypeConstraint("transformthis"), script(script), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addTransformThis(JSContext *cx, JSScript *script, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintTransformThis>(cx->compartment->pool, script, target));
}

/*
 * Constraint which adds a particular type to the 'this' types of all
 * discovered scripted functions.
 */
class TypeConstraintPropagateThis : public TypeConstraint
{
public:
    JSScript *script;
    jsbytecode *callpc;
    Type type;

    TypeConstraintPropagateThis(JSScript *script, jsbytecode *callpc, Type type)
        : TypeConstraint("propagatethis"), script(script), callpc(callpc), type(type)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addPropagateThis(JSContext *cx, JSScript *script, jsbytecode *pc, Type type)
{
    /* Don't add constraints when the call will be 'new' (see addCallProperty). */
    jsbytecode *callpc = script->analysis(cx)->getCallPC(pc);
    UntrapOpcode untrap(cx, script, callpc);
    if (JSOp(*callpc) == JSOP_NEW)
        return;

    add(cx, ArenaNew<TypeConstraintPropagateThis>(cx->compartment->pool, script, callpc, type));
}

/* Subset constraint which filters out primitive types. */
class TypeConstraintFilterPrimitive : public TypeConstraint
{
public:
    TypeSet *target;

    /* Primitive types other than null and undefined are passed through. */
    bool onlyNullVoid;

    TypeConstraintFilterPrimitive(TypeSet *target, bool onlyNullVoid)
        : TypeConstraint("filter"), target(target), onlyNullVoid(onlyNullVoid)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (onlyNullVoid) {
            if (type.isPrimitive(JSVAL_TYPE_NULL) || type.isPrimitive(JSVAL_TYPE_UNDEFINED))
                return;
        } else if (type.isPrimitive()) {
            return;
        }

        target->addType(cx, type);
    }
};

void
TypeSet::addFilterPrimitives(JSContext *cx, TypeSet *target, bool onlyNullVoid)
{
    add(cx, ArenaNew<TypeConstraintFilterPrimitive>(cx->compartment->pool,
                                                    target, onlyNullVoid));
}

void
ScriptAnalysis::pruneTypeBarriers(uint32 offset)
{
    TypeBarrier **pbarrier = &getCode(offset).typeBarriers;
    while (*pbarrier) {
        TypeBarrier *barrier = *pbarrier;
        if (barrier->target->hasType(barrier->type)) {
            /* Barrier is now obsolete, it can be removed. */
            *pbarrier = barrier->next;
        } else {
            pbarrier = &barrier->next;
        }
    }
}

/*
 * Cheesy limit on the number of objects we will tolerate in an observed type
 * set before refusing to add new type barriers for objects.
 * :FIXME: this heuristic sucks, and doesn't handle calls.
 */
static const uint32 BARRIER_OBJECT_LIMIT = 10;

void ScriptAnalysis::breakTypeBarriers(JSContext *cx, uint32 offset, bool all)
{
    TypeBarrier **pbarrier = &getCode(offset).typeBarriers;
    while (*pbarrier) {
        TypeBarrier *barrier = *pbarrier;
        if (barrier->target->hasType(barrier->type) ) {
            /* Barrier is now obsolete, it can be removed. */
            *pbarrier = barrier->next;
        } else if (all) {
            /* Force removal of the barrier. */
            barrier->target->addType(cx, barrier->type);
            *pbarrier = barrier->next;
        } else if (!barrier->type.isUnknown() &&
                   !barrier->type.isAnyObject() &&
                   barrier->type.isObject() &&
                   barrier->target->getObjectCount() >= BARRIER_OBJECT_LIMIT) {
            /* Maximum number of objects in the set exceeded. */
            barrier->target->addType(cx, barrier->type);
            *pbarrier = barrier->next;
        } else {
            pbarrier = &barrier->next;
        }
    }
}

void ScriptAnalysis::breakTypeBarriersSSA(JSContext *cx, const SSAValue &v)
{
    if (v.kind() != SSAValue::PUSHED)
        return;

    uint32 offset = v.pushedOffset();
    if (JSOp(script->code[offset]) == JSOP_GETPROP)
        breakTypeBarriersSSA(cx, poppedValue(offset, 0));

    breakTypeBarriers(cx, offset, true);
}

/*
 * Subset constraint for property reads and argument passing which can add type
 * barriers on the read instead of passing types along.
 */
class TypeConstraintSubsetBarrier : public TypeConstraint
{
public:
    JSScript *script;
    jsbytecode *pc;
    TypeSet *target;

    TypeConstraintSubsetBarrier(JSScript *script, jsbytecode *pc, TypeSet *target)
        : TypeConstraint("subsetBarrier"), script(script), pc(pc), target(target)
    {
        JS_ASSERT(!target->intermediate());
    }

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (!target->hasType(type)) {
            script->analysis(cx)->addTypeBarrier(cx, pc, target, type);
            return;
        }

        target->addType(cx, type);
    }
};

void
TypeSet::addSubsetBarrier(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintSubsetBarrier>(cx->compartment->pool, script, pc, target));
}

/*
 * Constraint which marks a pushed ARGUMENTS value as unknown if the script has
 * an arguments object created in the future.
 */
class TypeConstraintLazyArguments : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintLazyArguments(TypeSet *target)
        : TypeConstraint("lazyArgs"), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type) {}

    void newObjectState(JSContext *cx, TypeObject *object, bool force)
    {
        if (object->hasAnyFlags(OBJECT_FLAG_CREATED_ARGUMENTS))
            target->addType(cx, Type::UnknownType());
    }
};

void
TypeSet::addLazyArguments(JSContext *cx, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintLazyArguments>(cx->compartment->pool, target));
}

/*
 * Type constraint which marks the result of 'for in' loops as unknown if the
 * iterated value could be a generator.
 */
class TypeConstraintGenerator : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintGenerator(TypeSet *target)
        : TypeConstraint("generator"), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (type.isUnknown() || type.isAnyObject()) {
            target->addType(cx, Type::UnknownType());
            return;
        }

        if (type.isPrimitive())
            return;

        /*
         * Watch for 'for in' on Iterator and Generator objects, which can
         * produce values other than strings.
         */
        JSObject *proto = type.isTypeObject()
            ? type.typeObject()->proto
            : type.singleObject()->getProto();

        if (proto) {
            Class *clasp = proto->getClass();
            if (clasp == &js_IteratorClass || clasp == &js_GeneratorClass)
                target->addType(cx, Type::UnknownType());
        }
    }
};

/////////////////////////////////////////////////////////////////////
// TypeConstraint
/////////////////////////////////////////////////////////////////////

/* Get the object to use for a property access on type. */
static inline TypeObject *
GetPropertyObject(JSContext *cx, JSScript *script, Type type)
{
    if (type.isTypeObject())
        return type.typeObject();

    /* Force instantiation of lazy types for singleton objects. */
    if (type.isSingleObject())
        return type.singleObject()->getType(cx);

    /*
     * Handle properties attached to primitive types, treating this access as a
     * read on the primitive's new object.
     */
    TypeObject *object = NULL;
    switch (type.primitive()) {

      case JSVAL_TYPE_INT32:
      case JSVAL_TYPE_DOUBLE:
        object = script->types.standardType(cx, JSProto_Number);
        break;

      case JSVAL_TYPE_BOOLEAN:
        object = script->types.standardType(cx, JSProto_Boolean);
        break;

      case JSVAL_TYPE_STRING:
        object = script->types.standardType(cx, JSProto_String);
        break;

      default:
        /* undefined, null and lazy arguments do not have properties. */
        return NULL;
    }

    if (!object)
        cx->compartment->types.setPendingNukeTypes(cx);
    return object;
}

static inline bool
UsePropertyTypeBarrier(jsbytecode *pc)
{
    /*
     * At call opcodes, type barriers can only be added for the call bindings,
     * which TypeConstraintCall will add barrier constraints for directly.
     */
    uint32 format = js_CodeSpec[*pc].format;
    return (format & JOF_TYPESET) && !(format & JOF_INVOKE);
}

static inline void
MarkPropertyAccessUnknown(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target)
{
    if (UsePropertyTypeBarrier(pc))
        script->analysis(cx)->addTypeBarrier(cx, pc, target, Type::UnknownType());
    else
        target->addType(cx, Type::UnknownType());
}

/*
 * Handle a property access on a specific object. All property accesses go through
 * here, whether via x.f, x[f], or global name accesses.
 */
static inline void
PropertyAccess(JSContext *cx, JSScript *script, jsbytecode *pc, TypeObject *object,
               bool assign, TypeSet *target, jsid id)
{
    /* Reads from objects with unknown properties are unknown, writes to such objects are ignored. */
    if (object->unknownProperties()) {
        if (!assign)
            MarkPropertyAccessUnknown(cx, script, pc, target);
        return;
    }

    /* Capture the effects of a standard property access. */
    TypeSet *types = object->getProperty(cx, id, assign);
    if (!types)
        return;
    if (assign) {
        target->addSubset(cx, types);
    } else {
        if (!types->hasPropagatedProperty())
            object->getFromPrototypes(cx, id, types);
        if (UsePropertyTypeBarrier(pc))
            types->addSubsetBarrier(cx, script, pc, target);
        else
            types->addSubset(cx, target);
    }
}

/* Whether the JSObject/TypeObject referent of an access on type cannot be determined. */
static inline bool
UnknownPropertyAccess(JSScript *script, Type type)
{
    return type.isUnknown()
        || type.isAnyObject()
        || (!type.isObject() && !script->hasGlobal());
}

void
TypeConstraintProp::newType(JSContext *cx, TypeSet *source, Type type)
{
    UntrapOpcode untrap(cx, script, pc);

    if (UnknownPropertyAccess(script, type)) {
        /*
         * Access on an unknown object. Reads produce an unknown result, writes
         * need to be monitored.
         */
        if (assign)
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        else
            MarkPropertyAccessUnknown(cx, script, pc, target);
        return;
    }

    if (type.isPrimitive(JSVAL_TYPE_MAGIC)) {
        /* Ignore cases which will be accounted for by the followEscapingArguments analysis. */
        if (assign || (id != JSID_VOID && id != id_length(cx)))
            return;

        if (id == JSID_VOID)
            MarkPropertyAccessUnknown(cx, script, pc, target);
        else
            target->addType(cx, Type::Int32Type());
        return;
    }

    TypeObject *object = GetPropertyObject(cx, script, type);
    if (object)
        PropertyAccess(cx, script, pc, object, assign, target, id);
}

void
TypeConstraintCallProp::newType(JSContext *cx, TypeSet *source, Type type)
{
    UntrapOpcode untrap(cx, script, callpc);

    /*
     * For CALLPROP and CALLELEM, we need to update not just the pushed types
     * but also the 'this' types of possible callees. If we can't figure out
     * that set of callees, monitor the call to make sure discovered callees
     * get their 'this' types updated.
     */

    if (UnknownPropertyAccess(script, type)) {
        cx->compartment->types.monitorBytecode(cx, script, callpc - script->code);
        return;
    }

    TypeObject *object = GetPropertyObject(cx, script, type);
    if (object) {
        if (object->unknownProperties()) {
            cx->compartment->types.monitorBytecode(cx, script, callpc - script->code);
        } else {
            TypeSet *types = object->getProperty(cx, id, false);
            if (!types)
                return;
            if (!types->hasPropagatedProperty())
                object->getFromPrototypes(cx, id, types);
            /* Bypass addPropagateThis, we already have the callpc. */
            types->add(cx, ArenaNew<TypeConstraintPropagateThis>(cx->compartment->pool,
                                                                 script, callpc, type));
        }
    }
}

void
TypeConstraintCall::newType(JSContext *cx, TypeSet *source, Type type)
{
    JSScript *script = callsite->script;
    jsbytecode *pc = callsite->pc;

    if (type.isUnknown() || type.isAnyObject()) {
        /* Monitor calls on unknown functions. */
        cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        return;
    }

    JSScript *callee = NULL;

    if (type.isSingleObject()) {
        JSObject *obj = type.singleObject();

        if (!obj->isFunction()) {
            /* Calls on non-functions are dynamically monitored. */
            return;
        }

        if (obj->getFunctionPrivate()->isNative()) {
            /*
             * The return value and all side effects within native calls should
             * be dynamically monitored, except when the compiler is generating
             * specialized inline code or stub calls for a specific natives and
             * knows about the behavior of that native.
             */
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code, true);

            /*
             * Add type constraints capturing the possible behavior of
             * specialized natives which operate on properties. :XXX: use
             * better factoring for both this and the compiler code itself
             * which specializes particular natives.
             */

            Native native = obj->getFunctionPrivate()->native();

            if (native == js::array_push) {
                for (size_t ind = 0; ind < callsite->argumentCount; ind++) {
                    callsite->thisTypes->addSetProperty(cx, script, pc,
                                                        callsite->argumentTypes[ind], JSID_VOID);
                }
            }

            if (native == js::array_pop)
                callsite->thisTypes->addGetProperty(cx, script, pc, callsite->returnTypes, JSID_VOID);

            return;
        }

        callee = obj->getFunctionPrivate()->script();
    } else if (type.isTypeObject()) {
        callee = type.typeObject()->functionScript;
        if (!callee)
            return;
    } else {
        /* Calls on non-objects are dynamically monitored. */
        return;
    }

    unsigned nargs = callee->fun->nargs;

    if (!callee->types.ensureTypeArray(cx))
        return;

    /* Analyze the function if we have not already done so. */
    if (!callee->ensureRanInference(cx)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    /* Add bindings for the arguments of the call. */
    for (unsigned i = 0; i < callsite->argumentCount && i < nargs; i++) {
        TypeSet *argTypes = callsite->argumentTypes[i];
        TypeSet *types = callee->types.argTypes(i);
        argTypes->addSubsetBarrier(cx, script, pc, types);
    }

    /* Add void type for any formals in the callee not supplied at the call site. */
    for (unsigned i = callsite->argumentCount; i < nargs; i++) {
        TypeSet *types = callee->types.argTypes(i);
        types->addType(cx, Type::UndefinedType());
    }

    if (callsite->isNew) {
        /*
         * If the script does not return a value then the pushed value is the
         * new object (typical case). Note that we don't model construction of
         * the new value, which is done dynamically; we don't keep track of the
         * possible 'new' types for a given prototype type object.
         */
        callee->types.thisTypes()->addSubset(cx, callsite->returnTypes);
        callee->types.returnTypes()->addFilterPrimitives(cx, callsite->returnTypes, false);
    } else {
        /*
         * Add a binding for the return value of the call. We don't add a
         * binding for the receiver object, as this is done with PropagateThis
         * constraints added by the original JSOP_CALL* op. The type sets we
         * manipulate here have lost any correlations between particular types
         * in the 'this' and 'callee' sets, which we want to maintain for
         * polymorphic JSOP_CALLPROP invocations.
         */
        callee->types.returnTypes()->addSubset(cx, callsite->returnTypes);
    }
}

void
TypeConstraintPropagateThis::newType(JSContext *cx, TypeSet *source, Type type)
{
    if (type.isUnknown() || type.isAnyObject()) {
        /*
         * The callee is unknown, make sure the call is monitored so we pick up
         * possible this/callee correlations. This only comes into play for
         * CALLPROP and CALLELEM, for other calls we are past the type barrier
         * already and a TypeConstraintCall will also monitor the call.
         */
        cx->compartment->types.monitorBytecode(cx, script, callpc - script->code);
        return;
    }

    /* Ignore calls to natives, these will be handled by TypeConstraintCall. */
    JSScript *callee = NULL;

    if (type.isSingleObject()) {
        JSObject *object = type.singleObject();
        if (!object->isFunction() || !object->getFunctionPrivate()->isInterpreted())
            return;
        callee = object->getFunctionPrivate()->script();
    } else if (type.isTypeObject()) {
        TypeObject *object = type.typeObject();
        if (!object->isFunction() || !object->functionScript)
            return;
        callee = object->functionScript;
    } else {
        /* Ignore calls to primitives, these will go through a stub. */
        return;
    }

    if (!callee->types.ensureTypeArray(cx))
        return;

    callee->types.thisTypes()->addType(cx, this->type);
}

void
TypeConstraintArith::newType(JSContext *cx, TypeSet *source, Type type)
{
    /*
     * We only model a subset of the arithmetic behavior that is actually
     * possible. The following need to be watched for at runtime:
     *
     * 1. Operations producing a double where no operand was a double.
     * 2. Operations producing a string where no operand was a string (addition only).
     * 3. Operations producing a value other than int/double/string.
     */
    if (other) {
        /*
         * Addition operation, consider these cases:
         *   {int,bool} x {int,bool} -> int
         *   double x {int,bool,double} -> double
         *   string x any -> string
         */
        if (type.isUnknown() || other->unknown()) {
            target->addType(cx, Type::UnknownType());
        } else if (type.isPrimitive(JSVAL_TYPE_DOUBLE)) {
            if (other->hasAnyFlag(TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL |
                                  TYPE_FLAG_INT32 | TYPE_FLAG_DOUBLE | TYPE_FLAG_BOOLEAN |
                                  TYPE_FLAG_ANYOBJECT) ||
                other->getObjectCount() != 0) {
                target->addType(cx, Type::DoubleType());
            }
        } else if (type.isPrimitive(JSVAL_TYPE_STRING)) {
            target->addType(cx, Type::StringType());
        } else {
            if (other->hasAnyFlag(TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL |
                                  TYPE_FLAG_INT32 | TYPE_FLAG_BOOLEAN |
                                  TYPE_FLAG_ANYOBJECT) ||
                other->getObjectCount() != 0) {
                target->addType(cx, Type::Int32Type());
            }
            if (other->hasAnyFlag(TYPE_FLAG_DOUBLE))
                target->addType(cx, Type::DoubleType());
        }
    } else {
        if (type.isUnknown())
            target->addType(cx, Type::UnknownType());
        else if (type.isPrimitive(JSVAL_TYPE_DOUBLE))
            target->addType(cx, Type::DoubleType());
        else
            target->addType(cx, Type::Int32Type());
    }
}

void
TypeConstraintTransformThis::newType(JSContext *cx, TypeSet *source, Type type)
{
    if (type.isUnknown() || type.isAnyObject() || type.isObject() || script->strictModeCode) {
        target->addType(cx, type);
        return;
    }

    /*
     * Note: if |this| is null or undefined, the pushed value is the outer window. We
     * can't use script->getGlobalType() here because it refers to the inner window.
     */
    if (!script->hasGlobal() ||
        type.isPrimitive(JSVAL_TYPE_NULL) ||
        type.isPrimitive(JSVAL_TYPE_UNDEFINED)) {
        target->addType(cx, Type::UnknownType());
        return;
    }

    TypeObject *object = NULL;
    switch (type.primitive()) {
      case JSVAL_TYPE_INT32:
      case JSVAL_TYPE_DOUBLE:
        object = script->types.standardType(cx, JSProto_Number);
        break;
      case JSVAL_TYPE_BOOLEAN:
        object = script->types.standardType(cx, JSProto_Boolean);
        break;
      case JSVAL_TYPE_STRING:
        object = script->types.standardType(cx, JSProto_String);
        break;
      default:
        return;
    }

    if (!object) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    target->addType(cx, Type::ObjectType(object));
}

/////////////////////////////////////////////////////////////////////
// Freeze constraints
/////////////////////////////////////////////////////////////////////

/* Constraint which triggers recompilation of a script if any type is added to a type set. */
class TypeConstraintFreeze : public TypeConstraint
{
public:
    JSScript *script;

    /* Whether a new type has already been added, triggering recompilation. */
    bool typeAdded;

    TypeConstraintFreeze(JSScript *script)
        : TypeConstraint("freeze"), script(script), typeAdded(false)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (typeAdded)
            return;

        typeAdded = true;
        cx->compartment->types.addPendingRecompile(cx, script);
    }
};

void
TypeSet::addFreeze(JSContext *cx)
{
    add(cx, ArenaNew<TypeConstraintFreeze>(cx->compartment->pool,
                                           cx->compartment->types.compiledScript), false);
}

/*
 * Constraint which triggers recompilation of a script if a possible new JSValueType
 * tag is realized for a type set.
 */
class TypeConstraintFreezeTypeTag : public TypeConstraint
{
public:
    JSScript *script;

    /*
     * Whether the type tag has been marked unknown due to a type change which
     * occurred after this constraint was generated (and which triggered recompilation).
     */
    bool typeUnknown;

    TypeConstraintFreezeTypeTag(JSScript *script)
        : TypeConstraint("freezeTypeTag"), script(script), typeUnknown(false)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (typeUnknown)
            return;

        if (!type.isUnknown() && !type.isAnyObject() && type.isObject()) {
            /* Ignore new objects when the type set already has other objects. */
            if (source->getObjectCount() >= 2)
                return;
        }

        typeUnknown = true;
        cx->compartment->types.addPendingRecompile(cx, script);
    }
};

static inline JSValueType
GetValueTypeFromTypeFlags(TypeFlags flags)
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
      case (TYPE_FLAG_INT32 | TYPE_FLAG_DOUBLE):
        return JSVAL_TYPE_DOUBLE;
      case TYPE_FLAG_STRING:
        return JSVAL_TYPE_STRING;
      case TYPE_FLAG_LAZYARGS:
        return JSVAL_TYPE_MAGIC;
      default:
        return JSVAL_TYPE_UNKNOWN;
    }
}

JSValueType
TypeSet::getKnownTypeTag(JSContext *cx)
{
    TypeFlags flags = baseFlags();
    JSValueType type;

    if (baseObjectCount())
        type = flags ? JSVAL_TYPE_UNKNOWN : JSVAL_TYPE_OBJECT;
    else
        type = GetValueTypeFromTypeFlags(flags);

    /*
     * If the type set is totally empty then it will be treated as unknown,
     * but we still need to record the dependency as adding a new type can give
     * it a definite type tag. This is not needed if there are enough types
     * that the exact tag is unknown, as it will stay unknown as more types are
     * added to the set.
     */
    bool empty = flags == 0 && baseObjectCount() == 0;
    JS_ASSERT_IF(empty, type == JSVAL_TYPE_UNKNOWN);

    if (cx->compartment->types.compiledScript && (empty || type != JSVAL_TYPE_UNKNOWN)) {
        add(cx, ArenaNew<TypeConstraintFreezeTypeTag>(cx->compartment->pool,
                                                      cx->compartment->types.compiledScript), false);
    }

    return type;
}

/* Constraint which triggers recompilation if an object acquires particular flags. */
class TypeConstraintFreezeObjectFlags : public TypeConstraint
{
public:
    JSScript *script;

    /* Flags we are watching for on this object. */
    TypeObjectFlags flags;

    /* Whether the object has already been marked as having one of the flags. */
    bool *pmarked;
    bool localMarked;

    TypeConstraintFreezeObjectFlags(JSScript *script, TypeObjectFlags flags, bool *pmarked)
        : TypeConstraint("freezeObjectFlags"), script(script), flags(flags),
          pmarked(pmarked), localMarked(false)
    {}

    TypeConstraintFreezeObjectFlags(JSScript *script, TypeObjectFlags flags)
        : TypeConstraint("freezeObjectFlags"), script(script), flags(flags),
          pmarked(&localMarked), localMarked(false)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type) {}

    void newObjectState(JSContext *cx, TypeObject *object, bool force)
    {
        if (object->hasAnyFlags(flags) && !*pmarked) {
            *pmarked = true;
            cx->compartment->types.addPendingRecompile(cx, script);
        } else if (force) {
            cx->compartment->types.addPendingRecompile(cx, script);
        }
    }
};

/*
 * Constraint which triggers recompilation if any object in a type set acquire
 * particular flags.
 */
class TypeConstraintFreezeObjectFlagsSet : public TypeConstraint
{
public:
    JSScript *script;

    TypeObjectFlags flags;
    bool marked;

    TypeConstraintFreezeObjectFlagsSet(JSScript *script, TypeObjectFlags flags)
        : TypeConstraint("freezeObjectKindSet"), script(script), flags(flags), marked(false)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (marked) {
            /* Despecialized the kind we were interested in due to recompilation. */
            return;
        }

        if (type.isUnknown() || type.isAnyObject()) {
            /* Fallthrough and recompile. */
        } else if (type.isObject()) {
            TypeObject *object = type.isSingleObject()
                ? type.singleObject()->getType(cx)
                : type.typeObject();
            if (!object->hasAnyFlags(flags)) {
                /*
                 * Add a constraint on the the object to pick up changes in the
                 * object's properties.
                 */
                TypeSet *types = object->getProperty(cx, JSID_EMPTY, false);
                if (!types)
                    return;
                types->add(cx,
                    ArenaNew<TypeConstraintFreezeObjectFlags>(cx->compartment->pool,
                                                              script, flags, &marked), false);
                return;
            }
        } else {
            return;
        }

        marked = true;
        cx->compartment->types.addPendingRecompile(cx, script);
    }
};

bool
TypeSet::hasObjectFlags(JSContext *cx, TypeObjectFlags flags)
{
    if (unknownObject())
        return true;

    /*
     * Treat type sets containing no objects as having all object flags,
     * to spare callers from having to check this.
     */
    if (baseObjectCount() == 0)
        return true;

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        TypeObject *object = getTypeObject(i);
        if (!object) {
            JSObject *obj = getSingleObject(i);
            if (obj)
                object = obj->getType(cx);
        }
        if (object && object->hasAnyFlags(flags))
            return true;
    }

    /*
     * Watch for new objects of different kind, and re-traverse existing types
     * in this set to add any needed FreezeArray constraints.
     */
    add(cx, ArenaNew<TypeConstraintFreezeObjectFlagsSet>(cx->compartment->pool,
                                                         cx->compartment->types.compiledScript, flags));

    return false;
}

bool
TypeSet::HasObjectFlags(JSContext *cx, TypeObject *object, TypeObjectFlags flags)
{
    if (object->hasAnyFlags(flags))
        return true;

    TypeSet *types = object->getProperty(cx, JSID_EMPTY, false);
    if (!types)
        return true;
    types->add(cx,
        ArenaNew<TypeConstraintFreezeObjectFlags>(cx->compartment->pool,
                                                  cx->compartment->types.compiledScript, flags), false);
    return false;
}

void
types::MarkArgumentsCreated(JSContext *cx, JSScript *script)
{
    JS_ASSERT(!script->createdArgs);

    script->createdArgs = true;
    script->uninlineable = true;

    MarkTypeObjectFlags(cx, script->fun,
                        OBJECT_FLAG_CREATED_ARGUMENTS | OBJECT_FLAG_UNINLINEABLE);

    if (!script->usedLazyArgs)
        return;

    AutoEnterTypeInference enter(cx);

#ifdef JS_METHODJIT
    mjit::ExpandInlineFrames(cx->compartment, true);
#endif

    ScriptAnalysis *analysis = script->analysis(cx);
    if (analysis && !analysis->ranBytecode())
        analysis->analyzeBytecode(cx);
    if (!analysis || analysis->OOM())
        return;

    for (FrameRegsIter iter(cx); !iter.done(); ++iter) {
        StackFrame *fp = iter.fp();
        if (fp->isScriptFrame() && fp->script() == script) {
            /*
             * Check locals and stack slots, assignment to individual arguments
             * is treated as an escape on the arguments.
             */
            Value *sp = fp->base() + analysis->getCode(iter.pc()).stackDepth;
            for (Value *vp = fp->slots(); vp < sp; vp++) {
                if (vp->isMagicCheck(JS_LAZY_ARGUMENTS)) {
                    if (!js_GetArgsValue(cx, fp, vp))
                        vp->setNull();
                }
            }
        }
    }
}

static inline void
ObjectStateChange(JSContext *cx, TypeObject *object, bool markingUnknown, bool force)
{
    if (object->unknownProperties())
        return;

    /* All constraints listening to state changes are on the empty id. */
    TypeSet *types = object->maybeGetProperty(cx, JSID_EMPTY);

    /* Mark as unknown after getting the types, to avoid assertion. */
    if (markingUnknown)
        object->flags |= OBJECT_FLAG_DYNAMIC_MASK | OBJECT_FLAG_UNKNOWN_PROPERTIES;

    if (types) {
        TypeConstraint *constraint = types->constraintList;
        while (constraint) {
            constraint->newObjectState(cx, object, force);
            constraint = constraint->next;
        }
    }
}

void
TypeSet::WatchObjectReallocation(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isGlobal() && !obj->getType(cx)->unknownProperties());
    TypeSet *types = obj->getType(cx)->getProperty(cx, JSID_EMPTY, false);
    if (!types)
        return;

    /*
     * Reallocating the slots on a global object triggers an object state
     * change on the object with the 'force' parameter set, so we just need
     * a constraint which watches for such changes but no actual object flags.
     */
    types->add(cx, ArenaNew<TypeConstraintFreezeObjectFlags>(cx->compartment->pool,
                                                             cx->compartment->types.compiledScript,
                                                             0));
}

class TypeConstraintFreezeOwnProperty : public TypeConstraint
{
public:
    JSScript *script;

    bool updated;
    bool configurable;

    TypeConstraintFreezeOwnProperty(JSScript *script, bool configurable)
        : TypeConstraint("freezeOwnProperty"),
          script(script), updated(false), configurable(configurable)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type) {}

    void newPropertyState(JSContext *cx, TypeSet *source)
    {
        if (updated)
            return;
        if (source->isOwnProperty(configurable)) {
            updated = true;
            cx->compartment->types.addPendingRecompile(cx, script);
        }
    }
};

static void
CheckNewScriptProperties(JSContext *cx, TypeObject *type, JSScript *script);

bool
TypeSet::isOwnProperty(JSContext *cx, TypeObject *object, bool configurable)
{
    /*
     * Everywhere compiled code depends on definite properties associated with
     * a type object's newScript, we need to make sure there are constraints
     * in place which will mark those properties as configured should the
     * definite properties be invalidated.
     */
    if (object->flags & OBJECT_FLAG_NEW_SCRIPT_REGENERATE) {
        if (object->newScript) {
            CheckNewScriptProperties(cx, object, object->newScript->script);
        } else {
            JS_ASSERT(object->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED);
            object->flags &= ~OBJECT_FLAG_NEW_SCRIPT_REGENERATE;
        }
    }

    if (isOwnProperty(configurable))
        return true;

    add(cx, ArenaNew<TypeConstraintFreezeOwnProperty>(cx->compartment->pool,
                                                      cx->compartment->types.compiledScript,
                                                      configurable), false);
    return false;
}

bool
TypeSet::knownNonEmpty(JSContext *cx)
{
    if (baseFlags() != 0 || baseObjectCount() != 0)
        return true;

    add(cx, ArenaNew<TypeConstraintFreeze>(cx->compartment->pool,
                                           cx->compartment->types.compiledScript), false);

    return false;
}

int
TypeSet::getTypedArrayType(JSContext *cx)
{
    int arrayType = TypedArray::TYPE_MAX;
    unsigned count = getObjectCount();

    for (unsigned i = 0; i < count; i++) {
        JSObject *proto = NULL;
        if (JSObject *object = getSingleObject(i)) {
            proto = object->getProto();
        } else if (TypeObject *object = getTypeObject(i)) {
            JS_ASSERT(!object->hasAnyFlags(OBJECT_FLAG_NON_TYPED_ARRAY));
            proto = object->proto;
        }
        if (!proto)
            continue;

        int objArrayType = proto->getClass() - TypedArray::slowClasses;
        JS_ASSERT(objArrayType >= 0 && objArrayType < TypedArray::TYPE_MAX);

        /*
         * Set arrayType to the type of the first array. Return if there is an array
         * of another type.
         */
        if (arrayType == TypedArray::TYPE_MAX)
            arrayType = objArrayType;
        else if (arrayType != objArrayType)
            return TypedArray::TYPE_MAX;
    }

    /*
     * Assume the caller checked that OBJECT_FLAG_NON_TYPED_ARRAY is not set.
     * This means the set contains at least one object because sets with no
     * objects have all object flags.
     */
    JS_ASSERT(arrayType != TypedArray::TYPE_MAX);

    /* Recompile when another typed array is added to this set. */
    addFreeze(cx);

    return arrayType;
}

JSObject *
TypeSet::getSingleton(JSContext *cx, bool freeze)
{
    if (baseFlags() != 0 || baseObjectCount() != 1)
        return NULL;

    JSObject *obj = getSingleObject(0);
    if (!obj)
        return NULL;

    if (freeze) {
        add(cx, ArenaNew<TypeConstraintFreeze>(cx->compartment->pool,
                                               cx->compartment->types.compiledScript), false);
    }

    return obj;
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

TypeObject types::emptyTypeObject(JSID_VOID, NULL, false, true);

void
TypeCompartment::init(JSContext *cx)
{
    PodZero(this);

#ifndef JS_CPU_ARM
    if (cx && cx->getRunOptions() & JSOPTION_TYPE_INFERENCE)
        inferenceEnabled = true;
#endif
}

TypeObject *
TypeCompartment::newTypeObject(JSContext *cx, JSScript *script,
                               const char *name, const char *postfix,
                               JSProtoKey key, JSObject *proto, bool unknown)
{
#ifdef DEBUG
    if (*postfix) {
        unsigned len = strlen(name) + strlen(postfix) + 2;
        char *newName = (char *) alloca(len);
        JS_snprintf(newName, len, "%s:%s", name, postfix);
        name = newName;
    }
#if 0
    /* Add a unique counter to the name, to distinguish objects from different globals. */
    static unsigned nameCount = 0;
    unsigned len = strlen(name) + 15;
    char *newName = (char *) alloca(len);
    JS_snprintf(newName, len, "%u:%s", ++nameCount, name);
    name = newName;
#endif
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    if (!atom)
        return NULL;
    jsid id = ATOM_TO_JSID(atom);
#else
    jsid id = JSID_VOID;
#endif

    TypeObject *object = NewGCThing<TypeObject>(cx, gc::FINALIZE_TYPE_OBJECT, sizeof(TypeObject));
    if (!object)
        return NULL;
    new(object) TypeObject(id, proto, key == JSProto_Function, unknown);

    if (!cx->typeInferenceEnabled())
        object->flags |= OBJECT_FLAG_UNKNOWN_MASK;
    else
        object->setFlagsFromKey(cx, key);

    return object;
}

TypeObject *
TypeCompartment::newAllocationSiteTypeObject(JSContext *cx, const AllocationSiteKey &key)
{
    AutoEnterTypeInference enter(cx);

    if (!allocationSiteTable) {
        allocationSiteTable = cx->new_<AllocationSiteTable>();
        if (!allocationSiteTable || !allocationSiteTable->init()) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return NULL;
        }
    }

    char *name = NULL;
#ifdef DEBUG
    name = (char *) alloca(40);
    JS_snprintf(name, 40, "#%lu:%lu", key.script->id(), key.offset);
#endif

    AllocationSiteTable::AddPtr p = allocationSiteTable->lookupForAdd(key);
    JS_ASSERT(!p);

    JSObject *proto;
    if (!js_GetClassPrototype(cx, key.script->global(), key.kind, &proto, NULL))
        return NULL;

    TypeObject *res = newTypeObject(cx, key.script, name, "", key.kind, proto);
    if (!res) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return NULL;
    }

    jsbytecode *pc = key.script->code + key.offset;
    UntrapOpcode untrap(cx, key.script, pc);

    if (JSOp(*pc) == JSOP_NEWOBJECT && !key.uncached) {
        /*
         * This object is always constructed the same way and will not be
         * observed by other code before all properties have been added. Mark
         * all the properties as definite properties of the object.
         * :XXX: skipping for objects from uncached eval scripts, as entries
         * in the allocation site table may be stale and we could potentially
         * get a spurious hit. Fix this hack.
         */
        JSObject *baseobj = key.script->getObject(GET_SLOTNO(pc));

        if (!res->addDefiniteProperties(cx, baseobj))
            return NULL;
    }

    if (!allocationSiteTable->add(p, key, res)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return NULL;
    }

    return res;
}

static inline jsid
GetAtomId(JSContext *cx, JSScript *script, const jsbytecode *pc, unsigned offset)
{
    unsigned index = js_GetIndexFromBytecode(cx, script, (jsbytecode*) pc, offset);
    return MakeTypeId(cx, ATOM_TO_JSID(script->getAtom(index)));
}

static inline jsid
GetGlobalId(JSContext *cx, JSScript *script, const jsbytecode *pc)
{
    unsigned index = GET_SLOTNO(pc);
    return MakeTypeId(cx, ATOM_TO_JSID(script->getGlobalAtom(index)));
}

static inline JSObject *
GetScriptObject(JSContext *cx, JSScript *script, const jsbytecode *pc, unsigned offset)
{
    unsigned index = js_GetIndexFromBytecode(cx, script, (jsbytecode*) pc, offset);
    return script->getObject(index);
}

static inline const Value &
GetScriptConst(JSContext *cx, JSScript *script, const jsbytecode *pc)
{
    unsigned index = js_GetIndexFromBytecode(cx, script, (jsbytecode*) pc, 0);
    return script->getConst(index);
}

bool
types::UseNewType(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(cx->typeInferenceEnabled());

    UntrapOpcode untrap(cx, script, pc);

    /*
     * Make a heuristic guess at a use of JSOP_NEW that the constructed object
     * should have a fresh type object. We do this when the NEW is immediately
     * followed by a simple assignment to an object's .prototype field.
     * This is designed to catch common patterns for subclassing in JS:
     *
     * function Super() { ... }
     * function Sub1() { ... }
     * function Sub2() { ... }
     *
     * Sub1.prototype = new Super();
     * Sub2.prototype = new Super();
     *
     * Using distinct type objects for the particular prototypes of Sub1 and
     * Sub2 lets us continue to distinguish the two subclasses and any extra
     * properties added to those prototype objects.
     */
    if (JSOp(*pc) != JSOP_NEW)
        return false;
    pc += JSOP_NEW_LENGTH;
    if (JSOp(*pc) == JSOP_SETPROP) {
        jsid id = GetAtomId(cx, script, pc, 0);
        if (id == id_prototype(cx))
            return true;
    }

    return false;
}

void
TypeCompartment::growPendingArray(JSContext *cx)
{
    unsigned newCapacity = js::Max(unsigned(100), pendingCapacity * 2);
    PendingWork *newArray = (PendingWork *) js::OffTheBooks::calloc_(newCapacity * sizeof(PendingWork));
    if (!newArray) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    memcpy(newArray, pendingArray, pendingCount * sizeof(PendingWork));
    cx->free_(pendingArray);

    pendingArray = newArray;
    pendingCapacity = newCapacity;
}

void
TypeCompartment::processPendingRecompiles(JSContext *cx)
{
    /* Steal the list of scripts to recompile, else we will try to recursively recompile them. */
    Vector<JSScript*> *pending = pendingRecompiles;
    pendingRecompiles = NULL;

    JS_ASSERT(!pending->empty());

#ifdef JS_METHODJIT

    mjit::ExpandInlineFrames(cx->compartment, true);

    for (unsigned i = 0; i < pending->length(); i++) {
        JSScript *script = (*pending)[i];
        mjit::Recompiler recompiler(cx, script);
        if (script->hasJITCode())
            recompiler.recompile();
    }

#endif /* JS_METHODJIT */

    cx->delete_(pending);
}

void
TypeCompartment::setPendingNukeTypes(JSContext *cx)
{
    JS_ASSERT(cx->compartment->activeInference);
    if (!pendingNukeTypes) {
        js_ReportOutOfMemory(cx);
        pendingNukeTypes = true;
    }
}

void
TypeCompartment::nukeTypes(JSContext *cx)
{
    JSCompartment *compartment = cx->compartment;
    JS_ASSERT(this == &compartment->types);

    /*
     * This is the usual response if we encounter an OOM while adding a type
     * or resolving type constraints. Reset the compartment to not use type
     * inference, and recompile all scripts.
     *
     * Because of the nature of constraint-based analysis (add constraints, and
     * iterate them until reaching a fixpoint), we can't undo an add of a type set,
     * and merely aborting the operation which triggered the add will not be
     * sufficient for correct behavior as we will be leaving the types in an
     * inconsistent state.
     */
    JS_ASSERT(pendingNukeTypes);
    if (pendingRecompiles) {
        cx->free_(pendingRecompiles);
        pendingRecompiles = NULL;
    }

    /*
     * We may or may not be under the GC. In either case don't allocate, and
     * acquire the GC lock so we can update inferenceEnabled for all contexts.
     */

#ifdef JS_THREADSAFE
    Maybe<AutoLockGC> maybeLock;
    if (!cx->runtime->gcMarkAndSweep)
        maybeLock.construct(cx->runtime);
#endif

    inferenceEnabled = false;

    /* Update the cached inferenceEnabled bit in all contexts. */
    for (JSCList *cl = cx->runtime->contextList.next;
         cl != &cx->runtime->contextList;
         cl = cl->next) {
        JSContext *cx = js_ContextFromLinkField(cl);
        cx->setCompartment(cx->compartment);
    }

#ifdef JS_METHODJIT

    mjit::ExpandInlineFrames(cx->compartment, true);

    /* Throw away all JIT code in the compartment, but leave everything else alone. */
    for (JSCList *cursor = compartment->scripts.next;
         cursor != &compartment->scripts;
         cursor = cursor->next) {
        JSScript *script = reinterpret_cast<JSScript *>(cursor);
        if (script->hasJITCode()) {
            mjit::Recompiler recompiler(cx, script);
            recompiler.recompile();
        }
    }

#endif /* JS_METHODJIT */

}

void
TypeCompartment::addPendingRecompile(JSContext *cx, JSScript *script)
{
#ifdef JS_METHODJIT
    if (!script->jitNormal && !script->jitCtor) {
        /* Scripts which haven't been compiled yet don't need to be recompiled. */
        return;
    }

    if (!pendingRecompiles) {
        pendingRecompiles = cx->new_< Vector<JSScript*> >(cx);
        if (!pendingRecompiles) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
    }

    for (unsigned i = 0; i < pendingRecompiles->length(); i++) {
        if (script == (*pendingRecompiles)[i])
            return;
    }

    if (!pendingRecompiles->append(script)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }
#endif
}

void
TypeCompartment::monitorBytecode(JSContext *cx, JSScript *script, uint32 offset,
                                 bool returnOnly)
{
    ScriptAnalysis *analysis = script->analysis(cx);
    JS_ASSERT(analysis->ranInference());

    jsbytecode *pc = script->code + offset;
    UntrapOpcode untrap(cx, script, pc);

    JS_ASSERT_IF(returnOnly, js_CodeSpec[*pc].format & JOF_INVOKE);

    Bytecode &code = analysis->getCode(pc);

    if (returnOnly ? code.monitoredTypesReturn : code.monitoredTypes)
        return;

    InferSpew(ISpewOps, "addMonitorNeeded:%s #%u:%05u",
              returnOnly ? " returnOnly" : "", script->id(), offset);

    /* Dynamically monitor this call to keep track of its result types. */
    if (js_CodeSpec[*pc].format & JOF_INVOKE)
        code.monitoredTypesReturn = true;

    if (!returnOnly)
        code.monitoredTypes = true;

    cx->compartment->types.addPendingRecompile(cx, script);

    /* Trigger recompilation of any inline callers. */
    if (script->fun && !script->fun->hasLazyType())
        ObjectStateChange(cx, script->fun->type(), false, true);
}

/*
 * State for keeping track of which property type sets contain an object we are
 * scrubbing from all properties in the compartment. We make a list of
 * properties to update and fix them afterwards, as adding types can't be done
 * with the GC locked (as is done in IterateCells), and can potentially make
 * new type objects as well.
 */
struct MarkSetsUnknownState
{
    TypeObject *target;
    Vector<TypeSet *> pending;

    MarkSetsUnknownState(JSContext *cx, TypeObject *target)
        : target(target), pending(cx)
    {}
};

static void
MarkObjectSetsUnknownCallback(JSContext *cx, void *data, void *thing,
                              size_t traceKind, size_t thingSize)
{
    MarkSetsUnknownState *state = (MarkSetsUnknownState *) data;
    TypeObject *object = (TypeObject *) thing;

    unsigned count = object->getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        Property *prop = object->getProperty(i);
        if (prop && prop->types.hasType(Type::ObjectType(state->target))) {
            if (!state->pending.append(&prop->types))
                cx->compartment->types.setPendingNukeTypes(cx);
        }
    }
}

void
TypeCompartment::markSetsUnknown(JSContext *cx, TypeObject *target)
{
    JS_ASSERT(this == &cx->compartment->types);
    JS_ASSERT(!(target->flags & OBJECT_FLAG_SETS_MARKED_UNKNOWN));
    JS_ASSERT(!target->singleton);
    JS_ASSERT(target->unknownProperties());
    target->flags |= OBJECT_FLAG_SETS_MARKED_UNKNOWN;

    AutoEnterTypeInference enter(cx);

    /*
     * Mark both persistent and transient type sets which contain obj as having
     * a generic object type. It is not sufficient to mark just the persistent
     * sets, as analysis of individual opcodes can pull type objects from
     * static information (like initializer objects at various offsets).
     */

    MarkSetsUnknownState state(cx, target);

    IterateCells(cx, cx->compartment, gc::FINALIZE_TYPE_OBJECT,
                 (void *) &state, MarkObjectSetsUnknownCallback);

    for (unsigned i = 0; i < state.pending.length(); i++)
        state.pending[i]->addType(cx, Type::AnyObjectType());

    for (JSCList *cursor = cx->compartment->scripts.next;
         cursor != &cx->compartment->scripts;
         cursor = cursor->next) {
        JSScript *script = reinterpret_cast<JSScript *>(cursor);
        if (script->types.typeArray) {
            unsigned count = script->types.numTypeSets();
            for (unsigned i = 0; i < count; i++) {
                if (script->types.typeArray[i].hasType(Type::ObjectType(target)))
                    script->types.typeArray[i].addType(cx, Type::AnyObjectType());
            }
        }
        if (script->hasAnalysis() && script->analysis(cx)->ranInference()) {
            for (unsigned i = 0; i < script->length; i++) {
                if (!script->analysis(cx)->maybeCode(i))
                    continue;
                jsbytecode *pc = script->code + i;
                UntrapOpcode untrap(cx, script, pc);
                if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
                    continue;
                unsigned defCount = GetDefCount(script, i);
                if (ExtendedDef(pc))
                    defCount++;
                for (unsigned j = 0; j < defCount; j++) {
                    TypeSet *types = script->analysis(cx)->pushedTypes(pc, j);
                    if (types->hasType(Type::ObjectType(target)))
                        types->addType(cx, Type::AnyObjectType());
                }
            }
        }
    }
}

void
ScriptAnalysis::addTypeBarrier(JSContext *cx, const jsbytecode *pc, TypeSet *target, Type type)
{
    Bytecode &code = getCode(pc);

    if (!type.isUnknown() && !type.isAnyObject() &&
        type.isObject() && target->getObjectCount() >= BARRIER_OBJECT_LIMIT) {
        /* Ignore this barrier, just add the type to the target. */
        target->addType(cx, type);
        return;
    }

    if (!code.typeBarriers) {
        /*
         * Adding type barriers at a bytecode which did not have them before
         * will trigger recompilation. If there were already type barriers,
         * however, do not trigger recompilation (the script will be recompiled
         * if any of the barriers is ever violated).
         */
        cx->compartment->types.addPendingRecompile(cx, script);

        /* Trigger recompilation of any inline callers. */
        if (script->fun && !script->fun->hasLazyType())
            ObjectStateChange(cx, script->fun->type(), false, true);
    }

    /* Ignore duplicate barriers. */
    TypeBarrier *barrier = code.typeBarriers;
    while (barrier) {
        if (barrier->target == target && barrier->type == type)
            return;
        barrier = barrier->next;
    }

    InferSpew(ISpewOps, "typeBarrier: #%u:%05u: %sT%p%s %s",
              script->id(), pc - script->code,
              InferSpewColor(target), target, InferSpewColorReset(),
              TypeString(type));

    barrier = ArenaNew<TypeBarrier>(cx->compartment->pool, target, type);

    barrier->next = code.typeBarriers;
    code.typeBarriers = barrier;
}

#ifdef DEBUG
static void
PrintObjectCallback(JSContext *cx, void *data, void *thing,
                    size_t traceKind, size_t thingSize)
{
    TypeObject *object = (TypeObject *) thing;
    object->print(cx);
}
#endif

void
TypeCompartment::print(JSContext *cx)
{
    JSCompartment *compartment = this->compartment();

    if (!InferSpewActive(ISpewResult) || JS_CLIST_IS_EMPTY(&compartment->scripts))
        return;

    for (JSScript *script = (JSScript *)compartment->scripts.next;
         &script->links != &compartment->scripts;
         script = (JSScript *)script->links.next) {
        if (script->hasAnalysis() && script->analysis(cx)->ranInference())
            script->analysis(cx)->printTypes(cx);
    }

#ifdef DEBUG
    {
        AutoUnlockGC unlock(cx->runtime);
        IterateCells(cx, compartment, gc::FINALIZE_TYPE_OBJECT, NULL, PrintObjectCallback);
    }
#endif

    printf("Counts: ");
    for (unsigned count = 0; count < TYPE_COUNT_LIMIT; count++) {
        if (count)
            printf("/");
        printf("%u", typeCounts[count]);
    }
    printf(" (%u over)\n", typeCountOver);

    printf("Recompilations: %u\n", recompilations);
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment tables
/////////////////////////////////////////////////////////////////////

/*
 * The arrayTypeTable and objectTypeTable are per-compartment tables for making
 * common type objects to model the contents of large script singletons and
 * JSON objects. These are vanilla Arrays and native Objects, so we distinguish
 * the types of different ones by looking at the types of their properties.
 *
 * All singleton/JSON arrays which have the same prototype, are homogenous and
 * of the same element type will share a type object. All singleton/JSON
 * objects which have the same shape and property types will also share a type
 * object. We don't try to collate arrays or objects that have type mismatches.
 */

static inline bool
NumberTypes(Type a, Type b)
{
    return (a.isPrimitive(JSVAL_TYPE_INT32) || a.isPrimitive(JSVAL_TYPE_DOUBLE))
        && (b.isPrimitive(JSVAL_TYPE_INT32) || b.isPrimitive(JSVAL_TYPE_DOUBLE));
}

struct types::ArrayTableKey
{
    Type type;
    JSObject *proto;

    ArrayTableKey()
        : type(Type::UndefinedType()), proto(NULL)
    {}

    typedef ArrayTableKey Lookup;

    static inline uint32 hash(const ArrayTableKey &v) {
        return (uint32) (v.type.raw() ^ ((uint32)(size_t)v.proto >> 2));
    }

    static inline bool match(const ArrayTableKey &v1, const ArrayTableKey &v2) {
        return v1.type == v2.type && v1.proto == v2.proto;
    }
};

void
TypeCompartment::fixArrayType(JSContext *cx, JSObject *obj)
{
    AutoEnterTypeInference enter(cx);

    if (!arrayTypeTable) {
        arrayTypeTable = cx->new_<ArrayTypeTable>();
        if (!arrayTypeTable || !arrayTypeTable->init()) {
            arrayTypeTable = NULL;
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
    }

    /*
     * If the array is of homogenous type, pick a type object which will be
     * shared with all other singleton/JSON arrays of the same type.
     * If the array is heterogenous, keep the existing type object, which has
     * unknown properties.
     */
    JS_ASSERT(obj->isPackedDenseArray());

    unsigned len = obj->getDenseArrayInitializedLength();
    if (len == 0)
        return;

    Type type = GetValueType(cx, obj->getDenseArrayElement(0));

    for (unsigned i = 1; i < len; i++) {
        Type ntype = GetValueType(cx, obj->getDenseArrayElement(i));
        if (ntype != type) {
            if (NumberTypes(type, ntype))
                type = Type::DoubleType();
            else
                return;
        }
    }

    ArrayTableKey key;
    key.type = type;
    key.proto = obj->getProto();
    ArrayTypeTable::AddPtr p = arrayTypeTable->lookupForAdd(key);

    if (p) {
        obj->setType(p->value);
    } else {
        char *name = NULL;
#ifdef DEBUG
        static unsigned count = 0;
        name = (char *) alloca(20);
        JS_snprintf(name, 20, "TableArray:%u", ++count);
#endif

        TypeObject *objType = newTypeObject(cx, NULL, name, "", JSProto_Array, obj->getProto());
        if (!objType) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        obj->setType(objType);

        if (!objType->unknownProperties())
            objType->addPropertyType(cx, JSID_VOID, type);

        if (!arrayTypeTable->relookupOrAdd(p, key, objType)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
    }
}

/*
 * N.B. We could also use the initial shape of the object (before its type is
 * fixed) as the key in the object table, but since all references in the table
 * are weak the hash entries would usually be collected on GC even if objects
 * with the new type/shape are still live.
 */
struct types::ObjectTableKey
{
    jsid *ids;
    uint32 nslots;
    uint32 nfixed;
    JSObject *proto;

    typedef JSObject * Lookup;

    static inline uint32 hash(JSObject *obj) {
        return (uint32) (JSID_BITS(obj->lastProperty()->propid) ^
                         obj->slotSpan() ^ obj->numFixedSlots() ^
                         ((uint32)(size_t)obj->getProto() >> 2));
    }

    static inline bool match(const ObjectTableKey &v, JSObject *obj) {
        if (obj->slotSpan() != v.nslots ||
            obj->numFixedSlots() != v.nfixed ||
            obj->getProto() != v.proto) {
            return false;
        }
        const Shape *shape = obj->lastProperty();
        while (!JSID_IS_EMPTY(shape->propid)) {
            if (shape->propid != v.ids[shape->slot])
                return false;
            shape = shape->previous();
        }
        return true;
    }
};

struct types::ObjectTableEntry
{
    TypeObject *object;
    Shape *newShape;
    Type *types;
};

void
TypeCompartment::fixObjectType(JSContext *cx, JSObject *obj)
{
    AutoEnterTypeInference enter(cx);

    if (!objectTypeTable) {
        objectTypeTable = cx->new_<ObjectTypeTable>();
        if (!objectTypeTable || !objectTypeTable->init()) {
            objectTypeTable = NULL;
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
    }

    /*
     * Use the same type object for all singleton/JSON arrays with the same
     * base shape, i.e. the same fields written in the same order. If there
     * is a type mismatch with previous objects of the same shape, use the
     * generic unknown type.
     */
    JS_ASSERT(obj->isObject());

    if (obj->slotSpan() == 0 || obj->inDictionaryMode())
        return;

    ObjectTypeTable::AddPtr p = objectTypeTable->lookupForAdd(obj);
    const Shape *baseShape = obj->lastProperty();

    if (p) {
        /* The lookup ensures the shape matches, now check that the types match. */
        Type *types = p->value.types;
        for (unsigned i = 0; i < obj->slotSpan(); i++) {
            Type ntype = GetValueType(cx, obj->getSlot(i));
            if (ntype != types[i]) {
                if (NumberTypes(ntype, types[i])) {
                    if (types[i].isPrimitive(JSVAL_TYPE_INT32)) {
                        types[i] = Type::DoubleType();
                        const Shape *shape = baseShape;
                        while (!JSID_IS_EMPTY(shape->propid)) {
                            if (shape->slot == i) {
                                Type type = Type::DoubleType();
                                if (!p->value.object->unknownProperties()) {
                                    jsid id = MakeTypeId(cx, shape->propid);
                                    p->value.object->addPropertyType(cx, id, type);
                                }
                                break;
                            }
                            shape = shape->previous();
                        }
                    }
                } else {
                    return;
                }
            }
        }

        obj->setTypeAndShape(p->value.object, p->value.newShape);
    } else {
        /*
         * Make a new type to use, and regenerate a new shape to go with it.
         * Shapes are rooted at the empty shape for the object's type, so we
         * can't change the type without changing the shape.
         */
        JSObject *xobj = NewBuiltinClassInstance(cx, &js_ObjectClass,
                                                 (gc::FinalizeKind) obj->finalizeKind());
        if (!xobj) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        AutoObjectRooter xvr(cx, xobj);

        char *name = NULL;
#ifdef DEBUG
        static unsigned count = 0;
        name = (char *) alloca(20);
        JS_snprintf(name, 20, "TableObject:%u", ++count);
#endif

        TypeObject *objType = newTypeObject(cx, NULL, name, "", JSProto_Object, obj->getProto());
        if (!objType) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        xobj->setType(objType);

        jsid *ids = (jsid *) cx->calloc_(obj->slotSpan() * sizeof(jsid));
        if (!ids) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        Type *types = (Type *) cx->calloc_(obj->slotSpan() * sizeof(Type));
        if (!types) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        const Shape *shape = baseShape;
        while (!JSID_IS_EMPTY(shape->propid)) {
            ids[shape->slot] = shape->propid;
            types[shape->slot] = GetValueType(cx, obj->getSlot(shape->slot));
            if (!objType->unknownProperties()) {
                jsid id = MakeTypeId(cx, shape->propid);
                objType->addPropertyType(cx, id, types[shape->slot]);
            }
            shape = shape->previous();
        }

        /* Construct the new shape. */
        for (unsigned i = 0; i < obj->slotSpan(); i++) {
            if (!DefineNativeProperty(cx, xobj, ids[i], UndefinedValue(), NULL, NULL,
                                      JSPROP_ENUMERATE, 0, 0, DNP_SKIP_TYPE)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                return;
            }
        }
        JS_ASSERT(!xobj->inDictionaryMode());
        const Shape *newShape = xobj->lastProperty();

        if (!objType->addDefiniteProperties(cx, xobj))
            return;

        ObjectTableKey key;
        key.ids = ids;
        key.nslots = obj->slotSpan();
        key.nfixed = obj->numFixedSlots();
        key.proto = obj->getProto();
        JS_ASSERT(ObjectTableKey::match(key, obj));

        ObjectTableEntry entry;
        entry.object = objType;
        entry.newShape = (Shape *) newShape;
        entry.types = types;

        p = objectTypeTable->lookupForAdd(obj);
        if (!objectTypeTable->add(p, key, entry)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        obj->setTypeAndShape(objType, newShape);
    }
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

void
TypeObject::getFromPrototypes(JSContext *cx, jsid id, TypeSet *types, bool force)
{
    if (!force && types->hasPropagatedProperty())
        return;

    types->setPropagatedProperty();

    if (!proto)
        return;

    if (proto->getType(cx)->unknownProperties()) {
        types->addType(cx, Type::UnknownType());
        return;
    }

    TypeSet *protoTypes = proto->type()->getProperty(cx, id, false);
    if (!protoTypes)
        return;

    protoTypes->addSubset(cx, types);

    proto->type()->getFromPrototypes(cx, id, protoTypes);
}

static inline void
UpdatePropertyType(JSContext *cx, TypeSet *types, JSObject *obj, const Shape *shape)
{
    if (shape->hasGetterValue() || shape->hasSetterValue()) {
        types->addType(cx, Type::UnknownType());
    } else if (shape->slot != SHAPE_INVALID_SLOT &&
               (shape->hasDefaultGetter() || shape->isMethod())) {
        Type type = GetValueType(cx, obj->nativeGetSlot(shape->slot));
        types->addType(cx, type);
    }
}

bool
TypeObject::addProperty(JSContext *cx, jsid id, Property **pprop)
{
    JS_ASSERT(!*pprop);
    Property *base = singleton
        ? ArenaNew<Property>(cx->compartment->pool, id)
        : cx->new_<Property>(id);
    if (!base) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }

    if (singleton) {
        base->types.setIntermediate();

        /*
         * Fill the property in with any type the object already has in an
         * own property. We are only interested in plain native properties
         * which don't go through a barrier when read by the VM or jitcode.
         * We don't need to handle arrays or other JIT'ed non-natives as
         * these are not (yet) singletons.
         */

        if (JSID_IS_VOID(id)) {
            /* Go through all shapes on the object to get integer-valued properties. */
            const Shape *shape = singleton->lastProperty();
            while (!JSID_IS_EMPTY(shape->propid)) {
                if (JSID_IS_VOID(MakeTypeId(cx, shape->propid)))
                    UpdatePropertyType(cx, &base->types, singleton, shape);
                shape = shape->previous();
            }
        } else {
            const Shape *shape = singleton->nativeLookup(id);
            if (shape)
                UpdatePropertyType(cx, &base->types, singleton, shape);
        }
    }

    *pprop = base;

    InferSpew(ISpewOps, "typeSet: %sT%p%s property %s %s",
              InferSpewColor(&base->types), &base->types, InferSpewColorReset(),
              name(), TypeIdString(id));

    return true;
}

bool
TypeObject::addDefiniteProperties(JSContext *cx, JSObject *obj)
{
    if (unknownProperties())
        return true;

    /* Mark all properties of obj as definite properties of this type. */
    AutoEnterTypeInference enter(cx);

    const Shape *shape = obj->lastProperty();
    while (!JSID_IS_EMPTY(shape->propid)) {
        jsid id = MakeTypeId(cx, shape->propid);
        if (!JSID_IS_VOID(id) && obj->isFixedSlot(shape->slot) &&
            shape->slot <= (TYPE_FLAG_DEFINITE_MASK >> TYPE_FLAG_DEFINITE_SHIFT)) {
            TypeSet *types = getProperty(cx, id, true);
            if (!types)
                return false;
            types->setDefinite(shape->slot);
        }
        shape = shape->previous();
    }

    return true;
}

bool
TypeObject::matchDefiniteProperties(JSObject *obj)
{
    unsigned count = getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        Property *prop = getProperty(i);
        if (!prop)
            continue;
        if (prop->types.isDefiniteProperty()) {
            unsigned slot = prop->types.definiteSlot();

            bool found = false;
            const Shape *shape = obj->lastProperty();
            while (!JSID_IS_EMPTY(shape->propid)) {
                if (shape->slot == slot && shape->propid == prop->id) {
                    found = true;
                    break;
                }
                shape = shape->previous();
            }
            if (!found)
                return false;
        }
    }

    return true;
}

inline void
InlineAddTypeProperty(JSContext *cx, TypeObject *obj, jsid id, Type type)
{
    JS_ASSERT(id == MakeTypeId(cx, id));

    AutoEnterTypeInference enter(cx);

    TypeSet *types = obj->getProperty(cx, id, true);
    if (!types || types->hasType(type))
        return;

    InferSpew(ISpewOps, "externalType: property %s %s: %s",
              obj->name(), TypeIdString(id), TypeString(type));
    types->addType(cx, type);
}

void
TypeObject::addPropertyType(JSContext *cx, jsid id, Type type)
{
    InlineAddTypeProperty(cx, this, id, type);
}

void
TypeObject::addPropertyType(JSContext *cx, jsid id, const Value &value)
{
    InlineAddTypeProperty(cx, this, id, GetValueType(cx, value));
}

void
TypeObject::addPropertyType(JSContext *cx, const char *name, Type type)
{
    jsid id = JSID_VOID;
    if (name) {
        JSAtom *atom = js_Atomize(cx, name, strlen(name));
        if (!atom) {
            AutoEnterTypeInference enter(cx);
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        id = ATOM_TO_JSID(atom);
    }
    InlineAddTypeProperty(cx, this, id, type);
}

void
TypeObject::addPropertyType(JSContext *cx, const char *name, const Value &value)
{
    addPropertyType(cx, name, GetValueType(cx, value));
}

void
TypeObject::markPropertyConfigured(JSContext *cx, jsid id)
{
    AutoEnterTypeInference enter(cx);

    id = MakeTypeId(cx, id);

    TypeSet *types = getProperty(cx, id, true);
    if (types)
        types->setOwnProperty(cx, true);
}

void
TypeObject::markSlotReallocation(JSContext *cx)
{
    /*
     * Constraints listening for reallocation will trigger recompilation if
     * newObjectState is invoked with 'force' set to true.
     */
    AutoEnterTypeInference enter(cx);
    TypeSet *types = maybeGetProperty(cx, JSID_EMPTY);
    if (types) {
        TypeConstraint *constraint = types->constraintList;
        while (constraint) {
            constraint->newObjectState(cx, this, true);
            constraint = constraint->next;
        }
    }
}

void
TypeObject::setFlags(JSContext *cx, TypeObjectFlags flags)
{
    if ((this->flags & flags) == flags)
        return;

    AutoEnterTypeInference enter(cx);

    /* Sets of CREATED_ARGUMENTS should go through MarkArgumentsCreated. */
    JS_ASSERT_IF(flags & OBJECT_FLAG_CREATED_ARGUMENTS,
                 (flags & OBJECT_FLAG_UNINLINEABLE) && functionScript->createdArgs);

    this->flags |= flags;

    InferSpew(ISpewOps, "%s: setFlags %u", name(), flags);

    ObjectStateChange(cx, this, false, false);
}

void
TypeObject::markUnknown(JSContext *cx)
{
    AutoEnterTypeInference enter(cx);

    JS_ASSERT(cx->compartment->activeInference);
    JS_ASSERT(!unknownProperties());

    InferSpew(ISpewOps, "UnknownProperties: %s", name());

    ObjectStateChange(cx, this, true, true);

    /*
     * Existing constraints may have already been added to this object, which we need
     * to do the right thing for. We can't ensure that we will mark all unknown
     * objects before they have been accessed, as the __proto__ of a known object
     * could be dynamically set to an unknown object, and we can decide to ignore
     * properties of an object during analysis (i.e. hashmaps). Adding unknown for
     * any properties accessed already accounts for possible values read from them.
     */

    unsigned count = getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        Property *prop = getProperty(i);
        if (prop) {
            prop->types.addType(cx, Type::UnknownType());
            prop->types.setOwnProperty(cx, true);
        }
    }
}

void
TypeObject::clearNewScript(JSContext *cx)
{
    JS_ASSERT(!(flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED));
    flags |= OBJECT_FLAG_NEW_SCRIPT_CLEARED;

    /*
     * It is possible for the object to not have a new script yet but to have
     * one added in the future. When analyzing properties of new scripts we mix
     * in adding constraints to trigger clearNewScript with changes to the
     * type sets themselves (from breakTypeBarriers). It is possible that we
     * could trigger one of these constraints before AnalyzeNewScriptProperties
     * has finished, in which case we want to make sure that call fails.
     */
    if (!newScript)
        return;

    AutoEnterTypeInference enter(cx);

    /*
     * Any definite properties we added due to analysis of the new script when
     * the type object was created are now invalid: objects with the same type
     * can be created by using 'new' on a different script or through some
     * other mechanism (e.g. Object.create). Rather than clear out the definite
     * bits on the object's properties, just mark such properties as having
     * been deleted/reconfigured, which will have the same effect on JITs
     * wanting to use the definite bits to optimize property accesses.
     */
    for (unsigned i = 0; i < getPropertyCount(); i++) {
        Property *prop = getProperty(i);
        if (!prop)
            continue;
        if (prop->types.isDefiniteProperty())
            prop->types.setOwnProperty(cx, true);
    }

    /*
     * If we cleared the new script while in the middle of initializing an
     * object, it will still have the new script's shape and reflect the no
     * longer correct state of the object once its initialization is completed.
     * We can't really detect the possibility of this statically, but the new
     * script keeps track of where each property is initialized so we can walk
     * the stack and fix up any such objects.
     */
    for (FrameRegsIter iter(cx); !iter.done(); ++iter) {
        StackFrame *fp = iter.fp();
        if (fp->isScriptFrame() && fp->isConstructing() &&
            fp->script() == newScript->script && fp->thisValue().isObject() &&
            !fp->thisValue().toObject().hasLazyType() &&
            fp->thisValue().toObject().type() == this) {
            JSObject *obj = &fp->thisValue().toObject();
            jsbytecode *pc = iter.pc();

            /* Whether all identified 'new' properties have been initialized. */
            bool finished = false;

            /* If not finished, number of properties that have been added. */
            uint32 numProperties = 0;

            /*
             * If non-zero, we are scanning initializers in a call which has
             * already finished.
             */
            size_t depth = 0;

            for (TypeNewScript::Initializer *init = newScript->initializerList;; init++) {
                uint32 offset = uint32(pc - fp->script()->code);
                if (init->kind == TypeNewScript::Initializer::SETPROP) {
                    if (!depth && init->offset > offset) {
                        /* Advanced past all properties which have been initialized. */
                        break;
                    }
                    numProperties++;
                } else if (init->kind == TypeNewScript::Initializer::FRAME_PUSH) {
                    if (depth) {
                        depth++;
                    } else if (init->offset > offset) {
                        /* Advanced past all properties which have been initialized. */
                        break;
                    } else if (init->offset == offset) {
                        StackSegment &seg = cx->stack.space().containingSegment(fp);
                        if (seg.maybefp() == fp)
                            break;
                        fp = seg.computeNextFrame(fp);
                        pc = fp->pcQuadratic(cx->stack);
                    } else {
                        /* This call has already finished. */
                        depth = 1;
                    }
                } else if (init->kind == TypeNewScript::Initializer::FRAME_POP) {
                    if (depth) {
                        depth--;
                    } else {
                        /* This call has not finished yet. */
                        break;
                    }
                } else {
                    JS_ASSERT(init->kind == TypeNewScript::Initializer::DONE);
                    finished = true;
                    break;
                }
            }

            if (!finished)
                obj->rollbackProperties(cx, numProperties);
        }
    }

    cx->free_(newScript);
    newScript = NULL;
}

void
TypeObject::print(JSContext *cx)
{
    printf("%s : %s", name(), proto ? TypeString(Type::ObjectType(proto)) : "(null)");

    if (unknownProperties()) {
        printf(" unknown");
    } else {
        if (!hasAnyFlags(OBJECT_FLAG_NON_PACKED_ARRAY))
            printf(" packed");
        if (!hasAnyFlags(OBJECT_FLAG_NON_DENSE_ARRAY))
            printf(" dense");
        if (!hasAnyFlags(OBJECT_FLAG_NON_TYPED_ARRAY))
            printf(" typed");
        if (hasAnyFlags(OBJECT_FLAG_UNINLINEABLE))
            printf(" uninlineable");
        if (hasAnyFlags(OBJECT_FLAG_SPECIAL_EQUALITY))
            printf(" specialEquality");
        if (hasAnyFlags(OBJECT_FLAG_ITERATED))
            printf(" iterated");
    }

    unsigned count = getPropertyCount();

    if (count == 0) {
        printf(" {}\n");
        return;
    }

    printf(" {");

    for (unsigned i = 0; i < count; i++) {
        Property *prop = getProperty(i);
        if (prop) {
            printf("\n    %s:", TypeIdString(prop->id));
            prop->types.print(cx);
        }
    }

    printf("\n}\n");
}

/////////////////////////////////////////////////////////////////////
// Type Analysis
/////////////////////////////////////////////////////////////////////

/*
 * If the bytecode immediately following code/pc is a test of the value
 * pushed by code, that value should be marked as possibly void.
 */
static inline bool
CheckNextTest(jsbytecode *pc)
{
    jsbytecode *next = pc + GetBytecodeLength(pc);
    switch ((JSOp)*next) {
      case JSOP_IFEQ:
      case JSOP_IFNE:
      case JSOP_NOT:
      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
      case JSOP_TYPEOF:
      case JSOP_TYPEOFEXPR:
        return true;
      default:
        /* TRAP ok here */
        return false;
    }
}

static inline TypeObject *
GetInitializerType(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    if (!script->hasGlobal())
        return NULL;

    UntrapOpcode untrap(cx, script, pc);

    JSOp op = JSOp(*pc);
    JS_ASSERT(op == JSOP_NEWARRAY || op == JSOP_NEWOBJECT || op == JSOP_NEWINIT);

    bool isArray = (op == JSOP_NEWARRAY || (op == JSOP_NEWINIT && pc[1] == JSProto_Array));
    return script->types.initObject(cx, pc, isArray ? JSProto_Array : JSProto_Object);
}

/* Analyze type information for a single bytecode. */
bool
ScriptAnalysis::analyzeTypesBytecode(JSContext *cx, unsigned offset,
                                     TypeInferenceState &state)
{
    JS_ASSERT(!script->isUncachedEval);

    jsbytecode *pc = script->code + offset;
    JSOp op = (JSOp)*pc;

    Bytecode &code = getCode(offset);
    JS_ASSERT(!code.pushedTypes);

    InferSpew(ISpewOps, "analyze: #%u:%05u", script->id(), offset);

    unsigned defCount = GetDefCount(script, offset);
    if (ExtendedDef(pc))
        defCount++;

    TypeSet *pushed = ArenaArray<TypeSet>(cx->compartment->pool, defCount);
    if (!pushed)
        return false;
    PodZero(pushed, defCount);
    code.pushedTypes = pushed;

    /*
     * Add phi nodes introduced at this point to the list of all phi nodes in
     * the script. Types for these are not generated until after the script has
     * been processed, as types can flow backwards into phi nodes and the
     * source sets may not exist if we try to process these eagerly.
     */
    if (code.newValues) {
        SlotValue *newv = code.newValues;
        while (newv->slot) {
            if (newv->value.kind() != SSAValue::PHI || newv->value.phiOffset() != offset) {
                newv++;
                continue;
            }

            /*
             * The phi nodes at join points should all be unique, and every phi
             * node created should be in the phiValues list on some bytecode.
             */
            if (!state.phiNodes.append(newv->value.phiNode()))
                return false;
            TypeSet &types = newv->value.phiNode()->types;
            types.setIntermediate();
            InferSpew(ISpewOps, "typeSet: %sT%p%s phi #%u:%05u:%u",
                      InferSpewColor(&types), &types, InferSpewColorReset(),
                      script->id(), offset, newv->slot);
            newv++;
        }
    }

    /*
     * Treat decomposed ops as no-ops, we will analyze the decomposed version
     * instead. (We do, however, need to look at introduced phi nodes).
     */
    if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
        return true;

    for (unsigned i = 0; i < defCount; i++) {
        pushed[i].setIntermediate();
        InferSpew(ISpewOps, "typeSet: %sT%p%s pushed%u #%u:%05u",
                  InferSpewColor(&pushed[i]), &pushed[i], InferSpewColorReset(),
                  i, script->id(), offset);
    }

    /* Add type constraints for the various opcodes. */
    switch (op) {

        /* Nop bytecodes. */
      case JSOP_POP:
      case JSOP_NOP:
      case JSOP_TRACE:
      case JSOP_NOTRACE:
      case JSOP_GOTO:
      case JSOP_GOTOX:
      case JSOP_IFEQ:
      case JSOP_IFEQX:
      case JSOP_IFNE:
      case JSOP_IFNEX:
      case JSOP_LINENO:
      case JSOP_DEFCONST:
      case JSOP_LEAVEWITH:
      case JSOP_LEAVEBLOCK:
      case JSOP_RETRVAL:
      case JSOP_ENDITER:
      case JSOP_THROWING:
      case JSOP_GOSUB:
      case JSOP_GOSUBX:
      case JSOP_RETSUB:
      case JSOP_CONDSWITCH:
      case JSOP_DEFAULT:
      case JSOP_DEFAULTX:
      case JSOP_POPN:
      case JSOP_UNBRANDTHIS:
      case JSOP_STARTXML:
      case JSOP_STARTXMLEXPR:
      case JSOP_DEFXMLNS:
      case JSOP_SHARPINIT:
      case JSOP_INDEXBASE:
      case JSOP_INDEXBASE1:
      case JSOP_INDEXBASE2:
      case JSOP_INDEXBASE3:
      case JSOP_RESETBASE:
      case JSOP_RESETBASE0:
      case JSOP_BLOCKCHAIN:
      case JSOP_NULLBLOCKCHAIN:
      case JSOP_POPV:
      case JSOP_DEBUGGER:
      case JSOP_SETCALL:
      case JSOP_TABLESWITCH:
      case JSOP_TABLESWITCHX:
      case JSOP_LOOKUPSWITCH:
      case JSOP_LOOKUPSWITCHX:
      case JSOP_TRY:
        break;

        /* Bytecodes pushing values of known type. */
      case JSOP_VOID:
      case JSOP_PUSH:
        pushed[0].addType(cx, Type::UndefinedType());
        break;
      case JSOP_ZERO:
      case JSOP_ONE:
      case JSOP_INT8:
      case JSOP_INT32:
      case JSOP_UINT16:
      case JSOP_UINT24:
      case JSOP_BITAND:
      case JSOP_BITOR:
      case JSOP_BITXOR:
      case JSOP_BITNOT:
      case JSOP_RSH:
      case JSOP_LSH:
      case JSOP_URSH:
        pushed[0].addType(cx, Type::Int32Type());
        break;
      case JSOP_FALSE:
      case JSOP_TRUE:
      case JSOP_EQ:
      case JSOP_NE:
      case JSOP_LT:
      case JSOP_LE:
      case JSOP_GT:
      case JSOP_GE:
      case JSOP_NOT:
      case JSOP_STRICTEQ:
      case JSOP_STRICTNE:
      case JSOP_IN:
      case JSOP_INSTANCEOF:
      case JSOP_DELDESC:
        pushed[0].addType(cx, Type::BooleanType());
        break;
      case JSOP_DOUBLE:
        pushed[0].addType(cx, Type::DoubleType());
        break;
      case JSOP_STRING:
      case JSOP_TYPEOF:
      case JSOP_TYPEOFEXPR:
      case JSOP_QNAMEPART:
      case JSOP_XMLTAGEXPR:
      case JSOP_TOATTRVAL:
      case JSOP_ADDATTRNAME:
      case JSOP_ADDATTRVAL:
      case JSOP_XMLELTEXPR:
        pushed[0].addType(cx, Type::StringType());
        break;
      case JSOP_NULL:
        pushed[0].addType(cx, Type::NullType());
        break;

      case JSOP_REGEXP:
        if (script->hasGlobal()) {
            TypeObject *object = script->types.standardType(cx, JSProto_RegExp);
            if (!object)
                return false;
            pushed[0].addType(cx, Type::ObjectType(object));
        } else {
            pushed[0].addType(cx, Type::UnknownType());
        }
        break;

      case JSOP_OBJECT: {
        JSObject *obj = GetScriptObject(cx, script, pc, 0);
        pushed[0].addType(cx, Type::ObjectType(obj));
        break;
      }

      case JSOP_STOP:
        /* If a stop is reachable then the return type may be void. */
        if (script->fun)
            script->types.returnTypes()->addType(cx, Type::UndefinedType());
        break;

      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
        /* OR/AND push whichever operand determined the result. */
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_DUP:
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[1]);
        break;

      case JSOP_DUP2:
        poppedTypes(pc, 1)->addSubset(cx, &pushed[0]);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[1]);
        poppedTypes(pc, 1)->addSubset(cx, &pushed[2]);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[3]);
        break;

      case JSOP_SWAP:
      case JSOP_PICK: {
        unsigned pickedDepth = (op == JSOP_SWAP ? 1 : pc[1]);
        /* The last popped value is the last pushed. */
        poppedTypes(pc, pickedDepth)->addSubset(cx, &pushed[pickedDepth]);
        for (unsigned i = 0; i < pickedDepth; i++)
            poppedTypes(pc, i)->addSubset(cx, &pushed[pickedDepth - 1 - i]);
        break;
      }

      case JSOP_GETGLOBAL:
      case JSOP_CALLGLOBAL:
      case JSOP_GETGNAME:
      case JSOP_CALLGNAME: {
        jsid id;
        if (op == JSOP_GETGLOBAL || op == JSOP_CALLGLOBAL)
            id = GetGlobalId(cx, script, pc);
        else
            id = GetAtomId(cx, script, pc, 0);

        TypeSet *seen = script->types.bytecodeTypes(pc);
        seen->addSubset(cx, &pushed[0]);

        /*
         * Normally we rely on lazy standard class initialization to fill in
         * the types of global properties the script can access. In a few cases
         * the method JIT will bypass this, and we need to add the types direclty.
         */
        if (id == ATOM_TO_JSID(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]))
            seen->addType(cx, Type::UndefinedType());
        if (id == ATOM_TO_JSID(cx->runtime->atomState.NaNAtom))
            seen->addType(cx, Type::DoubleType());
        if (id == ATOM_TO_JSID(cx->runtime->atomState.InfinityAtom))
            seen->addType(cx, Type::DoubleType());

        /* Handle as a property access. */
        PropertyAccess(cx, script, pc, script->global()->getType(cx), false, seen, id);

        if (op == JSOP_CALLGLOBAL || op == JSOP_CALLGNAME) {
            pushed[1].addType(cx, Type::UnknownType());
            pushed[0].addPropagateThis(cx, script, pc, Type::UnknownType());
        }

        if (CheckNextTest(pc))
            pushed[0].addType(cx, Type::UndefinedType());
        break;
      }

      case JSOP_NAME:
      case JSOP_CALLNAME: {
        /*
         * The first value pushed by NAME/CALLNAME must always be added to the
         * bytecode types, we don't model these opcodes with inference.
         */
        TypeSet *seen = script->types.bytecodeTypes(pc);
        addTypeBarrier(cx, pc, seen, Type::UnknownType());
        seen->addSubset(cx, &pushed[0]);
        if (op == JSOP_CALLNAME) {
            pushed[1].addType(cx, Type::UnknownType());
            pushed[0].addPropagateThis(cx, script, pc, Type::UnknownType());
        }
        break;
      }

      case JSOP_BINDGNAME:
      case JSOP_BINDNAME:
        break;

      case JSOP_SETGNAME: {
        jsid id = GetAtomId(cx, script, pc, 0);
        PropertyAccess(cx, script, pc, script->global()->getType(cx),
                       true, poppedTypes(pc, 0), id);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;
      }

      case JSOP_SETNAME:
      case JSOP_SETCONST:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_GETXPROP: {
        TypeSet *seen = script->types.bytecodeTypes(pc);
        addTypeBarrier(cx, pc, seen, Type::UnknownType());
        seen->addSubset(cx, &pushed[0]);
        break;
      }

      case JSOP_GETFCSLOT:
      case JSOP_CALLFCSLOT: {
        unsigned index = GET_UINT16(pc);
        TypeSet *types = script->types.upvarTypes(index);
        types->addSubset(cx, &pushed[0]);
        if (op == JSOP_CALLFCSLOT) {
            pushed[1].addType(cx, Type::UndefinedType());
            pushed[0].addPropagateThis(cx, script, pc, Type::UndefinedType());
        }
        break;
      }

      case JSOP_GETARG:
      case JSOP_CALLARG:
      case JSOP_GETLOCAL:
      case JSOP_CALLLOCAL: {
        uint32 slot = GetBytecodeSlot(script, pc);
        if (trackSlot(slot)) {
            /*
             * Normally these opcodes don't pop anything, but they are given
             * an extended use holding the variable's SSA value before the
             * access. Use the types from here.
             */
            poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        } else if (slot < TotalSlots(script)) {
            TypeSet *types = script->types.slotTypes(slot);
            types->addSubset(cx, &pushed[0]);
        } else {
            /* Local 'let' variable. Punt on types for these, for now. */
            pushed[0].addType(cx, Type::UnknownType());
        }
        if (op == JSOP_CALLARG || op == JSOP_CALLLOCAL) {
            pushed[1].addType(cx, Type::UndefinedType());
            pushed[0].addPropagateThis(cx, script, pc, Type::UndefinedType());
        }
        break;
      }

      case JSOP_SETARG:
      case JSOP_SETLOCAL:
      case JSOP_SETLOCALPOP: {
        uint32 slot = GetBytecodeSlot(script, pc);
        if (!trackSlot(slot) && slot < TotalSlots(script)) {
            TypeSet *types = script->types.slotTypes(slot);
            poppedTypes(pc, 0)->addSubset(cx, types);
        }

        /*
         * For assignments to non-escaping locals/args, we don't need to update
         * the possible types of the var, as for each read of the var SSA gives
         * us the writes that could have produced that read.
         */
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;
      }

      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC:
      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC: {
        uint32 slot = GetBytecodeSlot(script, pc);
        if (trackSlot(slot)) {
            poppedTypes(pc, 0)->addArith(cx, &pushed[0]);
        } else if (slot < TotalSlots(script)) {
            TypeSet *types = script->types.slotTypes(slot);
            types->addArith(cx, types);
            types->addSubset(cx, &pushed[0]);
        } else {
            pushed[0].addType(cx, Type::UnknownType());
        }
        break;
      }

      case JSOP_ARGUMENTS: {
        /* Compute a precise type only when we know the arguments won't escape. */
        TypeObject *funType = script->fun->getType(cx);
        if (funType->unknownProperties() || funType->hasAnyFlags(OBJECT_FLAG_CREATED_ARGUMENTS)) {
            pushed[0].addType(cx, Type::UnknownType());
            break;
        }
        TypeSet *types = funType->getProperty(cx, JSID_EMPTY, false);
        if (!types)
            break;
        types->addLazyArguments(cx, &pushed[0]);
        pushed[0].addType(cx, Type::LazyArgsType());
        break;
      }

      case JSOP_SETPROP:
      case JSOP_SETMETHOD: {
        jsid id = GetAtomId(cx, script, pc, 0);
        poppedTypes(pc, 1)->addSetProperty(cx, script, pc, poppedTypes(pc, 0), id);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;
      }

      case JSOP_LENGTH:
      case JSOP_GETPROP:
      case JSOP_CALLPROP: {
        jsid id = GetAtomId(cx, script, pc, 0);
        TypeSet *seen = script->types.bytecodeTypes(pc);

        poppedTypes(pc, 0)->addGetProperty(cx, script, pc, seen, id);
        if (op == JSOP_CALLPROP)
            poppedTypes(pc, 0)->addCallProperty(cx, script, pc, id);

        seen->addSubset(cx, &pushed[0]);
        if (op == JSOP_CALLPROP)
            poppedTypes(pc, 0)->addFilterPrimitives(cx, &pushed[1], true);
        if (CheckNextTest(pc))
            pushed[0].addType(cx, Type::UndefinedType());
        break;
      }

      /*
       * We only consider ELEM accesses on integers below. Any element access
       * which is accessing a non-integer property must be monitored.
       */

      case JSOP_GETELEM:
      case JSOP_CALLELEM: {
        TypeSet *seen = script->types.bytecodeTypes(pc);

        poppedTypes(pc, 1)->addGetProperty(cx, script, pc, seen, JSID_VOID);
        if (op == JSOP_CALLELEM)
            poppedTypes(pc, 1)->addCallProperty(cx, script, pc, JSID_VOID);

        seen->addSubset(cx, &pushed[0]);
        if (op == JSOP_CALLELEM)
            poppedTypes(pc, 1)->addFilterPrimitives(cx, &pushed[1], true);
        if (CheckNextTest(pc))
            pushed[0].addType(cx, Type::UndefinedType());
        break;
      }

      case JSOP_SETELEM:
      case JSOP_SETHOLE:
        poppedTypes(pc, 2)->addSetProperty(cx, script, pc, poppedTypes(pc, 0), JSID_VOID);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_TOID:
        /*
         * This is only used for element inc/dec ops; any id produced which
         * is not an integer must be monitored.
         */
        pushed[0].addType(cx, Type::Int32Type());
        break;

      case JSOP_THIS:
        script->types.thisTypes()->addTransformThis(cx, script, &pushed[0]);
        break;

      case JSOP_RETURN:
      case JSOP_SETRVAL:
        if (script->fun)
            poppedTypes(pc, 0)->addSubset(cx, script->types.returnTypes());
        break;

      case JSOP_ADD:
        poppedTypes(pc, 0)->addArith(cx, &pushed[0], poppedTypes(pc, 1));
        poppedTypes(pc, 1)->addArith(cx, &pushed[0], poppedTypes(pc, 0));
        break;

      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_MOD:
      case JSOP_DIV:
        poppedTypes(pc, 0)->addArith(cx, &pushed[0]);
        poppedTypes(pc, 1)->addArith(cx, &pushed[0]);
        break;

      case JSOP_NEG:
      case JSOP_POS:
        poppedTypes(pc, 0)->addArith(cx, &pushed[0]);
        break;

      case JSOP_LAMBDA:
      case JSOP_LAMBDA_FC:
      case JSOP_DEFFUN:
      case JSOP_DEFFUN_FC:
      case JSOP_DEFLOCALFUN:
      case JSOP_DEFLOCALFUN_FC: {
        unsigned off = (op == JSOP_DEFLOCALFUN || op == JSOP_DEFLOCALFUN_FC) ? SLOTNO_LEN : 0;
        JSObject *obj = GetScriptObject(cx, script, pc, off);

        TypeSet *res = NULL;
        if (op == JSOP_LAMBDA || op == JSOP_LAMBDA_FC) {
            res = &pushed[0];
        } else if (op == JSOP_DEFLOCALFUN || op == JSOP_DEFLOCALFUN_FC) {
            uint32 slot = GetBytecodeSlot(script, pc);
            if (trackSlot(slot)) {
                res = &pushed[0];
            } else {
                /* Should not see 'let' vars here. */
                JS_ASSERT(slot < TotalSlots(script));
                res = script->types.slotTypes(slot);
            }
        }

        if (res) {
            if (script->hasGlobal())
                res->addType(cx, Type::ObjectType(obj));
            else
                res->addType(cx, Type::UnknownType());
        } else {
            cx->compartment->types.monitorBytecode(cx, script, offset);
        }
        break;
      }

      case JSOP_DEFVAR:
        break;

      case JSOP_CALL:
      case JSOP_EVAL:
      case JSOP_FUNCALL:
      case JSOP_FUNAPPLY:
      case JSOP_NEW: {
        TypeSet *seen = script->types.bytecodeTypes(pc);
        seen->addSubset(cx, &pushed[0]);

        /* Construct the base call information about this site. */
        unsigned argCount = GetUseCount(script, offset) - 2;
        TypeCallsite *callsite = ArenaNew<TypeCallsite>(cx->compartment->pool,
                                                        cx, script, pc, op == JSOP_NEW, argCount);
        if (!callsite || (argCount && !callsite->argumentTypes)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            break;
        }
        callsite->thisTypes = poppedTypes(pc, argCount);
        callsite->returnTypes = seen;

        for (unsigned i = 0; i < argCount; i++)
            callsite->argumentTypes[i] = poppedTypes(pc, argCount - 1 - i);

        /*
         * Mark FUNCALL and FUNAPPLY sites as monitored. The method JIT may
         * lower these into normal calls, and we need to make sure the
         * callee's argument types are checked on entry.
         */
        if (op == JSOP_FUNCALL || op == JSOP_FUNAPPLY)
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code);

        poppedTypes(pc, argCount + 1)->addCall(cx, callsite);
        break;
      }

      case JSOP_NEWINIT:
      case JSOP_NEWARRAY:
      case JSOP_NEWOBJECT: {
        TypeObject *initializer = GetInitializerType(cx, script, pc);
        if (script->hasGlobal()) {
            if (!initializer)
                return false;
            pushed[0].addType(cx, Type::ObjectType(initializer));
        } else {
            JS_ASSERT(!initializer);
            pushed[0].addType(cx, Type::UnknownType());
        }
        break;
      }

      case JSOP_ENDINIT:
        break;

      case JSOP_INITELEM: {
        const SSAValue &objv = poppedValue(pc, 2);
        jsbytecode *initpc = script->code + objv.pushedOffset();
        TypeObject *initializer = GetInitializerType(cx, script, initpc);

        if (initializer) {
            pushed[0].addType(cx, Type::ObjectType(initializer));
            if (!initializer->unknownProperties()) {
                /*
                 * Assume the initialized element is an integer. INITELEM can be used
                 * for doubles which don't map to the JSID_VOID property, which must
                 * be caught with dynamic monitoring.
                 */
                TypeSet *types = initializer->getProperty(cx, JSID_VOID, true);
                if (!types)
                    return false;
                if (state.hasGetSet) {
                    types->addType(cx, Type::UnknownType());
                } else if (state.hasHole) {
                    if (!initializer->unknownProperties())
                        initializer->setFlags(cx, OBJECT_FLAG_NON_PACKED_ARRAY);
                } else {
                    poppedTypes(pc, 0)->addSubset(cx, types);
                }
            }
        } else {
            pushed[0].addType(cx, Type::UnknownType());
        }
        state.hasGetSet = false;
        state.hasHole = false;
        break;
      }

      case JSOP_GETTER:
      case JSOP_SETTER:
        state.hasGetSet = true;
        break;

      case JSOP_HOLE:
        state.hasHole = true;
        break;

      case JSOP_INITPROP:
      case JSOP_INITMETHOD: {
        const SSAValue &objv = poppedValue(pc, 1);
        jsbytecode *initpc = script->code + objv.pushedOffset();
        TypeObject *initializer = GetInitializerType(cx, script, initpc);

        if (initializer) {
            pushed[0].addType(cx, Type::ObjectType(initializer));
            if (!initializer->unknownProperties()) {
                jsid id = GetAtomId(cx, script, pc, 0);
                TypeSet *types = initializer->getProperty(cx, id, true);
                if (!types)
                    return false;
                if (id == id___proto__(cx) || id == id_prototype(cx))
                    cx->compartment->types.monitorBytecode(cx, script, offset);
                else if (state.hasGetSet)
                    types->addType(cx, Type::UnknownType());
                else
                    poppedTypes(pc, 0)->addSubset(cx, types);
            }
        } else {
            pushed[0].addType(cx, Type::UnknownType());
        }
        state.hasGetSet = false;
        JS_ASSERT(!state.hasHole);
        break;
      }

      case JSOP_ENTERWITH:
      case JSOP_ENTERBLOCK:
        /*
         * Scope lookups can occur on the values being pushed here. We don't track
         * the value or its properties, and just monitor all name opcodes in the
         * script.
         */
        break;

      case JSOP_ITER: {
        /*
         * Use a per-script type set to unify the possible target types of all
         * 'for in' or 'for each' loops in the script. We need to mark the
         * value pushed by the ITERNEXT appropriately, but don't track the SSA
         * information to connect that ITERNEXT with the appropriate ITER.
         * This loses some precision when a script mixes 'for in' and
         * 'for each' loops together, oh well.
         */
        if (!state.forTypes) {
          state.forTypes = TypeSet::make(cx, "forTypes");
          if (!state.forTypes)
              return false;
        }
        poppedTypes(pc, 0)->addSubset(cx, state.forTypes);

        if (pc[1] & JSITER_FOREACH)
            state.forTypes->addType(cx, Type::UnknownType());

        /*
         * Feed any types from the pushed type set into the forTypes. If a
         * custom __iterator__ hook is added it will appear as an unknown
         * value pushed by this opcode.
         */
        pushed[0].addSubset(cx, state.forTypes);

        break;
      }

      case JSOP_ITERNEXT:
        /*
         * The value bound is a string, unless this is a 'for each' loop or the
         * iterated object is a generator or has an __iterator__ hook, which
         * we'll detect dynamically.
         */
        pushed[0].addType(cx, Type::StringType());
        state.forTypes->add(cx,
            ArenaNew<TypeConstraintGenerator>(cx->compartment->pool, &pushed[0]));
        break;

      case JSOP_MOREITER:
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        pushed[1].addType(cx, Type::BooleanType());
        break;

      case JSOP_ENUMELEM:
      case JSOP_ENUMCONSTELEM:
      case JSOP_ARRAYPUSH:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        break;

      case JSOP_THROW:
        /* There will be a monitor on the bytecode catching the exception. */
        break;

      case JSOP_FINALLY:
        /* Pushes information about whether an exception was thrown. */
        break;

      case JSOP_EXCEPTION:
        pushed[0].addType(cx, Type::UnknownType());
        break;

      case JSOP_DELPROP:
      case JSOP_DELELEM:
      case JSOP_DELNAME:
        pushed[0].addType(cx, Type::BooleanType());
        break;

      case JSOP_LEAVEBLOCKEXPR:
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_CASE:
      case JSOP_CASEX:
        poppedTypes(pc, 1)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_UNBRAND:
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_GENERATOR:
        if (script->fun) {
            if (script->hasGlobal()) {
                TypeObject *object = script->types.standardType(cx, JSProto_Generator);
                if (!object)
                    return false;
                script->types.returnTypes()->addType(cx, Type::ObjectType(object));
            } else {
                script->types.returnTypes()->addType(cx, Type::UnknownType());
            }
        }
        break;

      case JSOP_YIELD:
        pushed[0].addType(cx, Type::UnknownType());
        break;

      case JSOP_CALLXMLNAME:
        pushed[1].addType(cx, Type::UnknownType());
        /* FALLTHROUGH */
      case JSOP_XMLNAME:
        pushed[0].addType(cx, Type::UnknownType());
        break;

      case JSOP_SETXMLNAME:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_BINDXMLNAME:
        break;

      case JSOP_TOXML:
      case JSOP_TOXMLLIST:
      case JSOP_XMLPI:
      case JSOP_XMLCDATA:
      case JSOP_XMLCOMMENT:
      case JSOP_DESCENDANTS:
      case JSOP_TOATTRNAME:
      case JSOP_QNAMECONST:
      case JSOP_QNAME:
      case JSOP_ANYNAME:
      case JSOP_GETFUNNS:
        pushed[0].addType(cx, Type::UnknownType());
        break;

      case JSOP_FILTER:
        /* Note: the second value pushed by filter is a hole, and not modelled. */
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_ENDFILTER:
        poppedTypes(pc, 1)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_DEFSHARP:
        break;

      case JSOP_USESHARP:
        pushed[0].addType(cx, Type::UnknownType());
        break;

      case JSOP_CALLEE:
        if (script->hasGlobal())
            pushed[0].addType(cx, Type::ObjectType(script->fun));
        else
            pushed[0].addType(cx, Type::UnknownType());
        break;

      default:
        /* Display fine-grained debug information first */
        fprintf(stderr, "Unknown bytecode %02x at #%u:%05u\n", op, script->id(), offset);
        TypeFailure(cx, "Unknown bytecode %02x", op);
    }

    return true;
}

void
ScriptAnalysis::analyzeTypes(JSContext *cx)
{
    JS_ASSERT(!ranInference());

    if (OOM()) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    /*
     * Refuse to analyze the types in a script which is compileAndGo but is
     * running against a global with a cleared scope. Per GlobalObject::clear,
     * we won't be running anymore compileAndGo code against the global
     * (moreover, after clearing our analysis results will be wrong for the
     * script and trying to reanalyze here can cause reentrance problems if we
     * try to reinitialize standard classes that were cleared).
     */
    if (script->hasClearedGlobal())
        return;

    if (!ranSSA()) {
        analyzeSSA(cx);
        if (failed())
            return;
    }

    if (!script->types.ensureTypeArray(cx)) {
        setOOM(cx);
        return;
    }

    /*
     * Set this early to avoid reentrance. Any failures are OOMs, and will nuke
     * all types in the compartment.
     */
    ranInference_ = true;

    /* Make sure the initial type set of all local vars includes void. */
    for (unsigned i = 0; i < script->nfixed; i++)
        script->types.localTypes(i)->addType(cx, Type::UndefinedType());

    TypeInferenceState state(cx);

    unsigned offset = 0;
    while (offset < script->length) {
        Bytecode *code = maybeCode(offset);

        jsbytecode *pc = script->code + offset;
        UntrapOpcode untrap(cx, script, pc);

        if (code && !analyzeTypesBytecode(cx, offset, state)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        offset += GetBytecodeLength(pc);
    }

    for (unsigned i = 0; i < state.phiNodes.length(); i++) {
        SSAPhiNode *node = state.phiNodes[i];
        for (unsigned j = 0; j < node->length; j++) {
            const SSAValue &v = node->options[j];
            getValueTypes(v)->addSubset(cx, &node->types);
        }
    }

    /*
     * Replay any dynamic type results which have been generated for the script
     * either because we ran the interpreter some before analyzing or because
     * we are reanalyzing after a GC.
     */
    TypeResult *result = script->types.dynamicList;
    while (result) {
        pushedTypes(result->offset)->addType(cx, result->type);
        result = result->next;
    }

    if (!script->usesArguments || script->createdArgs)
        return;

    /*
     * Do additional analysis to determine whether the arguments object in the
     * script can escape.
     */

    /*
     * Note: don't check for strict mode code here, even though arguments
     * accesses in such scripts will always be deoptimized. These scripts can
     * have a JSOP_ARGUMENTS in their prologue which the usesArguments check
     * above does not account for. We filter in the interpreter and JITs
     * themselves.
     */
    if (script->fun->isHeavyweight() || cx->compartment->debugMode) {
        MarkArgumentsCreated(cx, script);
        return;
    }

    offset = 0;
    while (offset < script->length) {
        Bytecode *code = maybeCode(offset);
        jsbytecode *pc = script->code + offset;

        if (code && JSOp(*pc) == JSOP_ARGUMENTS) {
            Vector<SSAValue> seen(cx);
            if (!followEscapingArguments(cx, SSAValue::PushedValue(offset, 0), &seen)) {
                MarkArgumentsCreated(cx, script);
                return;
            }
        }

        offset += GetBytecodeLength(pc);
    }

    /*
     * The VM is now free to use the arguments in this script lazily. If we end
     * up creating an arguments object for the script in the future or regard
     * the arguments as escaping, we need to walk the stack and replace lazy
     * arguments objects with actual arguments objects.
     */
    script->usedLazyArgs = true;
}

bool
ScriptAnalysis::followEscapingArguments(JSContext *cx, const SSAValue &v, Vector<SSAValue> *seen)
{
    /*
     * trackUseChain is false for initial values of variables, which
     * cannot hold the script's arguments object.
     */
    if (!trackUseChain(v))
        return true;

    for (unsigned i = 0; i < seen->length(); i++) {
        if (v.equals((*seen)[i]))
            return true;
    }
    if (!seen->append(v)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }

    SSAUseChain *use = useChain(v);
    while (use) {
        if (!followEscapingArguments(cx, use, seen))
            return false;
        use = use->next;
    }

    return true;
}

bool
ScriptAnalysis::followEscapingArguments(JSContext *cx, SSAUseChain *use, Vector<SSAValue> *seen)
{
    if (!use->popped)
        return followEscapingArguments(cx, SSAValue::PhiValue(use->offset, use->u.phi), seen);

    jsbytecode *pc = script->code + use->offset;
    uint32 which = use->u.which;

    JSOp op = JSOp(*pc);
    JS_ASSERT(op != JSOP_TRAP);

    if (op == JSOP_POP || op == JSOP_POPN)
        return true;

    /* Allow GETELEM and LENGTH on arguments objects that don't escape. */

    /*
     * Note: if the element index is not an integer we will mark the arguments
     * as escaping at the access site.
     */
    if (op == JSOP_GETELEM && which == 1)
        return true;

    if (op == JSOP_LENGTH)
        return true;

    /* Allow assignments to non-closed locals (but not arguments). */

    if (op == JSOP_SETLOCAL) {
        uint32 slot = GetBytecodeSlot(script, pc);
        if (!trackSlot(slot))
            return false;
        if (!followEscapingArguments(cx, SSAValue::PushedValue(use->offset, 0), seen))
            return false;
        return followEscapingArguments(cx, SSAValue::WrittenVar(slot, use->offset), seen);
    }

    if (op == JSOP_GETLOCAL)
        return followEscapingArguments(cx, SSAValue::PushedValue(use->offset, 0), seen);

    return false;
}

/*
 * Persistent constraint clearing out newScript and definite properties from
 * an object should a property on another object get a setter.
 */
class TypeConstraintClearDefiniteSetter : public TypeConstraint
{
public:
    TypeObject *object;

    TypeConstraintClearDefiniteSetter(TypeObject *object)
        : TypeConstraint("clearDefiniteSetter"), object(object)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type) {
        if (!object->newScript)
            return;
        /*
         * Clear out the newScript shape and definite property information from
         * an object if the source type set could be a setter (its type set
         * becomes unknown).
         */
        if (!(object->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED) && type.isUnknown())
            object->clearNewScript(cx);
    }
};

/*
 * Constraint which clears definite properties on an object should a type set
 * contain any types other than a single object.
 */
class TypeConstraintClearDefiniteSingle : public TypeConstraint
{
public:
    TypeObject *object;

    TypeConstraintClearDefiniteSingle(TypeObject *object)
        : TypeConstraint("clearDefiniteSingle"), object(object)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type) {
        if (object->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED)
            return;

        if (source->baseFlags() || source->getObjectCount() > 1)
            object->clearNewScript(cx);
    }
};

static bool
AnalyzeNewScriptProperties(JSContext *cx, TypeObject *type, JSScript *script, JSObject **pbaseobj,
                           Vector<TypeNewScript::Initializer> *initializerList)
{
    /*
     * When invoking 'new' on the specified script, try to find some properties
     * which will definitely be added to the created object before it has a
     * chance to escape and be accessed elsewhere.
     *
     * Returns true if the entire script was analyzed (pbaseobj has been
     * preserved), false if we had to bail out part way through (pbaseobj may
     * have been cleared).
     */

    if (initializerList->length() > 50) {
        /*
         * Bail out on really long initializer lists (far longer than maximum
         * number of properties we can track), we may be recursing.
         */
        return false;
    }

    if (!script->ensureRanInference(cx)) {
        *pbaseobj = NULL;
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }
    ScriptAnalysis *analysis = script->analysis(cx);

    /*
     * Offset of the last bytecode which popped 'this' and which we have
     * processed. For simplicity, we scan for places where 'this' is pushed
     * and immediately analyze the place where that pushed value is popped.
     * This runs the risk of doing things out of order, if the script looks
     * something like 'this.f  = (this.g = ...)', so we watch and bail out if
     * a 'this' is pushed before the previous 'this' value was popped.
     */
    uint32 lastThisPopped = 0;

    unsigned nextOffset = 0;
    while (nextOffset < script->length) {
        unsigned offset = nextOffset;
        jsbytecode *pc = script->code + offset;
        UntrapOpcode untrap(cx, script, pc);

        JSOp op = JSOp(*pc);

        nextOffset += GetBytecodeLength(pc);

        Bytecode *code = analysis->maybeCode(pc);
        if (!code)
            continue;

        /*
         * End analysis after the first return statement from the script,
         * returning success if the return is unconditional.
         */
        if (op == JSOP_RETURN || op == JSOP_STOP || op == JSOP_RETRVAL) {
            if (offset < lastThisPopped) {
                *pbaseobj = NULL;
                return false;
            }
            return code->unconditional;
        }

        /* 'this' can escape through a call to eval. */
        if (op == JSOP_EVAL) {
            if (offset < lastThisPopped)
                *pbaseobj = NULL;
            return false;
        }

        /*
         * We are only interested in places where 'this' is popped. The new
         * 'this' value cannot escape and be accessed except through such uses.
         */
        if (op != JSOP_THIS)
            continue;

        SSAValue thisv = SSAValue::PushedValue(offset, 0);
        SSAUseChain *uses = analysis->useChain(thisv);

        JS_ASSERT(uses);
        if (uses->next || !uses->popped) {
            /* 'this' value popped in more than one place. */
            return false;
        }

        /* Maintain ordering property on how 'this' is used, as described above. */
        if (offset < lastThisPopped) {
            *pbaseobj = NULL;
            return false;
        }
        lastThisPopped = uses->offset;

        /* Only handle 'this' values popped in unconditional code. */
        Bytecode *poppedCode = analysis->maybeCode(uses->offset);
        if (!poppedCode || !poppedCode->unconditional)
            return false;

        pc = script->code + uses->offset;
        UntrapOpcode untrapUse(cx, script, pc);

        op = JSOp(*pc);

        JSObject *obj = *pbaseobj;

        if (op == JSOP_SETPROP && uses->u.which == 1) {
            /*
             * Don't use GetAtomId here, we need to watch for SETPROP on
             * integer properties and bail out. We can't mark the aggregate
             * JSID_VOID type property as being in a definite slot.
             */
            unsigned index = js_GetIndexFromBytecode(cx, script, pc, 0);
            jsid id = ATOM_TO_JSID(script->getAtom(index));
            if (MakeTypeId(cx, id) != id)
                return false;
            if (id == id_prototype(cx) || id == id___proto__(cx) || id == id_constructor(cx))
                return false;

            unsigned slotSpan = obj->slotSpan();
            if (!DefineNativeProperty(cx, obj, id, UndefinedValue(), NULL, NULL,
                                      JSPROP_ENUMERATE, 0, 0, DNP_SKIP_TYPE)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                *pbaseobj = NULL;
                return false;
            }

            if (obj->inDictionaryMode()) {
                *pbaseobj = NULL;
                return false;
            }

            if (obj->slotSpan() == slotSpan) {
                /* Set a duplicate property. */
                return false;
            }

            TypeNewScript::Initializer setprop(TypeNewScript::Initializer::SETPROP, uses->offset);
            if (!initializerList->append(setprop)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                *pbaseobj = NULL;
                return false;
            }

            if (obj->slotSpan() >= (TYPE_FLAG_DEFINITE_MASK >> TYPE_FLAG_DEFINITE_SHIFT)) {
                /* Maximum number of definite properties added. */
                return false;
            }

            /*
             * Ensure that if the properties named here could have a setter in
             * the direct prototype (and thus its transitive prototypes), the
             * definite properties get cleared from the shape.
             */
            TypeObject *parentObject = type->proto->getType(cx);
            if (parentObject->unknownProperties())
                return false;
            TypeSet *parentTypes = parentObject->getProperty(cx, id, false);
            if (!parentTypes || parentTypes->unknown())
                return false;
            parentObject->getFromPrototypes(cx, id, parentTypes);
            parentTypes->add(cx,
                ArenaNew<TypeConstraintClearDefiniteSetter>(cx->compartment->pool, type));
        } else if (op == JSOP_FUNCALL && uses->u.which == GET_ARGC(pc) - 1) {
            /*
             * Passed as the first parameter to Function.call. Follow control
             * into the callee, and add any definite properties it assigns to
             * the object as well. :TODO: This is narrow pattern matching on
             * the inheritance patterns seen in the v8-deltablue benchmark, and
             * needs robustness against other ways initialization can cross
             * script boundaries.
             *
             * Add constraints ensuring we are calling Function.call on a
             * particular script, removing definite properties from the result
             */

            /* Callee/this must have been pushed by a CALLPROP. */
            SSAValue calleev = analysis->poppedValue(pc, GET_ARGC(pc) + 1);
            if (calleev.kind() != SSAValue::PUSHED)
                return false;
            jsbytecode *calleepc = script->code + calleev.pushedOffset();
            UntrapOpcode untrapCallee(cx, script, calleepc);
            if (JSOp(*calleepc) != JSOP_CALLPROP || calleev.pushedIndex() != 0)
                return false;

            /*
             * This code may not have run yet, break any type barriers involved
             * in performing the call (for the greater good!).
             */
            analysis->breakTypeBarriersSSA(cx, analysis->poppedValue(calleepc, 0));
            analysis->breakTypeBarriers(cx, calleepc - script->code, true);

            TypeSet *funcallTypes = analysis->pushedTypes(calleepc, 0);
            TypeSet *scriptTypes = analysis->pushedTypes(calleepc, 1);

            /* Need to definitely be calling Function.call on a specific script. */
            JSObject *funcallObj = funcallTypes->getSingleton(cx, false);
            JSObject *scriptObj = scriptTypes->getSingleton(cx, false);
            if (!funcallObj || !scriptObj || !scriptObj->isFunction() ||
                !scriptObj->getFunctionPrivate()->isInterpreted()) {
                return false;
            }

            JSScript *functionScript = scriptObj->getFunctionPrivate()->script();

            /*
             * Generate constraints to clear definite properties from the type
             * should the Function.call or callee itself change in the future.
             */
            analysis->pushedTypes(calleev.pushedOffset(), 0)->add(cx,
                ArenaNew<TypeConstraintClearDefiniteSingle>(cx->compartment->pool, type));
            analysis->pushedTypes(calleev.pushedOffset(), 1)->add(cx,
                ArenaNew<TypeConstraintClearDefiniteSingle>(cx->compartment->pool, type));

            TypeNewScript::Initializer pushframe(TypeNewScript::Initializer::FRAME_PUSH, uses->offset);
            if (!initializerList->append(pushframe)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                *pbaseobj = NULL;
                return false;
            }

            if (!AnalyzeNewScriptProperties(cx, type, functionScript,
                                            pbaseobj, initializerList)) {
                return false;
            }

            TypeNewScript::Initializer popframe(TypeNewScript::Initializer::FRAME_POP, 0);
            if (!initializerList->append(popframe)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                *pbaseobj = NULL;
                return false;
            }

            /*
             * The callee never lets the 'this' value escape, continue looking
             * for definite properties in the remainder of this script.
             */
        } else {
            /* Unhandled use of 'this'. */
            return false;
        }
    }

    /* Will have hit a STOP or similar, unless the script always throws. */
    return true;
}

/*
 * Either make the newScript information for type when it is constructed
 * by the specified script, or regenerate the constraints for an existing
 * newScript on the type after they were cleared by a GC.
 */
static void
CheckNewScriptProperties(JSContext *cx, TypeObject *type, JSScript *script)
{
    if (type->unknownProperties())
        return;

    /* Strawman object to add properties to and watch for duplicates. */
    JSObject *baseobj = NewBuiltinClassInstance(cx, &js_ObjectClass, gc::FINALIZE_OBJECT16);
    if (!baseobj) {
        if (type->newScript)
            type->clearNewScript(cx);
        return;
    }

    Vector<TypeNewScript::Initializer> initializerList(cx);
    AnalyzeNewScriptProperties(cx, type, script, &baseobj, &initializerList);
    if (!baseobj || baseobj->slotSpan() == 0 || !!(type->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED)) {
        if (type->newScript)
            type->clearNewScript(cx);
        return;
    }

    /*
     * If the type already has a new script, we are just regenerating the type
     * constraints and don't need to make another TypeNewScript. Make sure that
     * the properties added to baseobj match the type's definite properties.
     */
    if (type->newScript) {
        if (!type->matchDefiniteProperties(baseobj))
            type->clearNewScript(cx);
        return;
    }

    gc::FinalizeKind kind = gc::GetGCObjectKind(baseobj->slotSpan());

    /* We should not have overflowed the maximum number of fixed slots for an object. */
    JS_ASSERT(gc::GetGCKindSlots(kind) >= baseobj->slotSpan());

    TypeNewScript::Initializer done(TypeNewScript::Initializer::DONE, 0);

    /*
     * The base object was created with a different type and
     * finalize kind than we will use for subsequent new objects.
     * Generate an object with the appropriate final shape.
     */
    baseobj = NewReshapedObject(cx, type, baseobj->getParent(), kind,
                                baseobj->lastProperty());
    if (!baseobj ||
        !type->addDefiniteProperties(cx, baseobj) ||
        !initializerList.append(done)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    size_t numBytes = sizeof(TypeNewScript)
                    + (initializerList.length() * sizeof(TypeNewScript::Initializer));
    type->newScript = (TypeNewScript *) cx->calloc_(numBytes);
    if (!type->newScript) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    type->newScript->script = script;
    type->newScript->finalizeKind = unsigned(kind);
    type->newScript->shape = baseobj->lastProperty();

    type->newScript->initializerList = (TypeNewScript::Initializer *)
        ((char *) type->newScript + sizeof(TypeNewScript));
    PodCopy(type->newScript->initializerList, initializerList.begin(), initializerList.length());
}

/////////////////////////////////////////////////////////////////////
// Printing
/////////////////////////////////////////////////////////////////////

void
ScriptAnalysis::printTypes(JSContext *cx)
{
    AutoEnterAnalysis enter(cx);
    TypeCompartment *compartment = &script->compartment->types;

    /*
     * Check if there are warnings for used values with unknown types, and build
     * statistics about the size of type sets found for stack values.
     */
    for (unsigned offset = 0; offset < script->length; offset++) {
        if (!maybeCode(offset))
            continue;

        jsbytecode *pc = script->code + offset;
        UntrapOpcode untrap(cx, script, pc);

        if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
            continue;

        unsigned defCount = GetDefCount(script, offset);
        if (!defCount)
            continue;

        for (unsigned i = 0; i < defCount; i++) {
            TypeSet *types = pushedTypes(offset, i);

            if (types->unknown()) {
                compartment->typeCountOver++;
                continue;
            }

            unsigned typeCount = 0;

            if (types->hasAnyFlag(TYPE_FLAG_ANYOBJECT) || types->getObjectCount() != 0)
                typeCount++;
            for (TypeFlags flag = 1; flag < TYPE_FLAG_ANYOBJECT; flag <<= 1) {
                if (types->hasAnyFlag(flag))
                    typeCount++;
            }

            /*
             * Adjust the type counts for floats: values marked as floats
             * are also marked as ints by the inference, but for counting
             * we don't consider these to be separate types.
             */
            if (types->hasAnyFlag(TYPE_FLAG_DOUBLE)) {
                JS_ASSERT(types->hasAnyFlag(TYPE_FLAG_INT32));
                typeCount--;
            }

            if (typeCount > TypeCompartment::TYPE_COUNT_LIMIT) {
                compartment->typeCountOver++;
            } else if (typeCount == 0) {
                /* Ignore values without types, this may be unreached code. */
            } else {
                compartment->typeCounts[typeCount-1]++;
            }
        }
    }

#ifdef DEBUG

    if (script->fun)
        printf("Function");
    else if (script->isCachedEval || script->isUncachedEval)
        printf("Eval");
    else
        printf("Main");
    printf(" #%u %s (line %d):\n", script->id(), script->filename, script->lineno);

    printf("locals:");
    printf("\n    return:");
    script->types.returnTypes()->print(cx);
    printf("\n    this:");
    script->types.thisTypes()->print(cx);

    for (unsigned i = 0; script->fun && i < script->fun->nargs; i++) {
        printf("\n    arg%u:", i);
        script->types.argTypes(i)->print(cx);
    }
    for (unsigned i = 0; i < script->nfixed; i++) {
        if (!trackSlot(LocalSlot(script, i))) {
            printf("\n    local%u:", i);
            script->types.localTypes(i)->print(cx);
        }
    }
    for (unsigned i = 0; i < script->bindings.countUpvars(); i++) {
        printf("\n    upvar%u:", i);
        script->types.upvarTypes(i)->print(cx);
    }
    printf("\n");

    for (unsigned offset = 0; offset < script->length; offset++) {
        if (!maybeCode(offset))
            continue;

        jsbytecode *pc = script->code + offset;
        UntrapOpcode untrap(cx, script, pc);

        PrintBytecode(cx, script, pc);

        if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
            continue;

        if (js_CodeSpec[*pc].format & JOF_TYPESET) {
            TypeSet *types = script->types.bytecodeTypes(pc);
            printf("  typeset %d:", (int) (types - script->types.typeArray));
            types->print(cx);
            printf("\n");
        }

        unsigned defCount = GetDefCount(script, offset);
        for (unsigned i = 0; i < defCount; i++) {
            printf("  type %d:", i);
            pushedTypes(offset, i)->print(cx);
            printf("\n");
        }

        if (getCode(offset).monitoredTypes)
            printf("  monitored\n");

        TypeBarrier *barrier = getCode(offset).typeBarriers;
        if (barrier != NULL) {
            printf("  barrier:");
            while (barrier) {
                printf(" %s", TypeString(barrier->type));
                barrier = barrier->next;
            }
            printf("\n");
        }
    }

    printf("\n");

#endif /* DEBUG */

}

/////////////////////////////////////////////////////////////////////
// Interface functions
/////////////////////////////////////////////////////////////////////

namespace js {
namespace types {

void
MarkIteratorUnknownSlow(JSContext *cx)
{
    /* Check whether we are actually at an ITER opcode. */

    jsbytecode *pc;
    JSScript *script = cx->stack.currentScript(&pc);
    if (!script || !pc)
        return;

    /*
     * Watch out if the caller is in a different compartment from this one.
     * This must have gone through a cross-compartment wrapper.
     */
    if (script->compartment != cx->compartment)
        return;

    js::analyze::UntrapOpcode untrap(cx, script, pc);

    if (JSOp(*pc) == JSOP_ITER)
        TypeDynamicResult(cx, script, pc, Type::UnknownType());
}

void
TypeMonitorCallSlow(JSContext *cx, JSObject *callee,
                    const CallArgs &args, bool constructing)
{
    unsigned nargs = callee->getFunctionPrivate()->nargs;
    JSScript *script = callee->getFunctionPrivate()->script();

    if (!script->types.ensureTypeArray(cx))
        return;

    if (!constructing) {
        Type type = GetValueType(cx, args.thisv());
        script->types.setThis(cx, type);
    }

    /*
     * Add constraints going up to the minimum of the actual and formal count.
     * If there are more actuals than formals the later values can only be
     * accessed through the arguments object, which is monitored.
     */
    unsigned arg = 0;
    for (; arg < args.argc() && arg < nargs; arg++)
        script->types.setArgument(cx, arg, args[arg]);

    /* Watch for fewer actuals than formals to the call. */
    for (; arg < nargs; arg++)
        script->types.setArgument(cx, arg, UndefinedValue());
}

static inline bool
IsAboutToBeFinalized(JSContext *cx, TypeObjectKey *key)
{
    /* Mask out the low bit indicating whether this is a type or JS object. */
    return !reinterpret_cast<const gc::Cell *>((jsuword) key & ~1)->isMarked();
}

void
TypeDynamicResult(JSContext *cx, JSScript *script, jsbytecode *pc, Type type)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    AutoEnterTypeInference enter(cx);

    UntrapOpcode untrap(cx, script, pc);

    /* Directly update associated type sets for applicable bytecodes. */
    if (js_CodeSpec[*pc].format & JOF_TYPESET) {
        TypeSet *types = script->types.bytecodeTypes(pc);
        if (!types->hasType(type)) {
            InferSpew(ISpewOps, "externalType: monitorResult #%u:%05u: %s",
                      script->id(), pc - script->code, TypeString(type));
            types->addType(cx, type);
        }
        return;
    }

    /*
     * For inc/dec ops, we need to go back and reanalyze the affected opcode
     * taking the overflow into account. We won't see an explicit adjustment
     * of the type of the thing being inc/dec'ed, nor will adding TYPE_DOUBLE to
     * the pushed value affect that type.
     */
    JSOp op = JSOp(*pc);
    const JSCodeSpec *cs = &js_CodeSpec[op];
    if (cs->format & (JOF_INC | JOF_DEC)) {
        switch (op) {
          case JSOP_INCLOCAL:
          case JSOP_DECLOCAL:
          case JSOP_LOCALINC:
          case JSOP_LOCALDEC:
          case JSOP_INCARG:
          case JSOP_DECARG:
          case JSOP_ARGINC:
          case JSOP_ARGDEC: {
            /*
             * Just mark the slot's type as holding the new type. This captures
             * the effect if the slot is not being tracked, and if the slot
             * doesn't escape we will update the pushed types below to capture
             * the slot's value after this write.
             */
            uint32 slot = GetBytecodeSlot(script, pc);
            if (slot < TotalSlots(script)) {
                TypeSet *types = script->types.slotTypes(slot);
                types->addType(cx, type);
            }
            break;
          }

          default:;
        }
    }

    if (script->hasAnalysis() && script->analysis(cx)->ranInference()) {
        /*
         * If the pushed set already has this type, we don't need to ensure
         * there is a TypeIntermediate. Either there already is one, or the
         * type could be determined from the script's other input type sets.
         */
        TypeSet *pushed = script->analysis(cx)->pushedTypes(pc, 0);
        if (pushed->hasType(type))
            return;
    } else {
        /* Scan all intermediate types on the script to check for a dupe. */
        TypeResult *result, **pstart = &script->types.dynamicList, **presult = pstart;
        while (*presult) {
            result = *presult;
            if (result->offset == unsigned(pc - script->code) && result->type == type) {
                if (presult != pstart) {
                    /* Move to the head of the list, maintain LRU order. */
                    *presult = result->next;
                    result->next = *pstart;
                    *pstart = result;
                }
                return;
            }
            presult = &result->next;
        }
    }

    InferSpew(ISpewOps, "externalType: monitorResult #%u:%05u: %s",
              script->id(), pc - script->code, TypeString(type));

    TypeResult *result = cx->new_<TypeResult>(pc - script->code, type);
    if (!result) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }
    result->next = script->types.dynamicList;
    script->types.dynamicList = result;

    if (script->hasAnalysis() && script->analysis(cx)->ranInference()) {
        TypeSet *pushed = script->analysis(cx)->pushedTypes(pc, 0);
        pushed->addType(cx, type);
    }

    /* Trigger recompilation of any inline callers. */
    if (script->fun && !script->fun->hasLazyType())
        ObjectStateChange(cx, script->fun->type(), false, true);
}

void
TypeMonitorResult(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value &rval)
{
    UntrapOpcode untrap(cx, script, pc);

    /* Allow the non-TYPESET scenario to simplify stubs used in compound opcodes. */
    if (!(js_CodeSpec[*pc].format & JOF_TYPESET))
        return;

    Type type = GetValueType(cx, rval);
    TypeSet *types = script->types.bytecodeTypes(pc);
    if (types->hasType(type))
        return;

    AutoEnterTypeInference enter(cx);

    InferSpew(ISpewOps, "bytecodeType: #%u:%05u: %s",
              script->id(), pc - script->code, TypeString(type));
    types->addType(cx, type);
}

} } /* namespace js::types */

/////////////////////////////////////////////////////////////////////
// TypeScript
/////////////////////////////////////////////////////////////////////

/*
 * Returns true if we don't expect to compute the correct types for some value
 * pushed by the specified bytecode.
 */
static inline bool
IgnorePushed(const jsbytecode *pc, unsigned index)
{
    JS_ASSERT(JSOp(*pc) != JSOP_TRAP);

    switch (JSOp(*pc)) {
      /* We keep track of the scopes pushed by BINDNAME separately. */
      case JSOP_BINDNAME:
      case JSOP_BINDGNAME:
      case JSOP_BINDXMLNAME:
        return true;

      /* Stack not consistent in TRY_BRANCH_AFTER_COND. */
      case JSOP_IN:
      case JSOP_EQ:
      case JSOP_NE:
      case JSOP_LT:
      case JSOP_LE:
      case JSOP_GT:
      case JSOP_GE:
        return (index == 0);

      /* Value not determining result is not pushed by OR/AND. */
      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
        return (index == 0);

      /* Holes tracked separately. */
      case JSOP_HOLE:
        return (index == 0);
      case JSOP_FILTER:
        return (index == 1);

      /* Storage for 'with' and 'let' blocks not monitored. */
      case JSOP_ENTERWITH:
      case JSOP_ENTERBLOCK:
        return true;

      /* We don't keep track of the iteration state for 'for in' or 'for each in' loops. */
      case JSOP_ITER:
      case JSOP_ITERNEXT:
      case JSOP_MOREITER:
      case JSOP_ENDITER:
        return true;

      /* Ops which can manipulate values pushed by opcodes we don't model. */
      case JSOP_DUP:
      case JSOP_DUP2:
      case JSOP_SWAP:
      case JSOP_PICK:
        return true;

      /* We don't keep track of state indicating whether there is a pending exception. */
      case JSOP_FINALLY:
        return true;

      /*
       * We don't treat GETLOCAL immediately followed by a pop as a use-before-def,
       * and while the type will have been inferred correctly the method JIT
       * may not have written the local's initial undefined value to the stack,
       * leaving a stale value.
       */
      case JSOP_GETLOCAL:
        return JSOp(pc[JSOP_GETLOCAL_LENGTH]) == JSOP_POP;

      default:
        return false;
    }
}

bool
TypeScript::makeTypeArray(JSContext *cx)
{
    JS_ASSERT(!typeArray);

    AutoEnterTypeInference enter(cx);

    unsigned count = numTypeSets();
    typeArray = (TypeSet *) cx->calloc_(sizeof(TypeSet) * count);
    if (!typeArray) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }

#ifdef DEBUG
    unsigned id = script()->id();
    for (unsigned i = 0; i < script()->nTypeSets; i++)
        InferSpew(ISpewOps, "typeSet: %sT%p%s bytecode%u #%u",
                  InferSpewColor(&typeArray[i]), &typeArray[i], InferSpewColorReset(),
                  i, id);
    InferSpew(ISpewOps, "typeSet: %sT%p%s return #%u",
              InferSpewColor(returnTypes()), returnTypes(), InferSpewColorReset(),
              id);
    InferSpew(ISpewOps, "typeSet: %sT%p%s this #%u",
              InferSpewColor(thisTypes()), thisTypes(), InferSpewColorReset(),
              id);
    unsigned nargs = script()->fun ? script()->fun->nargs : 0;
    for (unsigned i = 0; i < nargs; i++)
        InferSpew(ISpewOps, "typeSet: %sT%p%s arg%u #%u",
                  InferSpewColor(argTypes(i)), argTypes(i), InferSpewColorReset(),
                  i, id);
    for (unsigned i = 0; i < script()->nfixed; i++)
        InferSpew(ISpewOps, "typeSet: %sT%p%s local%u #%u",
                  InferSpewColor(localTypes(i)), localTypes(i), InferSpewColorReset(),
                  i, id);
    for (unsigned i = 0; i < script()->bindings.countUpvars(); i++)
        InferSpew(ISpewOps, "typeSet: %sT%p%s upvar%u #%u",
                  InferSpewColor(upvarTypes(i)), upvarTypes(i), InferSpewColorReset(),
                  i, id);
#endif

    return true;
}

bool
JSScript::typeSetFunction(JSContext *cx, JSFunction *fun, bool singleton)
{
    this->fun = fun;

    if (!cx->typeInferenceEnabled())
        return true;

    char *name = NULL;
#ifdef DEBUG
    name = (char *) alloca(10);
    JS_snprintf(name, 10, "#%u", id());
#endif

    if (singleton) {
        if (!fun->setSingletonType(cx))
            return false;
    } else {
        TypeObject *type = cx->compartment->types.newTypeObject(cx, this, name, "",
                                                                JSProto_Function, fun->getProto());
        if (!type)
            return false;
        AutoTypeRooter root(cx, type);

        js::Shape *shape = js::EmptyShape::create(cx, fun->getClass());
        if (!shape)
            return false;

        fun->setType(type);
        fun->setMap(shape);

        type->functionScript = this;
    }

    this->fun = fun;
    return true;
}

#ifdef DEBUG

void
TypeScript::checkBytecode(JSContext *cx, jsbytecode *pc, const js::Value *sp)
{
    AutoEnterTypeInference enter(cx);
    UntrapOpcode untrap(cx, script(), pc);

    if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
        return;

    if (!script()->hasAnalysis() || !script()->analysis(cx)->ranInference())
        return;
    ScriptAnalysis *analysis = script()->analysis(cx);

    int defCount = GetDefCount(script(), pc - script()->code);

    for (int i = 0; i < defCount; i++) {
        const js::Value &val = sp[-defCount + i];
        TypeSet *types = analysis->pushedTypes(pc, i);
        if (IgnorePushed(pc, i))
            continue;

        Type type = GetValueType(cx, val);

        if (!types->hasType(type)) {
            /* Display fine-grained debug information first */
            fprintf(stderr, "Missing type at #%u:%05u pushed %u: %s\n", 
                    script()->id(), unsigned(pc - script()->code), i, TypeString(type));
            TypeFailure(cx, "Missing type pushed %u: %s", i, TypeString(type));
        }
    }
}

#endif

/////////////////////////////////////////////////////////////////////
// JSObject
/////////////////////////////////////////////////////////////////////

bool
JSObject::shouldSplicePrototype(JSContext *cx)
{
    /*
     * During bootstrapping, if inference is enabled we need to make sure not
     * to splice a new prototype in for Function.prototype or the global
     * object if their __proto__ had previously been set to null, as this
     * will change the prototype for all other objects with the same type.
     * If inference is disabled we cannot determine from the object whether it
     * has had its __proto__ set after creation.
     */
    if (getProto() != NULL)
        return false;
    return !cx->typeInferenceEnabled() || hasSingletonType();
}

bool
JSObject::splicePrototype(JSContext *cx, JSObject *proto)
{
    /*
     * For singleton types representing only a single JSObject, the proto
     * can be rearranged as needed without destroying type information for
     * the old or new types. Note that type constraints propagating properties
     * from the old prototype are not removed.
     */
    JS_ASSERT_IF(cx->typeInferenceEnabled(), hasSingletonType());

    /*
     * Force type instantiation when splicing lazy types. This may fail,
     * in which case inference will be disabled for the compartment.
     */
    getType(cx);
    if (proto) {
        proto->getType(cx);
        if (!proto->getNewType(cx))
            return false;
    }

    if (!cx->typeInferenceEnabled()) {
        TypeObject *type = proto ? proto->getNewType(cx) : &emptyTypeObject;
        if (!type)
            return false;
        type_ = type;
        return true;
    }

    type()->proto = proto;

    AutoEnterTypeInference enter(cx);

    if (proto && proto->type()->unknownProperties() && !type()->unknownProperties()) {
        type()->markUnknown(cx);
        return true;
    }

    if (!type()->unknownProperties()) {
        /* Update properties on this type with any shared with the prototype. */
        unsigned count = type()->getPropertyCount();
        for (unsigned i = 0; i < count; i++) {
            Property *prop = type()->getProperty(i);
            if (prop && prop->types.hasPropagatedProperty())
                type()->getFromPrototypes(cx, prop->id, &prop->types, true);
        }
    }

    return true;
}

void
JSObject::makeLazyType(JSContext *cx)
{
    JS_ASSERT(cx->typeInferenceEnabled() && hasLazyType());
    AutoEnterTypeInference enter(cx);

    char *name = NULL;
#ifdef DEBUG
    name = (char *) alloca(20);
    JS_snprintf(name, 20, "<0x%p>", (void *) this);
#endif

    TypeObject *type = cx->compartment->types.newTypeObject(cx, NULL, name, "",
                                                            JSProto_Object, getProto());
    if (!type) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    /* Fill in the type according to the state of this object. */

    type->singleton = this;

    if (isFunction() && getFunctionPrivate() && getFunctionPrivate()->isInterpreted()) {
        type->functionScript = getFunctionPrivate()->script();
        if (type->functionScript->uninlineable)
            type->flags |= OBJECT_FLAG_UNINLINEABLE;
        if (type->functionScript->createdArgs)
            type->flags |= OBJECT_FLAG_CREATED_ARGUMENTS;
    }

#if JS_HAS_XML_SUPPORT
    /*
     * XML objects do not have equality hooks but are treated special by EQ/NE
     * ops. Just mark the type as totally unknown.
     */
    if (isXML() && !type->unknownProperties())
        type->markUnknown(cx);
#endif

    if (clasp->ext.equality)
        type->flags |= OBJECT_FLAG_SPECIAL_EQUALITY;

    if (type->unknownProperties()) {
        type_ = type;
        flags &= ~LAZY_TYPE;
        return;
    }

    /* Not yet generating singleton arrays. */
    type->flags |= OBJECT_FLAG_NON_DENSE_ARRAY
                |  OBJECT_FLAG_NON_PACKED_ARRAY
                |  OBJECT_FLAG_NON_TYPED_ARRAY;

    type_ = type;
    flags &= ~LAZY_TYPE;
}

void
JSObject::makeNewType(JSContext *cx, JSScript *newScript, bool unknown)
{
    JS_ASSERT(!newType);

    const char *name = NULL;
#ifdef DEBUG
    name = TypeString(Type::ObjectType(this));
#endif

    TypeObject *type = cx->compartment->types.newTypeObject(cx, NULL, name, "new",
                                                            JSProto_Object, this, unknown);
    if (!type)
        return;

    if (!cx->typeInferenceEnabled()) {
        newType = type;
        setDelegate();
        return;
    }

    AutoEnterTypeInference enter(cx);

    /*
     * Set the special equality flag for types whose prototype also has the
     * flag set. This is a hack, :XXX: need a real correspondence between
     * types and the possible js::Class of objects with that type.
     */
    if (hasSpecialEquality())
        type->flags |= OBJECT_FLAG_SPECIAL_EQUALITY;

    if (newScript)
        CheckNewScriptProperties(cx, type, newScript);

#if JS_HAS_XML_SUPPORT
    /* Special case for XML object equality, see makeLazyType(). */
    if (isXML() && !type->unknownProperties())
        type->flags |= OBJECT_FLAG_UNKNOWN_MASK;
#endif

    if (clasp->ext.equality)
        type->flags |= OBJECT_FLAG_SPECIAL_EQUALITY;

    /*
     * The new type is not present in any type sets, so mark the object as
     * unknown in all type sets it appears in. This allows the prototype of
     * such objects to mutate freely without triggering an expensive walk of
     * the compartment's type sets. (While scripts normally don't mutate
     * __proto__, the browser will for proxies and such, and we need to
     * accommodate this behavior).
     */
    if (type->unknownProperties())
        type->flags |= OBJECT_FLAG_SETS_MARKED_UNKNOWN;

    newType = type;
    setDelegate();
}

/////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////

void
TypeSet::sweep(JSContext *cx, JSCompartment *compartment)
{
    JS_ASSERT(!intermediate());
    uint32 objectCount = baseObjectCount();

    if (objectCount >= 2) {
        bool removed = false;
        unsigned objectCapacity = HashSetCapacity(objectCount);
        for (unsigned i = 0; i < objectCapacity; i++) {
            TypeObjectKey *object = objectSet[i];
            if (object && IsAboutToBeFinalized(cx, object)) {
                objectSet[i] = NULL;
                removed = true;
            }
        }
        if (removed) {
            /* Reconstruct the type set to re-resolve hash collisions. */
            TypeObjectKey **oldArray = objectSet;
            objectSet = NULL;
            objectCount = 0;
            for (unsigned i = 0; i < objectCapacity; i++) {
                TypeObjectKey *object = oldArray[i];
                if (object) {
                    TypeObjectKey **pentry =
                        HashSetInsert<TypeObjectKey *,TypeObjectKey,TypeObjectKey>
                            (cx, objectSet, objectCount, object, false);
                    if (pentry)
                        *pentry = object;
                    else
                        compartment->types.setPendingNukeTypes(cx);
                }
            }
            setBaseObjectCount(objectCount);
            cx->free_(oldArray);
        }
    } else if (objectCount == 1) {
        TypeObjectKey *object = (TypeObjectKey *) objectSet;
        if (IsAboutToBeFinalized(cx, object)) {
            objectSet = NULL;
            setBaseObjectCount(0);
        }
    }

    /*
     * All constraints are wiped out on each GC, including those propagating
     * into this type set from prototype properties.
     */
    constraintList = NULL;
    flags &= ~TYPE_FLAG_PROPAGATED_PROPERTY;
}

inline void
JSObject::revertLazyType()
{
    JS_ASSERT(hasSingletonType() && !hasLazyType());
    JS_ASSERT_IF(type_->proto, type_->proto->newType);
    flags |= LAZY_TYPE;
    type_ = (type_->proto) ? type_->proto->newType : &emptyTypeObject;
}

inline void
TypeObject::clearProperties()
{
    JS_ASSERT(singleton);
    setBasePropertyCount(0);
    propertySet = NULL;
}

/*
 * Before sweeping the arenas themselves, scan all type objects in a
 * compartment to fixup weak references: property type sets referencing dead
 * JS and type objects, and singleton JS objects whose type is not referenced
 * elsewhere. This also releases memory associated with dead type objects,
 * so that type objects do not need later finalization.
 */
static inline void
SweepTypeObject(JSContext *cx, TypeObject *object)
{
    /*
     * We may be regenerating existing type sets containing this object,
     * so reset contributions on each GC to avoid tripping the limit.
     */
    object->contribution = 0;

    if (object->singleton) {
        JS_ASSERT(!object->emptyShapes);
        JS_ASSERT(!object->newScript);

        /*
         * All properties on the object are allocated from the analysis pool,
         * and can be discarded. We will regenerate them as needed as code gets
         * reanalyzed.
         */
        object->clearProperties();

        if (!object->isMarked()) {
            /*
             * Singleton objects do not hold strong references on their types.
             * When removing the type, however, we need to fixup the singleton
             * so that it has a lazy type again. The generic 'new' type for the
             * proto must be live, since the type's prototype and its 'new'
             * type are both strong references.
             */
            JS_ASSERT_IF(object->singleton->isMarked() && object->proto,
                         object->proto->isMarked() && object->proto->newType->isMarked());
            object->singleton->revertLazyType();
        }

        return;
    }

    if (!object->isMarked()) {
        if (object->emptyShapes)
            Foreground::free_(object->emptyShapes);

        unsigned count = object->getPropertyCount();
        for (unsigned i = 0; i < count; i++) {
            Property *prop = object->getProperty(i);
            if (prop) {
                prop->types.clearObjects();
                Foreground::delete_(prop);
            }
        }
        if (count >= 2)
            Foreground::free_(object->propertySet);

        if (object->newScript)
            Foreground::free_(object->newScript);

        return;
    }

    /* Sweep type sets for all properties of the object. */
    unsigned count = object->getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        Property *prop = object->getProperty(i);
        if (prop)
            prop->types.sweep(cx, object->compartment());
    }

    /*
     * The GC will clear out the constraints ensuring the correctness of the
     * newScript information, these constraints will need to be regenerated
     * the next time we compile code which depends on this info.
     */
    if (object->newScript)
        object->flags |= OBJECT_FLAG_NEW_SCRIPT_REGENERATE;
}

void
SweepTypeObjects(JSContext *cx, JSCompartment *compartment)
{
    JS_ASSERT(!emptyTypeObject.emptyShapes);
    JS_ASSERT(!emptyTypeObject.newScript);

    gc::ArenaHeader *aheader = compartment->arenas[gc::FINALIZE_TYPE_OBJECT].getHead();
    size_t thingSize = sizeof(TypeObject);

    for (; aheader; aheader = aheader->next) {
        gc::Arena *arena = aheader->getArena();
        gc::FreeSpan firstSpan(aheader->getFirstFreeSpan());
        gc::FreeSpan *span = &firstSpan;

        for (uintptr_t thing = arena->thingsStart(thingSize); ; thing += thingSize) {
            JS_ASSERT(thing <= arena->thingsEnd());
            if (thing == span->start) {
                if (!span->hasNext())
                    break;
                thing = span->end;
                span = span->nextSpan();
            } else {
                SweepTypeObject(cx, reinterpret_cast<TypeObject *>(thing));
            }
        }
    }
}

void
TypeCompartment::sweep(JSContext *cx)
{
    JSCompartment *compartment = this->compartment();

    SweepTypeObjects(cx, compartment);

    /*
     * Iterate through the array/object type tables and remove all entries
     * referencing collected data. These tables only hold weak references.
     */

    if (arrayTypeTable) {
        for (ArrayTypeTable::Enum e(*arrayTypeTable); !e.empty(); e.popFront()) {
            const ArrayTableKey &key = e.front().key;
            TypeObject *obj = e.front().value;
            JS_ASSERT(obj->proto == key.proto);
            JS_ASSERT(!key.type.isSingleObject());

            bool remove = false;
            if (key.type.isTypeObject() && !key.type.typeObject()->isMarked())
                remove = true;
            if (!obj->isMarked())
                remove = true;

            if (remove)
                e.removeFront();
        }
    }

    if (objectTypeTable) {
        for (ObjectTypeTable::Enum e(*objectTypeTable); !e.empty(); e.popFront()) {
            const ObjectTableKey &key = e.front().key;
            const ObjectTableEntry &entry = e.front().value;
            JS_ASSERT(entry.object->proto == key.proto);

            bool remove = false;
            if (!entry.object->isMarked() || !entry.newShape->isMarked())
                remove = true;
            for (unsigned i = 0; !remove && i < key.nslots; i++) {
                if (JSID_IS_STRING(key.ids[i])) {
                    JSString *str = JSID_TO_STRING(key.ids[i]);
                    if (!str->isStaticAtom() && !str->isMarked())
                        remove = true;
                }
                JS_ASSERT(!entry.types[i].isSingleObject());
                if (entry.types[i].isTypeObject() && !entry.types[i].typeObject()->isMarked())
                    remove = true;
            }

            if (remove) {
                Foreground::free_(key.ids);
                Foreground::free_(entry.types);
                e.removeFront();
            }
        }
    }

    if (allocationSiteTable) {
        for (AllocationSiteTable::Enum e(*allocationSiteTable); !e.empty(); e.popFront()) {
            const AllocationSiteKey &key = e.front().key;
            TypeObject *object = e.front().value;

            if (key.uncached || key.script->isAboutToBeFinalized(cx) || !object->isMarked())
                e.removeFront();
        }
    }
}

TypeCompartment::~TypeCompartment()
{
    if (pendingArray)
        Foreground::free_(pendingArray);

    if (arrayTypeTable)
        Foreground::delete_(arrayTypeTable);

    if (objectTypeTable)
        Foreground::delete_(objectTypeTable);

    if (allocationSiteTable)
        Foreground::delete_(allocationSiteTable);
}

void
TypeScript::sweep(JSContext *cx)
{
    JSCompartment *compartment = script()->compartment;
    JS_ASSERT(compartment->types.inferenceEnabled);

    if (typeArray) {
        unsigned num = numTypeSets();

        if (script()->isAboutToBeFinalized(cx)) {
            /* Release all memory associated with the persistent type sets. */
            for (unsigned i = 0; i < num; i++)
                typeArray[i].clearObjects();
            cx->free_(typeArray);
            typeArray = NULL;
        } else {
            /* Condense all constraints in the persistent type sets. */
            for (unsigned i = 0; i < num; i++)
                typeArray[i].sweep(cx, compartment);
        }
    }

    TypeResult **presult = &dynamicList;
    while (*presult) {
        TypeResult *result = *presult;
        Type type = result->type;

        if (!type.isUnknown() && !type.isAnyObject() && type.isObject() &&
            IsAboutToBeFinalized(cx, type.objectKey())) {
            *presult = result->next;
            cx->delete_(result);
        } else {
            presult = &result->next;
        }
    }

    /*
     * Method JIT code depends on the type inference data which is about to
     * be purged, so purge the jitcode as well.
     */
#ifdef JS_METHODJIT
    if (script()->jitNormal)
        mjit::ReleaseScriptCode(cx, script(), true);
    if (script()->jitCtor)
        mjit::ReleaseScriptCode(cx, script(), false);
#endif
}

void
TypeScript::destroy()
{
    while (dynamicList) {
        TypeResult *next = dynamicList->next;
        Foreground::delete_(dynamicList);
        dynamicList = next;
    }

    Foreground::free_(typeArray);
}

size_t
TypeSet::dynamicSize()
{
    uint32 count = baseObjectCount();
    if (count >= 2)
        return HashSetCapacity(count) * sizeof(TypeObject *);
    return 0;
}

static void
GetScriptMemoryStats(JSScript *script, JSCompartment::TypeInferenceMemoryStats *stats)
{
    if (!script->types.typeArray)
        return;

    unsigned count = script->types.numTypeSets();
    stats->scriptMain += count * sizeof(TypeSet);
    for (unsigned i = 0; i < count; i++)
        stats->scriptSets += script->types.typeArray[i].dynamicSize();

    TypeResult *result = script->types.dynamicList;
    while (result) {
        stats->scriptMain += sizeof(TypeResult);
        result = result->next;
    }
}

void
JSCompartment::getTypeInferenceMemoryStats(JSContext *cx, TypeInferenceMemoryStats *stats)
{
    for (JSCList *cursor = scripts.next; cursor != &scripts; cursor = cursor->next) {
        JSScript *script = reinterpret_cast<JSScript *>(cursor);
        GetScriptMemoryStats(script, stats);
    }

    stats->poolMain += ArenaAllocatedSize(pool);
}

void
JSCompartment::getTypeInferenceObjectStats(TypeObject *object, TypeInferenceMemoryStats *stats)
{
    stats->objectMain += sizeof(TypeObject);

    if (object->singleton) {
        /*
         * Properties and TypeSet data for singletons are allocated in the
         * compartment's analysis pool.
         */
        return;
    }

    uint32 count = object->getPropertyCount();
    if (count >= 2)
        stats->objectMain += count * sizeof(Property *);

    for (unsigned i = 0; i < count; i++) {
        Property *prop = object->getProperty(i);
        if (prop) {
            stats->objectMain += sizeof(Property);
            stats->objectSets += prop->types.dynamicSize();
        }
    }
}
