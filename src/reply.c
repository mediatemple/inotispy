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
#include "reply.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *error_to_string (uint32_t err_code);

int
reply_send_message (char *message)
{
    int rv;
    zmq_msg_t msg;

    rv = zmq_msg_init_size(&msg, strlen(message));
    if (rv != 0) {
        LOG_ERROR("Failed to initialize message '%s': %s (%d)",
            message, zmq_strerror(errno), errno);
        return 1;
    }

    strncpy(zmq_msg_data(&msg), message, strlen(message));
    rv = zmq_send(zmq_listener, &msg, 0);

    if (rv != 0) {
        LOG_ERROR("Failed to send message '%s': %s (%d)",
            message, zmq_strerror(errno), errno);
        return 1;
    }

    return 0;
}

int
reply_send_error (uint32_t err_code)
{
    char *err;
    asprintf(&err,
        "{\"error\": {\"code\":%d, \"message\":\"%s\"}}",
            err_code, error_to_string(err_code));

    int rv = reply_send_message(err);

    free(err);
    return rv;
}

int
reply_send_success (void)
{
    return reply_send_message("{\"success\":1}");
}

char *
error_to_string (uint32_t err_code)
{
    if (err_code & ERROR_JSON_INVALID)
        return "Invalid JSON";
    else if (err_code & ERROR_JSON_PARSE)
        return "Failed to parse JSON";
    else if (err_code & ERROR_JSON_KEY_NOT_FOUND)
        return "Key not found in JSON";
    else if (err_code & ERROR_INOTIFY_WATCH_FAILED)
        return "Failed to set up inotify watch";
    else if (err_code & ERROR_INOTIFY_UNWATCH_FAILED)
        return "Failed to unwatch inotify watch";
    else if (err_code & ERROR_INVALID_EVENT_COUNT)
        return "Invald event count value";
    else if (err_code & ERROR_ZERO_BYTE_MESSAGE)
        return "Zero byte message received";
    else if (err_code & ERROR_INOTIFY_ROOT_NOT_WATCHED)
        return "This root is currently not watched under inotify";
    else if (err_code & ERROR_ZEROMQ_RECONNECT)
        return "Please re-initialize your ZeroMQ connection and reconnect to Inotispy";
    else if (err_code & ERROR_NOT_ABSOLUTE_PATH)
        return "Path must be absolute";
    else
        return "Unknown error";
}
