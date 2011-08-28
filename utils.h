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


/** @brief Read bytes from @p fd
 * @param fd File descriptor to read from
 * @param buffer Buffer to store data in
 * @param size Number of bytes to read
 * @return 0 on success, non-0 if EOF before @p size bytes read
 *
 * Loops if necessary to cope with short reads.  Calls fatal() on error.
 */
int do_read(int fd, void *buffer, size_t size);

/* libreadliine contains xmalloc/xrealloc!  We use some #defines to work around
 * the problem. */
#define xmalloc sftp__xmalloc
#define xrealloc sftp__xrealloc

/** @brief Allocate memory
 * @param n Number of bytes to allocate
 * @return Pointer to allocated memory
 *
 * Equivalent to @c malloc() but calls fatal() on error.
 *
 * Does not zero-fill.
 */
void *xmalloc(size_t n);

/** @brief Allocate memory
 * @param n Number of objects to allocate
 * @param size Size of one object
 * @return Pointer to allocated memory
 *
 * Equivalent to @c calloc() but calls fatal() on error.
 */
void *xcalloc(size_t n, size_t size);

/** @brief Reallocate memory
 * @param ptr Existing allocation
 * @param n Number of bytes to allocate
 * @return Pointer to allocated memory
 *
 * Equivalent to @c realloc() but calls fatal() on error.
 *
 * Does not zero-fill.
 */
void *xrealloc(void *ptr, size_t n);

/** @brief Reallocate memory
 * @param ptr Existing allocation
 * @param n Number of objects to allocate
 * @param size Size of one object
 * @return Pointer to allocated memory
 *
 * Equivalent to @c realloc() but calls fatal() on error.
 *
 * Does not zero-fill.
 */
void *xrecalloc(void *ptr, size_t n, size_t size);

/** @brief Duplicate a string
 * @param s String to duplicate
 * @return Duplicated string
 *
 * Equivalent to @c strdup() but calls fatal() on error.
 */
char *xstrdup(const char *s);

/** @brief Append to a string
 * @param a Allocator
 * @param s Existing string
 * @param ns Where length of @p s is stored
 * @param t String to append
 * @return New string
 */
char *append(struct allocator *a, char *s, size_t *ns, 
             const char *t);

/** @brief Append to a string
 * @param a Allocator
 * @param s Existing string
 * @param ns Where length of @p s is stored
 * @param t String to append
 * @param lt Length of @p t
 * @return New string
 */
char *appendn(struct allocator *a, char *s, size_t *ns, 
              const char *t, size_t lt);

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
 * If @ref RP_MUST_EXIST is set then the path will be converted even if it
 * does
 * not exist or cannot be accessed.  If it is clear but the path does not exist
 * or cannot be accessed then an error _may_ be returned (but this is not
 * guaranteed).
 *
 * Setting @ref RP_MUST_EXIST is an optimization for the case where you're
 * later
 * going to do an existence test.
 *
 * Sets @c errno on erorr.
 */
char *sftp_find_realpath(struct allocator *a, const char *path,
                         unsigned flags);

/** @brief Follow symlinks
 *
 * See sftp_find_realpath().
 */
#define RP_READLINK 0x0001

/** @brief Path must exist
 *
 * See sftp_find_realpath().
 */
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
 * The error is written either to standard error or syslog; see @ref
 * log_syslog.
 *
 * Terminates the process.
 */
void fatal(const char *msg, ...)
  attribute((noreturn))
  attribute((format(printf,1,2)));

/** @brief Fork a subprocess
 * @return 0 in the child, process ID in the parent
 *
 * Calls fatal() on error.
 */
pid_t xfork(void);

/** @brief Called after forking
 *
 * Affects the way that fatal() terminates the process.
 *
 * xfork() already calls it, any other calls to @c fork() should call it
 * explicitly.
 */
void forked(void);

/** @brief Whether to log to syslog
 *
 * Affects where fatal() writes to.
 */
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
