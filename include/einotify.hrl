-ifndef (_EINOTIFY_HRL).
-define (_EINOTIFY_HRL, included).

-record (einotify, {wd, mask, cookie, name}).

% the following are legal, implemented events that user-space can watch for
-define (IN_ACCESS,        16#00000001). % File was accessed
-define (IN_MODIFY,        16#00000002). % File was modified
-define (IN_ATTRIB,        16#00000004). % Metadata changed
-define (IN_CLOSE_WRITE,   16#00000008). % Writtable file was closed
-define (IN_CLOSE_NOWRITE, 16#00000010). % Unwrittable file closed
-define (IN_OPEN,          16#00000020). % File was opened
-define (IN_MOVED_FROM,    16#00000040). % File was moved from X
-define (IN_MOVED_TO,      16#00000080). % File was moved to Y
-define (IN_CREATE,        16#00000100). % Subfile was created
-define (IN_DELETE,        16#00000200). % Subfile was deleted
-define (IN_DELETE_SELF,   16#00000400). % Self was deleted
-define (IN_MOVE_SELF,     16#00000800). % Self was moved

% the following are legal events.  they are sent as needed to any watch
-define (IN_UNMOUNT,    16#00002000). % Backing fs was unmounted
-define (IN_Q_OVERFLOW, 16#00004000). % Event queued overflowed
-define (IN_IGNORED,    16#00008000). % File was ignored

% helper events
-define (IN_CLOSE, (?IN_CLOSE_WRITE bor ?IN_CLOSE_NOWRITE)). % close
-define (IN_MOVE,  (?IN_MOVED_FROM bor ?IN_MOVED_TO)).       % moves

% special flags
-define (IN_ONLYDIR,     16#01000000). % only watch the path if it is a directory
-define (IN_DONT_FOLLOW, 16#02000000). % don't follow a sym link
-define (IN_EXCL_UNLINK, 16#04000000). % exclude events on unlinked objects
-define (IN_MASK_ADD,    16#20000000). % add to the mask of an already existing watch
-define (IN_ISDIR,       16#40000000). % event occurred against dir
-define (IN_ONESHOT,     16#80000000). % only send event once

-define (IN_ALL_EVENTS, (?IN_ACCESS bor ?IN_MODIFY bor ?IN_ATTRIB bor
                         ?IN_CLOSE_WRITE bor ?IN_CLOSE_NOWRITE bor ?IN_OPEN bor
                         ?IN_MOVED_FROM bor ?IN_MOVED_TO bor ?IN_DELETE bor
                         ?IN_CREATE bor ?IN_DELETE_SELF bor ?IN_MOVE_SELF)).

-endif. % _EINOTIFY_HRL
