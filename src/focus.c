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
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <sys/time.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h> 
#include <libxfcegui4/libxfcegui4.h>

#include "screen.h"
#include "focus.h"
#include "misc.h"
#include "client.h"
#include "frame.h"
#include "stacking.h"
#include "transients.h"
#include "workspaces.h"
#include "hints.h"
#include "netwm.h"

typedef struct _ClientPair ClientPair;
struct _ClientPair
{
    Client *prefered;
    Client *highest;
};

static Client *client_focus  = NULL;
static Client *pending_focus = NULL;
static Client *last_ungrab   = NULL;

static ClientPair
clientGetTopMostFocusable (ScreenInfo *screen_info, int layer, Client * exclude)
{
    ClientPair top_client;
    Client *c = NULL;
    GList *index = NULL;

    TRACE ("entering clientGetTopMostFocusable");

    top_client.prefered = top_client.highest = NULL;
    for (index = screen_info->windows_stack; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        TRACE ("*** stack window \"%s\" (0x%lx), layer %i", c->name,
            c->window, (int) c->win_layer);

        if ((c->type & (WINDOW_SPLASHSCREEN | WINDOW_DOCK | WINDOW_DESKTOP))
            || ((layer != WIN_LAYER_DESKTOP) && (c->type & WINDOW_DESKTOP)))
        {
            continue;
        }
        
        if (!exclude || (c != exclude))
        {
            if ((c->win_layer <= layer) && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
            {
                if (clientSelectMask (c, 0, WINDOW_REGULAR_FOCUSABLE))
                {
                    top_client.prefered = c;
                }
                top_client.highest = c;
            }
            else if (c->win_layer > layer)
            {
                break;
            }
        }
    }

    return top_client;
}

void
clientFocusTop (ScreenInfo *screen_info, int layer)
{
    ClientPair top_client;
    DisplayInfo *display_info = NULL;

    display_info = screen_info->display_info;
    top_client = clientGetTopMostFocusable (screen_info, layer, NULL);
    if (top_client.prefered)
    {
        clientSetFocus (screen_info, top_client.prefered, 
                        myDisplayGetCurrentTime (screen_info->display_info), 
                        NO_FOCUS_FLAG);
    }
    else
    {
        clientSetFocus (screen_info, top_client.highest, 
                        myDisplayGetCurrentTime (screen_info->display_info), 
                        NO_FOCUS_FLAG);
    }
}

void
clientFocusNew(Client * c)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;
    gboolean give_focus;
    gboolean prevent_focus_stealing;

    g_return_if_fail (c != NULL);
    
    if (!clientAcceptFocus (c)|| (c->type & WINDOW_TYPE_DONT_FOCUS))
    {
        return;
    }
    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    give_focus = screen_info->params->focus_new;
    prevent_focus_stealing = screen_info->params->prevent_focus_stealing;

    /*  Try to avoid focus stealing */
    if ((client_focus) && (prevent_focus_stealing))
    {
        if (FLAG_TEST(c->flags, CLIENT_FLAG_HAS_USER_TIME) &&
            FLAG_TEST(client_focus->flags, CLIENT_FLAG_HAS_USER_TIME))
        {
            TRACE ("Current %u, new %u", client_focus->user_time, c->user_time);
            if (c->user_time < client_focus->user_time)
            {
                give_focus = FALSE;
            }
        }
    }

    clientGrabMouseButton (c);
    if (give_focus || FLAG_TEST(c->flags, CLIENT_FLAG_STATE_MODAL))
    {
        if (client_focus)
        {
            if (clientAdjustFullscreenLayer (client_focus, FALSE))
            {
                clientRaise (c);
            }
        }
        clientSetFocus (screen_info, c, 
                        CurrentTime, 
                        FOCUS_IGNORE_MODAL);
    }
    else
    {
        FLAG_SET (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION);
        clientSetNetState (c);
        clientGrabMouseButton (c);
    }
}

gboolean
clientSelectMask (Client * c, int mask, int type)
{
    TRACE ("entering clientSelectMask");

    g_return_val_if_fail (c != NULL, FALSE);

    if ((!clientAcceptFocus (c)) && !(mask & INCLUDE_SKIP_FOCUS))
    {
        return FALSE;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED) && !(mask & INCLUDE_HIDDEN))
    {
        return FALSE;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_PAGER)
        && !(mask & INCLUDE_SKIP_PAGER))
    {
        return FALSE;
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_TASKBAR)
        && !(mask & INCLUDE_SKIP_TASKBAR))
    {
        return FALSE;
    }
    if ((c->win_workspace != c->screen_info->current_ws) && !(mask & INCLUDE_ALL_WORKSPACES))
    {
        return FALSE;
    }
    if (c->type & type)
    {
        return TRUE;
    }
    return FALSE;
}

Client *
clientGetNext (Client * c, int mask)
{
    Client *c2 = NULL;
    unsigned int i;

    TRACE ("entering clientGetNext");

    if (c)
    {
        ScreenInfo *screen_info = c->screen_info;
        for (c2 = c->next, i = 0; (c2) && (i < screen_info->client_count - 1);
            c2 = c2->next, i++)
        {
            if (clientSelectMask (c2, mask, WINDOW_REGULAR_FOCUSABLE))
            {
                return c2;
            }
        }
    }
    return NULL;
}

Client *
clientGetPrevious (Client * c, int mask)
{
    Client *c2 = NULL;
    unsigned int i;

    TRACE ("entering clientGetPrevious");

    if (c)
    {
        ScreenInfo *screen_info = c->screen_info;
        for (c2 = c->prev, i = 0; (c2) && (i < screen_info->client_count);
            c2 = c2->prev, i++)
        {
            if (clientSelectMask (c2, mask, WINDOW_REGULAR_FOCUSABLE))
            {
                return c2;
            }
        }
    }
    return NULL;
}

void
clientPassFocus (ScreenInfo *screen_info, Client *c, Client *exclude)
{
    DisplayInfo *display_info = NULL;
    Client *new_focus = NULL;
    Client *current_focus = client_focus;
    ClientPair top_most;
    Client *c2 = NULL;
    Window dr, window;
    int rx, ry, wx, wy;
    unsigned int mask;
    int look_in_layer = (c ? c->win_layer : WIN_LAYER_NORMAL);

    TRACE ("entering clientPassFocus");

    if (pending_focus)
    {
        current_focus = pending_focus;
    }

    if ((c || current_focus) && (c != current_focus))
    {
        return;
    }

    display_info = screen_info->display_info;
    top_most = clientGetTopMostFocusable (screen_info, look_in_layer, exclude);
    if (screen_info->params->click_to_focus)
    {
        if (c)
        {
            if (clientIsModal (c))
            {
                /* If the window is a modal, send focus back to its parent window
                   Modals are transients, and we aren"t interested in modal
                   for group, so it safe to use clientGetTransient because 
                   it's really what we want...
                 */

                c2 = clientGetTransient (c);
                if (c2 && FLAG_TEST(c2->xfwm_flags, XFWM_FLAG_VISIBLE))
                {
                    new_focus = c2;
                    /* Usability: raise the parent, to grab user's attention */
                    clientRaise (c2);
                }
            }
            else
            {
                c2 = clientGetNext (c, 0);
                /* Send focus back to the previous window only if it's on a different
                   layer, to avoid sending focus back to a lowered window....
                 */
                if ((c2) && (c2->win_layer != c->win_layer))
                {
                    new_focus = c2;
                }
            }
        }
    }
    else if (XQueryPointer (myScreenGetXDisplay (screen_info), screen_info->xroot, &dr, &window, &rx, &ry, &wx, &wy, &mask))
    {
        new_focus = clientAtPosition (screen_info, rx, ry, exclude);
    }
    if (!new_focus)
    {
        new_focus = top_most.prefered ? top_most.prefered : top_most.highest;
    }
    clientSetFocus (screen_info, new_focus, 
                    myDisplayGetCurrentTime (screen_info->display_info), 
                    FOCUS_IGNORE_MODAL | FOCUS_FORCE);
}

gboolean
clientAcceptFocus (Client * c)
{
    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientAcceptFocus");

    /* Modal dialogs *always* accept focus */
    if (FLAG_TEST(c->flags, CLIENT_FLAG_STATE_MODAL))
    {
        return TRUE; 
    }
    /* First check GNOME protocol */
    if (c->win_hints & WIN_HINTS_SKIP_FOCUS)
    {
        return FALSE;
    }
    if (!FLAG_TEST (c->wm_flags, WM_FLAG_INPUT | WM_FLAG_TAKEFOCUS))
    {
        return FALSE;
    }
    
    return TRUE;
}

void
clientSortRing(Client *c)
{
    ScreenInfo *screen_info = NULL;
    
    g_return_if_fail (c != NULL);

    TRACE ("Sorting...");
    screen_info = c->screen_info;
    if (screen_info->client_count > 2 && c != screen_info->clients)
    {
        c->prev->next = c->next;
        c->next->prev = c->prev;

        c->prev = screen_info->clients->prev;
        c->next = screen_info->clients;
        screen_info->clients->prev->next = c;
        screen_info->clients->prev = c;
    }
    screen_info->clients = c;
}

void
clientUpdateFocus (ScreenInfo *screen_info, Client * c, unsigned short flags)
{
    DisplayInfo *display_info = NULL;
    Client *c2 = ((client_focus != c) ? client_focus : NULL);
    unsigned long data[2];

    TRACE ("entering clientUpdateFocus");

    display_info = screen_info->display_info;
    pending_focus = NULL;
    if ((c) && !clientAcceptFocus (c))
    {
        TRACE ("SKIP_FOCUS set for client \"%s\" (0x%lx)", c->name, c->window);
        return;
    }

    if ((c) && (c == client_focus) && !(flags & FOCUS_FORCE))
    {
        TRACE ("client \"%s\" (0x%lx) is already focused, ignoring request", c->name, c->window);
        return;
    }

    if (c == clientGetLastRaise (screen_info))
    {
        clientPassGrabMouseButton (c);
    }

    client_focus = c;
    if (c)
    {
        clientInstallColormaps (c);
        if (flags & FOCUS_SORT)
        {
            clientSortRing(c);
        }
        data[0] = c->window;
        clientAdjustFullscreenLayer (c, TRUE);
        frameDraw (c, FALSE, FALSE);
    }
    else
    {
        data[0] = None;
    }
    if (c2)
    {
        if (c)
        {
            clientAdjustFullscreenLayer (c2, FALSE);
            /* clientRaise (c); */
        }
        frameDraw (c2, FALSE, FALSE);
    }
    data[1] = None;
    XChangeProperty (display_info->dpy, screen_info->xroot, 
                     display_info->atoms[NET_ACTIVE_WINDOW], XA_WINDOW, 32,
                     PropModeReplace, (unsigned char *) data, 2);
}

void
clientSetFocus (ScreenInfo *screen_info, Client * c, Time timestamp, unsigned short flags)
{
    DisplayInfo *display_info = NULL;
    Client *c2 = NULL;

    TRACE ("entering clientSetFocus");
    
    display_info = screen_info->display_info;
    if ((c) && !(flags & FOCUS_IGNORE_MODAL))
    {
        c2 = clientGetModalFor (c);

        if (c2)
        {
            c = c2;
        }
    }
    c2 = ((client_focus != c) ? client_focus : NULL);
    if ((c) && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
    {
        TRACE ("setting focus to client \"%s\" (0x%lx)", c->name, c->window);
        if ((c == client_focus) && !(flags & FOCUS_FORCE))
        {
            TRACE ("client \"%s\" (0x%lx) is already focused, ignoring request",
                c->name, c->window);
            return;
        }        
        if (!clientAcceptFocus (c))
        {
            TRACE ("SKIP_FOCUS set for client \"%s\" (0x%lx)", c->name, c->window);
            return;
        }
        if (FLAG_TEST (c->wm_flags, WM_FLAG_INPUT))
        {
            pending_focus = c;
            XSetInputFocus (myScreenGetXDisplay (screen_info), c->window, RevertToPointerRoot, timestamp);
        }
        if (FLAG_TEST(c->wm_flags, WM_FLAG_TAKEFOCUS))
        {
            sendClientMessage (c->screen_info, c->window, WM_TAKE_FOCUS, timestamp);
        }
        if (FLAG_TEST(c->flags, CLIENT_FLAG_DEMANDS_ATTENTION))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION);
            clientSetNetState (c);
        }
    }
    else
    {
        unsigned long data[2];
        
        TRACE ("setting focus to none");
        
        data[0] = data[1] = None;
        client_focus = NULL;
        if (c2)
        {
            frameDraw (c2, FALSE, FALSE);
            XChangeProperty (clientGetXDisplay (c2), c2->screen_info->xroot, display_info->atoms[NET_ACTIVE_WINDOW], XA_WINDOW, 32,
                             PropModeReplace, (unsigned char *) data, 2);
        }
        XChangeProperty (myScreenGetXDisplay (screen_info), screen_info->xroot, display_info->atoms[NET_ACTIVE_WINDOW], XA_WINDOW, 32,
                         PropModeReplace, (unsigned char *) data, 2);
        XSetInputFocus (myScreenGetXDisplay (screen_info), screen_info->gnome_win, RevertToPointerRoot, timestamp);
    }
}

Client *
clientGetFocus (void)
{
    return (client_focus);
}

void
clientClearFocus (void)
{
    client_focus = NULL;
}

void
clientGrabMouseButton (Client * c)
{
    ScreenInfo *screen_info = NULL;
    
    g_return_if_fail (c != NULL);
    TRACE ("entering clientGrabMouseButton");
    TRACE ("grabbing buttons for client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    if (screen_info->params->raise_with_any_button)
    {
        grabButton(clientGetXDisplay (c), AnyButton, 0, c->window);
    }
    else
    {
        grabButton(clientGetXDisplay (c), Button1, 0, c->window);
    }
}

void
clientUngrabMouseButton (Client * c)
{
    ScreenInfo *screen_info = NULL;
    
    g_return_if_fail (c != NULL);
    TRACE ("entering clientUngrabMouseButton");
    TRACE ("ungrabing buttons for client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    if (screen_info->params->raise_with_any_button)
    {
        ungrabButton(clientGetXDisplay (c), AnyButton, 0, c->window);
    }
    else
    {
        ungrabButton(clientGetXDisplay (c), Button1, 0, c->window);
    }
}

void
clientGrabMouseButtonForAll (ScreenInfo *screen_info)
{
    Client *c;
    int i;
    
    g_return_if_fail (screen_info != NULL);
    TRACE ("entering clientGrabMouseButtonForAll");

    for (c = screen_info->clients, i = 0; (c) && (i < screen_info->client_count); c = c->next, i++)
    {
        clientGrabMouseButton (c);
    }
    clientClearLastUngrab ();
}

void
clientUngrabMouseButtonForAll (ScreenInfo *screen_info)
{
    Client *c;
    int i;
    
    g_return_if_fail (screen_info != NULL);
    TRACE ("entering clientUngrabMouseButtonForAll");

    for (c = screen_info->clients, i = 0; (c) && (i < screen_info->client_count); c = c->next, i++)
    {
        clientUngrabMouseButton (c);
    }
    clientClearLastUngrab ();
}

void
clientPassGrabMouseButton (Client * c)
{
    TRACE ("entering clientPassMouseGrabButton");
    TRACE ("ungrabing buttons for client \"%s\" (0x%lx)", c->name, c->window);

    if (c == NULL)
    {
        if (last_ungrab)
        {
            clientGrabMouseButton (last_ungrab);
        }
        last_ungrab = NULL;
        return;
    }

    if (last_ungrab == c)
    {
        return;
    }

    if (last_ungrab)
    {
        clientGrabMouseButton (last_ungrab);
    }

    clientUngrabMouseButton (c);
    last_ungrab = c;
}

Client *
clientGetLastUngrab (void)
{
    return last_ungrab;
}

void
clientClearLastUngrab (void)
{
    last_ungrab = NULL;
}
