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
workspaceSwitch (ScreenInfo *screen_info, int new_ws, Client * c2)
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

    if ((new_ws > screen_info->workspace_count - 1) || (new_ws < 0))
    {
        return;
    }

    /* Grab the pointer to avoid side effects with EnterNotify events */
    XGrabPointer (myScreenGetXDisplay (screen_info), screen_info->gnome_win, FALSE, EnterWindowMask, GrabModeAsync,
                       GrabModeAsync, None, None, GDK_CURRENT_TIME);

    if (new_ws == screen_info->current_ws)
    {
         new_ws = screen_info->previous_ws;
    }
    screen_info->previous_ws = screen_info->current_ws;
    screen_info->current_ws = new_ws;
    if (c2)
    {
        clientSetWorkspace (c2, new_ws, FALSE);
    }

    list_hide = NULL;
    previous = clientGetFocus ();

    /* First pass */
    for (index = screen_info->windows_stack; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_STICKY) 
            && ((c->win_workspace != new_ws)))
        {
            if (c == previous)
            {
                FLAG_SET (previous->xfwm_flags, XFWM_FLAG_FOCUS);
                clientSetFocus (screen_info, NULL, GDK_CURRENT_TIME, FOCUS_IGNORE_MODAL);
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
    for (index = g_list_last(screen_info->windows_stack); index; index = g_list_previous (index))
    {
        c = (Client *) index->data;
        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY)
            || FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
        {
            clientSetWorkspace (c, new_ws, TRUE);
            if (c == previous)
            {
                new_focus = c;
            }
            FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_FOCUS);
        }
        else if ((c->win_workspace == new_ws)
            && !FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
        {
            if (!clientIsTransientOrModal (c))
            {
                clientShow (c, FALSE);
            }
            if ((!new_focus) && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_FOCUS))
            {
                new_focus = c;
            }
            FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_FOCUS);
        }
    }

    setHint (myScreenGetXDisplay (screen_info), screen_info->xroot, win_workspace, new_ws);
    data[0] = new_ws;
    XChangeProperty (myScreenGetXDisplay (screen_info), screen_info->xroot, net_current_desktop, XA_CARDINAL, 32,
        PropModeReplace, (unsigned char *) data, 1);
    workspaceUpdateArea (screen_info);
    
    /* Ungrab the pointer we grabbed before mapping/unmapping all windows */
    XUngrabPointer (myScreenGetXDisplay (screen_info), GDK_CURRENT_TIME);

    if (!(screen_info->params->click_to_focus))
    {
        if (!(c2) && (XQueryPointer (myScreenGetXDisplay (screen_info), screen_info->xroot, &dr, &window, &rx, &ry, &wx, &wy, &mask)))
        {
            c = clientAtPosition (screen_info, rx, ry, NULL);
            if (c)
            {
                new_focus = c;
            }
        }
    }
    if (new_focus)
    {
        clientSetFocus (new_focus->screen_info, new_focus, GDK_CURRENT_TIME, NO_FOCUS_FLAG);
    }
}

void
workspaceSetNames (ScreenInfo * screen_info, char *names, int length)
{
    if (screen_info->workspace_names)
    {
        g_free (screen_info->workspace_names);
    }

    screen_info->workspace_names = names;
    screen_info->workspace_names_length = length;
}

void
workspaceSetCount (ScreenInfo * screen_info, int count)
{
    Client *c;
    int i;

    TRACE ("entering workspaceSetCount");

    if (count < 1)
    {
        count = 1;
    }
    if (count == screen_info->workspace_count)
    {
        return;
    }

    setHint (myScreenGetXDisplay (screen_info), screen_info->xroot, win_workspace_count, count);
    setHint (myScreenGetXDisplay (screen_info), screen_info->xroot, net_number_of_desktops, count);
    screen_info->workspace_count = count;

    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, i++)
    {
        if (c->win_workspace > count - 1)
        {
            clientSetWorkspace (c, count - 1, TRUE);
        }
    }
    if (screen_info->current_ws > count - 1)
    {
        workspaceSwitch (screen_info, count - 1, NULL);
    }
    setNetWorkarea (myScreenGetXDisplay (screen_info), screen_info->screen, screen_info->workspace_count, 
                         gdk_screen_get_width (screen_info->gscr),
                         gdk_screen_get_height (screen_info->gscr),
                         screen_info->margins);
}

void
workspaceUpdateArea (ScreenInfo *screen_info)
{
    Client *c;
    int prev_top;
    int prev_left;
    int prev_right;
    int prev_bottom;
    int i;

    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (screen_info->margins != NULL);
    g_return_if_fail (screen_info->gnome_margins != NULL);

    TRACE ("entering workspaceUpdateArea");

    prev_top = screen_info->margins[TOP];
    prev_left = screen_info->margins[LEFT];
    prev_right = screen_info->margins[RIGHT];
    prev_bottom = screen_info->margins[BOTTOM];

    for (i = 0; i < 4; i++)
    {
        screen_info->margins[i] = screen_info->gnome_margins[i];
    }

    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, i++)
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUT)
            && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
        {
            screen_info->margins[TOP]    = MAX (screen_info->margins[TOP],    c->struts[TOP]);
            screen_info->margins[LEFT]   = MAX (screen_info->margins[LEFT],   c->struts[LEFT]);
            screen_info->margins[RIGHT]  = MAX (screen_info->margins[RIGHT],  c->struts[RIGHT]);
            screen_info->margins[BOTTOM] = MAX (screen_info->margins[BOTTOM], c->struts[BOTTOM]);
        }
    }

    if ((prev_top != screen_info->margins[TOP]) || (prev_left != screen_info->margins[LEFT])
        || (prev_right != screen_info->margins[RIGHT])
        || (prev_bottom != screen_info->margins[BOTTOM]))
    {
        TRACE ("Margins have changed, updating net_workarea");
        setNetWorkarea (myScreenGetXDisplay (screen_info), screen_info->screen, screen_info->workspace_count, 
                             gdk_screen_get_width (screen_info->gscr),
                             gdk_screen_get_height (screen_info->gscr),
                             screen_info->margins);
    }
}
