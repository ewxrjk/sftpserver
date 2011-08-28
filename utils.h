/*
 * This file is part of the Green End SFTP Server.
 * Copyright (C) 2007, 2011 Richard Kettlewell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/** @file utils.h @brief Utility functions interface */

#ifndef UTILS_H
#define UTILS_H


int do_read(int fd, void *buffer, size_t size);
/* Error-checking workalike for read().  Returns 0 on success, non-0 at
 * EOF. */

/* libreadliine contains xmalloc/xrealloc!  We use some #defines to work around
 * the problem. */
#define xmalloc sftp__xmalloc
#define xrealloc sftp__xrealloc

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t size);
void *xrealloc(void *ptr, size_t n);
void *xrecalloc(void *ptr, size_t n, size_t size);
char *xstrdup(const char *s);
/* Error-checking workalikes for malloc() etc.  recalloc() does not
 * 0-fill expansion. */

char *append(struct allocator *a, char *s, size_t *ns, 
             const char *t);
char *appendn(struct allocator *a, char *s, size_t *ns, 
              const char *t, size_t lt);
/* Append T to S, expanding if need be.  NS tracks total size of S.  LT is the
 * length of T if present. */

/** @brief Convenient wrapper for readlink(2)
 * @param a Allocator to store result
 * @param path Path name to inspect
 * @return Value of symbolic link or a null pointer on error
 *
 * Sets @c errno on erorr.
 */
char *sftp_do_readlink(struct allocator *a, const char *path);

/** @brief Return the real (canonical) name of @p path
 * @param a Allocator to store result
 * @param path Path name to inspect
 * @param flags Flags
 * @return Canonical path name or a null pointer
 *
 * Valid flags are:
 * - @ref RP_READLINK
 * - @ref RP_MUST_EXIST
 *
 * If @ref RP_READLINK is set then symbolic links are followed.  Otherwise they
 * are
 * not and the transformation is purely lexical.
 *
 * If @ref RP_MAY_NOT_EXIST is set then the path will be converted even if it
 * does
 * not exist or cannot be accessed.  If it is clear but the path does not exist
 * or cannot be accessed then an error _may_ be returned (but this is not
 * guaranteed).
 *
 * Leaving @ref RP_MAY_NOT_EXIST is an optimization for the case where you're
 * later
 * going to do an existence test.
 *
 * Sets @c errno on erorr.
 */
char *sftp_find_realpath(struct allocator *a, const char *path,
                         unsigned flags);

/** @brief Follow symlinks */
#define RP_READLINK 0x0001

/** @brief Path must exist */
#define RP_MUST_EXIST 0x0002

/** @brief Compute the name of the current directory
 * @param a Allocator to store result
 * @return Name of current directory, or a null pointer
 */
char *sftp_getcwd(struct allocator *a);

/** @brief Compute the directory name part of @p path
 * @param a Allocator to store result
 * @param path Path name to extract directory from
 * @return Directory name
 */
const char *sftp_dirname(struct allocator *a, const char *path);

/** @brief Write an error and terminate
 * @param msg Format string as per @c printf(3)
 * @param ... Arguments
 *
 * Terminates the process.
 */
void fatal(const char *msg, ...)
  attribute((noreturn))
  attribute((format(printf,1,2)));

pid_t xfork(void);
void forked(void);

extern int log_syslog;

#endif /* UTILS_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
