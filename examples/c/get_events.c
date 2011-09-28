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

#include <zmq.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <json/json.h>

json_object *_parse_json(char *json)
{
    struct json_object *jobj;
    struct json_tokener *jtok;

    jtok = json_tokener_new();
    jobj = json_tokener_parse_ex(jtok, json, -1);

    if (jtok->err != json_tokener_success) {
	jobj = NULL;
    }

    json_tokener_free(jtok);

    return jobj;
}

char *fmt_event(json_object * event)
{
    json_object *name, *path, *mask;
    char *fmt;

    mask = json_object_object_get(event, "mask");
    name = json_object_object_get(event, "name");
    path = json_object_object_get(event, "path");

    asprintf(&fmt, "%s/%s (%d)", json_object_get_string(path),
	     json_object_get_string(name), json_object_get_int(mask));

    return fmt;
}

void print_events(json_object * events)
{
    int i, len;
    char *fmt;
    json_object *event;

    len = json_object_array_length(events);

    if (len > 0) {
	printf("Got events:\n");

	for (i = 0; i < len; i++) {
	    event = json_object_array_get_idx(events, i);
	    fmt = fmt_event(event);
	    printf("%d. %s\n", (i + 1), fmt);
	    free(fmt);
	}
    } else {
	printf("There are no events at this time\n");
    }
}

int main(int argc, char **argv)
{
    struct json_object *jobj, *events;
    int connect_rv, rv, msg_size;
    char *path, *message;
    void *context, *socket;
    zmq_msg_t request, reply;

    if (argc != 2) {
	printf("Usage: get_events <PATH>\n");
	return 1;
    }

    /* You'll probably want to make sure the path is valid here. */
    path = argv[1];

    context = zmq_init(1);
    socket = zmq_socket(context, ZMQ_REQ);
    connect_rv = zmq_connect(socket, "tcp://127.0.0.1:5559");

    if (connect_rv != 0) {
	printf("Failed to connect ZeroMQ socket: %s\n",
	       zmq_strerror(errno));
	return 1;
    }

    /* Request */
    asprintf(&message,
	     "{\"call\":\"get_events\",\"path\":\"%s\",\"count\":0}",
	     path);
    zmq_msg_init_size(&request, strlen(message));
    strncpy(zmq_msg_data(&request), message, strlen(message));
    free(message);

    rv = zmq_send(socket, &request, 0);
    zmq_msg_close(&request);
    if (rv != 0) {
	printf("Failed to send message to server: %s\n",
	       zmq_strerror(errno));
	return 1;
    }

    /* Reply */
    zmq_msg_init(&reply);
    rv = zmq_recv(socket, &reply, 0);
    if (rv != 0) {
	printf("Failed to receive message from server: %s\n",
	       zmq_strerror(errno));
	return 1;
    }

    msg_size = zmq_msg_size(&reply);
    message = malloc(msg_size + 1);
    memcpy(message, zmq_msg_data(&reply), msg_size);
    zmq_msg_close(&reply);
    message[msg_size] = '\0';

    /* Parse JSON */
    jobj = _parse_json(message);

    if (jobj == NULL) {
	printf("Failed to parse JSON!\n");
	return 1;
    }

    events = json_object_object_get(jobj, "data");
    if (events == NULL) {
	printf("Failed to find data for events in reply\n");
    } else if (json_object_is_type(events, json_type_array)) {
	print_events(events);
    }

    free(message);
    zmq_close(socket);
    zmq_term(context);

    return 0;
}
