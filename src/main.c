/*
 * Copyright (c) 2011-*, (mt) MediaTemple <mediatemple.net>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CON-
 * SEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "log.h"
#include "zeromq.h"
#include "config.h"
#include "inotify.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>             /* exit() */

void print_help_and_exit(void);

int main(int argc, char **argv)
{
    int rv, inotify_fd;
    void *zmq_receiver;

    zmq_pollitem_t items[2];

    /* A few command line args to handle. */
    if (argc == 2 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_help_and_exit();
    }

    if (argc == 2 &&
        (strcmp(argv[1], "-s") == 0 || strcmp(argv[1], "--silent") == 0))
        init_config(TRUE);      /* Silent mode ON */
    else
        init_config(FALSE);     /* Silent mode OFF */

    if (!CONFIG->silent)
        fprintf(stderr, "Running Inotispy...\n");

    /* Run all initialization functionality. */

    rv = init_logger();
    if (rv != 0)
        return 1;

    log_notice("Initializing daemon");

    inotify_fd = inotify_setup();
    assert(inotify_fd > 0);

    zmq_receiver = zmq_setup();
    if (zmq_receiver == NULL) {
        fprintf(stderr,
                "Failed to start Inotispy. Please see the log file for details\n");
        return 1;
    }

    items[0].socket = NULL;
    items[0].fd = inotify_fd;
    items[0].events = ZMQ_POLLIN;

    items[1].socket = zmq_receiver;
    items[1].events = ZMQ_POLLIN;

    log_debug("Entering event loop...");

    while (1) {

        rv = zmq_poll(items, 2, -1);
        if (rv == -1) {
            log_error("Failed to call zmq_poll(): %s", strerror(errno));
            continue;
        }

        if (items[0].revents & ZMQ_POLLIN) {
            /* log_trace("Found inotify event"); */
            inotify_handle_event();
        }

        if (items[1].revents & ZMQ_POLLIN) {
            /* log_trace("Found 0MQ event"); */
            zmq_handle_event();
        }
    }

    return 0;
}

void print_help_and_exit(void)
{
    printf("Usage: inotispy [--silent]\n");
    printf("\n");
    printf("  -s, --silent  Turn off printing to stderr.\n");
    printf("\n");
    printf
        ("Inotispy is an efficient file system change notification daemon based\n");
    printf
        ("on inotify. It recursively watches directory trees, queues file system\n");
    printf
        ("events that occur within those trees, and delivers those events to\n");
    printf("client applications via ZeroMQ sockets.\n");
    printf("\n");
    printf
        ("For more information on running, configuring, managing and using Inotispy\n");
    printf
        ("please refer to the documentation found at http://www.inotispy.org\n\n");

    exit(1);
}
