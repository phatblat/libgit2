/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "repository.h"
#include "commit.h"
#include "thread-utils.h"
#include "util.h"
#include "cache.h"
#include "odb.h"
#include "object.h"
#include "git2/oid.h"

GIT__USE_OIDMAP

size_t git_cache__max_object_size[8] = {
	0, /* GIT_OBJ__EXT1 */
	4096,  /* GIT_OBJ_COMMIT */
	4096,  /* GIT_OBJ_TREE */
	0, /* GIT_OBJ_BLOB */
	4096,  /* GIT_OBJ_TAG */
	0, /* GIT_OBJ__EXT2 */
	0, /* GIT_OBJ_OFS_DELTA */
	0 /* GIT_OBJ_REF_DELTA */
};

int git_cache_init(git_cache *cache)
{
	cache->used_memory = 0;
	cache->map = git_oidmap_alloc();
	git_mutex_init(&cache->lock);
	return 0;
}

void git_cache_free(git_cache *cache)
{
	git_oidmap_free(cache->map);
	git_mutex_free(&cache->lock);
}

/* Call with lock, yo */
static void cache_evict_entries(git_cache *cache, size_t evict_count)
{
	uint32_t seed = rand();

	/* do not infinite loop if there's not enough entries to evict  */
	if (evict_count > kh_size(cache->map))
		return;

	while (evict_count > 0) {
		khiter_t pos = seed++ % kh_end(cache->map);

		if (kh_exist(cache->map, pos)) {
			git_cached_obj *evict = kh_val(cache->map, pos);

			evict_count--;
			cache->used_memory -= evict->size;
			git_cached_obj_decref(evict);

			kh_del(oid, cache->map, pos);
		}
	}
}

static bool cache_should_store(git_otype object_type, size_t object_size)
{
	size_t max_size = git_cache__max_object_size[object_type];

	if (max_size == 0 || object_size > max_size)
		return false;

	return true;
}

static void *cache_get(git_cache *cache, const git_oid *oid, unsigned int flags)
{
	khiter_t pos;
	git_cached_obj *entry = NULL;

	if (git_mutex_lock(&cache->lock) < 0)
		return NULL;

	pos = kh_get(oid, cache->map, oid);
	if (pos != kh_end(cache->map)) {
		entry = kh_val(cache->map, pos);

		if (flags && entry->flags != flags) {
			entry = NULL;
		} else {
			git_cached_obj_incref(entry);
		}
	}

	git_mutex_unlock(&cache->lock);

	return entry;
}

static void *cache_store(git_cache *cache, git_cached_obj *entry)
{
	khiter_t pos;

	git_cached_obj_incref(entry);

	if (!cache_should_store(entry->type, entry->size))
		return entry;

	if (git_mutex_lock(&cache->lock) < 0)
		return entry;

	pos = kh_get(oid, cache->map, &entry->oid);

	/* not found */
	if (pos == kh_end(cache->map)) {
		int rval;

		pos = kh_put(oid, cache->map, &entry->oid, &rval);
		if (rval >= 0) {
			kh_key(cache->map, pos) = &entry->oid;
			kh_val(cache->map, pos) = entry;
			git_cached_obj_incref(entry);
			cache->used_memory += entry->size;
		}
	}
	/* found */
	else {
		git_cached_obj *stored_entry = kh_val(cache->map, pos);

		if (stored_entry->flags == entry->flags) {
			git_cached_obj_decref(entry);
			git_cached_obj_incref(stored_entry);
			entry = stored_entry;
		} else if (stored_entry->flags == GIT_CACHE_STORE_RAW &&
			entry->flags == GIT_CACHE_STORE_PARSED) {
			git_cached_obj_decref(stored_entry);
			git_cached_obj_incref(entry);

			kh_key(cache->map, pos) = &entry->oid;
			kh_val(cache->map, pos) = entry;
		} else {
			/* NO OP */
		}
	}

	git_mutex_unlock(&cache->lock);
	return entry;
}

void *git_cache_store_raw(git_cache *cache, git_odb_object *entry)
{
	entry->cached.flags = GIT_CACHE_STORE_RAW;
	return cache_store(cache, (git_cached_obj *)entry);
}

void *git_cache_store_parsed(git_cache *cache, git_object *entry)
{
	entry->cached.flags = GIT_CACHE_STORE_PARSED;
	return cache_store(cache, (git_cached_obj *)entry);
}

git_odb_object *git_cache_get_raw(git_cache *cache, const git_oid *oid)
{
	return cache_get(cache, oid, GIT_CACHE_STORE_RAW);
}

git_object *git_cache_get_parsed(git_cache *cache, const git_oid *oid)
{
	return cache_get(cache, oid, GIT_CACHE_STORE_PARSED);
}

void *git_cache_get_any(git_cache *cache, const git_oid *oid)
{
	return cache_get(cache, oid, GIT_CACHE_STORE_ANY);
}

void git_cached_obj_decref(void *_obj)
{
	git_cached_obj *obj = _obj;

	if (git_atomic_dec(&obj->refcount) == 0) {
		switch (obj->flags) {
		case GIT_CACHE_STORE_RAW:
			git_odb_object__free(_obj);
			break;

		case GIT_CACHE_STORE_PARSED:
			git_object__free(_obj);
			break;

		default:
			git__free(_obj);
			break;
		}
	}
}
