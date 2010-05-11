/* -*- Mode: C; tab-width: 8; c-basic-offset: 8 -*- */
/* vim:set softtabstop=8 shiftwidth=8: */
/*-
 * Copyright (C) 2006-2008 Jason Evans <jasone@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _JEMALLOC_H_
#define _JEMALLOC_H_

/* grab size_t */
#ifdef _MSC_VER
#include <crtdefs.h>
#else
#include <stddef.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char jemalloc_bool;

extern const char	*_malloc_options;

/*
 * jemalloc_stats() is not a stable interface.  When using jemalloc_stats_t, be
 * sure that the compiled results of jemalloc.c are in sync with this header
 * file.
 */
typedef struct {
	/*
	 * Run-time configuration settings.
	 */
	jemalloc_bool	opt_abort;	/* abort(3) on error? */
	jemalloc_bool	opt_junk;	/* Fill allocated/free memory with 0xa5/0x5a? */
	jemalloc_bool	opt_utrace;	/* Trace all allocation events? */
	jemalloc_bool	opt_sysv;	/* SysV semantics? */
	jemalloc_bool	opt_xmalloc;	/* abort(3) on OOM? */
	jemalloc_bool	opt_zero;	/* Fill allocated memory with 0x0? */
	size_t	narenas;	/* Number of arenas. */
	size_t	balance_threshold; /* Arena contention rebalance threshold. */
	size_t	quantum;	/* Allocation quantum. */
	size_t	small_max;	/* Max quantum-spaced allocation size. */
	size_t	large_max;	/* Max sub-chunksize allocation size. */
	size_t	chunksize;	/* Size of each virtual memory mapping. */
	size_t	dirty_max;	/* Max dirty pages per arena. */

	/*
	 * Current memory usage statistics.
	 */
	size_t	mapped;		/* Bytes mapped (not necessarily committed). */
	size_t	committed;	/* Bytes committed (readable/writable). */
	size_t	allocated;	/* Bytes allocted (in use by application). */
	size_t	dirty;		/* Bytes dirty (committed unused pages). */
} jemalloc_stats_t;

/* Darwin and Linux already have memory allocation functions */
#if (!defined(MOZ_MEMORY_DARWIN) && !defined(MOZ_MEMORY_LINUX))
void	*malloc(size_t size);
void	*valloc(size_t size);
void	*calloc(size_t num, size_t size);
void	*realloc(void *ptr, size_t size);
void	free(void *ptr);
int	posix_memalign(void **memptr, size_t alignment, size_t size);
#endif /* MOZ_MEMORY_DARWIN, MOZ_MEMORY_LINUX */

/* Linux has memalign and malloc_usable_size */
#if !defined(MOZ_MEMORY_LINUX)
void	*memalign(size_t alignment, size_t size);
size_t	malloc_usable_size(const void *ptr);
#endif /* MOZ_MEMORY_LINUX */

void	jemalloc_stats(jemalloc_stats_t *stats);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _JEMALLOC_H_ */
