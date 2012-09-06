#include "clar_libgit2.h"
#include "diff_helpers.h"

git_tree *resolve_commit_oid_to_tree(
	git_repository *repo,
	const char *partial_oid)
{
	size_t len = strlen(partial_oid);
	git_oid oid;
	git_object *obj = NULL;
	git_tree *tree = NULL;

	if (git_oid_fromstrn(&oid, partial_oid, len) == 0)
		git_object_lookup_prefix(&obj, repo, &oid, len, GIT_OBJ_ANY);
	cl_assert(obj);
	if (git_object_type(obj) == GIT_OBJ_TREE)
		return (git_tree *)obj;
	cl_assert(git_object_type(obj) == GIT_OBJ_COMMIT);
	cl_git_pass(git_commit_tree(&tree, (git_commit *)obj));
	git_object_free(obj);
	return tree;
}

int diff_file_fn(
	void *cb_data,
	git_diff_delta *delta,
	float progress)
{
	diff_expects *e = cb_data;

	GIT_UNUSED(progress);

	if (delta->binary)
		e->at_least_one_of_them_is_binary = true;

	e->files++;
	switch (delta->status) {
	case GIT_DELTA_ADDED: e->file_adds++; break;
	case GIT_DELTA_DELETED: e->file_dels++; break;
	case GIT_DELTA_MODIFIED: e->file_mods++; break;
	case GIT_DELTA_IGNORED: e->file_ignored++; break;
	case GIT_DELTA_UNTRACKED: e->file_untracked++; break;
	case GIT_DELTA_UNMODIFIED: e->file_unmodified++; break;
	default: break;
	}
	return 0;
}

int diff_hunk_fn(
	void *cb_data,
	git_diff_delta *delta,
	git_diff_range *range,
	const char *header,
	size_t header_len)
{
	diff_expects *e = cb_data;

	GIT_UNUSED(delta);
	GIT_UNUSED(header);
	GIT_UNUSED(header_len);

	e->hunks++;
	e->hunk_old_lines += range->old_lines;
	e->hunk_new_lines += range->new_lines;
	return 0;
}

int diff_line_fn(
	void *cb_data,
	git_diff_delta *delta,
	git_diff_range *range,
	char line_origin,
	const char *content,
	size_t content_len)
{
	diff_expects *e = cb_data;

	GIT_UNUSED(delta);
	GIT_UNUSED(range);
	GIT_UNUSED(content);
	GIT_UNUSED(content_len);

	e->lines++;
	switch (line_origin) {
	case GIT_DIFF_LINE_CONTEXT:
		e->line_ctxt++;
		break;
	case GIT_DIFF_LINE_ADDITION:
		e->line_adds++;
		break;
	case GIT_DIFF_LINE_ADD_EOFNL:
		assert(0);
		break;
	case GIT_DIFF_LINE_DELETION:
		e->line_dels++;
		break;
	case GIT_DIFF_LINE_DEL_EOFNL:
		/* technically not a line delete, but we'll count it as such */
		e->line_dels++;
		break;
	default:
		break;
	}
	return 0;
}

int diff_foreach_via_iterator(
	git_diff_list *diff,
	void *data,
	git_diff_file_fn file_cb,
	git_diff_hunk_fn hunk_cb,
	git_diff_data_fn line_cb)
{
	int error, curr, total;
	git_diff_iterator *iter;
	git_diff_delta *delta;

	if ((error = git_diff_iterator_new(&iter, diff)) < 0)
		return error;

	curr  = 0;
	total = git_diff_iterator_num_files(iter);

	while (!(error = git_diff_iterator_next_file(&delta, iter))) {
		git_diff_range *range;
		const char *hdr;
		size_t hdr_len;

		/* call file_cb for this file */
		if (file_cb != NULL && file_cb(data, delta, (float)curr / total) != 0)
			goto abort;

		if (!hunk_cb && !line_cb)
			continue;

		while (!(error = git_diff_iterator_next_hunk(
				&range, &hdr, &hdr_len, iter))) {
			char origin;
			const char *line;
			size_t line_len;

			if (hunk_cb && hunk_cb(data, delta, range, hdr, hdr_len) != 0)
				goto abort;

			if (!line_cb)
				continue;

			while (!(error = git_diff_iterator_next_line(
				&origin, &line, &line_len, iter))) {

				if (line_cb(data, delta, range, origin, line, line_len) != 0)
					goto abort;
			}

			if (error && error != GIT_ITEROVER)
				goto done;
		}

		if (error && error != GIT_ITEROVER)
			goto done;
	}

done:
	git_diff_iterator_free(iter);

	if (error == GIT_ITEROVER)
		error = 0;

	return error;

abort:
	git_diff_iterator_free(iter);
	giterr_clear();

	return GIT_EUSER;
}
