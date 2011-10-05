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

#ifndef _INOTISPY_REPLY_H_
#define _INOTISPY_REPLY_H_

#ifndef _INOTISPY_REPLY_ERRORS_
#define _INOTISPY_REPLY_ERRORS_

/* Supported error masks. */
#define ERROR_JSON_INVALID                 0x0001
#define ERROR_JSON_PARSE                   0x0002
#define ERROR_JSON_KEY_NOT_FOUND           0x0004
#define ERROR_INOTIFY_WATCH_FAILED         0x0008
#define ERROR_INOTIFY_UNWATCH_FAILED       0x0010
#define ERROR_INVALID_EVENT_COUNT          0x0020
#define ERROR_ZERO_BYTE_MESSAGE            0x0040
#define ERROR_INOTIFY_ROOT_NOT_WATCHED     0x0080
#define ERROR_INOTIFY_ROOT_ALREADY_WATCHED 0x0100
#define ERROR_ZEROMQ_RECONNECT             0x0200
#define ERROR_NOT_ABSOLUTE_PATH            0x0400
#define ERROR_INOTIFY_PARENT_OF_ROOT       0x0800
#define ERROR_INOTIFY_ROOT_DOES_NOT_EXIST  0x1000
#define ERROR_FAILED_TO_CREATE_NEW_THREAD  0x2000
#define ERROR_MEMORY_ALLOCATION            0x4000

#endif /*_INOTISPY_REPLY_ERRORS_*/

/* Send a 0MQ reply to the client.
 *
 * This function *DOES NOT* do any JSON formatting. It simply
 * takes the string *message you pass it and sends it back to
 * the client. The calling code is responsible for formatting
 * the message into JSON.
 */
int reply_send_message(char *message);

/* Wrapper functions for error and success. */
int reply_send_error(unsigned int error_code);
int reply_send_success(void);

/* Stringify an error code. */
char *error_to_string(unsigned int err_code);

#endif /*_INOTISPY_REPLY_H_*/
