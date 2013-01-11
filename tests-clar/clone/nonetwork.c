#include "clar_libgit2.h"

#include "git2/clone.h"
#include "repository.h"

#define LIVE_REPO_URL "git://github.com/libgit2/TestGitRepository"

static git_clone_options g_options;
static git_repository *g_repo;
static git_reference* g_ref;
static git_remote* g_remote;

void test_clone_nonetwork__initialize(void)
{
	git_checkout_opts dummy_opts = GIT_CHECKOUT_OPTS_INIT;

	g_repo = NULL;

	memset(&g_options, 0, sizeof(git_clone_options));
	g_options.version = GIT_CLONE_OPTIONS_VERSION;
	g_options.checkout_opts = dummy_opts;
	g_options.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
}

void test_clone_nonetwork__cleanup(void)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}

	if (g_ref) {
		git_reference_free(g_ref);
		g_ref = NULL;
	}

	if (g_remote) {
		git_remote_free(g_remote);
		g_remote = NULL;
	}

	cl_fixture_cleanup("./foo");
}

void test_clone_nonetwork__bad_url(void)
{
	/* Clone should clean up the mess if the URL isn't a git repository */
	cl_git_fail(git_clone(&g_repo, "not_a_repo", "./foo", &g_options));
	cl_assert(!git_path_exists("./foo"));
	g_options.bare = true;
	cl_git_fail(git_clone(&g_repo, "not_a_repo", "./foo", &g_options));
	cl_assert(!git_path_exists("./foo"));
}

void test_clone_nonetwork__local(void)
{
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__local_absolute_path(void)
{
	const char *local_src;
	local_src = cl_fixture("testrepo.git");
	cl_git_pass(git_clone(&g_repo, local_src, "./foo", &g_options));
}

void test_clone_nonetwork__local_bare(void)
{
	g_options.bare = true;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__fail_when_the_target_is_a_file(void)
{
	cl_git_mkfile("./foo", "Bar!");
	cl_git_fail(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__fail_with_already_existing_but_non_empty_directory(void)
{
	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_mkfile("./foo/bar", "Baz!");
	cl_git_fail(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__custom_origin_name(void)
{
	g_options.remote_name = "my_origin";
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_git_pass(git_remote_load(&g_remote, g_repo, "my_origin"));
}

void test_clone_nonetwork__custom_push_url(void)
{
	const char *url = "http://example.com";

	g_options.pushurl = url;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_git_pass(git_remote_load(&g_remote, g_repo, "origin"));
	cl_assert_equal_s(url, git_remote_pushurl(g_remote));
}

void test_clone_nonetwork__custom_fetch_spec(void)
{
	const git_refspec *actual_fs;
	const char *spec = "+refs/heads/master:refs/heads/foo";

	g_options.fetch_spec = spec;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_git_pass(git_remote_load(&g_remote, g_repo, "origin"));
	actual_fs = git_remote_fetchspec(g_remote);
	cl_assert_equal_s("refs/heads/master", git_refspec_src(actual_fs));
	cl_assert_equal_s("refs/heads/foo", git_refspec_dst(actual_fs));

	cl_git_pass(git_reference_lookup(&g_ref, g_repo, "refs/heads/foo"));
}

void test_clone_nonetwork__custom_push_spec(void)
{
	const git_refspec *actual_fs;
	const char *spec = "+refs/heads/master:refs/heads/foo";

	g_options.push_spec = spec;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_git_pass(git_remote_load(&g_remote, g_repo, "origin"));
	actual_fs = git_remote_pushspec(g_remote);
	cl_assert_equal_s("refs/heads/master", git_refspec_src(actual_fs));
	cl_assert_equal_s("refs/heads/foo", git_refspec_dst(actual_fs));
}

void test_clone_nonetwork__custom_autotag(void)
{
	git_strarray tags = {0};

	g_options.remote_autotag = GIT_REMOTE_DOWNLOAD_TAGS_NONE;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_git_pass(git_tag_list(&tags, g_repo));
	cl_assert_equal_i(0, tags.count);

	git_strarray_free(&tags);
}

void test_clone_nonetwork__cope_with_already_existing_directory(void)
{
	p_mkdir("./foo", GIT_DIR_MODE);
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));
}

void test_clone_nonetwork__can_prevent_the_checkout_of_a_standard_repo(void)
{
	git_buf path = GIT_BUF_INIT;

	g_options.checkout_opts.checkout_strategy = 0;
	cl_git_pass(git_clone(&g_repo, cl_git_fixture_url("testrepo.git"), "./foo", &g_options));

	cl_git_pass(git_buf_joinpath(&path, git_repository_workdir(g_repo), "master.txt"));
	cl_assert_equal_i(false, git_path_isfile(git_buf_cstr(&path)));

	git_buf_free(&path);
}

