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
        xfwm4    - (c) 2002-2006 Olivier Fourdan

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

#include "display.h"
#include "screen.h"
#include "misc.h"
#include "transients.h"
#include "workspaces.h"
#include "settings.h"
#include "client.h"
#include "focus.h"
#include "stacking.h"
#include "hints.h"

static void
workspaceGetPosition (ScreenInfo *screen_info, int n, int * row, int * col)
{
    NetWmDesktopLayout l;
    int major_length, minor_length, tmp;

    l = screen_info->desktop_layout;
    if (l.orientation == NET_WM_ORIENTATION_HORZ)
    {
        major_length = l.cols;
        minor_length = l.rows;
    }
    else
    {
        major_length = l.rows;
        minor_length = l.cols;
    }

    *row = n / major_length;
    *col = n % major_length;

    switch (l.start)
    {
        case NET_WM_TOPRIGHT:
            *col = major_length - *col - 1;
            break;
        case NET_WM_BOTTOMLEFT:
            *row = minor_length - *row - 1;
            break;
        case NET_WM_BOTTOMRIGHT:
            *col = major_length - *col - 1;
            *row = minor_length - *row - 1;
            break;
        default:
            break;
    }

    if (l.orientation == NET_WM_ORIENTATION_VERT)
    {
        tmp = *row;
        *row = *col;
        *col = tmp;
        if ((l.start == NET_WM_TOPRIGHT) || (l.start == NET_WM_BOTTOMLEFT))
        {
            *row = l.rows - *row - 1;
            *col = l.cols - *col - 1;
        }
    }
}

static int
workspaceGetNumber (ScreenInfo *screen_info, int row, int col)
{
    NetWmDesktopLayout l;
    int major_length, minor_length, n, tmp;

    l = screen_info->desktop_layout;
    if (l.orientation == NET_WM_ORIENTATION_HORZ)
    {
        major_length = l.cols;
        minor_length = l.rows;
    }
    else
    {
        major_length = l.rows;
        minor_length = l.cols;
    }

    if (l.orientation == NET_WM_ORIENTATION_VERT)
    {
        tmp = row;
        row = col;
        col = tmp;
        if ((l.start == NET_WM_TOPRIGHT) || (l.start == NET_WM_BOTTOMLEFT))
        {
            row = minor_length - row - 1;
            col = major_length - col - 1;
        }
    }

    switch (l.start)
    {
        case NET_WM_TOPRIGHT:
            col = major_length - col - 1;
            break;
        case NET_WM_BOTTOMLEFT:
            row = minor_length - row - 1;
            break;
        case NET_WM_BOTTOMRIGHT:
            col = major_length - col - 1;
            row = minor_length - row - 1;
            break;
        default:
            break;
    }

    n = (row * major_length) + col;
    return n;
}

static int
modify_with_wrap (int value, int by, int limit, gboolean wrap)
{
    if (by >= limit) by = limit - 1;
    value += by;
    if (value >= limit)
    {
        if (!wrap)
        {
            value = limit - 1;
        }
        else
        {
            value = value % limit;
        }
    }
    else if (value < 0)
    {
        if (!wrap)
        {
            value = 0;
        }
        else
        {
            value = (value + limit) % limit;
        }
    }
    return value;
}

/* returns TRUE if the workspace was changed, FALSE otherwise */
gboolean
workspaceMove (ScreenInfo *screen_info, int rowmod, int colmod, Client * c)
{
    int row, col, newrow, newcol, previous_ws, n;

    workspaceGetPosition(screen_info, screen_info->current_ws, &row, &col);
    newrow = modify_with_wrap(row, rowmod, screen_info->desktop_layout.rows, screen_info->params->wrap_layout);
    newcol = modify_with_wrap(col, colmod, screen_info->desktop_layout.cols, screen_info->params->wrap_layout);
    n = workspaceGetNumber(screen_info, newrow, newcol);

    if (n == screen_info->current_ws)
    {
        return FALSE;
    }

    previous_ws = screen_info->current_ws;
    if ((n >= 0) && (n < screen_info->workspace_count))
    {
        workspaceSwitch(screen_info, n, c, TRUE);
    }
    else if (screen_info->params->wrap_layout)
    {
        if (colmod < 0)
        {
           n = screen_info->workspace_count - 1;
        }
        else
        {
            if (colmod > 0)
            {
                newcol = 0;
            }
            else if (rowmod > 0)
            {
                newrow = 0;
            }
            else if (rowmod < 0)
            {
                newrow--;
            }
            else
            {
                return FALSE;
            }

            n = workspaceGetNumber(screen_info, newrow, newcol);
        }
        workspaceSwitch(screen_info, n, c, TRUE);
    }

    return (screen_info->current_ws != previous_ws);
}

void
workspaceSwitch (ScreenInfo *screen_info, int new_ws, Client * c2, gboolean update_focus)
{
    DisplayInfo *display_info;
    Client *c, *new_focus;
    Client *previous;
    GList *index;
    GList *list_hide;
    Window dr, window;
    int rx, ry, wx, wy;
    unsigned int mask;
    unsigned long data[1];

    TRACE ("entering workspaceSwitch");

    display_info = screen_info->display_info;
    new_focus = NULL;
    if ((new_ws == screen_info->current_ws) && (screen_info->params->toggle_workspaces))
    {
        new_ws = screen_info->previous_ws;
    }

    if (new_ws == screen_info->current_ws)
    {
        return;
    }

    if (screen_info->params->wrap_cycle)
    {
        if (new_ws > screen_info->workspace_count - 1)
        {
            new_ws = 0;
        }
        if (new_ws < 0)
        {
            new_ws = screen_info->workspace_count - 1;
        }
    }
    else if ((new_ws > screen_info->workspace_count - 1) || (new_ws < 0))
    {
        return;
    }

    myScreenGrabPointer (screen_info, EnterWindowMask, None, myDisplayGetCurrentTime (display_info));

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
                clientSetFocus (screen_info, NULL, myDisplayGetCurrentTime (display_info), FOCUS_IGNORE_MODAL);
            }
            if (!clientIsValidTransientOrModal (c))
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
            if (!clientIsValidTransientOrModal (c))
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

    setHint (display_info, screen_info->xroot, WIN_WORKSPACE, new_ws);
    data[0] = new_ws;
    XChangeProperty (myScreenGetXDisplay (screen_info), screen_info->xroot,
                     display_info->atoms[NET_CURRENT_DESKTOP], XA_CARDINAL, 32,
                     PropModeReplace, (unsigned char *) data, 1);
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

    myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));

    if (new_focus && update_focus)
    {
        clientSetFocus (new_focus->screen_info, new_focus, myDisplayGetCurrentTime (display_info), NO_FOCUS_FLAG);
    }
}

void
workspaceSetNames (ScreenInfo * screen_info, gchar **names, int items)
{
    if (screen_info->workspace_names)
    {
        g_strfreev (screen_info->workspace_names);
    }

    screen_info->workspace_names = names;
    screen_info->workspace_names_items = items;
}

void
workspaceSetCount (ScreenInfo * screen_info, int count)
{
    DisplayInfo *display_info;
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

    display_info = screen_info->display_info;
    setHint (display_info, screen_info->xroot, WIN_WORKSPACE_COUNT, count);
    setHint (display_info, screen_info->xroot, NET_NUMBER_OF_DESKTOPS, count);
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
        workspaceSwitch (screen_info, count - 1, NULL, TRUE);
    }
    setNetWorkarea (display_info, screen_info->xroot, screen_info->workspace_count,
                    screen_info->width, screen_info->height, screen_info->margins);
    /* Recompute the layout based on the (changed) number of desktops */
    getDesktopLayout(display_info, screen_info->xroot, screen_info->workspace_count,
                     &screen_info->desktop_layout);
}

void
workspaceUpdateArea (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;
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

    display_info = screen_info->display_info;
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
        setNetWorkarea (display_info, screen_info->xroot, screen_info->workspace_count,
                        screen_info->width, screen_info->height, screen_info->margins);
        /* Also prevent windows from being off screen, just like when screen is resized */
        clientScreenResize(screen_info);
    }
}
