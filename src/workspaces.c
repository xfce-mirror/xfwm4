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
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2003 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libxfce4util/debug.h>
#include "main.h"
#include "workspaces.h"
#include "settings.h"
#include "client.h"
#include "hints.h"

void workspaceSwitch(int new_ws, Client * c2)
{
    Client *c, *new_focus = NULL;
    Client *previous;
    GSList *list_of_windows;
    GSList *index;
    Window dr, window;
    int rx, ry, wx, wy;
    unsigned int mask;
    unsigned long data[1];
    XEvent an_event;

    TRACE("entering workspaceSwitch");

    if(new_ws > params.workspace_count - 1)
    {
        new_ws = 0;
    }
    else if (new_ws < 0)
    {
        new_ws = params.workspace_count - 1;
    }
    
    if(new_ws == workspace)
    {
        return;
    }

    workspace = new_ws;
    if(c2)
    {
        clientSetWorkspace(c2, new_ws, FALSE);
    }
    
    previous = clientGetFocus();
    list_of_windows = clientGetStackList();
    /* First pass */
    for(index = list_of_windows; index; index = g_slist_next(index))
    {
        c = (Client *) index->data;
        if(CLIENT_FLAG_TEST_AND_NOT(c, CLIENT_FLAG_VISIBLE, CLIENT_FLAG_STICKY) && ((c->win_workspace != new_ws)))
        {
            if (c == previous)
            {
                CLIENT_FLAG_SET(previous, CLIENT_FLAG_FOCUS);
                clientSetFocus(NULL, FALSE);
            }
            if (!clientIsTransient(c))
            {
                clientHide(c, new_ws, FALSE);
            }
        }
    }

    /* Second pass */
    list_of_windows = g_slist_reverse(list_of_windows);
    for(index = list_of_windows; index; index = g_slist_next(index))
    {
        c = (Client *) index->data;
        if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_STICKY | CLIENT_FLAG_VISIBLE))
        {
            clientSetWorkspace(c, new_ws, TRUE);
            if(c == previous)
            {
                new_focus = c;
            }
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_FOCUS);
        }
        else if((c->win_workspace == new_ws) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_HIDDEN))
        {
            if (!clientIsTransient(c))
            {
                clientShow(c, FALSE);
            }
            if((!new_focus) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_FOCUS))
            {
                new_focus = c;
            }
            CLIENT_FLAG_UNSET(c, CLIENT_FLAG_FOCUS);
        }
    }
    
    /* Free the list */
    if (list_of_windows)
    {
        g_slist_free(list_of_windows);
    }
    
    setGnomeHint(dpy, root, win_workspace, new_ws);
    data[0] = new_ws;
    XChangeProperty(dpy, root, net_current_desktop, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)data, 1);
    workspaceUpdateArea(margins, gnome_margins);
    XSync(dpy, FALSE);
    if(!(params.click_to_focus))
    {
        /* Just get rid of EnterNotify events when using focus follow mouse */
        while(XCheckTypedEvent(dpy, EnterNotify, &an_event))
            ;
        if (!(c2) && (XQueryPointer(dpy, root, &dr, &window, &rx, &ry, &wx, &wy, &mask)))
        {
            c = clientAtPosition(rx, ry, NULL);
            if (c)
            {
                new_focus = c;
            }
        }
    }
    clientSetFocus(new_focus, TRUE);
}

void workspaceSetCount(int count)
{
    Client *c;
    int i;

    TRACE("entering workspaceSetCount");

    if(count < 1)
    {
        count = 1;
    }
    if(count == params.workspace_count)
    {
        return;
    }

    setGnomeHint(dpy, root, win_workspace_count, count);
    setNetHint(dpy, root, net_number_of_desktops, count);
    params.workspace_count = count;

    for(c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        if(c->win_workspace > count - 1)
        {
            clientSetWorkspace(c, count - 1, TRUE);
        }
    }
    if(workspace > count - 1)
    {
        workspaceSwitch(count - 1, NULL);
    }
    set_net_workarea(dpy, screen, params.workspace_count, margins);
}

void workspaceGetArea(CARD32 * m1, CARD32 * m2, Client * c)
{
    Client *c2;
    int i;

    TRACE("entering workspaceGetArea");
    
    g_return_if_fail (m1 != NULL);

    for(i = 0; i < 4; i++)
    {
        if (m2 == NULL)
        {
            m1[i] = 0;
        }
        else
        {
            m1[i] = m2[i];
        }
    }
    
    for(c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
    {
        if(((!c) || (c != c2)) && CLIENT_FLAG_TEST_ALL(c2, CLIENT_FLAG_HAS_STRUTS | CLIENT_FLAG_VISIBLE))
        {
            m1[MARGIN_TOP] = MAX(m1[MARGIN_TOP], c2->struts[MARGIN_TOP]);
            m1[MARGIN_LEFT] = MAX(m1[MARGIN_LEFT], c2->struts[MARGIN_LEFT]);
            m1[MARGIN_RIGHT] = MAX(m1[MARGIN_RIGHT], c2->struts[MARGIN_RIGHT]);
            m1[MARGIN_BOTTOM] = MAX(m1[MARGIN_BOTTOM], c2->struts[MARGIN_BOTTOM]);
        }
    }
}

void workspaceUpdateArea(CARD32 * m1, CARD32 * m2)
{
    int prev_top;
    int prev_left;
    int prev_right;
    int prev_bottom;
    
    g_return_if_fail (m1 != NULL);
    g_return_if_fail (m2 != NULL);

    TRACE("entering workspaceUpdateArea");
    
    prev_top = m1[MARGIN_TOP];
    prev_left = m1[MARGIN_LEFT];
    prev_right = m1[MARGIN_RIGHT];
    prev_bottom = m1[MARGIN_BOTTOM];
    
    workspaceGetArea(m1, m2, NULL);
    
    TRACE("Desktop area computed : (%d,%d,%d,%d)", (int)m1[0], (int)m1[1], (int)m1[2], (int)m1[3]);
    if((prev_top != m1[MARGIN_TOP]) || (prev_left != m1[MARGIN_LEFT]) || (prev_right != m1[MARGIN_RIGHT]) || (prev_bottom != m1[MARGIN_BOTTOM]))
    {
        TRACE("Margins have changed, updating net_workarea");
        set_net_workarea(dpy, screen, params.workspace_count, m1);
    }
}
