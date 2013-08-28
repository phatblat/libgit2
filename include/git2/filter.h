/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_filter_h__
#define INCLUDE_git_filter_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "buffer.h"

/**
 * @file git2/filter.h
 * @brief Git filter APIs
 *
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Filters are applied in one of two directions: smudging - which is
 * exporting a file from the Git object database to the working directory,
 * and cleaning - which is importing a file from the working directory to
 * the Git object database.  These values control which direction of
 * change is being applied.
 */
typedef enum {
	GIT_FILTER_SMUDGE = 0,
	GIT_FILTER_TO_WORKTREE = GIT_FILTER_SMUDGE,
	GIT_FILTER_CLEAN = 1,
	GIT_FILTER_TO_ODB = GIT_FILTER_CLEAN,
} git_filter_mode_t;

/**
 * A filter that can transform file data
 *
 * This represents a filter that can be used to transform or even replace
 * file data.  Libgit2 includes one built in filter and it is possible to
 * write your own (see git2/sys/filter.h for information on that).
 *
 * The built in filter is:
 *
 * * "crlf" which uses the complex rules with the "text", "eol", and
 *   "crlf" file attributes to decide how to convert between LF and CRLF
 *   line endings
 */
typedef struct git_filter git_filter;

GIT_EXTERN(git_filter *) git_filter_lookup(const char *name);

#define GIT_FILTER_CRLF "crlf"

GIT_EXTERN(int) git_filter_apply_to_buffer(
	git_buffer *out,
	git_filter *filter,
	const git_buffer *input,
	const char *as_path,
	git_filter_mode_t mode);

GIT_END_DECL

/** @} */

#endif
