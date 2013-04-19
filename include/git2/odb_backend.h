/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_odb_backend_h__
#define INCLUDE_git_odb_backend_h__

#include "git2/common.h"
#include "git2/types.h"

/**
 * @file git2/backend.h
 * @brief Git custom backend functions
 * @defgroup git_odb Git object database routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Constructors for in-box ODB backends.
 */
GIT_EXTERN(int) git_odb_backend_pack(git_odb_backend **out, const char *objects_dir);
GIT_EXTERN(int) git_odb_backend_loose(git_odb_backend **out, const char *objects_dir, int compression_level, int do_fsync);
GIT_EXTERN(int) git_odb_backend_one_pack(git_odb_backend **out, const char *index_file);

/** Streaming mode */
enum {
	GIT_STREAM_RDONLY = (1 << 1),
	GIT_STREAM_WRONLY = (1 << 2),
	GIT_STREAM_RW = (GIT_STREAM_RDONLY | GIT_STREAM_WRONLY),
};

/** A stream to read/write from a backend */
struct git_odb_stream {
	git_odb_backend *backend;
	unsigned int mode;

	int (*read)(git_odb_stream *stream, char *buffer, size_t len);
	int (*write)(git_odb_stream *stream, const char *buffer, size_t len);
	int (*finalize_write)(git_oid *oid_p, git_odb_stream *stream);
	void (*free)(git_odb_stream *stream);
};

/** A stream to write a pack file to the ODB */
struct git_odb_writepack {
	git_odb_backend *backend;

	int (*add)(git_odb_writepack *writepack, const void *data, size_t size, git_transfer_progress *stats);
	int (*commit)(git_odb_writepack *writepack, git_transfer_progress *stats);
	void (*free)(git_odb_writepack *writepack);
};

GIT_END_DECL

#endif
