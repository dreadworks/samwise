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
log_thread_log (
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
log_thread_handle_cmd (
    sam_log_inner_t *state,
    const char *cmd,
    zmsg_t *msg)
{

    zframe_t *payload   = zmsg_pop (msg);
    sam_log_lvl_t lvl = *(sam_log_lvl_t *) zframe_data (payload);
    zframe_destroy (&payload);
    payload = zmsg_pop (msg);

    if (!strcmp (cmd, "add_handler")) {
        assert (zmsg_size (msg) == 0);

        sam_log_handler_t handler = *(sam_log_handler_t *)
            zframe_data (payload);

        sam_log_lvl_t lvl_c = 0;
        for (; lvl_c <= lvl; lvl_c++) {
            zlist_t *handler_list = get_handler_list (state, lvl_c);
            zlist_push (handler_list, &handler);   // TODO &handler okay?
        }

    }
    else if (!strcmp (cmd, "log")) {
        zframe_t *time_frame = zmsg_pop (msg);
        assert (zmsg_size (msg) == 0);

        char
            line_buf[SAM_LOG_LINE_MAXSIZE + 64],
            date_buf[SAM_LOG_DATE_MAXSIZE];

        char *raw  = zframe_strdup (payload);

        // create timestamp
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
            log_thread_log (handler, lvl, line_buf);
        }

        free (raw);
    }

    zframe_destroy (&payload);
}


static void
log_thread (zsock_t *pipe, void *args)
{
    sam_log_inner_t state;

    state.line_fmt = "[%s] %s | %.*s\n";
    state.date_fmt = "%T";

    state.handler.trace = zlist_new ();
    state.handler.info  = zlist_new ();
    state.handler.error = zlist_new ();

    zsock_signal (pipe, 0);

    bool terminated = false;
    while (!terminated) {
        zmsg_t *msg = zmsg_recv (pipe);

        if (!msg) {
            break; // interrupted
        }

        char *cmd = zmsg_popstr (msg);

        if (!strcmp (cmd, "$TERM")) {
            zlist_destroy (&state.handler.trace);
            zlist_destroy (&state.handler.info);
            zlist_destroy (&state.handler.error);
            terminated = true;
        }
        else {
            log_thread_handle_cmd (&state, cmd, msg);
        }

        zmsg_destroy (&msg);
        free (cmd);
    }
}


sam_log_t *
sam_log_new ()
{
    sam_log_t *logger = malloc (sizeof (sam_log_t));
    logger->log_thread = zactor_new (log_thread, NULL);
    return logger;
}


void
sam_log_destroy (sam_log_t **logger)
{
    zactor_destroy (&(*logger)->log_thread);
    free (*logger);
    *logger = NULL;
}


static zmsg_t *
prepare_cmd_msg (
    const char *cmd,
    sam_log_lvl_t lvl,
    void *payload,
    const int len)
{
    zmsg_t *msg = zmsg_new ();
    zframe_t *frame = zframe_new (cmd, strlen(cmd));
    zmsg_append (msg, &frame);

    frame = zframe_new (&lvl, sizeof (sam_log_lvl_t));
    zmsg_append (msg, &frame);

    frame = zframe_new (payload, len);
    zmsg_append (msg, &frame);

    return msg;

}


void
sam_log (
    sam_log_t *logger,
    sam_log_lvl_t lvl,
    char *line)
{
    zmsg_t *msg = prepare_cmd_msg("log", lvl, line, strlen (line));

    time_t time_curr = time (NULL);
    zframe_t *timestamp = zframe_new (&time_curr, sizeof (time_t));
    zmsg_append (msg, &timestamp);

    zactor_send (logger->log_thread, &msg);
}


void
sam_log_add_handler (
    sam_log_t *logger,
    sam_log_lvl_t lvl,
    sam_log_handler_t handler)
{
    int len = sizeof (sam_log_handler_t);
    zmsg_t *msg = prepare_cmd_msg("add_handler", lvl, &handler, len);
    zactor_send (logger->log_thread, &msg);
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


void
sam_log_test (void)
{
    printf ("\n** SAM_LOG **\n");
    printf ("[main] creating logger\n");
    sam_log_t *logger = sam_log_new ();

    printf ("[main] appending log handler\n");
    sam_log_add_handler (
        logger, SAM_LOG_LVL_TRACE, SAM_LOG_HANDLER_STD);

    printf ("[main] sending log request\n");
    sam_log (logger, SAM_LOG_LVL_TRACE, "trace test");
    sam_log (logger, SAM_LOG_LVL_INFO, "info test");
    sam_log (logger, SAM_LOG_LVL_ERROR, "error test");

    printf ("[main] destroying logger\n");
    sam_log_destroy (&logger);

    printf ("[main] exiting\n");
}
