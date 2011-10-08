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
#include "config.h"
#include "inotify.h"

#include <zmq.h>
#include <glib.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <linux/limits.h>

pthread_mutex_t zmq_mutex = PTHREAD_MUTEX_INITIALIZER;

static void zmq_dispatch_event(Request * req);

void *zmq_setup(void)
{
    int rv, bind_rv;
    void *zmq_context;
    char *zmq_uri;

    rv = asprintf(&zmq_uri, "%s:%d", ZMQ_ADDR, CONFIG->port);
    if (rv == -1) {
        log_error("Failed to allocate memory for the ZeroMQ URI: %s",
                  "zmq.c:zmq_setup()");
        return NULL;
    }

    zmq_context = zmq_init(ZMQ_THREADS);
    zmq_listener = zmq_socket(zmq_context, ZMQ_REP);
    bind_rv = zmq_bind(zmq_listener, zmq_uri);

    free(zmq_uri);

    if (bind_rv != 0) {
        log_error("Failed to bind ZeroMQ socket: '%s'", strerror(errno));
        return NULL;
    }

    return zmq_listener;
}

/* This is where we grab a 0MQ request off the socket, try to
 * identify it as JSON, and then send it to the json-c parsing
 * functions if it looks like valid JSON. If it parses correctly
 * and contains the manditory 'call' field then this request is
 * sent to the dispatcher.
 */
void zmq_handle_event(void)
{
    int i, rv, nil, msg_size;
    char *json;
    Request *req;

    pthread_mutex_lock(&zmq_mutex);

    zmq_msg_t request;
    zmq_msg_init(&request);
    rv = zmq_recv(zmq_listener, &request, 0);

    /* If the call to recv() failed then we need to tell the
     * client to reconnect. This should only happen under
     * very heavy load, or from connections from many clients.
     */
    if (rv != 0) {
        log_trace
            ("Failed to call recv(): %s. Sending reconnect error to client",
             zmq_strerror(errno));

        reply_send_error(ERROR_ZEROMQ_RECONNECT);
        pthread_mutex_unlock(&zmq_mutex);
        return;
    }

    msg_size = zmq_msg_size(&request);
    if (!msg_size > 0) {
        log_trace("Got 0 byte message. Skipping...");

        zmq_msg_close(&request);
        pthread_mutex_unlock(&zmq_mutex);
        return;
    }

    json = malloc(msg_size + 1);
    if (json == NULL) {
        log_error("Failed to allocate memory for JSON message: %s",
                  "zmq.c:zmq_handle_event()");
        reply_send_error(ERROR_MEMORY_ALLOCATION);
        return;
    }

    memcpy(json, zmq_msg_data(&request), msg_size);
    zmq_msg_close(&request);
    json[msg_size] = '\0';

    if (strlen(json) == 0) {
        log_trace("Message contained no data");

        free(json);
        pthread_mutex_unlock(&zmq_mutex);
        return;
    }

    log_trace("Received raw message: '%s'", json);

    /* Check for junk messages. This is not by any means an
     * exhaustive check for valid JSON. Just quick, dirty and
     * fast way to throw out invalid messages.
     *
     * We start by removing all the junk characters from the end
     * (if there are any) from the first closing curly '}' brace
     * on. Then, if there is any text left and the first and last
     * characters are an open curly and a close curly, respectively,
     * we continue on.
     */
    for (i = 0, nil = 0; i < strlen(json); i++) {
        if (nil == 1)
            json[i] = '\0';
        else if (json[i] == '}')
            nil = 1;
    }

    if (strlen(json) < 1
        || (json[0] != '{' && json[strlen(json) - 1] != '}')) {
        free(json);
        reply_send_error(ERROR_JSON_INVALID);
        pthread_mutex_unlock(&zmq_mutex);
        return;
    }

    /* Handle JSON parsing and request here. */
    req = request_parse(json);  /* Function is in request.c */
    if (req == (Request *) - 1) {
        log_error("Failed to allocate memory for new JSON message: %s",
                  json);
        reply_send_error(ERROR_MEMORY_ALLOCATION);
        pthread_mutex_unlock(&zmq_mutex);
        return;
    } else if (req == NULL) {
        log_error("Failed to parse JSON message: %s", json);

        free(json);
        reply_send_error(ERROR_JSON_PARSE);
        pthread_mutex_unlock(&zmq_mutex);
        return;
    }

    free(json);
    pthread_mutex_unlock(&zmq_mutex);

    zmq_dispatch_event(req);
}

/*
 * Event handlers
 */

static void EVENT_watch(const Request * req)
{
    int rv, mask, max_events;
    char *path;

    /* Grab the path from our request, or bail if the user
     * did not supply a valid one.
     */
    rv = asprintf(&path, request_get_path(req));
    if (rv == -1) {
        log_error("Failed to allocate memory for watch path: %s",
                  "zmq.c:EVENT_watch()");
        reply_send_error(ERROR_MEMORY_ALLOCATION);
        return;
    }

    if (path == NULL) {
        log_warn("JSON parsed successfully but no 'path' field found");
        reply_send_error(ERROR_JSON_KEY_NOT_FOUND);
        free(path);
        return;
    }

    if (path[0] != '/') {
        log_warn("Path '%s' is invalid. It must be an absolute path");
        reply_send_error(ERROR_NOT_ABSOLUTE_PATH);
        free(path);
        return;
    }

    log_debug("Watching new root at path '%s'", path);

    /* Check for user defined configuration overrides. */
    mask = request_get_mask(req);
    if (mask == 0) {
        mask = INOTIFY_DEFAULT_MASK;
        log_trace("Using default inotify mask: %lu", mask);
    } else {
        log_trace("Using user defined inotify mask: %lu", mask);
    }

    max_events = request_get_max_events(req);
    if (max_events == 0) {
        max_events = CONFIG->max_inotify_events;
        log_trace("Using default max events %d", max_events);
    } else {
        log_trace("Using user defined max events %d", max_events);
    }

    /* Watch our new root. */
    rv = inotify_watch_tree(path, mask, max_events);
    if (rv != 0) {
        reply_send_error(rv);
        free(path);
        return;
    }

    free(path);
    reply_send_success();
}

static void EVENT_unwatch(const Request * req)
{
    int rv;
    char *path = request_get_path(req);

    if (path == NULL) {
        log_warn("JSON parsed successfully but no 'path' field found");
        reply_send_error(ERROR_JSON_KEY_NOT_FOUND);
        return;
    }

    rv = inotify_unwatch_tree(path);
    if (rv != 0) {
        reply_send_error(rv);
        return;
    }

    reply_send_success();
}

static JOBJ inotify_event_to_jobj(const Event * event)
{
    JOBJ jobj;
    JOBJ jint_mask, jint_cookie;
    JOBJ jstr_path, jstr_name;
    /*JOBJ jint_wd, jint_len; UNNEEDED */

    jobj = json_object_new_object();

    jint_mask = json_object_new_int(event->mask);
    jint_cookie = json_object_new_int(event->cookie);
    jstr_path = json_object_new_string(event->path);
    jstr_name = json_object_new_string(event->name);
    /*jint_len    = json_object_new_int(event->len); */
    /*jint_wd     = json_object_new_int(event->wd); */

    /* Add our data */
    json_object_object_add(jobj, "name", jstr_name);
    json_object_object_add(jobj, "path", jstr_path);
    json_object_object_add(jobj, "mask", jint_mask);

    /* The inotify cookie value is only set when a file is moved.
     * for all other operations it's value is 0 (zero). So we only
     * pass this value on to the user if it has a value other than
     * zero.
     */
    if (event->cookie != 0)
        json_object_object_add(jobj, "cookie", jint_cookie);
    else
        json_object_put(jint_cookie);

    /* The following is data we don't need to pass to the client.
     *
     * json_object_object_add(jobj, "wd", jint_wd);
     * json_object_object_add(jobj, "len", jint_len);
     */

    return jobj;
}

static void EVENT_get_queue_size(const Request * req)
{
    int rv;
    char *reply;
    Root *root;
    guint size;

    const char* path = request_get_path(req);

    if (path == NULL) {
        log_warn("JSON parsed successfully but no 'path' field found");
        reply_send_error(ERROR_JSON_KEY_NOT_FOUND);
        return;
    }

    root = inotify_is_root(path);

    if (root == NULL) {
        log_warn("Path '%s' is not a currently watch root", path);
        reply_send_error(ERROR_INOTIFY_ROOT_NOT_WATCHED);
        return;
    }

    pthread_mutex_lock(&zmq_mutex);
    size = g_queue_get_length(root->queue);
    pthread_mutex_unlock(&zmq_mutex);

    rv = asprintf(&reply, "{\"data\":%d}", size);
    if (rv == -1) {
        log_error("Failed to allocate memory for reply JSON: %s",
                  "zmq.c:EVENT_get_queue_size");
        reply_send_error(ERROR_MEMORY_ALLOCATION);
        return;
    }

    reply_send_message(reply);

    free(reply);
}

static void EVENT_get_events(const Request * req)
{
    int i, count;
    char *path;
    Event **events;
    JOBJ jobj, jarr;

    path = request_get_path(req);

    if (path == NULL) {
        log_warn("JSON parsed successfully but no 'path' field found");
        reply_send_error(ERROR_JSON_KEY_NOT_FOUND);
        return;
    }

    if (inotify_is_root(path) == NULL) {
        log_warn("Path '%s' is not a currently watch root", path);
        reply_send_error(ERROR_INOTIFY_ROOT_NOT_WATCHED);
        return;
    }

    count = request_get_count(req);

    if (count == -1) {
        reply_send_error(ERROR_INVALID_EVENT_COUNT);
        return;
    }

    log_trace("Trying to get %d events for root '%s'", count, path);
    events = inotify_get_events(path, count);
    if (events == (Event **) - 1) {
        reply_send_error(ERROR_MEMORY_ALLOCATION);
        return;
    }

    if (events == NULL) {
        log_trace("No events found for root at path '%s'", path);
        reply_send_message("{\"data\":[]}");
        return;
    }

    jobj = json_object_new_object();
    jarr = json_object_new_array();

    for (i = 0; events[i]; i++) {
        json_object_array_add(jarr, inotify_event_to_jobj(events[i]));
    }
    json_object_object_add(jobj, "data", jarr);

    reply_send_message((char *) json_object_to_json_string(jobj));

    inotify_free_events(events);

    /* Using json-c, when you create new json_objects you need
     * to "put" them in order to get the correct memory freeing
     * when they fall out of scope. This is essentially json-c's
     * way of preemptively freeing dynamically allocated data.
     */
    json_object_put(jobj);
}

/* This is just a way for client code to see all of the
 * roots Inotispy is currently watching.
 */
static void EVENT_get_roots(void)
{
    int i;
    char **roots;
    JOBJ jobj, jarr;

    if (inotify_num_watched_roots < 1) {
        reply_send_message("{\"data\":[]}");
        return;
    }

    roots = inotify_get_roots();
    if (roots == NULL) {
        reply_send_error(ERROR_MEMORY_ALLOCATION);
        return;
    }

    jobj = json_object_new_object();
    jarr = json_object_new_array();

    for (i = 0; roots[i]; i++) {
        JOBJ path = json_object_new_string(roots[i]);
        json_object_array_add(jarr, path);
    }

    json_object_object_add(jobj, "data", jarr);

    reply_send_message((char *) json_object_to_json_string(jobj));

    inotify_free_roots(roots);
    json_object_put(jobj);
}

static void zmq_dispatch_event(Request * req)
{
    char *call = req->call;

    log_debug("Dispatching call '%s' with data '%s'",
              call, request_to_string(req));

    if (strcmp(call, "watch") == 0) {
        EVENT_watch(req);
    } else if (strcmp(call, "unwatch") == 0) {
        EVENT_unwatch(req);
    } else if (strcmp(call, "get_events") == 0) {
        EVENT_get_events(req);
    } else if (strcmp(call, "get_queue_size") == 0) {
        EVENT_get_queue_size(req);
    } else if (strcmp(call, "get_roots") == 0) {
        EVENT_get_roots();
    } else {
        log_warn("Unknown call: '%s'", call);
    }

    request_free(req);
}
