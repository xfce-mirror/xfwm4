/*
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
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h> 

#include "screen.h"
#include "misc.h"
#include "transients.h"
#include "workspaces.h"
#include "settings.h"
#include "client.h"
#include "focus.h"
#include "stacking.h"
#include "hints.h"

void
workspaceSwitch (ScreenData *md, int new_ws, Client * c2)
{
    Client *c, *new_focus = NULL;
    Client *previous;
    GList *index;
    GList *list_hide;
    Window dr, window;
    int rx, ry, wx, wy;
    unsigned int mask;
    unsigned long data[1];

    TRACE ("entering workspaceSwitch");

    if (new_ws > params.workspace_count - 1)
    {
        new_ws = 0;
    }
    else if (new_ws < 0)
    {
        new_ws = params.workspace_count - 1;
    }

    if (new_ws == md->current_ws)
    {
        return;
    }

    /* Grab the pointer to avoid side effects with EnterNotify events */
    XGrabPointer (md->dpy, md->gnome_win, FALSE, EnterWindowMask, GrabModeAsync,
                       GrabModeAsync, None, None, GDK_CURRENT_TIME);

    md->current_ws = new_ws;
    if (c2)
    {
        clientSetWorkspace (c2, new_ws, FALSE);
    }

    list_hide = NULL;
    previous = clientGetFocus ();

    /* First pass */
    for (index = windows_stack; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_VISIBLE,
                CLIENT_FLAG_STICKY) && ((c->win_workspace != new_ws)))
        {
            if (c == previous)
            {
                FLAG_SET (previous->flags, CLIENT_FLAG_FOCUS);
                clientSetFocus (md, NULL, GDK_CURRENT_TIME, FOCUS_IGNORE_MODAL);
            }
            if (!clientIsTransientOrModal (c))
            {
                /* Just build of windows that will be hidden, so that
                   we can record the previous focus even when on a
                   transient (otherwise the transient vanishes along
                   with its parent window which is placed below...
                 */
                list_hide = g_list_append (list_hide, c);
            }
        }
    }

    /* First pass and a half :) */
    if (list_hide)
    {
        for (index = list_hide; index; index = g_list_next (index))
        {
            c = (Client *) index->data;
            clientHide (c, new_ws, FALSE);
        }
        g_list_free (list_hide);
    }

    /* Second pass */
    for (index = g_list_last(windows_stack); index; index = g_list_previous (index))
    {
        c = (Client *) index->data;
        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY | CLIENT_FLAG_VISIBLE))
        {
            clientSetWorkspace (c, new_ws, TRUE);
            if (c == previous)
            {
                new_focus = c;
            }
            FLAG_UNSET (c->flags, CLIENT_FLAG_FOCUS);
        }
        else if ((c->win_workspace == new_ws)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_HIDDEN))
        {
            if (!clientIsTransientOrModal (c))
            {
                clientShow (c, FALSE);
            }
            if ((!new_focus) && FLAG_TEST (c->flags, CLIENT_FLAG_FOCUS))
            {
                new_focus = c;
            }
            FLAG_UNSET (c->flags, CLIENT_FLAG_FOCUS);
        }
    }

    setHint (md->dpy, md->xroot, win_workspace, new_ws);
    data[0] = new_ws;
    XChangeProperty (md->dpy, md->xroot, net_current_desktop, XA_CARDINAL, 32,
        PropModeReplace, (unsigned char *) data, 1);
    workspaceUpdateArea (md);
    
    /* Ungrab the pointer we grabbed before mapping/unmapping all windows */
    XUngrabPointer (md->dpy, GDK_CURRENT_TIME);

    if (!(params.click_to_focus))
    {
        if (!(c2) && (XQueryPointer (md->dpy, md->xroot, &dr, &window, &rx, &ry, &wx, &wy, &mask)))
        {
            c = clientAtPosition (rx, ry, NULL);
            if (c)
            {
                new_focus = c;
            }
        }
    }
    if (new_focus)
    {
        clientSetFocus (new_focus->md, new_focus, GDK_CURRENT_TIME, NO_FOCUS_FLAG);
    }
}

void
workspaceSetNames (char *names, int length)
{
    if (params.workspace_names)
    {
        g_free (params.workspace_names);
    }

    params.workspace_names = names;
    params.workspace_names_length = length;
}

void
workspaceSetCount (ScreenData * md, int count)
{
    Client *c;
    int i;

    TRACE ("entering workspaceSetCount");

    if (count < 1)
    {
        count = 1;
    }
    if (count == params.workspace_count)
    {
        return;
    }

    setHint (md->dpy, md->xroot, win_workspace_count, count);
    setHint (md->dpy, md->xroot, net_number_of_desktops, count);
    params.workspace_count = count;

    for (c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        if (c->win_workspace > count - 1)
        {
            clientSetWorkspace (c, count - 1, TRUE);
        }
    }
    if (md->current_ws > count - 1)
    {
        workspaceSwitch (md, count - 1, NULL);
    }
    setNetWorkarea (md->dpy, md->screen, params.workspace_count, 
                         gdk_screen_get_width (md->gscr),
                         gdk_screen_get_height (md->gscr),
                         md->margins);
}

void
workspaceUpdateArea (ScreenData *md)
{
    Client *c;
    int prev_top;
    int prev_left;
    int prev_right;
    int prev_bottom;
    int i;

    g_return_if_fail (md != NULL);
    g_return_if_fail (md->margins != NULL);
    g_return_if_fail (md->gnome_margins != NULL);

    TRACE ("entering workspaceUpdateArea");

    prev_top = md->margins[TOP];
    prev_left = md->margins[LEFT];
    prev_right = md->margins[RIGHT];
    prev_bottom = md->margins[BOTTOM];

    for (i = 0; i < 4; i++)
    {
        md->margins[i] = md->gnome_margins[i];
    }

    for (c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        if (FLAG_TEST_ALL (c->flags, CLIENT_FLAG_HAS_STRUT | CLIENT_FLAG_VISIBLE))
        {
            md->margins[TOP]    = MAX (md->margins[TOP],    c->struts[TOP]);
            md->margins[LEFT]   = MAX (md->margins[LEFT],   c->struts[LEFT]);
            md->margins[RIGHT]  = MAX (md->margins[RIGHT],  c->struts[RIGHT]);
            md->margins[BOTTOM] = MAX (md->margins[BOTTOM], c->struts[BOTTOM]);
        }
    }

    if ((prev_top != md->margins[TOP]) || (prev_left != md->margins[LEFT])
        || (prev_right != md->margins[RIGHT])
        || (prev_bottom != md->margins[BOTTOM]))
    {
        TRACE ("Margins have changed, updating net_workarea");
        setNetWorkarea (md->dpy, md->screen, params.workspace_count, 
                             gdk_screen_get_width (md->gscr),
                             gdk_screen_get_height (md->gscr),
                             md->margins);
    }
}
