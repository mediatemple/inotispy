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

#define _LOG_FILE "/var/log/inotispy.log"

#define _LOG_LEVEL_ERROR   1
#define _LOG_LEVEL_WARN    2
#define _LOG_LEVEL_NOTICE  3
#define _LOG_LEVEL_DEBUG   4
#define _LOG_LEVEL_TRACE   5

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
void _LOG_ERROR(char *fmt, ...);
void _LOG_WARN(char *fmt, ...);
void _LOG_NOTICE(char *fmt, ...);
void _LOG_DEBUG(char *fmt, ...);
void _LOG_TRACE(char *fmt, ...);

#endif /*_INOTISPY_LOG_H_*/
