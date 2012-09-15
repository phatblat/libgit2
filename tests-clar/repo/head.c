#include "clar_libgit2.h"
#include "refs.h"

git_repository *repo;

void test_repo_head__initialize(void)
{
	repo = cl_git_sandbox_init("testrepo.git");
}

void test_repo_head__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_repo_head__head_detached(void)
{
	git_reference *ref;
	git_oid oid;

	cl_assert(git_repository_head_detached(repo) == 0);

	/* detach the HEAD */
	git_oid_fromstr(&oid, "c47800c7266a2be04c571c04d5a6614691ea99bd");
	cl_git_pass(git_reference_create_oid(&ref, repo, "HEAD", &oid, 1));
	cl_assert(git_repository_head_detached(repo) == 1);
	git_reference_free(ref);

	/* take the reop back to it's original state */
	cl_git_pass(git_reference_create_symbolic(&ref, repo, "HEAD", "refs/heads/master", 1));
	cl_assert(git_repository_head_detached(repo) == 0);

	git_reference_free(ref);
}

void test_repo_head__head_orphan(void)
{
	git_reference *ref;

	cl_assert(git_repository_head_orphan(repo) == 0);

	/* orphan HEAD */
	cl_git_pass(git_reference_create_symbolic(&ref, repo, "HEAD", "refs/heads/orphan", 1));
	cl_assert(git_repository_head_orphan(repo) == 1);
	git_reference_free(ref);

	/* take the reop back to it's original state */
	cl_git_pass(git_reference_create_symbolic(&ref, repo, "HEAD", "refs/heads/master", 1));
	cl_assert(git_repository_head_orphan(repo) == 0);

	git_reference_free(ref);
}
