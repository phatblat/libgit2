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
#include "refspec.h"

int git_refspec_parse(git_refspec *refspec, const char *str)
{
	char *delim;

	memset(refspec, 0x0, sizeof(git_refspec));

	if (*str == '+') {
		refspec->force = 1;
		str++;
	}

	delim = strchr(str, ':');
	if (delim == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse refspec. No ':'");

	refspec->src = git__strndup(str, delim - str);
	if (refspec->src == NULL)
		return GIT_ENOMEM;

	refspec->dst = git__strdup(delim + 1);
	if (refspec->dst == NULL) {
		free(refspec->src);
		refspec->src = NULL;
		return GIT_ENOMEM;
	}

	return GIT_SUCCESS;
}

const char *git_refspec_src(const git_refspec *refspec)
{
	return refspec->src;
}

const char *git_refspec_dst(const git_refspec *refspec)
{
	return refspec->dst;
}
