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

#include "reply.h"
#include "inotify.h"
#include "utils.h"

#include <glib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>              /* isalnum() */
#include <dirent.h>
#include <unistd.h>             /* read(), usleep() */
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib/ghash.h>

static pthread_mutex_t inotify_mutex = PTHREAD_MUTEX_INITIALIZER;
static int IN_MEMCLEAN = 0;
static int IN_ROOT_REWATCH = 0;
static int NUM_ROOT_REWATCH = 0;

/* When you create a new thread using pthreads you give it
 * a reference to a subroutine and it envokes that subroutine.
 * Unlike other subroutines where you can choose how many
 * arguments you'd like to pass in, a newly threaded subroutine
 * can only take a single argument.
 *
 * The way to get more than one piece of data to your new thread
 * is to create a struct with all the data, and then pass in a
 * single pointer to that struct.
 *
 * The following typedef is that struct.
 */
typedef struct thread_data {
    char *path;
    Root *root;
    int cleanup;
} T_Data;

/* Prototypes for private functions. */
static Root *inotify_path_to_root(const char *path);
static Root *make_root(const char *path, int mask, int max_events,
                       int rewatch);
static Watch *make_watch(int wd, const char *path);
static char *inotify_is_parent(const char *path);
static int inotify_enqueue(const Root * root, const IN_Event * event,
                           const char *path);
static void free_node_mem(Event * node, gpointer user_data);

static int do_watch_tree(const char *path, Root * root, int cleanup);
static void *_do_watch_tree(void *thread_data);
static void _do_watch_tree_rec(char *path, Root * root, int cleanup);
static void *_destroy_root(void *thread_data);
static void *_inotify_memclean(void *thread_data);

/* Initialize inotify file descriptor and set up meta data hashes.
 *
 * On success the inotify file descriptor is returned.
 * On failure 0 (zero) is returned.
 */
int inotify_setup(void)
{
    int rv;

    inotify_num_watched_roots = 0;

    inotify_fd = inotify_init();

    if (inotify_fd < 0) {
        log_error("Inotify failed to init: %s", strerror(errno));
        return 0;
    }

    inotify_wd_to_watch =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

    if (inotify_wd_to_watch == NULL) {
        log_error("Failed to init GHashTable inotify_wd_to_watch");
        return 0;
    }

    inotify_path_to_watch =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    if (inotify_path_to_watch == NULL) {
        log_error("Failed to init GHashTable inotify_path_to_watch");
        return 0;
    }

    inotify_roots =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    if (inotify_roots == NULL) {
        log_error("Failed to init GHashTable inotify_roots");
        return 0;
    }

    DIR *d = opendir(INOTIFY_ROOT_DUMP_DIR);
    if (d == NULL) {
        closedir(d);
        log_debug("Root dump directory does not exist. Creating it...");
        rv = mkdir(INOTIFY_ROOT_DUMP_DIR, 0644);
        if ((rv == -1) && (errno != EEXIST)) {
            log_error
                ("Failed to create root dump directory: %d: %s",
                 errno, strerror(errno));
        }
    } else {
        closedir(d);
        log_debug("Reading root dump file and re-watching roots");

        FILE *dump = fopen(INOTIFY_ROOT_DUMP_FILE, "r");
        if (dump == NULL) {
            log_warn("Failed to open presistant root dump file '%s': %s",
                     INOTIFY_ROOT_DUMP_FILE, strerror(errno));
        } else {
            int mask, max_events;
            char *path;
            char line[1024];
            char delim[] = ",";

            while (fgets(line, sizeof line, dump) != NULL) {
                if (line[strlen(line) - 1] == '\n')
                    line[strlen(line) - 1] = '\0';

                path = strtok(line, delim);
                mask = atoi(strtok(NULL, delim));
                max_events = atoi(strtok(NULL, delim));

                if (!(path && mask && max_events)) {
                    log_error("Invalid entry in dump file '%s': %s",
                              INOTIFY_ROOT_DUMP_FILE,
                              "entry must contain a path, mask, and max_events");
                    continue;
                }

                log_notice("Rewatching tree at root '%s'", path);
                inotify_watch_tree(path, mask, max_events, 1);
            }
        }
    }

    return inotify_fd;
}

int is_a_dir(char *dir)
{
    struct stat st;

    if (stat(dir, &st) == 0)
        return S_ISDIR(st.st_mode);

    return 0;
}

int inotify_num_watched_dirs(void)
{
    return (int) g_hash_table_size(inotify_path_to_watch);
}

void inotify_handle_event(void)
{
    int i = 0, rv, num_in_events;
    char buffer[INOTIFY_EVENT_BUF_LEN];
    char *path, *abs_path;
    Root *root;
    IN_Event *event;

    event = NULL;

    sync();

    /* Grab the next buffer of inotoify events. If the
     * length of this buffer is negative then we've
     * encountered a read error.
     */
    num_in_events = read(inotify_fd, buffer, INOTIFY_EVENT_BUF_LEN);

    if (num_in_events < 0) {
        log_error("Inotify read error: %s", strerror(errno));
        return;
    }

    /* Loop through, read, and act on the returned
     * list of inotify events.
     */
    while (i < num_in_events) {

        event = (struct inotify_event *) &buffer[i];

        /* Skip bogus events. */
        if ((event == NULL) || (event->len == 0)) {
            i += INOTIFY_EVENT_SIZE;
            continue;
        }

        /* IN_Q_OVERFLOW is the event that occurrs when inotify's
         * event buffer is full.
         */
        if (event->mask & IN_Q_OVERFLOW) {
            log_error
                ("Inotify event buffer is full: Raise the value in %s %s",
                 "/proc/sys/fs/inotify/max_queued_events if",
                 "this is a chronic error");
            i += INOTIFY_EVENT_SIZE + event->len;
            continue;
        }

        /* IN_CLOSE_NOWRITE events occur on a directory when inotify
         * sets up a watch on it. They also occur when someone does
         * a tab complete within a watched tree. We don't care about
         * these events and never want to queue them, so we skip.
         */
        if ((event->mask & IN_ISDIR) && (event->mask & IN_CLOSE_NOWRITE)) {
            log_trace("Skipping inotify IN_CLOSE_NOWRITE event on wd %d",
                      event->wd);
            i += INOTIFY_EVENT_SIZE + event->len;
            continue;
        }

        /* IN_IGNORED events occur when we remove an inotify watch by
         * making a call to inotify_rm_watch(). We don't care about
         * these events and never want to queue them.
         */
        if (event->mask & IN_IGNORED) {
            log_trace("Skipping inotify IN_IGNORED event on wd %d",
                      event->wd);
            i += INOTIFY_EVENT_SIZE + event->len;
            continue;
        }

        /* Skip .~tmp~ events (generated by rsync) */
        char *tmp_str = ".~tmp~";
        char *idx = NULL;

        if (idx = strstr(event->name, tmp_str)) {
            idx += strlen(tmp_str);
            if (*idx == '\0') {
                log_trace("Skipping '.~tmp~' event on wd %d", event->wd);
                i += INOTIFY_EVENT_SIZE + event->len;
                continue;
            }
        }


        log_trace("Got inotify event on %s '%s' for wd %d",
                  ((event->mask & IN_ISDIR) ? "directory" : "file"),
                  event->name, event->wd);

        pthread_mutex_lock(&inotify_mutex);

        /* Since inotify only reports the name of the file
         * or directory under notification we need to lookup
         * it's parent path in our watch descriptor hash map.
         */
        Watch *watch = g_hash_table_lookup(inotify_wd_to_watch,
                                           GINT_TO_POINTER(event->wd)
            );

        /* Move onto the next event if we can't find its watcher.
         *
         * This really ony happens when there is a rapid creation and
         * immediate deletion of directorie trees. The only time I've
         * seen it happen are during stress/load tests where large trees,
         * (i.e. the linux kernel tarball, or Erlang OTP tarball) are un-
         * tar-ed and rm -rf-ed in rapid succession. This is not behavior
         * I really wanted to see, but because of the reactive nature of
         * inotify, and the fact that file system events are not instan-
         * taious this situation might occur. There is eventual consistency
         * so for now I'm labling this less of a bug and more of an
         * unfortunate feature.
         */
        if (watch == NULL) {
            log_trace
                ("Failed to look up watcher for wd '%d' in inotify_handle_event (%s)",
                 event->wd, event->name);
            i += INOTIFY_EVENT_SIZE + event->len;
            pthread_mutex_unlock(&inotify_mutex);
            continue;
        }

        /* Look up the root meta data. */
        int path_len = strlen(watch->path);
        path = malloc(path_len + 1);
        if (path == NULL) {
            log_error
                ("Failed to allocate memory while copying watch path '%s': %s",
                 watch->path, "inotify.c:inotify_handle_event()");
            i += INOTIFY_EVENT_SIZE + event->len;
            pthread_mutex_unlock(&inotify_mutex);
            continue;
        }

        memcpy(path, watch->path, path_len);
        path[path_len] = '\0';

        root = inotify_path_to_root(path);

        if (root == NULL) {
            log_debug("Failed to look up meta data for root '%s'", path);
            i += INOTIFY_EVENT_SIZE + event->len;
            free(path);
            pthread_mutex_unlock(&inotify_mutex);
            continue;
        }

        if (root->pause) {
            log_trace("Root is currently paused. Skipping event");
            i += INOTIFY_EVENT_SIZE + event->len;
            free(path);
            pthread_mutex_unlock(&inotify_mutex);
            continue;
        }

        if (root->destroy != 0) {
            log_trace("Root is being destroyed. Skipping event");
            i += INOTIFY_EVENT_SIZE + event->len;
            free(path);
            pthread_mutex_unlock(&inotify_mutex);
            continue;
        }

        pthread_mutex_unlock(&inotify_mutex);

        /* Construct the absolute path for this event. */
        if (strcmp(path, "/") == 0)
            rv = mk_string(&abs_path, "/%s", path, event->name);
        else
            rv = mk_string(&abs_path, "%s/%s", path, event->name);

        if (rv == -1) {
            log_error
                ("Failed to allocate memory while creating absolute event path: %s",
                 "inotify.c:inotify_handle_event()");
            i += INOTIFY_EVENT_SIZE + event->len;
            free(path);
            continue;
        }

        log_trace("Got event for '%s'", abs_path);

        if (event->mask & IN_ISDIR) {

            /* Here we have new directory creation, which means that
             * beyond just queuing the event(s) we also need to perform
             * a recursive watch on the new tree and make sure those
             * watches are tied to the appropriate root path.
             */
            if ((event->mask & IN_CREATE)
                || (event->mask & IN_MOVED_TO)) {

                /* Skip .~tmp~ directories (generated by rsync) */
                char *tmp_str = ".~tmp~";
                char *idx = NULL;

                if (idx = strstr(path, tmp_str)) {
                    idx += strlen(tmp_str);
                    if (*idx == '\0') {
                        log_trace
                            ("Skipping do_watch_tree() on '.~tmp~' directory: %s",
                             path);
                        i += INOTIFY_EVENT_SIZE + event->len;
                        free(path);
                        free(abs_path);
                        continue;
                    }
                }

                log_trace("New directory '%s' found", abs_path);
                usleep(1000);

                rv = do_watch_tree(abs_path, root, 0);
                if (rv != 0) {
                    log_error("Failed to watch root at dir '%s': %s",
                              abs_path, error_to_string(rv));
                    i += INOTIFY_EVENT_SIZE + event->len;
                    free(path);
                    free(abs_path);
                    continue;
                }
            }

            /* Here we have directory deletion. So we need to tell
             * inotify to stop watching this directory for us, as
             * well as remove the mapping we have stored in our
             * watch descriptor meta maps.
             */
            else if ((event->mask & IN_DELETE)
                     || (event->mask & IN_MOVED_FROM)) {
                log_trace
                    ("Existing directory '%s' has been moved or removed",
                     abs_path);

                /* TODO: How can we determine if the actual root itself
                 *       is being deleted since the root itself is not
                 *       watched by anything?
                 */

                pthread_mutex_lock(&inotify_mutex);

                Watch *delete = g_hash_table_lookup(inotify_path_to_watch,
                                                    abs_path);

                if (delete == NULL) {
                    log_trace
                        ("Failed to look up watcher for path '%s' while attempting to delete it",
                         abs_path);
                    i += INOTIFY_EVENT_SIZE + event->len;
                    free(path);
                    free(abs_path);
                    pthread_mutex_unlock(&inotify_mutex);
                    continue;
                }

                inotify_rm_watch(inotify_fd, delete->wd);

                /* Clean up meta data mappings and tell inotify
                 * to stop watching the deleted dir.
                 */
                void *t1, *t2;
                int wd = delete->wd;

                if (g_hash_table_lookup_extended
                    (inotify_wd_to_watch, GINT_TO_POINTER(wd), &t1, &t2)) {
                    g_hash_table_remove(inotify_wd_to_watch,
                                        GINT_TO_POINTER(wd));
                }

                if (g_hash_table_lookup_extended
                    (inotify_path_to_watch, abs_path, &t1, &t2)
                    && abs_path[0] == '/') {
                    g_hash_table_remove(inotify_path_to_watch, abs_path);
                }

                free(delete->path);
                free(delete);

                if (event->mask & IN_MOVED_FROM) {
                    int rv;
                    GList *key = NULL, *keys = NULL;
                    char *tmp, *sub_path, *ptr;
                    Watch *sub_watch;

                    log_trace
                        ("Existing directory '%s' has been moved. Unwatching it's sub dirs",
                         abs_path);

                    /* Destroy all the watches associated with this subroot. */
                    keys = g_hash_table_get_keys(inotify_path_to_watch);
                    rv = mk_string(&tmp, "%s/", abs_path);
                    if (rv == -1) {
                        log_error
                            ("Failed to allocate memory for temporary path variable: %s",
                             "inotify.c:inotify_handle_event()");
                        g_list_free(keys);
                        free(path);
                        free(abs_path);
                        pthread_mutex_unlock(&inotify_mutex);
                        return;
                    }

                    for (key = keys; key; key = key->next) {
                        sub_path = (char *) key->data;

                        if ((ptr = strstr(sub_path, tmp))
                            && ptr == sub_path) {
                            sub_watch =
                                g_hash_table_lookup(inotify_path_to_watch,
                                                    sub_path);

                            if (sub_watch == NULL) {
                                log_warn
                                    ("While unwatching sub dirs unable to look up watch for path %s",
                                     sub_path);
                            } else {
                                log_debug("Unwatching sub path '%s'",
                                          sub_path);

                                rv = inotify_rm_watch(inotify_fd,
                                                      sub_watch->wd);
                                if (rv != 0) {
                                    log_warn
                                        ("Failed to call inotify_rm_watch() on wd:%d: %s",
                                         sub_watch->wd, strerror(errno));
                                }

                                g_hash_table_remove(inotify_wd_to_watch,
                                                    GINT_TO_POINTER
                                                    (sub_watch->wd));
                                g_hash_table_remove(inotify_path_to_watch,
                                                    sub_path);

                                free(sub_watch->path);
                                free(sub_watch);
                            }
                        }
                    }

                    free(tmp);
                    g_list_free(keys);
                }

                pthread_mutex_unlock(&inotify_mutex);
            }
        }

        /* Queue event */
        if (event->mask & root->mask) {
            rv = inotify_enqueue(root, event, path);
            if (rv != 0)
                log_warn("Failed to queue event for wd:%d path:%s: %s",
                         event->wd, path, error_to_string(rv));
        }

        free(path);
        free(abs_path);

        i += INOTIFY_EVENT_SIZE + event->len;
    }
}

/* Add a new inotify event to its Root's queue.
 *
 * On success 0 (zero) is returned.
 * On failure the appropriate error code is returned.
 */
static int inotify_enqueue(const Root * root, const IN_Event * event,
                           const char *path)
{
    int rv, queue_len;
    Event *node;

    pthread_mutex_lock(&inotify_mutex);

    if (root == NULL) {
        log_warn
            ("Failed to enqueue because root at path %s does not exist",
             path);
        pthread_mutex_unlock(&inotify_mutex);
        return ERROR_INOTIFY_ROOT_DOES_NOT_EXIST;
    }

    /* Check to make sure we don't overflow the queue */
    queue_len = (int) g_queue_get_length(root->queue);

    log_trace("Root '%s' has %d/%d events queued",
              root->path, queue_len, root->max_events);

    if (queue_len >= root->max_events) {
        log_warn
            ("Queue full for root '%s' (max_events=%d). Dropping event!",
             root->path, root->max_events);
        pthread_mutex_unlock(&inotify_mutex);
        return ERROR_INOTIFY_ROOT_QUEUE_FULL;
    }

    log_trace("Queuing event root:%s path:%s name:%s",
              root->path, path, event->name);

    /* Create our new queue node and copy over all
     * the data fields from the event.
     */
    node = (Event *) malloc(sizeof(Event));
    if (node == NULL) {
        log_error("Failed to allocate memory for new queue node: %s",
                  "inotify.c:inotify_enqueue()");
        return ERROR_MEMORY_ALLOCATION;
    }

    node->wd = event->wd;
    node->mask = event->mask;
    node->cookie = event->cookie;
    node->len = event->len;

    rv = mk_string(&node->name, "%s", event->name);
    if (rv == -1) {
        log_error("Failed to allocate memory for new queue node NAME: %s",
                  "inotify.c:inotify_enqueue()");
        return ERROR_MEMORY_ALLOCATION;
    }

    rv = mk_string(&node->path, "%s", path);
    if (rv == -1) {
        log_error("Failed to allocate memory for new queue node PATH: %s",
                  "inotify.c:inotify_enqueue()");
        return ERROR_MEMORY_ALLOCATION;
    }

    /* Add new node to the queue. */
    if (root != NULL)
        g_queue_push_tail(root->queue, node);

    pthread_mutex_unlock(&inotify_mutex);
    return 0;
}

/* Return a list of all the currently watched root paths. */
char **inotify_get_roots(void)
{
    int i = 0, rv;
    GList *key = NULL, *keys = NULL;
    char **roots;

    pthread_mutex_lock(&inotify_mutex);

    keys = g_hash_table_get_keys(inotify_roots);
    roots = malloc((g_list_length(keys) + 1) * (sizeof *roots));

    if (roots == NULL) {
        log_error("Failed to allocate memory for roots list: %s",
                  "inotify.c:inotify_get_roots()");
        g_list_free(keys);
        pthread_mutex_unlock(&inotify_mutex);
        return NULL;
    }

    for (key = keys; key; key = key->next) {
        rv = mk_string(&roots[i++], "%s", (char *) key->data);

        if (rv == -1) {
            log_error
                ("Failed to allocate memory while adding root to list: %s",
                 "inotify.c:inotify_get_roots()");
            g_list_free(keys);
            pthread_mutex_unlock(&inotify_mutex);
            return NULL;
        }
    }

    rv = mk_string(&roots[i], "EOL");
    if (rv == -1) {
        log_error
            ("Failed to allocate memory while adding EOL to list: %s",
             "inotify.c:inotify_get_roots()");
        g_list_free(keys);
        pthread_mutex_unlock(&inotify_mutex);
        return NULL;
    }

    g_list_free(keys);

    pthread_mutex_unlock(&inotify_mutex);

    return roots;
}

/* Take the data structure created by the inotify_get_roots()
 * (ABOVE) and free all it's dynamically allocated memory.
 */
void inotify_free_roots(char **roots)
{
    int i;

    for (i = 0; strcmp(roots[i], "EOL") != 0; i++)
        free(roots[i]);

    free(roots[i]);
    free(roots);
}

void inotify_cleanup(void)
{
    inotify_dump_roots();
}

void inotify_dump_roots(void)
{
    int i, rv;
    FILE *fp;
    GList *roots_ptr = NULL, *roots = NULL;
    Root *root;
    char *dump_file;

    pthread_mutex_lock(&inotify_mutex);

    fp = fopen(INOTIFY_ROOT_DUMP_FILE, "w");
    if (fp == NULL) {
        log_error("Failed to open root dump file %s for writing: %s",
                  dump_file, strerror(errno));
        free(dump_file);
        pthread_mutex_unlock(&inotify_mutex);
        return;
    }

    roots = g_hash_table_get_values(inotify_roots);

    for (roots_ptr = roots; roots != NULL; roots = roots->next) {
        root = roots->data;
        if (root->rewatch)
            fprintf(fp, "%s,%d,%d\n", root->path, root->mask,
                    root->max_events);
    }

    g_list_free(roots_ptr);
    fclose(fp);
    free(dump_file);
    pthread_mutex_unlock(&inotify_mutex);
}

/* Take the data structure that holds events and free all
 * it's dynamically allocated memory.
 */
void inotify_free_events(Event ** events)
{
    int i;

    if (events != NULL) {
        for (i = 0; events[i]; i++) {
            free_node_mem(events[i], NULL);
        }
    }

    free(events);
}

static Event **inotify_dequeue(Root * root, int count)
{
    int i, queue_len;
    Event *e, **events;

    if (count == 0)
        log_debug("Dequeuing *all* events from root '%s'", root->path);
    else
        log_debug("Dequeuing %d events from root '%s'", count, root->path);

    pthread_mutex_lock(&inotify_mutex);

    queue_len = (int) g_queue_get_length(root->queue);

    if (queue_len == 0) {
        pthread_mutex_unlock(&inotify_mutex);
        return NULL;
    }

    if (count == 0 || count > queue_len)
        count = queue_len;

    log_trace("Root '%s' has %d/%d events queued. Dequeueing %d events.",
              root->path, queue_len, root->max_events, count);

    events = malloc((count + 1) * sizeof *events);
    if (events == NULL) {
        log_error("Failed to allocate memory for events list: %s",
                  "inotify.c:inotify_dequeue()");
        pthread_mutex_unlock(&inotify_mutex);
        return (Event **) - 1;
    }

    for (i = 0; i < count; i++) {
        e = g_queue_pop_head(root->queue);

        log_trace("Dequeued event root:%s path:%s name:%s",
                  root->path, e->path, e->name);

        events[i] = e;
    }
    events[i] = NULL;

    pthread_mutex_unlock(&inotify_mutex);
    return events;
}

/* Given a root path return 'count' events from the
 * front of the queue, if there are any.
 */
Event **inotify_get_events(const char *path, int count)
{
    Root *root;

    root = inotify_is_root(path);
    if (root == NULL) {
        log_warn
            ("Cannot get event for path '%s' since it is not a watched root'",
             path);
        return (Event **) NULL;
    }

    return inotify_dequeue(root, count);
}

/* Given a root path grab a single event off the queue */
Event **inotify_get_event(const char *path)
{
    return inotify_get_events(path, 1);
}

/* Given a path return data indicating whether or not the path
 * is a watched root.
 *
 * Since roots are stored in a hash table this is really not
 * necessary, it just provides a little syntatic sugar.
 */
Root *inotify_is_root(const char *path)
{
    return g_hash_table_lookup(inotify_roots, path);
}

/* Given a path determine if it has a watched root, and if so
 * what that root path is. For example, if the root path
 *
 *   /zing/zang
 *
 * is being watched then calling this function with the following
 * paths would return the value '/zing/zang':
 *
 *   /zing/zang/zong
 *   /zing/zang/zoop/boop
 */
Root *inotify_path_to_root(const char *path)
{
    int rv;
    GList *key = NULL, *keys = NULL;
    char *tmp;
    Root *root;

    root = g_hash_table_lookup(inotify_roots, "/");
    if (root != NULL)
        return root;

    keys = g_hash_table_get_keys(inotify_roots);

    for (key = keys; key; key = key->next) {

        rv = mk_string(&tmp, "%s/", (char *) key->data);
        if (rv == -1) {
            log_error
                ("Failed to allocate memory while copying data to a temporary variable: %s",
                 "inotify.c:inotify_path_to_root()");
            g_list_free(keys);
            return NULL;
        }

        if ((strcmp(path, key->data) == 0) || strstr(path, tmp)) {
            free(tmp);

            root = g_hash_table_lookup(inotify_roots, key->data);

            if (root == NULL) {
                log_warn("Failed to look up root for '%s'", key->data);
                g_list_free(keys);
                return NULL;
            }

            log_trace("Found root '%s' for path '%s'", key->data, path);

            g_list_free(keys);

            return root;
        }

        free(tmp);
    }

    g_list_free(keys);
    return NULL;
}

/* Given a path see if it's the parent path of a currently watched
 * root. For example, if the root path
 *
 *   /foo/bar/baz
 *
 * is being watched, then all of the following ARE parents:
 *
 *   /foo/bar
 *   /foo
 *   /
 *
 * and the following ARE NOT parents:
 *
 *   /bing/bong
 *   /foo/bar/bang
 *
 * This function is primarily used to determine if a new request
 * to watch a directory tree will collide with a tree that's already
 * being watched.
 */
char *inotify_is_parent(const char *path)
{
    int rv;
    char *tmp, *parent;
    GList *key = NULL, *keys = NULL;

    rv = mk_string(&tmp, "%s/", path);
    if (rv == -1) {
        log_error("Failed to allocate memory for temporary path '%s': %s",
                  path, "inotify.c:inotify_is_parent()");
        return (char *) -1;
    }

    keys = g_hash_table_get_keys(inotify_roots);

    for (key = keys; key; key = key->next) {
        if (strstr(key->data, tmp)) {
            rv = mk_string(&parent, "%s", key->data);
            if (rv == -1) {
                log_error
                    ("Failed to allocate memory for return value '%s': %s",
                     path, "inotify.c:inotify_is_parent()");
                return (char *) -1;
            }
            free(tmp);
            g_list_free(keys);
            return parent;
        }
    }

    free(tmp);
    g_list_free(keys);

    return NULL;
}

/* When a client app makes an 'unwatch' call this is the function
 * that eventually gets called to do the dirty work.
 */
static int destroy_root(Root * root)
{
    int rv;
    pthread_t t;
    pthread_attr_t attr;

    if (root == NULL) {
        log_warn("Attempting to destroy an unwatched root");
        return ERROR_INOTIFY_ROOT_DOES_NOT_EXIST;
    }

    root->destroy = 1;
    usleep(1000);

    /* Initialize thread attribute to automatically detach */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    rv = pthread_create(&t, &attr, _destroy_root, (void *) root);
    if (rv) {
        log_error("Failed to create new thread for destroying root '%s'",
                  root->path);
        return ERROR_FAILED_TO_CREATE_NEW_THREAD;
    }

    pthread_attr_destroy(&attr);

    return 0;
}

static void *_destroy_root(void *thread_data)
{
    Root *root;
    int rv;
    Watch *watch;
    char *tmp, *path, *ptr;
    GList *key = NULL, *keys = NULL;

    root = thread_data;

    pthread_mutex_lock(&inotify_mutex);

    rv = mk_string(&tmp, "%s/", root->path);
    if (rv == -1) {
        log_error
            ("Failed to allocate memory for temporary path variable: %s",
             "inotify.c:destroy_root()");
        return NULL;
    }

    /* Destroy all the queue data associated with this root. */
    g_queue_foreach(root->queue, (GFunc) free_node_mem, NULL);
    g_queue_free(root->queue);

    /* Destroy all the watches associated with this root. */
    keys = g_hash_table_get_keys(inotify_path_to_watch);

    pthread_mutex_unlock(&inotify_mutex);

    for (key = keys; key; key = key->next) {

        path = (char *) key->data;

        if ((ptr = strstr(path, tmp)) && ptr == path) {

            pthread_mutex_lock(&inotify_mutex);
            watch = g_hash_table_lookup(inotify_path_to_watch, path);
            pthread_mutex_unlock(&inotify_mutex);

            if (watch == NULL) {
                log_warn
                    ("During recusrive delete unable to look up watch for path %s",
                     path);
                continue;
            }

            log_debug("Unwatching path '%s'", path);

            rv = inotify_rm_watch(inotify_fd, watch->wd);
            if (rv != 0) {
                log_warn("Failed to call inotify_rm_watch() on wd:%d: %s",
                         watch->wd, strerror(errno));
            }

            pthread_mutex_lock(&inotify_mutex);
            g_hash_table_remove(inotify_wd_to_watch,
                                GINT_TO_POINTER(watch->wd));
            g_hash_table_remove(inotify_path_to_watch, path);
            pthread_mutex_unlock(&inotify_mutex);

            free(watch->path);
            free(watch);
        }
    }
    free(tmp);

    pthread_mutex_lock(&inotify_mutex);

    /* Destroy the root watch itself. */
    watch = g_hash_table_lookup(inotify_path_to_watch, root->path);
    if (watch != NULL) {
        g_hash_table_remove(inotify_wd_to_watch,
                            GINT_TO_POINTER(watch->wd));
        g_hash_table_remove(inotify_path_to_watch, root->path);

        free(watch->path);
        free(watch);
    }

    char *root_path = root->path;
    g_hash_table_remove(inotify_roots, root_path);
    free(root_path);
    root = NULL;
    --inotify_num_watched_roots;

    g_list_free(keys);

    pthread_mutex_unlock(&inotify_mutex);

    inotify_dump_roots();
    pthread_exit(NULL);
}

int inotify_pause_tree(char *path)
{
    Root *root;

    pthread_mutex_lock(&inotify_mutex);

    root = inotify_is_root(path);
    if (root == NULL) {
        log_warn
            ("Cannot pause path '%s' since it is not a watched root'",
             path);
        pthread_mutex_unlock(&inotify_mutex);
        return ERROR_INOTIFY_ROOT_NOT_WATCHED;
    }

    root->pause = 1;

    pthread_mutex_unlock(&inotify_mutex);

    return 0;
}

int inotify_unpause_tree(char *path)
{
    Root *root;

    pthread_mutex_lock(&inotify_mutex);

    root = inotify_is_root(path);
    if (root == NULL) {
        log_warn
            ("Cannot unpause path '%s' since it is not a watched root'",
             path);
        pthread_mutex_unlock(&inotify_mutex);
        return ERROR_INOTIFY_ROOT_NOT_WATCHED;
    }

    root->pause = 0;

    pthread_mutex_unlock(&inotify_mutex);

    return 0;
}

int inotify_unwatch_tree(char *path)
{
    int last;
    Root *root;

    /* Clean up path by removing the trailing slash, if it exists. */
    last = strlen(path) - 1;
    if (path[last] == '/')
        path[last] = '\0';

    /* First check to see if the path is a valid,
     * watched root.
     */
    root = inotify_is_root(path);
    if (root == NULL) {
        log_warn
            ("Cannot unwatch path '%s' since it is not a watched root'",
             path);
        return ERROR_INOTIFY_ROOT_NOT_WATCHED;
    } else if (root->destroy) {
        log_warn("Currently destroying tree at root '%s'", path);
        return ERROR_INOTIFY_ROOT_BEING_DESTROYED;
    }

    return destroy_root(root);
}

/* Recursively watch a tree. This involves setting up inotify watches
 * for each directory in the tree, as well as adding entries in the
 * meta data mappings.
 */
int inotify_watch_tree(char *path, int mask, int max_events, int rewatch)
{
    int rv, last;

    log_trace("Entering inotify_watch_tree() on path '%s' with mask %lu",
              path, mask);

    /* Clean up path by removing the trailing slash, it exists. */
    last = strlen(path) - 1;
    if ((path[last] == '/') && (strcmp(path, "/") != 0))
        path[last] = '\0';

    /* A quick check of the current state of watched roots. */
    {
        /* First we make sure we're not already watching a tree
         * at this root. This includes a sub tree, i.e. if the
         * root '/foo' is already being watched the user requests
         * a watch at '/foo/bar/baz'.
         */
        pthread_mutex_lock(&inotify_mutex);
        Root *r = inotify_path_to_root(path);

        if (r != NULL) {
            pthread_mutex_unlock(&inotify_mutex);

            if (strcmp(path, r->path) == 0) {
                if (r->destroy) {
                    log_warn("Currently destroying tree at root '%s'",
                             path);
                    return ERROR_INOTIFY_ROOT_BEING_DESTROYED;
                } else {
                    log_warn("Already watching tree at root '%s'", path);
                    return ERROR_INOTIFY_ROOT_ALREADY_WATCHED;
                }
            } else {
                log_warn
                    ("path '%s' is the child of already watched root '%s'",
                     path, r->path);
                return ERROR_INOTIFY_CHILD_OF_ROOT;
            }
        }

        /* Second, we check to see if the path the user is trying
         * to watch is a parent of an already watched root, i.e.
         * if '/foo/bar/baz' is already being wacthed and a user
         * requests a watch at '/foo'.
         */
        char *sub_path = inotify_is_parent(path);
        if (sub_path == (char *) -1) {
            log_error
                ("Memory allocation error while calling inotify_is_parent");
            free(sub_path);
            pthread_mutex_unlock(&inotify_mutex);
            return ERROR_MEMORY_ALLOCATION;
        } else if (sub_path) {
            log_warn
                ("Path '%s' is the parent of already watched root '%s'",
                 path, sub_path);
            free(sub_path);
            pthread_mutex_unlock(&inotify_mutex);
            return ERROR_INOTIFY_PARENT_OF_ROOT;
        }

        pthread_mutex_unlock(&inotify_mutex);
    }

    /* Check to make sure root is a valid, and open-able, directory. */
    {
        DIR *d = opendir(path);
        if (d == NULL) {
            log_error("Failed to open root at dir '%s': %s",
                      path, strerror(errno));
            return ERROR_INOTIFY_ROOT_DOES_NOT_EXIST;
        }
        closedir(d);
    }

    /* Next we allocate space and store the meta data
     * for our new root.
     */
    Root *new_root;

    pthread_mutex_lock(&inotify_mutex);

    new_root = make_root(path, mask, max_events, rewatch);
    if (new_root == NULL) {
        log_error
            ("Failed to create new root for path %s: memory allocation error",
             path);
        pthread_mutex_unlock(&inotify_mutex);
        return ERROR_MEMORY_ALLOCATION;
    }

    g_hash_table_replace(inotify_roots, g_strdup(path), new_root);
    ++inotify_num_watched_roots;

    pthread_mutex_unlock(&inotify_mutex);

    inotify_dump_roots();

    /* Finally we need to recursively setup inotify
     * watches for our new root.
     */
    rv = do_watch_tree(new_root->path, new_root, 0);
    if (rv != 0) {
        log_error("Failed to watch root at dir '%s': %s", path,
                  error_to_string(rv));
    }

    return rv;
}

/* Recursive, threaded portion of inotify_watch_tree(). */
static int do_watch_tree(const char *path, Root * root, int cleanup)
{
    int rv;
    pthread_t t;
    pthread_attr_t attr;
    T_Data *data;

    if ((root == NULL) || (root->destroy != 0)) {
        log_trace("Bailing out watch tree on path %s. %s", path,
                  "It's root is either unwatched or is being destroyed");
        return ERROR_INOTIFY_ROOT_DOES_NOT_EXIST;
    }

    data = (T_Data *) malloc(sizeof(T_Data));

    if (data == NULL) {
        log_error("Failed to allocate memory for thread data: %s",
                  "inotify.c:do_watch_tree()");
        return ERROR_MEMORY_ALLOCATION;
    }

    rv = mk_string(&data->path, "%s", path);
    if (rv == -1) {
        log_error("Failed to allocate memory for thread data PATH: %s",
                  "inotify.c:do_watch_tree()");
        return ERROR_MEMORY_ALLOCATION;
    }

    data->root = root;
    data->cleanup = cleanup;

    /* Initialize thread attribute to automatically detach */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    rv = pthread_create(&t, &attr, _do_watch_tree, (void *) data);
    if (rv) {
        log_error("Failed to create new thread for watch on '%s': %d",
                  path, rv);
        free(data->path);
        free(data);
        return ERROR_FAILED_TO_CREATE_NEW_THREAD;
    }

    pthread_attr_destroy(&attr);

    return 0;
}

static void *_do_watch_tree(void *thread_data)
{
    int cleanup;
    T_Data *data;
    data = thread_data;
    cleanup = data->cleanup;

    if ((data->root == NULL) || (data->root->destroy != 0)) {
        log_error
            ("Bailing out of recursive watch because the root is not watched: %s",
             "inotify.c:_do_watch_tree()");
        free(data->path);
        free(data);
        return (void *) 1;
    }

    if (strlen(data->path) < strlen(data->root->path)) {
        log_error
            ("Bailing out of recursive watch because a bad path was given: %s",
             "inotify.c:_do_watch_tree()");
        free(data->path);
        free(data);
        return (void *) 1;
    }

    if (data->cleanup) {
        IN_ROOT_REWATCH = 1;
        NUM_ROOT_REWATCH = 0;
    }

    _do_watch_tree_rec(data->path, data->root, data->cleanup);

    if (data->cleanup) {
        log_notice
            ("Completed full rewatch of root '%s' finding %d orphaned directories",
             data->path, NUM_ROOT_REWATCH);
        IN_ROOT_REWATCH = 0;
        NUM_ROOT_REWATCH = 0;
    }

    free(data->path);
    free(data);

    pthread_exit(NULL);

    return (void *) 0;
}

static void _do_watch_tree_rec(char *path, Root * root, int cleanup)
{
    int wd, rv;
    DIR *d;
    struct dirent *dir;
    Watch *watch;
    char *tmp;
    struct stat stat_buf;

    /* Skip .~tmp~ directories (generated by rsync) */
    char *tmp_str = ".~tmp~";
    char *idx = NULL;

    if (idx = strstr(path, tmp_str)) {
        idx += strlen(tmp_str);
        if (*idx == '\0') {
            log_trace("Skipping watch on '.~tmp~' directory: %s", path);
            return;
        }
    }

    pthread_mutex_lock(&inotify_mutex);

    if ((root == NULL) || (root->destroy != 0)) {
        log_trace("Skipping watch tree on path %s. %s", path,
                  "because root has been unwatched or is being destroyed");
        pthread_mutex_unlock(&inotify_mutex);
        return;
    }

    /* If we're in cleanup mode just check to see if the path we've currently
     * recursed to is being watched. If not it has been missed or deleted at
     * some point and we need to rewatch it. If it's already in the watch list
     * skip it and move on down the tree.
     */
    if (cleanup) {
        watch = g_hash_table_lookup(inotify_path_to_watch, path);

        if (watch != NULL) {
            pthread_mutex_unlock(&inotify_mutex);
            return;
        }

        ++NUM_ROOT_REWATCH;
        log_debug
            ("In memclean routine found orphan path '%s' that needs to be rewatched",
             path);
    }

    /* Check to make sure path is a valid, and open-able, directory. */
    {
        DIR *d = opendir(path);
        if (d == NULL) {
            log_debug
                ("While doing recursive watch failed to open root at dir '%s' in _do_watch_tree_rec(): %s",
                 path, strerror(errno));
            pthread_mutex_unlock(&inotify_mutex);
            return;
        }
        closedir(d);
    }

    /* XXX: Need switch here to do ALL_EVENTS or just the root->mask. */
    /*
       wd = inotify_add_watch(inotify_fd, path,
       IN_ALL_EVENTS | IN_DONT_FOLLOW);
     */
    wd = inotify_add_watch(inotify_fd, path, root->mask | IN_DONT_FOLLOW);

    if (wd < 0) {
        log_error("Failed to set up inotify watch for path '%s': %s",
                  path, strerror(errno));
        pthread_mutex_unlock(&inotify_mutex);
        return;
    }

    log_trace("Watching wd:%d path:%s", wd, path);

    watch = make_watch(wd, path);
    if (watch == NULL) {
        log_error("Failed to create new watch for wd:%d path:%s: %s",
                  "memory allocation error", wd, path);
        pthread_mutex_unlock(&inotify_mutex);
        return;
    }

    if (g_hash_table_lookup(inotify_wd_to_watch, GINT_TO_POINTER(wd))) {
        log_debug
            ("Found a tree that's already being watched: wd:%d path:%s",
             wd, path);
        free(watch->path);
        free(watch);
        pthread_mutex_unlock(&inotify_mutex);
        return;
    }

    g_hash_table_replace(inotify_wd_to_watch, GINT_TO_POINTER(wd), watch);
    g_hash_table_replace(inotify_path_to_watch, g_strdup(path), watch);

    pthread_mutex_unlock(&inotify_mutex);

    d = opendir(path);
    if (d == NULL) {
        log_error("Failed to open dir '%s' in _do_watch_tree_rec(): %s",
                  path, strerror(errno));
        closedir(d);
        return;
    }

    while ((dir = readdir(d))) {

        if (strcmp(dir->d_name, ".") == 0
            || strcmp(dir->d_name, "..") == 0)
            continue;

        if (strcmp(path, "/") == 0)
            rv = mk_string(&tmp, "/%s", dir->d_name);
        else
            rv = mk_string(&tmp, "%s/%s", path, dir->d_name);

        if (rv == -1) {
            log_error
                ("Failed to allocate memroy for temporary path variable: %s",
                 "inotify.c:_do_watch_tree_rec");
            free(tmp);
            closedir(d);
            return;
        }

        /* If d_type is DT_UNKNOWN then we are on a file system that
         * does not support dirent::d_type flags. This includes XFS
         * and ZFS, and probably others.
         *
         * Since we cannot use the d_type flag on XFS and ZFS to
         * determine if a file is a directory we have to make a call
         * to stat() for each file we encounter. This is not very
         * efficient for large trees, but it's what we're left with.
         */
        if (dir->d_type == DT_UNKNOWN) {
            lstat(tmp, &stat_buf);

            if (S_ISLNK(stat_buf.st_mode) || !S_ISDIR(stat_buf.st_mode)) {
                free(tmp);
                continue;
            }
        } else if ((dir->d_type == DT_LNK) || (dir->d_type != DT_DIR)) {
            free(tmp);
            continue;
        }

        /* Make sure that while looping our root hasn't been destroyed. */
        if ((root == NULL) || (root->destroy != 0)) {
            log_trace("Skipping watch tree on path %s. %s", path,
                      "because root has been unwatched or is being destroyed");
            free(tmp);
            closedir(d);
            return;
        }

        /* Recurse! */
        _do_watch_tree_rec(tmp, root, cleanup);

        free(tmp);
    }

    closedir(d);
}

/* Create a new root meta data structure. */
static Root *make_root(const char *path, int mask, int max_events,
                       int rewatch)
{
    int rv;
    Root *root;

    root = malloc(sizeof(Root));
    if (root == NULL) {
        log_error("Failed to allocate memory for new root: %s",
                  "inotify.c:make_root()");
        return NULL;
    }

    rv = mk_string(&root->path, "%s", path);
    if (rv == -1) {
        log_error("Failed to allocate memory for new root PATH: %s",
                  "inotify.c:make_root()");
        return NULL;
    }

    root->mask = mask;
    root->queue = g_queue_new();
    root->max_events = max_events;
    root->destroy = 0;
    root->pause = 0;
    root->rewatch = rewatch;
    root->persist = 0;          /* TODO: Future feature */

    g_queue_init(root->queue);

    return root;
}

/* Create a new watch structure. There will be one of these
 * for every single directory we set up an inotify watch
 * for.
 */
static Watch *make_watch(int wd, const char *path)
{
    int rv, len;
    size_t size;
    Watch *watch;

    size = strlen(path);
    watch = malloc(sizeof(Watch));
    if (watch == NULL) {
        log_error("Failed to allocate memory for new watch: %s",
                  "inotify.c:make_watch()");
        return NULL;
    }

    watch->wd = wd;

    len = strlen(path);
    watch->path = malloc(len + 1);
    if (watch->path == NULL) {
        log_error("Failed to allocate memory for new watch PATH '%s': %s",
                  path, "inotify.c:make_watch()");
        return NULL;
    }

    memcpy(watch->path, path, len);
    watch->path[len] = '\0';

    return watch;
}

/* Free up the dynamically allocated memory of a queue node. */
static void free_node_mem(Event * node, gpointer user_data)
{
    free(node->path);
    free(node->name);
    free(node);
}

/* This function will periodically get called to go through
 * the full list of watched directories and see if they still
 * exist on disk. If they exist in Inotispy's watch list but
 * do not exist on disk then something goofy happend (which
 * can occur under heavy disk load with quick file system
 * operations, i.e. creates immediately followed by deletes)
 * and we just need to clean up the memory that's been allocated
 * for the non-existant directories.
 */
void inotify_memclean(void)
{
    int rv;
    pthread_t t;
    pthread_attr_t attr;

    if (IN_MEMCLEAN) {
        log_debug("Already performing a memclean. Skipping operation");
        return;
    }

    /* Initialize thread attribute to automatically detach */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    rv = pthread_create(&t, &attr, _inotify_memclean, NULL);
    if (rv) {
        log_error("Failed to create new thread for memclean operation");
        return;
    }

    pthread_attr_destroy(&attr);
}

void inotify_rewatch_roots(void)
{
    int rv;
    Root *root;
    GList *roots_ptr = NULL, *roots = NULL;

    if (IN_ROOT_REWATCH != 0) {
        log_debug
            ("Already performing complete root rewatch on all roots. Skipping...");
        return;
    }

    log_notice("Performing complete root rewatch on all roots");

    /* Now let's rewatch all our roots for directories
     * that somehow got lost in the madness.
     */
    pthread_mutex_lock(&inotify_mutex);
    roots = g_hash_table_get_values(inotify_roots);
    pthread_mutex_unlock(&inotify_mutex);

    for (roots_ptr = roots; roots != NULL; roots = roots->next) {
        root = roots->data;

        log_debug("Rewatching root '%s'", root->path);
        rv = do_watch_tree(root->path, root, 1);

        if (rv != 0) {
            log_error("Failed to watch root at dir '%s': %s", root->path,
                      error_to_string(rv));
        }
    }

    g_list_free(roots_ptr);
}

static void *_inotify_memclean(void *thread_data)
{
    int i, rv;
    double count = 0, total = 0;
    GList *key = NULL, *keys = NULL;
    char *tmp, *sub_path, *ptr;
    Watch *watch;

    thread_data = NULL;
    IN_MEMCLEAN = 1;

    /* Sleep for a couple seconds to let potential
     * file system operations to catch up.
     */
    sleep(2);

    log_notice("Performing the inotify metadata memory cleanup.");

    pthread_mutex_lock(&inotify_mutex);
    keys = g_hash_table_get_keys(inotify_path_to_watch);
    pthread_mutex_unlock(&inotify_mutex);

    for (key = keys; key; key = key->next, ++total) {

        char *path = key->data;
        path[strlen(path)] = 0;
        /* If the path does not start with at '/' then we've found a garbage
         * path key. This is most likey because the inotify_path_to_watch hash
         * was modified while the memcleanup routine was running. This is a
         * little sloppy, but I'm just going to continue letting the cleanup
         * run. This is bound to happen and the next time around will clean
         * things up.
         */
        if (path[0] != '/')
            continue;

        /* If the directory does not exist on disk or has become a garbage
         * path that does not start with at slash (/), and is still in this
         * list then we have a rogue watch that needs it's meta data blown
         * away.
         */
        if (!is_a_dir(path)) {

            watch = NULL;
            void *t1, *t2;
            pthread_mutex_lock(&inotify_mutex);

            if (g_hash_table_lookup_extended
                (inotify_path_to_watch, path, &t1, &t2)) {
                watch = g_hash_table_lookup(inotify_path_to_watch, path);
            }

            if (watch == NULL) {
                log_trace
                    ("In memclean routine, watcher for path '%s' is already freed",
                     path);
                pthread_mutex_unlock(&inotify_mutex);
                continue;
            }

            log_debug("Found rogue directory '%s'.", path);

            ++count;

            /* Clean up meta data mappings and tell inotify
             * to stop watching the deleted dir.
             */
            inotify_rm_watch(inotify_fd, watch->wd);

            if (g_hash_table_lookup_extended
                (inotify_wd_to_watch, GINT_TO_POINTER(watch->wd), NULL,
                 NULL)) {
                g_hash_table_remove(inotify_wd_to_watch,
                                    GINT_TO_POINTER(watch->wd));
            }

            if (g_hash_table_lookup_extended
                (inotify_path_to_watch, path, NULL, NULL)) {
                g_hash_table_remove(inotify_path_to_watch, path);
            }

            free(watch->path);
            free(watch);
            watch = NULL;

            pthread_mutex_unlock(&inotify_mutex);
        }
    }

    /* Log our results. */
    if (count > 0) {
        log_notice
            ("Memory cleanup results: %d/%d rogue watches found -- %7.4f%% cleanup rate",
             (int) count, (int) total, ((count / total) * 100));
    } else {
        log_notice("Memory cleanup found 0/%d rogue watches.",
                   (int) total);
    }

    pthread_mutex_lock(&inotify_mutex);

    g_list_free(keys);
    IN_MEMCLEAN = 0;

    pthread_mutex_unlock(&inotify_mutex);
    pthread_exit(NULL);
}
