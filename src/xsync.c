/*      $Id$

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2, or (at your option)
        any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., Inc., 51 Franklin Street, Fifth Floor, Boston,
        MA 02110-1301, USA.


        xfwm4    - (c) 2002-2011 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xsync.h"

#ifdef HAVE_XSYNC
gboolean
getXSyncCounter (DisplayInfo *display_info, Window window, XSyncCounter *counter)
{
    long val;

    g_return_val_if_fail (window != None, FALSE);
    g_return_val_if_fail (counter != NULL, FALSE);
    TRACE ("entering getXSyncCounter");

    val = 0;
    if (getHint (display_info, window, NET_WM_SYNC_REQUEST_COUNTER, &val))
    {
        *counter = (XSyncCounter) val;
        return TRUE;
    }

    return FALSE;
}

void
sendXSyncRequest (DisplayInfo *display_info, Window window, XSyncValue value)
{
    XClientMessageEvent xev;

    g_return_if_fail (window != None);
    TRACE ("entering sendXSyncRequest");

    xev.type = ClientMessage;
    xev.window = window;
    xev.message_type = display_info->atoms[WM_PROTOCOLS];
    xev.format = 32;
    xev.data.l[0] = (long) display_info->atoms[NET_WM_SYNC_REQUEST];
    xev.data.l[1] = (long) myDisplayGetCurrentTime (display_info);
    xev.data.l[2] = (long) XSyncValueLow32 (value);
    xev.data.l[3] = (long) XSyncValueHigh32 (value);
    xev.data.l[4] = (long) 0L;
    XSendEvent (display_info->dpy, window, FALSE, NoEventMask, (XEvent *) &xev);
}

void
clientIncrementXSyncValue (Client *c)
{
    XSyncValue add;
    int overflow;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->xsync_counter != None);

    TRACE ("entering clientIncrementXSyncValue");

    XSyncIntToValue (&add, 1);
    XSyncValueAdd (&c->xsync_value, c->xsync_value, add, &overflow);
}

gboolean
clientCreateXSyncAlarm (Client *c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    XSyncAlarmAttributes values;

    g_return_val_if_fail (c != NULL, FALSE);
    g_return_val_if_fail (c->xsync_counter != None, FALSE);

    TRACE ("entering clientCreateXSyncAlarm");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    XSyncIntToValue (&c->xsync_value, 0);
    XSyncSetCounter (display_info->dpy, c->xsync_counter, c->xsync_value);
    XSyncIntToValue (&values.trigger.wait_value, 1);
    XSyncIntToValue (&values.delta, 1);

    values.trigger.counter = c->xsync_counter;
    values.trigger.value_type = XSyncAbsolute;
    values.trigger.test_type = XSyncPositiveComparison;
    values.events = True;

    c->xsync_alarm = XSyncCreateAlarm (display_info->dpy,
                                       XSyncCACounter |
                                       XSyncCADelta |
                                       XSyncCAEvents |
                                       XSyncCATestType |
                                       XSyncCAValue |
                                       XSyncCAValueType,
                                       &values);
    return (c->xsync_alarm != None);
}

void
clientDestroyXSyncAlarm (Client *c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->xsync_alarm != None);

    TRACE ("entering clientDestroyXSyncAlarm");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    XSyncDestroyAlarm (display_info->dpy, c->xsync_alarm);
    c->xsync_alarm = None;
}

void
clientXSyncClearTimeout (Client * c)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientXSyncClearTimeout");

    if (c->xsync_timeout_id)
    {
        g_source_remove (c->xsync_timeout_id);
        c->xsync_timeout_id = 0;
    }
}

static gboolean
clientXSyncTimeout (gpointer data)
{
    Client *c;
    XWindowChanges wc;

    TRACE ("entering clientXSyncTimeout");

    c = (Client *) data;
    if (c)
    {
        TRACE ("XSync timeout for client \"%s\" (0x%lx)", c->name, c->window);
        clientXSyncClearTimeout (c);
        c->xsync_waiting = FALSE;
        c->xsync_enabled = FALSE;

        wc.x = c->x;
        wc.y = c->y;
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, NO_CFG_FLAG);
    }
    return (TRUE);
}

static void
clientXSyncResetTimeout (Client * c)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientXSyncResetTimeout");

    clientXSyncClearTimeout (c);
    c->xsync_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                              CLIENT_XSYNC_TIMEOUT,
                                              (GtkFunction) clientXSyncTimeout,
                                              (gpointer) c, NULL);
}

void
clientXSyncRequest (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    TRACE ("entering clientXSyncRequest");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    clientIncrementXSyncValue (c);
    sendXSyncRequest (display_info, c->window, c->xsync_value);
    clientXSyncResetTimeout (c);
    c->xsync_waiting = TRUE;
}

gboolean
clientXSyncEnable (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientXSyncEnable");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    c->xsync_enabled = FALSE;
    if (display_info->have_xsync)
    {
        if ((c->xsync_counter) && (c->xsync_alarm))
        {
            c->xsync_enabled = TRUE;
        }
    }
    return (c->xsync_enabled);
}
#endif /* HAVE_XSYNC */
