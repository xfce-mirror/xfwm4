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
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h> 
#include <libxfcegui4/libxfcegui4.h>

#include "screen.h"
#include "netwm.h"
#include "misc.h"
#include "client.h"
#include "frame.h"
#include "hints.h"
#include "workspaces.h"
#include "transients.h"
#include "stacking.h"

void
clientSetNetState (Client * c)
{
    int i;
    Atom data[16];

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetNetState");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    i = 0;
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        TRACE ("clientSetNetState : shaded");
        data[i++] = net_wm_state_shaded;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        TRACE ("clientSetNetState : sticky");
        data[i++] = net_wm_state_sticky;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL))
    {
        TRACE ("clientSetNetState : modal");
        data[i++] = net_wm_state_modal;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_PAGER))
    {
        TRACE ("clientSetNetState : skip_pager");
        data[i++] = net_wm_state_skip_pager;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_TASKBAR))
    {
        TRACE ("clientSetNetState : skip_taskbar");
        data[i++] = net_wm_state_skip_taskbar;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
    {
        TRACE ("clientSetNetState : maximize vert + horiz");
        data[i++] = net_wm_state_maximized_horz;
        data[i++] = net_wm_state_maximized_vert;
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ))
    {
        TRACE ("clientSetNetState : maximize horiz");
        data[i++] = net_wm_state_maximized_horz;
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT))
    {
        TRACE ("clientSetNetState : vert");
        data[i++] = net_wm_state_maximized_vert;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE ("clientSetNetState : fullscreen");
        data[i++] = net_wm_state_fullscreen;
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
    {
        TRACE ("clientSetNetState : above");
        data[i++] = net_wm_state_above;
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
    {
        TRACE ("clientSetNetState : below");
        data[i++] = net_wm_state_below;
    }
    if (!FLAG_TEST (c->flags, CLIENT_FLAG_VISIBLE))
    {
        TRACE ("clientSetNetState : hidden");
        data[i++] = net_wm_state_hidden;
    }
    XChangeProperty (clientGetXDisplay (c), c->window, net_wm_state, XA_ATOM, 32,
        PropModeReplace, (unsigned char *) data, i);
    /*
       We also set GNOME hint here for consistency and convenience, 
       although the meaning of net_wm_state and win_state aren't the same.
     */
    setHint (clientGetXDisplay (c), c->window, win_state, c->win_state);
}

void
clientGetNetState (Client * c)
{
    int n_atoms = 0;
    Atom *atoms = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGetNetState");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_SESSION_MANAGED))
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
	{
            TRACE ("clientGetNetState : shaded from session management");
            c->win_state |= WIN_STATE_SHADED;
            FLAG_SET (c->flags, CLIENT_FLAG_SHADED);
	}
        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
	{
            TRACE ("clientGetNetState : sticky from session management");
            c->win_state |= WIN_STATE_STICKY;
            FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
	}
        if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ))
	{
            TRACE ("clientGetNetState : maximized horiz from session management");
            c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
	}
        if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT))
	{
            TRACE ("clientGetNetState : maximized vert from session management");
            c->win_state |= WIN_STATE_MAXIMIZED_VERT;
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
	}
    }
    
    if (getAtomList (clientGetXDisplay (c), c->window, net_wm_state, &atoms, &n_atoms))
    {
        int i;
        TRACE ("clientGetNetState: %i atoms detected", n_atoms);

        i = 0;
        while (i < n_atoms)
        {
            if ((atoms[i] == net_wm_state_shaded))
            {
                TRACE ("clientGetNetState : shaded");
                c->win_state |= WIN_STATE_SHADED;
                FLAG_SET (c->flags, CLIENT_FLAG_SHADED);
            }
            else if ((atoms[i] == net_wm_state_sticky))
            {
                TRACE ("clientGetNetState : sticky");
                c->win_state |= WIN_STATE_STICKY;
                FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
            }
            else if ((atoms[i] == net_wm_state_maximized_horz))
            {
                TRACE ("clientGetNetState : maximized horiz");
                c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
                FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
            }
            else if ((atoms[i] == net_wm_state_maximized_vert))
            {
                TRACE ("clientGetNetState : maximized vert");
                c->win_state |= WIN_STATE_MAXIMIZED_VERT;
                FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
            }
            else if ((atoms[i] == net_wm_state_fullscreen))
            {
	        if (!FLAG_TEST_ALL (c->flags, CLIENT_FLAG_ABOVE | CLIENT_FLAG_BELOW))
		{
                    TRACE ("clientGetNetState : fullscreen");
                    FLAG_SET (c->flags, CLIENT_FLAG_FULLSCREEN);
                }
	    }
            else if ((atoms[i] == net_wm_state_above))
            {
	        if (!FLAG_TEST_ALL (c->flags, CLIENT_FLAG_FULLSCREEN | CLIENT_FLAG_BELOW))
		{
                    TRACE ("clientGetNetState : above");
                    FLAG_SET (c->flags, CLIENT_FLAG_ABOVE);
		}
            }
            else if ((atoms[i] == net_wm_state_below))
            {
	        if (!FLAG_TEST_ALL (c->flags, CLIENT_FLAG_ABOVE | CLIENT_FLAG_FULLSCREEN))
		{
                    TRACE ("clientGetNetState : below");
                    FLAG_SET (c->flags, CLIENT_FLAG_BELOW);
		}
            }
            else if (atoms[i] == net_wm_state_modal)
            {
                TRACE ("clientGetNetState : modal");
                FLAG_SET (c->flags, CLIENT_FLAG_STATE_MODAL);
            }
            else if (atoms[i] == net_wm_state_skip_pager)
            {
                TRACE ("clientGetNetState : skip_pager");
                FLAG_SET (c->flags, CLIENT_FLAG_SKIP_PAGER);
            }
            else if (atoms[i] == net_wm_state_skip_taskbar)
            {
                TRACE ("clientGetNetState : skip_taskbar");
                FLAG_SET (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            }
            else
            {
                g_message (_("%s: Unmanaged net_wm_state (window 0x%lx)"),
                    g_get_prgname (), c->window);
            }

            ++i;
        }
        if (atoms)
        {
            XFree (atoms);
        }
    }
}

void
clientUpdateNetState (Client * c, XClientMessageEvent * ev)
{
    unsigned long action;
    Atom first;
    Atom second;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateNetState");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    action = ((XEvent *) ev)->xclient.data.l[0];
    first = ((XEvent *) ev)->xclient.data.l[1];
    second = ((XEvent *) ev)->xclient.data.l[2];

    if ((first == net_wm_state_shaded) || (second == net_wm_state_shaded))
    {
        if ((action == NET_WM_STATE_ADD)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            clientShade (c);
        }
        else if ((action == NET_WM_STATE_REMOVE)
            && FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            clientUnshade (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            clientToggleShaded (c);
        }
    }

    if ((first == net_wm_state_sticky) || (second == net_wm_state_sticky))
    {
        if (!clientIsTransientOrModal (c)
            && FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STICK))
        {
            if ((action == NET_WM_STATE_ADD)
                && !FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
            {
                clientStick (c, TRUE);
            }
            else if ((action == NET_WM_STATE_REMOVE)
                && FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
            {
                clientUnstick (c, TRUE);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                clientToggleSticky (c, TRUE);
            }
            frameDraw (c, FALSE, FALSE);
        }
    }

    if ((first == net_wm_state_maximized_horz)
        || (second == net_wm_state_maximized_horz)
        || (first == net_wm_state_maximized_vert)
        || (second == net_wm_state_maximized_vert))
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
        {
            if ((action == NET_WM_STATE_ADD)
                && !FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
            {
                unsigned long mode = 0;
                if ((first == net_wm_state_maximized_horz)
                    || (second == net_wm_state_maximized_horz))
                {
                    mode |=
                        !FLAG_TEST (c->flags,
                        CLIENT_FLAG_MAXIMIZED_HORIZ) ?
                        WIN_STATE_MAXIMIZED_HORIZ : 0;
                }
                if ((first == net_wm_state_maximized_vert)
                    || (second == net_wm_state_maximized_vert))
                {
                    mode |=
                        !FLAG_TEST (c->flags,
                        CLIENT_FLAG_MAXIMIZED_VERT) ? WIN_STATE_MAXIMIZED_VERT
                        : 0;
                }
                clientToggleMaximized (c, mode);
            }
            else if ((action == NET_WM_STATE_REMOVE)
                && FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
            {
                unsigned long mode = 0;
                if ((first == net_wm_state_maximized_horz)
                    || (second == net_wm_state_maximized_horz))
                {
                    mode |=
                        FLAG_TEST (c->flags,
                        CLIENT_FLAG_MAXIMIZED_HORIZ) ?
                        WIN_STATE_MAXIMIZED_HORIZ : 0;
                }
                if ((first == net_wm_state_maximized_vert)
                    || (second == net_wm_state_maximized_vert))
                {
                    mode |=
                        FLAG_TEST (c->flags,
                        CLIENT_FLAG_MAXIMIZED_VERT) ? WIN_STATE_MAXIMIZED_VERT
                        : 0;
                }
                clientToggleMaximized (c, mode);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                unsigned long mode = 0;
                if ((first == net_wm_state_maximized_horz)
                    || (second == net_wm_state_maximized_horz))
                {
                    mode |= WIN_STATE_MAXIMIZED_HORIZ;
                }
                if ((first == net_wm_state_maximized_vert)
                    || (second == net_wm_state_maximized_vert))
                {
                    mode |= WIN_STATE_MAXIMIZED_VERT;
                }
                clientToggleMaximized (c, mode);
            }
        }
    }

    if ((first == net_wm_state_modal) || (second == net_wm_state_modal))
    {
        if ((action == NET_WM_STATE_ADD)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState (c);
            clientWindowType (c);
        }
        else if ((action == NET_WM_STATE_REMOVE)
            && FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState (c);
            clientWindowType (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            FLAG_TOGGLE (c->flags, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState (c);
            clientWindowType (c);
        }
        frameDraw (c, TRUE, FALSE);
    }

    if ((first == net_wm_state_fullscreen)
        || (second == net_wm_state_fullscreen))
    {
        if (!clientIsTransientOrModal (c))
        {
            if ((action == NET_WM_STATE_ADD)
                && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
            {
                FLAG_SET (c->flags, CLIENT_FLAG_FULLSCREEN);
                clientUpdateFullscreenState (c);
            }
            else if ((action == NET_WM_STATE_REMOVE)
                && FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
            {
                FLAG_UNSET (c->flags, CLIENT_FLAG_FULLSCREEN);
                clientUpdateFullscreenState (c);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_FULLSCREEN);
                clientUpdateFullscreenState (c);
            }
        }
    }

    if ((first == net_wm_state_above) || (second == net_wm_state_above))
    {
        if (!clientIsTransientOrModal (c) && !FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
        {
            if ((action == NET_WM_STATE_ADD)
                && !FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
            {
                FLAG_SET (c->flags, CLIENT_FLAG_ABOVE);
                clientUpdateAboveState (c);
            }
            else if ((action == NET_WM_STATE_REMOVE)
                && FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
            {
                FLAG_UNSET (c->flags, CLIENT_FLAG_ABOVE);
                clientUpdateAboveState (c);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_ABOVE);
                clientUpdateAboveState (c);
            }
        }
    }

    if ((first == net_wm_state_below) || (second == net_wm_state_below))
    {
        if (!clientIsTransientOrModal (c) && !FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
        {
            if ((action == NET_WM_STATE_ADD)
                && !FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
            {
                FLAG_SET (c->flags, CLIENT_FLAG_BELOW);
                clientUpdateBelowState (c);
            }
            else if ((action == NET_WM_STATE_REMOVE)
                && FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
            {
                FLAG_UNSET (c->flags, CLIENT_FLAG_BELOW);
                clientUpdateBelowState (c);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_BELOW);
                clientUpdateBelowState (c);
            }
        }
    }

    if ((first == net_wm_state_skip_pager)
        || (second == net_wm_state_skip_pager))
    {
        if ((action == NET_WM_STATE_ADD)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_PAGER))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState (c);
        }
        else if ((action == NET_WM_STATE_REMOVE)
            && FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_PAGER))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            FLAG_TOGGLE (c->flags, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState (c);
        }
    }

    if ((first == net_wm_state_skip_taskbar)
        || (second == net_wm_state_skip_taskbar))
    {
        if ((action == NET_WM_STATE_ADD)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_TASKBAR))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState (c);
        }
        else if ((action == NET_WM_STATE_REMOVE)
            && FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_TASKBAR))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            FLAG_TOGGLE (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState (c);
        }
    }
}

void
clientUpdateFullscreenState (Client * c)
{
    XWindowChanges wc;
    int layer;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateFullscreenState");
    TRACE ("Update fullscreen state for client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        GdkRectangle rect;
        gint monitor_nbr;
        int cx, cy;

        cx = frameX (c) + (frameWidth (c) / 2);
        cy = frameY (c) + (frameHeight (c) / 2);

        monitor_nbr = gdk_screen_get_monitor_at_point (c->screen_info->gscr, cx, cy);
        gdk_screen_get_monitor_geometry (c->screen_info->gscr, monitor_nbr, &rect);

        c->fullscreen_old_x = c->x;
        c->fullscreen_old_y = c->y;
        c->fullscreen_old_width = c->width;
        c->fullscreen_old_height = c->height;
        c->fullscreen_old_layer = c->win_layer;

        wc.x = rect.x;
        wc.y = rect.y;
        wc.width = rect.width;
        wc.height = rect.height;
        layer = WIN_LAYER_ABOVE_DOCK;
    }
    else
    {
        wc.x = c->fullscreen_old_x;
        wc.y = c->fullscreen_old_y;
        wc.width = c->fullscreen_old_width;
        wc.height = c->fullscreen_old_height;
        layer = c->fullscreen_old_layer;
    }
    clientSetNetState (c);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
    {
        /* 
           For some reason, the configure can generate EnterNotify events
           on lower windows, causing a nasty race cond with apps trying to
           grab focus in focus follow mouse mode. Grab the pointer to
           avoid these effects
         */
        XGrabPointer (clientGetXDisplay (c), c->screen_info->gnome_win, FALSE, EnterWindowMask, GrabModeAsync,
                           GrabModeAsync, None, None, GDK_CURRENT_TIME);
        clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, NO_CFG_FLAG);
        XUngrabPointer (clientGetXDisplay (c), GDK_CURRENT_TIME);
    }
    else
    {
        c->x = wc.x;
        c->y = wc.y;
        c->height = wc.height;
        c->width = wc.width;
    }
    clientSetLayer (c, layer);
}

void
clientGetNetWmType (Client * c)
{
    int n_atoms = 0;
    Atom *atoms = NULL;
    int i;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGetNetWmType");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    n_atoms = 0;
    atoms = NULL;

    if (!getAtomList (clientGetXDisplay (c), c->window, net_wm_window_type, &atoms, &n_atoms))
    {
        switch (c->win_layer)
        {
            case WIN_LAYER_DESKTOP:
                c->type_atom = net_wm_window_type_desktop;
                break;
            case WIN_LAYER_DOCK:
                c->type_atom = net_wm_window_type_dock;
                break;
            case WIN_LAYER_NORMAL:
                c->type_atom = net_wm_window_type_normal;
                break;
            default:
                c->type_atom = None;
                break;
        }
    }
    else
    {
        i = 0;
        while (i < n_atoms)
        {
            if ((atoms[i] == net_wm_window_type_desktop)
                || (atoms[i] == net_wm_window_type_dock)
                || (atoms[i] == net_wm_window_type_toolbar)
                || (atoms[i] == net_wm_window_type_menu)
                || (atoms[i] == net_wm_window_type_dialog)
                || (atoms[i] == net_wm_window_type_normal)
                || (atoms[i] == net_wm_window_type_utility)
                || (atoms[i] == net_wm_window_type_splash))
            {
                c->type_atom = atoms[i];
                break;
            }
            ++i;
        }
        if (atoms)
        {
            XFree (atoms);
        }
    }
    clientWindowType (c);
}

void
clientGetInitialNetWmDesktop (Client * c)
{
    Client *c2 = NULL;
    long val = 0;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGetInitialNetWmDesktop");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    /* This is to make sure that transient are shown with their "ancestor" window */
    c2 = clientGetTransient (c);
    if (c2)
    {
        FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
        c->win_workspace = c2->win_workspace;
        if (FLAG_TEST (c2->flags, CLIENT_FLAG_STICKY))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
            c->win_state |= WIN_STATE_STICKY;
        }
    }
    else
    {
        if (!FLAG_TEST_ALL (c->flags,
                CLIENT_FLAG_SESSION_MANAGED | CLIENT_FLAG_WORKSPACE_SET))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
            c->win_workspace = c->screen_info->current_ws;
        }
        if (getHint (clientGetXDisplay (c), c->window, net_wm_desktop, &val))
        {
            TRACE ("atom net_wm_desktop detected");
            if (val == (int) ALL_WORKSPACES)
            {
                if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_STICK,
                        CLIENT_FLAG_STICKY))
                {
                    TRACE
                        ("atom net_wm_desktop specifies window \"%s\" is sticky",
                        c->name);
                    FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
                    c->win_state |= WIN_STATE_STICKY;
                }
                c->win_workspace = c->screen_info->current_ws;
            }
            else
            {
                TRACE
                    ("atom net_wm_desktop specifies window \"%s\" is on desk %i",
                    c->name, (int) val);
                c->win_workspace = (int) val;
            }
            FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
        }
        else if (getHint (clientGetXDisplay (c), c->window, win_workspace, &val))
        {
            TRACE ("atom win_workspace specifies window \"%s\" is on desk %i",
                c->name, (int) val);
            c->win_workspace = (int) val;
            FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
        }
    }
    TRACE ("initial desktop for window \"%s\" is %i", c->name,
        c->win_workspace);
    if (c->win_workspace > c->screen_info->workspace_count - 1)
    {
        TRACE ("value off limits, using %i instead",
            c->screen_info->workspace_count - 1);
        c->win_workspace = c->screen_info->workspace_count - 1;
        FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
    }
    TRACE ("initial desktop for window \"%s\" is %i", c->name,
        c->win_workspace);
    setHint (clientGetXDisplay (c), c->window, win_workspace, c->win_workspace);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        setHint (clientGetXDisplay (c), c->window, net_wm_desktop, (unsigned long) ALL_WORKSPACES);
    }
    else
    {
        setHint (clientGetXDisplay (c), c->window, net_wm_desktop, (unsigned long) c->win_workspace);
    }
}

void
clientSetNetClientList (ScreenInfo * screen_info, Atom a, GList * list)
{
    Window *listw;
    Window *index_dest;
    GList *index_src;
    gint size, i;

    TRACE ("entering clientSetNetClientList");

    size = g_list_length (list);
    if (size < 1)
    {
        XDeleteProperty (myScreenGetXDisplay (screen_info), screen_info->xroot, a);
    }
    else if ((listw = (Window *) malloc ((size + 1) * sizeof (Window))))
    {
        TRACE ("%i windows in list for %i clients", size, screen_info->client_count);
        for (i = 0, index_dest = listw, index_src = list; i < size;
            i++, index_dest++, index_src = g_list_next (index_src))
        {
            Client *c = (Client *) index_src->data;
            *index_dest = c->window;
        }
        XChangeProperty (myScreenGetXDisplay (screen_info), screen_info->xroot, a, XA_WINDOW, 32, PropModeReplace,
            (unsigned char *) listw, size);
        free (listw);
    }
}

void
clientGetNetStruts (Client * c)
{
    gulong *struts = NULL;
    int nitems;
    int i;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGetNetStruts for \"%s\" (0x%lx)", c->name,
        c->window);

    for (i = 0; i < 12; i++)
    {
        c->struts[i] = 0;
    }
    FLAG_UNSET (c->flags, CLIENT_FLAG_HAS_STRUT);
    FLAG_UNSET (c->flags, CLIENT_FLAG_HAS_STRUT_PARTIAL);

    if (getCardinalList (clientGetXDisplay (c), c->window, net_wm_strut_partial, &struts, &nitems))
    {
        if (nitems != 12)
        {
            if (struts)
            {
                XFree (struts);
            }
            return;
        }

        FLAG_SET (c->flags, CLIENT_FLAG_HAS_STRUT);
        FLAG_SET (c->flags, CLIENT_FLAG_HAS_STRUT_PARTIAL);
        for (i = 0; i < 12; i++)
        {
            c->struts[i] = (int) struts[i];
        }

        XFree (struts);
        workspaceUpdateArea (c->screen_info);
    }
    else if (getCardinalList (clientGetXDisplay (c), c->window, net_wm_strut, &struts, &nitems))
    {
        if (nitems != 4)
        {
            if (struts)
            {
                XFree (struts);
            }
            return;
        }

        FLAG_SET (c->flags, CLIENT_FLAG_HAS_STRUT);
        for (i = 0; i < 4; i++)
        {
            c->struts[i] = (int) struts[i];
        }
        for (i = 4; i < 12; i++)
        {
            c->struts[i] = 0;
        }
        /* Fill(in values as for partial struts */
        c->struts[TOP_START_X] = c->struts[BOTTOM_START_X] = 0;
        c->struts[TOP_END_X] = c->struts[BOTTOM_END_X] = 
            gdk_screen_get_width (c->screen_info->gscr);
        c->struts[LEFT_START_Y] = c->struts[RIGHT_START_Y] = 0;
        c->struts[LEFT_END_Y] = c->struts[RIGHT_END_Y] = 
            gdk_screen_get_height (c->screen_info->gscr);
        
        XFree (struts);
        workspaceUpdateArea (c->screen_info);
    }
}

void
clientSetNetActions (Client * c)
{
    Atom atoms[6];
    int i = 0;

    atoms[i++] = net_wm_action_close;
    if (CLIENT_CAN_MAXIMIZE_WINDOW (c))
    {
        atoms[i++] = net_wm_action_maximize_horz;
        atoms[i++] = net_wm_action_maximize_vert;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STICK))
    {
        atoms[i++] = net_wm_action_change_desktop;
        atoms[i++] = net_wm_action_stick;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_BORDER))
    {
        atoms[i++] = net_wm_action_shade;
    }
    XChangeProperty (clientGetXDisplay (c), c->window, net_wm_allowed_actions, XA_ATOM, 32,
        PropModeReplace, (unsigned char *) atoms, i);
}

void
clientWindowType (Client * c)
{
    netWindowType old_type;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientWindowType");
    TRACE ("type for client \"%s\" (0x%lx)", c->name, c->window);

    old_type = c->type;
    c->initial_layer = c->win_layer;
    if (c->type_atom != None)
    {
        if (c->type_atom == net_wm_window_type_desktop)
        {
            TRACE ("atom net_wm_window_type_desktop detected");
            c->type = WINDOW_DESKTOP;
            c->initial_layer = WIN_LAYER_DESKTOP;
            c->win_state |= WIN_STATE_STICKY;
            FLAG_SET (c->flags,
                CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_STICKY |
                CLIENT_FLAG_SKIP_TASKBAR);
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_RESIZE | CLIENT_FLAG_HAS_MOVE | 
                CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE | 
                CLIENT_FLAG_HAS_MENU | CLIENT_FLAG_HAS_STICK |
                CLIENT_FLAG_HAS_BORDER);
        }
        else if (c->type_atom == net_wm_window_type_dock)
        {
            TRACE ("atom net_wm_window_type_dock detected");
            c->type = WINDOW_DOCK;
            c->initial_layer = WIN_LAYER_DOCK;
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_BORDER |  CLIENT_FLAG_HAS_MOVE |
                CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE | 
                CLIENT_FLAG_HAS_MENU | CLIENT_FLAG_HAS_STICK);
        }
        else if (c->type_atom == net_wm_window_type_toolbar)
        {
            TRACE ("atom net_wm_window_type_toolbar detected");
            c->type = WINDOW_TOOLBAR;
            c->initial_layer = WIN_LAYER_NORMAL;
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE |
                CLIENT_FLAG_HAS_STICK);
        }
        else if (c->type_atom == net_wm_window_type_menu)
        {
            TRACE ("atom net_wm_window_type_menu detected");
            c->type = WINDOW_MENU;
            c->initial_layer = WIN_LAYER_NORMAL;
            /* The policy here is unclear :
               http://mail.gnome.org/archives/wm-spec-list/2002-May/msg00001.html
               As it seems, GNOME and KDE don't treat menu the same way...
             */
            FLAG_SET (c->flags,
                CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_SKIP_TASKBAR);
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_MAXIMIZE |
                CLIENT_FLAG_HAS_STICK);
        }
        else if (c->type_atom == net_wm_window_type_dialog)
        {
            TRACE ("atom net_wm_window_type_dialog detected");
            c->type = WINDOW_DIALOG;
            c->initial_layer = WIN_LAYER_NORMAL;
        }
        else if (c->type_atom == net_wm_window_type_normal)
        {
            TRACE ("atom net_wm_window_type_normal detected");
            c->type = WINDOW_NORMAL;
            c->initial_layer = WIN_LAYER_NORMAL;
        }
        else if (c->type_atom == net_wm_window_type_utility)
        {
            TRACE ("atom net_wm_window_type_utility detected");
            c->type = WINDOW_UTILITY;
            c->initial_layer = WIN_LAYER_NORMAL;
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_STICK);
        }
        else if (c->type_atom == net_wm_window_type_splash)
        {
            TRACE ("atom net_wm_window_type_splash detected");
            c->type = WINDOW_SPLASHSCREEN;
            c->initial_layer = WIN_LAYER_ABOVE_DOCK;
            FLAG_UNSET (c->flags,
                CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_HIDE |
                CLIENT_FLAG_HAS_MENU | CLIENT_FLAG_HAS_MOVE |
                CLIENT_FLAG_HAS_RESIZE | CLIENT_FLAG_HAS_STICK);
        }
    }
    else
    {
        TRACE ("no \"net\" atom detected");
        c->type = UNSET;
        c->initial_layer = c->win_layer;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL))
    {
        TRACE ("window is modal");
        c->type = WINDOW_MODAL_DIALOG;
    }

    if (clientIsTransientOrModal (c))
    {
        Client *c2;

        TRACE ("Window \"%s\" is a transient or a modal", c->name);

        c2 = clientGetHighestTransientOrModalFor (c);
        if (c2)
        {
            if (clientIsTransient (c))
            {
                c->initial_layer = c2->win_layer;
            }
            else if (c->initial_layer < c2->win_layer) /* clientIsModal (c) */
            {
                c->initial_layer = c2->win_layer;
            }
            TRACE ("Applied layer is %i", c->initial_layer);
        }
        FLAG_UNSET (c->flags,
            CLIENT_FLAG_HAS_HIDE | CLIENT_FLAG_HAS_STICK |
            CLIENT_FLAG_STICKY);
    }
    if ((old_type != c->type) || (c->initial_layer != c->win_layer))
    {
        TRACE ("setting layer %i", c->initial_layer);
        clientSetNetState (c);
        clientSetLayer (c, c->initial_layer);
    }
}

void
clientUpdateAboveState (Client * c)
{
    int layer;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateAboveState");
    TRACE ("Update above state for client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
    {
        layer = WIN_LAYER_ABOVE_DOCK;
    }
    else
    {
        layer = c->initial_layer;
    }
    clientSetNetState (c);
    clientSetLayer (c, layer);
}

void
clientUpdateBelowState (Client * c)
{
    int layer;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateBelowState");
    TRACE ("Update below state for client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
    {
        layer = WIN_LAYER_BELOW;
    }
    else
    {
        layer = c->initial_layer;
    }
    clientSetNetState (c);
    clientSetLayer (c, layer);
}

