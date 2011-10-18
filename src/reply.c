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
#include "reply.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int reply_send_message(const char *message)
{
    int rv;
    zmq_msg_t msg;

    rv = zmq_msg_init_size(&msg, strlen(message));
    if (rv != 0) {
        log_error("Failed to initialize message '%s': %s (%d)",
                  message, zmq_strerror(errno), errno);
        return 1;
    }

    strncpy(zmq_msg_data(&msg), message, strlen(message));
    rv = zmq_send(zmq_listener, &msg, ZMQ_NOBLOCK);

    if (rv != 0) {
        log_error("Failed to send message '%s': %s (%d)",
                  message, zmq_strerror(errno), errno);
        return 1;
    }

    return 0;
}

int reply_send_error(unsigned int err_code)
{
    int rv, do_free;
    char *err;

    do_free = 1;
    rv = mk_string(&err,
                   "{\"error\": {\"code\":%d, \"message\":\"%s\"}}",
                   err_code, error_to_string(err_code));
    if (rv == -1) {
        log_error("Failed to allocate memory for error reply: %s",
                  "reply.c:reply_send_error()");
        err =
            "{\"error\": {\"code\":0, \"Memory allocation error. Please check the logs for more details\"}}";
        do_free = 0;
    }


    rv = reply_send_message(err);

    if (do_free)
        free(err);

    return rv;
}

int reply_send_success(void)
{
    return reply_send_message("{\"success\":1}");
}

const char *error_to_string(unsigned int err_code)
{
    switch (err_code) {
    case ERROR_JSON_INVALID:
        return "Invalid JSON";
    case ERROR_JSON_PARSE:
        return "Failed to parse JSON";
    case ERROR_JSON_KEY_NOT_FOUND:
        return "Key not found in JSON";
    case ERROR_INOTIFY_WATCH_FAILED:
        return "Failed to set up new inotify watch";
    case ERROR_INOTIFY_UNWATCH_FAILED:
        return "Failed to unwatch root";
    case ERROR_INVALID_EVENT_COUNT:
        return "Invald event count value";
    case ERROR_ZERO_BYTE_MESSAGE:
        return "Zero byte message received";
    case ERROR_INOTIFY_ROOT_NOT_WATCHED:
        return "This root is currently not watched under inotify";
    case ERROR_INOTIFY_ROOT_ALREADY_WATCHED:
        return "This root is currently being watched under inotify";
    case ERROR_INOTIFY_PARENT_OF_ROOT:
        return "This directory is the parent of a currently watched root";
    case ERROR_INOTIFY_CHILD_OF_ROOT:
        return "This directory is the child of a currently watched root";
    case ERROR_INOTIFY_ROOT_QUEUE_FULL:
        return "Inotify event queue is full for this root";
    case ERROR_INOTIFY_ROOT_DOES_NOT_EXIST:
        return "This directory does not exist";
    case ERROR_MEMORY_ALLOCATION:
        return
            "Failed to allocate new memory. Check the log for the specific module and function where the error occurred.";
    case ERROR_FAILED_TO_CREATE_NEW_THREAD:
        return "Failed to create a new thread";
    case ERROR_INOTIFY_ROOT_BEING_DESTROYED:
        return "Inotify root currently being destroyed";
    case ERROR_ZEROMQ_RECONNECT:
        return
            "Please re-initialize your ZeroMQ connection and reconnect to Inotispy";
    case ERROR_NOT_ABSOLUTE_PATH:
        return "Path must be absolute";
    case ERROR_BAD_CALL:
        return "User tried to execute an unsupported call";
    default:
        return "Unknown error";
    }
}
