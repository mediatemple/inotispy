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

#ifndef _INOTISPY_REQUEST_H_
#define _INOTISPY_REQUEST_H_

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <json/json.h>

typedef struct json_object  * JOBJ;
typedef struct json_tokener * JTOK;

#ifndef _INOTISPY_REQUEST_H_META_
#define _INOTISPY_REQUEST_H_META_
typedef struct request {
    char *call;
    char *json;
    JOBJ  parser;
} Request;
#endif /*_INOTISPY_REQUEST_H_META_*/

/* Take a printable string and attempt to parse
 * is as JSON.
 */
Request * request_parse(char *json);

/* Look up a key in the JSON hash and return it's
 * value, or NULL if it doesn't exist.
 */
int    request_get_key_int (Request *request, char *key);
char * request_get_key_str (Request *request, char *key);

/* Helper functions to serve mainly as syntatic sugar. */
int      request_get_count      (Request *req);
int      request_get_max_events (Request *req);
int      request_get_mask       (Request *req);
char *   request_get_call       (Request *req);
char *   request_get_path       (Request *req);

/* Turn the JSON object into a printable string. */
char * request_to_string (Request *req);

void request_free(Request *req);

#endif /*_INOTISPY_REQUEST_H_*/
