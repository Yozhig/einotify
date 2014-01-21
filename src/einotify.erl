-module (einotify).

%% API
-export ([ new/0
         , add_watch/3
         , rm_watch/2
         , close/1
         ]).

%% gen_server callbacks
-export ([ init/1
         , handle_call/3
         , handle_cast/2
         , handle_info/2
         , terminate/2
         , code_change/3
         ]).

-record (s, { owner
            , port
            , queue
            }).

-include ("einotify.hrl").

-define (cmd_add_watch, 0).
-define (cmd_rm_watch,  1).

-define (
    dbg (F, A),
    io:format (standard_error, ?MODULE_STRING ":~w: " F "~n", [?LINE | A])
).

-define (dbg (F), ?dbg (F, [])).

-type flag() :: access | modify | attrib | close_write | close_nowrite | open |
                moved_from | moved_to | create | delete | delete_self |
                move_self | close | move | onlydir | dont_follow | excl_unlink |
                mask_add | oneshot | all_events.


%%==============================================================================
%% API
%%==============================================================================

-spec new () -> {ok, Pid :: pid()}.
%% Starts inotify instance. Returns a pid to be used in subsequent calls.
new () ->
    gen_server:start_link (?MODULE, self (), []).

-spec add_watch (Pid :: pid(), Filename :: string(), Flags :: integer() | [flag()]) ->
        {ok, Fd :: integer()} | {error, Code :: integer()}.
%% Adds or modifies a watch for the file or directory.
add_watch (Pid, Filename, Mask) when is_integer (Mask) ->
    call (Pid, {request, {?cmd_add_watch, {Filename, Mask}}});

add_watch (Pid, Filename, Flags) ->
    call (Pid, {request, {?cmd_add_watch, {Filename, flags (Flags)}}}).

-spec rm_watch (Pid :: pid(), Fd :: integer()) -> ok | {error, Code :: integer()}.
%% Removes a watch for the file or directory.
rm_watch (Pid, Fd) ->
    call (Pid, {request, {?cmd_rm_watch, Fd}}).

-spec close (Pid :: pid()) -> ok.
%% Stops inotify instance.
close (Pid) ->
    call (Pid, stop).


%%==============================================================================
%% gen_server callbacks
%%==============================================================================

init (Owner) ->
    monitor (process, Owner),
    Opts = [ {packet, 2}
           , exit_status
           , use_stdio
           , binary
           , {parallelism, true}
           ],
    Port = open_port ({spawn_executable, port ()}, Opts),
    {ok, #s{ owner = Owner
           , port  = Port
           , queue = queue:new ()
           }}.

handle_call ({request, Req}, From, #s{port = P, queue = Q} = State) ->
    port_command (P, term_to_binary (Req)),
    {noreply, State#s{queue = queue:in (From, Q)}};

handle_call (stop, _From, State) ->
    {stop, normal, ok, State};

handle_call (_Request, _From, State) ->
    {noreply, State}.

handle_cast (_Msg, State) ->
    {noreply, State}.

handle_info ({P, {data, Data}}, #s{owner = Owner, port = P, queue = Q} = State) ->
    Msg = binary_to_term (Data),
    case Msg of
        #einotify{} ->
            Owner ! Msg,
            {noreply, State};
        _ -> % this is reply to the command
            {{value, Client}, Q2} = queue:out (Q),
            gen_server:reply (Client, Msg),
            {noreply, State#s{queue = Q2}}
    end;

handle_info ({P, {exit_status, S}}, #s{port = P} = State) ->
    {stop, {port_closed, S}, State};

handle_info ({'DOWN', _Ref, process, Owner, _Reason}, #s{owner = Owner} = State) ->
    {stop, normal, State};

handle_info (_Info, State) ->
    ?dbg ("unhandled info: ~p", [_Info]),
    {noreply, State}.

terminate (_Reason, #s{port = P} = _State) ->
    port_close (P).

code_change (_OldVsn, State, _Extra) ->
    {ok, State}.


%%==============================================================================
%% Internal functions
%%==============================================================================

app () ->
    case application:get_application (?MODULE) of
        {ok, App} -> App;
        _         -> einotify
    end.

priv () ->
    case code:priv_dir (app ()) of
        {error, _} -> "priv";
        Priv       -> Priv
    end.

port () ->
    filename:join (priv (), ?MODULE_STRING).

call (Pid, Msg) ->
    gen_server:call (Pid, Msg, infinity).

%%------------------------------------------------------------------------------

-spec flags (Flags :: [flag()]) -> Mask :: integer().
flags (Flags) ->
    flags (Flags, 0).

flags ([F | T], Mask) ->
    flags (T, Mask bor flag (F));
flags ([], Mask) ->
    Mask.

-spec flag (Flag :: flag()) -> integer().
flag (access)        -> ?IN_ACCESS;
flag (modify)        -> ?IN_MODIFY;
flag (attrib)        -> ?IN_ATTRIB;
flag (close_write)   -> ?IN_CLOSE_WRITE;
flag (close_nowrite) -> ?IN_CLOSE_NOWRITE;
flag (open)          -> ?IN_OPEN;
flag (moved_from)    -> ?IN_MOVED_FROM;
flag (moved_to)      -> ?IN_MOVED_TO;
flag (create)        -> ?IN_CREATE;
flag (delete)        -> ?IN_DELETE;
flag (delete_self)   -> ?IN_DELETE_SELF;
flag (move_self)     -> ?IN_MOVE_SELF;

flag (close)         -> ?IN_CLOSE;
flag (move)          -> ?IN_MOVE;

flag (onlydir)       -> ?IN_ONLYDIR;
flag (dont_follow)   -> ?IN_DONT_FOLLOW;
flag (excl_unlink)   -> ?IN_EXCL_UNLINK;
flag (mask_add)      -> ?IN_MASK_ADD;
flag (isdir)         -> ?IN_ISDIR;
flag (oneshot)       -> ?IN_ONESHOT;

flag (all_events)    -> ?IN_ALL_EVENTS.
