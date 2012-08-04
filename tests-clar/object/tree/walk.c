#include "clar_libgit2.h"
#include "tree.h"

static const char *tree_oid = "1810dff58d8a660512d4832e740f692884338ccd";
static git_repository *g_repo;

void test_object_tree_walk__initialize(void)
{
   g_repo = cl_git_sandbox_init("testrepo");
}

void test_object_tree_walk__cleanup(void)
{
   cl_git_sandbox_cleanup();
}

static int treewalk_count_cb(
	const char *root, const git_tree_entry *entry, void *payload)
{
	int *count = payload;

	GIT_UNUSED(root);
	GIT_UNUSED(entry);

	(*count) += 1;

	return 0;
}

void test_object_tree_walk__0(void)
{
	git_oid id;
	git_tree *tree;
	int ct;

	git_oid_fromstr(&id, tree_oid);

	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));

	ct = 0;
	cl_git_pass(git_tree_walk(tree, treewalk_count_cb, GIT_TREEWALK_PRE, &ct));
	cl_assert_equal_i(3, ct);

	ct = 0;
	cl_git_pass(git_tree_walk(tree, treewalk_count_cb, GIT_TREEWALK_POST, &ct));
	cl_assert_equal_i(3, ct);

	git_tree_free(tree);
}


static int treewalk_stop_cb(
	const char *root, const git_tree_entry *entry, void *payload)
{
	int *count = payload;

	GIT_UNUSED(root);
	GIT_UNUSED(entry);

	(*count) += 1;

	return (*count == 2);
}

static int treewalk_stop_immediately_cb(
	const char *root, const git_tree_entry *entry, void *payload)
{
	GIT_UNUSED(root);
	GIT_UNUSED(entry);
	GIT_UNUSED(payload);
	return -100;
}

void test_object_tree_walk__1(void)
{
	git_oid id;
	git_tree *tree;
	int ct;

	git_oid_fromstr(&id, tree_oid);

	cl_git_pass(git_tree_lookup(&tree, g_repo, &id));

	ct = 0;
	cl_assert_equal_i(
		GIT_EUSER, git_tree_walk(tree, treewalk_stop_cb, GIT_TREEWALK_PRE, &ct));
	cl_assert_equal_i(2, ct);

	ct = 0;
	cl_assert_equal_i(
		GIT_EUSER, git_tree_walk(tree, treewalk_stop_cb, GIT_TREEWALK_POST, &ct));
	cl_assert_equal_i(2, ct);

	cl_assert_equal_i(
		GIT_EUSER, git_tree_walk(
			tree, treewalk_stop_immediately_cb, GIT_TREEWALK_PRE, NULL));

	cl_assert_equal_i(
		GIT_EUSER, git_tree_walk(
			tree, treewalk_stop_immediately_cb, GIT_TREEWALK_POST, NULL));

	git_tree_free(tree);
}
