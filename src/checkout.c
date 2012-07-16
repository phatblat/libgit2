/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#include "git2/checkout.h"
#include "git2/repository.h"
#include "git2/refs.h"
#include "git2/tree.h"
#include "git2/commit.h"
#include "git2/blob.h"

#include "common.h"
#include "refs.h"
#include "buffer.h"
#include "repository.h"
#include "filter.h"
#include "blob.h"

GIT_BEGIN_DECL


typedef struct tree_walk_data
{
	git_indexer_stats *stats;
	git_repository *repo;
	git_odb *odb;
} tree_walk_data;


static int blob_contents_to_file(git_repository *repo, git_buf *fnbuf, const git_oid *id, int mode)
{
	int retcode = GIT_ERROR;

	git_buf filteredcontents = GIT_BUF_INIT;
	if (!git_filter_blob_contents(&filteredcontents, repo, id, git_buf_cstr(fnbuf))) {
		int fd = git_futils_creat_withpath(git_buf_cstr(fnbuf),
													  GIT_DIR_MODE, mode);
		if (fd >= 0) {
			if (!p_write(fd, git_buf_cstr(&filteredcontents),
							 git_buf_len(&filteredcontents)))
				retcode = 0;
			else
				retcode = GIT_ERROR;
			p_close(fd);
		}
	}
	git_buf_free(&filteredcontents);

	return retcode;
}

static int checkout_walker(const char *path, git_tree_entry *entry, void *payload)
{
	int retcode = 0;
	tree_walk_data *data = (tree_walk_data*)payload;
	int attr = git_tree_entry_attributes(entry);

	/* TODO: handle submodules  */

	if (S_ISLNK(attr)) {
		printf("It's a link!\n'");
	} else {
		switch(git_tree_entry_type(entry)) {
		case GIT_OBJ_TREE:
			/* Nothing to do; the blob handling creates necessary directories. */
			break;

		case GIT_OBJ_BLOB:
			{
				git_buf fnbuf = GIT_BUF_INIT;
				git_buf_join_n(&fnbuf, '/', 3,
									git_repository_workdir(data->repo),
									path,
									git_tree_entry_name(entry));
				retcode = blob_contents_to_file(data->repo, &fnbuf, git_tree_entry_id(entry), attr);
				git_buf_free(&fnbuf);
			}
			break;

		default:
			retcode = -1;
			break;
		}
	}

	data->stats->processed++;
	return retcode;
}


int git_checkout_force(git_repository *repo, git_indexer_stats *stats)
{
	int retcode = GIT_ERROR;
	git_indexer_stats dummy_stats;
	git_tree *tree;
	tree_walk_data payload;

	assert(repo);
	if (!stats) stats = &dummy_stats;

	if (git_repository_is_bare(repo)) {
		giterr_set(GITERR_INVALID, "Checkout is not allowed for bare repositories");
		return GIT_ERROR;
	}

	stats->total = stats->processed = 0;
	payload.stats = stats;
	payload.repo = repo;
	if (git_repository_odb(&payload.odb, repo) < 0) return GIT_ERROR;

	/* TODO: stats->total is never calculated. */

	if (!git_repository_head_tree(&tree, repo)) {
		/* Checkout the files */
		if (!git_tree_walk(tree, checkout_walker, GIT_TREEWALK_POST, &payload)) {
			retcode = 0;
		}
		git_tree_free(tree);
	}

	git_odb_free(payload.odb);
	return retcode;
}


GIT_END_DECL
