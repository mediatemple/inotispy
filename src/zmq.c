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

#define MLOCK() pthread_mutex_lock(&zmq_mutex);
#define MUNLOCK() pthread_mutex_unlock(&zmq_mutex);

void *zmq_setup(void)
{
    int bind_rv;
    void *zmq_context;
    char *zmq_uri;

    asprintf(&zmq_uri, "%s:%d", ZMQ_ADDR, CONFIG->port);

    zmq_context = zmq_init(ZMQ_THREADS);
    zmq_listener = zmq_socket(zmq_context, ZMQ_REP);
    bind_rv = zmq_bind(zmq_listener, zmq_uri);

    free(zmq_uri);

    if (bind_rv != 0) {
	LOG_ERROR("Failed to bind ZeroMQ socket: '%s'", strerror(errno));
	return NULL;
    }

    return zmq_listener;
}

/* This is where we grab a 0MQ request off the socket, try to
 * identify it as JSON, and then send it to the json-c parsing
 * functions if it looks like valid JSON. If it parses correctly
 * and contains the manditory 'call' field then this request is
 * sent to the dispatcher.
 *
 * XXX CODE REVIEW
 *
 *     My check for junk messages feels sloppy, though it seems to
 *     work just fine. I was looking for a quick way to avoid sending
 *     non-JSON data to the parser. Is there a better way to do this?
 *     Should I be doing this at all or should I just pass data along
 *     and let the parser handle junk?
 */
void zmq_handle_event(void *receiver)
{
    int i, rv, nil, msg_size;
    char *json;
    Request *req;

    MLOCK();
    zmq_msg_t request;
    zmq_msg_init(&request);
    rv = zmq_recv(zmq_listener, &request, 0);

    /* If the call to recv() failed then we need to tell the
     * client to reconnect. This should only happen under
     * very heavy load, or from connections from many clients.
     */
    if (rv != 0) {
	LOG_TRACE
	    ("Failed to call recv(): %s. Sending reconnect error to client",
	     zmq_strerror(errno));

	reply_send_error(ERROR_ZEROMQ_RECONNECT);
	MUNLOCK();
	return;
    }

    msg_size = zmq_msg_size(&request);
    if (!msg_size > 0) {
	LOG_TRACE("Got 0 byte message. Skipping...");

	zmq_msg_close(&request);
	MUNLOCK();
	return;
    }

    json = malloc(msg_size + 1);
    memcpy(json, zmq_msg_data(&request), msg_size);
    zmq_msg_close(&request);
    json[msg_size] = '\0';

    if (strlen(json) == 0) {
	LOG_TRACE("Message contained no data");

	free(json);
	MUNLOCK();
	return;
    }

    LOG_TRACE("Received raw message: '%s'", json);

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
	MUNLOCK();
	return;
    }

    /* Handle JSON parsing and request here. */
    req = request_parse(json);	/* Function is in request.c */

    if (req == NULL) {
	LOG_WARN("Invalid JSON message: %s", json);

	free(json);
	reply_send_error(ERROR_JSON_PARSE);
	MUNLOCK();
	return;
    }

    free(json);
    MUNLOCK();

    zmq_dispatch_event(req);
}

/*
 * Event handlers
 */

void EVENT_watch(Request * req)
{
    int mask, max_events;
    char *path;

    /* Grab the path from our request, or bail if the user
     * did not supply a valid one.
     */
    path = request_get_path(req);

    if (path == NULL) {
	LOG_WARN("JSON parsed successfully but no 'path' field found");
	reply_send_error(ERROR_JSON_KEY_NOT_FOUND);
	return;
    }

    if (path[0] != '/') {
	LOG_WARN("Path '%s' is invalid. It must be an absolute path");
	reply_send_error(ERROR_NOT_ABSOLUTE_PATH);
	return;
    }

    LOG_DEBUG("Watching new root at path '%s'", path);

    /* Check for user defined configuration overrides. */
    mask = request_get_mask(req);
    if (mask == 0) {
	mask = INOTIFY_DEFAULT_MASK;
	LOG_TRACE("Using default inotify mask: %lu", mask);
    } else {
	LOG_TRACE("Using user defined inotify mask: %lu", mask);
    }

    max_events = request_get_max_events(req);
    if (max_events == 0) {
	max_events = CONFIG->max_inotify_events;
	LOG_TRACE("Using default max events %d", max_events);
    } else {
	LOG_TRACE("Using user defined max events %d", max_events);
    }

    /* Watch our new root. */
    if (inotify_watch_tree(path, mask, max_events) != 0) {
	reply_send_error(ERROR_INOTIFY_WATCH_FAILED);
	return;
    }

    reply_send_success();
}

void EVENT_unwatch(Request * req)
{
    char *path = request_get_path(req);

    if (path == NULL) {
	LOG_WARN("JSON parsed successfully but no 'path' field found");
	reply_send_error(ERROR_JSON_KEY_NOT_FOUND);
	return;
    }

    if (inotify_unwatch_tree(path) != 0) {
	reply_send_error(ERROR_INOTIFY_UNWATCH_FAILED);
	return;
    }

    reply_send_success();
}

/* XXX CODE REVIEW
 *
 *     I don't know a better, more dynamic way to turn a C struct
 *     into a JSON object. This seems super messy, but this is C
 *     and I'm not sure there is a more elegant solution.
 */
JOBJ inotify_event_to_jobj(Event * event)
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

void EVENT_get_queue_size(Request * req)
{
    char *path, *reply;
    Root *root;
    guint size;

    path = request_get_path(req);

    if (path == NULL) {
	LOG_WARN("JSON parsed successfully but no 'path' field found");
	reply_send_error(ERROR_JSON_KEY_NOT_FOUND);
	return;
    }

    root = inotify_is_root(path);

    if (root == NULL) {
	LOG_WARN("Path '%s' is not a currently watch root", path);
	reply_send_error(ERROR_INOTIFY_ROOT_NOT_WATCHED);
	return;
    }

    MLOCK();
    size = g_queue_get_length(root->queue);
    MUNLOCK();

    asprintf(&reply, "{\"data\":%d}", size);
    reply_send_message(reply);

    free(reply);
}

void EVENT_get_events(Request * req)
{
    int i, count;
    char *path;
    Event **events;
    JOBJ jobj, jarr;

    path = request_get_path(req);

    if (path == NULL) {
	LOG_WARN("JSON parsed successfully but no 'path' field found");
	reply_send_error(ERROR_JSON_KEY_NOT_FOUND);
	return;
    }

    if (inotify_is_root(path) == NULL) {
	LOG_WARN("Path '%s' is not a currently watch root", path);
	reply_send_error(ERROR_INOTIFY_ROOT_NOT_WATCHED);
	return;
    }

    count = request_get_count(req);

    if (count == -1) {
	reply_send_error(ERROR_INVALID_EVENT_COUNT);
	return;
    }

    LOG_TRACE("Trying to get %d events for root '%s'", count, path);
    events = inotify_get_events(path, count);

    if (events == NULL) {
	LOG_TRACE("No events found for root at path '%s'", path);
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
void EVENT_get_roots(void)
{
    int i;
    char **roots;
    JOBJ jobj, jarr;

    if (inotify_num_watched_roots < 1) {
	reply_send_message("{\"data\":[]}");
	return;
    }

    roots = inotify_get_roots();

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

/* XXX CODE REVIEW
 * 
 *     Another way to do this would be to set up constant (or enum)
 *     integer values for each of the possible calls with some hooks
 *     for validity checking:
 *     
 *       #define CALL_RANGE_MIN 1
 *       #define CALL_RANGE_MAX N
 *     
 *       #define CALL_WATCH   1
 *       #define CALL_UNWATCH 2
 *         ...
 *       #define CALL_FOO     N
 *     
 *       #define CALL_VALID(X) \
 *         (return ((X >= CALL_RANGE_MIN) && (X <= CALL_RANGE_MAX)) ? 1 : 0)
 *     
 *     This would make the code a little more clear since I would
 *     use a proper switch clause, and it would probably make the
 *     code more efficient since a switch clause would be compairing
 *     integer values instead of doing a character by character
 *     string comparison. However, this approach would require client
 *     code using Inotispy to do one of the following:
 *     
 *       * Use meaningless integer constants in their code.
 *       * Define their own set of named constants to use.
 *       * Find an existing Inotispy binding in their language
 *         that already has these constants defined.
 *     
 *     If anyone thinks this is a better approach please say so and
 *     I'll do the logic swap.
 */
void zmq_dispatch_event(Request * req)
{
    char *call = req->call;

    LOG_DEBUG("Dispatching call '%s' with data '%s'",
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
	LOG_WARN("Unknown call: '%s'", call);
    }

    request_free(req);
}
