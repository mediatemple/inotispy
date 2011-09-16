#
# Copyright (c) 2011-*, James Conerly <james@jamesconerly.com>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

AC_PREREQ([2.67])
AC_INIT([inotispy], [1.0.1], [james@jamesconerly.com])

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AC_CONFIG_SRCDIR([src/main.c])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADER([sys/inotify.h],,AC_MSG_ERROR([cannot find header sys/inotify.h]))
AC_CHECK_HEADER([zmq.h],,AC_MSG_ERROR([cannot find header zmq.h]))
AC_CHECK_HEADER([json/json.h],,AC_MSG_ERROR([cannot find header json/json.h]))
AC_CHECK_HEADER([pthread.h],,AC_MSG_ERROR([cannot find header pthread.h]))

AC_CHECK_HEADERS(glib.h [glib/ghash.h] [glib/gqueue.h] stdint.h stdlib.h string.h unistd.h time.h errno.h dirent.h unistd.h [sys/stat.h] [sys/types.h] stdarg.h [linux/limits.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strerror strstr vasprintf strcpy asprintf read])

AC_OUTPUT(Makefile src/Makefile)
