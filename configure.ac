#
# Copyright (c) 2011-*, (mt) MediaTemple <mediatemple.net>
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
#  - Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  - Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CON-
# SEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

AC_PREREQ([2.67])
AC_INIT([inotispy], [1.0.1], [james@jamesconerly.com])

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AC_CONFIG_SRCDIR([src/main.c])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES(GLIB2, glib-2.0 >= 2.22)

# CPPFLAGS for Glib
CPPFLAGS=$(pkg-config --cflags glib-2.0)
#CPPFLAGS="-I/usr/lib64/glib-2.0/include/ -I/usr/lib/glib-2.0/include -I/usr/include/glib-2.0"

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADER([sys/inotify.h],,AC_MSG_ERROR([Cannot find header sys/inotify.h. Please make sure you are on a Linux operating system to use this software.]))
AC_CHECK_HEADER([zmq.h],,AC_MSG_ERROR([Cannot find header zmq.h. Please download and install ZeroMQ from http://www.zeromq.org/area:download ]))
AC_CHECK_HEADER([json/json.h],,AC_MSG_ERROR([Cannot find header json/json.h. Please download and install json-c http://oss.metaparadigm.com/json-c/]))
AC_CHECK_HEADER([pthread.h],,AC_MSG_ERROR([Cannot find header pthread.h. Please make sure pthreads are installed and that your system is capable of running threaded software.]))

AC_CHECK_HEADERS(stdint.h stdlib.h string.h unistd.h time.h errno.h dirent.h unistd.h [sys/stat.h] [sys/types.h] stdarg.h [linux/limits.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strerror strstr vasprintf strcpy asprintf read])

AC_OUTPUT(Makefile src/Makefile bin/Makefile doc/Makefile)
