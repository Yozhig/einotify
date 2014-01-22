#include <assert.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "control.h"
#include "evl.h"
#include "log.h"
#include "watch.h"

#define MAX_EVENTS 10

int
main (int argc, char *argv[])
{
    struct evl_inst *loop;

    log_init (0, 1);

    loop = evl_init (MAX_EVENTS, NULL, NULL);
    assert (loop);

    int rc = watch_init (loop);
    assert (rc == 0);
    control_init (loop);

    evl_start (loop);

    evl_destroy (loop);

    return 0;
}
