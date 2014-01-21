#ifndef _CONTROL_H
#define _CONTROL_H

#include "evl.h"

extern void
control_init (struct evl_inst *loop);

extern void
control_notify (int wd, uint32_t mask, uint32_t cookie, const char *name, uint32_t len);

#endif /* _CONTROL_H */
