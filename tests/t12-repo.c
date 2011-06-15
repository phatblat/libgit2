/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "test_lib.h"
#include "test_helpers.h"

#include "odb.h"
#include "git2/odb_backend.h"
#include "repository.h"

typedef struct {
	git_odb_backend base;
	int position;
} fake_backend;

git_odb_backend *new_backend(int position)
{
	fake_backend *b;

	b = git__malloc(sizeof(fake_backend));
	if (b == NULL)
		return NULL;

	memset(b, 0x0, sizeof(fake_backend));
	b->position = position;
	return (git_odb_backend *)b;
}

int test_backend_sorting(git_odb *odb)
{
	unsigned int i;

	for (i = 0; i < odb->backends.length; ++i) {
		fake_backend *internal = *((fake_backend **)git_vector_get(&odb->backends, i));

		if (internal == NULL)
			return GIT_ERROR;

		if (internal->position != (int)i)
			return GIT_ERROR;
	}

	return GIT_SUCCESS;
}

BEGIN_TEST(odb0, "assure that ODB backends are properly sorted")
	git_odb *odb;
	must_pass(git_odb_new(&odb));
	must_pass(git_odb_add_backend(odb, new_backend(0), 5));
	must_pass(git_odb_add_backend(odb, new_backend(2), 3));
	must_pass(git_odb_add_backend(odb, new_backend(1), 4));
	must_pass(git_odb_add_backend(odb, new_backend(3), 1));
	must_pass(test_backend_sorting(odb));
	git_odb_close(odb);
END_TEST

BEGIN_TEST(odb1, "assure that alternate backends are properly sorted")
	git_odb *odb;
	must_pass(git_odb_new(&odb));
	must_pass(git_odb_add_backend(odb, new_backend(0), 5));
	must_pass(git_odb_add_backend(odb, new_backend(2), 3));
	must_pass(git_odb_add_backend(odb, new_backend(1), 4));
	must_pass(git_odb_add_backend(odb, new_backend(3), 1));
	must_pass(git_odb_add_alternate(odb, new_backend(4), 5));
	must_pass(git_odb_add_alternate(odb, new_backend(6), 3));
	must_pass(git_odb_add_alternate(odb, new_backend(5), 4));
	must_pass(git_odb_add_alternate(odb, new_backend(7), 1));
	must_pass(test_backend_sorting(odb));
	git_odb_close(odb);
END_TEST


#define STANDARD_REPOSITORY 0
#define BARE_REPOSITORY 1

static int ensure_repository_init(
	const char *working_directory,
	int repository_kind,
	const char *expected_path_index,
	const char *expected_path_repository,
	const char *expected_working_directory)
{
	char path_odb[GIT_PATH_MAX];
	git_repository *repo;

	if (gitfo_isdir(working_directory) == GIT_SUCCESS)
		return GIT_ERROR;

	git__joinpath(path_odb, expected_path_repository, GIT_OBJECTS_DIR);

	if (git_repository_init(&repo, working_directory, repository_kind) < GIT_SUCCESS)
		return GIT_ERROR;

	if (repo->path_workdir != NULL || expected_working_directory != NULL) {
		if (git__suffixcmp(repo->path_workdir, expected_working_directory) != 0)
			goto cleanup;
	}

	if (git__suffixcmp(repo->path_odb, path_odb) != 0)
		goto cleanup;

	if (git__suffixcmp(repo->path_repository, expected_path_repository) != 0)
		goto cleanup;

	if (repo->path_index != NULL || expected_path_index != NULL) {
		if (git__suffixcmp(repo->path_index, expected_path_index) != 0)
			goto cleanup;

		if (git_repository_is_bare(repo) == 1)
			goto cleanup;
	} else if (git_repository_is_bare(repo) == 0)
			goto cleanup;

	if (git_repository_is_empty(repo) == 0)
		goto cleanup;

	git_repository_free(repo);
	rmdir_recurs(working_directory);

	return GIT_SUCCESS;

cleanup:
	git_repository_free(repo);
	rmdir_recurs(working_directory);
	return GIT_ERROR;
}

BEGIN_TEST(init0, "initialize a standard repo")
	char path_index[GIT_PATH_MAX], path_repository[GIT_PATH_MAX];

	git__joinpath(path_repository, TEMP_REPO_FOLDER, GIT_DIR);
	git__joinpath(path_index, path_repository, GIT_INDEX_FILE);

	must_pass(ensure_repository_init(TEMP_REPO_FOLDER, STANDARD_REPOSITORY, path_index, path_repository, TEMP_REPO_FOLDER));
	must_pass(ensure_repository_init(TEMP_REPO_FOLDER_NS, STANDARD_REPOSITORY, path_index, path_repository, TEMP_REPO_FOLDER));
END_TEST

BEGIN_TEST(init1, "initialize a bare repo")
	char path_repository[GIT_PATH_MAX];

	git__joinpath(path_repository, TEMP_REPO_FOLDER, "");

	must_pass(ensure_repository_init(TEMP_REPO_FOLDER, BARE_REPOSITORY, NULL, path_repository, NULL));
	must_pass(ensure_repository_init(TEMP_REPO_FOLDER_NS, BARE_REPOSITORY, NULL, path_repository, NULL));
END_TEST

BEGIN_TEST(init2, "Initialize and open a bare repo with a relative path escaping out of the current working directory")
	char path_repository[GIT_PATH_MAX];
	char current_workdir[GIT_PATH_MAX];
	const int mode = 0755; /* or 0777 ? */
	git_repository* repo;

	must_pass(gitfo_getcwd(current_workdir, sizeof(current_workdir)));

	git__joinpath(path_repository, TEMP_REPO_FOLDER, "a/b/c/");
	must_pass(gitfo_mkdir_recurs(path_repository, mode));

	must_pass(chdir(path_repository));

	must_pass(git_repository_init(&repo, "../d/e.git", 1));
	must_pass(git__suffixcmp(repo->path_repository, "/a/b/d/e.git/"));

	git_repository_free(repo);

	must_pass(git_repository_open(&repo, "../d/e.git"));

	git_repository_free(repo);

	must_pass(chdir(current_workdir));
	rmdir_recurs(TEMP_REPO_FOLDER);
END_TEST

#define EMPTY_BARE_REPOSITORY_NAME		"empty_bare.git"
#define EMPTY_BARE_REPOSITORY_FOLDER	TEST_RESOURCES "/" EMPTY_BARE_REPOSITORY_NAME "/"

BEGIN_TEST(open0, "Open a bare repository that has just been initialized by git")
	git_repository *repo;

	must_pass(copydir_recurs(EMPTY_BARE_REPOSITORY_FOLDER, TEMP_REPO_FOLDER));
	must_pass(remove_placeholders(TEMP_REPO_FOLDER, "dummy-marker.txt"));

	must_pass(git_repository_open(&repo, TEMP_REPO_FOLDER));
	must_be_true(git_repository_path(repo, GIT_REPO_PATH) != NULL);
	must_be_true(git_repository_path(repo, GIT_REPO_PATH_WORKDIR) == NULL);

	git_repository_free(repo);
	must_pass(rmdir_recurs(TEMP_REPO_FOLDER));
END_TEST

#define SOURCE_EMPTY_REPOSITORY_NAME	"empty_standard_repo/.gitted"
#define EMPTY_REPOSITORY_NAME			"empty_standard_repo/.git"
#define EMPTY_REPOSITORY_FOLDER			TEST_RESOURCES "/" SOURCE_EMPTY_REPOSITORY_NAME "/"
#define DEST_REPOSITORY_FOLDER			TEMP_REPO_FOLDER DOT_GIT "/"

BEGIN_TEST(open1, "Open a standard repository that has just been initialized by git")
	git_repository *repo;

	must_pass(copydir_recurs(EMPTY_REPOSITORY_FOLDER, DEST_REPOSITORY_FOLDER));
	must_pass(remove_placeholders(DEST_REPOSITORY_FOLDER, "dummy-marker.txt"));

	must_pass(git_repository_open(&repo, DEST_REPOSITORY_FOLDER));
	must_be_true(git_repository_path(repo, GIT_REPO_PATH) != NULL);
	must_be_true(git_repository_path(repo, GIT_REPO_PATH_WORKDIR) != NULL);

	git_repository_free(repo);
	must_pass(rmdir_recurs(TEMP_REPO_FOLDER));
END_TEST


BEGIN_TEST(open2, "Open a bare repository with a relative path escaping out of the current working directory")
	char new_current_workdir[GIT_PATH_MAX];
	char current_workdir[GIT_PATH_MAX];
	char path_repository[GIT_PATH_MAX];

	const int mode = 0755; /* or 0777 ? */
	git_repository* repo;

	/* Setup the repository to open */
	must_pass(gitfo_getcwd(current_workdir, sizeof(current_workdir)));
	strcpy(path_repository, current_workdir);
	git__joinpath_n(path_repository, 3, path_repository, TEMP_REPO_FOLDER, "a/d/e.git");
	must_pass(copydir_recurs(REPOSITORY_FOLDER, path_repository));

	/* Change the current working directory */
	git__joinpath(new_current_workdir, TEMP_REPO_FOLDER, "a/b/c/");
	must_pass(gitfo_mkdir_recurs(new_current_workdir, mode));
	must_pass(chdir(new_current_workdir));

	must_pass(git_repository_open(&repo, "../../d/e.git"));

	git_repository_free(repo);

	must_pass(chdir(current_workdir));
	rmdir_recurs(TEMP_REPO_FOLDER);
END_TEST

BEGIN_TEST(empty0, "test if a repository is empty or not")

	git_repository *repo_empty, *repo_normal;

	must_pass(git_repository_open(&repo_normal, REPOSITORY_FOLDER));
	must_be_true(git_repository_is_empty(repo_normal) == 0);
	git_repository_free(repo_normal);

	must_pass(git_repository_open(&repo_empty, EMPTY_BARE_REPOSITORY_FOLDER));
	must_be_true(git_repository_is_empty(repo_empty) == 1);
	git_repository_free(repo_empty);
END_TEST

#define DISCOVER_FOLDER TEST_RESOURCES "/discover.git"

#define SUB_REPOSITORY_FOLDER_NAME "sub_repo"
#define SUB_REPOSITORY_FOLDER DISCOVER_FOLDER "/" SUB_REPOSITORY_FOLDER_NAME
#define SUB_REPOSITORY_FOLDER_SUB SUB_REPOSITORY_FOLDER "/sub"
#define SUB_REPOSITORY_FOLDER_SUB_SUB SUB_REPOSITORY_FOLDER_SUB "/subsub"
#define SUB_REPOSITORY_FOLDER_SUB_SUB_SUB SUB_REPOSITORY_FOLDER_SUB_SUB "/subsubsub"

#define REPOSITORY_ALTERNATE_FOLDER DISCOVER_FOLDER "/alternate_sub_repo"
#define REPOSITORY_ALTERNATE_FOLDER_SUB REPOSITORY_ALTERNATE_FOLDER "/sub"
#define REPOSITORY_ALTERNATE_FOLDER_SUB_SUB REPOSITORY_ALTERNATE_FOLDER_SUB "/subsub"
#define REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB REPOSITORY_ALTERNATE_FOLDER_SUB_SUB "/subsubsub"

#define ALTERNATE_MALFORMED_FOLDER1 DISCOVER_FOLDER "/alternate_malformed_repo1"
#define ALTERNATE_MALFORMED_FOLDER2 DISCOVER_FOLDER "/alternate_malformed_repo2"
#define ALTERNATE_MALFORMED_FOLDER3 DISCOVER_FOLDER "/alternate_malformed_repo3"
#define ALTERNATE_NOT_FOUND_FOLDER DISCOVER_FOLDER "/alternate_not_found_repo"

static int ensure_repository_discover(const char *start_path, const char *ceiling_dirs, const char *expected_path)
{
	int error;
	char found_path[GIT_PATH_MAX];

	error = git_repository_discover(found_path, sizeof(found_path), start_path, 0, ceiling_dirs);
	//across_fs is always 0 as we can't automate the filesystem change tests

	if (error < GIT_SUCCESS)
		return error;

	return strcmp(found_path, expected_path) ? GIT_ERROR : GIT_SUCCESS;
}

static int write_file(const char *path, const char *content)
{
	int error;
	git_file file;

	if (gitfo_exists(path) == GIT_SUCCESS) {
		error = gitfo_unlink(path);

		if (error < GIT_SUCCESS)
			return error;
	}

	file = gitfo_creat_force(path, 0644);
	if (file < GIT_SUCCESS)
		return file;

	error = gitfo_write(file, (void*)content, strlen(content) * sizeof(char));

	gitfo_close(file);

	return error;
}

//no check is performed on ceiling_dirs length, so be sure it's long enough
static int append_ceiling_dir(char *ceiling_dirs, const char *path)
{
	int len = strlen(ceiling_dirs);
	int error;

	error = gitfo_prettify_dir_path(ceiling_dirs + len + (len ? 1 : 0), GIT_PATH_MAX, path, NULL);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to append ceiling directory.");

	if (len)
		ceiling_dirs[len] = GIT_PATH_LIST_SEPARATOR;

	return GIT_SUCCESS;
}

BEGIN_TEST(discover0, "test discover")
	git_repository *repo;
	char ceiling_dirs[GIT_PATH_MAX * 2] = "";
	char repository_path[GIT_PATH_MAX];
	char sub_repository_path[GIT_PATH_MAX];
	char found_path[GIT_PATH_MAX];
	int mode = 0755;

	rmdir_recurs(DISCOVER_FOLDER);
	must_pass(append_ceiling_dir(ceiling_dirs,TEST_RESOURCES));
	gitfo_mkdir_recurs(DISCOVER_FOLDER, mode);

	must_be_true(git_repository_discover(repository_path, sizeof(repository_path), DISCOVER_FOLDER, 0, ceiling_dirs) == GIT_ENOTAREPO);

	must_pass(git_repository_init(&repo, DISCOVER_FOLDER, 1));
	must_pass(git_repository_discover(repository_path, sizeof(repository_path), DISCOVER_FOLDER, 0, ceiling_dirs));

	must_pass(git_repository_init(&repo, SUB_REPOSITORY_FOLDER, 0));
	must_pass(gitfo_mkdir_recurs(SUB_REPOSITORY_FOLDER_SUB_SUB_SUB, mode));
	must_pass(git_repository_discover(sub_repository_path, sizeof(sub_repository_path), SUB_REPOSITORY_FOLDER, 0, ceiling_dirs));

	must_pass(gitfo_mkdir_recurs(SUB_REPOSITORY_FOLDER_SUB_SUB_SUB, mode));
	must_pass(ensure_repository_discover(SUB_REPOSITORY_FOLDER_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(SUB_REPOSITORY_FOLDER_SUB_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(SUB_REPOSITORY_FOLDER_SUB_SUB_SUB, ceiling_dirs, sub_repository_path));

	must_pass(gitfo_mkdir_recurs(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB, mode));
	must_pass(write_file(REPOSITORY_ALTERNATE_FOLDER "/" DOT_GIT, "gitdir: ../" SUB_REPOSITORY_FOLDER_NAME "/" DOT_GIT));
	must_pass(write_file(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB "/" DOT_GIT, "gitdir: ../../../" SUB_REPOSITORY_FOLDER_NAME "/" DOT_GIT));
	must_pass(write_file(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB "/" DOT_GIT, "gitdir: ../../../../"));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB, ceiling_dirs, repository_path));

	must_pass(gitfo_mkdir_recurs(ALTERNATE_MALFORMED_FOLDER1, mode));
	must_pass(write_file(ALTERNATE_MALFORMED_FOLDER1 "/" DOT_GIT, "Anything but not gitdir:"));
	must_pass(gitfo_mkdir_recurs(ALTERNATE_MALFORMED_FOLDER2, mode));
	must_pass(write_file(ALTERNATE_MALFORMED_FOLDER2 "/" DOT_GIT, "gitdir:"));
	must_pass(gitfo_mkdir_recurs(ALTERNATE_MALFORMED_FOLDER3, mode));
	must_pass(write_file(ALTERNATE_MALFORMED_FOLDER3 "/" DOT_GIT, "gitdir: \n\n\n"));
	must_pass(gitfo_mkdir_recurs(ALTERNATE_NOT_FOUND_FOLDER, mode));
	must_pass(write_file(ALTERNATE_NOT_FOUND_FOLDER "/" DOT_GIT, "gitdir: a_repository_that_surely_does_not_exist"));
	must_fail(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_MALFORMED_FOLDER1, 0, ceiling_dirs));
	must_fail(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_MALFORMED_FOLDER2, 0, ceiling_dirs));
	must_fail(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_MALFORMED_FOLDER3, 0, ceiling_dirs));
	must_be_true(git_repository_discover(found_path, sizeof(found_path), ALTERNATE_NOT_FOUND_FOLDER, 0, ceiling_dirs) == GIT_ENOTAREPO);

	must_pass(append_ceiling_dir(ceiling_dirs, SUB_REPOSITORY_FOLDER));
	//this must pass as ceiling_directories cannot predent the current
	//working directory to be checked
	must_pass(git_repository_discover(found_path, sizeof(found_path), SUB_REPOSITORY_FOLDER, 0, ceiling_dirs));
	must_be_true(git_repository_discover(found_path, sizeof(found_path), SUB_REPOSITORY_FOLDER_SUB, 0, ceiling_dirs) == GIT_ENOTAREPO);
	must_be_true(git_repository_discover(found_path, sizeof(found_path), SUB_REPOSITORY_FOLDER_SUB_SUB, 0, ceiling_dirs) == GIT_ENOTAREPO);
	must_be_true(git_repository_discover(found_path, sizeof(found_path), SUB_REPOSITORY_FOLDER_SUB_SUB_SUB, 0, ceiling_dirs) == GIT_ENOTAREPO);

	//.gitfile redirection should not be affected by ceiling directories
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB, ceiling_dirs, sub_repository_path));
	must_pass(ensure_repository_discover(REPOSITORY_ALTERNATE_FOLDER_SUB_SUB_SUB, ceiling_dirs, repository_path));

	rmdir_recurs(DISCOVER_FOLDER);
END_TEST

BEGIN_SUITE(repository)
	ADD_TEST(odb0);
	ADD_TEST(odb1);
	ADD_TEST(init0);
	ADD_TEST(init1);
	ADD_TEST(init2);
	ADD_TEST(open0);
	ADD_TEST(open1);
	ADD_TEST(open2);
	ADD_TEST(empty0);
	ADD_TEST(discover0);
END_SUITE

