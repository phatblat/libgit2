#include "clar_libgit2.h"
#include <git2.h>
#include "fileops.h"
#include "hash.h"
#include "iterator.h"
#include "vector.h"
#include "posix.h"


/*
 * This is a packfile with three objects. The second is a delta which
 * depends on the third, which is also a delta.
 */
unsigned char out_of_order_pack[] = {
  0x50, 0x41, 0x43, 0x4b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03,
  0x32, 0x78, 0x9c, 0x63, 0x67, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x76,
  0xe6, 0x8f, 0xe8, 0x12, 0x9b, 0x54, 0x6b, 0x10, 0x1a, 0xee, 0x95, 0x10,
  0xc5, 0x32, 0x8e, 0x7f, 0x21, 0xca, 0x1d, 0x18, 0x78, 0x9c, 0x63, 0x62,
  0x66, 0x4e, 0xcb, 0xcf, 0x07, 0x00, 0x02, 0xac, 0x01, 0x4d, 0x75, 0x01,
  0xd7, 0x71, 0x36, 0x66, 0xf4, 0xde, 0x82, 0x27, 0x76, 0xc7, 0x62, 0x2c,
  0x10, 0xf1, 0xb0, 0x7d, 0xe2, 0x80, 0xdc, 0x78, 0x9c, 0x63, 0x62, 0x62,
  0x62, 0xb7, 0x03, 0x00, 0x00, 0x69, 0x00, 0x4c, 0xde, 0x7d, 0xaa, 0xe4,
  0x19, 0x87, 0x58, 0x80, 0x61, 0x09, 0x9a, 0x33, 0xca, 0x7a, 0x31, 0x92,
  0x6f, 0xae, 0x66, 0x75
};
unsigned int out_of_order_pack_len = 112;

/*
 * Packfile with two objects. The second is a delta against an object
 * which is not in the packfile
 */
unsigned char thin_pack[] = {
  0x50, 0x41, 0x43, 0x4b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x32, 0x78, 0x9c, 0x63, 0x67, 0x00, 0x00, 0x00, 0x10, 0x00, 0x08, 0x76,
  0xe6, 0x8f, 0xe8, 0x12, 0x9b, 0x54, 0x6b, 0x10, 0x1a, 0xee, 0x95, 0x10,
  0xc5, 0x32, 0x8e, 0x7f, 0x21, 0xca, 0x1d, 0x18, 0x78, 0x9c, 0x63, 0x62,
  0x66, 0x4e, 0xcb, 0xcf, 0x07, 0x00, 0x02, 0xac, 0x01, 0x4d, 0x42, 0x52,
  0x3a, 0x6f, 0x39, 0xd1, 0xfe, 0x66, 0x68, 0x6b, 0xa5, 0xe5, 0xe2, 0x97,
  0xac, 0x94, 0x6c, 0x76, 0x0b, 0x04
};
unsigned int thin_pack_len = 78;

unsigned char base_obj[] = { 07, 076 };
unsigned int base_obj_len = 2;

void test_pack_indexer__out_of_order(void)
{
	git_indexer_stream *idx;
	git_transfer_progress stats;

	cl_git_pass(git_indexer_stream_new(&idx, ".", NULL, NULL, NULL));
	cl_git_pass(git_indexer_stream_add(idx, out_of_order_pack, out_of_order_pack_len, &stats));
	cl_git_pass(git_indexer_stream_finalize(idx, &stats));

	cl_assert_equal_i(stats.total_objects, 3);
	cl_assert_equal_i(stats.received_objects, 3);
	cl_assert_equal_i(stats.indexed_objects, 3);

	git_indexer_stream_free(idx);
}

void test_pack_indexer__fix_thin(void)
{
	git_indexer_stream *idx;
	git_transfer_progress stats;
	git_repository *repo;
	git_odb *odb;
	git_oid id, should_id;

	cl_git_pass(git_repository_init(&repo, "thin.git", true));
	cl_git_pass(git_repository_odb(&odb, repo));

	/* Store the missing base into your ODB so the indexer can fix the pack */
	cl_git_pass(git_odb_write(&id, odb, base_obj, base_obj_len, GIT_OBJ_BLOB));
	git_oid_fromstr(&should_id, "e68fe8129b546b101aee9510c5328e7f21ca1d18");
	cl_assert(!git_oid_cmp(&id, &should_id));

	cl_git_pass(git_indexer_stream_new(&idx, ".", odb, NULL, NULL));
	cl_git_pass(git_indexer_stream_add(idx, thin_pack, thin_pack_len, &stats));
	cl_git_pass(git_indexer_stream_finalize(idx, &stats));

	cl_assert_equal_i(stats.total_objects, 2);
	cl_assert_equal_i(stats.received_objects, 2);
	cl_assert_equal_i(stats.indexed_objects, 2);
	cl_assert_equal_i(stats.local_objects, 1);

	git_oid_fromstr(&should_id, "11f0f69b334728fdd8bc86b80499f22f29d85b15");
	cl_assert(!git_oid_cmp(git_indexer_stream_hash(idx), &should_id));

	git_indexer_stream_free(idx);
	git_odb_free(odb);
	git_repository_free(repo);

	/*
	 * The pack's name/hash only tells us what objects there are,
	 * so we need to go through the packfile again in order to
	 * figure out whether we calculated the trailer correctly.
	 */
	{
		unsigned char buffer[128];
		int fd;
		ssize_t read;
		git_off_t left;
		struct stat st;
		const char *name = "pack-11f0f69b334728fdd8bc86b80499f22f29d85b15.pack";

		fd = p_open(name, O_RDONLY);
		cl_assert(fd != -1);

		cl_git_pass(p_stat(name, &st));
		left = st.st_size;

		cl_git_pass(git_indexer_stream_new(&idx, ".", NULL, NULL, NULL));
		read = p_read(fd, buffer, sizeof(buffer));
		cl_assert(read != -1);
		p_close(fd);

		cl_git_pass(git_indexer_stream_add(idx, buffer, read, &stats));
		cl_git_pass(git_indexer_stream_finalize(idx, &stats));

		cl_assert_equal_i(stats.total_objects, 3);
		cl_assert_equal_i(stats.received_objects, 3);
		cl_assert_equal_i(stats.indexed_objects, 3);
		cl_assert_equal_i(stats.local_objects, 0);

		git_indexer_stream_free(idx);
	}
}
