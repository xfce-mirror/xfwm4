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
    Client *c, *f = NULL;
    Client *previous;
    GSList *list_of_windows;
    GSList *index;
    unsigned long data[1];
    XEvent an_event;

    TRACE("entering workspaceSwitch");

    if((new_ws < 0) && params.wrap_workspaces)
    {
        new_ws = params.workspace_count - 1;
    }
    if((new_ws > params.workspace_count - 1) && params.wrap_workspaces)
    {
        new_ws = 0;
    }
    if(new_ws == workspace)
    {
        return;
    }

    previous = clientGetFocus();
    if(previous)
    {
        CLIENT_FLAG_SET(previous, CLIENT_FLAG_FOCUS);
        if(CLIENT_FLAG_TEST(previous, CLIENT_FLAG_STICKY))
        {
            f = previous;
        }
    }

    workspace = new_ws;
    if(c2)
    {
        clientSetWorkspace(c2, new_ws, FALSE);
    }
    clientSetFocus(c2, FALSE);
    
    list_of_windows = clientGetStackList();
    /* First pass */
    for(index = list_of_windows; index; index = g_slist_next(index))
    {
        c = (Client *) index->data;
        if(CLIENT_FLAG_TEST_AND_NOT(c, CLIENT_FLAG_VISIBLE, CLIENT_FLAG_STICKY) && !(c->transient_for) && ((c->win_workspace != new_ws)))
        {
            clientHide(c, FALSE);
        }
    }

    /* Second pass */
    list_of_windows = g_slist_reverse(list_of_windows);
    for(index = list_of_windows; index; index = g_slist_next(index))
    {
        c = (Client *) index->data;
        if(CLIENT_FLAG_TEST(c, CLIENT_FLAG_STICKY))
        {
            clientSetWorkspace(c, new_ws, TRUE);
        }
        else
        {
            if((c->win_workspace == new_ws) && !(c->transient_for) && !CLIENT_FLAG_TEST(c, CLIENT_FLAG_HIDDEN))
            {
                clientShow(c, FALSE);
                if((!f) && CLIENT_FLAG_TEST(c, CLIENT_FLAG_FOCUS))
                {
                    f = c;
                }
                CLIENT_FLAG_UNSET(c, CLIENT_FLAG_FOCUS);
            }
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
    /* Just get rid of EnterNotify events when using focus follow mouse */
    XSync(dpy, FALSE);
    if(!params.click_to_focus)
    {
        while(XCheckTypedEvent(dpy, EnterNotify, &an_event))
            ;
    }
    clientSetFocus(f, TRUE);
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
    set_net_workarea(dpy, root, params.workspace_count, margins);
}

void workspaceUpdateArea(CARD32 * margins, CARD32 * gnome_margins)
{
    Client *c;
    int i;
    int prev_top = margins[MARGIN_TOP];
    int prev_left = margins[MARGIN_LEFT];
    int prev_right = margins[MARGIN_RIGHT];
    int prev_bottom = margins[MARGIN_BOTTOM];

    TRACE("entering workspaceSetCount");

    for(i = 0; i < 4; i++)
    {
        margins[i] = gnome_margins[i];
    }
    for(c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        if(CLIENT_FLAG_TEST_ALL(c, CLIENT_FLAG_HAS_STRUTS | CLIENT_FLAG_VISIBLE))
        {
            margins[MARGIN_TOP] = MAX(margins[MARGIN_TOP], c->struts[MARGIN_TOP]);
            margins[MARGIN_LEFT] = MAX(margins[MARGIN_LEFT], c->struts[MARGIN_LEFT]);
            margins[MARGIN_RIGHT] = MAX(margins[MARGIN_RIGHT], c->struts[MARGIN_RIGHT]);
            margins[MARGIN_BOTTOM] = MAX(margins[MARGIN_BOTTOM], c->struts[MARGIN_BOTTOM]);
        }
    }
    TRACE("Desktop area computed : (%d,%d,%d,%d)", (int)margins[0], (int)margins[1], (int)margins[2], (int)margins[3]);
    if((prev_top != margins[MARGIN_TOP]) || (prev_left != margins[MARGIN_LEFT]) || (prev_right != margins[MARGIN_RIGHT]) || (prev_bottom != margins[MARGIN_BOTTOM]))
    {
        TRACE("Margins have changed, updating net_workarea");
        set_net_workarea(dpy, screen, params.workspace_count, margins);
    }
}
