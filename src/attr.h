/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_attr_h__
#define INCLUDE_attr_h__

#include "attr_file.h"
#include "strmap.h"

#define GIT_ATTR_CONFIG   "core.attributesfile"
#define GIT_IGNORE_CONFIG "core.excludesfile"

typedef struct {
	int initialized;
	git_pool pool;
	git_strmap *files;	/* hash path to git_attr_file of rules */
	git_strmap *macros;	/* hash name to vector<git_attr_assignment> */
	const char *cfg_attr_file; /* cached value of core.attributesfile */
	const char *cfg_excl_file; /* cached value of core.excludesfile */
} git_attr_cache;

extern int git_attr_cache__init(git_repository *repo);

extern int git_attr_cache__insert_macro(
	git_repository *repo, git_attr_rule *macro);

extern git_attr_rule *git_attr_cache__lookup_macro(
	git_repository *repo, const char *name);

extern int git_attr_cache__lookup_or_create_file(
	git_repository *repo,
	const char *key,
	const char *filename,
	int (*loader)(git_repository *, const char *, git_attr_file *),
	git_attr_file **file_ptr);

extern int git_attr_cache__push_file(
	git_repository *repo,
	git_vector     *stack,
	const char     *base,
	const char     *filename,
	int (*loader)(git_repository *, const char *, git_attr_file *));

/* returns true if path is in cache */
extern bool git_attr_cache__is_cached(git_repository *repo, const char *path);

#endif
