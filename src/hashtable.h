/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_hashtable_h__
#define INCLUDE_hashtable_h__

#include "git2/common.h"
#include "git2/oid.h"
#include "git2/odb.h"
#include "common.h"

#define GIT_HASHTABLE_HASHES 3

typedef uint32_t (*git_hash_ptr)(const void *, int hash_id);
typedef int (*git_hash_keyeq_ptr)(const void *key_a, const void *key_b);

struct git_hashtable_node {
	const void *key;
	void *value;
};

#define GIT_HASHTABLE_STASH_SIZE 3

struct git_hashtable {
	struct git_hashtable_node *nodes;

	size_t size_mask;
	size_t size;
	size_t key_count;

	struct git_hashtable_node stash[GIT_HASHTABLE_STASH_SIZE];
	int stash_count;

	int is_resizing;

	git_hash_ptr hash;
	git_hash_keyeq_ptr key_equal;
};

typedef struct git_hashtable_node git_hashtable_node;
typedef struct git_hashtable git_hashtable;

git_hashtable *git_hashtable_alloc(
	size_t min_size,
	git_hash_ptr hash,
	git_hash_keyeq_ptr key_eq);

void *git_hashtable_lookup(git_hashtable *h, const void *key);
int git_hashtable_remove2(git_hashtable *table, const void *key, void **old_value);

GIT_INLINE(int) git_hashtable_remove(git_hashtable *table, const void *key)
{
	void *_unused;
	return git_hashtable_remove2(table, key, &_unused);
}


void git_hashtable_free(git_hashtable *h);
void git_hashtable_clear(git_hashtable *h);
int git_hashtable_merge(git_hashtable *self, git_hashtable *other);

int git_hashtable_insert2(git_hashtable *h, const void *key, void *value, void **old_value);

GIT_INLINE(int) git_hashtable_insert(git_hashtable *h, const void *key, void *value)
{
	void *_unused;
	return git_hashtable_insert2(h, key, value, &_unused);
}

#define git_hashtable_node_at(nodes, pos) ((git_hashtable_node *)(&nodes[pos]))

#define GIT_HASHTABLE__FOREACH(self,block) { \
	size_t _c; \
	git_hashtable_node *_n = (self)->nodes; \
	for (_c = (self)->size; _c > 0; _c--, _n++)	{ \
		if (!_n->key) continue; block } }

#define GIT_HASHTABLE_FOREACH(self, pkey, pvalue, code)\
	GIT_HASHTABLE__FOREACH(self,{(pkey)=_n->key;(pvalue)=_n->value;code;})

#define GIT_HASHTABLE_FOREACH_KEY(self, pkey, code)\
	GIT_HASHTABLE__FOREACH(self,{(pkey)=_n->key;code;})

#define GIT_HASHTABLE_FOREACH_VALUE(self, pvalue, code)\
	GIT_HASHTABLE__FOREACH(self,{(pvalue)=_n->value;code;})

#define GIT_HASHTABLE_FOREACH_DELETE() {\
	_node->key = NULL; _node->value = NULL; _self->key_count--;\
}

/*
 * If you want a hashtable with standard string keys, you can
 * just pass git_hash__strcmp_cb and git_hash__strhash_cb to
 * git_hashtable_alloc.
 */
#define git_hash__strcmp_cb git__strcmp_cb
extern uint32_t git_hash__strhash_cb(const void *key, int hash_id);

#endif
