/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#include "posix.h"
#include "path.h"
#include "utf8-conv.h"
#include <errno.h>
#include <io.h>
#include <fcntl.h>


int p_unlink(const char *path)
{
	int ret = 0;
	wchar_t* buf;

	buf = conv_utf8_to_utf16(path);
	_wchmod(buf, 0666);
	ret = _wunlink(buf);
	free(buf);

	return ret;
}

int p_fsync(int fd)
{
	HANDLE fh = (HANDLE)_get_osfhandle(fd);

	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}

	if (!FlushFileBuffers(fh)) {
		DWORD code = GetLastError();

		if (code == ERROR_INVALID_HANDLE)
			errno = EINVAL;
		else
			errno = EIO;

		return -1;
	}

	return 0;
}

GIT_INLINE(time_t) filetime_to_time_t(const FILETIME *ft)
{
	long long winTime = ((long long)ft->dwHighDateTime << 32) + ft->dwLowDateTime;
	winTime -= 116444736000000000LL; /* Windows to Unix Epoch conversion */
	winTime /= 10000000;		 /* Nano to seconds resolution */
	return (time_t)winTime;
}

static int do_lstat(const char *file_name, struct stat *buf)
{
	WIN32_FILE_ATTRIBUTE_DATA fdata;
	wchar_t* fbuf = conv_utf8_to_utf16(file_name);

	if (GetFileAttributesExW(fbuf, GetFileExInfoStandard, &fdata)) {
		int fMode = S_IREAD;

		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			fMode |= S_IFDIR;
		else
			fMode |= S_IFREG;

		if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
			fMode |= S_IWRITE;

		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			fMode |= S_IFLNK;

		buf->st_ino = 0;
		buf->st_gid = 0;
		buf->st_uid = 0;
		buf->st_nlink = 1;
		buf->st_mode = (mode_t)fMode;
		buf->st_size = fdata.nFileSizeLow; /* Can't use nFileSizeHigh, since it's not a stat64 */
		buf->st_dev = buf->st_rdev = (_getdrive() - 1);
		buf->st_atime = filetime_to_time_t(&(fdata.ftLastAccessTime));
		buf->st_mtime = filetime_to_time_t(&(fdata.ftLastWriteTime));
		buf->st_ctime = filetime_to_time_t(&(fdata.ftCreationTime));

		free(fbuf);
		return GIT_SUCCESS;
	}

	free(fbuf);

	switch (GetLastError()) {
		case ERROR_ACCESS_DENIED:
		case ERROR_SHARING_VIOLATION:
		case ERROR_LOCK_VIOLATION:
		case ERROR_SHARING_BUFFER_EXCEEDED:
			return GIT_EOSERR;

		case ERROR_BUFFER_OVERFLOW:
		case ERROR_NOT_ENOUGH_MEMORY:
			return GIT_ENOMEM;

		default:
			return GIT_EINVALIDPATH;
	}
}

int p_lstat(const char *file_name, struct stat *buf)
{
	int namelen, error;
	char alt_name[GIT_PATH_MAX];

	if ((error = do_lstat(file_name, buf)) == GIT_SUCCESS)
		return GIT_SUCCESS;

	/* if file_name ended in a '/', Windows returned ENOENT;
	 * try again without trailing slashes
	 */
	if (error != GIT_EINVALIDPATH)
		return git__throw(GIT_EOSERR, "Failed to lstat file");

	namelen = strlen(file_name);
	if (namelen && file_name[namelen-1] != '/')
		return git__throw(GIT_EOSERR, "Failed to lstat file");

	while (namelen && file_name[namelen-1] == '/')
		--namelen;

	if (!namelen || namelen >= GIT_PATH_MAX)
		return git__throw(GIT_ENOMEM, "Failed to lstat file");

	memcpy(alt_name, file_name, namelen);
	alt_name[namelen] = 0;
	return do_lstat(alt_name, buf);
}

int p_readlink(const char *link, char *target, size_t target_len)
{
	typedef DWORD (WINAPI *fpath_func)(HANDLE, LPWSTR, DWORD, DWORD);
	static fpath_func pGetFinalPath = NULL;
	HANDLE hFile;
	DWORD dwRet;
	wchar_t* link_w;
	wchar_t* target_w;

	/*
	 * Try to load the pointer to pGetFinalPath dynamically, because
	 * it is not available in platforms older than Vista
	 */
	if (pGetFinalPath == NULL) {
		HINSTANCE library = LoadLibrary("kernel32");

		if (library != NULL)
			pGetFinalPath = (fpath_func)GetProcAddress(library, "GetFinalPathNameByHandleW");

		if (pGetFinalPath == NULL)
			return git__throw(GIT_EOSERR,
				"'GetFinalPathNameByHandleW' is not available in this platform");
	}

	link_w = conv_utf8_to_utf16(link);

	hFile = CreateFileW(link_w,			// file to open
			GENERIC_READ,			// open for reading
			FILE_SHARE_READ,		// share for reading
			NULL,					// default security
			OPEN_EXISTING,			// existing file only
			FILE_FLAG_BACKUP_SEMANTICS, // normal file
			NULL);					// no attr. template

	free(link_w);

	if (hFile == INVALID_HANDLE_VALUE)
		return GIT_EOSERR;

	if (target_len <= 0) {
		return GIT_EINVALIDARGS;
	}

	target_w = (wchar_t*)git__malloc(target_len * sizeof(wchar_t));

	dwRet = pGetFinalPath(hFile, target_w, target_len, 0x0);
	if (dwRet >= target_len) {
		free(target_w);
		CloseHandle(hFile);
		return GIT_ENOMEM;
	}

	if (!WideCharToMultiByte(CP_UTF8, 0, target_w, -1, target, target_len * sizeof(char), NULL, NULL)) {
		free(target_w);
		return GIT_EOSERR;
	}

	free(target_w);
	CloseHandle(hFile);

	if (dwRet > 4) {
		/* Skip first 4 characters if they are "\\?\" */
		if (target[0] == '\\' && target[1] == '\\' && target[2] == '?' && target[3] == '\\') {
			char tmp[GIT_PATH_MAX];
			unsigned int offset = 4;
			dwRet -= 4;

			/* \??\UNC\ */
			if (dwRet > 7 && target[4] == 'U' && target[5] == 'N' && target[6] == 'C') {
				offset += 2;
				dwRet -= 2;
				target[offset] = '\\';
			}

			memcpy(tmp, target + offset, dwRet);
			memcpy(target, tmp, dwRet);
		}
	}

	target[dwRet] = '\0';
	return dwRet;
}

int p_open(const char *path, int flags)
{
	int fd;
	wchar_t* buf = conv_utf8_to_utf16(path);
	fd = _wopen(buf, flags | _O_BINARY);

	free(buf);
	return fd;
}

int p_creat(const char *path, int mode)
{
	int fd;
	wchar_t* buf = conv_utf8_to_utf16(path);
	fd = _wopen(buf, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, mode);

	free(buf);
	return fd;
}

int p_getcwd(char *buffer_out, size_t size)
{
	wchar_t* buf = (wchar_t*)git__malloc(sizeof(wchar_t) * (int)size);
	_wgetcwd(buf, (int)size);

	if (!WideCharToMultiByte(CP_UTF8, 0, buf, -1, buffer_out, size, NULL, NULL)) {
		free(buf);
		return GIT_EOSERR;
	}

	free(buf);
	return GIT_SUCCESS;
}

int p_stat(const char* path, struct stat* buf)
{
	return do_lstat(path, buf);
}

int p_chdir(const char* path)
{
	wchar_t* buf = conv_utf8_to_utf16(path);
	int ret = _wchdir(buf);

	free(buf);
	return ret;
}

int p_chmod(const char* path, int mode)
{
	wchar_t* buf = conv_utf8_to_utf16(path);
	int ret = _wchmod(buf, mode);

	free(buf);
	return ret;
}

int p_rmdir(const char* path)
{
	wchar_t* buf = conv_utf8_to_utf16(path);
	int ret = _wrmdir(buf);

	free(buf);
	return ret;
}

int p_hide_directory__w32(const char *path)
{
	int error;
	wchar_t* buf = conv_utf8_to_utf16(path);

	error = SetFileAttributesW(buf, FILE_ATTRIBUTE_HIDDEN) != 0 ?
		GIT_SUCCESS : GIT_ERROR; /* MSDN states a "non zero" value indicates a success */

	free(buf);

	if (error < GIT_SUCCESS)
		error = git__throw(GIT_EOSERR, "Failed to hide directory '%s'", path);

	return error;
}

char *p_realpath(const char *orig_path, char *buffer)
{
	int ret, alloc = 0;
	wchar_t* orig_path_w = conv_utf8_to_utf16(orig_path);
	wchar_t* buffer_w = (wchar_t*)git__malloc(GIT_PATH_MAX * sizeof(wchar_t));

	if (buffer == NULL) {
		buffer = (char *)git__malloc(GIT_PATH_MAX);
		alloc = 1;
	}

	ret = GetFullPathNameW(orig_path_w, GIT_PATH_MAX, buffer_w, NULL);
	free(orig_path_w);

	if (!ret || ret > GIT_PATH_MAX) {
		free(buffer_w);
		if (alloc) free(buffer);

		return NULL;
	}

	if (!WideCharToMultiByte(CP_UTF8, 0, buffer_w, -1, buffer, GIT_PATH_MAX, NULL, NULL)) {
		free(buffer_w);
		if (alloc) free(buffer);
	}
	
	free(buffer_w);
	git_path_mkposix(buffer);
	return buffer;
}

int p_vsnprintf(char *buffer, size_t count, const char *format, va_list argptr)
{
#ifdef _MSC_VER
	int len = _vsnprintf(buffer, count, format, argptr);
	return (len < 0) ? _vscprintf(format, argptr) : len;
#else /* MinGW */
	return vsnprintf(buffer, count, format, argptr);
#endif
}

int p_snprintf(char *buffer, size_t count, const char *format, ...)
{
	va_list va;
	int r;

	va_start(va, format);
	r = p_vsnprintf(buffer, count, format, va);
	va_end(va);

	return r;
}

extern int p_creat(const char *path, int mode);

int p_mkstemp(char *tmp_path)
{
#if defined(_MSC_VER)
	if (_mktemp_s(tmp_path, GIT_PATH_MAX) != 0)
		return GIT_EOSERR;
#else
	if (_mktemp(tmp_path) == NULL)
		return GIT_EOSERR;
#endif

	return p_creat(tmp_path, 0744);
}

char* p_getenv(const char* name)
{
	wchar_t* buf;
	wchar_t* name_w = conv_utf8_to_utf16(name);
	char* ret;
	DWORD len;

	len = GetEnvironmentVariableW(name_w, NULL, 0);
	if (len == 0) {
		free(name_w);
		return NULL;
	}

	len++;  /* Null Terminator */
	buf = malloc(sizeof(wchar_t) * len);
	GetEnvironmentVariableW(name_w, buf, len);

	ret = conv_utf16_to_utf8(buf);

	free(name_w);
	free(buf);
	return ret;
}

int p_setenv(const char* name, const char* value, int overwrite)
{
	if (overwrite != 1)
		return EINVAL;

	return (SetEnvironmentVariableA(name, value) == 0 ? GIT_EOSERR : GIT_SUCCESS);
}
