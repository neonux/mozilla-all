/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla SpiderMonkey JavaScript code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Leary <cdleary@mozilla.com>
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

#ifndef RegExpObject_h__
#define RegExpObject_h__

#include <stddef.h>
#include "jsobj.h"

#include "js/TemplateLib.h"

#include "yarr/Yarr.h"
#if ENABLE_YARR_JIT
#include "yarr/YarrJIT.h"
#include "yarr/YarrSyntaxChecker.h"
#else
#include "yarr/pcre/pcre.h"
#endif

namespace js {

enum RegExpRunStatus
{
    RegExpRunStatus_Error,
    RegExpRunStatus_Success,
    RegExpRunStatus_Success_NotFound
};

class RegExpObject : public ::JSObject
{
    static const uintN LAST_INDEX_SLOT          = 0;
    static const uintN SOURCE_SLOT              = 1;
    static const uintN GLOBAL_FLAG_SLOT         = 2;
    static const uintN IGNORE_CASE_FLAG_SLOT    = 3;
    static const uintN MULTILINE_FLAG_SLOT      = 4;
    static const uintN STICKY_FLAG_SLOT         = 5;

  public:
    static const uintN RESERVED_SLOTS = 6;

    /*
     * Note: The regexp statics flags are OR'd into the provided flags,
     * so this function is really meant for object creation during code
     * execution, as opposed to during something like XDR.
     */
    static inline RegExpObject *
    create(JSContext *cx, RegExpStatics *res, const jschar *chars, size_t length, RegExpFlag flags,
           TokenStream *ts);

    static inline RegExpObject *
    createNoStatics(JSContext *cx, const jschar *chars, size_t length, RegExpFlag flags,
                    TokenStream *ts);

    static RegExpObject *clone(JSContext *cx, RegExpObject *obj, RegExpObject *proto);

    /* Note: fallible. */
    JSFlatString *toString(JSContext *cx) const;

    /*
     * Run the regular expression over the input text.
     *
     * Results are placed in |output| as integer pairs. For eaxmple,
     * |output[0]| and |output[1]| represent the text indices that make
     * up the "0" (whole match) pair. Capturing parens will result in
     * more output.
     *
     * N.B. it's the responsibility of the caller to hook the |output|
     * into the |RegExpStatics| appropriately, if necessary.
     */
    RegExpRunStatus execute(JSContext *cx, const jschar *chars, size_t length, size_t *lastIndex,
                            LifoAllocScope &allocScope, MatchPairs **output);

    /* Accessors. */

    const Value &getLastIndex() const {
        return getSlot(LAST_INDEX_SLOT);
    }
    void setLastIndex(const Value &v) {
        setSlot(LAST_INDEX_SLOT, v);
    }
    void setLastIndex(double d) {
        setSlot(LAST_INDEX_SLOT, NumberValue(d));
    }
    void zeroLastIndex() {
        setSlot(LAST_INDEX_SLOT, Int32Value(0));
    }

    JSLinearString *getSource() const {
        return &getSlot(SOURCE_SLOT).toString()->asLinear();
    }
    void setSource(JSLinearString *source) {
        setSlot(SOURCE_SLOT, StringValue(source));
    }
    RegExpFlag getFlags() const {
        uintN flags = 0;
        flags |= global() ? GlobalFlag : 0;
        flags |= ignoreCase() ? IgnoreCaseFlag : 0;
        flags |= multiline() ? MultilineFlag : 0;
        flags |= sticky() ? StickyFlag : 0;
        return RegExpFlag(flags);
    }

    /* Flags. */

    void setIgnoreCase(bool enabled)    { setSlot(IGNORE_CASE_FLAG_SLOT, BooleanValue(enabled)); }
    void setGlobal(bool enabled)        { setSlot(GLOBAL_FLAG_SLOT, BooleanValue(enabled)); }
    void setMultiline(bool enabled)     { setSlot(MULTILINE_FLAG_SLOT, BooleanValue(enabled)); }
    void setSticky(bool enabled)        { setSlot(STICKY_FLAG_SLOT, BooleanValue(enabled)); }
    bool ignoreCase() const { return getSlot(IGNORE_CASE_FLAG_SLOT).toBoolean(); }
    bool global() const     { return getSlot(GLOBAL_FLAG_SLOT).toBoolean(); }
    bool multiline() const  { return getSlot(MULTILINE_FLAG_SLOT).toBoolean(); }
    bool sticky() const     { return getSlot(STICKY_FLAG_SLOT).toBoolean(); }

    /*
     * N.B. |RegExpObject|s can be mutated in place because of |RegExp.prototype.compile|, hence
     * |reset| for re-initialization.
     */

    inline bool reset(JSContext *cx, RegExpObject *other);
    inline bool reset(JSContext *cx, AlreadyIncRefed<RegExpPrivate> rep);
    inline bool reset(JSContext *cx, JSLinearString *source, RegExpFlag flags);

    inline RegExpPrivate *getOrCreatePrivate(JSContext *cx);
    inline void finalize(JSContext *cx);

  private:
    /* The |RegExpPrivate| is lazily created at the time of use. */
    inline RegExpPrivate *getPrivate() const;

    /*
     * Precondition: the syntax for |source| has already been validated.
     * Side effect: sets the private field.
     */
    bool makePrivate(JSContext *cx);

    friend bool ResetRegExpObject(JSContext *, RegExpObject *, JSLinearString *, RegExpFlag);
    friend bool ResetRegExpObject(JSContext *, RegExpObject *, AlreadyIncRefed<RegExpPrivate>);

    /*
     * Compute the initial shape to associate with fresh RegExp objects,
     * encoding their initial properties. Return the shape after
     * changing this regular expression object's last property to it.
     */
    const Shape *assignInitialShape(JSContext *cx);

    RegExpObject();
    RegExpObject &operator=(const RegExpObject &reo);
}; /* class RegExpObject */

/* Abstracts away the gross |RegExpPrivate| backend details. */
class RegExpPrivateCode
{
#if ENABLE_YARR_JIT
    typedef JSC::Yarr::BytecodePattern BytecodePattern;
    typedef JSC::Yarr::ErrorCode ErrorCode;
    typedef JSC::Yarr::JSGlobalData JSGlobalData;
    typedef JSC::Yarr::YarrCodeBlock YarrCodeBlock;
    typedef JSC::Yarr::YarrPattern YarrPattern;

    /* Note: Native code is valid only if |codeBlock.isFallBack() == false|. */
    YarrCodeBlock   codeBlock;
    BytecodePattern *byteCode;
#else
    JSRegExp        *compiled;
#endif

  public:
    RegExpPrivateCode()
      :
#if ENABLE_YARR_JIT
        codeBlock(),
        byteCode(NULL)
#else
        compiled(NULL)
#endif
    { }

    ~RegExpPrivateCode() {
#if ENABLE_YARR_JIT
        codeBlock.release();
        if (byteCode)
            Foreground::delete_<BytecodePattern>(byteCode);
#else
        if (compiled)
            jsRegExpFree(compiled);
#endif
    }

    static bool checkSyntax(JSContext *cx, TokenStream *tokenStream, JSLinearString *source) {
#if ENABLE_YARR_JIT
        ErrorCode error = JSC::Yarr::checkSyntax(*source);
        if (error == JSC::Yarr::NoError)
            return true;

        reportYarrError(cx, tokenStream, error);
        return false;
#else
# error "Syntax checking not implemented for !ENABLE_YARR_JIT"
#endif
    }

#if ENABLE_YARR_JIT
    static inline bool isJITRuntimeEnabled(JSContext *cx);
    static void reportYarrError(JSContext *cx, TokenStream *ts, JSC::Yarr::ErrorCode error);
#else
    static void reportPCREError(JSContext *cx, int error);
#endif

    inline bool compile(JSContext *cx, JSLinearString &pattern, TokenStream *ts, uintN *parenCount,
                        RegExpFlag flags);

    inline RegExpRunStatus execute(JSContext *cx, const jschar *chars, size_t length, size_t start,
                                   int *output, size_t outputCount);

    static size_t getOutputSize(size_t pairCount) {
#if ENABLE_YARR_JIT
        return pairCount * 2;
#else
        return pairCount * 3; /* Should be x2, but PCRE has... needs. */
#endif
    }
};

/*
 * The "meat" of the builtin regular expression objects: it contains the
 * mini-program that represents the source of the regular expression.
 * Excepting refcounts, this is an immutable datastructure after
 * compilation.
 *
 * Non-atomic refcounting is used, so single-thread invariants must be
 * maintained: we check regexp operations are performed in a single
 * compartment.
 *
 * Note: refCount cannot overflow because that would require more
 * referring regexp objects than there is space for in addressable
 * memory.
 */
class RegExpPrivate
{
    RegExpPrivateCode   code;
    JSLinearString      *source;
    size_t              refCount;
    uintN               parenCount;
    RegExpFlag          flags;

  public:
    DebugOnly<JSCompartment *> compartment;

  private:
    RegExpPrivate(JSLinearString *source, RegExpFlag flags, JSCompartment *compartment)
      : source(source), refCount(1), parenCount(0), flags(flags), compartment(compartment)
    { }

    JS_DECLARE_ALLOCATION_FRIENDS_FOR_PRIVATE_CONSTRUCTOR;

    bool compile(JSContext *cx, TokenStream *ts);
    static inline void checkMatchPairs(JSString *input, int *buf, size_t matchItemCount);

  public:
    static AlreadyIncRefed<RegExpPrivate>
    create(JSContext *cx, JSLinearString *source, RegExpFlag flags, TokenStream *ts);

    static AlreadyIncRefed<RegExpPrivate>
    create(JSContext *cx, JSLinearString *source, JSString *flags, TokenStream *ts);

    RegExpRunStatus execute(JSContext *cx, const jschar *chars, size_t length, size_t *lastIndex,
                            LifoAllocScope &allocScope, MatchPairs **output);

    /* Mutators */

    void incref(JSContext *cx);
    void decref(JSContext *cx);

    /* For JIT access. */
    size_t *addressOfRefCount() { return &refCount; }

    /* Accessors */

    JSLinearString *getSource() const   { return source; }
    size_t getParenCount() const        { return parenCount; }

    /* Accounts for the "0" (whole match) pair. */
    size_t pairCount() const            { return parenCount + 1; }

    RegExpFlag getFlags() const         { return flags; }
    bool ignoreCase() const { return flags & IgnoreCaseFlag; }
    bool global() const     { return flags & GlobalFlag; }
    bool multiline() const  { return flags & MultilineFlag; }
    bool sticky() const     { return flags & StickyFlag; }
};

/*
 * Parse regexp flags. Report an error and return false if an invalid
 * sequence of flags is encountered (repeat/invalid flag).
 *
 * N.B. flagStr must be rooted.
 */
bool
ParseRegExpFlags(JSContext *cx, JSString *flagStr, RegExpFlag *flagsOut);

inline bool
ValueIsRegExp(const Value &v);

inline bool
IsRegExpMetaChar(jschar c);

} /* namespace js */

extern JS_FRIEND_API(JSObject *) JS_FASTCALL
js_CloneRegExpObject(JSContext *cx, JSObject *obj, JSObject *proto);

JSBool
js_XDRRegExpObject(JSXDRState *xdr, JSObject **objp);

#endif
