/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79 ft=cpp:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   Andreas Gal <gal@mozilla.com>
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

#ifndef jstracer_h___
#define jstracer_h___

#include "jsstddef.h"
#include "jslock.h"

#include "nanojit/nanojit.h"

/*
 * Tracker is used to keep track of values being manipulated by the 
 * interpreter during trace recording.
 */
class Tracker 
{
    struct Page {
        struct Page* next;
        long base;
        nanojit::LIns* map[0];
    };
    struct Page* pagelist;
    
    long            getPageBase(const void* v) const;
    struct Page*    findPage(const void* v) const;
    struct Page*    addPage(const void* v);
public:    
    Tracker();
    ~Tracker();
    
    nanojit::LIns*  get(const void* v) const;
    void            set(const void* v, nanojit::LIns* ins);
    void            clear();
};

class TraceRecorder {
    JSContext*              cx;
    Tracker                 tracker;
    struct JSStackFrame*    entryFrame;
    struct JSFrameRegs      entryRegs;
    nanojit::Fragment*      fragment;
    nanojit::LirBuffer*     lirbuf;
    nanojit::LirWriter*     lir;
    nanojit::LirWriter*     lir_buf_writer;
    nanojit::LirWriter*     verbose_filter;
    nanojit::LirWriter*     cse_filter;
    nanojit::LirWriter*     expr_filter;
    struct JSFrameRegs      markRegs;
    nanojit::SideExit       exit;

    unsigned nativeFrameSlots(JSStackFrame* fp) const;
    void buildTypeMap(JSStackFrame* fp, char* m) const;
public:
    TraceRecorder(JSContext* cx, nanojit::Fragmento*);
    ~TraceRecorder();
    
    inline jsbytecode* entryPC() const
    {
        return entryRegs.pc;
    }
    
    void mark();
    void recover();

    void abort();
    
    unsigned calldepth() const;
    JSStackFrame* findFrame(void* p) const;
    bool TraceRecorder::onFrame(void* p) const;
    unsigned nativeFrameSlots() const;
    unsigned nativeFrameOffset(void* p) const;
    void buildTypeMap(char* m) const;
    bool unbox_jsval(jsval v, int t, double* slot) const;
    bool box_jsval(jsval* vp, int t, double* slot) const;
    bool unbox(JSStackFrame* fp, char* m, double* native) const;
    bool box(JSStackFrame* fp, char* m, double* native) const;

    void set(void* p, nanojit::LIns* l);
    nanojit::LIns* get(void* p);
    
    void readstack(void*);
    
    void copy(void* a, void* v);
    void imm(jsint i, void* v);
    void imm(jsdouble d, void* v);
    void unary(nanojit::LOpcode op, void* a, void* v);
    void binary(nanojit::LOpcode op, void* a, void* b, void* v);
    void binary0(nanojit::LOpcode op, void* a, void* v);
    void call(int id, void* a, void* v);
    void call(int id, void* a, void* b, void* v);
    void call(int id, void* a, void* b, void* c, void* v);
    
    void iinc(void* a, int32_t incr, void* v);

    void load(void* a, int32_t i, void* v);

    void guard_0(bool expected, void* a);
    void guard_h(bool expected, void* a);
    void guard_ov(bool expected, void* a);
    void guard_eq(bool expected, void* a, void* b);
    void guard_eqi(bool expected, void* a, int i);
    
    void closeLoop(nanojit::Fragmento* fragmento);
};

/*
 * Trace monitor. Every runtime is associated with a trace monitor that keeps
 * track of loop frequencies for all JavaScript code loaded into that runtime.
 * For this we use a loop table. Adjacent slots in the loop table, one for each
 * loop header in a given script, are requested using lock-free synchronization
 * from the runtime-wide loop table slot space, when the script is compiled.
 *
 * The loop table also doubles as trace tree pointer table once a loop achieves
 * a certain number of iterations and we recorded a tree for that loop.
 */
struct JSTraceMonitor {
    int                     freq;
    nanojit::Fragmento*     fragmento;
    TraceRecorder*          recorder;
};

#define TRACING_ENABLED(cx)       JS_HAS_OPTION(cx, JSOPTION_JIT)
#define TRACE_TRIGGER_MASK 0x3f

extern bool
js_StartRecording(JSContext* cx);

extern void
js_AbortRecording(JSContext* cx);

extern void
js_EndRecording(JSContext* cx);

#endif /* jstracer_h___ */
