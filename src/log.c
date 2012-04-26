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
#include "utils.h"

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>

static int log_level;
static FILE *logger;

/* Map an Inotispy log level to a syslog priority.
 *
 * The only difference is in the bottom two:
 *
 *       Inotispy     |  syslog
 *   -----------------+-----------
 *   LOG_LEVEL_DEBUG | LOG_INFO
 *   LOG_LEVEL_TRACE | LOG_DEBUG
 */
static int level_to_syslog_priority(int level)
{
    switch (level) {
    case LOG_LEVEL_ERROR:
        return LOG_ERR;
    case LOG_LEVEL_WARN:
        return LOG_WARNING;
    case LOG_LEVEL_NOTICE:
        return LOG_NOTICE;
    case LOG_LEVEL_DEBUG:
        return LOG_INFO;
    case LOG_LEVEL_TRACE:
        return LOG_DEBUG;

    default:
        return LOG_NOTICE;
    }
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

static char *time_str(void)
{
    int len;
    time_t t;
    char *t_str;

    t = time(NULL);
    t_str = ctime(&t);
    len = strlen(t_str) - 1;

    /* Remove potential newline */
    if (t_str[len] == '\n')
        t_str[len] = 0;

    return t_str;
}

static void log_msg(int level, const char *fmt, va_list ap)
{
    int rv;
    char *msg;

    if (level > log_level)
        return;

    rv = vasprintf(&msg, fmt, ap);
    if (rv == -1) {
        level = LOG_LEVEL_ERROR;
        msg = "Failed to allocate memory for log message: log.c:log_msg()";
    }

    if (CONFIG->logging_enabled) {
        fprintf(logger, "[%s] [%s] %s\n", time_str(), level_str(level),
                msg);
        fflush(logger);
    }

    if (CONFIG->log_syslog) {
        syslog(level_to_syslog_priority(CONFIG->log_level), msg);
    }

    free(msg);
}

void log_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(LOG_LEVEL_ERROR, fmt, ap);
    va_end(ap);
}

void log_warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(LOG_LEVEL_WARN, fmt, ap);
    va_end(ap);
}

void log_notice(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(LOG_LEVEL_NOTICE, fmt, ap);
    va_end(ap);
}

void log_debug(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg(LOG_LEVEL_DEBUG, fmt, ap);
    va_end(ap);
}

void log_trace(const char *fmt, ...)
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
    int rv;
    char *err;

    if (level >= LOG_LEVEL_ERROR && level <= LOG_LEVEL_TRACE) {
        log_level = level;
    } else {
        rv = mk_string(&err, "Log level %d is invalid", level);
        if (rv == -1)
            err =
                "Failed to allocate memory for log message: log.c:set_log_level()";
        log_warn(err);
        free(err);
    }
}

int init_logger(void)
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

void close_logger(void)
{
    sync();
    fclose(logger);
}
