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
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <gtk/gtk.h>
#include <glib.h>
#include "main.h"
#include "workspaces.h"
#include "client.h"
#include "hints.h"
#include "debug.h"

int workspace_count = -1, wrap_workspaces;
int workspace;

void workspaceSwitch(int new_ws, Client * c2)
{
    Client *c, *f = NULL;
    Client *last;
    unsigned long data[1];
    int i;

    DBG("entering workspaceSwitch\n");

    if((new_ws < 0) && wrap_workspaces)
    {
        new_ws = workspace_count - 1;
    }
    if((new_ws > workspace_count - 1) && wrap_workspaces)
    {
        new_ws = 0;
    }
    if((new_ws < 0) || (new_ws > workspace_count - 1) || (new_ws == workspace))
    {
        return;
    }

    f = clientGetFocus();
    if(f)
    {
        f->focus = True;
    }
    
    if(c2)
    {
        setGnomeHint(dpy, c2->window, win_workspace, new_ws);
        data[0] = new_ws;
        XChangeProperty (dpy, c2->window, net_wm_desktop, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 1);
        c2->win_workspace = new_ws;
    }

    setGnomeHint(dpy, root, win_workspace, new_ws);
    data[0] = new_ws;
    XChangeProperty (dpy, root, net_current_desktop, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 1);
    workspace = new_ws;
    
    /* 
       Do the switch in two passes :
       - The first one, unmapping windows from bottom to top 
       - The second pass, mapping from top to bottom
       
       ==> this save a lot of expose events and make things a lot faster ;-)
     */
     
    /* First pass */
    for(last = clients, i = 0; i < client_count; last = last->next, i++);
    for(c = last, i = 0; i < client_count; c = c->prev, i++)
    {
        if((c->visible) && !(c->sticky) && ((c->win_workspace != new_ws)))
	{
            clientHide(c, False);
	}
    }
    /* Second pass */
    for(c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        if(c->sticky)
	{
            clientSetWorkspace(c, new_ws);
        }
	else
        {
            if((c->win_workspace == new_ws) && !(c->hidden))
            {
                clientShow(c, False);
                if(c->focus)
		{
                    f = c;
                }
		c->focus = False;
            }
        }
    }
    if(c2)
    {
        f = c2;
	clientRaise (c2);
    }
    workspaceUpdateArea(margins, gnome_margins);
    clientSetFocus(f, True);
    XSync (dpy, 0);
}

void workspaceSetCount(int count)
{
    Client *c;
    int i;
    unsigned long data[1];

    DBG("entering workspaceSetCount\n");

    if(count < 1)
    {
        count = 1;
    }
    if(count == workspace_count)
    {
        return;
    }

    setGnomeHint(dpy, root, win_workspace_count, count);
    data[0] = count;
    XChangeProperty (dpy, root, net_number_of_desktops, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 1);
    workspace_count = count;

    for(c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        if(c->win_workspace > count - 1)
	{
            clientSetWorkspace(c, count - 1);
	}
    }
    if(workspace > count - 1)
    {
        workspaceSwitch(count - 1, NULL);
    }
}

void workspaceUpdateArea(CARD32 *margins, CARD32 *gnome_margins)
{
    Client *c;
    int i;
    int prev_top    = margins[MARGIN_TOP];
    int prev_left   = margins[MARGIN_LEFT];
    int prev_right  = margins[MARGIN_RIGHT];
    int prev_bottom = margins[MARGIN_BOTTOM];

    DBG("entering workspaceSetCount\n");

    for (i = 0; i < 4; i++)
    {
        margins[i] = gnome_margins[i];
    }
    for(c = clients, i = 0; i < client_count; c = c->next, i++)
    {
        if((c->has_struts) && (c->visible))
	{
	    margins[MARGIN_TOP]    = MAX(margins[MARGIN_TOP],    c->struts[MARGIN_TOP]);
	    margins[MARGIN_LEFT]   = MAX(margins[MARGIN_LEFT],   c->struts[MARGIN_LEFT]);
	    margins[MARGIN_RIGHT]  = MAX(margins[MARGIN_RIGHT],  c->struts[MARGIN_RIGHT]);
	    margins[MARGIN_BOTTOM] = MAX(margins[MARGIN_BOTTOM], c->struts[MARGIN_BOTTOM]);
	}
    }
    DBG("Desktop area computed : (%d,%d,%d,%d)\n", margins[0], margins[1], margins[2], margins[3]);
    if ((prev_top != margins[MARGIN_TOP]) || (prev_left != margins[MARGIN_LEFT]) || (prev_right != margins[MARGIN_RIGHT]) || (prev_bottom != margins[MARGIN_BOTTOM]))
    {
        DBG("Margins have changed, updating net_workarea\n");
        set_net_workarea (dpy, root, margins);
    }
}
