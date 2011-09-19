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

#include "log.h"
#include "zmq.h"
#include "config.h"
#include "inotify.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

int
main(void)
{
    int   rc, inotify_fd;
    void *zmq_receiver;

    zmq_pollitem_t items[2];

    init_config();
    fprintf(stderr, "Running Inotispy...\n");

    init_logger();
    LOG_NOTICE("Initializing daemon");

    inotify_fd = inotify_setup();
    assert(inotify_fd > 0);

    zmq_receiver = zmq_setup();
    assert(zmq_receiver != NULL);

    items[0].socket = NULL;
    items[0].fd     = inotify_fd;
    items[0].events = ZMQ_POLLIN;

    items[1].socket = zmq_receiver;
    items[1].events = ZMQ_POLLIN;

    LOG_DEBUG("Entering event loop...");

    while (1) {

        rc = zmq_poll(items, 2, -1);
        if (rc == -1) {
            LOG_ERROR("Failed to call zmq_poll(): %s", strerror(errno));
            continue;
        }

        if(items[0].revents & ZMQ_POLLIN) {
            LOG_TRACE("Found inotify event");
            inotify_handle_event(inotify_fd);
        }

        if(items[1].revents & ZMQ_POLLIN) {
            LOG_TRACE("Found 0MQ event");
            zmq_handle_event(zmq_receiver);
        }
    }

    return 0;
}

