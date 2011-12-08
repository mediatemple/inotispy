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
#include "utils.h"

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>             /* exit() */
#include <sys/utsname.h>        /* uname() */
#include <getopt.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>

static pthread_mutex_t main_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Default location for our processes pid file. */
#define DEFAULT_PID_FILE "/var/run/inotispy/inotispy.pid"
static char *pid_file;

/* This is the timer for dumping the rewatch roots, as well as
 * for checking to see if the configuration has been updated.
 */
#define ALARM_TIMEOUT 10
static time_t alarm_timer;

/* Are we a daemon? */
static int daemon_mode;

static void print_help_and_exit(void);
static void alarm_handler(void);
static void sig_handler(int sig);
static void write_pid();
static void check_pid();
static int clear_pid();

int main(int argc, char **argv)
{
    int pid, c, rv, inotify_fd;
    void *zmq_receiver;
    struct utsname u_name;
    int option_index;
    int silent, help;
    char *config_file;

    static struct option long_opts[] = {
        {"daemon", no_argument, 0, 'd'},
        {"pidfile", required_argument, 0, 'p'},
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
    daemon_mode = 0;
    silent = 0;
    help = 0;
    pid_file = DEFAULT_PID_FILE;
    config_file = NULL;
    option_index = 0;

    while ((c =
            getopt_long(argc, argv, "dshc:p:", long_opts,
                        &option_index)) != -1) {
        switch (c) {
        case 'd':
            daemon_mode = 1;
            break;
        case 's':
            silent = 1;
            break;
        case 'h':
            help = 1;
            break;
        case 'p':
            pid_file = optarg;
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
            exit(1);
        default:
            abort();
        }
    }

    if (help)
        print_help_and_exit();

    if (daemon_mode) {
        check_pid();

        silent = 1;
        if (daemon(0, 0) == -1) {
            fprintf(stderr, "Failed to start inotispy in daemon mode: %s",
                    strerror(errno));
            exit(1);
        }
        write_pid();
    }

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
    signal(SIGHUP, sig_handler);
    signal(SIGSEGV, sig_handler);

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

    alarm_timer = time(NULL);
    alarm_handler();

    while (1) {

        rv = zmq_poll(items, 2, -1);
        if ((rv == -1) && (errno != EINTR)) {
            log_error("Failed to call zmq_poll(): %d: %s", errno,
                      strerror(errno));
            continue;
        }

        if (time(NULL) - alarm_timer > ALARM_TIMEOUT) {
            alarm_handler();
            alarm_timer = time(NULL);
        }

        if (items[0].revents & ZMQ_POLLIN) {
            inotify_handle_event();
        }

        if (items[1].revents & ZMQ_POLLIN) {
            zmq_handle_event();
        }
    }
}

static void check_pid(void)
{
    DIR *d;
    FILE *fp;
    char line[8];
    char *procdir;
    int pid, rv;

    /* Check to see if the pid file exists and if it has
     * a value in it.
     */
    fp = fopen(pid_file, "r");
    if (fp == NULL)
        return;

    if (fgets(line, sizeof line, fp) == NULL) {
        if (!CONFIG->silent)
            fprintf(stderr, "Found pid file %s but it was empty",
                    pid_file);
        fclose(fp);
        return;
    }

    if (line[strlen(line) - 1] == '\n')
        line[strlen(line) - 1] = '\0';

    pid = atoi(line);
    fclose(fp);

    rv = mk_string(&procdir, "/proc/%d", pid);
    if (rv == -1) {
        fprintf(stderr,
                "Failed to allocate memory while in check_pid()\n");
        exit(1);
    }

    /* We have a pid file with a value, now let's check to see if
     * this pid is currently running or is stale. If it's stale we
     * remove it and let Inotispy run.
     */
    d = opendir(procdir);
    free(procdir);

    if (d == NULL) {
        fprintf(stderr,
                "Found stale pid file. Cleaning up and running Inotispy\n");

        if (clear_pid() != 0)
            exit(1);

        return;
    }

    closedir(d);
    fprintf(stderr,
            "Inotispy is currently running in daemon mode. Exiting...\n");
    exit(1);
}

static int clear_config(void)
{
    int rv;

    rv = unlink(CONF_DUMP_FILE);
    if (rv == -1) {
        if (daemon_mode)
            fprintf(stderr, "Failed to unlink config dump file %s: %s\n",
                    CONF_DUMP_FILE, strerror(errno));
        return 1;
    }

    return 0;
}

static int clear_pid(void)
{
    int rv;

    rv = unlink(pid_file);
    if (rv == -1) {
        if (daemon_mode)
            fprintf(stderr, "Failed to unlink stale pid file %s: %s\n",
                    pid_file, strerror(errno));
        return 1;
    }

    return 0;
}

static void write_pid(void)
{
    FILE *fp;

    fp = fopen(pid_file, "w");
    if (fp == NULL) {
        fprintf(stderr, "Failed to open pid file %s for writing: %s\n",
                pid_file, strerror(errno));
        exit(1);
    }

    fprintf(fp, "%d", getpid());
    fclose(fp);
}

static void sig_handler(int sig)
{
    switch (sig) {
    case SIGINT:
        if (!CONFIG->silent)
            printf("Interrupt received. Dying gracefully...\n");

        log_notice("Inotispy receieved an interrupt. %s",
                   "Dumping roots and exiting");
        inotify_dump_roots();
        clear_pid();
        clear_config();
        exit(sig);
        break;
    case SIGSEGV:
        log_error("Inotispy encounterd a segmentation fault. Exiting...");
        exit(sig);
        break;
    case SIGHUP:
        alarm_handler();
        break;
    }
}

static void alarm_handler(void)
{
    pthread_mutex_lock(&main_mutex);

    /* Dump rewatch roots. */
    inotify_dump_roots();

    /* Handle updates to config file. */
    if (config_has_an_update()) {
        if (reload_config() != 0) {
            log_warn("Failed to reload newly updated configuration file");
        } else {
            set_log_level(CONFIG->log_level);
            log_notice
                ("Configuration file had an update and was reloaded");
        }
    }

    /* Dump current configuration. */
    print_config(CONF_DUMP_FILE);

    pthread_mutex_unlock(&main_mutex);
}

static void print_help_and_exit(void)
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
