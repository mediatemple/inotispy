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
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>             /* usleep() */
#include <string.h>
#include <stdarg.h>
#include <json/json.h>

#define ZMQ_URI "tcp://127.0.0.1:5559"

void *socket, *context;

static void print_help(void);
static void print_version(void);

int mk_string(char **ret, const char *fmt, ...)
{
    int count, len;
    va_list ap;
    char *buf;

    *ret = NULL;

    va_start(ap, fmt);
    len = count = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (count >= 0) {

        if ((buf = malloc(count + 1)) == NULL)
            return 1;

        va_start(ap, fmt);
        count = vsnprintf(buf, count + 1, fmt, ap);
        va_end(ap);

        if (count < 0) {
            free(buf);
            return count;
        }
        buf[len] = '\0';
        *ret = buf;
    }

    return count;
}

json_object *parse_json(char *json)
{
    struct json_object *jobj;
    struct json_tokener *jtok;

    jtok = json_tokener_new();
    jobj = json_tokener_parse_ex(jtok, json, -1);

    if (jtok->err != json_tokener_success) {
        json_tokener_free(jtok);
        printf("Failed to parse JSON: %s\n", json);
        exit(1);
    }

    json_tokener_free(jtok);

    return jobj;
}

void handle_error(char *json)
{
    json_object *jobj, *error, *code, *message;

    jobj = parse_json(json);

    error = json_object_object_get(jobj, "error");
    if (error == NULL)
        return;

    code = json_object_object_get(error, "code");
    message = json_object_object_get(error, "message");

    printf("ERROR: %d: %s\n", json_object_get_int(code),
           json_object_get_string(message));

    exit(1);
}

void send_request(char *message, int flag)
{
    int rv;
    zmq_msg_t request;

    zmq_msg_init_size(&request, strlen(message));
    strncpy(zmq_msg_data(&request), message, strlen(message));

    rv = zmq_send(socket, &request, flag);
    zmq_msg_close(&request);
    if (rv != 0) {
        printf("Failed to send message to server: %s\n",
               zmq_strerror(errno));
        exit(1);
    }
}

char *get_reply(void)
{
    int rv, msg_size;
    char *message;
    zmq_msg_t reply;

    zmq_msg_init(&reply);
    rv = zmq_recv(socket, &reply, 0);
    if (rv != 0) {
        printf("Failed to receive message from server: %s\n",
               zmq_strerror(errno));
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

int get_queue_size(char *path)
{
    int rv;
    char *request, *reply;
    json_object *jobj, *queue_size;

    rv = mk_string(&request,
                   "{\"call\":\"get_queue_size\",\"path\":\"%s\"}", path);

    if (rv == -1) {
        printf("Failed to allocate memory for JSON request in function %s",
               "get_queue_size()");
        exit(1);
    }

    send_request(request, 0);
    free(request);

    reply = get_reply();
    jobj = parse_json(reply);
    free(reply);

    queue_size = json_object_object_get(jobj, "data");
    if (queue_size == NULL) {
        printf("Failed to find data for queue_size in reply\n");
        exit(1);
    } else if (json_object_is_type(queue_size, json_type_int)) {
        rv = json_object_get_int(queue_size);
    } else {
        printf("Data received from server had invalid format\n");
        exit(1);
    }

    return rv;
}

int is_unsigned_int(char *str)
{
    int i, len;

    len = strlen(str);

    if (str[0] == '-')
        return 0;

    for (i = 0; i < len; i++) {
        if (!isdigit(str[i]))
            return 0;
    }

    return 1;
}



char *get_events_raw(char *path, int count)
{
    int rv;
    char *request;

    rv = mk_string(&request,
                   "{\"call\":\"get_events\",\"path\":\"%s\",\"count\":%d}",
                   path, count);

    if (rv == -1) {
        printf("Failed to allocate memory for JSON request in function %s",
               "get_events_raw()");
        exit(1);
    }

    send_request(request, 0);
    free(request);

    return get_reply();
}

void flush_queue(char *path)
{
    get_events_raw(path, 0);
    printf("Queue for root %s has been flushed\n", path);
}

char *fmt_event(json_object * event)
{
    int rv;
    json_object *name, *path, *mask;
    char *fmt;

    mask = json_object_object_get(event, "mask");
    name = json_object_object_get(event, "name");
    path = json_object_object_get(event, "path");

    rv = mk_string(&fmt, "%s/%s  %d", json_object_get_string(path),
                   json_object_get_string(name),
                   json_object_get_int(mask));

    if (rv == -1) {
        printf
            ("Failed to allocate memory for formatted event in function %s",
             "fmt_event()");
        exit(1);
    }

    return fmt;
}


void print_events(json_object * events)
{
    int i, len;
    char *fmt;
    json_object *event;

    len = json_object_array_length(events);

    if (len > 0) {
        for (i = 0; i < len; i++) {
            event = json_object_array_get_idx(events, i);
            fmt = fmt_event(event);
            printf("%s\n", fmt);
            free(fmt);
        }
    } else {
        printf("There are no events at this time\n");
    }
}

void list_events(char *path, char *count)
{
    char *reply;
    json_object *jobj, *events;

    if (!is_unsigned_int(count)) {
        printf
            ("ERROR: Invalid value for 'count' while calling get_events.\n");
        printf
            ("       This should be a number >= 0, where 0 means you want\n");
        printf("       to retrieve *all* the events in the queue.\n");
        printf
            ("       Run `inotispyctl --help` or `man inotispyctl` for more info.\n");
        exit(1);
    }

    reply = get_events_raw(path, atoi(count));
    jobj = parse_json(reply);
    free(reply);

    events = json_object_object_get(jobj, "data");
    if (events == NULL) {
        printf("ERROR: Failed to find data for events in reply\n");
        exit(1);
    } else if (json_object_is_type(events, json_type_array)) {
        print_events(events);
    }

}

void list_queue_size(char *path)
{
    int queue_size;

    queue_size = get_queue_size(path);

    printf("%s  %d\n", path, queue_size);
}

void watch(char *path)
{
    int rv;
    char *message;

    rv = mk_string(&message, "{\"call\":\"watch\",\"path\":\"%s\"}", path);

    if (rv == -1) {
        printf("Failed to allocate memory for JSON request in function %s",
               "watch()");
        exit(1);
    }

    send_request(message, 0);
    free(message);

    get_reply();
    printf("Root %s is now being watched\n", path);
}

void unwatch(char *path)
{
    int rv;
    char *message;

    rv = mk_string(&message, "{\"call\":\"unwatch\",\"path\":\"%s\"}",
                   path);

    if (rv == -1) {
        printf("Failed to allocate memory for JSON request in function %s",
               "unwatch()");
        exit(1);
    }

    send_request(message, 0);
    free(message);

    get_reply();
    printf("Root %s is no longer being watched\n", path);
}

void pause_root(char *path)
{
    int rv;
    char *message;

    rv = mk_string(&message, "{\"call\":\"pause\",\"path\":\"%s\"}", path);

    if (rv == -1) {
        printf("Failed to allocate memory for JSON request in function %s",
               "pause()");
        exit(1);
    }

    send_request(message, 0);
    free(message);

    get_reply();
    printf("Root %s is now paused\n", path);
}

void unpause_root(char *path)
{
    int rv;
    char *message;

    rv = mk_string(&message, "{\"call\":\"unpause\",\"path\":\"%s\"}",
                   path);

    if (rv == -1) {
        printf("Failed to allocate memory for JSON request in function %s",
               "unpause()");
        exit(1);
    }

    send_request(message, 0);
    free(message);

    get_reply();
    printf("Root %s is now unpaused\n", path);
}

void list_roots(int queue)
{
    int i, len;
    char *reply, *path;
    json_object *jobj, *roots, *root;

    send_request("{\"call\":\"get_roots\"}", 0);
    reply = get_reply();
    jobj = parse_json(reply);
    free(reply);

    roots = json_object_object_get(jobj, "data");
    if (roots == NULL) {
        printf("Failed to find data for roots in reply\n");
        exit(1);
    } else if (json_object_is_type(roots, json_type_array)) {
        len = json_object_array_length(roots);
        if (len > 0) {
            for (i = 0; i < len; i++) {
                root = json_object_array_get_idx(roots, i);
                path = (char *) json_object_get_string(root);
                if (queue)
                    printf("%s  %d\n", path, get_queue_size(path));
                else
                    printf("%s\n", path);
            }
        } else {
            printf("There are no currently watched roots\n");
        }
    } else {
        printf("Data received from server had invalid format\n");
        exit(1);
    }
}

void zmq_ping(void)
{
    int rv, i, pong, size;
    zmq_msg_t reply;
    zmq_msg_init(&reply);

    printf("ping... ");
    send_request("{\"call\":\"ping\"}", 0);

    for (i = 0, pong = 0; i < 10; i++) {
        rv = zmq_recv(socket, &reply, ZMQ_NOBLOCK);
        size = zmq_msg_size(&reply);

        if (size) {
            pong = 1;
            break;
        }
        usleep(100000);         /* Sleep for 1/10th of a second. */
    }

    zmq_msg_close(&reply);

    if (pong) {
        printf("pong! Inotispy is up and running.\n");
    } else {
        printf("nothing. Check to make sure Inotispy is running.\n");
        exit(1);
    }
}

int main(int argc, char **argv)
{
    int c, rv, connect_rv, dir_idx;
    int option_index;
    char *command, *zmq_uri;

    setbuf(stdout, NULL);

    static struct option long_opts[] = {
        {"help", no_argument, 0, 'h'},
        {"zmq_uri", required_argument, 0, 'u'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    if (argc < 2) {
        printf
            ("No command speficied. Run `inotispyctl --help` for more info.\n");
        exit(1);
    }

    option_index = 0;
    zmq_uri = NULL;

    while ((c =
            getopt_long(argc, argv, "hu:", long_opts,
                        &option_index)) != -1) {
        switch (c) {
        case 'v':
            print_version();
            break;
        case 'h':
            print_help();
            break;
        case 'u':
            zmq_uri = optarg;
            break;
        case '?':
            if (optopt == 'u') {
                print_help();
            }
            return 1;
        default:
            abort();
        }
    }

    if (argv[optind] == NULL) {
        printf
            ("No command speficied. Run `inotispyctl --help` for more info.\n");
        exit(1);
    }

    if (zmq_uri == NULL)
        zmq_uri = ZMQ_URI;

    /* 0MQ connection */
    context = zmq_init(1);
    socket = zmq_socket(context, ZMQ_REQ);
    connect_rv = zmq_connect(socket, zmq_uri);
    // free(zmq_uri);

    if (connect_rv != 0) {
        printf("Failed to connect ZeroMQ socket: %s\n",
               zmq_strerror(errno));
        exit(1);
    }

    /* Dispatcher */
    command = argv[optind];
    dir_idx = optind + 1;
    if (strcmp(command, "ping") == 0) {
        zmq_ping();
    } else if (strcmp(command, "list_roots") == 0) {
        list_roots(0);
    } else if (strcmp(command, "list_queues") == 0) {
        list_roots(1);
    } else if (strcmp(command, "get_events") == 0) {
        if (argv[dir_idx] == NULL || argv[dir_idx + 1] == NULL) {
            printf
                ("ERROR: Command get_events requires a target dir and a count\n");
            print_help();
        }
        list_events(argv[dir_idx], argv[dir_idx + 1]);
    } else if (strcmp(command, "queue_size") == 0) {
        if (argv[dir_idx] == NULL) {
            printf("ERROR: Command queue_size requires a target dir\n");
            print_help();
        }
        list_queue_size(argv[dir_idx]);
    } else if (strcmp(command, "flush_queue") == 0) {
        if (argv[dir_idx] == NULL) {
            printf("ERROR: Command flush_queue requires a target dir\n");
            print_help();
        }
        flush_queue(argv[dir_idx]);
    } else if (strcmp(command, "watch") == 0) {
        if (argv[dir_idx] == NULL) {
            printf("ERROR: Command watch requires a target dir\n");
            print_help();
        }
        watch(argv[dir_idx]);
    } else if (strcmp(command, "unwatch") == 0) {
        if (argv[dir_idx] == NULL) {
            printf("ERROR: Command unwatch requires a target dir\n");
            print_help();
        }
        unwatch(argv[dir_idx]);
    } else if (strcmp(command, "pause") == 0) {
        if (argv[dir_idx] == NULL) {
            printf("ERROR: Command pause requires a target dir\n");
            print_help();
        }
        pause_root(argv[dir_idx]);
    } else if (strcmp(command, "unpause") == 0) {
        if (argv[dir_idx] == NULL) {
            printf("ERROR: Command unpause requires a target dir\n");
            print_help();
        }
        unpause_root(argv[dir_idx]);
    } else {
        printf("ERROR: Unknown command: %s\n", command);
        print_help();
    }

    zmq_close(socket);
    zmq_term(context);

    return 0;
}

static void print_version(void)
{
    printf("inotispyctl v%s (c) 2012 (mt) MediaTemple\n", INOTISPY_VERSION);
    exit(0);
}

static void print_help(void)
{
    printf("\n");
    printf("Usage: inotispyctl [option] <command> [command args]\n");
    printf("\n");
    printf("Options:\n");
    printf(" -h, --help                  Print this help menu\n");
    printf
        (" -u, --zmq_uri <uri>         Use a zmq_uri othar than the default,\n");
    printf("                             which is tcp://127.0.0.1:5559\n");
    printf("\n");
    printf("Commands:\n");
    printf
        (" - list_roots                List each currently watched root.\n");
    printf
        (" - list_queues               List each currently watched root\n");
    printf("                             and it's current queue size.\n");
    printf
        (" - watch <dir>               Watch a new root at directory <dir>.\n");
    printf
        (" - unwatch <dir>             Unwatch a root with directory <dir>.\n");
    printf
        (" - pause <dir>               Pause a root from queuing events.\n");
    printf
        (" - unpause <dir>             Unpause a root so it resumes queuing events.\n");
    printf
        (" - queue_size <dir>          Get the queue size for a specific root.\n");
    printf
        (" - flush_queue <dir>         Flush the queue for a specific root.\n");
    printf
        (" - get_events <dir> <count>  Get events for a specific root.\n");
    printf
        ("                             A count of 0 (zero) will retrieve *all*\n");
    printf
        ("                             the events currently in that root's queue.\n");
    printf("\n");
/*
    printf("For more information please visit http://www.inotispy.org\n");
    printf("\n");
*/

    exit(1);
}
