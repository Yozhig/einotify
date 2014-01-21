#define _GNU_SOURCE /* for TEMP_FAILURE_RETRY */

#include <assert.h>
#include <ei.h>
#include <erl_driver.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "control.h"
#include "evl.h"
#include "log.h"
#include "watch.h"

static struct evl_handler *eh = NULL;

/* control commands */
enum {
    CMD_ADD_WATCH = 0,
    CMD_RM_WATCH,
    CMD_MAX
};

/* control callback prototype */
typedef void (control_func) (const char *buf, int idx);
/* array of control callbacks (defined below) */
static control_func *const funcs[];

/* receive buffer size */
#define RBUF_SZ 2048
/* receive buffer */
static char rbuf[RBUF_SZ];
/* send buffer size */
#define SBUF_SZ 2048
/* send buffer */
static char sbuf[SBUF_SZ];

/* Reads exact `count' bytes */
static ssize_t
read_exact (int fd, void *buf, size_t count)
{
    size_t rem = count;

    while (rem > 0) {
        ssize_t r = TEMP_FAILURE_RETRY (read (fd, buf, rem));
        if (r == -1)
            return -1;
        rem -= r;
    }

    return count;
}

static void
encode_tuple (void *buf, int *idx, const char *atom)
{
    int rc;
    rc = ei_encode_version (buf, idx);
    assert (rc == 0);
    rc = ei_encode_tuple_header (buf, idx, 2);
    assert (rc == 0);
    rc = ei_encode_atom (buf, idx, atom);
    assert (rc == 0);
}

static void
do_write (void *buf, uint16_t count)
{
    int fd = fileno (stdout);
    uint16_t len = htobe16 (count);

    ssize_t sent = TEMP_FAILURE_RETRY (write (fd, &len, sizeof (len)));
    if (sent == -1)
        perror ("write");
    assert (sent == sizeof (len));

    sent = TEMP_FAILURE_RETRY (write (fd, buf, count));
    assert (sent == count);
}

static void
_reply_badarg (void)
{
    int rc, idx = 0;

    encode_tuple (sbuf, &idx, "error");
    rc = ei_encode_atom (sbuf, &idx, "badarg");
    assert (rc == 0);

    do_write (sbuf, idx);
}

#define reply_badarg() \
do { \
    DEBUG ("%s: badarg", __func__); \
    _reply_badarg (); \
} while (0)

static void
reply_error (int code)
{
    int rc, idx = 0;

    encode_tuple (sbuf, &idx, "error");
    rc = ei_encode_long (sbuf, &idx, code);
    assert (rc == 0);

    do_write (sbuf, idx);
}

static void
reply_add (int wfd)
{
    int rc, idx = 0;
    encode_tuple (sbuf, &idx, "ok");
    rc = ei_encode_long (sbuf, &idx, wfd);
    assert (rc == 0);

    do_write (sbuf, idx);
}

static void
reply_ok (void)
{
    int rc, idx = 0;
    rc = ei_encode_version (sbuf, &idx);
    assert (rc == 0);
    rc = ei_encode_atom (sbuf, &idx, "ok");
    assert (rc == 0);

    do_write (sbuf, idx);
}

static inline void
handle_msg (const char *rbuf, uint16_t len)
{
    int idx = 0;
    int tmp;
    unsigned long cmd;

    if (ei_decode_version (rbuf, &idx, &tmp)
        || ei_decode_tuple_header (rbuf, &idx, &tmp)
        || tmp != 2
        || ei_decode_ulong (rbuf, &idx, &cmd)
        || cmd >= CMD_MAX) {
        reply_badarg ();
        return;
    }

    funcs[cmd] (rbuf, idx);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static inline void
flush (int fd, size_t count)
{
    size_t rem = count;
    while (rem > 0) {
        ssize_t n = read_exact (fd, rbuf, MIN (RBUF_SZ, rem));
        assert (n > 0);
        rem -= n;
    }
}

static inline void
receive (int fd)
{
    uint16_t len;
    ssize_t n;

    n = read_exact (fd, &len, sizeof (len));
    assert (n == sizeof (len));
    len = be16toh (len);

    //assert (len <= RBUF_SZ);
    if (len > RBUF_SZ) {
        flush (fd, len);
        reply_badarg ();
        return;
    }

    n = read_exact (fd, rbuf, len);
    assert (n > 0);

    handle_msg (rbuf, len);
}

static void
control_handler (struct evl_handler *eh, uint32_t events, void *_nil)
{
    if (events & EPOLLIN) {
        receive (eh->fd);
    }

    if (events & (EPOLLERR | EPOLLHUP)) {
        exit (EXIT_SUCCESS);
    }
}

void
control_init (struct evl_inst *loop)
{
    assert (eh == NULL);
    eh = evl_add (loop, fileno (stdin), EPOLLIN, &control_handler, NULL);
    assert (eh != NULL);
}

void
control_notify (int wd, uint32_t mask, uint32_t cookie, const char *name, uint32_t len)
{
    assert (eh != NULL);

    int rc, idx = 0;

    rc = ei_encode_version (sbuf, &idx);
    assert (rc == 0);
    rc = ei_encode_tuple_header (sbuf, &idx, 5);
    assert (rc == 0);

    rc = ei_encode_atom (sbuf, &idx, "einotify");
    assert (rc == 0);
    rc = ei_encode_ulong (sbuf, &idx, wd);
    assert (rc == 0);
    rc = ei_encode_ulong (sbuf, &idx, mask);
    assert (rc == 0);
    rc = ei_encode_ulong (sbuf, &idx, cookie);
    assert (rc == 0);
    if (len)
        rc = ei_encode_string (sbuf, &idx, name);
    else
        rc = ei_encode_empty_list (sbuf, &idx);
    assert (rc == 0);

    do_write (sbuf, idx);
}

/******************************************************************************/

static void
add_watch (const char *buf, int idx)
{
    int ar, tp, sz;
    unsigned long mask;

    if (ei_decode_tuple_header (buf, &idx, &ar)
        || ar != 2
        || ei_get_type (buf, &idx, &tp, &sz)) { // FIXME: check tp
        reply_badarg ();
        return;
    }

    char *f = malloc (sz + 1);
    assert (f != NULL);

    if (ei_decode_string (buf, &idx, f)
        || ei_decode_ulong (buf, &idx, &mask)) {
        free (f);
        reply_badarg ();
        return;
    }

    int wfd = watch_add (f, mask);
    if (wfd == -1) {
        reply_error (errno);
    } else {
        reply_add (wfd);
    }

    free (f);
}

static void
rm_watch (const char *buf, int idx)
{
    unsigned long wfd;

    if (ei_decode_ulong (buf, &idx, &wfd)) {
        reply_badarg ();
        return;
    }

    int rc = watch_rm (wfd);
    if (rc == 0) {
        reply_ok ();
    } else {
        reply_error (errno);
    }
}

/******************************************************************************/

static control_func *const funcs[] = {
    [CMD_ADD_WATCH] = &add_watch,
    [CMD_RM_WATCH]  = &rm_watch,
};
