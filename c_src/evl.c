/**
 * @file evl.c
 *
 * @brief Epoll simplify interface.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#define _GNU_SOURCE /* for TEMP_FAILURE_RETRY */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <zmq.h>

#include "evl.h"
#include "chain.h"

/**
 * Creates epoll file descriptor and initializes various structures.
 *
 * @param max_events  maximum number of events returned by epoll_wait()
 *
 * @param loop_start  callback function that will be called BEFORE events
 *                    processing on every wait loop iteration (may be NULL)
 *
 * @param loop_end    callback function that will be called AFTER events
 *                    processing on every wait loop iteration (may be NULL)
 *
 * @return data structure to be used by all other functions or NULL on error
 */
struct evl_inst *
evl_init (int max_events, evl_hook_fn loop_start, evl_hook_fn loop_end)
{
  int error;
  struct evl_inst *inst;

  if (max_events <= 0) {
    errno = EINVAL;
    return NULL;
  }

  inst = malloc (sizeof (*inst) + max_events * sizeof (struct epoll_event));
  if (!inst) {
    errno = ENOMEM;
    return NULL;
  }

  inst->fd = epoll_create (max_events);
  if (inst->fd < 0) {
    error = errno;
    perror ("epoll_create");
    free (inst);
    errno = error;
    return NULL;
  }

  inst->list       = NULL;
  inst->cl         = NULL;
  inst->flags      = 0;
  inst->max_events = max_events;

  inst->loop_start = loop_start;
  inst->loop_end   = loop_end;

  return inst;
}

/**
 * Frees structures for removed file descriptors.
 *
 * @param inst  data structure pointer returned by evl_init()
 */
static void
evl_cleanup (struct evl_inst *inst)
{
  struct evl_handler *eh;

  while (inst->cl) {
    eh = inst->cl;
    chain_del (inst->cl, eh);
    free (eh);
  }
}

#ifdef EVL_ZMQ
static inline void
evl_handle_z (struct evl_handler *eh, uint32_t fd_events)
{
  uint32_t events;
  do {
    uint32_t zevents;
    size_t zevents_size = sizeof (zevents);

    if (zmq_getsockopt (eh->zsocket, ZMQ_EVENTS, &zevents, &zevents_size) == -1) {
      fprintf (stderr,
               "%s: cannot get ZMQ_EVENTS on socket %p",
               __func__,
               eh->zsocket);
      return;
    }

    events = 0;

    if ((zevents & ZMQ_POLLIN) && (eh->zevents & ZMQ_POLLIN))
      events |= EPOLLIN;
    if ((zevents & ZMQ_POLLOUT) && (eh->zevents & ZMQ_POLLOUT))
      events |= EPOLLOUT;

    if (events)
      eh->fn (eh, events, eh->data);
  } while (events);
}
#endif /* EVL_ZMQ */

/**
 * Starts the event loop.
 *
 * @param inst  data structure pointer returned by evl_init()
 *
 * @return 0 on success (after evl_stop call), -1 on error
 */
int
evl_start (struct evl_inst *inst)
{
  int i, n;
  struct evl_handler *eh;

  inst->flags |= EVL_IF_RUNNING;

  do {
    n = TEMP_FAILURE_RETRY (
          epoll_wait (inst->fd, inst->events, inst->max_events, -1)
        );

    if (n < 0) {
      perror ("epoll_wait");
      return -1;
    }

    if (inst->loop_start) {
      (*inst->loop_start) (inst);
    }

    for (i = 0; i < n; ++i) {
      eh = (struct evl_handler *) inst->events[i].data.ptr;
      if (eh->flags & EVL_HF_ALIVE) {
#ifdef EVL_ZMQ
        if (eh->zsocket)
          evl_handle_z (eh, inst->events[i].events);
        else
#endif /* EVL_ZMQ */
          (*eh->fn) (eh, inst->events[i].events, eh->data);
      }
    }

    if (inst->loop_end) {
      (*inst->loop_end) (inst);
    }

    evl_cleanup (inst);
  } while (inst->flags & EVL_IF_RUNNING);

  return 0;
}

/**
 * Call this function to return from event loop.
 *
 * @param inst  data structure pointer returned by evl_init()
 */
void
evl_stop (struct evl_inst *inst)
{
  inst->flags &= ~EVL_IF_RUNNING;
}

/**
 * Close epoll file descriptor and free allocated data structures.
 *
 * @param inst  data structure pointer returned by evl_init()
 */
void
evl_destroy (struct evl_inst *inst)
{
  struct evl_handler *eh;

  close (inst->fd);

  while (inst->list) {
    eh = inst->list;
    chain_del (inst->list, eh);
    free (eh);
  }

  evl_cleanup (inst);

  free (inst);
}

struct evl_handler *
evl_add (struct evl_inst *inst,
         int fd,
         uint32_t events,
         evl_hcb_fn fun,
         void *user_data)
{
  struct epoll_event ev;
  struct evl_handler *eh = malloc (sizeof (*eh));

  if (!eh) {
    errno = ENOMEM;
    return NULL;
  }

  memset (eh, 0, sizeof (*eh));

  ev.events   = events;
  ev.data.ptr = eh;
  if (epoll_ctl (inst->fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
    int tmp = errno;
    perror ("EPOLL_CTL_ADD");
    free (eh);
    errno = tmp;
    return NULL;
  }

  eh->fd    = fd;
  eh->fn    = fun;
  eh->data  = user_data;
  eh->flags = EVL_HF_ALIVE;
  chain_add (inst->list, eh);

  return eh;
}

#ifdef EVL_ZMQ
struct evl_handler *
evl_add_z (struct evl_inst *inst,
           void *zsocket,
           uint32_t zevents,
           evl_hcb_fn fun,
           void *user_data)
{
  int fd;
  size_t fd_size = sizeof (fd);
  if (zmq_getsockopt (zsocket, ZMQ_FD, &fd, &fd_size))
    return NULL;

  struct epoll_event ev;
  struct evl_handler *eh = malloc (sizeof (*eh));

  if (!eh) {
    errno = ENOMEM;
    return NULL;
  }

  memset (eh, 0, sizeof (*eh));

  ev.events   = zevents ? EPOLLIN : 0;
  ev.data.ptr = eh;
  if (epoll_ctl (inst->fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
    int tmp = errno;
    perror ("EPOLL_CTL_ADD");
    free (eh);
    errno = tmp;
    return NULL;
  }

  eh->fd      = fd;
  eh->zsocket = zsocket;
  eh->fn      = fun;
  eh->zevents = zevents;
  eh->data    = user_data;
  eh->flags   = EVL_HF_ALIVE;
  chain_add (inst->list, eh);

  return eh;
}
#endif /* EVL_ZMQ */

struct evl_handler *
evl_get (struct evl_inst *inst, int fd)
{
  struct evl_handler *eh;

  chain_for_each (inst->list, eh) {
    if (eh->fd == fd) {
      return eh;
    }
  }

  errno = ENOENT;
  return NULL;
}

int
evl_mod (struct evl_inst *inst, struct evl_handler *eh, uint32_t events)
{
  struct epoll_event ev;

  ev.events   = events;
  ev.data.ptr = eh;

  if (epoll_ctl (inst->fd, EPOLL_CTL_MOD, eh->fd, &ev) < 0) {
    perror ("EPOLL_CTL_MOD");
    return -1;
  }

  return 0;
}

int
evl_mod_fd (struct evl_inst *inst, int fd, uint32_t events)
{
  struct evl_handler *eh = evl_get (inst, fd);

  if (!eh) {
    return -1;
  }

  return evl_mod (inst, eh, events);
}

void
evl_del (struct evl_inst *inst, struct evl_handler *eh)
{
  if (epoll_ctl (inst->fd, EPOLL_CTL_DEL, eh->fd, NULL) < 0) {
    perror ("EPOLL_CTL_DEL");
  }

  eh->flags &= ~EVL_HF_ALIVE;
  chain_del (inst->list, eh);
  chain_add (inst->cl, eh);
}

int
evl_del_fd (struct evl_inst *inst, int fd)
{
  struct evl_handler *eh = evl_get (inst, fd);

  if (!eh) {
    return -1;
  }

  evl_del (inst, eh);

  return 0;
}
