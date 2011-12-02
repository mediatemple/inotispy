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

#ifndef _INOTISPY_REQUEST_H_
#define _INOTISPY_REQUEST_H_

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <json/json.h>

typedef struct json_object *JOBJ;
typedef struct json_tokener *JTOK;

#ifndef _INOTISPY_REQUEST_H_META_
#define _INOTISPY_REQUEST_H_META_
typedef struct request {
    char *call;
    char *json;
    JOBJ parser;
} Request;
#endif /*_INOTISPY_REQUEST_H_META_*/

/* Take a printable string and attempt to parse
 * is as JSON.
 */
Request *request_parse(const char *json);

/* Look up a key in the JSON hash and return it's
 * value, or NULL if it doesn't exist.
 */
int request_get_key_int(const Request * request, const char *key);
char *request_get_key_str(const Request * request, const char *key);

/* Helper functions to serve mainly as syntatic sugar. */
int request_get_count(const Request * req);
int request_get_max_events(const Request * req);
int request_get_mask(const Request * req);
int request_get_persist(const Request * req);
char *request_get_call(const Request * req);
char *request_get_path(const Request * req);
int request_is_verbose(const Request * req);

/* Turn the JSON object into a printable string. */
const char *request_to_string(const Request * req);

void request_free(Request * req);

#endif /*_INOTISPY_REQUEST_H_*/
