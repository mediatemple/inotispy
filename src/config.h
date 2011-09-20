/*
 * Copyright (c) 2011-*, James Conerly <james@jamesconerly.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _INOTISPY_CONFIG_H_
#define _INOTISPY_CONFIG_H_

#include "zmq.h"
#include "log.h"
#include "inotify.h"

#include <glib.h>

#define CONF_GROUP "global"
#define INOTISPY_CONFIG_FILE "/etc/inotispy.conf"

struct inotispy_config {

    /* zmq.h */
    int port;

    /* log.h */
    int   log_level;
    char *log_file;

    /* inotify.h */
    int max_inotify_events;

    /* Toggle printing information to stderr */
    gboolean silent;
};

struct inotispy_config *CONFIG;

/* Read in config values from /etc/inotispy.conf.
 *
 * Takes:
 *  - boolean value toggeling silent mode.
 *
 * Returns:
 *  - 0 (zero) on success.
 *  - 1 on failure.
 */
int init_config (gboolean silent);

#endif /*_INOTISPY_CONFIG_H_*/
