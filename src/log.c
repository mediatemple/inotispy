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
#include <syslog.h>

int log_level;
FILE *logger;

int level_to_syslog_priority(int level);

int init_logger()
{
    log_level = CONFIG->log_level;

    /* Private logger */
    logger = fopen(CONFIG->log_file, "a");
    if (logger == NULL) {
        fprintf(stderr,
                "Failed to open file '%s': %s\n", CONFIG->log_file,
                strerror(errno));
        return 1;
    }

    /* syslog */
    if (CONFIG->log_syslog) {
        setlogmask(LOG_UPTO(level_to_syslog_priority(CONFIG->log_level)));
        openlog(APPLICATION_NAME, LOG_PID | LOG_CONS | LOG_NDELAY,
                LOG_DAEMON);
    }

    return 0;
}

/* Map an Inotispy log level to a syslog priority.
 *
 * The only difference is in the bottom two:
 *
 *       Inotispy     |  syslog
 *   -----------------+-----------
 *   _LOG_LEVEL_DEBUG | LOG_INFO
 *   _LOG_LEVEL_TRACE | LOG_DEBUG
 */
int level_to_syslog_priority(int level)
{
    switch (level) {
    case _LOG_LEVEL_ERROR:
        return LOG_ERR;
    case _LOG_LEVEL_WARN:
        return LOG_WARNING;
    case _LOG_LEVEL_NOTICE:
        return LOG_NOTICE;
    case _LOG_LEVEL_DEBUG:
        return LOG_INFO;
    case _LOG_LEVEL_TRACE:
        return LOG_DEBUG;

    default:
        return LOG_NOTICE;
    }
}

char *level_str(int level)
{
    switch (level) {
    case _LOG_LEVEL_ERROR:
        return "ERROR";
    case _LOG_LEVEL_WARN:
        return "WARN";
    case _LOG_LEVEL_NOTICE:
        return "NOTICE";
    case _LOG_LEVEL_DEBUG:
        return "DEBUG";
    case _LOG_LEVEL_TRACE:
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

    if (CONFIG->log_syslog) {
        syslog(level_to_syslog_priority(CONFIG->log_level), msg);
    }

    free(msg);
}

void _LOG_ERROR(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(_LOG_LEVEL_ERROR, fmt, ap);
    va_end(ap);
}

void _LOG_WARN(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(_LOG_LEVEL_WARN, fmt, ap);
    va_end(ap);
}

void _LOG_NOTICE(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(_LOG_LEVEL_NOTICE, fmt, ap);
    va_end(ap);
}

void _LOG_DEBUG(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(_LOG_LEVEL_DEBUG, fmt, ap);
    va_end(ap);
}

void _LOG_TRACE(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(_LOG_LEVEL_TRACE, fmt, ap);
    va_end(ap);
}

int get_log_level(void)
{
    return log_level;
}

void set_log_level(int level)
{
    if (level >= _LOG_LEVEL_ERROR && level <= _LOG_LEVEL_TRACE) {
        log_level = level;
    } else {
        char *err;
        asprintf(&err, "Log level %d is invalid", level);
        _LOG_WARN(err);
        free(err);
    }
}
