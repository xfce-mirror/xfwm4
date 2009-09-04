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
        xfwm4    - (c) 2002-2009 Olivier Fourdan

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
    KeyCode cancel;
    int key, modifier;
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
    cancel = screen_info->params->keys[KEY_CANCEL].keycode;
    modifier = screen_info->params->keys[KEY_CYCLE_WINDOWS].modifier;
    status = EVENT_FILTER_STOP;
    removed = NULL;
    cycling = TRUE;
    gone = FALSE;

    /* Update the display time */
    myDisplayUpdateCurrentTime (display_info, xevent);

    switch (xevent->type)
    {
        case DestroyNotify:
            if ((removed = myScreenGetClientFromWindow (screen_info, ((XDestroyWindowEvent *) xevent)->window, SEARCH_WINDOW)) == NULL)
                break; /* No need to go any further */
            gone |= (c == removed);
            /* Walk through */
        case UnmapNotify:
            if (!removed && (removed = myScreenGetClientFromWindow (screen_info, ((XUnmapEvent *) xevent)->window, SEARCH_WINDOW)) == NULL)
                break; /* No need to go any further */
            gone |= (c == removed);
            c = tabwinRemoveClient(passdata->tabwin, removed);
            passdata->c = c;
            status = EVENT_FILTER_CONTINUE;
            /* Walk through */
        case KeyPress:
            key_pressed = (xevent->type == KeyPress);
            if (gone || key_pressed)
            {
                if (key_pressed)
                {
                    Client *c2 = NULL;
                    key = myScreenGetKeyPressed (screen_info, (XKeyEvent *) xevent);
                    /*
                     * We cannot simply check for key == KEY_CANCEL here because of the
                     * mofidier being pressed, so we need to look at the keycode directly.
                     */
                    if (xevent->xkey.keycode == cancel)
                    {
                        c2 = tabwinGetHead (passdata->tabwin);
                        cycling = FALSE;
                    }
                    else if (key == KEY_CYCLE_REVERSE_WINDOWS)
                    {
                        TRACE ("Cycle: previous");
                        c2 = tabwinSelectPrev(passdata->tabwin);
                    }
                    else if (key == KEY_CYCLE_WINDOWS)
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
        myScreenUngrabKeyboard (screen_info, CurrentTime);
        myScreenUngrabPointer (screen_info, CurrentTime);

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
        passdata.tabwin = tabwinCreate (c, passdata.c, passdata.cycle_range,
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

        if ((focused) && (passdata.c != focused))
        {
            /* We might be able to avoid this if we are about to switch workspace */
            clientAdjustFullscreenLayer (focused, FALSE);
        }
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_WAS_SHOWN))
        {
            /* We are explicitely activating a window that was shown before show-desktop */
            clientClearAllShowDesktop (screen_info);
        }
        if (workspace != screen_info->current_ws)
        {
            workspaceSwitch (screen_info, workspace, c, FALSE, myDisplayGetCurrentTime (display_info));
        }

        sibling = clientGetTransientFor(c);
        clientRaise (sibling, None);
        clientShow (sibling, TRUE);
        clientSetFocus (screen_info, c, myDisplayGetCurrentTime (display_info), NO_FOCUS_FLAG);
        clientSetLastRaise (c);
    }

    /*
     * Use CurrentTime instead of actual last event time to make sure
     * that the grab is released in any case.
     */
    myScreenUngrabKeyboard (screen_info, CurrentTime);
    myScreenUngrabPointer (screen_info, CurrentTime);
}
