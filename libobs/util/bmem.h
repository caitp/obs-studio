/*
 * Copyright (c) 2013 Hugh Bailey <obs.jim@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include "c99defs.h"
#include "base.h"
#include <wchar.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

struct base_allocator {
	void *(*malloc)(size_t);
	void *(*realloc)(void *, size_t);
	void (*free)(void *);
};

#define __EXP1(X) #X
#define __EXP(X) __EXP1(X)
#define __PRETTY_MEM_REASON__  __PRETTY_FUNCTION__, " (" __EXP(__FILE__) ":" __EXP(__LINE__) ")"

EXPORT void base_set_allocator(struct base_allocator *defs);

EXPORT void *__bmalloc__(size_t size, const char* func, const char* reason);
#define bmalloc(size) __bmalloc__(size, __PRETTY_MEM_REASON__)
EXPORT void *__brealloc__(void *ptr, size_t size, const char* func, const char* reason);
#define brealloc(ptr, size) __brealloc__(ptr, size, __PRETTY_MEM_REASON__)
EXPORT void bfree(void *ptr);

EXPORT int base_get_alignment(void);

EXPORT long bnum_allocs(void);
EXPORT void bmem_print_leaks(void);

EXPORT void *__bmemdup__(const void *ptr, size_t size, const char* func, const char* reason);
#define bmemdup(ptr, size) __bmemdup__((ptr), (size), __PRETTY_MEM_REASON__)

static inline void *__bzalloc__(size_t size, const char* func, const char* reason)
{
	void *mem = __bmalloc__(size, func, reason);
	if (mem)
		memset(mem, 0, size);
	return mem;
}

#define bzalloc(size) __bzalloc__(size, __PRETTY_MEM_REASON__)

static inline char *__bstrdup_n__(const char *str, size_t n, const char* func, const char* reason)
{
	char *dup;
	if (!str)
		return NULL;

	dup = (char*)__bmemdup__(str, n+1, func, reason);
	dup[n] = 0;

	return dup;
}

#define bstrdup_n(str, n) __bstrdup_n__(str, n, __PRETTY_MEM_REASON__)

static inline wchar_t *__bwstrdup_n__(const wchar_t *str, size_t n, const char* func, const char* reason)
{
	wchar_t *dup;
	if (!str)
		return NULL;

	dup = (wchar_t*)__bmemdup__(str, (n+1) * sizeof(wchar_t), func, reason);
	dup[n] = 0;

	return dup;
}

#define bwstrdup_n(str, n) __bwstrdup_n__(str, n, __PRETTY_MEM_REASON__)

static inline char *__bstrdup__(const char *str, const char* func, const char* reason)
{
	if (!str)
		return NULL;

	return __bstrdup_n__(str, strlen(str), func, reason);
}

#define bstrdup(str) __bstrdup__(str, __PRETTY_MEM_REASON__)

static inline wchar_t *__bwstrdup__(const wchar_t *str, const char* func, const char* reason)
{
	if (!str)
		return NULL;

	return __bwstrdup_n__(str, wcslen(str), func, reason);
}
#define bwstrdup(str) __bwstrdup__(str, __PRETTY_MEM_REASON__)

#ifdef __cplusplus
}
#endif
