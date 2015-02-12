/*  =========================================================================

    sam_log - a logging facility

    This Source Code Form is subject to the terms of the MIT
    License. If a copy of the MIT License was not distributed with
    this file, You can obtain one at http://opensource.org/licenses/MIT

    =========================================================================
*/
/**

   @brief the central logging facility
   @file sam_log.c

   This class enables fast and asynchronous logging
   constrained by different log levels. It is possible
   to define multiple or no output sources per log
   level. Logging is implemented asynchronously.


   connections:
       "[<>^v]" : connect
            "o" : bind


     controller
       |   \
       |   PIPE
       |     \        PULL   PUSH
      PIPE   actor[0] o------< sam_logger[0]
       |               \
    actor[i]            \
       o PULL           ^ PUSH
                      sam_logger[i]

*/

#include "../include/sam_prelude.h"


//  --------------------------------------------------------------------------
/// Function that decides which log handler to call.
static void
multiplex (
    sam_log_handler_t handler,
    sam_log_lvl_t lvl,
    const char *line)
{
    switch (handler) {
    case SAM_LOG_HANDLER_STD:
        sam_log_handler_std (lvl, line);
        break;
    default:
        assert (false);
    }
}


//  --------------------------------------------------------------------------
/// Returns the handler list belonging to a specific log level.
static zlist_t *
get_handler_list (
    sam_log_inner_t *state,
    sam_log_lvl_t lvl)
{
    switch (lvl) {
    case (SAM_LOG_LVL_TRACE):
        return state->handler.trace;
    case (SAM_LOG_LVL_INFO):
        return state->handler.info;
    case (SAM_LOG_LVL_ERROR):
        return state->handler.error;
    }

    assert (false);
}


//  --------------------------------------------------------------------------
/// Returns the string representation of a specific log level.
const char *
get_lvl_repr (sam_log_lvl_t lvl)
{
    switch (lvl) {
    case (SAM_LOG_LVL_TRACE):
        return SAM_LOG_LVL_TRACE_REPR;
    case (SAM_LOG_LVL_INFO):
        return SAM_LOG_LVL_INFO_REPR;
    case (SAM_LOG_LVL_ERROR):
        return SAM_LOG_LVL_ERROR_REPR;
    }

    assert (false);
}


//  --------------------------------------------------------------------------
/// Handle commands arriving on the PULL socket.
/// Currently there is only one command implemented: log.
/// The log command takes the log line, formats it according to
/// the formatting defined in the inner state. Log lines exceeding
/// SAM_LOG_LINE_MAX_SIZE are truncated.
static void
pll_handle_cmd (
    sam_log_inner_t *state,
    const char *cmd,
    sam_log_lvl_t lvl,
    zmsg_t *msg)
{

    if (!strcmp (cmd, "log")) {
        char
            line_buf[SAM_LOG_LINE_MAXSIZE + 64],
            date_buf[SAM_LOG_DATE_MAXSIZE];

        // read logger name
        char *name = zmsg_popstr (msg);
        char *raw = zmsg_popstr (msg);

        // read and convert timestamp
        zframe_t *time_frame = zmsg_pop (msg);
        assert (zmsg_size (msg) == 0);

        time_t time_curr = *(time_t *) zframe_data (time_frame);
        zframe_destroy (&time_frame);
        struct tm *time_loc = localtime (&time_curr);
        strftime (date_buf, SAM_LOG_DATE_MAXSIZE, state->date_fmt, time_loc);

        // format output string
        sprintf (
            line_buf, state->line_fmt,
            date_buf,
            16, name, // TODO maxsize configuration
            get_lvl_repr (lvl),
            SAM_LOG_LINE_MAXSIZE, raw);

        // clean up
        free (name);
        free (raw);

        // invoke handler
        zlist_t *handler_list = get_handler_list (state, lvl);
        zlist_first (handler_list);

        while (zlist_item (handler_list) != NULL) {
            sam_log_handler_t handler =
                (sam_log_handler_t) zlist_next (handler_list);
            multiplex (handler, lvl, line_buf);
        }
    }
}


//  --------------------------------------------------------------------------
/// Callback function for requests arriving on the PULL socket.
/// Expects a message with at least 3 frames:
/// | char *cmd | byte[] lvl | void *payload | [...]
static int
pll_callback (zloop_t *loop UU, zsock_t *pll, void *args)
{
    char *cmd;
    sam_log_lvl_t *lvl;
    size_t *size;
    zmsg_t *msg = zmsg_new ();
    sam_log_inner_t *state = (sam_log_inner_t *) args;

    zsock_recv (pll, "sbm", &cmd, &lvl, &size, &msg);
    pll_handle_cmd (state, cmd, *lvl, msg);

    free (cmd);
    free (lvl);
    zmsg_destroy (&msg);

    return 0;
}


//  --------------------------------------------------------------------------
/// Callback function for requests arriving on the actor's PIPE.
/// Expects at least 1 frame:
/// | char *cmd | [...]
static int
pipe_callback (zloop_t *loop UU, zsock_t *pipe, void *args)
{
    zmsg_t *msg = zmsg_recv (pipe);
    bool term = false;

    if (!msg) {
        assert (false);
    }

    char *cmd = zmsg_popstr (msg);
    sam_log_inner_t *state = (sam_log_inner_t *) args;

    if (!strcmp (cmd, "$TERM")) {
        term = true;
    }

    // add or rem handler command
    // read severity and handler reference and (un)register the
    // handler to all severities up to the provided level.
    else if (!strcmp (cmd, "add_handler") || !strcmp (cmd, "rem_handler")) {
        zframe_t *payload = zmsg_pop (msg);
        sam_log_lvl_t lvl = *(sam_log_lvl_t *) zframe_data (payload);
        zframe_destroy (&payload);

        payload = zmsg_pop (msg);
        assert (zmsg_size (msg) == 0);
        sam_log_handler_t handler = *(sam_log_handler_t *)
            zframe_data (payload);

        if (!strcmp (cmd, "add_handler")) {
            sam_log_lvl_t lvl_c = 0;
            for (; lvl_c <= lvl; lvl_c++) {
                zlist_t *handler_list = get_handler_list (state, lvl_c);
                zlist_remove (handler_list, &handler);

                // TODO &handler okay?
                int rc = zlist_push (handler_list, &handler);
                assert (rc == 0);
            }

        } else if (!strcmp (cmd, "rem_handler")) {
            sam_log_lvl_t lvl_c = lvl;
            for (; lvl_c <= SAM_LOG_LVL_TRACE; lvl_c++) {
                zlist_t *handler_list = get_handler_list (state, lvl_c);
                zlist_remove (handler_list, &handler);
            }

        }

        zframe_destroy (&payload);
    }

    free (cmd);
    zmsg_destroy (&msg);

    if (term) {
        return -1;
    }

    return 0;
}


//  --------------------------------------------------------------------------
/// Actor thread for the log facility.
/// A loop gets started that polls on the PULL socket and the actor's PIPE.
static void
actor (zsock_t *pipe, void *args)
{
    sam_log_inner_t state;
    char *endpoint = (char *) args;

    // timestamp, level, logger name, message
    state.line_fmt = "%s [%.*s] (%s): %.*s\n";
    state.date_fmt = "%T";

    state.handler.trace = zlist_new ();
    state.handler.info  = zlist_new ();
    state.handler.error = zlist_new ();

    zsock_t *pll = zsock_new_pull (endpoint);

    zloop_t *loop = zloop_new ();
    zloop_reader (loop, pipe, pipe_callback, &state);
    zloop_reader (loop, pll, pll_callback, &state);

    // initialization done
    zsock_signal (pipe, 0);
    zloop_start (loop);

    zloop_destroy (&loop);
    zsock_destroy (&pll);

    zlist_destroy (&state.handler.trace);
    zlist_destroy (&state.handler.info);
    zlist_destroy (&state.handler.error);
}


//  --------------------------------------------------------------------------
/// Create a new log facility.
/// This function starts a new thread as a zactor. It also generates a
/// unique socket endpoint for sam_logger instances to connect to if the
/// endpoint parameter is NULL. A copy of the endpoint name is held inside.
sam_log_t *
sam_log_new (char *endpoint)
{
    if (endpoint == NULL) {
        zuuid_t *id = zuuid_new ();
        char *id_str = zuuid_str (id);

        endpoint = malloc (14 + strlen (id_str));
        assert (endpoint);

        sprintf (endpoint, "inproc://log-%s", id_str);
        zuuid_destroy (&id);
    }
    else {
        endpoint = strdup (endpoint);
        assert (endpoint);
    }

    sam_log_t *log = malloc (sizeof (sam_log_t));
    assert (log);

    log->endpoint = endpoint;
    log->actor = zactor_new (actor, endpoint);

    return log;
}


//  --------------------------------------------------------------------------
/// Destroy a logger facility.
/// Please note that log messages not already handled may get lost.
/// TODO: optional synchronisation method
void
sam_log_destroy (sam_log_t **self)
{
    assert (self);

    free ((*self)->endpoint);
    zactor_destroy (&(*self)->actor);

    free (*self);
    *self = NULL;
}



//  --------------------------------------------------------------------------
/// Send a command to the log facility.
/// This function builds the typical three-framed request for the internal
/// PIPE: | char *cmd | byte[] lvl | void *payload |
static void
send_cmd (
    sam_log_t *self,
    const char *cmd,
    sam_log_lvl_t lvl,
    void *payload,
    uint payload_len)
{
    zframe_t *lvl_f = zframe_new (&lvl, sizeof (sam_log_lvl_t));
    zframe_t *pay_f = zframe_new (payload, payload_len);

    int rc = zsock_send (self->actor, "sff", cmd, lvl_f, pay_f);
    assert (rc == 0);

    zframe_destroy (&lvl_f);
    zframe_destroy (&pay_f);
}


//  --------------------------------------------------------------------------
/// Add a handler method up to a specific log level.
/// This function sends a three-framed request via the internal PIPE to
/// the log facility. This, in turn, registers the handler for all log
/// levels up to and including the provided one.
void
sam_log_add_handler (
    sam_log_t *self,
    sam_log_lvl_t lvl,
    sam_log_handler_t handler)
{
    assert (self);
    send_cmd (self, "add_handler", lvl, &handler, sizeof (sam_log_handler_t));
}


//  --------------------------------------------------------------------------
/// Removes a handler method up to a specific log level.
/// This function sends a three-framed request via the internal PIPE to
/// the log facility. This, in turn, removes all handlers for all
/// log levels up to and including the provided one.
void
sam_log_remove_handler (
    sam_log_t *self,
    sam_log_lvl_t lvl,
    sam_log_handler_t handler)
{
    assert (self);
    send_cmd (self, "rem_handler", lvl, &handler, sizeof (sam_log_handler_t));
}


//  --------------------------------------------------------------------------
/// Log handler that prints messages to std*.
/// TRACE and INFO messages are printed to stdout, ERROR to stderr.
void
sam_log_handler_std (sam_log_lvl_t lvl, const char *msg)
{
    switch (lvl) {
    case SAM_LOG_LVL_TRACE:
        fputs (msg, stdout);
        break;

    case SAM_LOG_LVL_INFO:
        fputs (msg, stdout);
        break;

    case SAM_LOG_LVL_ERROR:
        fputs (msg, stderr);
        break;
    }
}


//  --------------------------------------------------------------------------
/// Accessor method to retrieve the log facilities socket endpoint.
char *
sam_log_endpoint (sam_log_t *self)
{
    assert (self);
    return self->endpoint;
}


//  --------------------------------------------------------------------------
/// Self test this class.
void
sam_log_test (void)
{
    printf ("\n** SAM_LOG **\n");
    printf ("[log] creating logger\n");
    sam_log_t *log = sam_log_new (NULL);

    printf ("[log] appending log handler\n");
    sam_log_add_handler (log, SAM_LOG_LVL_TRACE, SAM_LOG_HANDLER_STD);

    printf ("[log] creating logger\n");
    char *endpoint = sam_log_endpoint (log);
    sam_logger_t *logger = sam_logger_new ("test", endpoint);

    printf ("[log] sending log request\n");
    sam_log_trace (logger, "1");
    sam_log_info (logger, "2");
    sam_log_error (logger, "3");
    sleep (1);

    printf ("[log] only log error level\n");
    sam_log_remove_handler (log, SAM_LOG_LVL_INFO, SAM_LOG_HANDLER_STD);
    sam_log_trace (logger, "TEST FAILED");
    sam_log_info (logger, "TEST FAILED");
    sam_log_error (logger, "4");
    sleep (1);

    printf ("[log] re-add info level (idempotency)\n");
    sam_log_add_handler (log, SAM_LOG_LVL_INFO, SAM_LOG_HANDLER_STD);
    sam_log_trace (logger, "TEST FAILED");
    sam_log_info (logger, "5");
    sam_log_error (logger, "6");
    sleep (1);

    printf ("[log] formatted output\n");
    sam_log_infof (logger, "%d %c %s", 7, '7', "7");
    sleep (1);

    printf ("[log] other log facility with custom endpoint\n");
    endpoint = "ipc://log-test";
    sam_log_t *ipc_log = sam_log_new (endpoint);
    sam_log_add_handler (ipc_log, SAM_LOG_LVL_TRACE, SAM_LOG_HANDLER_STD);

    sam_logger_destroy (&logger);
    logger = sam_logger_new ("ipc", endpoint);
    sam_log_trace (logger, "8");

    sleep (1);
    printf ("[log] uniquely counted to 8?\n");

    printf ("[log] destroying the logger\n");
    sam_logger_destroy (&logger);
    sam_log_destroy (&ipc_log);

    printf ("[log] destroying log facility\n");
    sam_log_destroy (&log);

    printf ("[log] exiting\n");
}
