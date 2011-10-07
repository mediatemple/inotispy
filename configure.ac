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
AC_CONFIG_SRCDIR([src/main.c])
AM_INIT_AUTOMAKE([1.9 tar-pax foreign])

AC_DEFINE(_GNU_SOURCE)

# Checks for programs.
AC_PROG_CC_C99

# Checks for packages via pkg-config
PKG_CHECK_MODULES(DEPS, glib-2.0 >= 2.22 json libzmq)

AC_CHECK_HEADER([sys/inotify.h],,AC_MSG_ERROR([Cannot find header sys/inotify.h. Please make sure you are on a Linux operating system to use this software.]))

m4_include([ax_pthread.m4])

AX_PTHREAD([],AC_MSG_ERROR([Must have POSIX threads]))

LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

AC_OUTPUT(Makefile src/Makefile bin/Makefile doc/Makefile etc/Makefile)
