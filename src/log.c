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
