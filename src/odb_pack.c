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

#include "common.h"
#include "git2/zlib.h"
#include "git2/repository.h"
#include "git2/oid.h"
#include "fileops.h"
#include "hash.h"
#include "odb.h"
#include "delta-apply.h"
#include "sha1_lookup.h"
#include "mwindow.h"
#include "pack.h"

#include "git2/odb_backend.h"

struct pack_backend {
	git_odb_backend parent;
	git_vector packs;
	struct pack_file *last_found;
	char *pack_folder;
	time_t pack_folder_mtime;
};

/**
 * The wonderful tale of a Packed Object lookup query
 * ===================================================
 *   A riveting and epic story of epicness and ASCII
 *          art, presented by yours truly,
 *               Sir Vicent of Marti
 *
 *
 *	Chapter 1: Once upon a time...
 *	Initialization of the Pack Backend
 *	--------------------------------------------------
 *
 *	# git_odb_backend_pack
 *	| Creates the pack backend structure, initializes the
 *	| callback pointers to our default read() and exist() methods,
 *	| and tries to preload all the known packfiles in the ODB.
 *  |
 *	|-# packfile_load_all
 *	  | Tries to find the `pack` folder, if it exists. ODBs without
 *	  | a pack folder are ignored altogether. If there's a `pack` folder
 *	  | we run a `dirent` callback through every file in the pack folder
 *	  | to find our packfiles. The packfiles are then sorted according
 *	  | to a sorting callback.
 * 	  |
 *	  |-# packfile_load__cb
 *	  | | This callback is called from `dirent` with every single file
 *	  | | inside the pack folder. We find the packs by actually locating
 *	  | | their index (ends in ".idx"). From that index, we verify that
 *	  | | the corresponding packfile exists and is valid, and if so, we
 *    | | add it to the pack list.
 *	  | |
 *	  | |-# packfile_check
 *	  |     Make sure that there's a packfile to back this index, and store
 *	  |     some very basic information regarding the packfile itself,
 *	  |     such as the full path, the size, and the modification time.
 *	  |     We don't actually open the packfile to check for internal consistency.
 *    |
 *    |-# packfile_sort__cb
 *        Sort all the preloaded packs according to some specific criteria:
 *        we prioritize the "newer" packs because it's more likely they
 *        contain the objects we are looking for, and we prioritize local
 *        packs over remote ones.
 *
 *
 *
 *	Chapter 2: To be, or not to be...
 *	A standard packed `exist` query for an OID
 *	--------------------------------------------------
 *
 *  # pack_backend__exists
 *  | Check if the given SHA1 oid exists in any of the packs
 *  | that have been loaded for our ODB.
 *  |
 *  |-# pack_entry_find
 *    | Iterate through all the packs that have been preloaded
 *    | (starting by the pack where the latest object was found)
 *    | to try to find the OID in one of them.
 *    |
 *    |-# pack_entry_find1
 *      | Check the index of an individual pack to see if the SHA1
 *      | OID can be found. If we can find the offset to that SHA1
 *      | inside of the index, that means the object is contained
 *      | inside of the packfile and we can stop searching.
 *      | Before returning, we verify that the packfile behing the
 *      | index we are searching still exists on disk.
 *      |
 *      |-# pack_entry_find_offset
 *      | | Mmap the actual index file to disk if it hasn't been opened
 *      | | yet, and run a binary search through it to find the OID.
 *      | | See <http://book.git-scm.com/7_the_packfile.html> for specifics
 *      | | on the Packfile Index format and how do we find entries in it.
 *      | |
 *      | |-# pack_index_open
 *      |   | Guess the name of the index based on the full path to the
 *      |   | packfile, open it and verify its contents. Only if the index
 *      |   | has not been opened already.
 *      |   |
 *      |   |-# pack_index_check
 *      |       Mmap the index file and do a quick run through the header
 *      |       to guess the index version (right now we support v1 and v2),
 *      |       and to verify that the size of the index makes sense.
 *      |
 *      |-# packfile_open
 *          See `packfile_open` in Chapter 3
 *
 *
 *
 *	Chapter 3: The neverending story...
 *	A standard packed `lookup` query for an OID
 *	--------------------------------------------------
 *	TODO
 *
 */


/***********************************************************
 *
 * FORWARD DECLARATIONS
 *
 ***********************************************************/

static void pack_window_free_all(struct pack_backend *backend, struct pack_file *p);
static int pack_window_contains(git_mwindow *win, off_t offset);

static int packfile_sort__cb(const void *a_, const void *b_);

static void pack_index_free(struct pack_file *p);

static int pack_index_check(const char *path,  struct pack_file *p);
static int pack_index_open(struct pack_file *p);

static struct pack_file *packfile_alloc(int extra);
static int packfile_open(struct pack_file *p);
static int packfile_check(struct pack_file **pack_out, const char *path);
static int packfile_load__cb(void *_data, char *path);
static int packfile_refresh_all(struct pack_backend *backend);

static off_t nth_packed_object_offset(const struct pack_file *p, uint32_t n);

/* Can find the offset of an object given
 * a prefix of an identifier.
 * Throws GIT_EAMBIGUOUSOIDPREFIX if short oid
 * is ambiguous within the pack.
 * This method assumes that len is between
 * GIT_OID_MINPREFIXLEN and GIT_OID_HEXSZ.
 */
static int pack_entry_find_offset(
		off_t *offset_out,
		git_oid *found_oid,
		struct pack_file *p,
		const git_oid *short_oid,
		unsigned int len);

static int pack_entry_find1(
		struct pack_entry *e,
		struct pack_file *p,
		const git_oid *short_oid,
		unsigned int len);

static int pack_entry_find(struct pack_entry *e,
		struct pack_backend *backend, const git_oid *oid);

/* Can find the offset of an object given
 * a prefix of an identifier.
 * Throws GIT_EAMBIGUOUSOIDPREFIX if short oid
 * is ambiguous.
 * This method assumes that len is between
 * GIT_OID_MINPREFIXLEN and GIT_OID_HEXSZ.
 */
static int pack_entry_find_prefix(struct pack_entry *e,
					struct pack_backend *backend,
					const git_oid *short_oid,
					unsigned int len);



/***********************************************************
 *
 * PACK WINDOW MANAGEMENT
 *
 ***********************************************************/

GIT_INLINE(void) pack_window_free_all(struct pack_backend *GIT_UNUSED(backend), struct pack_file *p)
{
	git_mwindow_free_all(&p->mwf);
}

GIT_INLINE(int) pack_window_contains(git_mwindow *win, off_t offset)
{
	/* We must promise at least 20 bytes (one hash) after the
	 * offset is available from this window, otherwise the offset
	 * is not actually in this window and a different window (which
	 * has that one hash excess) must be used.  This is to support
	 * the object header and delta base parsing routines below.
	 */
	return git_mwindow_contains(win, offset + 20);
}


/***********************************************************
 *
 * PACK INDEX METHODS
 *
 ***********************************************************/

static void pack_index_free(struct pack_file *p)
{
	if (p->index_map.data) {
		git_futils_mmap_free(&p->index_map);
		p->index_map.data = NULL;
	}
}

static int pack_index_check(const char *path,  struct pack_file *p)
{
	struct pack_idx_header *hdr;
	uint32_t version, nr, i, *index;

	void *idx_map;
	size_t idx_size;

	struct stat st;

	/* TODO: properly open the file without access time */
	git_file fd = p_open(path, O_RDONLY /*| O_NOATIME */);

	int error;

	if (fd < 0)
		return git__throw(GIT_EOSERR, "Failed to check index. File missing or corrupted");

	if (p_fstat(fd, &st) < GIT_SUCCESS) {
		p_close(fd);
		return git__throw(GIT_EOSERR, "Failed to check index. File appears to be corrupted");
	}

	if (!git__is_sizet(st.st_size))
		return GIT_ENOMEM;

	idx_size = (size_t)st.st_size;

	if (idx_size < 4 * 256 + 20 + 20) {
		p_close(fd);
		return git__throw(GIT_EOBJCORRUPTED, "Failed to check index. Object is corrupted");
	}

	error = git_futils_mmap_ro(&p->index_map, fd, 0, idx_size);
	p_close(fd);

	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to check index");

	hdr = idx_map = p->index_map.data;

	if (hdr->idx_signature == htonl(PACK_IDX_SIGNATURE)) {
		version = ntohl(hdr->idx_version);

		if (version < 2 || version > 2) {
			git_futils_mmap_free(&p->index_map);
			return git__throw(GIT_EOBJCORRUPTED, "Failed to check index. Unsupported index version");
		}

	} else
		version = 1;

	nr = 0;
	index = idx_map;

	if (version > 1)
		index += 2;  /* skip index header */

	for (i = 0; i < 256; i++) {
		uint32_t n = ntohl(index[i]);
		if (n < nr) {
			git_futils_mmap_free(&p->index_map);
			return git__throw(GIT_EOBJCORRUPTED, "Failed to check index. Index is non-monotonic");
		}
		nr = n;
	}

	if (version == 1) {
		/*
		 * Total size:
		 *  - 256 index entries 4 bytes each
		 *  - 24-byte entries * nr (20-byte sha1 + 4-byte offset)
		 *  - 20-byte SHA1 of the packfile
		 *  - 20-byte SHA1 file checksum
		 */
		if (idx_size != 4*256 + nr * 24 + 20 + 20) {
			git_futils_mmap_free(&p->index_map);
			return git__throw(GIT_EOBJCORRUPTED, "Failed to check index. Object is corrupted");
		}
	} else if (version == 2) {
		/*
		 * Minimum size:
		 *  - 8 bytes of header
		 *  - 256 index entries 4 bytes each
		 *  - 20-byte sha1 entry * nr
		 *  - 4-byte crc entry * nr
		 *  - 4-byte offset entry * nr
		 *  - 20-byte SHA1 of the packfile
		 *  - 20-byte SHA1 file checksum
		 * And after the 4-byte offset table might be a
		 * variable sized table containing 8-byte entries
		 * for offsets larger than 2^31.
		 */
		unsigned long min_size = 8 + 4*256 + nr*(20 + 4 + 4) + 20 + 20;
		unsigned long max_size = min_size;

		if (nr)
			max_size += (nr - 1)*8;

		if (idx_size < min_size || idx_size > max_size) {
			git_futils_mmap_free(&p->index_map);
			return git__throw(GIT_EOBJCORRUPTED, "Failed to check index. Wrong index size");
		}

		/* Make sure that off_t is big enough to access the whole pack...
		 * Is this an issue in libgit2? It shouldn't. */
		if (idx_size != min_size && (sizeof(off_t) <= 4)) {
			git_futils_mmap_free(&p->index_map);
			return git__throw(GIT_EOSERR, "Failed to check index. off_t not big enough to access the whole pack");
		}
	}

	p->index_version = version;
	p->num_objects = nr;
	return GIT_SUCCESS;
}

static int pack_index_open(struct pack_file *p)
{
	char *idx_name;
	int error;

	if (p->index_map.data)
		return GIT_SUCCESS;

	idx_name = git__strdup(p->pack_name);
	strcpy(idx_name + strlen(idx_name) - STRLEN(".pack"), ".idx");

	error = pack_index_check(idx_name, p);
	free(idx_name);

	return error == GIT_SUCCESS ? GIT_SUCCESS : git__rethrow(error, "Failed to open index");
}









/***********************************************************
 *
 * PACKFILE METHODS
 *
 ***********************************************************/

static int packfile_sort__cb(const void *a_, const void *b_)
{
	const struct pack_file *a = a_;
	const struct pack_file *b = b_;
	int st;

	/*
	 * Local packs tend to contain objects specific to our
	 * variant of the project than remote ones.  In addition,
	 * remote ones could be on a network mounted filesystem.
	 * Favor local ones for these reasons.
	 */
	st = a->pack_local - b->pack_local;
	if (st)
		return -st;

	/*
	 * Younger packs tend to contain more recent objects,
	 * and more recent objects tend to get accessed more
	 * often.
	 */
	if (a->mtime < b->mtime)
		return 1;
	else if (a->mtime == b->mtime)
		return 0;

	return -1;
}

static struct pack_file *packfile_alloc(int extra)
{
	struct pack_file *p = git__malloc(sizeof(*p) + extra);
	memset(p, 0, sizeof(*p));
	p->mwf.fd = -1;
	return p;
}


static void packfile_free(struct pack_backend *backend, struct pack_file *p)
{
	assert(p);

	/* clear_delta_base_cache(); */
	pack_window_free_all(backend, p);

	if (p->mwf.fd != -1)
		p_close(p->mwf.fd);

	pack_index_free(p);

	free(p->bad_object_sha1);
	free(p);
}

static int packfile_open(struct pack_file *p)
{
	struct stat st;
	struct pack_header hdr;
	git_oid sha1;
	unsigned char *idx_sha1;

	if (!p->index_map.data && pack_index_open(p) < GIT_SUCCESS)
		return git__throw(GIT_ENOTFOUND, "Failed to open packfile. File not found");

	/* TODO: open with noatime */
	p->mwf.fd = p_open(p->pack_name, O_RDONLY);
	if (p->mwf.fd < 0 || p_fstat(p->mwf.fd, &st) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to open packfile. File appears to be corrupted");

	if (git_mwindow_file_register(&p->mwf) < GIT_SUCCESS) {
		p_close(p->mwf.fd);
		return git__throw(GIT_ERROR, "Failed to register packfile windows");
	}

	/* If we created the struct before we had the pack we lack size. */
	if (!p->mwf.size) {
		if (!S_ISREG(st.st_mode))
			goto cleanup;
		p->mwf.size = (off_t)st.st_size;
	} else if (p->mwf.size != st.st_size)
		goto cleanup;

#if 0
	/* We leave these file descriptors open with sliding mmap;
	 * there is no point keeping them open across exec(), though.
	 */
	fd_flag = fcntl(p->mwf.fd, F_GETFD, 0);
	if (fd_flag < 0)
		return error("cannot determine file descriptor flags");

	fd_flag |= FD_CLOEXEC;
	if (fcntl(p->pack_fd, F_SETFD, fd_flag) == -1)
		return GIT_EOSERR;
#endif

	/* Verify we recognize this pack file format. */
	if (p_read(p->mwf.fd, &hdr, sizeof(hdr)) < GIT_SUCCESS)
		goto cleanup;

	if (hdr.hdr_signature != htonl(PACK_SIGNATURE))
		goto cleanup;

	if (!pack_version_ok(hdr.hdr_version))
		goto cleanup;

	/* Verify the pack matches its index. */
	if (p->num_objects != ntohl(hdr.hdr_entries))
		goto cleanup;

	if (p_lseek(p->mwf.fd, p->mwf.size - GIT_OID_RAWSZ, SEEK_SET) == -1)
		goto cleanup;

	if (p_read(p->mwf.fd, sha1.id, GIT_OID_RAWSZ) < GIT_SUCCESS)
		goto cleanup;

	idx_sha1 = ((unsigned char *)p->index_map.data) + p->index_map.len - 40;

	if (git_oid_cmp(&sha1, (git_oid *)idx_sha1) != 0)
		goto cleanup;

	return GIT_SUCCESS;

cleanup:
	p_close(p->mwf.fd);
	p->mwf.fd = -1;
	return git__throw(GIT_EPACKCORRUPTED, "Failed to packfile. Pack is corrupted");
}

static int packfile_check(struct pack_file **pack_out, const char *path)
{
	struct stat st;
	struct pack_file *p;
	size_t path_len;

	*pack_out = NULL;
	path_len = strlen(path);
	p = packfile_alloc(path_len + 2);

	/*
	 * Make sure a corresponding .pack file exists and that
	 * the index looks sane.
	 */
	path_len -= STRLEN(".idx");
	if (path_len < 1) {
		free(p);
		return git__throw(GIT_ENOTFOUND, "Failed to check packfile. Wrong path name");
	}

	memcpy(p->pack_name, path, path_len);

	strcpy(p->pack_name + path_len, ".keep");
	if (git_futils_exists(p->pack_name) == GIT_SUCCESS)
		p->pack_keep = 1;

	strcpy(p->pack_name + path_len, ".pack");
	if (p_stat(p->pack_name, &st) < GIT_SUCCESS || !S_ISREG(st.st_mode)) {
		free(p);
		return git__throw(GIT_ENOTFOUND, "Failed to check packfile. File not found");
	}

	/* ok, it looks sane as far as we can check without
	 * actually mapping the pack file.
	 */
	p->mwf.size = (off_t)st.st_size;
	p->pack_local = 1;
	p->mtime = (git_time_t)st.st_mtime;

	/* see if we can parse the sha1 oid in the packfile name */
	if (path_len < 40 ||
		git_oid_fromstr(&p->sha1, path + path_len - GIT_OID_HEXSZ) < GIT_SUCCESS)
		memset(&p->sha1, 0x0, GIT_OID_RAWSZ);

	*pack_out = p;
	return GIT_SUCCESS;
}

static int packfile_load__cb(void *_data, char *path)
{
	struct pack_backend *backend = (struct pack_backend *)_data;
	struct pack_file *pack;
	int error;
	size_t i;

	if (git__suffixcmp(path, ".idx") != 0)
		return GIT_SUCCESS; /* not an index */

	for (i = 0; i < backend->packs.length; ++i) {
		struct pack_file *p = git_vector_get(&backend->packs, i);
		if (memcmp(p->pack_name, path, strlen(path) - STRLEN(".idx")) == 0)
			return GIT_SUCCESS;
	}

	error = packfile_check(&pack, path);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to load packfile");

	if (git_vector_insert(&backend->packs, pack) < GIT_SUCCESS) {
		free(pack);
		return GIT_ENOMEM;
	}

	return GIT_SUCCESS;
}

static int packfile_refresh_all(struct pack_backend *backend)
{
	int error;
	struct stat st;

	if (backend->pack_folder == NULL)
		return GIT_SUCCESS;

	if (p_stat(backend->pack_folder, &st) < 0 || !S_ISDIR(st.st_mode))
		return git__throw(GIT_ENOTFOUND, "Failed to refresh packfiles. Backend not found");

	if (st.st_mtime != backend->pack_folder_mtime) {
		char path[GIT_PATH_MAX];
		strcpy(path, backend->pack_folder);

		/* reload all packs */
		error = git_futils_direach(path, GIT_PATH_MAX, packfile_load__cb, (void *)backend);
		if (error < GIT_SUCCESS)
			return git__rethrow(error, "Failed to refresh packfiles");

		git_vector_sort(&backend->packs);
		backend->pack_folder_mtime = st.st_mtime;
	}

	return GIT_SUCCESS;
}








/***********************************************************
 *
 * PACKFILE ENTRY SEARCH INTERNALS
 *
 ***********************************************************/

static off_t nth_packed_object_offset(const struct pack_file *p, uint32_t n)
{
	const unsigned char *index = p->index_map.data;
	index += 4 * 256;
	if (p->index_version == 1) {
		return ntohl(*((const uint32_t *)(index + 24 * n)));
	} else {
		uint32_t off;
		index += 8 + p->num_objects * (20 + 4);
		off = ntohl(*((const uint32_t *)(index + 4 * n)));
		if (!(off & 0x80000000))
			return off;
		index += p->num_objects * 4 + (off & 0x7fffffff) * 8;
		return (((uint64_t)ntohl(*((const uint32_t *)(index + 0)))) << 32) |
				   ntohl(*((const uint32_t *)(index + 4)));
	}
}

static int pack_entry_find_offset(
		off_t *offset_out,
		git_oid *found_oid,
		struct pack_file *p,
		const git_oid *short_oid,
		unsigned int len)
{
	const uint32_t *level1_ofs = p->index_map.data;
	const unsigned char *index = p->index_map.data;
	unsigned hi, lo, stride;
	int pos, found = 0;
	const unsigned char *current = 0;

	*offset_out = 0;

	if (index == NULL) {
		int error;

		if ((error = pack_index_open(p)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to find offset for pack entry");

		assert(p->index_map.data);

		index = p->index_map.data;
		level1_ofs = p->index_map.data;
	}

	if (p->index_version > 1) {
		level1_ofs += 2;
		index += 8;
	}

	index += 4 * 256;
	hi = ntohl(level1_ofs[(int)short_oid->id[0]]);
	lo = ((short_oid->id[0] == 0x0) ? 0 : ntohl(level1_ofs[(int)short_oid->id[0] - 1]));

	if (p->index_version > 1) {
		stride = 20;
	} else {
		stride = 24;
		index += 4;
	}

#ifdef INDEX_DEBUG_LOOKUP
	printf("%02x%02x%02x... lo %u hi %u nr %d\n",
		short_oid->id[0], short_oid->id[1], short_oid->id[2], lo, hi, p->num_objects);
#endif

	/* Use git.git lookup code */
	pos =  sha1_entry_pos(index, stride, 0, lo, hi, p->num_objects, short_oid->id);

	if (pos >= 0) {
		/* An object matching exactly the oid was found */
		found = 1;
		current = index + pos * stride;
	} else {
		/* No object was found */
		/* pos refers to the object with the "closest" oid to short_oid */
		pos = - 1 - pos;
		if (pos < (int)p->num_objects) {
			current = index + pos * stride;

			if (!git_oid_ncmp(short_oid, (const git_oid *)current, len)) {
				found = 1;
			}
		}
	}

	if (found && pos + 1 < (int)p->num_objects) {
		/* Check for ambiguousity */
		const unsigned char *next = current + stride;

		if (!git_oid_ncmp(short_oid, (const git_oid *)next, len)) {
			found = 2;
		}
	}

	if (!found) {
		return git__throw(GIT_ENOTFOUND, "Failed to find offset for pack entry. Entry not found");
	} else if (found > 1) {
		return git__throw(GIT_EAMBIGUOUSOIDPREFIX, "Failed to find offset for pack entry. Ambiguous sha1 prefix within pack");
	} else {
		*offset_out = nth_packed_object_offset(p, pos);
		git_oid_fromraw(found_oid, current);

#ifdef INDEX_DEBUG_LOOKUP
		unsigned char hex_sha1[GIT_OID_HEXSZ + 1];
		git_oid_fmt(hex_sha1, found_oid);
		hex_sha1[GIT_OID_HEXSZ] = '\0';
		printf("found lo=%d %s\n", lo, hex_sha1);
#endif
		return GIT_SUCCESS;
	}
}

static int pack_entry_find1(
		struct pack_entry *e,
		struct pack_file *p,
		const git_oid *short_oid,
		unsigned int len)
{
	off_t offset;
	git_oid found_oid;
	int error;

	assert(p);

	if (len == GIT_OID_HEXSZ && p->num_bad_objects) {
		unsigned i;
		for (i = 0; i < p->num_bad_objects; i++)
			if (git_oid_cmp(short_oid, &p->bad_object_sha1[i]) == 0)
				return git__throw(GIT_ERROR, "Failed to find pack entry. Bad object found");
	}

	error = pack_entry_find_offset(&offset, &found_oid, p, short_oid, len);
	if (error < GIT_SUCCESS)
		return git__rethrow(error, "Failed to find pack entry. Couldn't find offset");

	/* we found a unique entry in the index;
	 * make sure the packfile backing the index
	 * still exists on disk */
	if (p->mwf.fd == -1 && packfile_open(p) < GIT_SUCCESS)
		return git__throw(GIT_EOSERR, "Failed to find pack entry. Packfile doesn't exist on disk");

	e->offset = offset;
	e->p = p;

	git_oid_cpy(&e->sha1, &found_oid);
	return GIT_SUCCESS;
}

static int pack_entry_find(struct pack_entry *e, struct pack_backend *backend, const git_oid *oid)
{
	int error;
	size_t i;

	if ((error = packfile_refresh_all(backend)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to find pack entry");

	if (backend->last_found &&
		pack_entry_find1(e, backend->last_found, oid, GIT_OID_HEXSZ) == GIT_SUCCESS)
		return GIT_SUCCESS;

	for (i = 0; i < backend->packs.length; ++i) {
		struct pack_file *p;

		p = git_vector_get(&backend->packs, i);
		if (p == backend->last_found)
			continue;

		if (pack_entry_find1(e, p, oid, GIT_OID_HEXSZ) == GIT_SUCCESS) {
			backend->last_found = p;
			return GIT_SUCCESS;
		}
	}

	return git__throw(GIT_ENOTFOUND, "Failed to find pack entry");
}

static int pack_entry_find_prefix(
	struct pack_entry *e,
	struct pack_backend *backend,
	const git_oid *short_oid,
	unsigned int len)
{
	int error;
	size_t i;
	unsigned found = 0;

	if ((error = packfile_refresh_all(backend)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to find pack entry");

	if (backend->last_found) {
		error = pack_entry_find1(e, backend->last_found, short_oid, len);
		if (error == GIT_EAMBIGUOUSOIDPREFIX) {
			return git__rethrow(error, "Failed to find pack entry. Ambiguous sha1 prefix");
		} else if (error == GIT_SUCCESS) {
			found = 1;
		}
	}

	for (i = 0; i < backend->packs.length; ++i) {
		struct pack_file *p;

		p = git_vector_get(&backend->packs, i);
		if (p == backend->last_found)
			continue;

		error = pack_entry_find1(e, p, short_oid, len);
		if (error == GIT_EAMBIGUOUSOIDPREFIX) {
			return git__rethrow(error, "Failed to find pack entry. Ambiguous sha1 prefix");
		} else if (error == GIT_SUCCESS) {
			found++;
			if (found > 1)
				break;
			backend->last_found = p;
		}
	}

	if (!found) {
		return git__rethrow(GIT_ENOTFOUND, "Failed to find pack entry");
	} else if (found > 1) {
		return git__rethrow(GIT_EAMBIGUOUSOIDPREFIX, "Failed to find pack entry. Ambiguous sha1 prefix");
	} else {
		return GIT_SUCCESS;
	}

}


/***********************************************************
 *
 * PACKED BACKEND PUBLIC API
 *
 * Implement the git_odb_backend API calls
 *
 ***********************************************************/

/*
int pack_backend__read_header(git_rawobj *obj, git_odb_backend *backend, const git_oid *oid)
{
	pack_location location;

	assert(obj && backend && oid);

	if (locate_packfile(&location, (struct pack_backend *)backend, oid) < 0)
		return GIT_ENOTFOUND;

	return read_header_packed(obj, &location);
}
*/

int pack_backend__read(void **buffer_p, size_t *len_p, git_otype *type_p, git_odb_backend *backend, const git_oid *oid)
{
	struct pack_entry e;
	git_rawobj raw;
	int error;

	if ((error = pack_entry_find(&e, (struct pack_backend *)backend, oid)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to read pack backend");

	if ((error = packfile_unpack(&raw, e.p, e.offset)) < GIT_SUCCESS)
		return git__rethrow(error, "Failed to read pack backend");

	*buffer_p = raw.data;
	*len_p = raw.len;
	*type_p = raw.type;

	return GIT_SUCCESS;
}

int pack_backend__read_prefix(
	git_oid *out_oid,
	void **buffer_p,
	size_t *len_p,
	git_otype *type_p,
	git_odb_backend *backend,
	const git_oid *short_oid,
	unsigned int len)
{
	if (len < GIT_OID_MINPREFIXLEN)
		return git__throw(GIT_EAMBIGUOUSOIDPREFIX, "Failed to read pack backend. Prefix length is lower than %d.", GIT_OID_MINPREFIXLEN);

	if (len >= GIT_OID_HEXSZ) {
		/* We can fall back to regular read method */
		int error = pack_backend__read(buffer_p, len_p, type_p, backend, short_oid);
		if (error == GIT_SUCCESS)
			git_oid_cpy(out_oid, short_oid);

		return error;
	} else {
		struct pack_entry e;
		git_rawobj raw;
		int error;

		if ((error = pack_entry_find_prefix(&e, (struct pack_backend *)backend, short_oid, len)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to read pack backend");

		if ((error = packfile_unpack(&raw, e.p, e.offset)) < GIT_SUCCESS)
			return git__rethrow(error, "Failed to read pack backend");

		*buffer_p = raw.data;
		*len_p = raw.len;
		*type_p = raw.type;
		git_oid_cpy(out_oid, &e.sha1);
	}

	return GIT_SUCCESS;
}

int pack_backend__exists(git_odb_backend *backend, const git_oid *oid)
{
	struct pack_entry e;
	return pack_entry_find(&e, (struct pack_backend *)backend, oid) == GIT_SUCCESS;
}

void pack_backend__free(git_odb_backend *_backend)
{
	struct pack_backend *backend;
	size_t i;

	assert(_backend);

	backend = (struct pack_backend *)_backend;

	for (i = 0; i < backend->packs.length; ++i) {
		struct pack_file *p = git_vector_get(&backend->packs, i);
		packfile_free(backend, p);
	}

	git_vector_free(&backend->packs);
	free(backend->pack_folder);
	free(backend);
}

int git_odb_backend_pack(git_odb_backend **backend_out, const char *objects_dir)
{
	struct pack_backend *backend;
	char path[GIT_PATH_MAX];

	backend = git__calloc(1, sizeof(struct pack_backend));
	if (backend == NULL)
		return GIT_ENOMEM;

	if (git_vector_init(&backend->packs, 8, packfile_sort__cb) < GIT_SUCCESS) {
		free(backend);
		return GIT_ENOMEM;
	}

	git_path_join(path, objects_dir, "pack");
	if (git_futils_isdir(path) == GIT_SUCCESS) {
		backend->pack_folder = git__strdup(path);
		backend->pack_folder_mtime = 0;

		if (backend->pack_folder == NULL) {
			free(backend);
			return GIT_ENOMEM;
		}
	}

	backend->parent.read = &pack_backend__read;
	backend->parent.read_prefix = &pack_backend__read_prefix;
	backend->parent.read_header = NULL;
	backend->parent.exists = &pack_backend__exists;
	backend->parent.free = &pack_backend__free;

	*backend_out = (git_odb_backend *)backend;
	return GIT_SUCCESS;
}
