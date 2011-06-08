/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#include "PageBlock.h"
#include "wtf/Assertions.h"

#if WTF_OS_UNIX && !WTF_OS_SYMBIAN
#include <unistd.h>
#endif

#if WTF_OS_WINDOWS
#include <malloc.h>
#include <windows.h>
#endif

#if WTF_OS_SYMBIAN
#include <e32hal.h>
#include <e32std.h>
#endif

namespace WTF {

static size_t s_pageSize;

#if WTF_OS_UNIX && !WTF_OS_SYMBIAN

inline size_t systemPageSize()
{
    return getpagesize();
}

#elif WTF_OS_WINDOWS

inline size_t systemPageSize()
{
    static size_t size = 0;
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    size = system_info.dwPageSize;
    return size;
}

#elif WTF_OS_SYMBIAN

inline size_t systemPageSize()
{
    static TInt page_size = 0;
    UserHal::PageSizeInBytes(page_size);
    return page_size;
}

#endif

size_t pageSize()
{
    if (!s_pageSize)
        s_pageSize = systemPageSize();
    ASSERT(isPowerOfTwo(s_pageSize));
    return s_pageSize;
}

} // namespace WTF
