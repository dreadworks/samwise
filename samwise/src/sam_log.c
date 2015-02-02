/*  =========================================================================

    sam_log - a logging facility

    Copyright (c) 2015 XING AG, Felix Hamann

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

*/

#include "../include/sam_prelude.h"


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

        // read log line
        zframe_t *payload = zmsg_pop (msg);
        char *raw  = zframe_strdup (payload);
        zframe_destroy (&payload);

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
            get_lvl_repr (lvl),
            date_buf,
            SAM_LOG_LINE_MAXSIZE, raw);

        // invoke handler
        zlist_t *handler_list = get_handler_list (state, lvl);
        zlist_first (handler_list);
        
        while (zlist_item (handler_list) != NULL) {
            sam_log_handler_t handler =
                (sam_log_handler_t) zlist_next (handler_list);
            multiplex (handler, lvl, line_buf);
        }

        free (raw);
    }
}


static int
pll_callback (zloop_t *loop, zsock_t *pll, void *args)
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


static int
pipe_callback (zloop_t *loop, zsock_t *pipe, void *args)
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

    // add handler command
    // read severity and handler reference and register the handler
    // to all severities up to the provided level.
    else if (!strcmp (cmd, "add_handler")) {
        zframe_t *payload = zmsg_pop (msg);
        sam_log_lvl_t lvl = *(sam_log_lvl_t *) zframe_data (payload);
        zframe_destroy (&payload);

        payload = zmsg_pop (msg);
        assert (zmsg_size (msg) == 0);
        sam_log_handler_t handler = *(sam_log_handler_t *)
            zframe_data (payload);

        sam_log_lvl_t lvl_c = 0;
        for (; lvl_c <= lvl; lvl_c++) {
            zlist_t *handler_list = get_handler_list (state, lvl_c);
            zlist_push (handler_list, &handler);   // TODO &handler okay?
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



static void
actor (zsock_t *pipe, void *args)
{
    sam_log_inner_t state;
    char *endpoint = (char *) args;

    state.line_fmt = "[%s] %s | %.*s\n";
    state.date_fmt = "%T";

    state.handler.trace = zlist_new ();
    state.handler.info  = zlist_new ();
    state.handler.error = zlist_new ();

    zsock_t *pll = zsock_new_pull (endpoint);

    zloop_t *loop = zloop_new ();
    zloop_reader (loop, pll, pll_callback, &state);
    zloop_reader (loop, pipe, pipe_callback, &state);

    // initialization done
    zsock_signal (pipe, 0);
    zloop_start (loop);

    zloop_destroy (&loop);
    zsock_destroy (&pll);

    zlist_destroy (&state.handler.trace);
    zlist_destroy (&state.handler.info);
    zlist_destroy (&state.handler.error);
}


sam_log_t *
sam_log_new ()
{
    zuuid_t *id = zuuid_new ();
    char *id_str = zuuid_str (id);

    sam_log_t *log = malloc (sizeof (sam_log_t));
    assert (log);

    char *endpoint = malloc (14 + strlen (id_str));
    assert (endpoint);
    sprintf (endpoint, "inproc://log-%s", id_str);

    log->endpoint = endpoint;
    log->actor = zactor_new (actor, endpoint);

    zuuid_destroy (&id);
    return log;
}


void
sam_log_destroy (sam_log_t **log)
{
    assert (log);

    free ((*log)->endpoint);
    zactor_destroy (&(*log)->actor);

    free (*log);
    *log = NULL;
}


void
sam_log (
    sam_logger_t *logger,
    sam_log_lvl_t lvl,
    const char *line)
{
    sam_logger_send (logger, lvl, line);
}


void
sam_log_add_handler (
    sam_log_t *log,
    sam_log_lvl_t lvl,
    sam_log_handler_t handler)
{
    assert (log);

    zframe_t *lvl_frame =
        zframe_new (&lvl, sizeof (sam_log_lvl_t));

    zframe_t *handler_frame =
        zframe_new (&handler, sizeof (sam_log_handler_t));

    int rc = zsock_send (
        log->actor,
        "sff",
        "add_handler",
        lvl_frame,
        handler_frame);

    assert (rc == 0);
    zframe_destroy (&lvl_frame);
    zframe_destroy (&handler_frame);
}


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


char *
sam_log_endpoint (sam_log_t *log)
{
    assert (log);
    return log->endpoint;
}


void
sam_log_test (void)
{
    printf ("\n** SAM_LOG **\n");
    printf ("[main] creating logger\n");
    sam_log_t *log = sam_log_new ();

    printf ("[main] appending log handler\n");
    sam_log_add_handler (log, SAM_LOG_LVL_TRACE, SAM_LOG_HANDLER_STD);

    char *endpoint = sam_log_endpoint (log);
    printf ("[main] creating logger for %s\n", endpoint);
    sam_logger_t *logger = sam_logger_new (endpoint);

    printf ("[main] sending log request\n");
    sam_log (logger, SAM_LOG_LVL_TRACE, "trace test");
    sam_log (logger, SAM_LOG_LVL_INFO, "info test");
    sam_log (logger, SAM_LOG_LVL_ERROR, "error test");

    printf ("[main] destroying the logger\n");
    sam_logger_destroy (&logger);

    printf ("[main] destroying log facility\n");
    sam_log_destroy (&log);

    printf ("[main] exiting\n");
}
