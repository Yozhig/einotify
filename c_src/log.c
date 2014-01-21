#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stddef.h>

#include "log.h"

void
log_init (int daemon, int debug)
{
  openlog (NULL, LOG_CONS | (daemon ? 0 : LOG_PERROR), LOG_DAEMON);
  setlogmask (LOG_UPTO (debug ? LOG_DEBUG : LOG_INFO));
}
