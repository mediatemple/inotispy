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

# CPPFLAGS for Glib
CPPFLAGS="-I/usr/lib/glib-2.0/include -I/usr/include/glib-2.0"

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADER([sys/inotify.h],,AC_MSG_ERROR([Cannot find header sys/inotify.h. Please make sure you are on a Linux operating system to use this software.]))
AC_CHECK_HEADER([zmq.h],,AC_MSG_ERROR([Cannot find header zmq.h. Please download and install ZeroMQ from http://www.zeromq.org/area:download ]))
AC_CHECK_HEADER([json/json.h],,AC_MSG_ERROR([Cannot find header json/json.h. Please download and install json-c http://oss.metaparadigm.com/json-c/]))
AC_CHECK_HEADER([pthread.h],,AC_MSG_ERROR([Cannot find header pthread.h. Please make sure pthreads are installed and that your system is capable of running threaded software.]))
AC_CHECK_HEADER([glib.h],,AC_MSG_ERROR([Cannot find header glib.h. Please download and install GLib-2.0 from here http://www.linuxfromscratch.org/blfs/view/cvs/general/glib2.html]))
AC_CHECK_HEADER([glib/ghash.h],,AC_MSG_ERROR([Cannot find header glib/ghash.h. Please download and install GLib-2.0 from here http://www.linuxfromscratch.org/blfs/view/cvs/general/glib2.html]))
AC_CHECK_HEADER([glib/gqueue.h],,AC_MSG_ERROR([Cannot find header glib/gqueue.h. Please download and install GLib-2.0 from here http://www.linuxfromscratch.org/blfs/view/cvs/general/glib2.html]))

AC_CHECK_HEADERS(stdint.h stdlib.h string.h unistd.h time.h errno.h dirent.h unistd.h [sys/stat.h] [sys/types.h] stdarg.h [linux/limits.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strerror strstr vasprintf strcpy asprintf read])

AC_OUTPUT(Makefile src/Makefile bin/Makefile)
