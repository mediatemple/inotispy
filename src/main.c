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
#include <signal.h>
#include <stdlib.h>             /* exit() */
#include <unistd.h>             /* alarm() */
#include <sys/utsname.h>        /* uname() */
#include <getopt.h>
#include <unistd.h>

/* This is the timer for dumping the rewatch roots, as well as
 * for checking to see if the configuration has been updated.
 */
#define ALARM_TIMEOUT 10

void print_help_and_exit(void);
void sig_handler(int sig);

int main(int argc, char **argv)
{
    int c, rv, inotify_fd;
    void *zmq_receiver;
    struct utsname u_name;
    int option_index;
    int silent, help;
    char *config_file;

    static struct option long_opts[] = {
        {"silent", no_argument, 0, 's'},
        {"help", no_argument, 0, 'h'},
        {"config", required_argument, 0, 'c'},
        {0, 0, 0, 0}
    };

    zmq_pollitem_t items[2];

    /* Make sure we're on linux. */
    rv = uname(&u_name);
    if (rv == -1) {
        fprintf(stderr, "Failed to call uname(2): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (strcmp(u_name.sysname, "Linux") != 0) {
        fprintf(stderr, "Tisk tisk... Inotispy is for Linux.\n");
        exit(EXIT_FAILURE);
    }

    /* Handle command line args. */
    silent = 0;
    help = 0;
    config_file = NULL;
    option_index = 0;

    while ((c =
            getopt_long(argc, argv, "shc:", long_opts,
                        &option_index)) != -1) {
        switch (c) {
        case 's':
            silent = 1;
            break;
        case 'h':
            help = 1;
            break;
        case 'c':
            config_file = optarg;
            break;
        case '?':
            if (optopt == 'c')
                fprintf(stderr, "Option -%c requires an argument.\n",
                        optopt);
            else if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            else
                fprintf(stderr,
                        "Unknown option character `\\x%x'.\n", optopt);
            return 1;
        default:
            abort();
        }
    }

    if (help)
        print_help_and_exit();

    /* Initialize our configuration. */
    init_config(silent, config_file);

    if (!CONFIG->silent)
        fprintf(stderr, "Running Inotispy with pid %d...\n", getpid());

    /* Logging. */
    rv = init_logger();
    if (rv != 0)
        return EXIT_FAILURE;

    /* Signal handling for graceful dying. */
    signal(SIGINT, sig_handler);
    signal(SIGSEGV, sig_handler);
    signal(SIGALRM, sig_handler);

    log_notice("Initializing daemon");

    inotify_fd = inotify_setup();
    assert(inotify_fd > 0);

    zmq_receiver = zmq_setup();
    if (zmq_receiver == NULL) {
        fprintf(stderr,
                "Failed to start Inotispy. Please see the log file for details\n");
        return EXIT_FAILURE;
    }

    items[0].socket = NULL;
    items[0].fd = inotify_fd;
    items[0].events = ZMQ_POLLIN;

    items[1].socket = zmq_receiver;
    items[1].events = ZMQ_POLLIN;

    log_debug("Entering event loop...");

    alarm(ALARM_TIMEOUT);

    while (1) {

        rv = zmq_poll(items, 2, -1);
        if ((rv == -1) && (errno != EINTR)) {
            log_error("Failed to call zmq_poll(): %d: %s", errno,
                      strerror(errno));
            continue;
        }

        if (items[0].revents & ZMQ_POLLIN) {
            inotify_handle_event();
        }

        if (items[1].revents & ZMQ_POLLIN) {
            zmq_handle_event();
        }
    }
}

void sig_handler(int sig)
{
    switch (sig) {
    case SIGINT:
        if (!CONFIG->silent)
            printf("Interrupt received. Dying gracefully...\n");

        log_notice("Inotispy receieved an interrupt. %s",
                   "Dumping roots and exiting");
        inotify_dump_roots();
        exit(sig);
        break;
    case SIGSEGV:
        log_error("Inotispy encounterd a segmentation fault. Exiting...");
        exit(sig);
        break;
    case SIGALRM:
        inotify_dump_roots();
        if (config_has_an_update()) {
            if (reload_config() != 0) {
                log_warn
                    ("Failed to reload newly updated configuration file");
            } else {
                set_log_level(CONFIG->log_level);
                log_notice
                    ("Configuration file had an update and was reloaded");
            }
        }
        alarm(ALARM_TIMEOUT);
        break;
    }
}

void print_help_and_exit(void)
{
    printf("Usage: inotispy [options]\n");
    printf("\n");
    printf("  -h, --help           Print this help and exit.\n");
    printf("  -s, --silent         Turn off printing to stderr.\n");
    printf
        ("  -c, --config=CONFIG  Specify the location of the config file.\n");
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
        ("please refer to the manpage documentation included with this distribution.\n\n");

    exit(EXIT_SUCCESS);
}
