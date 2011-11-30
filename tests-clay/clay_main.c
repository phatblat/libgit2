#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

/* required for sandboxing */
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#	include <windows.h>
#	include <io.h>
#	include <shellapi.h>
#	include <direct.h>

#	define _MAIN_CC __cdecl

#	define stat(path, st) _stat(path, st)
#	define mkdir(path, mode) _mkdir(path)
#	define chdir(path) _chdir(path)
#	define access(path, mode) _access(path, mode)
#	define strdup(str) _strdup(str)

#	ifndef __MINGW32__
#		pragma comment(lib, "shell32")
#		define strncpy(to, from, to_size) strncpy_s(to, to_size, from, _TRUNCATE)
#		define W_OK 02
#		define S_ISDIR(x) ((x & _S_IFDIR) != 0)
#		define mktemp_s(path, len) _mktemp_s(path, len)
#	endif
	typedef struct _stat STAT_T;
#else
#	include <sys/wait.h> /* waitpid(2) */
#	include <unistd.h>
#	define _MAIN_CC
	typedef struct stat STAT_T;
#endif

#include "clay.h"

static void fs_rm(const char *_source);
static void fs_copy(const char *_source, const char *dest);

static const char *
fixture_path(const char *base, const char *fixture_name);

struct clay_error {
	const char *test;
	int test_number;
	const char *suite;
	const char *file;
	int line_number;
	const char *error_msg;
	char *description;

	struct clay_error *next;
};

static struct {
	const char *active_test;
	const char *active_suite;

	int suite_errors;
	int total_errors;

	int test_count;

	struct clay_error *errors;
	struct clay_error *last_error;

	void (*local_cleanup)(void *);
	void *local_cleanup_payload;

	jmp_buf trampoline;
	int trampoline_enabled;
} _clay;

struct clay_func {
	const char *name;
	void (*ptr)(void);
};

struct clay_suite {
	const char *name;
	struct clay_func initialize;
	struct clay_func cleanup;
	const struct clay_func *tests;
	size_t test_count;
};

/* From clay_print_*.c */
static void clay_print_init(int test_count, int suite_count, const char *suite_names);
static void clay_print_shutdown(int test_count, int suite_count, int error_count);
static void clay_print_error(int num, const struct clay_error *error);
static void clay_print_ontest(const char *test_name, int test_number, int failed);
static void clay_print_onsuite(const char *suite_name);
static void clay_print_onabort(const char *msg, ...);

/* From clay_sandbox.c */
static void clay_unsandbox(void);
static int clay_sandbox(void);

/* Autogenerated test data by clay */
static const struct clay_func _clay_cb_buf_basic[] = {
    {"printf", &test_buf_basic__printf},
	{"resize", &test_buf_basic__resize}
};
static const struct clay_func _clay_cb_config_add[] = {
    {"to_existing_section", &test_config_add__to_existing_section},
	{"to_new_section", &test_config_add__to_new_section}
};
static const struct clay_func _clay_cb_config_new[] = {
    {"write_new_config", &test_config_new__write_new_config}
};
static const struct clay_func _clay_cb_config_read[] = {
    {"blank_lines", &test_config_read__blank_lines},
	{"case_sensitive", &test_config_read__case_sensitive},
	{"empty_files", &test_config_read__empty_files},
	{"header_in_last_line", &test_config_read__header_in_last_line},
	{"invalid_ext_headers", &test_config_read__invalid_ext_headers},
	{"lone_variable", &test_config_read__lone_variable},
	{"multiline_value", &test_config_read__multiline_value},
	{"number_suffixes", &test_config_read__number_suffixes},
	{"prefixes", &test_config_read__prefixes},
	{"simple_read", &test_config_read__simple_read},
	{"subsection_header", &test_config_read__subsection_header}
};
static const struct clay_func _clay_cb_config_stress[] = {
    {"dont_break_on_invalid_input", &test_config_stress__dont_break_on_invalid_input}
};
static const struct clay_func _clay_cb_config_write[] = {
    {"delete_inexistent", &test_config_write__delete_inexistent},
	{"delete_value", &test_config_write__delete_value},
	{"replace_value", &test_config_write__replace_value}
};
static const struct clay_func _clay_cb_core_buffer[] = {
    {"0", &test_core_buffer__0},
	{"1", &test_core_buffer__1},
	{"2", &test_core_buffer__2},
	{"3", &test_core_buffer__3},
	{"4", &test_core_buffer__4},
	{"5", &test_core_buffer__5},
	{"6", &test_core_buffer__6},
	{"7", &test_core_buffer__7},
	{"8", &test_core_buffer__8},
	{"9", &test_core_buffer__9}
};
static const struct clay_func _clay_cb_core_dirent[] = {
    {"dont_traverse_dot", &test_core_dirent__dont_traverse_dot},
	{"dont_traverse_empty_folders", &test_core_dirent__dont_traverse_empty_folders},
	{"traverse_slash_terminated_folder", &test_core_dirent__traverse_slash_terminated_folder},
	{"traverse_subfolder", &test_core_dirent__traverse_subfolder},
	{"traverse_weird_filenames", &test_core_dirent__traverse_weird_filenames}
};
static const struct clay_func _clay_cb_core_filebuf[] = {
    {"0", &test_core_filebuf__0},
	{"1", &test_core_filebuf__1},
	{"2", &test_core_filebuf__2},
	{"3", &test_core_filebuf__3},
	{"4", &test_core_filebuf__4},
	{"5", &test_core_filebuf__5}
};
static const struct clay_func _clay_cb_core_oid[] = {
    {"streq", &test_core_oid__streq}
};
static const struct clay_func _clay_cb_core_path[] = {
    {"0", &test_core_path__0},
	{"1", &test_core_path__1},
	{"2", &test_core_path__2},
	{"5", &test_core_path__5},
	{"6", &test_core_path__6},
	{"7", &test_core_path__7}
};
static const struct clay_func _clay_cb_core_rmdir[] = {
    {"delete_recursive", &test_core_rmdir__delete_recursive},
	{"fail_to_delete_non_empty_dir", &test_core_rmdir__fail_to_delete_non_empty_dir}
};
static const struct clay_func _clay_cb_core_string[] = {
    {"0", &test_core_string__0},
	{"1", &test_core_string__1}
};
static const struct clay_func _clay_cb_core_strtol[] = {
    {"int32", &test_core_strtol__int32},
	{"int64", &test_core_strtol__int64}
};
static const struct clay_func _clay_cb_core_vector[] = {
    {"0", &test_core_vector__0},
	{"1", &test_core_vector__1},
	{"2", &test_core_vector__2}
};
static const struct clay_func _clay_cb_index_rename[] = {
    {"single_file", &test_index_rename__single_file}
};
static const struct clay_func _clay_cb_network_remotes[] = {
    {"fnmatch", &test_network_remotes__fnmatch},
	{"parsing", &test_network_remotes__parsing},
	{"refspec_parsing", &test_network_remotes__refspec_parsing},
	{"transform", &test_network_remotes__transform}
};
static const struct clay_func _clay_cb_object_raw_chars[] = {
    {"build_valid_oid_from_raw_bytes", &test_object_raw_chars__build_valid_oid_from_raw_bytes},
	{"find_invalid_chars_in_oid", &test_object_raw_chars__find_invalid_chars_in_oid}
};
static const struct clay_func _clay_cb_object_raw_compare[] = {
    {"compare_allocfmt_oids", &test_object_raw_compare__compare_allocfmt_oids},
	{"compare_fmt_oids", &test_object_raw_compare__compare_fmt_oids},
	{"compare_pathfmt_oids", &test_object_raw_compare__compare_pathfmt_oids},
	{"succeed_on_copy_oid", &test_object_raw_compare__succeed_on_copy_oid},
	{"succeed_on_oid_comparison_equal", &test_object_raw_compare__succeed_on_oid_comparison_equal},
	{"succeed_on_oid_comparison_greater", &test_object_raw_compare__succeed_on_oid_comparison_greater},
	{"succeed_on_oid_comparison_lesser", &test_object_raw_compare__succeed_on_oid_comparison_lesser}
};
static const struct clay_func _clay_cb_object_raw_convert[] = {
    {"succeed_on_oid_to_string_conversion", &test_object_raw_convert__succeed_on_oid_to_string_conversion},
	{"succeed_on_oid_to_string_conversion_big", &test_object_raw_convert__succeed_on_oid_to_string_conversion_big}
};
static const struct clay_func _clay_cb_object_raw_fromstr[] = {
    {"fail_on_invalid_oid_string", &test_object_raw_fromstr__fail_on_invalid_oid_string},
	{"succeed_on_valid_oid_string", &test_object_raw_fromstr__succeed_on_valid_oid_string}
};
static const struct clay_func _clay_cb_object_raw_hash[] = {
    {"hash_buffer_in_single_call", &test_object_raw_hash__hash_buffer_in_single_call},
	{"hash_by_blocks", &test_object_raw_hash__hash_by_blocks},
	{"hash_commit_object", &test_object_raw_hash__hash_commit_object},
	{"hash_junk_data", &test_object_raw_hash__hash_junk_data},
	{"hash_multi_byte_object", &test_object_raw_hash__hash_multi_byte_object},
	{"hash_one_byte_object", &test_object_raw_hash__hash_one_byte_object},
	{"hash_tag_object", &test_object_raw_hash__hash_tag_object},
	{"hash_tree_object", &test_object_raw_hash__hash_tree_object},
	{"hash_two_byte_object", &test_object_raw_hash__hash_two_byte_object},
	{"hash_vector", &test_object_raw_hash__hash_vector},
	{"hash_zero_length_object", &test_object_raw_hash__hash_zero_length_object}
};
static const struct clay_func _clay_cb_object_raw_short[] = {
    {"oid_shortener_no_duplicates", &test_object_raw_short__oid_shortener_no_duplicates},
	{"oid_shortener_stresstest_git_oid_shorten", &test_object_raw_short__oid_shortener_stresstest_git_oid_shorten}
};
static const struct clay_func _clay_cb_object_raw_size[] = {
    {"validate_oid_size", &test_object_raw_size__validate_oid_size}
};
static const struct clay_func _clay_cb_object_raw_type2string[] = {
    {"check_type_is_loose", &test_object_raw_type2string__check_type_is_loose},
	{"convert_string_to_type", &test_object_raw_type2string__convert_string_to_type},
	{"convert_type_to_string", &test_object_raw_type2string__convert_type_to_string}
};
static const struct clay_func _clay_cb_object_tree_frompath[] = {
    {"fail_when_processing_an_invalid_path", &test_object_tree_frompath__fail_when_processing_an_invalid_path},
	{"fail_when_processing_an_unknown_tree_segment", &test_object_tree_frompath__fail_when_processing_an_unknown_tree_segment},
	{"retrieve_tree_from_path_to_treeentry", &test_object_tree_frompath__retrieve_tree_from_path_to_treeentry}
};
static const struct clay_func _clay_cb_odb_loose[] = {
    {"exists", &test_odb_loose__exists},
	{"simple_reads", &test_odb_loose__simple_reads}
};
static const struct clay_func _clay_cb_odb_packed[] = {
    {"mass_read", &test_odb_packed__mass_read},
	{"read_header_0", &test_odb_packed__read_header_0},
	{"read_header_1", &test_odb_packed__read_header_1}
};
static const struct clay_func _clay_cb_odb_sorting[] = {
    {"alternate_backends_sorting", &test_odb_sorting__alternate_backends_sorting},
	{"basic_backends_sorting", &test_odb_sorting__basic_backends_sorting}
};
static const struct clay_func _clay_cb_repo_getters[] = {
    {"empty", &test_repo_getters__empty},
	{"head_detached", &test_repo_getters__head_detached},
	{"head_orphan", &test_repo_getters__head_orphan}
};
static const struct clay_func _clay_cb_repo_init[] = {
    {"bare_repo", &test_repo_init__bare_repo},
	{"bare_repo_noslash", &test_repo_init__bare_repo_noslash},
	{"standard_repo", &test_repo_init__standard_repo},
	{"standard_repo_noslash", &test_repo_init__standard_repo_noslash}
};
static const struct clay_func _clay_cb_repo_open[] = {
    {"bare_empty_repo", &test_repo_open__bare_empty_repo},
	{"standard_empty_repo", &test_repo_open__standard_empty_repo}
};
static const struct clay_func _clay_cb_status_single[] = {
    {"hash_single_file", &test_status_single__hash_single_file}
};
static const struct clay_func _clay_cb_status_worktree[] = {
    {"empty_repository", &test_status_worktree__empty_repository},
	{"whole_repository", &test_status_worktree__whole_repository}
};

static const struct clay_suite _clay_suites[] = {
    {
        "buf::basic",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_buf_basic, 2
    },
	{
        "config::add",
        {"initialize", &test_config_add__initialize},
        {"cleanup", &test_config_add__cleanup},
        _clay_cb_config_add, 2
    },
	{
        "config::new",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_config_new, 1
    },
	{
        "config::read",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_config_read, 11
    },
	{
        "config::stress",
        {"initialize", &test_config_stress__initialize},
        {"cleanup", &test_config_stress__cleanup},
        _clay_cb_config_stress, 1
    },
	{
        "config::write",
        {"initialize", &test_config_write__initialize},
        {"cleanup", &test_config_write__cleanup},
        _clay_cb_config_write, 3
    },
	{
        "core::buffer",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_core_buffer, 10
    },
	{
        "core::dirent",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_core_dirent, 5
    },
	{
        "core::filebuf",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_core_filebuf, 6
    },
	{
        "core::oid",
        {"initialize", &test_core_oid__initialize},
        {NULL, NULL},
        _clay_cb_core_oid, 1
    },
	{
        "core::path",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_core_path, 6
    },
	{
        "core::rmdir",
        {"initialize", &test_core_rmdir__initialize},
        {NULL, NULL},
        _clay_cb_core_rmdir, 2
    },
	{
        "core::string",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_core_string, 2
    },
	{
        "core::strtol",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_core_strtol, 2
    },
	{
        "core::vector",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_core_vector, 3
    },
	{
        "index::rename",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_index_rename, 1
    },
	{
        "network::remotes",
        {"initialize", &test_network_remotes__initialize},
        {"cleanup", &test_network_remotes__cleanup},
        _clay_cb_network_remotes, 4
    },
	{
        "object::raw::chars",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_object_raw_chars, 2
    },
	{
        "object::raw::compare",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_object_raw_compare, 7
    },
	{
        "object::raw::convert",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_object_raw_convert, 2
    },
	{
        "object::raw::fromstr",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_object_raw_fromstr, 2
    },
	{
        "object::raw::hash",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_object_raw_hash, 11
    },
	{
        "object::raw::short",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_object_raw_short, 2
    },
	{
        "object::raw::size",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_object_raw_size, 1
    },
	{
        "object::raw::type2string",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_object_raw_type2string, 3
    },
	{
        "object::tree::frompath",
        {"initialize", &test_object_tree_frompath__initialize},
        {"cleanup", &test_object_tree_frompath__cleanup},
        _clay_cb_object_tree_frompath, 3
    },
	{
        "odb::loose",
        {"initialize", &test_odb_loose__initialize},
        {"cleanup", &test_odb_loose__cleanup},
        _clay_cb_odb_loose, 2
    },
	{
        "odb::packed",
        {"initialize", &test_odb_packed__initialize},
        {"cleanup", &test_odb_packed__cleanup},
        _clay_cb_odb_packed, 3
    },
	{
        "odb::sorting",
        {"initialize", &test_odb_sorting__initialize},
        {"cleanup", &test_odb_sorting__cleanup},
        _clay_cb_odb_sorting, 2
    },
	{
        "repo::getters",
        {"initialize", &test_repo_getters__initialize},
        {"cleanup", &test_repo_getters__cleanup},
        _clay_cb_repo_getters, 3
    },
	{
        "repo::init",
        {"initialize", &test_repo_init__initialize},
        {NULL, NULL},
        _clay_cb_repo_init, 4
    },
	{
        "repo::open",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_repo_open, 2
    },
	{
        "status::single",
        {NULL, NULL},
        {NULL, NULL},
        _clay_cb_status_single, 1
    },
	{
        "status::worktree",
        {"initialize", &test_status_worktree__initialize},
        {"cleanup", &test_status_worktree__cleanup},
        _clay_cb_status_worktree, 2
    }
};

static size_t _clay_suite_count = 34;
static size_t _clay_callback_count = 114;

/* Core test functions */
static void
clay_run_test(
	const struct clay_func *test,
	const struct clay_func *initialize,
	const struct clay_func *cleanup)
{
	int error_st = _clay.suite_errors;

	_clay.trampoline_enabled = 1;

	if (setjmp(_clay.trampoline) == 0) {
		if (initialize->ptr != NULL)
			initialize->ptr();

		test->ptr();
	}

	_clay.trampoline_enabled = 0;

	if (_clay.local_cleanup != NULL)
		_clay.local_cleanup(_clay.local_cleanup_payload);

	if (cleanup->ptr != NULL)
		cleanup->ptr();

	_clay.test_count++;

	/* remove any local-set cleanup methods */
	_clay.local_cleanup = NULL;
	_clay.local_cleanup_payload = NULL;

	clay_print_ontest(
		test->name,
		_clay.test_count,
		(_clay.suite_errors > error_st)
	);
}

static void
clay_report_errors(void)
{
	int i = 1;
	struct clay_error *error, *next;

	error = _clay.errors;
	while (error != NULL) {
		next = error->next;
		clay_print_error(i++, error);
		free(error->description);
		free(error);
		error = next;
	}

	_clay.errors = _clay.last_error = NULL;
}

static void
clay_run_suite(const struct clay_suite *suite)
{
	const struct clay_func *test = suite->tests;
	size_t i;

	clay_print_onsuite(suite->name);

	_clay.active_suite = suite->name;
	_clay.suite_errors = 0;

	for (i = 0; i < suite->test_count; ++i) {
		_clay.active_test = test[i].name;
		clay_run_test(&test[i], &suite->initialize, &suite->cleanup);
	}
}

#if 0 /* temporarily disabled */
static void
clay_run_single(const struct clay_func *test,
	const struct clay_suite *suite)
{
	_clay.suite_errors = 0;
	_clay.active_suite = suite->name;
	_clay.active_test = test->name;

	clay_run_test(test, &suite->initialize, &suite->cleanup);
}
#endif

static void
clay_usage(const char *arg)
{
	printf("Usage: %s [options]\n\n", arg);
	printf("Options:\n");
//	printf("  -tXX\t\tRun only the test number XX\n");
	printf("  -sXX\t\tRun only the suite number XX\n");
	exit(-1);
}

static void
clay_parse_args(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; ++i) {
		char *argument = argv[i];
		char action;
		int num;

		if (argument[0] != '-')
			clay_usage(argv[0]);

		action = argument[1];
		num = strtol(argument + 2, &argument, 10);

		if (*argument != '\0' || num < 0)
			clay_usage(argv[0]);

		switch (action) {
		case 's':
			if ((size_t)num >= _clay_suite_count) {
				clay_print_onabort("Suite number %d does not exist.\n", num);
				exit(-1);
			}

			clay_run_suite(&_clay_suites[num]);
			break;

		default:
			clay_usage(argv[0]);
		}
	}
}

static int
clay_test(int argc, char **argv)
{
	clay_print_init(
		(int)_clay_callback_count,
		(int)_clay_suite_count,
		""
	);

	if (clay_sandbox() < 0) {
		fprintf(stderr,
			"Failed to sandbox the test runner.\n"
			"Testing will proceed without sandboxing.\n");
	}

	if (argc > 1) {
		clay_parse_args(argc, argv);
	} else {
		size_t i;
		for (i = 0; i < _clay_suite_count; ++i)
			clay_run_suite(&_clay_suites[i]);
	}

	clay_print_shutdown(
		(int)_clay_callback_count,
		(int)_clay_suite_count,
		_clay.total_errors
	);

	clay_unsandbox();
	return _clay.total_errors;
}

void
clay__assert(
	int condition,
	const char *file,
	int line,
	const char *error_msg,
	const char *description,
	int should_abort)
{
	struct clay_error *error;

	if (condition)
		return;

	error = calloc(1, sizeof(struct clay_error));

	if (_clay.errors == NULL)
		_clay.errors = error;

	if (_clay.last_error != NULL)
		_clay.last_error->next = error;

	_clay.last_error = error;

	error->test = _clay.active_test;
	error->test_number = _clay.test_count;
	error->suite = _clay.active_suite;
	error->file = file;
	error->line_number = line;
	error->error_msg = error_msg;

	if (description != NULL)
		error->description = strdup(description);

	_clay.suite_errors++;
	_clay.total_errors++;

	if (should_abort) {
		if (!_clay.trampoline_enabled) {
			clay_print_onabort(
				"Fatal error: a cleanup method raised an exception.");
			exit(-1);
		}

		longjmp(_clay.trampoline, -1);
	}
}

void cl_set_cleanup(void (*cleanup)(void *), void *opaque)
{
	_clay.local_cleanup = cleanup;
	_clay.local_cleanup_payload = opaque;
}

static char _clay_path[4096];

static int
is_valid_tmp_path(const char *path)
{
	STAT_T st;

	if (stat(path, &st) != 0)
		return 0;

	if (!S_ISDIR(st.st_mode))
		return 0;

	return (access(path, W_OK) == 0);
}

static int
find_tmp_path(char *buffer, size_t length)
{
#ifndef _WIN32
	static const size_t var_count = 4;
	static const char *env_vars[] = {
		"TMPDIR", "TMP", "TEMP", "USERPROFILE"
 	};

 	size_t i;

	for (i = 0; i < var_count; ++i) {
		const char *env = getenv(env_vars[i]);
		if (!env)
			continue;

		if (is_valid_tmp_path(env)) {
			strncpy(buffer, env, length);
			return 0;
		}
	}

	/* If the environment doesn't say anything, try to use /tmp */
	if (is_valid_tmp_path("/tmp")) {
		strncpy(buffer, "/tmp", length);
		return 0;
	}

#else
	if (GetTempPath((DWORD)length, buffer))
		return 0;
#endif

	/* This system doesn't like us, try to use the current directory */
	if (is_valid_tmp_path(".")) {
		strncpy(buffer, ".", length);
		return 0;
	}

	return -1;
}

static void clay_unsandbox(void)
{
	if (_clay_path[0] == '\0')
		return;

#ifdef _WIN32
	chdir("..");
#endif

	fs_rm(_clay_path);
}

static int build_sandbox_path(void)
{
	const char path_tail[] = "clay_tmp_XXXXXX";
	size_t len;

	if (find_tmp_path(_clay_path, sizeof(_clay_path)) < 0)
		return -1;

	len = strlen(_clay_path);

#ifdef _WIN32
	{ /* normalize path to POSIX forward slashes */
		size_t i;
		for (i = 0; i < len; ++i) {
			if (_clay_path[i] == '\\')
				_clay_path[i] = '/';
		}
	}
#endif

	if (_clay_path[len - 1] != '/') {
		_clay_path[len++] = '/';
	}

	strncpy(_clay_path + len, path_tail, sizeof(_clay_path) - len);

#ifdef _WIN32
	if (mktemp_s(_clay_path, sizeof(_clay_path)) != 0)
		return -1;

	if (mkdir(_clay_path, 0700) != 0)
		return -1;
#else
	if (mkdtemp(_clay_path) == NULL)
		return -1;
#endif

	return 0;
}

static int clay_sandbox(void)
{
	if (_clay_path[0] == '\0' && build_sandbox_path() < 0)
		return -1;

	if (chdir(_clay_path) != 0)
		return -1;

	return 0;
}


static const char *
fixture_path(const char *base, const char *fixture_name)
{
	static char _path[4096];
	size_t root_len;

	root_len = strlen(base);
	strncpy(_path, base, sizeof(_path));

	if (_path[root_len - 1] != '/')
		_path[root_len++] = '/';

	if (fixture_name[0] == '/')
		fixture_name++;

	strncpy(_path + root_len,
		fixture_name,
		sizeof(_path) - root_len);

	return _path;
}

#ifdef CLAY_FIXTURE_PATH
const char *cl_fixture(const char *fixture_name)
{
	return fixture_path(CLAY_FIXTURE_PATH, fixture_name);
}

void cl_fixture_sandbox(const char *fixture_name)
{
	fs_copy(cl_fixture(fixture_name), _clay_path);
}

void cl_fixture_cleanup(const char *fixture_name)
{
	fs_rm(fixture_path(_clay_path, fixture_name));
}
#endif

#ifdef _WIN32

#define FOF_FLAGS (FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR)

static char *
fileops_path(const char *_path)
{
	char *path = NULL;
	size_t length, i;

	if (_path == NULL)
		return NULL;

	length = strlen(_path);
	path = malloc(length + 2);

	if (path == NULL)
		return NULL;

	memcpy(path, _path, length);
	path[length] = 0;
	path[length + 1] = 0;

	for (i = 0; i < length; ++i) {
		if (path[i] == '/')
			path[i] = '\\';
	}

	return path;
}

static void
fileops(int mode, const char *_source, const char *_dest)
{
	SHFILEOPSTRUCT fops;

	char *source = fileops_path(_source);
	char *dest = fileops_path(_dest);

	ZeroMemory(&fops, sizeof(SHFILEOPSTRUCT));

	fops.wFunc = mode;
	fops.pFrom = source;
	fops.pTo = dest;
	fops.fFlags = FOF_FLAGS;

	cl_assert_(
		SHFileOperation(&fops) == 0,
		"Windows SHFileOperation failed"
	);

	free(source);
	free(dest);
}

static void
fs_rm(const char *_source)
{
	fileops(FO_DELETE, _source, NULL);
}

static void
fs_copy(const char *_source, const char *_dest)
{
	fileops(FO_COPY, _source, _dest);
}

void
cl_fs_cleanup(void)
{
	fs_rm(fixture_path(_clay_path, "*"));
}

#else
static int
shell_out(char * const argv[])
{
	int status;
	pid_t pid;

	pid = fork();

	if (pid < 0) {
		fprintf(stderr,
			"System error: `fork()` call failed.\n");
		exit(-1);
	}

	if (pid == 0) {
		execv(argv[0], argv);
	}

	waitpid(pid, &status, 0);
	return WEXITSTATUS(status);
}

static void
fs_copy(const char *_source, const char *dest)
{
	char *argv[5];
	char *source;
	size_t source_len;

	source = strdup(_source);
	source_len = strlen(source);

	if (source[source_len - 1] == '/')
		source[source_len - 1] = 0;

	argv[0] = "/bin/cp";
	argv[1] = "-R";
	argv[2] = source;
	argv[3] = (char *)dest;
	argv[4] = NULL;

	cl_must_pass_(
		shell_out(argv),
		"Failed to copy test fixtures to sandbox"
	);

	free(source);
}

static void
fs_rm(const char *source)
{
	char *argv[4];

	argv[0] = "/bin/rm";
	argv[1] = "-Rf";
	argv[2] = (char *)source;
	argv[3] = NULL;

	cl_must_pass_(
		shell_out(argv),
		"Failed to cleanup the sandbox"
	);
}

void
cl_fs_cleanup(void)
{
	clay_unsandbox();
	clay_sandbox();
}
#endif


static void clay_print_init(int test_count, int suite_count, const char *suite_names)
{
	(void)suite_names;
	(void)suite_count;
	printf("TAP version 13\n");
	printf("1..%d\n", test_count);
}

static void clay_print_shutdown(int test_count, int suite_count, int error_count)
{
	(void)test_count;
	(void)suite_count;
	(void)error_count;

	printf("\n");
}

static void clay_print_error(int num, const struct clay_error *error)
{
	(void)num;

	printf("  ---\n");
	printf("  message : %s\n", error->error_msg);
	printf("  severity: fail\n");
	printf("  suite   : %s\n", error->suite);
	printf("  test    : %s\n", error->test);
	printf("  file    : %s\n", error->file);
	printf("  line    : %d\n", error->line_number);

	if (error->description != NULL)
		printf("  description: %s\n", error->description);

	printf("  ...\n");
}

static void clay_print_ontest(const char *test_name, int test_number, int failed)
{
	printf("%s %d - %s\n",
		failed ? "not ok" : "ok",
		test_number,
		test_name
	);

	clay_report_errors();
}

static void clay_print_onsuite(const char *suite_name)
{
	printf("# *** %s ***\n", suite_name);
}

static void clay_print_onabort(const char *msg, ...)
{
	va_list argp;
	va_start(argp, msg);
	fprintf(stdout, "Bail out! ");
	vfprintf(stdout, msg, argp);
	va_end(argp);
}


int _MAIN_CC main(int argc, char *argv[])
{
    return clay_test(argc, argv);
}
