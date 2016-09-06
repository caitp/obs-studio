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

#include <stdlib.h>
#include <string.h>
#include "base.h"
#include "bmem.h"
#include "platform.h"
#include "threading.h"

/*
 * NOTE: totally jacked the mem alignment trick from ffmpeg, credit to them:
 *   http://www.ffmpeg.org/
 */

#define ALIGNMENT 32

/* TODO: use memalign for non-windows systems */
#if defined(_WIN32)
#define ALIGNED_MALLOC 1
#else
#define ALIGNMENT_HACK 1
#endif

static void *a_malloc(size_t size)
{
#ifdef ALIGNED_MALLOC
	return _aligned_malloc(size, ALIGNMENT);
#elif ALIGNMENT_HACK
	void *ptr = NULL;
	long diff;

	ptr  = malloc(size + ALIGNMENT);
	if (ptr) {
		diff = ((~(long)ptr) & (ALIGNMENT - 1)) + 1;
		ptr  = (char *)ptr + diff;
		((char *)ptr)[-1] = (char)diff;
	}

	return ptr;
#else
	return malloc(size);
#endif
}

static void *a_realloc(void *ptr, size_t size)
{
#ifdef ALIGNED_MALLOC
	return _aligned_realloc(ptr, size, ALIGNMENT);
#elif ALIGNMENT_HACK
	long diff;

	if (!ptr)
		return a_malloc(size);
	diff = ((char *)ptr)[-1];
	ptr = realloc((char*)ptr - diff, size + diff);
	if (ptr)
		ptr = (char *)ptr + diff;
	return ptr;
#else
	return realloc(ptr, size);
#endif
}

static void a_free(void *ptr)
{
#ifdef ALIGNED_MALLOC
	_aligned_free(ptr);
#elif ALIGNMENT_HACK
	if (ptr)
		free((char *)ptr - ((char*)ptr)[-1]);
#else
	free(ptr);
#endif
}

static struct base_allocator alloc = {a_malloc, a_realloc, a_free};
static long num_allocs = 0;

void base_set_allocator(struct base_allocator *defs)
{
	memcpy(&alloc, defs, sizeof(struct base_allocator));
}

struct s_alloc_list {
	void* ptr;
	const char* reason;
	struct s_alloc_list* next;
};

static struct s_alloc_list* alloc_list = NULL;
static pthread_mutex_t alloc_list_mutex = PTHREAD_MUTEX_INITIALIZER;
void addToList(struct s_alloc_list** list, void* ptr, const char* reason) {
	if (reason == NULL)
		reason = "unknown";
	pthread_mutex_lock(&alloc_list_mutex);
	struct s_alloc_list* new_list = malloc(sizeof(struct s_alloc_list));
	new_list->ptr = ptr;
	new_list->next = *list;
	new_list->reason = strdup(reason);
	*list = new_list;
	pthread_mutex_unlock(&alloc_list_mutex);
}

void removeFromList(struct s_alloc_list** list, void* ptr) {
	pthread_mutex_lock(&alloc_list_mutex);
	struct s_alloc_list* curr = (*list);
	struct s_alloc_list* prev = NULL;
	while (curr) {
		struct s_alloc_list* next = curr->next;
		if (curr->ptr == ptr) {
			if (prev)
				prev->next = next;
			if (curr == *list)
				*list = next;
			free(curr);
			break;
		}
		prev = curr;
		curr = next;
	}
	pthread_mutex_unlock(&alloc_list_mutex);
}

const char* concatFuncReason(const char* func, const char* reason) {
	size_t length = strlen(func) + strlen(reason) + 1;
	char* buf = malloc(length);
	*buf = 0;
	snprintf(buf, length, "%s %s", func, reason);
	return buf;
}

void *__bmalloc__(size_t size, const char* func, const char* reason)
{
	void *ptr = alloc.malloc(size);
	if (!ptr && !size)
		ptr = alloc.malloc(1);
	if (!ptr) {
		os_breakpoint();
		bcrash("Out of memory while trying to allocate %lu bytes",
				(unsigned long)size);
	}

	os_atomic_inc_long(&num_allocs);
	addToList(&alloc_list, ptr, concatFuncReason(func, reason));
	return ptr;
}

void *__brealloc__(void *ptr, size_t size, const char* func, const char* reason)
{
	if (!ptr)
		os_atomic_inc_long(&num_allocs);
	else
		removeFromList(&alloc_list, ptr);
	ptr = alloc.realloc(ptr, size);
	if (!ptr && !size)
		ptr = alloc.realloc(ptr, 1);
	if (!ptr) {
		os_breakpoint();
		bcrash("Out of memory while trying to allocate %lu bytes",
				(unsigned long)size);
	}
	if (ptr)
		addToList(&alloc_list, ptr, concatFuncReason(func, reason));

	return ptr;
}

void bfree(void *ptr)
{
	if (ptr) {
		os_atomic_dec_long(&num_allocs);
		removeFromList(&alloc_list, ptr);
	}
	alloc.free(ptr);
}

long bnum_allocs(void)
{
	return num_allocs;
}

void bmem_print_leaks(void)
{
	pthread_mutex_lock(&alloc_list_mutex);
	while (alloc_list) {
		struct s_alloc_list* next = alloc_list->next;
		blog(LOG_INFO, "  %p (%s)", alloc_list->ptr, alloc_list->reason);
		free(alloc_list->reason);
		free(alloc_list);
		alloc_list = next;
	}
	pthread_mutex_unlock(&alloc_list_mutex);
}

int base_get_alignment(void)
{
	return ALIGNMENT;
}

void *__bmemdup__(const void *ptr, size_t size, const char* func, const char* reason)
{
	void *out = __bmalloc__(size, func, reason);
	if (size)
		memcpy(out, ptr, size);

	return out;
}
