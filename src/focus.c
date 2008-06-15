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
static Client *user_focus    = NULL;
static Client *last_ungrab   = NULL;
static Client *delayed_focus = NULL;
static guint focus_timeout   = 0;

#if 0
static void
clientDumpList (ScreenInfo *screen_info)
{
    int i;
    Client *c;

    g_print ("Dumping client list\n");
    for (c = screen_info->clients, i = 0; (c) && (i < screen_info->client_count); c = c->next, i++)
    {
        g_print ("    [%i] 0x%lx - %s\n", i, c->window, c->name);
    }
}
#endif

static ClientPair
clientGetTopMostFocusable (ScreenInfo *screen_info, int layer, GList * exclude_list)
{
    ClientPair top_client;
    Client *c;
    GList *index;

    TRACE ("entering clientGetTopMostFocusable");

    top_client.prefered = top_client.highest = NULL;
    for (index = screen_info->windows_stack; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        TRACE ("*** stack window \"%s\" (0x%lx), layer %i", c->name,
            c->window, (int) c->win_layer);

        if (!clientAcceptFocus (c) || (c->type & WINDOW_TYPE_DONT_FOCUS))
        {
            continue;
        }

        if (!g_list_find (exclude_list, (gconstpointer) c))
        {
            if (c->win_layer > layer)
            {
                break;
            }
            else if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
            {
                if (clientSelectMask (c, 0, WINDOW_REGULAR_FOCUSABLE))
                {
                    top_client.prefered = c;
                }
                top_client.highest = c;
            }
        }
    }

    return top_client;
}

void
clientFocusTop (ScreenInfo *screen_info, int layer, Time timestamp)
{
    ClientPair top_client;
    DisplayInfo *display_info;

    display_info = screen_info->display_info;
    top_client = clientGetTopMostFocusable (screen_info, layer, NULL);
    if (top_client.prefered)
    {
        clientSetFocus (screen_info, top_client.prefered,
                        timestamp,
                        NO_FOCUS_FLAG);
    }
    else
    {
        clientSetFocus (screen_info, top_client.highest,
                        timestamp,
                        NO_FOCUS_FLAG);
    }
}

gboolean
clientFocusNew(Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    gboolean give_focus;
    gboolean prevent_focus_stealing;
    gboolean prevented;

    g_return_val_if_fail (c != NULL, FALSE);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    give_focus = (c-> type & WINDOW_REGULAR_FOCUSABLE) && (screen_info->params->focus_new);
    prevent_focus_stealing = screen_info->params->prevent_focus_stealing;
    prevented = FALSE;

    /*  Try to avoid focus stealing */
    if (!clientAcceptFocus (c) || (c->type & WINDOW_TYPE_DONT_FOCUS))
    {
        give_focus = FALSE;
    }
    else if ((client_focus) && (prevent_focus_stealing))
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STARTUP_TIME) && (c->user_time == (Time) CurrentTime))
        {
            TRACE ("Given startup time is 0, not focusing");
            give_focus = FALSE;
            prevented = TRUE;
        }
        else if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STARTUP_TIME | CLIENT_FLAG_HAS_USER_TIME))
        {
            if (TIMESTAMP_IS_BEFORE (c->user_time, client_focus->user_time))
            {
                TRACE ("Current %u, new %u", (unsigned int) client_focus->user_time, (unsigned int) c->user_time);
                give_focus = FALSE;
                prevented = TRUE;
            }
        }
    }

    if ((give_focus) || FLAG_TEST(c->flags, CLIENT_FLAG_STATE_MODAL))
    {
        give_focus = TRUE;
        if ((client_focus) && !(clientIsTransientOrModalFor (c, client_focus)))
        {
            clientAdjustFullscreenLayer (client_focus, FALSE);
        }
        clientRaise (c, None);
        clientShow (c, TRUE);
        clientSetFocus (screen_info, c,
                        myDisplayGetCurrentTime (display_info),
                        FOCUS_IGNORE_MODAL);
        clientPassGrabMouseButton (c);
    }
    else
    {
        Client *c2 = clientGetFocus();

        if ((prevented) && (c2 != NULL) && (c2->win_layer == c->win_layer))
        {
            TRACE ("clientFocusNew: Setting WM_STATE_DEMANDS_ATTENTION flag on \"%s\" (0x%lx)", c->name, c->window);
            FLAG_SET (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION);
            clientSortRing(c);
            clientLower (c, c2->frame);
            clientSortRing(c2);
            clientSetOpacity (c, c->opacity, 0, 0);
        }
        else
        {
            clientRaise (c, None);
            clientSortRing(c);
        }

        clientShow (c, TRUE);
        clientSetNetState (c);
        clientGrabMouseButton (c);
    }

    return (give_focus);
}

gboolean
clientSelectMask (Client * c, int mask, int type)
{
    g_return_val_if_fail (c != NULL, FALSE);
    TRACE ("entering clientSelectMask");

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
    Client *c2;
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
    Client *c2;
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
clientPassFocus (ScreenInfo *screen_info, Client *c, GList *exclude_list)
{
    DisplayInfo *display_info;
    ClientPair top_most;
    Client *new_focus;
    Client *current_focus;
    Client *c2;
    Window dr, window;
    unsigned int mask;
    int rx, ry, wx, wy;
    int look_in_layer;

    TRACE ("entering clientPassFocus");

    look_in_layer = (c ? c->win_layer : WIN_LAYER_NORMAL);
    new_focus = NULL;
    current_focus = client_focus;
    c2 = NULL;
#if 1
    if (pending_focus)
    {
        current_focus = pending_focus;
    }
#endif
    if ((c || current_focus) && (c != current_focus))
    {
        return;
    }

    if (current_focus == last_ungrab)
    {
        clientPassGrabMouseButton (NULL);
    }

    display_info = screen_info->display_info;
    top_most = clientGetTopMostFocusable (screen_info, look_in_layer, exclude_list);
#if 0
    if (screen_info->params->click_to_focus)
    {
        if (c)
        {
            if (clientIsModal (c))
            {
                /* If the window is a modal, send focus back to its parent
                 * window.
                 * Modals are transients, and we aren't interested in modal
                 * for group, so it safe to use clientGetTransient because
                 * it's really what we want...
                 */
                c2 = clientGetTransient (c);
                if (c2 && FLAG_TEST(c2->xfwm_flags, XFWM_FLAG_VISIBLE))
                {
                    new_focus = c2;
                    /* Usability: raise the parent, to grab user's attention */
                    clientRaise (c2, None);
                }
            }
            else
            {
                c2 = clientGetNext (c, 0);
                if ((c2) && (c2->win_layer >= c->win_layer))
                {
                    new_focus = c2;
                }
            }
        }
    }
    else
#endif
    if (!(screen_info->params->click_to_focus) &&
        XQueryPointer (myScreenGetXDisplay (screen_info), screen_info->xroot, &dr, &window, &rx, &ry, &wx, &wy, &mask))
    {
        new_focus = clientAtPosition (screen_info, rx, ry, exclude_list);
    }
    if (!new_focus)
    {
        new_focus = top_most.prefered ? top_most.prefered : top_most.highest;
    }
    clientSetFocus (screen_info, new_focus,
                    myDisplayGetCurrentTime (display_info),
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
    if ((c->screen_info->params->focus_hint)
        && !FLAG_TEST (c->wm_flags, WM_FLAG_INPUT | WM_FLAG_TAKEFOCUS))
    {
        return FALSE;
    }

    return TRUE;
}

void
clientSortRing(Client *c)
{
    ScreenInfo *screen_info;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSortRing");

    screen_info = c->screen_info;
    if ((screen_info->client_count > 2) && (c != screen_info->clients))
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
clientSetLast(Client *c)
{
    ScreenInfo *screen_info;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetLast");

    screen_info = c->screen_info;
    if (screen_info->client_count > 2)
    {
        if (screen_info->clients == c)
        {
            screen_info->clients = screen_info->clients->next;
        }
        else
        {
            c->prev->next = c->next;
            c->next->prev = c->prev;

            c->prev = screen_info->clients->prev;
            c->next = screen_info->clients;

            screen_info->clients->prev->next = c;
            screen_info->clients->prev = c;
        }
    }
}

void
clientUpdateFocus (ScreenInfo *screen_info, Client * c, unsigned short flags)
{
    DisplayInfo *display_info;
    Client *c2;
    unsigned long data[2];

    TRACE ("entering clientUpdateFocus");

    c2 = ((client_focus != c) ? client_focus : NULL);
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

    /*
       We can release the button mouse grab if we don't raise on click or if the focused window
       is the one that has been raised at last.
     */
    if (!(screen_info->params->raise_on_click) || (c == clientGetLastRaise (screen_info)))
    {
        clientPassGrabMouseButton (c);
    }

    client_focus = c;
    if (c)
    {
        user_focus = c;
        clientInstallColormaps (c);
        if (flags & FOCUS_SORT)
        {
            clientSortRing(c);
        }
        if (FLAG_TEST(c->flags, CLIENT_FLAG_DEMANDS_ATTENTION))
        {
            TRACE ("Un-setting WM_STATE_DEMANDS_ATTENTION flag on \"%s\" (0x%lx)", c->name, c->window);
            FLAG_UNSET (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION);
            clientSetNetState (c);
        }
        data[0] = c->window;
        clientAdjustFullscreenLayer (c, TRUE);
        frameQueueDraw (c, FALSE);
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
            /* clientRaise (c, None); */
        }
        frameQueueDraw (c2, FALSE);
    }
    data[1] = None;
    XChangeProperty (display_info->dpy, screen_info->xroot,
                     display_info->atoms[NET_ACTIVE_WINDOW], XA_WINDOW, 32,
                     PropModeReplace, (unsigned char *) data, 2);
    clientClearDelayedFocus ();
    clientUpdateOpacity (screen_info, c);
}

void
clientSetFocus (ScreenInfo *screen_info, Client *c, Time timestamp, unsigned short flags)
{
    DisplayInfo *display_info;
    Client *c2;

    TRACE ("entering clientSetFocus");

    display_info = screen_info->display_info;
    c2 = NULL;
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
        TRACE ("setting focus to client \"%s\" (0x%lx) with timestamp %u", c->name, c->window, (unsigned int) timestamp);
        user_focus = c;
        if (FLAG_TEST(c->flags, CLIENT_FLAG_DEMANDS_ATTENTION))
        {
            TRACE ("Un-setting WM_STATE_DEMANDS_ATTENTION flag on \"%s\" (0x%lx)", c->name, c->window);
            FLAG_UNSET (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION);
            clientSetNetState (c);
        }
        if ((c == client_focus) && !(flags & FOCUS_FORCE))
        {
            TRACE ("client \"%s\" (0x%lx) is already focused, ignoring request", c->name, c->window);
            return;
        }
        if (!clientAcceptFocus (c))
        {
            TRACE ("SKIP_FOCUS set for client \"%s\" (0x%lx)", c->name, c->window);
            return;
        }
        if (FLAG_TEST (c->wm_flags, WM_FLAG_INPUT) || !(screen_info->params->focus_hint))
        {
            pending_focus = c;
            /*
             * When shaded, the client window is unmapped, so it can not be focused.
             * Instead, we focus the frame that is still mapped.
             */
            if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
            {
                XSetInputFocus (myScreenGetXDisplay (screen_info), c->frame, RevertToPointerRoot, timestamp);
            }
            else
            {
                XSetInputFocus (myScreenGetXDisplay (screen_info), c->window, RevertToPointerRoot, timestamp);
            }
            clientUpdateOpacity (screen_info, c);
        }
        else
        {
            /*
             * If we are relying only on the client application to take focus, we need to set the focus
             * explicitely on our own fallback window otherwise there is a race condition between the
             * application and the window manager. If the application does not take focus before the
             * the previously focused window is unmapped (when iconifying or closing for example), the focus
             * will be reverted to the root window and focus transition will fail.
             */
            XSetInputFocus (myScreenGetXDisplay (screen_info), screen_info->xfwm4_win, RevertToPointerRoot, timestamp);
        }

        if (FLAG_TEST(c->wm_flags, WM_FLAG_TAKEFOCUS))
        {
            pending_focus = c;
            sendClientMessage (c->screen_info, c->window, WM_TAKE_FOCUS, timestamp);
        }
    }
    else
    {
        unsigned long data[2];

        TRACE ("setting focus to none");

        data[0] = data[1] = None;
        client_focus = NULL;
        pending_focus = NULL;

        if (c2)
        {
            frameQueueDraw (c2, FALSE);
            XChangeProperty (clientGetXDisplay (c2), c2->screen_info->xroot, display_info->atoms[NET_ACTIVE_WINDOW], XA_WINDOW, 32,
                             PropModeReplace, (unsigned char *) data, 2);
        }
        XChangeProperty (myScreenGetXDisplay (screen_info), screen_info->xroot, display_info->atoms[NET_ACTIVE_WINDOW], XA_WINDOW, 32,
                         PropModeReplace, (unsigned char *) data, 2);
        XSetInputFocus (myScreenGetXDisplay (screen_info), screen_info->xfwm4_win, RevertToPointerRoot, timestamp);
        clientClearDelayedFocus ();
        clientUpdateOpacity (screen_info, c);
    }
}

void
clientInitFocusFlag (Client * c)
{
    ScreenInfo *screen_info;
    Client *c2;
    GList *index;
    int workspace;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetFocus");

    if (!clientAcceptFocus (c) || (c->type & WINDOW_TYPE_DONT_FOCUS))
    {
       return;
    }

    screen_info = c->screen_info;
    workspace = c->win_workspace;
    for (index = screen_info->windows_stack; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if ((c2->win_workspace == workspace) && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_FOCUS))
        {
            FLAG_UNSET (c2->xfwm_flags, XFWM_FLAG_FOCUS);
        }
    }
    FLAG_SET (c->xfwm_flags, XFWM_FLAG_FOCUS);
}

Client *
clientGetFocus (void)
{
    return (client_focus);
}

Client *
clientGetFocusPending (void)
{
    return (pending_focus);
}

Client *
clientGetFocusOrPending (void)
{
    if (client_focus)
    {
        return (client_focus);
    }
    return (pending_focus);
}

Client *
clientGetUserFocus (void)
{
    return (user_focus);
}

void
clientClearFocus (Client *c)
{
    if ((c == NULL) || (c == client_focus))
    {
        client_focus = NULL;
    }
    if ((c == NULL) || (c == pending_focus))
    {
        pending_focus = NULL;
    }
    if ((c == NULL) || (c == user_focus))
    {
        user_focus = NULL;
    }
}

void
clientGrabMouseButton (Client * c)
{
    ScreenInfo *screen_info;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGrabMouseButton");
    TRACE ("grabbing buttons for client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    if (screen_info->params->raise_with_any_button)
    {
        grabButton(clientGetXDisplay (c), AnyButton, AnyModifier, c->window);
    }
    else
    {
        grabButton(clientGetXDisplay (c), Button1, AnyModifier, c->window);
    }
}

void
clientUngrabMouseButton (Client * c)
{
    ScreenInfo *screen_info;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUngrabMouseButton");
    TRACE ("ungrabing buttons for client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    ungrabButton(clientGetXDisplay (c), AnyButton, AnyModifier, c->window);
    /* We've ungrabbed way too much, so regrab the regular buttons/modifiers */
    clientGrabButtons (c);
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

    TRACE ("ungrabing buttons for client \"%s\" (0x%lx)", c->name, c->window);
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

static gboolean
delayed_focus_cb (gpointer data)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    TRACE ("entering delayed_focus_cb");
    g_return_val_if_fail (delayed_focus != NULL, FALSE);

    screen_info = delayed_focus->screen_info;
    display_info = screen_info->display_info;
    clientSetFocus (screen_info, delayed_focus, myDisplayGetCurrentTime (display_info), NO_FOCUS_FLAG);
    focus_timeout = 0;
    delayed_focus = NULL;

    return (FALSE);
}

void
clientClearDelayedFocus (void)
{
    if(focus_timeout)
    {
        g_source_remove (focus_timeout);
        focus_timeout = 0;
    }
    delayed_focus = NULL;
}

void
clientAddDelayedFocus (Client *c)
{
    ScreenInfo *screen_info;

    screen_info = c->screen_info;
    delayed_focus = c;
    focus_timeout = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                        screen_info->params->focus_delay,
                                        (GSourceFunc) delayed_focus_cb,
                                        NULL, NULL);
}

Client *
clientGetDelayedFocus (void)
{
    return delayed_focus;
}
