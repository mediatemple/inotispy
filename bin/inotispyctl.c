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

#include <zmq.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json/json.h>

void *socket, *context;

void print_help (void);

json_object *
parse_json (char *json)
{
    struct json_object  *jobj;
    struct json_tokener *jtok;

    jtok = json_tokener_new();
    jobj = json_tokener_parse_ex(jtok, json, -1);

    if(jtok->err != json_tokener_success) {
        json_tokener_free(jtok);
        printf("Failed to parse JSON: %s\n", json);
        exit(1);
    }

    json_tokener_free(jtok);

    return jobj;
}

void
handle_error (char *json)
{
    json_object *jobj, *error, *code, *message;

    jobj = parse_json(json);

    error = json_object_object_get(jobj, "error");
    if (error == NULL)
        return;

    code    = json_object_object_get(error, "code");
    message = json_object_object_get(error, "message");

    printf("ERROR: %d: %s\n", json_object_get_int(code),
        json_object_get_string(message));

    exit(1);
}

void
send_request (char *message)
{
    int rv;
    zmq_msg_t request;

    zmq_msg_init_size(&request, strlen(message));
    strncpy(zmq_msg_data(&request), message, strlen(message));

    rv = zmq_send(socket, &request, 0);
    zmq_msg_close(&request);
    if (rv != 0) {
         printf("Failed to send message to server: %s\n", zmq_strerror(errno));
         exit(1);
    }
}

char *
get_reply (void)
{
    int rv, msg_size;
    char *message;
    zmq_msg_t reply;

    zmq_msg_init (&reply);
    rv = zmq_recv(socket, &reply, 0);
    if (rv != 0) {
         printf("Failed to receive message from server: %s\n", zmq_strerror(errno));
         exit(1);
    }

    msg_size = zmq_msg_size(&reply);
    message = malloc(msg_size + 1);
    memcpy(message, zmq_msg_data(&reply), msg_size);
    zmq_msg_close(&reply);
    message[msg_size] = '\0';

    handle_error(message);

    return message;
}

int
get_queue_size (char *path)
{
    int rv;
    char *request, *reply;
    json_object *jobj, *queue_size;

    asprintf(&request, "{\"call\":\"get_queue_size\",\"path\":\"%s\"}", path);
    send_request(request);
    free(request);

    reply = get_reply();
    jobj = parse_json(reply);
    free(reply);

    queue_size = json_object_object_get(jobj, "data");
    if (queue_size == NULL) {
        printf("Failed to find data for queue_size in reply\n");
        exit(1);
    }
    else if (json_object_is_type(queue_size, json_type_int)) {
        rv = json_object_get_int(queue_size);
    }
    else {
        printf("Data received from server had invalid format\n");
        exit(1);
    }

    return rv;
}

char *
get_events_raw (char *path, int count)
{
    char *request;

    asprintf(&request,
        "{\"call\":\"get_events\",\"path\":\"%s\",\"count\":%d}", path, count);
    send_request(request);
    free(request);

    return get_reply();
}

void
flush_queue (char *path)
{
    get_events_raw(path, 0);
    printf("Queue for root %s has been flushed\n", path);
}

char *
fmt_event (json_object *event)
{
    json_object *name, *path, *mask;
    char *fmt;

    mask = json_object_object_get(event, "mask");
    name = json_object_object_get(event, "name");
    path = json_object_object_get(event, "path");

    asprintf(&fmt, "%s/%s  %d", json_object_get_string(path),
        json_object_get_string(name), json_object_get_int(mask));

    return fmt;
}


void
print_events (json_object *events)
{
    int i, len;
    char *fmt;
    json_object *event;

    len = json_object_array_length(events);

    if (len > 0) {
        for (i=0; i < len; i++) {
            event = json_object_array_get_idx(events, i);
            fmt = fmt_event(event);
            printf("%s\n", fmt);
            free(fmt);
        }
    }
    else {
        printf("There are no events at this time\n");
    }
}

void
list_events(char *path, int count)
{
    char *reply;
    json_object *jobj, *events;

    if (count < 0) {
        printf("Inavlid value for count argument\n");
        exit(1);
    }

    reply = get_events_raw(path, count);
    jobj = parse_json(reply);
    free(reply);

    events = json_object_object_get(jobj, "data");
    if (events == NULL) {
        printf("ERROR: Failed to find data for events in reply\n");
        exit(1);
    }
    else if ( json_object_is_type(events, json_type_array)) {
        print_events(events);
    }

}

void
list_queue_size (char *path)
{
    int queue_size;

    queue_size = get_queue_size(path);

    printf("%s  %d\n", path, queue_size);
}

void
watch (char *path)
{
    char *message;

    asprintf(&message, "{\"call\":\"watch\",\"path\":\"%s\"}", path);
    send_request(message);
    free(message);

    get_reply();
    printf("Root %s is now being watched\n", path);
}

void
unwatch (char *path)
{
    char *message;

    asprintf(&message, "{\"call\":\"unwatch\",\"path\":\"%s\"}", path);
    send_request(message);
    free(message);

    get_reply();
    printf("Root %s is no longer being watched\n", path);
}

void
list_roots (int queue)
{
    int i, len;
    char *reply, *path;
    json_object *jobj, *roots, *root;

    send_request("{\"call\":\"get_roots\"}");
    reply = get_reply();
    jobj = parse_json(reply);
    free(reply);

    roots = json_object_object_get(jobj, "data");
    if (roots == NULL) {
        printf("Failed to find data for roots in reply\n");
        exit(1);
    }
    else if ( json_object_is_type(roots, json_type_array)) {
        len = json_object_array_length(roots);
        if (len > 0) {
            for (i=0; i<len; i++) {
                root = json_object_array_get_idx(roots, i);
                path = (char *) json_object_get_string(root);
                if (queue)
                    printf("%s  %d\n", path, get_queue_size(path));
                else
                    printf("%s\n", path);
            }
        }
        else {
            printf("There are no currently watched roots\n");
        }
    }
    else {
        printf("Data received from server had invalid format\n");
        exit(1);
    }
}

int
main (int argc, char **argv)
{
    int connect_rv, dir_idx, cmd_idx, port;
    char *command, *zmq_uri;

    if (argc < 2) {
        printf("No command speficied. Run `inotispyctl --help` for more info.\n");
        exit(1);
    }

    if ( strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 )
        print_help();

    /* Check to see if the user passed in a different port. */
    if ( strcmp(argv[1], "-p") == 0 || strcmp(argv[1], "--port") == 0 ) {
        if (argc < 4)
            print_help();

        port    = atoi(argv[2]);
        cmd_idx = 3;
        dir_idx = 4;
    }
    else {
        port    = 5559;
        cmd_idx = 1;
        dir_idx = 2;
    }

    /* 0MQ connection */
    asprintf(&zmq_uri, "tcp://127.0.0.1:%d", port);

    context    = zmq_init(1);
    socket     = zmq_socket(context, ZMQ_REQ);
    connect_rv = zmq_connect(socket, zmq_uri);
    free(zmq_uri);

    if (connect_rv != 0) {
        printf("Failed to connect ZeroMQ socket: %s\n", zmq_strerror(errno));
        exit(1);
    }

    /* Dispatcher */
    command = argv[cmd_idx];
    if ( strcmp(command, "list_roots") == 0 ) {
        list_roots(0);
    }
    else if ( strcmp(command, "list_queues") == 0 ) {
        list_roots(1);
    }
    else if ( strcmp(command, "get_events") == 0 ) {
        if (argc != (dir_idx+2)) {
            printf("ERROR: Command get_events requires a target dir and a count\n");
            print_help();
        }
        list_events(argv[dir_idx], atoi(argv[dir_idx+1]));
    }
    else if ( strcmp(command, "queue_size") == 0 ) {
        if (argc != (dir_idx+1)) {
            printf("ERROR: Command queue_size requires a target dir\n");
            print_help();
        }
        list_queue_size(argv[dir_idx]);
    }
    else if ( strcmp(command, "flush_queue") == 0 ) {
        if (argc != (dir_idx+1)) {
            printf("ERROR: Command flush_queue requires a target dir\n");
            print_help();
        }
        flush_queue(argv[dir_idx]);
    }
    else if ( strcmp(command, "watch") == 0 ) {
        if (argc != (dir_idx+1)) {
            printf("ERROR: Command watch requires a target dir\n");
            print_help();
        }
        watch(argv[dir_idx]);
    }
    else if ( strcmp(command, "unwatch") == 0 ) {
        if (argc != (dir_idx+1)) {
            printf("ERROR: Command unwatch requires a target dir\n");
            print_help();
        }
        unwatch(argv[dir_idx]);
    }
    else {
        printf("ERROR: Unknown command: %s\n", command);
        print_help();
    }

    zmq_close(socket);
    zmq_term(context);

    return 0;
}

void
print_help (void)
{
    printf("\n");
    printf("Usage: inotispyctl [option] <command> [command args]\n");
    printf("\n");
    printf("Options:\n");
    printf(" -h, --help                  Print this help menu\n");
    printf(" -p, --port <num>            Use a port other than the default 5559\n");
    printf("\n");
    printf("Commands:\n");
    printf(" - list_roots                List each currently watched root.\n");
    printf(" - list_queues               List each currently watched root\n");
    printf("                             and it's current queue size.\n");
    printf(" - watch <dir>               Watch a new root at directory <dir>.\n");
    printf(" - unwatch <dir>             Unwatch a root with directory <dir>.\n");
    printf(" - queue_size <dir>          Get the queue size for a specific root.\n");
    printf(" - flush_queue <dir>         Flush the queue for a specific root.\n");
    printf(" - get_events <dir> <count>  Get events for a specific root.\n");
    printf("                             A count of 0 (zero) will retrieve *all*\n");
    printf("                             the events currently in that root's queue.\n");
    printf("\n");
/*
    printf("For more information please visit http://www.inotispy.org\n");
    printf("\n");
*/

    exit(1);
}
