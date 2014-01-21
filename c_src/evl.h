#ifndef _EVL_H
#define _EVL_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdint.h>
#include <sys/epoll.h>

//#define EVL_ZMQ
#undef EVL_ZMQ /* FIXME: move it to config.h */

struct evl_handler;

typedef void (evl_hcb_fn) (struct evl_handler *eh,
                           uint32_t events,
                           void *user_data);

struct evl_handler {
  /* chain */
  struct evl_handler *prev;
  struct evl_handler *next;
  /* file descriptor */
  int                fd;
#ifdef EVL_ZMQ
  /* zmq socket */
  void               *zsocket;
  /* zmq events */
  uint32_t           zevents;
#endif /* EVL_ZMQ */
  /* callback function */
  evl_hcb_fn         *fn;
  /* callback argument */
  void               *data;
  /* flags */
  int                flags;
};

#define EVL_HF_ALIVE (1 << 0)

struct evl_inst;

typedef void (evl_hook_fn) (struct evl_inst *);

struct evl_inst {
  evl_hook_fn        *loop_start;
  evl_hook_fn        *loop_end;
  /* handlers list */
  struct evl_handler *list;
  /* cleanup list */
  struct evl_handler *cl;
  /* epoll fd */
  int                fd;
  int                flags;
  int                max_events;
  struct epoll_event events[0]; /* variable sized array (see evl_init) */
};

#define EVL_IF_RUNNING (1 << 0)

extern struct evl_inst *
evl_init (int max_events, evl_hook_fn loop_start, evl_hook_fn loop_end);

extern int
evl_start (struct evl_inst *inst);

extern void
evl_stop (struct evl_inst *inst);

extern void
evl_destroy (struct evl_inst *inst);

extern struct evl_handler *
evl_add (struct evl_inst *inst,
         int fd,
         uint32_t events,
         evl_hcb_fn fun,
         void *user_data);

#ifdef EVL_ZMQ
extern struct evl_handler *
evl_add_z (struct evl_inst *inst,
           void *zsocket,
           uint32_t zevents,
           evl_hcb_fn fun,
           void *user_data);
#endif /* EVL_ZMQ */

extern struct evl_handler *
evl_get (struct evl_inst *inst, int fd);

extern int
evl_mod (struct evl_inst *inst, struct evl_handler *eh, uint32_t events);

extern int
evl_mod_fd (struct evl_inst *inst, int fd, uint32_t events);

extern void
evl_del (struct evl_inst *inst, struct evl_handler *eh);

extern int
evl_del_fd (struct evl_inst *inst, int fd);

#endif /* _EVL_H */
