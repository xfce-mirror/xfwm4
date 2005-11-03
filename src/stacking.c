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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <glib.h>
#include <libxfce4util/libxfce4util.h> 

#include "display.h"
#include "screen.h"
#include "mywindow.h"
#include "client.h"
#include "stacking.h"
#include "netwm.h"
#include "transients.h"
#include "frame.h"
#include "focus.h"

void
clientApplyStackList (ScreenInfo *screen_info)
{
    Window *xwinstack;
    guint nwindows;
    gint i = 0;

    DBG ("applying stack list");
    nwindows = g_list_length (screen_info->windows_stack);

    xwinstack = g_new (Window, nwindows + 4);
    xwinstack[i++] = MYWINDOW_XWINDOW (screen_info->sidewalk[0]);
    xwinstack[i++] = MYWINDOW_XWINDOW (screen_info->sidewalk[1]);
    xwinstack[i++] = MYWINDOW_XWINDOW (screen_info->sidewalk[2]);
    xwinstack[i++] = MYWINDOW_XWINDOW (screen_info->sidewalk[3]);

    if (nwindows)
    {
        GList *index = NULL;
        Client *c = NULL;

        for (index = g_list_last(screen_info->windows_stack); index; index = g_list_previous (index))
        {
            c = (Client *) index->data;
            xwinstack[i++] = c->frame;
            DBG ("  [%i] \"%s\" (0x%lx)", i, c->name, c->window);
        }
    }

    XRestackWindows (myScreenGetXDisplay (screen_info), xwinstack, (int) nwindows + 4);

    g_free (xwinstack);
}

Client *
clientGetLowestTransient (Client * c)
{
    Client *lowest_transient = NULL, *c2;
    GList *index = NULL;
    ScreenInfo *screen_info = NULL;

    g_return_val_if_fail (c != NULL, NULL);

    TRACE ("entering clientGetLowestTransient");

    screen_info = c->screen_info;
    for (index = screen_info->windows_stack; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if ((c2 != c) && clientIsTransientFor (c2, c))
        {
            lowest_transient = c2;
            break;
        }
    }
    return lowest_transient;
}

Client *
clientGetHighestTransientOrModalFor (Client * c)
{
    Client *highest_transient = NULL;
    Client *c2 = NULL;
    GList *index = NULL;
    ScreenInfo *screen_info = NULL;

    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering clientGetHighestTransientOrModalFor");

    screen_info = c->screen_info;
    for (index = screen_info->windows_stack; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if (c2)
        {
            if (clientIsTransientOrModalFor (c, c2))
            {
                highest_transient = c2;
            }
        }
    }

    return highest_transient;
}

Client *
clientGetTopMostForGroup (Client * c)
{
    Client *top_most = NULL;
    Client *c2 = NULL;
    GList *index = NULL;
    ScreenInfo *screen_info = NULL;

    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering clientGetTopMostForGroup");

    screen_info = c->screen_info;
    for (index = screen_info->windows_stack; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if (c2 != c)
        {
            if (clientSameGroup (c, c2))
            {
                top_most = c2;
            }
        }
    }

    return top_most;
}

gboolean
clientIsTopMost (Client *c)
{
    ScreenInfo *screen_info;
    GList *index = NULL;
    
    g_return_val_if_fail (c != NULL, FALSE);
    TRACE ("entering clientIsTopMost");
    
    screen_info = c->screen_info;
    
    index = g_list_find (screen_info->windows_stack, (gconstpointer) c);
    if (index)
    {
        GList *index2 = g_list_next (index);
        if (index2)
        {
            Client *c2 = (Client *) index2->data;
            return (c2->win_layer > c->win_layer);
        }
        else
        {
            return TRUE;
        }
    }
    return FALSE;
}

Client *
clientGetNextTopMost (ScreenInfo *screen_info, int layer, Client * exclude)
{
    Client *top = NULL, *c;
    GList *index = NULL;

    TRACE ("entering clientGetNextTopMost");

    for (index = screen_info->windows_stack; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        TRACE ("*** stack window \"%s\" (0x%lx), layer %i", c->name, c->window, (int) c->win_layer);
        if (!exclude || (c != exclude))
        {
            if (c->win_layer > layer)
            {
                top = c;
                break;
            }
        }
    }

    return top;
}

Client *
clientGetBottomMost (ScreenInfo *screen_info, int layer, Client * exclude)
{
    Client *bot = NULL, *c;
    GList *index = NULL;

    TRACE ("entering clientGetBottomMost");

    for (index = screen_info->windows_stack; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        if (c)
        {
            TRACE ("*** stack window \"%s\" (0x%lx), layer %i", c->name,
                c->window, (int) c->win_layer);
            if (!exclude || (c != exclude))
            {
                if (c->win_layer < layer)
                {
                    bot = c;
                }
                else if (c->win_layer >= layer)
                {
                    break;
                }
            }
        }
    }
    return bot;
}

Client *
clientAtPosition (ScreenInfo *screen_info, int x, int y, Client * exclude)
{
    /* This function does the same as XQueryPointer but w/out the race
       conditions caused by querying the X server
     */
    GList *index = NULL;
    Client *c = NULL;
    Client *c2 = NULL;

    TRACE ("entering clientAtPosition");

    for (index = g_list_last (screen_info->windows_stack); index; index = g_list_previous (index))
    {
        c2 = (Client *) index->data;
        if ((frameX (c2) < x) && (frameX (c2) + frameWidth (c2) > x)
            && (frameY (c2) < y) && (frameY (c2) + frameHeight (c2) > y))
        {
            if (clientSelectMask (c2, 0, WINDOW_REGULAR_FOCUSABLE) 
                && (c2 != exclude))
            {
                c = c2;
        	break;
            }
        }
    }

    return c;
}

void
clientRaise (Client * c)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientRaise");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (c == screen_info->last_raise)
    {
        TRACE ("client \"%s\" (0x%lx) already raised", c->name, c->window);
        return;
    }
    TRACE ("raising client \"%s\" (0x%lx)", c->name, c->window);

    if (c == clientGetFocus ())
    {
        clientPassGrabMouseButton (c);
    }

    if (g_list_length (screen_info->windows_stack) < 1)
    {
        return;
    }

    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MANAGED))
    {
        Client *c2, *c3;
        Client *client_sibling = NULL;
        GList *transients = NULL;
        GList *sibling = NULL;
        GList *index1, *index2;
        GList *windows_stack_copy;

        /* Copy the existing window stack temporarily as reference */
        windows_stack_copy = g_list_copy (screen_info->windows_stack);
        /* Search for the window that will be just on top of the raised window (layers...) */
        client_sibling = clientGetNextTopMost (screen_info, c->win_layer, c);
        screen_info->windows_stack = g_list_remove (screen_info->windows_stack, (gconstpointer) c);
        if (client_sibling)
        {
            /* If there is one, look for its place in the list */
            sibling = g_list_find (screen_info->windows_stack, (gconstpointer) client_sibling);
            /* Place the raised window just before it */
            screen_info->windows_stack = g_list_insert_before (screen_info->windows_stack, sibling, c);
        }
        else
        {
            /* There will be no window on top of the raised window, so place it at the end of list */
            screen_info->windows_stack = g_list_append (screen_info->windows_stack, c);
        }
        /* Now, look for transients, transients of transients, etc. */
        for (index1 = windows_stack_copy; index1; index1 = g_list_next (index1))
        {
            c2 = (Client *) index1->data;
            if (c2)
            {
                if ((c2 != c) && clientIsTransientOrModalFor (c2, c) && (c2->win_layer <= c->win_layer))
                {
                    transients = g_list_append (transients, c2);
                    if (sibling)
                    {
                        /* Make sure client_sibling is not c2 otherwise we create a circular linked list */
                        if (client_sibling != c2)
                        {
                            /* Place the transient window just before sibling */
                            screen_info->windows_stack = g_list_remove (screen_info->windows_stack, (gconstpointer) c2);
                            screen_info->windows_stack = g_list_insert_before (screen_info->windows_stack, sibling, c2);
                        }
                    }
                    else
                    {
                        /* There will be no window on top of the transient window, so place it at the end of list */
                        screen_info->windows_stack = g_list_remove (screen_info->windows_stack, (gconstpointer) c2);
                        screen_info->windows_stack = g_list_append (screen_info->windows_stack, c2);
                    }
                }
                else
                {
                    for (index2 = transients; index2; index2 = g_list_next (index2))
                    {
                        c3 = (Client *) index2->data;
                        if ((c3 != c2) && clientIsTransientOrModalFor (c2, c3))
                        {
                            transients = g_list_append (transients, c2);
                            if (sibling)
                            {
                                /* Again, make sure client_sibling is not c2 to avoid a circular linked list */
                                if (client_sibling != c2)
                                {
                                    /* Place the transient window just before sibling */
                                    screen_info->windows_stack = g_list_remove (screen_info->windows_stack, (gconstpointer) c2);
                                    screen_info->windows_stack = g_list_insert_before (screen_info->windows_stack, sibling, c2);
                                }
                            }
                            else
                            {
                                /* There will be no window on top of the transient window, so place it at the end of list */
                                screen_info->windows_stack = g_list_remove (screen_info->windows_stack, (gconstpointer) c2);
                                screen_info->windows_stack = g_list_append (screen_info->windows_stack, c2);
                            }
                            break;
                        }
                    }
                }
            }
        }
        if (transients)
        {
            g_list_free (transients);
        }
        if (windows_stack_copy)
        {
            g_list_free (windows_stack_copy);
        }
        /* Now, screen_info->windows_stack contains the correct window stack
           We still need to tell the X Server to reflect the changes
         */
        clientApplyStackList (screen_info);
        clientSetNetClientList (c->screen_info, display_info->atoms[NET_CLIENT_LIST_STACKING], screen_info->windows_stack);
        screen_info->last_raise = c;
    }
}

void
clientLower (Client * c)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientLower");
    TRACE ("lowering client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (g_list_length (screen_info->windows_stack) < 1)
    {
        return;
    }

    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MANAGED))
    {
        Client *client_sibling = NULL;

        if (clientIsTransientOrModalForGroup (c))
        {
            client_sibling = clientGetTopMostForGroup (c);
        }
        else if (clientIsTransient (c))
        {
            client_sibling = clientGetTransient (c);
        }
        if ((!client_sibling) || 
            (client_sibling && (client_sibling->win_layer < c->win_layer)))
        {
            client_sibling = clientGetBottomMost (screen_info, c->win_layer, c);
        }
        if (client_sibling != c)
        {
            screen_info->windows_stack = g_list_remove (screen_info->windows_stack, (gconstpointer) c);
            /* Paranoid check to avoid circular linked list */
            if (client_sibling)
            {
                GList *sibling = g_list_find (screen_info->windows_stack, (gconstpointer) client_sibling);
                gint position = g_list_position (screen_info->windows_stack, sibling) + 1;

                screen_info->windows_stack = g_list_insert (screen_info->windows_stack, c, position);
                TRACE ("lowest client is \"%s\" (0x%lx) at position %i",
                        client_sibling->name, client_sibling->window, position);
            }
            else
            {
                screen_info->windows_stack = g_list_prepend (screen_info->windows_stack, c);
            }
        }
        /* Now, screen_info->windows_stack contains the correct window stack
           We still need to tell the X Server to reflect the changes
         */
        clientApplyStackList (screen_info);
        clientSetNetClientList (screen_info, display_info->atoms[NET_CLIENT_LIST_STACKING], screen_info->windows_stack);
        clientPassGrabMouseButton (NULL);
        clientPassFocus (screen_info, c, NULL);
        if (screen_info->last_raise == c)
        {
            screen_info->last_raise = NULL;
        }
    }
}

gboolean
clientAdjustFullscreenLayer (Client *c, gboolean set)
{
    g_return_val_if_fail (c, FALSE);

    if (set)
    {
        if (FLAG_TEST(c->xfwm_flags, XFWM_FLAG_LEGACY_FULLSCREEN)
            || FLAG_TEST(c->flags, CLIENT_FLAG_FULLSCREEN))
        {
            clientSetLayer (c, WIN_LAYER_ABOVE_DOCK);
            return TRUE;
        }
    }
    else if (c->win_layer == WIN_LAYER_ABOVE_DOCK)
    {
        if (FLAG_TEST(c->xfwm_flags, XFWM_FLAG_LEGACY_FULLSCREEN)
            || FLAG_TEST(c->flags, CLIENT_FLAG_FULLSCREEN))
        {
            if (FLAG_TEST(c->flags, CLIENT_FLAG_FULLSCREEN))
            {
                clientSetLayer (c, c->fullscreen_old_layer);
            }
            else
            {
                clientSetLayer (c, WIN_LAYER_NORMAL);
            }
            return TRUE;
        }
    }
    return FALSE;
}

void
clientAddToList (Client * c)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientAddToList");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    myDisplayAddClient (display_info, c);

    screen_info->client_count++;
    if (screen_info->clients)
    {
        c->prev = screen_info->clients->prev;
        c->next = screen_info->clients;
        screen_info->clients->prev->next = c;
        screen_info->clients->prev = c;
    }
    else
    {
        screen_info->clients = c;
        c->next = c;
        c->prev = c;
    }

    TRACE ("adding window \"%s\" (0x%lx) to windows list", c->name, c->window);
    screen_info->windows = g_list_append (screen_info->windows, c);
    screen_info->windows_stack = g_list_append (screen_info->windows_stack, c);

    clientSetNetClientList (screen_info, display_info->atoms[NET_CLIENT_LIST], screen_info->windows);
    clientSetNetClientList (screen_info, display_info->atoms[WIN_CLIENT_LIST], screen_info->windows);
    clientSetNetClientList (screen_info, display_info->atoms[NET_CLIENT_LIST_STACKING], screen_info->windows_stack);

    FLAG_SET (c->xfwm_flags, XFWM_FLAG_MANAGED);
}

void
clientRemoveFromList (Client * c)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientRemoveFromList");

    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_MANAGED);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    myDisplayRemoveClient (display_info, c);

    g_assert (screen_info->client_count > 0);
    screen_info->client_count--;
    if (screen_info->client_count == 0)
    {
        screen_info->clients = NULL;
    }
    else
    {
        c->next->prev = c->prev;
        c->prev->next = c->next;
        if (c == screen_info->clients)
        {
            screen_info->clients = screen_info->clients->next;
        }
    }

    TRACE ("removing window \"%s\" (0x%lx) from windows list", c->name, c->window);
    screen_info->windows = g_list_remove (screen_info->windows, c);

    TRACE ("removing window \"%s\" (0x%lx) from screen_info->windows_stack list", c->name, c->window);
    screen_info->windows_stack = g_list_remove (screen_info->windows_stack, c);

    clientSetNetClientList (screen_info, display_info->atoms[NET_CLIENT_LIST], screen_info->windows);
    clientSetNetClientList (screen_info, display_info->atoms[WIN_CLIENT_LIST], screen_info->windows);
    clientSetNetClientList (screen_info, display_info->atoms[NET_CLIENT_LIST_STACKING], screen_info->windows_stack);

    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_MANAGED);
}

GList *
clientGetStackList (ScreenInfo *screen_info)
{
    GList *windows_stack_copy = NULL;

    g_return_val_if_fail (screen_info, NULL);

    if (screen_info->windows_stack)
    {
        windows_stack_copy = g_list_copy (screen_info->windows_stack);
    }
    return windows_stack_copy;
}

void
clientSetLastRaise (Client *c)
{
    ScreenInfo *screen_info = NULL;

    g_return_if_fail (c != NULL);

    screen_info = c->screen_info;
    screen_info->last_raise = c;
}

Client *
clientGetLastRaise (ScreenInfo *screen_info)
{
    g_return_val_if_fail (screen_info, NULL);
    return (screen_info->last_raise);
}

void
clientClearLastRaise (ScreenInfo *screen_info)
{
    g_return_if_fail (screen_info);
    screen_info->last_raise = NULL;
}
