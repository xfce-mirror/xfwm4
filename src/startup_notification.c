/*
        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; You may only use version 2 of the License,
        you have no option to use any other version.
 
        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.
 
        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
        Metacity - (c) 2003 Havoc Pennington
        xfwm4    - (c) 2003 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <libsn/sn.h>

#include "client.h"
#include "main.h"

#define STARTUP_TIMEOUT (30 /* seconds */ * 1000)

SnDisplay *sn_display;
SnMonitorContext *sn_context;
GSList *startup_sequences;
guint startup_sequence_timeout;

typedef struct
{
    GSList *list;
    GTimeVal now;
}
CollectTimedOutData;

static gboolean sn_startup_sequence_timeout (void *data);

static void
sn_error_trap_push (SnDisplay * sn_display, Display * dpy)
{
    g_return_if_fail (sn_display != NULL);
    g_return_if_fail (sn_context != NULL);

    gdk_error_trap_push ();
}

static void
sn_error_trap_pop (SnDisplay * sn_display, Display * dpy)
{
    g_return_if_fail (sn_display != NULL);
    g_return_if_fail (sn_context != NULL);

    gdk_error_trap_pop ();
}

static void
sn_update_feedback (void)
{
    g_return_if_fail (sn_display != NULL);
    g_return_if_fail (sn_context != NULL);

    if (startup_sequences != NULL)
    {
        XDefineCursor (dpy, root, busy_cursor);
    }
    else
    {
        XDefineCursor (dpy, root, root_cursor);
    }
}

static void
sn_add_sequence (SnStartupSequence * sequence)
{
    g_return_if_fail (sn_display != NULL);
    g_return_if_fail (sn_context != NULL);
    g_return_if_fail (sequence != NULL);

    sn_startup_sequence_ref (sequence);
    startup_sequences = g_slist_prepend (startup_sequences, sequence);

    if (startup_sequence_timeout == 0)
    {
        startup_sequence_timeout =
            g_timeout_add (1000, sn_startup_sequence_timeout, NULL);
    }
    sn_update_feedback ();
}

static void
sn_remove_sequence (SnStartupSequence * sequence)
{
    g_return_if_fail (sn_display != NULL);
    g_return_if_fail (sn_context != NULL);
    g_return_if_fail (sequence != NULL);

    startup_sequences = g_slist_remove (startup_sequences, sequence);
    sn_startup_sequence_unref (sequence);

    if ((startup_sequences == NULL) && (startup_sequence_timeout != 0))
    {
        g_source_remove (startup_sequence_timeout);
        startup_sequence_timeout = 0;
    }
    sn_update_feedback ();
}

static void
sn_collect_timed_out_foreach (void *element, void *data)
{
    CollectTimedOutData *ctod = data;
    SnStartupSequence *sequence = element;
    long tv_sec, tv_usec;
    double elapsed;

    g_return_if_fail (sn_display != NULL);
    g_return_if_fail (sn_context != NULL);

    sn_startup_sequence_get_last_active_time (sequence, &tv_sec, &tv_usec);

    elapsed =
        ((((double) ctod->now.tv_sec - tv_sec) * G_USEC_PER_SEC +
            (ctod->now.tv_usec - tv_usec))) / 1000.0;

    if (elapsed > STARTUP_TIMEOUT)
    {
        ctod->list = g_slist_prepend (ctod->list, sequence);
    }
}

static gboolean
sn_startup_sequence_timeout (void *data)
{
    CollectTimedOutData ctod;
    GSList *tmp;

    g_return_val_if_fail (sn_display != NULL, FALSE);
    g_return_val_if_fail (sn_context != NULL, FALSE);

    ctod.list = NULL;
    g_get_current_time (&ctod.now);
    g_slist_foreach (startup_sequences, sn_collect_timed_out_foreach, &ctod);

    tmp = ctod.list;
    while (tmp != NULL)
    {
        SnStartupSequence *sequence = tmp->data;

        sn_startup_sequence_complete (sequence);

        tmp = tmp->next;
    }

    g_slist_free (ctod.list);

    if (startup_sequences != NULL)
    {
        return TRUE;
    }
    else
    {
        /* remove */
        startup_sequence_timeout = 0;
        return FALSE;
    }
}

static void
sn_screen_event (SnMonitorEvent * event, void *user_data)
{
    const char *wmclass;
    SnStartupSequence *sequence;

    g_return_if_fail (sn_display != NULL);
    g_return_if_fail (sn_context != NULL);
    g_return_if_fail (event != NULL);

    sequence = sn_monitor_event_get_startup_sequence (event);

    switch (sn_monitor_event_get_type (event))
    {
        case SN_MONITOR_EVENT_INITIATED:
            wmclass = sn_startup_sequence_get_wmclass (sequence);
            sn_add_sequence (sequence);
            break;

        case SN_MONITOR_EVENT_COMPLETED:
            sn_remove_sequence (sn_monitor_event_get_startup_sequence
                (event));
            break;

        case SN_MONITOR_EVENT_CHANGED:
            break;

        case SN_MONITOR_EVENT_CANCELED:
        default:
            break;
    }
}

void
sn_client_startup_properties (Client * c)
{
    char *startup_id = NULL;
    GSList *tmp = NULL;
    SnStartupSequence *sequence;

    g_return_if_fail (sn_display != NULL);
    g_return_if_fail (sn_context != NULL);

    startup_id = clientGetStartupId (c);

    sequence = NULL;
    if (startup_id == NULL)
    {
        tmp = startup_sequences;
        while (tmp != NULL)
        {
            const char *wmclass;

            wmclass = sn_startup_sequence_get_wmclass (tmp->data);

            if ((wmclass != NULL) && ((c->class.res_class
                        && !strcmp (wmclass, c->class.res_class))
                    || (c->class.res_name
                        && !strcmp (wmclass, c->class.res_name))))
            {
                sequence = tmp->data;

                free (c->startup_id);
                c->startup_id =
                    strdup (sn_startup_sequence_get_id (sequence));
                startup_id = c->startup_id;

                sn_startup_sequence_complete (sequence);
                break;
            }

            tmp = tmp->next;
        }
    }

    if (startup_id == NULL)
    {
        return;
    }

    if (sequence == NULL)
    {
        tmp = startup_sequences;
        while (tmp != NULL)
        {
            const char *id;

            id = sn_startup_sequence_get_id (tmp->data);

            if (!strcmp (id, startup_id))
            {
                sequence = tmp->data;
                break;
            }

            tmp = tmp->next;
        }
    }

    if (sequence != NULL)
    {
        int workspace;

        if (!FLAG_TEST (c->flags, CLIENT_FLAG_WORKSPACE_SET))
        {
            workspace = sn_startup_sequence_get_workspace (sequence);
            if (workspace >= 0)
            {
                FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
                c->win_workspace = workspace;
            }
        }

        return;
    }
}

void
sn_init_display (Display * dpy, int screen)
{
    sn_display = NULL;
    sn_context = NULL;

    g_return_if_fail (dpy != NULL);

    sn_display = sn_display_new (dpy, sn_error_trap_push, sn_error_trap_pop);
    if (sn_display != NULL)
    {
        sn_context =
            sn_monitor_context_new (sn_display, screen, sn_screen_event, NULL,
            NULL);
    }
    startup_sequences = NULL;
    startup_sequence_timeout = 0;
}

void
sn_close_display (void)
{
    if (sn_display)
    {
        sn_display_unref (sn_display);
    }
    sn_display = NULL;
}

void
sn_process_event (XEvent * event)
{
    g_return_if_fail (sn_display != NULL);
    sn_display_process_event (sn_display, event);
}

#endif
