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

#ifndef _INOTISPY_LOG_H_
#define _INOTISPY_LOG_H_

/* XXX CODE REVIEW
 *
 *     Originally I had planned on using log4c for logging, but after spending
 *     a couple hours with it I couldn't get it to work properly and it just
 *     felt super bloated. Both the config file approach and the API approach
 *     in log4c would have ended up needing as much, if not more, code than
 *     the home rolled solution I have here.
 *
 *     What I wrote is extremely simple, but that's all I'm looking for. If
 *     I missed the point with log4c, or there is another simpler C logger
 *     out there please advise.
 */

#define LOG_FILE "/var/log/inotispy.log"

#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_WARN    2
#define LOG_LEVEL_NOTICE  3
#define LOG_LEVEL_DEBUG   4
#define LOG_LEVEL_TRACE   5

/* Initialize and setup our logger */
int init_logger(void);

/* Get and set the current log level.
 *
 * Eventually Inotispy will periodically re-read it's configuration
 * file in real time so that behavior (like log levels) can change
 * without needing to restart the daemon.
 */
int get_log_level(void);
void set_log_level(int level);

/* Turn a integer log level into it's corresponding string value. */
char *level_str(int level);

/* XXX I had thought about trying to make these functions macros,
 *     but the pre-processor doesn't know how to handle the ...
 *     functionality, which is needed here.
 */
void LOG_ERROR(char *fmt, ...);
void LOG_WARN(char *fmt, ...);
void LOG_NOTICE(char *fmt, ...);
void LOG_DEBUG(char *fmt, ...);
void LOG_TRACE(char *fmt, ...);

#endif /*_INOTISPY_LOG_H_*/
