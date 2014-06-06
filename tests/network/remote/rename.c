#include "clar_libgit2.h"
#include "config/config_helpers.h"

#include "repository.h"

static git_remote *_remote;
static git_repository *_repo;

void test_network_remote_rename__initialize(void)
{
	_repo = cl_git_sandbox_init("testrepo.git");

	cl_git_pass(git_remote_load(&_remote, _repo, "test"));
}

void test_network_remote_rename__cleanup(void)
{
	git_remote_free(_remote);
	_remote = NULL;

	cl_git_sandbox_cleanup();
}

static int dont_call_me_cb(const char *fetch_refspec, void *payload)
{
	GIT_UNUSED(fetch_refspec);
	GIT_UNUSED(payload);

	cl_assert(false);

	return -1;
}

void test_network_remote_rename__renaming_a_remote_moves_related_configuration_section(void)
{
	git_strarray problems = {0};

	assert_config_entry_existence(_repo, "remote.test.fetch", true);
	assert_config_entry_existence(_repo, "remote.just/renamed.fetch", false);

	cl_git_pass(git_remote_rename(&problems, _remote, "just/renamed"));
	cl_assert_equal_i(0, problems.count);
	git_strarray_free(&problems);

	assert_config_entry_existence(_repo, "remote.test.fetch", false);
	assert_config_entry_existence(_repo, "remote.just/renamed.fetch", true);
}

void test_network_remote_rename__renaming_a_remote_updates_branch_related_configuration_entries(void)
{
	git_strarray problems = {0};

	assert_config_entry_value(_repo, "branch.master.remote", "test");

	cl_git_pass(git_remote_rename(&problems, _remote, "just/renamed"));
	cl_assert_equal_i(0, problems.count);
	git_strarray_free(&problems);

	assert_config_entry_value(_repo, "branch.master.remote", "just/renamed");
}

void test_network_remote_rename__renaming_a_remote_updates_default_fetchrefspec(void)
{
	git_strarray problems = {0};

	cl_git_pass(git_remote_rename(&problems, _remote, "just/renamed"));
	cl_assert_equal_i(0, problems.count);
	git_strarray_free(&problems);

	assert_config_entry_value(_repo, "remote.just/renamed.fetch", "+refs/heads/*:refs/remotes/just/renamed/*");
}

void test_network_remote_rename__renaming_a_remote_without_a_fetchrefspec_doesnt_create_one(void)
{
	git_config *config;
	git_strarray problems = {0};

	git_remote_free(_remote);
	cl_git_pass(git_repository_config__weakptr(&config, _repo));
	cl_git_pass(git_config_delete_entry(config, "remote.test.fetch"));

	cl_git_pass(git_remote_load(&_remote, _repo, "test"));

	assert_config_entry_existence(_repo, "remote.test.fetch", false);

	cl_git_pass(git_remote_rename(&problems, _remote, "just/renamed"));
	cl_assert_equal_i(0, problems.count);
	git_strarray_free(&problems);

	assert_config_entry_existence(_repo, "remote.just/renamed.fetch", false);
}

void test_network_remote_rename__renaming_a_remote_notifies_of_non_default_fetchrefspec(void)
{
	git_config *config;

	git_strarray problems = {0};

	git_remote_free(_remote);
	cl_git_pass(git_repository_config__weakptr(&config, _repo));
	cl_git_pass(git_config_set_string(config, "remote.test.fetch", "+refs/*:refs/*"));
	cl_git_pass(git_remote_load(&_remote, _repo, "test"));

	cl_git_pass(git_remote_rename(&problems, _remote, "just/renamed"));
	cl_assert_equal_i(1, problems.count);
	cl_assert_equal_s("+refs/*:refs/*", problems.strings[0]);
	git_strarray_free(&problems);

	assert_config_entry_value(_repo, "remote.just/renamed.fetch", "+refs/*:refs/*");

	git_strarray_free(&problems);
}

void test_network_remote_rename__new_name_can_contain_dots(void)
{
	git_strarray problems = {0};

	cl_git_pass(git_remote_rename(&problems, _remote, "just.renamed"));
	cl_assert_equal_i(0, problems.count);
	git_strarray_free(&problems);
	cl_assert_equal_s("just.renamed", git_remote_name(_remote));
}

void test_network_remote_rename__new_name_must_conform_to_reference_naming_conventions(void)
{
	git_strarray problems = {0};

	cl_assert_equal_i(
		GIT_EINVALIDSPEC,
		git_remote_rename(&problems, _remote, "new@{name"));
}

void test_network_remote_rename__renamed_name_is_persisted(void)
{
	git_remote *renamed;
	git_repository *another_repo;
	git_strarray problems = {0};

	cl_git_fail(git_remote_load(&renamed, _repo, "just/renamed"));

	cl_git_pass(git_remote_rename(&problems, _remote, "just/renamed"));
	cl_assert_equal_i(0, problems.count);
	git_strarray_free(&problems);

	cl_git_pass(git_repository_open(&another_repo, "testrepo.git"));
	cl_git_pass(git_remote_load(&renamed, _repo, "just/renamed"));

	git_remote_free(renamed);
	git_repository_free(another_repo);
}

void test_network_remote_rename__cannot_overwrite_an_existing_remote(void)
{
	git_strarray problems = {0};

	cl_assert_equal_i(GIT_EEXISTS, git_remote_rename(&problems, _remote, "test"));
	cl_assert_equal_i(GIT_EEXISTS, git_remote_rename(&problems, _remote, "test_with_pushurl"));
}

void test_network_remote_rename__renaming_a_remote_moves_the_underlying_reference(void)
{
	git_reference *underlying;
	git_strarray problems = {0};

	cl_assert_equal_i(GIT_ENOTFOUND, git_reference_lookup(&underlying, _repo, "refs/remotes/just/renamed"));
	cl_git_pass(git_reference_lookup(&underlying, _repo, "refs/remotes/test/master"));
	git_reference_free(underlying);

	cl_git_pass(git_remote_rename(&problems, _remote, "just/renamed"));
	cl_assert_equal_i(0, problems.count);
	git_strarray_free(&problems);

	cl_assert_equal_i(GIT_ENOTFOUND, git_reference_lookup(&underlying, _repo, "refs/remotes/test/master"));
	cl_git_pass(git_reference_lookup(&underlying, _repo, "refs/remotes/just/renamed/master"));
	git_reference_free(underlying);
}

void test_network_remote_rename__cannot_rename_an_inmemory_remote(void)
{
	git_remote *remote;
	git_strarray problems = {0};

	cl_git_pass(git_remote_create_anonymous(&remote, _repo, "file:///blah", NULL));
	cl_git_fail(git_remote_rename(&problems, remote, "newname"));

	git_strarray_free(&problems);
	git_remote_free(remote);
}

void test_network_remote_rename__overwrite_ref_in_target(void)
{
	git_oid id;
	char idstr[GIT_OID_HEXSZ + 1] = {0};
	git_remote *remote;
	git_reference *ref;
	git_branch_t btype;
	git_branch_iterator *iter;
	git_strarray problems = {0};

	cl_git_pass(git_oid_fromstr(&id, "a65fedf39aefe402d3bb6e24df4d4f5fe4547750"));
	cl_git_pass(git_reference_create(&ref, _repo, "refs/remotes/renamed/master", &id, 1, NULL, NULL));
	git_reference_free(ref);

	cl_git_pass(git_remote_load(&remote, _repo, "test"));
	cl_git_pass(git_remote_rename(&problems, remote, "renamed"));
	git_remote_free(remote);
	cl_assert_equal_i(0, problems.count);
	git_strarray_free(&problems);

	/* make sure there's only one remote-tracking branch */
	cl_git_pass(git_branch_iterator_new(&iter, _repo, GIT_BRANCH_REMOTE));
	cl_git_pass(git_branch_next(&ref, &btype, iter));
	cl_assert_equal_s("refs/remotes/renamed/master", git_reference_name(ref));
	git_oid_fmt(idstr, git_reference_target(ref));
	cl_assert_equal_s("be3563ae3f795b2b4353bcce3a527ad0a4f7f644", idstr);
	git_reference_free(ref);

	cl_git_fail_with(GIT_ITEROVER, git_branch_next(&ref, &btype, iter));
	git_branch_iterator_free(iter);
}
