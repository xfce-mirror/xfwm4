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


        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2008 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "client.h"
#include "focus.h"
#include "frame.h"
#include "settings.h"
#include "stacking.h"
#include "tabwin.h"
#include "transients.h"
#include "wireframe.h"
#include "workspaces.h"
#include "event_filter.h"

typedef struct _ClientCycleData ClientCycleData;
struct _ClientCycleData
{
    Client *c;
    Tabwin *tabwin;
    Window wireframe;
    int cycle_range;
};

static eventFilterStatus
clientCycleEventFilter (XEvent * xevent, gpointer data)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    ClientCycleData *passdata;
    Client *c, *removed;
    eventFilterStatus status;
    KeyCode cycle;
    KeyCode cancel;
    int modifier;
    gboolean key_pressed, cycling, gone;

    TRACE ("entering clientCycleEventFilter");

    passdata = (ClientCycleData *) data;
    if (passdata->c == NULL)
    {
        return EVENT_FILTER_CONTINUE;
    }

    c = passdata->c;
    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    cycle = screen_info->params->keys[KEY_CYCLE_WINDOWS].keycode;
    cancel = screen_info->params->keys[KEY_CANCEL].keycode;
    modifier = screen_info->params->keys[KEY_CYCLE_WINDOWS].modifier;
    key_pressed = ((xevent->type == KeyPress) && ((xevent->xkey.keycode == cycle) ||
                                                  (xevent->xkey.keycode == cancel)));
    status = EVENT_FILTER_STOP;
    cycling = TRUE;
    gone = FALSE;

    /* Update the display time */
    myDisplayUpdateCurrentTime (display_info, xevent);

    switch (xevent->type)
    {
        case DestroyNotify:
            removed = myScreenGetClientFromWindow (screen_info, ((XDestroyWindowEvent *) xevent)->window, SEARCH_WINDOW);
            gone |= (c == removed);
            c = tabwinRemoveClient(passdata->tabwin, removed);
            passdata->c = c;
            status = EVENT_FILTER_CONTINUE;
            /* Walk through */
        case UnmapNotify:
            removed = myScreenGetClientFromWindow (screen_info, ((XUnmapEvent *) xevent)->window, SEARCH_WINDOW);
            gone |= (c == removed);
            c = tabwinRemoveClient(passdata->tabwin, removed);
            passdata->c = c;
            status = EVENT_FILTER_CONTINUE;
            /* Walk through */
        case KeyPress:
            if (gone || key_pressed)
            {
                if (key_pressed)
                {
                    Client *c2 = NULL;

                    if (xevent->xkey.keycode == cancel)
                    {
                        c2 = tabwinGetHead (passdata->tabwin);
                        cycling = FALSE;
                    }
                    /* If KEY_CYCLE_WINDOWS has Shift, then do not reverse */
                    else if (!(modifier & ShiftMask) && (xevent->xkey.state & ShiftMask))
                    {
                        TRACE ("Cycle: previous");
                        c2 = tabwinSelectPrev(passdata->tabwin);
                    }
                    else
                    {
                        TRACE ("Cycle: next");
                        c2 = tabwinSelectNext(passdata->tabwin);
                    }
                    if (c2)
                    {
                        c = c2;
                        passdata->c = c;
                    }

                    /* If last key press event had not our modifier pressed, finish cycling */
                    if (!(xevent->xkey.state & modifier))
                    {
                        cycling = FALSE;
                    }
                }

                if (cycling)
                {
                    if (c)
                    {
                        if (passdata->wireframe)
                        {
                            wireframeUpdate (c, passdata->wireframe);
                        }
                    }
                    else
                    {
                        cycling = FALSE;
                    }
                }
            }
            break;
        case KeyRelease:
            {
                int keysym = XLookupKeysym (&xevent->xkey, 0);

                if (!(xevent->xkey.state & modifier) ||
                    (IsModifierKey(keysym) && (keysym != XK_Shift_L) && (keysym != XK_Shift_R)))
                {
                    cycling = FALSE;
                }
            }
            break;
        case ButtonPress:
        case ButtonRelease:
        case EnterNotify:
        case MotionNotify:
            break;
        default:
            status = EVENT_FILTER_CONTINUE;
            break;
    }

    if (!cycling)
    {
        TRACE ("event loop now finished");
        gtk_main_quit ();
    }

    return status;
}

void
clientCycle (Client * c, XKeyEvent * ev)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    ClientCycleData passdata;
    gboolean g1, g2;
    int key;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientCycle");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    g1 = myScreenGrabKeyboard (screen_info, ev->time);
    g2 = myScreenGrabPointer (screen_info, LeaveWindowMask,  None, ev->time);

    if (!g1 || !g2)
    {
        TRACE ("grab failed in clientCycle");

        gdk_beep ();
        myScreenUngrabKeyboard (screen_info, ev->time);
        myScreenUngrabPointer (screen_info, ev->time);

        return;
    }

    if (screen_info->params->cycle_hidden)
    {
        passdata.cycle_range = INCLUDE_HIDDEN;
    }
    else
    {
        passdata.cycle_range = 0;
    }
    if (!screen_info->params->cycle_minimum)
    {
        passdata.cycle_range |= INCLUDE_SKIP_TASKBAR | INCLUDE_SKIP_PAGER;
    }
    if (screen_info->params->cycle_workspaces)
    {
        passdata.cycle_range |= INCLUDE_ALL_WORKSPACES;
    }
    key = myScreenGetKeyPressed (screen_info, ev);
    if (key == KEY_CYCLE_REVERSE_WINDOWS)
    {
        passdata.c = clientGetPrevious (c, passdata.cycle_range);
    }
    else
    {
        passdata.c = clientGetNext (c, passdata.cycle_range);
    }
    passdata.wireframe = None;

    /* If there is one single client, and if it's eligible for focus, use it */
    if ((passdata.c == NULL) && (c != clientGetFocus()) &&
        clientSelectMask (c, passdata.cycle_range, WINDOW_REGULAR_FOCUSABLE))
    {
        passdata.c = c;
    }

    if (passdata.c)
    {
        TRACE ("entering cycle loop");
        if (screen_info->params->cycle_draw_frame)
        {
            passdata.wireframe = wireframeCreate (passdata.c);
        }
        passdata.tabwin = tabwinCreate (passdata.c->screen_info->gscr, c,
                                        passdata.c, passdata.cycle_range,
                                        screen_info->params->cycle_workspaces);
        eventFilterPush (display_info->xfilter, clientCycleEventFilter, &passdata);
        gtk_main ();
        eventFilterPop (display_info->xfilter);
        TRACE ("leaving cycle loop");
        tabwinDestroy (passdata.tabwin);
        g_free (passdata.tabwin);
        if (passdata.wireframe)
        {
            wireframeDelete (screen_info, passdata.wireframe);
        }
        updateXserverTime (display_info);
    }

    if (passdata.c)
    {
        Client *focused;
        Client *sibling;
        int workspace;

        c = passdata.c;
        workspace = c->win_workspace;
        focused = clientGetFocus ();

        if (workspace != screen_info->current_ws)
        {
            workspaceSwitch (screen_info, workspace, c, FALSE, myDisplayGetCurrentTime (display_info));
        }

        if ((focused) && (passdata.c != focused))
        {
            clientClearAllShowDesktop (screen_info);
            clientAdjustFullscreenLayer (focused, FALSE);
        }

        sibling = clientGetTransientFor(c);
        clientRaise (sibling, None);
        clientShow (sibling, TRUE);
        clientSetFocus (screen_info, c, myDisplayGetCurrentTime (display_info), NO_FOCUS_FLAG);
    }

    myScreenUngrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
    myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));
}
