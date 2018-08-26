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

/** @file charset.h @brief Character conversion interface */

#ifndef CHARSET_H
#  define CHARSET_H

#  include <wchar.h>
#  include <iconv.h>

/** @brief Convert a multibyte string to a wide string
 * @param s Multibyte string to convert
 * @return Wide-character equivalent of @p s, or a null pointer on error
 */
wchar_t *sftp_mbs2wcs(const char *s);

/** @brief Generic string converter
 * @param a Allocator in which to store result
 * @param cd Conversion descriptor created by @c iconv_open()
 * @param sp Input/output string
 * @return 0 on success, -1 on error
 *
 * On input, @c *sp points to the string to convert.
 *
 * On success, @c *sp is replaced with a pointer to converted string.  On error
 * it is not modified.
 */
int sftp_iconv(struct allocator *a, iconv_t cd, char **sp);

#endif /* CHARSET_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
