#ifndef INCLUDE_transport_h__
#define INCLUDE_transport_h__

#include "git2/transport.h"
#include "git2/net.h"
#include "vector.h"

/*
 * A day in the life of a network operation
 * ========================================
 *
 * The library gets told to ls-remote/push/fetch on/to/from some
 * remote. We look at the URL of the remote and fill the function
 * table with whatever is appropriate (the remote may be git over git,
 * ssh or http(s). It may even be an hg or svn repository, the library
 * at this level doesn't care, it just calls the helpers.
 *
 * The first call is to ->connect() which connects to the remote,
 * making use of the direction if necessary. This function must also
 * store the remote heads and any other information it needs.
 *
 * If we just want to execute ls-remote, ->ls() gets
 * called. Otherwise, the have/want/need list needs to be built via
 * ->wanthaveneed(). We can then ->push() or ->pull(). When we're
 * done, we call ->close() to close the connection. ->free() takes
 * care of freeing all the resources.
 */

struct git_transport {
	/**
	 * Where the repo lives
	 */
	char *url;
	/**
	 * Whether we want to push or fetch
	 */
	int direction : 1; /* 0 fetch, 1 push */
	int connected : 1;
	/**
	 * Connect and store the remote heads
	 */
	int (*connect)(struct git_transport *transport, int dir);
	/**
	 * Give a list of references, useful for ls-remote
	 */
	int (*ls)(struct git_transport *transport, git_headarray *headarray);
	/**
	 * Calculate want/have/need. May not even be needed.
	 */
	int (*wanthaveneed)(struct git_transport *transport, void *something);
	/**
	 * Build the pack
	 */
	int (*build_pack)(struct git_transport *transport);
	/**
	 * Push the changes over
	 */
	int (*push)(struct git_transport *transport);
	/**
	 * Send the list of 'want' refs
	 */
	int (*send_wants)(struct git_transport *transport, git_headarray *list);
	/**
	 * Fetch the changes
	 */
	int (*fetch)(struct git_transport *transport);
	/**
	 * Close the connection
	 */
	int (*close)(struct git_transport *transport);
	/**
	 * Free the associated resources
	 */
	void (*free)(struct git_transport *transport);
};

int git_transport_local(struct git_transport **transport);
int git_transport_git(struct git_transport **transport);
int git_transport_dummy(struct git_transport **transport);

int git_transport_send_wants(struct git_transport *transport, git_headarray *array);

#endif
