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
#ifndef INCLUDE_git_commit_h__
#define INCLUDE_git_commit_h__

#include "common.h"
#include "types.h"
#include "oid.h"
#include "object.h"

/**
 * @file git2/commit.h
 * @brief Git commit parsing, formatting routines
 * @defgroup git_commit Git commit parsing, formatting routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Lookup a commit object from a repository.
 *
 * @param commit pointer to the looked up commit
 * @param repo the repo to use when locating the commit.
 * @param id identity of the commit to locate.  If the object is
 *        an annotated tag it will be peeled back to the commit.
 * @return 0 on success; error code otherwise
 */
GIT_INLINE(int) git_commit_lookup(git_commit **commit, git_repository *repo, const git_oid *id)
{
	return git_object_lookup((git_object **)commit, repo, id, GIT_OBJ_COMMIT);
}

/**
 * Get the id of a commit.
 *
 * @param commit a previously loaded commit.
 * @return object identity for the commit.
 */
GIT_EXTERN(const git_oid *) git_commit_id(git_commit *commit);

/**
 * Get the short (one line) message of a commit.
 *
 * @param commit a previously loaded commit.
 * @return the short message of a commit
 */
GIT_EXTERN(const char *) git_commit_message_short(git_commit *commit);

/**
 * Get the full message of a commit.
 *
 * @param commit a previously loaded commit.
 * @return the message of a commit
 */
GIT_EXTERN(const char *) git_commit_message(git_commit *commit);

/**
 * Get the commit time (i.e. committer time) of a commit.
 *
 * @param commit a previously loaded commit.
 * @return the time of a commit
 */
GIT_EXTERN(git_time_t) git_commit_time(git_commit *commit);

/**
 * Get the commit timezone offset (i.e. committer's preferred timezone) of a commit.
 *
 * @param commit a previously loaded commit.
 * @return positive or negative timezone offset, in minutes from UTC
 */
GIT_EXTERN(int) git_commit_time_offset(git_commit *commit);

/**
 * Get the committer of a commit.
 *
 * @param commit a previously loaded commit.
 * @return the committer of a commit
 */
GIT_EXTERN(const git_signature *) git_commit_committer(git_commit *commit);

/**
 * Get the author of a commit.
 *
 * @param commit a previously loaded commit.
 * @return the author of a commit
 */
GIT_EXTERN(const git_signature *) git_commit_author(git_commit *commit);

/**
 * Get the tree pointed to by a commit.
 *
 * @param tree_out pointer where to store the tree object
 * @param commit a previously loaded commit.
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_commit_tree(git_tree **tree_out, git_commit *commit);

/**
 * Get the number of parents of this commit
 *
 * @param commit a previously loaded commit.
 * @return integer of count of parents
 */
GIT_EXTERN(unsigned int) git_commit_parentcount(git_commit *commit);

/**
 * Get the specified parent of the commit.
 *
 * @param parent Pointer where to store the parent commit
 * @param commit a previously loaded commit.
 * @param n the position of the parent (from 0 to `parentcount`)
 * @return 0 on success; error code otherwise
 */
GIT_EXTERN(int) git_commit_parent(git_commit **parent, git_commit *commit, unsigned int n);


/**
 * Create a new commit in the repository
 *
 *
 * @param oid Pointer where to store the OID of the
 *	newly created commit
 *
 * @param repo Repository where to store the commit
 *
 * @param update_ref If not NULL, name of the reference that
 *	will be updated to point to this commit. If the reference
 *	is not direct, it will be resolved to a direct reference.
 *	Use "HEAD" to update the HEAD of the current branch and
 *	make it point to this commit
 *
 * @param author Signature representing the author and the authory
 *	time of this commit
 *
 * @param committer Signature representing the committer and the
 *  commit time of this commit
 *
 * @param message Full message for this commit
 *
 * @param tree_oid Object ID of the tree for this commit. Note that
 *  no validation is performed on this OID. Use the _o variants of
 *  this method to assure a proper tree is passed to the commit.
 *
 * @param parent_count Number of parents for this commit
 *
 * @param parents Array of pointers to parent OIDs for this commit.
 *	Note that no validation is performed on these OIDs. Use the _o
 *	variants of this method to assure that are parents for the commit
 *	are proper objects.
 *
 * @return 0 on success; error code otherwise
 *	The created commit will be written to the Object Database and
 *	the given reference will be updated to point to it
 */
GIT_EXTERN(int) git_commit_create(
		git_oid *oid,
		git_repository *repo,
		const char *update_ref,
		const git_signature *author,
		const git_signature *committer,
		const char *message,
		const git_oid *tree_oid,
		int parent_count,
		const git_oid *parent_oids[]);

/**
 * Create a new commit in the repository using `git_object`
 * instances as parameters.
 *
 * The `tree_oid` and `parent_oids` paremeters now take a instance
 * of `git_tree` and `git_commit`, respectively.
 *
 * All other parameters remain the same
 *
 * @see git_commit_create
 */
GIT_EXTERN(int) git_commit_create_o(
		git_oid *oid,
		git_repository *repo,
		const char *update_ref,
		const git_signature *author,
		const git_signature *committer,
		const char *message,
		const git_tree *tree,
		int parent_count,
		const git_commit *parents[]);

/**
 * Create a new commit in the repository using `git_object`
 * instances and a variable argument list.
 *
 * The `tree_oid` paremeter now takes a instance
 * of `const git_tree *`.
 *
 * The parents for the commit are specified as a variable
 * list of pointers to `const git_commit *`. Note that this
 * is a convenience method which may not be safe to export
 * for certain languages or compilers
 *
 * All other parameters remain the same
 *
 * @see git_commit_create
 */
GIT_EXTERN(int) git_commit_create_ov(
		git_oid *oid,
		git_repository *repo,
		const char *update_ref,
		const git_signature *author,
		const git_signature *committer,
		const char *message,
		const git_tree *tree,
		int parent_count,
		...);


/**
 * Create a new commit in the repository using 
 * a variable argument list.
 *
 * The parents for the commit are specified as a variable
 * list of pointers to `const git_oid *`. Note that this
 * is a convenience method which may not be safe to export
 * for certain languages or compilers
 *
 * All other parameters remain the same
 *
 * @see git_commit_create
 */
GIT_EXTERN(int) git_commit_create_v(
		git_oid *oid,
		git_repository *repo,
		const char *update_ref,
		const git_signature *author,
		const git_signature *committer,
		const char *message,
		const git_oid *tree_oid,
		int parent_count,
		...);

/** @} */
GIT_END_DECL
#endif
