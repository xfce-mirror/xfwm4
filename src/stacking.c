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
 
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <glib.h>
#include <libxfce4util/libxfce4util.h> 

#include "screen.h"
#include "mywindow.h"
#include "client.h"
#include "stacking.h"
#include "netwm.h"
#include "transients.h"
#include "frame.h"
#include "focus.h"

GList *windows_stack  = NULL;
static Client *last_raise    = NULL;
static GList *windows        = NULL;

void
clientApplyStackList (ScreenData *md)
{
    Window *xwinstack;
    guint nwindows;
    gint i = 0;

    DBG ("applying stack list");
    nwindows = g_list_length (windows_stack);

    xwinstack = g_new (Window, nwindows + 2);
    xwinstack[i++] = MYWINDOW_XWINDOW (md->sidewalk[0]);
    xwinstack[i++] = MYWINDOW_XWINDOW (md->sidewalk[1]);

    if (nwindows)
    {
        GList *index;
        Client *c;
        
        for (index = g_list_last(windows_stack); index; index = g_list_previous (index))
        {
            c = (Client *) index->data;
            xwinstack[i++] = c->frame;
            DBG ("  [%i] \"%s\" (0x%lx)", i, c->name, c->window);
        }
    }

    XRestackWindows (md->dpy, xwinstack, (int) nwindows + 2);
    
    g_free (xwinstack);
}

gboolean
clientTransientOrModalHasAncestor (Client * c, int ws)
{
    Client *c2;
    GList *index;

    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientTransientOrModalHasAncestor");

    if (!clientIsTransientOrModal (c))
    {
        return FALSE;
    }

    for (index = windows_stack; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if ((c2 != c) && !clientIsTransientOrModal (c2)
            && clientIsTransientOrModalFor (c, c2)
            && !FLAG_TEST (c2->flags, CLIENT_FLAG_HIDDEN)
            && (c2->win_workspace == ws))
        {
            return TRUE;
        }
    }
    return FALSE;

}

Client *
clientGetLowestTransient (Client * c)
{
    Client *lowest_transient = NULL, *c2;
    GList *index;

    g_return_val_if_fail (c != NULL, NULL);

    TRACE ("entering clientGetLowestTransient");

    for (index = windows_stack; index; index = g_list_next (index))
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
    Client *c2;
    GList *index;

    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering clientGetHighestTransientOrModalFor");

    for (index = windows_stack; index; index = g_list_next (index))
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
    Client *c2;
    GList *index;

    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering clientGetTopMostForGroup");

    for (index = windows_stack; index; index = g_list_next (index))
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

Client *
clientGetNextTopMost (int layer, Client * exclude)
{
    Client *top = NULL, *c;
    GList *index;

    TRACE ("entering clientGetNextTopMost");

    for (index = windows_stack; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        TRACE ("*** stack window \"%s\" (0x%lx), layer %i", c->name,
            c->window, (int) c->win_layer);
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
clientGetBottomMost (int layer, Client * exclude)
{
    Client *bot = NULL, *c;
    GList *index;

    TRACE ("entering clientGetBottomMost");

    for (index = windows_stack; index; index = g_list_next (index))
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
clientAtPosition (int x, int y, Client * exclude)
{
    /* This function does the same as XQueryPointer but w/out the race
       conditions caused by querying the X server
     */
    GList *index;
    Client *c = NULL;
    Client *c2 = NULL;

    TRACE ("entering clientAtPos");

    for (index = g_list_last (windows_stack); index; index = g_list_previous (index))
    {
        c2 = (Client *) index->data;
        if (clientSelectMask (c2, 0) && (c2 != exclude))
        {
            if ((frameX (c2) < x) && (frameX (c2) + frameWidth (c2) > x)
                && (frameY (c2) < y) && (frameY (c2) + frameHeight (c2) > y))
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
    g_return_if_fail (c != NULL);
    TRACE ("entering clientRaise");

    if (c == last_raise)
    {
        TRACE ("client \"%s\" (0x%lx) already raised", c->name, c->window);
        return;
    }
    TRACE ("raising client \"%s\" (0x%lx)", c->name, c->window);

    if (g_list_length (windows_stack) < 1)
    {
        return;
    }

    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
    {
        Client *c2, *c3;
        Client *client_sibling = NULL;
        GList *transients = NULL;
        GList *sibling = NULL;
        GList *index1, *index2;
        GList *windows_stack_copy;

        /* Copy the existing window stack temporarily as reference */
        windows_stack_copy = g_list_copy (windows_stack);
        /* Search for the window that will be just on top of the raised window (layers...) */
        client_sibling = clientGetNextTopMost (c->win_layer, c);
        windows_stack = g_list_remove (windows_stack, (gconstpointer) c);
        if (client_sibling)
        {
            /* If there is one, look for its place in the list */
            sibling = g_list_find (windows_stack, (gconstpointer) client_sibling);
            /* Place the raised window just before it */
            windows_stack = g_list_insert_before (windows_stack, sibling, c);
        }
        else
        {
            /* There will be no window on top of the raised window, so place it at the end of list */
            windows_stack = g_list_append (windows_stack, c);
        }
        /* Now, look for transients, transients of transients, etc. */
        for (index1 = windows_stack_copy; index1; index1 = g_list_next (index1))
        {
            c2 = (Client *) index1->data;
            if (c2)
            {
                if ((c2 != c) && clientIsTransientOrModalFor (c2, c))
                {
                    transients = g_list_append (transients, c2);
                    if (sibling)
                    {
                        /* Make sure client_sibling is not c2 otherwise we create a circular linked list */
                        if (client_sibling != c2)
                        {
                            /* Place the transient window just before sibling */
                            windows_stack = g_list_remove (windows_stack, (gconstpointer) c2);
                            windows_stack = g_list_insert_before (windows_stack, sibling, c2);
                        }
                    }
                    else
                    {
                        /* There will be no window on top of the transient window, so place it at the end of list */
                        windows_stack = g_list_remove (windows_stack, (gconstpointer) c2);
                        windows_stack = g_list_append (windows_stack, c2);
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
                                    windows_stack = g_list_remove (windows_stack, (gconstpointer) c2);
                                    windows_stack = g_list_insert_before (windows_stack, sibling, c2);
                                }
                            }
                            else
                            {
                                /* There will be no window on top of the transient window, so place it at the end of list */
                                windows_stack = g_list_remove (windows_stack, (gconstpointer) c2);
                                windows_stack = g_list_append (windows_stack, c2);
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
        /* Now, windows_stack contains the correct window stack
           We still need to tell the X Server to reflect the changes 
         */
        clientApplyStackList (c->md);
        clientSetNetClientList (c->md, net_client_list_stacking, windows_stack);
        last_raise = c;
    }
}

void
clientLower (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientLower");
    TRACE ("lowering client \"%s\" (0x%lx)", c->name, c->window);

    if (g_list_length (windows_stack) < 1)
    {
        return;
    }

    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
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
        if (!client_sibling)
        {
            client_sibling = clientGetBottomMost (c->win_layer, c);
        }
        if (client_sibling != c)
        {
            windows_stack = g_list_remove (windows_stack, (gconstpointer) c);
            /* Paranoid check to avoid circular linked list */
            if (client_sibling)
            {
                GList *sibling = g_list_find (windows_stack, (gconstpointer) client_sibling);
                gint position = g_list_position (windows_stack, sibling) + 1;

                windows_stack = g_list_insert (windows_stack, c, position);
                TRACE ("lowest client is \"%s\" (0x%lx) at position %i", 
                        client_sibling->name, client_sibling->window, position);
            }
            else
            {
                windows_stack = g_list_prepend (windows_stack, c);
            }
        }
        /* Now, windows_stack contains the correct window stack
           We still need to tell the X Server to reflect the changes 
         */
        clientApplyStackList (c->md);
        clientSetNetClientList (c->md, net_client_list_stacking, windows_stack);
        if (last_raise == c)
        {
            last_raise = NULL;
        }
    }
}

void
clientAddToList (Client * c)
{
    Client *client_sibling = NULL;
    GList *sibling = NULL;
    
    g_return_if_fail (c != NULL);
    TRACE ("entering clientAddToList");

    client_count++;
    if (clients)
    {
        c->prev = clients->prev;
        c->next = clients;
        clients->prev->next = c;
        clients->prev = c;
    }
    else
    {
        clients = c;
        c->next = c;
        c->prev = c;
    }

    TRACE ("adding window \"%s\" (0x%lx) to windows list", c->name,
        c->window);
    windows = g_list_append (windows, c);

    client_sibling = clientGetLowestTransient (c);
    /* Paranoid check to avoid circular linked list */
    if (client_sibling != c)
    {
        if (client_sibling)
        {
            /* The client has already a transient mapped */
            sibling = g_list_find (windows_stack, (gconstpointer) client_sibling);
            windows_stack = g_list_insert_before (windows_stack, sibling, c);
        }
        else
        {
            client_sibling = clientGetNextTopMost (c->win_layer, c);
            /* Paranoid check to avoid circular linked list */
            if (client_sibling != c)
            {
                if (client_sibling)
                {
                    sibling = g_list_find (windows_stack, (gconstpointer) client_sibling);
                    windows_stack = g_list_insert_before (windows_stack, sibling, c);
                }
                else
                {
                    windows_stack = g_list_append (windows_stack, c);
                }
            }
        }
    }
    
    clientSetNetClientList (c->md, net_client_list, windows);
    clientSetNetClientList (c->md, win_client_list, windows);
    clientSetNetClientList (c->md, net_client_list_stacking, windows_stack);
  
    FLAG_SET (c->flags, CLIENT_FLAG_MANAGED);
}

void
clientRemoveFromList (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientRemoveFromList");
    g_assert (client_count > 0);

    FLAG_UNSET (c->flags, CLIENT_FLAG_MANAGED);
    client_count--;
    if (client_count == 0)
    {
        clients = NULL;
    }
    else
    {
        c->next->prev = c->prev;
        c->prev->next = c->next;
        if (c == clients)
        {
            clients = clients->next;
        }
    }

    TRACE ("removing window \"%s\" (0x%lx) from windows list", c->name,
        c->window);
    windows = g_list_remove (windows, c);

    TRACE ("removing window \"%s\" (0x%lx) from windows_stack list", c->name,
        c->window);
    windows_stack = g_list_remove (windows_stack, c);

    clientSetNetClientList (c->md, net_client_list, windows);
    clientSetNetClientList (c->md, win_client_list, windows);
    clientSetNetClientList (c->md, net_client_list_stacking, windows_stack);

    FLAG_UNSET (c->flags, CLIENT_FLAG_MANAGED);
}

GList *
clientGetStackList (void)
{
    GList *windows_stack_copy = NULL;
    if (windows_stack)
    {
        windows_stack_copy = g_list_copy (windows_stack);
    }
    return windows_stack_copy;
}

void
clientSetLastRaise (Client *c)
{
    last_raise = c;
}

Client *
clientGetLastRaise (void)
{
    return (last_raise);
}

void
clientClearLastRaise (void)
{
    last_raise = NULL;
}
