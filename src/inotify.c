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
#include "inotify.h"

#include <glib.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib/ghash.h>

pthread_mutex_t inotify_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MLOCK() pthread_mutex_lock(&inotify_mutex);
#define MUNLOCK() pthread_mutex_unlock(&inotify_mutex);

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
} T_Data;

/* Prototypes for private functions. */
Root *inotify_path_to_root(char *path);
Root *make_root(char *path, int mask, int max_events);
Watch *make_watch(int wd, char *path);
int inotify_root_exists(char *path);
char *inotify_is_parent(char *path);
int inotify_enqueue(Root * root, IN_Event * event, char *path);
void free_node_mem(Event * node, gpointer user_data);

int do_watch_tree(char *path, Root * root);
void *_do_watch_tree(void *data);
void _do_watch_tree_rec(char *path, Root * root);

int do_unwatch_tree(char *path, Root * root);
void *_do_unwatch_tree(void *data);
void _do_unwatch_tree_rec(char *path);

/* Initialize inotify file descriptor and set up meta data hashes.
 *
 * On success the inotify file descriptor is returned.
 * On failure 0 (zero) is returned.
 */
int inotify_setup(void)
{
    inotify_num_watched_roots = 0;

    inotify_fd = inotify_init();

    if (inotify_fd < 0) {
	LOG_ERROR("Inotify failed to init: %s", strerror(errno));
	return 0;
    }

    inotify_wd_to_watch =
	g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

    if (inotify_wd_to_watch == NULL) {
	LOG_ERROR("Failed to init GHashTable inotify_wd_to_watch");
	return 0;
    }

    inotify_path_to_watch =
	g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    if (inotify_path_to_watch == NULL) {
	LOG_ERROR("Failed to init GHashTable inotify_path_to_watch");
	return 0;
    }

    inotify_roots =
	g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    if (inotify_roots == NULL) {
	LOG_ERROR("Failed to init GHashTable inotify_roots");
	return 0;
    }

    return inotify_fd;
}

void inotify_handle_event(int fd)
{
    int i = 0;
    int num_in_events;
    char buffer[INOTIFY_EVENT_BUF_LEN];

    /* Grab the next buffer of inotoify events. If the
     * length of this buffer is negative then we've
     * encountered a read error.
     */
    num_in_events = read(fd, buffer, INOTIFY_EVENT_BUF_LEN);

    if (num_in_events < 0) {
	LOG_ERROR("Inotify read error: %s", strerror(errno));
	return;
    }

    /* Loop through, read, and act on the returned
     * list of inotify events.
     */
    while (i < num_in_events) {

	IN_Event *event = (struct inotify_event *) &buffer[i];

	LOG_TRACE("Got inotify event '%s' for wd %d", event->name,
		  event->wd);

	if (event->len) {

	    Root *root;
	    char *path;
	    char *abs_path;

	    /* Since inotify only reports the name of the file
	     * or directory under notification we need to lookup
	     * it's parent path in our watch descriptor hash map.
	     */
	    MLOCK();
	    Watch *watch =
		g_hash_table_lookup(inotify_wd_to_watch,
				    GINT_TO_POINTER(event->wd)
		);
	    MUNLOCK();

	    /* Move onto the next event if we can't find its watcher.
	     *
	     * XXX: This should only happen if something goes
	     *      *seriously* wrong. What this means is that
	     *      Inotispy is getting inotify event notifications
	     *      for directories it doesn't know it's watching.
	     */
	    if (watch == NULL) {
		LOG_ERROR
		    ("Failed to look up watcher for wd %d in inotify_handle_event",
		     event->wd);
		i += INOTIFY_EVENT_SIZE + event->len;
		continue;
	    }

	    /* Look up the root meta data. */
	    path = watch->path;
	    root = inotify_path_to_root(path);

	    if (root == NULL) {
		LOG_ERROR("Failed to look up meta data for root '%s'",
			  path);
		i += INOTIFY_EVENT_SIZE + event->len;
		continue;
	    }

	    /* Construct the absolute path for this event. */
	    asprintf(&abs_path, "%s/%s", path, event->name);

	    LOG_DEBUG("Got event for '%s'", abs_path);

	    if (event->mask & IN_ISDIR) {

		/* Here we have new directory creation, which means that
		 * beyond just queuing the event(s) we also need to perform
		 * a recursive watch on the new tree and make sure those
		 * watches are tied to the appropriate root path.
		 */
		if ((event->mask & IN_CREATE)
		    || (event->mask & IN_MOVED_TO)) {
		    LOG_DEBUG("New directory '%s' found", abs_path);

		    do_watch_tree(abs_path, root);
		}

		/* Here we have directory deletion. So we need to tell
		 * inotify to stop watching this directory tree for us,
		 * as well as remove the mapping we have stored in our
		 * watch descriptor hash map.
		 */
		else if ((event->mask & IN_DELETE)
			 || (event->mask & IN_MOVED_FROM)) {
		    LOG_DEBUG("Existing directory '%s' has been removed",
			      abs_path);

		    MLOCK();
		    Watch *delete =
			g_hash_table_lookup(inotify_path_to_watch,
					    g_strdup(abs_path)
			);
		    MUNLOCK();

		    if (delete == NULL) {
			LOG_WARN("Failed to look up watcher for path %s",
				 abs_path);
			i += INOTIFY_EVENT_SIZE + event->len;
			free(abs_path);
			continue;
		    }

		    /* Clean up meta data mappings and tell inotify
		     * to stop watching the deleted dir.
		     */
		    int wd = delete->wd;

		    MLOCK();
		    g_hash_table_remove(inotify_wd_to_watch,
					GINT_TO_POINTER(wd));
		    g_hash_table_remove(inotify_path_to_watch, abs_path);
		    MUNLOCK();

		    int rv = inotify_rm_watch(inotify_fd, wd);
		    if (rv != 0) {
			LOG_WARN
			    ("Failed call to inotify_rm_watch on dir '%s': %s",
			     abs_path, strerror(errno));
		    }

		    /* Check to see if the directory being deleted is
		     * the root itself. If so we have some extra work
		     * to do XXX *what extra work?* XXX TODO
		     */
		    if (inotify_is_root(path) != NULL) {
			LOG_DEBUG("Deleting root at path '%s'", path);
		    }
		}
	    }

	    /* Queue event */
	    if (event->mask & root->mask)
		inotify_enqueue(root, event, path);

	    free(abs_path);
	}

	i += INOTIFY_EVENT_SIZE + event->len;
    }
}

/* Add a new inotify event to its Root's queue.
 *
 * On success 0 (zero) is returned.
 * On failure 1 is returned.
 */
int inotify_enqueue(Root * root, IN_Event * event, char *path)
{
    int queue_len;
    Event *node;

    MLOCK();

    /* Check to make sure we don't overflow the queue */
    queue_len = (int) g_queue_get_length(root->queue);

    LOG_TRACE("Root '%s' has %d/%d events queued",
	      root->path, queue_len, root->max_events);

    if (queue_len >= root->max_events) {
	LOG_WARN("Queue full for root '%s' (%d). Dropping event!",
		 root->path, root->max_events);
	MUNLOCK();
	return 1;
    }

    LOG_DEBUG("Queuing event root:%s path:%s name:%s",
	      root->path, path, event->name);

    /* Create our new queue node and copy over all
     * the data fields from the event.
     */
    node = (Event *) malloc(sizeof(Event));
    node->wd = event->wd;
    node->mask = event->mask;
    node->cookie = event->cookie;
    node->len = event->len;

    asprintf(&node->name, "%s", event->name);
    asprintf(&node->path, "%s", path);

    /* Add new node to the queue. */
    g_queue_push_tail(root->queue, node);

    MUNLOCK();
    return 0;
}

/* Return a list of all the currently watched root paths. */
char **inotify_get_roots(void)
{
    int i = 0;
    char **roots;
    GList *keys;

    MLOCK();
    keys = g_hash_table_get_keys(inotify_roots);
    MUNLOCK();

    roots = malloc((g_list_length(keys) + 1) * (sizeof *roots));

    for (; keys != NULL; keys = keys->next)
	asprintf(&roots[i++], "%s", (char *) keys->data);

    roots[i] = NULL;

    g_list_free(keys);
    return roots;
}

/* Take the data structure created by the inotify_get_roots()
 * (ABOVE) and free all it's dynamically allocated memory.
 */
void inotify_free_roots(char **roots)
{
    int i;

    for (i = 0; roots[i]; free(roots[i++]));

    free(roots);
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
	    free(events[i]);
	}
    }

    free(events);
}

Event **inotify_dequeue(Root * root, int count)
{
    int i, queue_len;
    Event *e, **events;

    if (count == 0)
	LOG_DEBUG("Dequeuing *all* events from root '%s'", root->path);
    else
	LOG_DEBUG("Dequeuing %d events from root '%s'", count, root->path);

    MLOCK();

    queue_len = (int) g_queue_get_length(root->queue);

    if (queue_len == 0) {
	MUNLOCK();
	return NULL;
    }

    if (count == 0 || count > queue_len)
	count = queue_len;

    LOG_TRACE("Root '%s' has %d/%d events queued. Dequeueing %d events.",
	      root->path, queue_len, root->max_events, count);

    events = malloc((count + 1) * sizeof *events);

    for (i = 0; i < count; i++) {
	e = g_queue_pop_head(root->queue);

	LOG_DEBUG("Dequeued event root:%s path:%s name:%s",
		  root->path, e->path, e->name);

	events[i] = e;
    }
    events[i] = NULL;

    MUNLOCK();
    return events;
}

/* Given a root path return 'count' events from the
 * front of the queue, if there are any.
 */
Event **inotify_get_events(char *path, int count)
{
    Root *root;

    root = inotify_is_root(path);
    if (root == NULL) {
	LOG_WARN
	    ("Cannot get event for path '%s' since it is not a watched root'",
	     path);
	return (Event **) NULL;
    }

    return inotify_dequeue(root, count);
}

/* Given a root path grab a single event off the queue */
Event **inotify_get_event(char *path)
{
    return inotify_get_events(path, 1);
}

/* Given a path return data indicating whether or not the path
 * is a watched root.
 *
 * Since roots are stored in a hash table this is really not
 * necessary, it just provides a little syntatic sugar.
 */
Root *inotify_is_root(char *path)
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
Root *inotify_path_to_root(char *path)
{
    GList *keys;

    MLOCK();
    keys = g_hash_table_get_keys(inotify_roots);
    MUNLOCK();

    for (; keys != NULL; keys = keys->next) {

	char *tmp;
	asprintf(&tmp, "%s/", (char *) keys->data);

	if ((strcmp(path, keys->data) == 0) || strstr(path, tmp)) {
	    free(tmp);
	    Root *root;

	    MLOCK();
	    root = g_hash_table_lookup(inotify_roots, keys->data);
	    MUNLOCK();

	    if (root == NULL) {
		LOG_WARN("Failed to look up root for '%s'", keys->data);
		g_list_free(keys);
		return NULL;
	    }

	    LOG_TRACE("Found root '%s' for path '%s'", keys->data, path);
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
char *inotify_is_parent(char *path)
{
    char *tmp;
    GList *keys;

    asprintf(&tmp, "%s/", path);

    MLOCK();
    keys = g_hash_table_get_keys(inotify_roots);
    MUNLOCK();

    for (; keys != NULL; keys = keys->next) {
	if (strstr(keys->data, tmp)) {
	    free(tmp);
	    g_list_free(keys);
	    return keys->data;
	}
    }

    free(tmp);
    g_list_free(keys);

    return NULL;
}

/* Recursively unwatch a tree. This includes removing each inotify
 * watch, as well as removing entries in the meta data mappings.
 */
int inotify_unwatch_tree(char *path)
{
    int rv;
    DIR *d;
    Root *root;

    /* Clean up path by removing the trailing slash, if it exists. */
    {
	int last = strlen(path) - 1;
	if (path[last] == '/')
	    path[last] = '\0';
    }

    /* First check to see if the path is a valid,
     * watched root.
     */
    root = inotify_is_root(path);
    if (root == NULL) {
	LOG_WARN
	    ("Cannot unwatch path '%s' since it is not a watched root'",
	     path);
	return 1;
    }

    /* Next make sure we're not currently performing a recursive 
     * watch on this tree.
     */
    if (root->busy == 1) {
	LOG_WARN
	    ("Root '%s' is currently being initialized. Unwatch aborted",
	     path);
	return 1;
    }

    /* Finally check to make sure the path is a valid, open-able,
     * directory.
     */
    d = opendir(path);
    if (d == NULL) {
	LOG_WARN("Failed to open root at dir '%s': %s",
		 path, strerror(errno));
	closedir(d);
	return 1;
    }
    closedir(d);

    LOG_NOTICE("Un-watching tree at root '%s'", path);

    /* Recursively remove the inotify watches of each
     * sub directory of this root.
     */
    rv = do_unwatch_tree(path, root);
    if (rv != 0) {
	LOG_ERROR("Failed to unwatch root at dir '%s'", path);
	return 1;
    }

    return 0;
}

/* Recursively watch a tree. This involves setting up inotify watches
 * for each directory in the tree, as well as adding entries in the
 * meta data mappings.
 */
int inotify_watch_tree(char *path, int mask, int max_events)
{
    LOG_TRACE("Entering inotify_watch_tree() on path '%s' with mask %lu",
	      path, mask);

    /* Clean up path by removing the trailing slash, it exists. */
    {
	int last = strlen(path) - 1;
	if (path[last] == '/')
	    path[last] = '\0';
    }

    /* A quick check of the current state of watched roots. */
    {
	/* First we make sure we're not already watching a tree
	 * at this root. This includes a sub tree, i.e. if the
	 * root '/foo' is already being watched the user requests
	 * a watch at '/foo/bar/baz'.
	 */
	Root *r = inotify_path_to_root(path);

	if (r != NULL) {
	    LOG_WARN("Already watching tree '%s' at root '%s'",
		     path, r->path);
	    return 1;
	}

	/* Second, we check to see if the path the user is trying
	 * to watch is a parent of an already watched root, i.e.
	 * if '/foo/bar/baz' is already being wacthed and a user
	 * requests a watch at '/foo'.
	 */
	char *sub_path = inotify_is_parent(path);

	if (sub_path) {
	    LOG_WARN
		("Path '%s' is the parent of already watched root '%s'",
		 path, sub_path);
	    return 1;
	}
    }

    LOG_NOTICE("Watching new tree at root '%s'", path);

    /* Check to make sure root is a valid, and open-able, directory. */
    {
	DIR *d = opendir(path);
	if (d == NULL) {
	    LOG_ERROR("Failed to open root at dir '%s': %s",
		      path, strerror(errno));
	    return 1;
	}
	closedir(d);
    }

    /* Next we allocate space and store the meta data
     * for our new root.
     */
    Root *new_root;

    MLOCK();
    new_root = make_root(path, mask, max_events);
    g_hash_table_replace(inotify_roots, g_strdup(path), new_root);
    ++inotify_num_watched_roots;
    MUNLOCK();

    /* Finally we need to recursively setup inotify
     * watches for our new root.
     */
    do_watch_tree(new_root->path, new_root);

    /* TODO What error checking/return value goes here? */
    return 0;
}

/* Threaded portion of inotify_unwatch_tree(). */
int do_unwatch_tree(char *path, Root * root)
{
    int rc;
    pthread_t t;
    T_Data *data = (T_Data *) malloc(sizeof(T_Data));

    asprintf(&data->path, "%s", path);
    data->root = root;

    rc = pthread_create(&t, NULL, _do_unwatch_tree, (void *) data);
    if (rc) {
	LOG_ERROR("Failed to create new thread for UN-watch on '%s': %d",
		  path, rc);
	free(data->path);
	free(data);
	return 1;
    }

    return 0;
}

void *_do_unwatch_tree(void *thread_data)
{
    Root *root;
    T_Data *data;
    data = thread_data;

    root = data->root;

    /* Blow away this root's meta-data. */
    MLOCK();
    free(root->path);
    g_queue_foreach(root->queue, (GFunc) free_node_mem, NULL);
    g_queue_free(root->queue);
    MUNLOCK();

    /* Do our recursive UN-watching. */
    root->busy = 1;
    _do_unwatch_tree_rec(data->path);

    /* Blow away this root */
    MLOCK();
    g_hash_table_remove(inotify_roots, data->path);
    --inotify_num_watched_roots;
    MUNLOCK();

    /* Clean up dynamically allocated memory. */
    free(data->path);
    free(data);

    return (void *) 0;
}

/* Recursive portion of inotify_unwatch_tree(). */
void _do_unwatch_tree_rec(char *path)
{
    DIR *d;
    Watch *delete;
    struct dirent *dir;

    MLOCK();
    delete = g_hash_table_lookup(inotify_path_to_watch, path);
    MUNLOCK();

    if (delete == NULL) {
	LOG_WARN
	    ("Failed to look up watcher for path '%s' during recursive unwatch",
	     path);
	return;
    }

    LOG_TRACE("Un-watching wd:%d path:%s", delete->wd, path);

    /* Remove inotify watch and blow away meta data mappings. */
    int rv = inotify_rm_watch(inotify_fd, delete->wd);
    if (rv != 0) {
	LOG_WARN("Failed call to inotify_rm_watch on dir '%s': %s",
		 path, strerror(errno));
    }

    MLOCK();
    g_hash_table_remove(inotify_wd_to_watch, GINT_TO_POINTER(delete->wd));
    g_hash_table_remove(inotify_path_to_watch, path);
    MUNLOCK();

    free(delete->path);
    free(delete);

    d = opendir(path);
    if (d == NULL) {
	LOG_ERROR("Failed to open dir '%s': %s", path, strerror(errno));
	closedir(d);
	return;
    }

    /* XXX WARNING! dirent::d_type flags DO NOT work on XFS...
     *              (and apparently several other file systems)
     */
    while ((dir = readdir(d))) {
	if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0 || dir->d_type == DT_LNK) {	/* Skip symlinks! */
	    continue;
	}

	if (dir->d_type == DT_DIR) {
	    char *tmp;
	    asprintf(&tmp, "%s/%s", path, dir->d_name);

	    /* Recurse! */
	    _do_unwatch_tree_rec(tmp);

	    free(tmp);
	}
    }

    closedir(d);
}

/* Recursive, threaded portion of inotify_watch_tree(). */
int do_watch_tree(char *path, Root * root)
{
    int rc;
    pthread_t t;
    T_Data *data = (T_Data *) malloc(sizeof(T_Data));

    asprintf(&data->path, "%s", path);
    data->root = root;

    rc = pthread_create(&t, NULL, _do_watch_tree, (void *) data);
    if (rc) {
	LOG_ERROR("Failed to create new thread for watch on '%s': %d",
		  path, rc);
	free(data->path);
	free(data);
	return 1;
    }

    return 0;
}

void *_do_watch_tree(void *thread_data)
{
    T_Data *data;
    data = thread_data;

    data->root->busy = 1;
    _do_watch_tree_rec(data->path, data->root);
    data->root->busy = 0;

    free(data->path);
    free(data);
    return (void *) 0;
}

void _do_watch_tree_rec(char *path, Root * root)
{
    int wd;
    DIR *d;
    struct dirent *dir;
    Watch *watch;

    //wd = inotify_add_watch(inotify_fd, path, root->mask); 
    wd = inotify_add_watch(inotify_fd, path, IN_ALL_EVENTS);

    LOG_TRACE("Watching wd:%d path:%s", wd, path);

    if (wd < 0) {
	LOG_ERROR("Failed to set up inotify watch for path '%s'", path);
	return;
    }

    watch = make_watch(wd, path);

    MLOCK();
    g_hash_table_replace(inotify_wd_to_watch, GINT_TO_POINTER(wd), watch);
    g_hash_table_replace(inotify_path_to_watch, g_strdup(path), watch);
    MUNLOCK();

    d = opendir(path);
    if (d == NULL) {
	LOG_ERROR("Failed to open dir: %s", strerror(errno));
	closedir(d);
	return;
    }

    while ((dir = readdir(d))) {
	if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0 || dir->d_type == DT_LNK) {	/* Skip symlinks! */
	    continue;
	}

	if (dir->d_type == DT_DIR) {
	    char *tmp;
	    asprintf(&tmp, "%s/%s", path, dir->d_name);

	    /* Recurse! */
	    _do_watch_tree_rec(tmp, root);

	    free(tmp);
	}
    }

    closedir(d);
}

/* Create a new root meta data structure. */
Root *make_root(char *path, int mask, int max_events)
{
    Root *root = malloc(sizeof(Root));

    root->path = malloc(strlen(path) + 1);
    strcpy(root->path, path);

    root->mask = mask;
    root->queue = g_queue_new();
    root->max_events = max_events;
    root->busy = 0;
    root->persist = 0;		/* TODO: Future feature */

    g_queue_init(root->queue);

    return root;
}

/* Create a new watch structure. There will be one of these
 * for every single directory we set up an inotify watch
 * for.
 */
Watch *make_watch(int wd, char *path)
{
    size_t size;
    Watch *watch;

    size = strlen(path);
    watch = malloc(sizeof(Watch));

    watch->wd = wd;

    watch->path = malloc(size + 1);
    memcpy(watch->path, path, size);
    watch->path[size] = '\0';

    return watch;
}

/* Free up the dynamically allocated memory of a queue node. */
void free_node_mem(Event * node, gpointer user_data)
{
    free(node->path);
    free(node->name);
}
