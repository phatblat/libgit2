#ifndef INCLUDE_git_refspec_h__
#define INCLUDE_git_refspec_h__

#include "git2/types.h"

/**
 * Get the source specifier
 *
 * @param refspec the refspec
 * @return the refspec's source specifier
 */
const char *git_refspec_src(const git_refspec *refspec);

/**
 * Get the destination specifier
 *
 * @param refspec the refspec
 * @return the refspec's destination specifier
 */
const char *git_refspec_dst(const git_refspec *refspec);

#endif
