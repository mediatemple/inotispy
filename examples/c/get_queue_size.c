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

int main(int argc, char **argv)
{
    int connect_rv, rv, msg_size;
    char *path, *message;
    void *context, *socket;
    zmq_msg_t request, reply;

    if (argc != 2) {
        printf("Usage: get_queue_size <PATH>\n");
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
    asprintf(&message, "{\"call\":\"get_queue_size\",\"path\":\"%s\"}",
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

    printf("Got reply: %s\n", message);

    free(message);
    zmq_close(socket);
    zmq_term(context);

    return 0;
}
