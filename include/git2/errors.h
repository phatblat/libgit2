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
#ifndef INCLUDE_git_errors_h__
#define INCLUDE_git_errors_h__

/**
 * @file git2/errors.h
 * @brief Git error handling routines and variables
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/**
 * Return a detailed error string with the latest error
 * that occurred in the library.
 * @return a string explaining the error
 */
GIT_EXTERN(const char *) git_lasterror(void);

/**
 * strerror() for the Git library
 *
 * Get a string description for a given error code.
 * NOTE: This method will be eventually deprecated in favor
 * of the new `git_lasterror`.
 *
 * @param num The error code to explain
 * @return a string explaining the error code
 */
GIT_EXTERN(const char *) git_strerror(int num);

/** @} */
GIT_END_DECL
#endif
