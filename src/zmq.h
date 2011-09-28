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

#ifndef _INOTISPY_ZMQ_H_
#define _INOTISPY_ZMQ_H_

#include "request.h"

#include <zmq.h>

#ifndef _INOTISPY_ZMQ_H_META_
#define _INOTISPY_ZMQ_H_META_

#define ZMQ_ADDR        "tcp://*"
#define ZMQ_PORT        5559
#define ZMQ_THREADS     16
#define ZMQ_MAX_MSG_LEN 1024

/* 0MQ socket listener. */
void *zmq_listener;

#endif /*_INOTISPY_ZMQ_H_META_*/

/* Initialization. Set up our 0MQ file descriptor and our ZMQ_*
 * socket listeners.
 *
 * XXX: At the time of this writing this daemon only supports
 *      the REQ/REP request/reply 0MQ pattern. In the future
 *      it also might (should) support the PUB/SUB publisher/
 *      subscriber pattern so that clients don't have to
 *      implement their own polling mechanism for retreiving
 *      inotify events.
 */
void *zmq_setup(void);

/* When a 0MQ event comes in over the wire this is the function
 * that will get invoked. This function serves mainly as a
 * sanity checker and forwarder. What it does is:
 *
 * 1. Grab the message off the wire
 * 2. Dirty check to see if the message looks like JSON. (Bail if not)
 * 3. Parse JSON and see if it has a "call" field. (Bail if not)
 * 4. If JSON parsed successfully and there was a "call" field
 *    create a Request struct/blob and send that off to the dispatcher.
 */
void zmq_handle_event(void *receiver);

/* Given a successfully parsed request, take the "call" value and
 * invoke the appropriate EVENT_* funcion to handle that request.
 */
void zmq_dispatch_event(Request * req);

#endif /*_INOTISPY_ZMQ_H_*/
