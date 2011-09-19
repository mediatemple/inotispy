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

#ifndef _INOTISPY_REPLY_H_ 
#define _INOTISPY_REPLY_H_ 

/* XXX CODE REVIEW
 *
 *     What is a better way to do error handling? Should a more detailed
 *     message bubble up and be passed to the client? Or is documenting
 *     errors and error codes good enough for coders to write clients
 *     that can handle errors elegantly?
 */

/* Supported error masks. */
#define ERROR_JSON_INVALID             0x0001 
#define ERROR_JSON_PARSE               0x0002
#define ERROR_JSON_KEY_NOT_FOUND       0x0004
#define ERROR_INOTIFY_WATCH_FAILED     0x0008
#define ERROR_INOTIFY_UNWATCH_FAILED   0x0010
#define ERROR_INVALID_EVENT_COUNT      0x0020
#define ERROR_ZERO_BYTE_MESSAGE        0x0040
#define ERROR_INOTIFY_ROOT_NOT_WATCHED 0x0080
#define ERROR_ZEROMQ_RECONNECT         0x0100

/* Actually send a 0MQ reply to the client.
 *
 * This function *DOES NOT* do any JSON formatting. It simply
 * takes the string '*message' you pass it and sends it back to
 * the client. The calling code is responsible for formatting
 * the message into JSON.
 */
int reply_send_message(char *message);

/* Wrapper functions for error and success. */
int reply_send_error (int err_code);
int reply_send_success (void);

#endif /*_INOTISPY_REPLY_H_*/
