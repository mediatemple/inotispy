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

#include "log.h"
#include "config.h"

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

int log_level;
FILE *logger;

int init_logger()
{
    log_level = CONFIG->log_level;
    logger = fopen(CONFIG->log_file, "a");

    if (logger == NULL) {
	fprintf(stderr,
		"Failed to open file '%s': %s\n", CONFIG->log_file,
		strerror(errno));
	return 1;
    }

    return 0;
}

char *level_str(int level)
{
    switch (level) {
    case LOG_LEVEL_ERROR:
	return "ERROR";
    case LOG_LEVEL_WARN:
	return "WARN";
    case LOG_LEVEL_NOTICE:
	return "NOTICE";
    case LOG_LEVEL_DEBUG:
	return "DEBUG";
    case LOG_LEVEL_TRACE:
	return "TRACE";

    default:
	return "UNKNOWN";
    }
}

char *time_str()
{
    time_t t = time(NULL);
    char *t_str = ctime(&t);
    char *p;

    for (p = t_str; *p != '\n' && *p != '\0'; p++);
    *p = '\0';

    return t_str;
}

void log_msg(int level, char *fmt, va_list ap)
{
    if (level > log_level)
	return;

    char *msg;
    vasprintf(&msg, fmt, ap);

    fprintf(logger, "[%s] [%s] %s\n", time_str(), level_str(level), msg);

    fflush(logger);
    free(msg);
}

void LOG_ERROR(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(LOG_LEVEL_ERROR, fmt, ap);
    va_end(ap);
}

void LOG_WARN(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(LOG_LEVEL_WARN, fmt, ap);
    va_end(ap);
}

void LOG_NOTICE(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(LOG_LEVEL_NOTICE, fmt, ap);
    va_end(ap);
}

void LOG_DEBUG(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(LOG_LEVEL_DEBUG, fmt, ap);
    va_end(ap);
}

void LOG_TRACE(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(LOG_LEVEL_TRACE, fmt, ap);
    va_end(ap);
}

int get_log_level(void)
{
    return log_level;
}

void set_log_level(int level)
{
    if (level >= LOG_LEVEL_ERROR && level <= LOG_LEVEL_TRACE) {
	log_level = level;
    } else {
	char *err;
	asprintf(&err, "Log level %d is invalid", level);
	LOG_WARN(err);
	free(err);
    }
}
