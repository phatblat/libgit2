/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "buffer.h"
#include "posix.h"
#include <stdarg.h>

#define ENSURE_SIZE(b, d) \
	if ((ssize_t)(d) > buf->asize && git_buf_grow(b, (d)) < GIT_SUCCESS)\
		return;

int git_buf_grow(git_buf *buf, size_t target_size)
{
	char *new_ptr;

	if (buf->asize < 0)
		return GIT_ENOMEM;

	if (target_size <= (size_t)buf->asize)
		return GIT_SUCCESS;

	if (buf->asize == 0)
		buf->asize = target_size;

	/* grow the buffer size by 1.5, until it's big enough
	 * to fit our target size */
	while (buf->asize < (int)target_size)
		buf->asize = (buf->asize << 1) - (buf->asize >> 1);

	/* round allocation up to multiple of 8 */
	buf->asize = (buf->asize + 7) & ~7;

	new_ptr = git__realloc(buf->ptr, buf->asize);
	if (!new_ptr) {
		buf->asize = -1;
		return GIT_ENOMEM;
	}

	buf->ptr = new_ptr;
	return GIT_SUCCESS;
}

int git_buf_oom(const git_buf *buf)
{
	return (buf->asize < 0);
}

void git_buf_set(git_buf *buf, const char *data, size_t len)
{
	if (len == 0 || data == NULL) {
		git_buf_clear(buf);
	} else {
		ENSURE_SIZE(buf, len);
		memmove(buf->ptr, data, len);
		buf->size = len;
	}
}

void git_buf_sets(git_buf *buf, const char *string)
{
	git_buf_set(buf, string, string ? strlen(string) : 0);
}

void git_buf_putc(git_buf *buf, char c)
{
	ENSURE_SIZE(buf, buf->size + 1);
	buf->ptr[buf->size++] = c;
}

void git_buf_put(git_buf *buf, const char *data, size_t len)
{
	ENSURE_SIZE(buf, buf->size + len);
	memmove(buf->ptr + buf->size, data, len);
	buf->size += len;
}

void git_buf_puts(git_buf *buf, const char *string)
{
	if (string != NULL)
		git_buf_put(buf, string, strlen(string));
}

void git_buf_printf(git_buf *buf, const char *format, ...)
{
	int len;
	va_list arglist;

	ENSURE_SIZE(buf, buf->size + 1);

	while (1) {
		va_start(arglist, format);
		len = p_vsnprintf(buf->ptr + buf->size, buf->asize - buf->size, format, arglist);
		va_end(arglist);

		if (len < 0) {
			buf->asize = -1;
			return;
		}

		if (len + 1 <= buf->asize - buf->size) {
			buf->size += len;
			return;
		}

		ENSURE_SIZE(buf, buf->size + len + 1);
	}
}

const char *git_buf_cstr(git_buf *buf)
{
	if (buf->size + 1 > buf->asize &&
		git_buf_grow(buf, buf->size + 1) < GIT_SUCCESS)
		return NULL;

	buf->ptr[buf->size] = '\0';
	return buf->ptr;
}

void git_buf_free(git_buf *buf)
{
	if (buf) {
		if (buf->ptr) {
			git__free(buf->ptr);
			buf->ptr = NULL;
		}
		buf->asize = 0;
		buf->size = 0;
	}
}

void git_buf_clear(git_buf *buf)
{
	buf->size = 0;
}

void git_buf_consume(git_buf *buf, const char *end)
{
	if (end > buf->ptr && end <= buf->ptr + buf->size) {
		size_t consumed = end - buf->ptr;
		memmove(buf->ptr, end, buf->size - consumed);
		buf->size -= consumed;
	}
}

void git_buf_swap(git_buf *buf_a, git_buf *buf_b)
{
	git_buf t = *buf_a;
	*buf_a = *buf_b;
	*buf_b = t;
}

char *git_buf_take_cstr(git_buf *buf)
{
	char *data = NULL;

	if (buf->ptr == NULL)
		return NULL;

	if (buf->size + 1 > buf->asize &&
		git_buf_grow(buf, buf->size + 1) < GIT_SUCCESS)
		return NULL;

	data = buf->ptr;
	data[buf->size] = '\0';

	buf->ptr = NULL;
	buf->asize = 0;
	buf->size = 0;

	return data;
}

void git_buf_join(git_buf *buf, char separator, int nbuf, ...)
{
	/* Make two passes to avoid multiple reallocation */

	va_list ap;
	int i;
	int total_size = 0;
	char *out;

	if (buf->size > 0 && buf->ptr[buf->size - 1] != separator)
		++total_size; /* space for initial separator */

	va_start(ap, nbuf);
	for (i = 0; i < nbuf; ++i) {
		const char* segment;
		int segment_len;

		segment = va_arg(ap, const char *);
		if (!segment)
			continue;

		segment_len = strlen(segment);
		total_size += segment_len;
		if (segment_len == 0 || segment[segment_len - 1] != separator)
			++total_size; /* space for separator */
	}
	va_end(ap);

	ENSURE_SIZE(buf, buf->size + total_size);

	out = buf->ptr + buf->size;

	/* append separator to existing buf if needed */
	if (buf->size > 0 && out[-1] != separator)
		*out++ = separator;

	va_start(ap, nbuf);
	for (i = 0; i < nbuf; ++i) {
		const char* segment;
		int segment_len;

		segment = va_arg(ap, const char *);
		if (!segment)
			continue;

		/* skip leading separators */
		if (out > buf->ptr && out[-1] == separator)
			while (*segment == separator) segment++;

		/* copy over next buffer */
		segment_len = strlen(segment);
		if (segment_len > 0) {
			memmove(out, segment, segment_len);
			out += segment_len;
		}

		/* append trailing separator (except for last item) */
		if (i < nbuf - 1 && out > buf->ptr && out[-1] != separator)
			*out++ = separator;
	}
	va_end(ap);

	/* set size based on num characters actually written */
	buf->size = out - buf->ptr;
}

