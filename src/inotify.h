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

#ifndef _INOTISPY_INOTIFY_H_
#define _INOTISPY_INOTIFY_H_

#include <stdint.h>
#include <glib/ghash.h>
#include <glib/gqueue.h>
#include <sys/inotify.h>

#define INOTIFY_ROOT_DUMP_DIR  "/var/run/inotispy"
#define INOTIFY_ROOT_DUMP_FILE "/var/run/inotispy/roots.dump"
#define INOTIFY_EVENT_SIZE     ( sizeof (struct inotify_event) )
#define INOTIFY_EVENT_BUF_LEN  ( 1024 * ( INOTIFY_EVENT_SIZE + 16 ) )
#define INOTIFY_MAX_EVENTS     65536    /* This number is arbatrary */
#define INOTIFY_DEFAULT_MASK   ( \
        IN_ATTRIB              | \
        IN_MOVED_FROM          | \
        IN_MOVED_TO            | \
        IN_CREATE              | \
        IN_CLOSE_WRITE         | \
        IN_DELETE              | \
        IN_UNMOUNT             | \
        IN_DONT_FOLLOW           \
    )

#ifndef _INOTISPY_INOTIFY_H_META_
#define _INOTISPY_INOTIFY_H_META_

typedef struct inotify_event IN_Event;

/* Meta data for the root of each watched tree. */
typedef struct inotify_root {
    char *path;
    uint32_t mask;
    int max_events;
    GQueue *queue;
    int destroy;
    int pause;
    int rewatch;
    int persist;                /* Future feature */
} Root;

typedef struct inotify_watch {
    int wd;
    char *path;
} Watch;

/* Event queue node. This is identical to the inotify_event
 * struct (SEE: man inotify) plus one more field for the
 * path of the event. The inotify_event struct is:
 *
 *   struct inotify_event {
 *       int      wd;     
 *       uint32_t mask;   
 *       uint32_t cookie; 
 *       uint32_t len;    
 *       char     name[]; 
 *   };
 */
typedef struct inotify_queue_node {
    int wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
    char *path;
    char *name;
} Event;

/* Global inotify file descriptor. This daemon will
 * only ever need one of these.
 */
int inotify_fd;

/* Running tally of watched roots. */
int inotify_num_watched_roots;

/* Global hashs for mapping the inotify related meta data.
 * 
 * - inotify_roots
 * 
 *   For each root path watched we need to store some basic
 *   information. Meta data that applies to the entire tree
 *   starting at a given root will stored in this hash.
 * 
 * - inotify_wd_to_watch
 * 
 *   The data in this hash is used to construt absolute paths
 *   for the events that occur.
 * 
 *   Inotify unfortunatly leaves it up to you to do anything
 *   smart with the notifications you get back. When you set
 *   up an inotify_watch on a given directory it will give you
 *   back a "watch descriptor" which is an integer value. And
 *   when you receive an event from a file in that watched
 *   directory you will get back the name of the file and the
 *   "wd" integer value. Inotify DOES NOT give you the absolute
 *   path to the event; it's up to you to re-construct that.
 * 
 *   So, let's say we watch directory /a/b/c and get back wd=1.
 *   If someone creates file '/a/b/c/foo.txt' the event we get
 *   back will look something like:
 * 
 *     {
 *        wd = 1;
 *        name = "foo.txt";
 *     }
 * 
 *   To get the full path we look up key '1' in inotify_wd_to_watch
 *   to get the value 'a/b/c', then we concatinate it with the
 *   name 'foo.txt' and bam we have the absolute path for the event.
 * 
 *   XXX: This does not scale well. If you watch a giant tree
 *        then lots of memory will be used up. This is a sad, but
 *        very real, reality.
 * 
 * - inotify_path_to_watch
 * 
 *   On the flip side if we want to UN-watch a directory or
 *   directory tree we're going to have to know the integer watch
 *   descriptor value for that directory because that's what the
 *   funtion inotify_rm_watch() requires as an argument. This
 *   hash let's us take an absolute path and look up it's watch
 *   descriptor.
 */
GHashTable *inotify_roots;
GHashTable *inotify_wd_to_watch;
GHashTable *inotify_path_to_watch;

#endif /*_INOTIOFY_H_META_*/

/* Initialize */
int inotify_setup(void);

/* Verify is a path is a currently watched root */
Root *inotify_is_root(const char *path);

/* Event handler for new inotify alerts. */
void inotify_handle_event(void);

/* Recursively watch a directory tree */
int inotify_watch_tree(char *path, int mask, int max_events, int rewatch);

/* Recursively UN-watch a directory tree. */
int inotify_unwatch_tree(char *path);

/* Pause and unpause a tree from queuing inotify events. */
int inotify_pause_tree(char *path);
int inotify_unpause_tree(char *path);

/* Get (and free) a list of all the currently watched roots. */
char **inotify_get_roots(void);
void inotify_free_roots(char **roots);

/* Free up an event buffer. */
void inotify_free_events(Event ** events);

/* Functions for retrieving queued events */
Event **inotify_get_event(const char *path);
Event **inotify_get_events(const char *path, int count);

/* Dump the currently watched roots to a file so that the
 * server can re-watch them automatically on start up.
 */
void inotify_dump_roots(void);

/* Get the total number of current inotify watches. */
int inotify_num_watched_dirs(void);

#endif /*_INOTISPY_INOTIFY_H_*/
