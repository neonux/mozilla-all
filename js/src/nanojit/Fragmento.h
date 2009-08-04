/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
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
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *   Mozilla TraceMonkey Team
 *   Asko Tontti <atontti@cc.hut.fi>
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


#ifndef __nanojit_Fragmento__
#define __nanojit_Fragmento__

namespace nanojit
{
    struct GuardRecord;
    class Assembler;

    typedef avmplus::GCSortedMap<const void*, uint32_t, avmplus::LIST_NonGCObjects> BlockSortedMap;
    class BlockHist: public BlockSortedMap
    {
    public:
        BlockHist(avmplus::GC*gc) : BlockSortedMap(gc)
        {
        }
        uint32_t count(const void *p) {
            uint32_t c = 1+get(p);
            put(p, c);
            return c;
        }
    };

    struct fragstats;
    /*
     *
     * This is the main control center for creating and managing fragments.
     */
    class Fragmento : public avmplus::GCFinalizedObject
    {
        public:
            Fragmento(AvmCore* core, LogControl* logc, uint32_t cacheSizeLog2, CodeAlloc *codeAlloc);
            ~Fragmento();

            AvmCore*    core();

            Fragment*   getLoop(const void* ip);
            Fragment*   getAnchor(const void* ip);
            // Remove one fragment. The caller is responsible for making sure
            // that this does not destroy any resources shared with other
            // fragments (such as a LirBuffer or this fragment itself as a
            // jump target).
            void        clearFrags();    // clear all fragments from the cache
            Fragment*   createBranch(SideExit *exit, const void* ip);
            Fragment*   newFrag(const void* ip);
            Fragment*   newBranch(Fragment *from, const void* ip);

            verbose_only ( uint32_t pageCount(); )
            verbose_only( void addLabel(Fragment* f, const char *prefix, int id); )

            // stats
            struct
            {
                uint32_t    pages;                    // pages consumed
                uint32_t    flushes, ilsize, abcsize, compiles, totalCompiles;
            }
            _stats;

            verbose_only( DWB(BlockHist*)        enterCounts; )
            verbose_only( DWB(BlockHist*)        mergeCounts; )
            verbose_only( LabelMap*        labels; )

            #ifdef AVMPLUS_VERBOSE
            void    drawTrees(char *fileName);
            #endif

            void        clearFragment(Fragment *f);
        private:
            AvmCore*        _core;
            CodeAlloc*      _codeAlloc;
            FragmentMap     _frags;        /* map from ip -> Fragment ptr  */

            const uint32_t _max_pages;
            uint32_t _pagesGrowth;
    };

    enum TraceKind {
        LoopTrace,
        BranchTrace,
        MergeTrace
    };

    /**
     * Fragments are linear sequences of native code that have a single entry
     * point at the start of the fragment and may have one or more exit points
     *
     * It may turn out that that this arrangement causes too much traffic
     * between d and i-caches and that we need to carve up the structure differently.
     */
    class Fragment : public avmplus::GCFinalizedObject
    {
        public:
            Fragment(const void*);
            ~Fragment();

            NIns*            code()                            { return _code; }
            void            setCode(NIns* codee)               { _code = codee; }
            int32_t&        hits()                             { return _hits; }
            void            blacklist();
            bool            isBlacklisted()        { return _hits < 0; }
            void            releaseLirBuffer();
            void            releaseCode(CodeAlloc *alloc);
            void            releaseTreeMem(CodeAlloc *alloc);
            bool            isAnchor() { return anchor == this; }
            bool            isRoot() { return root == this; }
            void            onDestroy();

            verbose_only( uint32_t        _called; )
            verbose_only( uint32_t        _native; )
            verbose_only( uint32_t      _exitNative; )
            verbose_only( uint32_t        _lir; )
            verbose_only( uint32_t        _lirbytes; )
            verbose_only( const char*    _token; )
            verbose_only( uint64_t      traceTicks; )
            verbose_only( uint64_t      interpTicks; )
            verbose_only( DWB(Fragment*) eot_target; )
            verbose_only( uint32_t        sid;)
            verbose_only( uint32_t        compileNbr;)

            DWB(Fragment*) treeBranches;
            DWB(Fragment*) branches;
            DWB(Fragment*) nextbranch;
            DWB(Fragment*) anchor;
            DWB(Fragment*) root;
            DWB(Fragment*) parent;
            DWB(Fragment*) first;
            DWB(Fragment*) peer;
            LirBuffer*     lirbuf;
            LIns*          lastIns;
            SideExit*      spawnedFrom;
            GuardRecord*   outbound;

            TraceKind kind;
            const void* ip;
            uint32_t guardCount;
            uint32_t xjumpCount;
            uint32_t recordAttempts;
            int32_t blacklistLevel;
            NIns* fragEntry;
            NIns* loopEntry;
            void* vmprivate;
            CodeList* codeList;

        private:
            NIns*            _code;        // ptr to start of code
            int32_t          _hits;
    };
}
#endif // __nanojit_Fragmento__
