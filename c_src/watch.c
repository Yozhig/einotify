#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#define _GNU_SOURCE /* for TEMP_FAILURE_RETRY */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "control.h"
#include "log.h"
#include "watch.h"

static struct evl_handler *eh = NULL;

static void
watch_handler (struct evl_handler *eh, uint32_t _events, void *_nil)
{
    unsigned int sz = 0;

    if (ioctl (eh->fd, FIONREAD, &sz) == -1) {
        ERR ("ioctl (FIONREAD): %s", strerror (errno));
        return;
    }

    if (sz == 0) {
        return;
    }

    void *buf = malloc (sz); // FIXME: may be allocate static buffer for this
    assert (buf);

    int rc = TEMP_FAILURE_RETRY (read (eh->fd, buf, sz));
    //DEBUG ("%s: read event: %d", __func__, rc);
    assert (rc > 0);

    struct inotify_event *event = buf;

    while ((void*) event < buf + sz) {
        /*if (event->mask & IN_UNMOUNT) {
            // FIXME: what should we do on umount? (file is now ignored by inotify)
            ERR ("%s: Backing FS was unmounted", __func__);
        }*/

        /*if (event->mask & IN_Q_OVERFLOW) {
            ERR ("%s: Queue Overflow", __func__);
            // TODO: rescan directory to be sure that no events have been lost
        }*/

        /*if (event->mask & IN_IGNORED) {
            ERR ("%s: File was ignored", __func__);
        }*/

        control_notify (event->wd, event->mask, event->cookie, event->name, event->len);

        event = (void *) event + sizeof (*event) + event->len;
    }

    free (buf);
}

int
watch_init (struct evl_inst *loop)
{
    assert (eh == NULL);

    int ifd = inotify_init1 (IN_NONBLOCK | IN_CLOEXEC);
    if (ifd == -1) {
        ERR ("inotify_init1: %s", strerror (errno));
        return -1;
    }

    eh = evl_add (loop, ifd, EPOLLIN, &watch_handler, NULL);
    assert (eh != NULL);

    return 0;
}

void
watch_destroy (struct evl_inst *loop)
{
    if (eh != NULL) {
        evl_del (loop, eh);
        TEMP_FAILURE_RETRY (close (eh->fd));
        eh = NULL;
    }
}

int
watch_add (const char *path, uint32_t mask)
{
    assert (eh != NULL);

    int fd = inotify_add_watch (eh->fd, path, mask);
    if (fd == -1) {
        int tmp = errno;
        ERR ("inotify_add_watch: %s", strerror (errno));
        errno = tmp;
        return -1;
    }

    return fd;
}

int
watch_rm (int wfd)
{
    assert (eh != NULL);

    return inotify_rm_watch (eh->fd, wfd);
}
