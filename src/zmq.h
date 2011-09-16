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

#ifndef _INOTISPY_ZMQ_H_
#define _INOTISPY_ZMQ_H_

#include "request.h"

#include <zmq.h>

#ifndef __INOTISPY_ZMQ_H_META__
#define __INOTISPY_ZMQ_H_META__

#define ZMQ_ADDR        "tcp://*:5555"
#define ZMQ_THREADS     16
#define ZMQ_MAX_MSG_LEN 1024

/* 0MQ socket listener. */
void *zmq_listener;

#endif /*__INOTISPY_ZMQ_H_META__*/

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
void * zmq_setup (void);

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
void zmq_handle_event (void *receiver);

/* Given a successfully parsed request, take the "call" value and
 * invoke the appropriate EVENT_* funcion to handle that request.
 */
void zmq_dispatch_event (Request *req);

#endif /*_INOTISPY_ZMQ_H_*/
