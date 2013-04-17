#include "git2/blame.h"
#include "common.h"
#include "vector.h"

typedef struct git_blame__line {
	git_oid origin_oid;
	uint16_t tracked_line_number;
} git_blame__line;

struct git_blame {
	const char *path;
	git_vector hunks;
	git_repository *repository;
	git_blame_options options;

	git_blame__line *lines;

	/* Trivial context */
};

git_blame *git_blame__alloc(
	git_repository *repo,
	git_blame_options opts,
	const char *path);
