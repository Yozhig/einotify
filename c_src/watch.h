#ifndef _WATCH_H
#define _WATCH_H

#include "evl.h"

extern int
watch_init (struct evl_inst *loop);

extern void
watch_destroy (struct evl_inst *loop);

extern int
watch_add (const char *path, uint32_t mask);

extern int
watch_rm (int wfd);

#endif /* _WATCH_H */
