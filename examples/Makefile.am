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

if HAS_DATAROOTDIR
    DATA_ROOT = $(datarootdir)
else
    DATA_ROOT = $(prefix)/share
endif

cexampledir = $(DATA_ROOT)/doc/@PACKAGE@/examples
dist_cexample_DATA = c/get_roots.c \
    c/get_events.c \
    c/get_queue_size.c \
    c/watch_dir.c \
    c/unwatch_dir.c \
    c/Makefile \
    c/README

perlexampledir = $(DATA_ROOT)/doc/@PACKAGE@/examples
dist_perlexample_DATA = perl/get_roots.pl \
    perl/get_events.pl \
    perl/get_queue_size.pl \
    perl/watch_dir.pl \
    perl/unwatch_dir.pl \
    perl/README
