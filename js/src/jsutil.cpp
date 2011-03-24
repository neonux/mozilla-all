/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corp.
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

/*
 * PR assertion checker.
 */
#include <stdio.h>
#include <stdlib.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"

#ifdef WIN32
#    include "jswin.h"
#else
#    include <signal.h>
#endif

using namespace js;

#ifdef DEBUG
/* For JS_OOM_POSSIBLY_FAIL in jsutil.h. */
JS_PUBLIC_DATA(JSUint32) OOM_maxAllocations = (JSUint32)-1;
JS_PUBLIC_DATA(JSUint32) OOM_counter = 0;
#endif

/*
 * Checks the assumption that JS_FUNC_TO_DATA_PTR and JS_DATA_TO_FUNC_PTR
 * macros uses to implement casts between function and data pointers.
 */
JS_STATIC_ASSERT(sizeof(void *) == sizeof(void (*)()));

JS_PUBLIC_API(void) JS_Assert(const char *s, const char *file, JSIntn ln)
{
    fprintf(stderr, "Assertion failure: %s, at %s:%d\n", s, file, ln);
    fflush(stderr);
#if defined(WIN32)
    /*
     * We used to call DebugBreak() on Windows, but amazingly, it causes
     * the MSVS 2010 debugger not to be able to recover a call stack.
     */
    *((int *) NULL) = 0;
    exit(3);
#elif defined(__APPLE__)
    /*
     * On Mac OS X, Breakpad ignores signals. Only real Mach exceptions are
     * trapped.
     */
    *((int *) NULL) = 0;  /* To continue from here in GDB: "return" then "continue". */
    raise(SIGABRT);  /* In case above statement gets nixed by the optimizer. */
#else
    raise(SIGABRT);  /* To continue from here in GDB: "signal 0". */
#endif
}

#ifdef JS_BASIC_STATS

#include <math.h>
#include <string.h>
#include "jscompat.h"
#include "jsbit.h"

/*
 * Histogram bins count occurrences of values <= the bin label, as follows:
 *
 *   linear:  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10 or more
 *     2**x:  0,   1,   2,   4,   8,  16,  32,  64, 128, 256, 512 or more
 *    10**x:  0,   1,  10, 100, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9 or more
 *
 * We wish to count occurrences of 0 and 1 values separately, always.
 */
static uint32
BinToVal(uintN logscale, uintN bin)
{
    JS_ASSERT(bin <= 10);
    if (bin <= 1 || logscale == 0)
        return bin;
    --bin;
    if (logscale == 2)
        return JS_BIT(bin);
    JS_ASSERT(logscale == 10);
    return (uint32) pow(10.0, (double) bin);
}

static uintN
ValToBin(uintN logscale, uint32 val)
{
    uintN bin;

    if (val <= 1)
        return val;
    bin = (logscale == 10)
          ? (uintN) ceil(log10((double) val))
          : (logscale == 2)
          ? (uintN) JS_CeilingLog2(val)
          : val;
    return JS_MIN(bin, 10);
}

void
JS_BasicStatsAccum(JSBasicStats *bs, uint32 val)
{
    uintN oldscale, newscale, bin;
    double mean;

    ++bs->num;
    if (bs->max < val)
        bs->max = val;
    bs->sum += val;
    bs->sqsum += (double)val * val;

    oldscale = bs->logscale;
    if (oldscale != 10) {
        mean = bs->sum / bs->num;
        if (bs->max > 16 && mean > 8) {
            newscale = (bs->max > 1e6 && mean > 1000) ? 10 : 2;
            if (newscale != oldscale) {
                uint32 newhist[11], newbin;

                PodArrayZero(newhist);
                for (bin = 0; bin <= 10; bin++) {
                    newbin = ValToBin(newscale, BinToVal(oldscale, bin));
                    newhist[newbin] += bs->hist[bin];
                }
                memcpy(bs->hist, newhist, sizeof bs->hist);
                bs->logscale = newscale;
            }
        }
    }

    bin = ValToBin(bs->logscale, val);
    ++bs->hist[bin];
}

double
JS_MeanAndStdDev(uint32 num, double sum, double sqsum, double *sigma)
{
    double var;

    if (num == 0 || sum == 0) {
        *sigma = 0;
        return 0;
    }

    var = num * sqsum - sum * sum;
    if (var < 0 || num == 1)
        var = 0;
    else
        var /= (double)num * (num - 1);

    /* Windows says sqrt(0.0) is "-1.#J" (?!) so we must test. */
    *sigma = (var != 0) ? sqrt(var) : 0;
    return sum / num;
}

void
JS_DumpBasicStats(JSBasicStats *bs, const char *title, FILE *fp)
{
    double mean, sigma;

    mean = JS_MeanAndStdDevBS(bs, &sigma);
    fprintf(fp, "\nmean %s %g, std. deviation %g, max %lu\n",
            title, mean, sigma, (unsigned long) bs->max);
    JS_DumpHistogram(bs, fp);
}

void
JS_DumpHistogram(JSBasicStats *bs, FILE *fp)
{
    uintN bin;
    uint32 cnt, max;
    double sum, mean;

    for (bin = 0, max = 0, sum = 0; bin <= 10; bin++) {
        cnt = bs->hist[bin];
        if (max < cnt)
            max = cnt;
        sum += cnt;
    }
    mean = sum / cnt;
    for (bin = 0; bin <= 10; bin++) {
        uintN val = BinToVal(bs->logscale, bin);
        uintN end = (bin == 10) ? 0 : BinToVal(bs->logscale, bin + 1);
        cnt = bs->hist[bin];
        if (val + 1 == end)
            fprintf(fp, "        [%6u]", val);
        else if (end != 0)
            fprintf(fp, "[%6u, %6u]", val, end - 1);
        else
            fprintf(fp, "[%6u,   +inf]", val);
        fprintf(fp, ": %8u ", cnt);
        if (cnt != 0) {
            if (max > 1e6 && mean > 1e3)
                cnt = (uint32) ceil(log10((double) cnt));
            else if (max > 16 && mean > 8)
                cnt = JS_CeilingLog2(cnt);
            for (uintN i = 0; i < cnt; i++)
                putc('*', fp);
        }
        putc('\n', fp);
    }
}

#endif /* JS_BASIC_STATS */

#if defined(DEBUG_notme) && defined(XP_UNIX)

#define __USE_GNU 1
#include <dlfcn.h>
#include <string.h>
#include "jshash.h"
#include "jsprf.h"

JSCallsite js_calltree_root = {0, NULL, NULL, 0, NULL, NULL, NULL, NULL};

static JSCallsite *
CallTree(void **bp)
{
    void **bpup, **bpdown, *pc;
    JSCallsite *parent, *site, **csp;
    Dl_info info;
    int ok, offset;
    const char *symbol;
    char *method;

    /* Reverse the stack frame list to avoid recursion. */
    bpup = NULL;
    for (;;) {
        bpdown = (void**) bp[0];
        bp[0] = (void*) bpup;
        if ((void**) bpdown[0] < bpdown)
            break;
        bpup = bp;
        bp = bpdown;
    }

    /* Reverse the stack again, finding and building a path in the tree. */
    parent = &js_calltree_root;
    do {
        bpup = (void**) bp[0];
        bp[0] = (void*) bpdown;
        pc = bp[1];

        csp = &parent->kids;
        while ((site = *csp) != NULL) {
            if (site->pc == (uint32)pc) {
                /* Put the most recently used site at the front of siblings. */
                *csp = site->siblings;
                site->siblings = parent->kids;
                parent->kids = site;

                /* Site already built -- go up the stack. */
                goto upward;
            }
            csp = &site->siblings;
        }

        /* Check for recursion: see if pc is on our ancestor line. */
        for (site = parent; site; site = site->parent) {
            if (site->pc == (uint32)pc)
                goto upward;
        }

        /*
         * Not in tree at all: let's find our symbolic callsite info.
         * XXX static syms are masked by nearest lower global
         */
        info.dli_fname = info.dli_sname = NULL;
        ok = dladdr(pc, &info);
        if (ok < 0) {
            fprintf(stderr, "dladdr failed!\n");
            return NULL;
        }

/* XXXbe sub 0x08040000? or something, see dbaron bug with tenthumbs comment */
        symbol = info.dli_sname;
        offset = (char*)pc - (char*)info.dli_fbase;
        method = symbol
                 ? strdup(symbol)
                 : JS_smprintf("%s+%X",
                               info.dli_fname ? info.dli_fname : "main",
                               offset);
        if (!method)
            return NULL;

        /* Create a new callsite record. */
        site = (JSCallsite *) js_malloc(sizeof(JSCallsite));
        if (!site)
            return NULL;

        /* Insert the new site into the tree. */
        site->pc = (uint32)pc;
        site->name = method;
        site->library = info.dli_fname;
        site->offset = offset;
        site->parent = parent;
        site->siblings = parent->kids;
        parent->kids = site;
        site->kids = NULL;

      upward:
        parent = site;
        bpdown = bp;
        bp = bpup;
    } while (bp);

    return site;
}

JS_FRIEND_API(JSCallsite *)
JS_Backtrace(int skip)
{
    void **bp, **bpdown;

    /* Stack walking code adapted from Kipp's "leaky". */
#if defined(__i386)
    __asm__( "movl %%ebp, %0" : "=g"(bp));
#elif defined(__x86_64__)
    __asm__( "movq %%rbp, %0" : "=g"(bp));
#else
    /*
     * It would be nice if this worked uniformly, but at least on i386 and
     * x86_64, it stopped working with gcc 4.1, because it points to the
     * end of the saved registers instead of the start.
     */
    bp = (void**) __builtin_frame_address(0);
#endif
    while (--skip >= 0) {
        bpdown = (void**) *bp++;
        if (bpdown < bp)
            break;
        bp = bpdown;
    }

    return CallTree(bp);
}

JS_FRIEND_API(void)
JS_DumpBacktrace(JSCallsite *trace)
{
    while (trace) {
        fprintf(stdout, "%s [%s +0x%X]\n", trace->name, trace->library,
                trace->offset);
        trace = trace->parent;
    }
}

#endif /* defined(DEBUG_notme) && defined(XP_UNIX) */
