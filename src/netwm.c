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

        xfwm4    - (c) 2002-2006 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "client.h"
#include "compositor.h"
#include "display.h"
#include "frame.h"
#include "focus.h"
#include "hints.h"
#include "misc.h"
#include "netwm.h"
#include "screen.h"
#include "stacking.h"
#include "transients.h"
#include "workspaces.h"

void
clientSetNetState (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Atom data[16];
    int i;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetNetState");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    i = 0;
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        TRACE ("clientSetNetState : shaded");
        data[i++] = display_info->atoms[NET_WM_STATE_SHADED];
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        TRACE ("clientSetNetState : sticky");
        data[i++] = display_info->atoms[NET_WM_STATE_STICKY];
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL))
    {
        TRACE ("clientSetNetState : modal");
        data[i++] = display_info->atoms[NET_WM_STATE_MODAL];
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_PAGER))
    {
        TRACE ("clientSetNetState : skip_pager");
        data[i++] = display_info->atoms[NET_WM_STATE_SKIP_PAGER];
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_TASKBAR))
    {
        TRACE ("clientSetNetState : skip_taskbar");
        data[i++] = display_info->atoms[NET_WM_STATE_SKIP_TASKBAR];
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
    {
        TRACE ("clientSetNetState : maximize vert + horiz");
        data[i++] = display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ];
        data[i++] = display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT];
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ))
    {
        TRACE ("clientSetNetState : maximize horiz");
        data[i++] = display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ];
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT))
    {
        TRACE ("clientSetNetState : vert");
        data[i++] = display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT];
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE ("clientSetNetState : fullscreen");
        data[i++] = display_info->atoms[NET_WM_STATE_FULLSCREEN];
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
    {
        TRACE ("clientSetNetState : above");
        data[i++] = display_info->atoms[NET_WM_STATE_ABOVE];
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
    {
        TRACE ("clientSetNetState : below");
        data[i++] = display_info->atoms[NET_WM_STATE_BELOW];
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
    {
        TRACE ("clientSetNetState : hidden");
        data[i++] = display_info->atoms[NET_WM_STATE_HIDDEN];
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION))
    {
        TRACE ("clientSetNetState : demands_attention");
        data[i++] = display_info->atoms[NET_WM_STATE_DEMANDS_ATTENTION];
    }
    XChangeProperty (display_info->dpy, c->window,
                     display_info->atoms[NET_WM_STATE], XA_ATOM, 32,
                     PropModeReplace, (unsigned char *) data, i);
    /*
       We also set GNOME hint here for consistency and convenience,
       although the meaning of net_wm_state and win_state aren't the same.
     */
    setHint (display_info, c->window, WIN_STATE, c->win_state);
}

void
clientGetNetState (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    int n_atoms;
    Atom *atoms;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGetNetState");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    atoms = NULL;
    n_atoms = 0;

    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_SESSION_MANAGED))
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            TRACE ("clientGetNetState : shaded from session management");
            c->win_state |= WIN_STATE_SHADED;
            FLAG_SET (c->flags, CLIENT_FLAG_SHADED);
        }
        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
        {
            TRACE ("clientGetNetState : sticky from session management");
            c->win_state |= WIN_STATE_STICKY;
            FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
        }
        if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ))
        {
            TRACE ("clientGetNetState : maximized horiz from session management");
            c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
        }
        if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT))
        {
            TRACE ("clientGetNetState : maximized vert from session management");
            c->win_state |= WIN_STATE_MAXIMIZED_VERT;
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
        }
    }

    if (getAtomList (display_info, c->window, NET_WM_STATE, &atoms, &n_atoms))
    {
        int i;
        TRACE ("clientGetNetState: %i atoms detected", n_atoms);

        i = 0;
        while (i < n_atoms)
        {
            if ((atoms[i] == display_info->atoms[NET_WM_STATE_SHADED]))
            {
                TRACE ("clientGetNetState : shaded");
                c->win_state |= WIN_STATE_SHADED;
                FLAG_SET (c->flags, CLIENT_FLAG_SHADED);
            }
            else if ((atoms[i] == display_info->atoms[NET_WM_STATE_STICKY]))
            {
                TRACE ("clientGetNetState : sticky");
                c->win_state |= WIN_STATE_STICKY;
                FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
            }
            else if ((atoms[i] == display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ]))
            {
                TRACE ("clientGetNetState : maximized horiz");
                c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
                FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
            }
            else if ((atoms[i] == display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT]))
            {
                TRACE ("clientGetNetState : maximized vert");
                c->win_state |= WIN_STATE_MAXIMIZED_VERT;
                FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
            }
            else if ((atoms[i] == display_info->atoms[NET_WM_STATE_FULLSCREEN]))
            {
                if (!FLAG_TEST_ALL (c->flags, CLIENT_FLAG_ABOVE | CLIENT_FLAG_BELOW))
                {
                    TRACE ("clientGetNetState : fullscreen");
                    FLAG_SET (c->flags, CLIENT_FLAG_FULLSCREEN);
                }
            }
            else if ((atoms[i] == display_info->atoms[NET_WM_STATE_ABOVE]))
            {
                if (!FLAG_TEST_ALL (c->flags, CLIENT_FLAG_FULLSCREEN | CLIENT_FLAG_BELOW))
                {
                    TRACE ("clientGetNetState : above");
                    FLAG_SET (c->flags, CLIENT_FLAG_ABOVE);
                }
            }
            else if ((atoms[i] == display_info->atoms[NET_WM_STATE_BELOW]))
            {
                if (!FLAG_TEST_ALL (c->flags, CLIENT_FLAG_ABOVE | CLIENT_FLAG_FULLSCREEN))
                {
                    TRACE ("clientGetNetState : below");
                    FLAG_SET (c->flags, CLIENT_FLAG_BELOW);
                }
            }
            else if (atoms[i] == display_info->atoms[NET_WM_STATE_MODAL])
            {
                TRACE ("clientGetNetState : modal");
                FLAG_SET (c->flags, CLIENT_FLAG_STATE_MODAL);
            }
            else if (atoms[i] == display_info->atoms[NET_WM_STATE_SKIP_PAGER])
            {
                TRACE ("clientGetNetState : skip_pager");
                FLAG_SET (c->flags, CLIENT_FLAG_SKIP_PAGER);
            }
            else if (atoms[i] == display_info->atoms[NET_WM_STATE_SKIP_TASKBAR])
            {
                TRACE ("clientGetNetState : skip_taskbar");
                FLAG_SET (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            }
            else if (atoms[i] == display_info->atoms[NET_WM_STATE_HIDDEN])
            {
                TRACE ("clientGetNetState : state_hidden");
                FLAG_SET (c->flags, CLIENT_FLAG_ICONIFIED);
            }
            else if (atoms[i] == display_info->atoms[NET_WM_STATE_DEMANDS_ATTENTION])
            {
                TRACE ("clientGetNetState : demands_attention");
                FLAG_SET (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION);
            }
            else
            {
                g_warning ("Unmanaged net_wm_state (window 0x%lx)", c->window);
            }

            ++i;
        }
        if (atoms)
        {
            XFree (atoms);
        }
    }
}

void
clientUpdateNetState (Client * c, XClientMessageEvent * ev)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    unsigned long action, mode;
    Atom first;
    Atom second;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateNetState");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    action = ev->data.l[0];
    first  = ev->data.l[1];
    second = ev->data.l[2];
    mode = 0;

    if ((first  == display_info->atoms[NET_WM_STATE_SHADED]) ||
        (second == display_info->atoms[NET_WM_STATE_SHADED]))
    {
        if ((action == NET_WM_STATE_ADD) && !FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            clientShade (c);
        }
        else if ((action == NET_WM_STATE_REMOVE) && FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            clientUnshade (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            clientToggleShaded (c);
        }
    }

    if ((first  == display_info->atoms[NET_WM_STATE_STICKY]) ||
        (second == display_info->atoms[NET_WM_STATE_STICKY]))
    {
        if (!clientIsValidTransientOrModal (c) && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_STICK))
        {
            if ((action == NET_WM_STATE_ADD) && !FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
            {
                clientStick (c, TRUE);
            }
            else if ((action == NET_WM_STATE_REMOVE) && FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
            {
                clientUnstick (c, TRUE);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                clientToggleSticky (c, TRUE);
            }
            frameDraw (c, FALSE);
        }
    }

    if ((first  == display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ]) ||
        (second == display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ]) ||
        (first  == display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT]) ||
        (second == display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT]))
    {
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MAXIMIZE))
        {
            if ((action == NET_WM_STATE_ADD) && !FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
            {
                mode = 0;
                if ((first  == display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ]) ||
                    (second == display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ]))
                {
                    mode |= !FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ) ? WIN_STATE_MAXIMIZED_HORIZ : 0;
                }
                if ((first  == display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT]) ||
                    (second == display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT]))
                {
                    mode |= !FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT) ? WIN_STATE_MAXIMIZED_VERT : 0;
                }
                clientToggleMaximized (c, mode, TRUE);
            }
            else if ((action == NET_WM_STATE_REMOVE) && FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
            {
                mode = 0;
                if ((first  == display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ]) ||
                    (second == display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ]))
                {
                    mode |= FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ) ? WIN_STATE_MAXIMIZED_HORIZ : 0;
                }
                if ((first  == display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT]) ||
                    (second == display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT]))
                {
                    mode |= FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT) ? WIN_STATE_MAXIMIZED_VERT : 0;
                }
                clientToggleMaximized (c, mode, TRUE);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                mode = 0;
                if ((first  == display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ]) ||
                    (second == display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ]))
                {
                    mode |= WIN_STATE_MAXIMIZED_HORIZ;
                }
                if ((first  == display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT]) ||
                    (second == display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT]))
                {
                    mode |= WIN_STATE_MAXIMIZED_VERT;
                }
                clientToggleMaximized (c, mode, TRUE);
            }
        }
    }

    if ((first  == display_info->atoms[NET_WM_STATE_MODAL]) ||
        (second == display_info->atoms[NET_WM_STATE_MODAL]))
    {
        if ((action == NET_WM_STATE_ADD) && !FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState (c);
            clientWindowType (c);
        }
        else if ((action == NET_WM_STATE_REMOVE) && FLAG_TEST (c->flags, CLIENT_FLAG_STATE_MODAL))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState (c);
            clientWindowType (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            FLAG_TOGGLE (c->flags, CLIENT_FLAG_STATE_MODAL);
            clientSetNetState (c);
            clientWindowType (c);
        }
        frameDraw (c, TRUE);
    }

    if ((first  == display_info->atoms[NET_WM_STATE_FULLSCREEN]) ||
        (second == display_info->atoms[NET_WM_STATE_FULLSCREEN]))
    {
        if (!clientIsValidTransientOrModal (c))
        {
            if ((action == NET_WM_STATE_ADD) && !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
            {
                FLAG_SET (c->flags, CLIENT_FLAG_FULLSCREEN);
                clientUpdateFullscreenState (c);
            }
            else if ((action == NET_WM_STATE_REMOVE) && FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
            {
                FLAG_UNSET (c->flags, CLIENT_FLAG_FULLSCREEN);
                clientUpdateFullscreenState (c);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_FULLSCREEN);
                clientUpdateFullscreenState (c);
            }
        }
    }

    if ((first  == display_info->atoms[NET_WM_STATE_ABOVE]) ||
        (second == display_info->atoms[NET_WM_STATE_ABOVE]))
    {
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
        {
            if ((action == NET_WM_STATE_ADD) && !FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
            {
                FLAG_SET (c->flags, CLIENT_FLAG_ABOVE);
                clientUpdateAboveState (c);
            }
            else if ((action == NET_WM_STATE_REMOVE) && FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
            {
                FLAG_UNSET (c->flags, CLIENT_FLAG_ABOVE);
                clientUpdateAboveState (c);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_ABOVE);
                clientUpdateAboveState (c);
            }
        }
    }

    if ((first  == display_info->atoms[NET_WM_STATE_BELOW]) ||
        (second == display_info->atoms[NET_WM_STATE_BELOW]))
    {
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
        {
            if ((action == NET_WM_STATE_ADD) && !FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
            {
                compositorDamageWindow (display_info, c->frame);
                FLAG_SET (c->flags, CLIENT_FLAG_BELOW);
                clientUpdateBelowState (c);
            }
            else if ((action == NET_WM_STATE_REMOVE) && FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
            {
                compositorDamageWindow (display_info, c->frame);
                FLAG_UNSET (c->flags, CLIENT_FLAG_BELOW);
                clientUpdateBelowState (c);
            }
            else if (action == NET_WM_STATE_TOGGLE)
            {
                compositorDamageWindow (display_info, c->frame);
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_BELOW);
                clientUpdateBelowState (c);
            }
        }
    }

    if ((first  == display_info->atoms[NET_WM_STATE_SKIP_PAGER]) ||
        (second == display_info->atoms[NET_WM_STATE_SKIP_PAGER]))
    {
        if ((action == NET_WM_STATE_ADD) && !FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_PAGER))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState (c);
        }
        else if ((action == NET_WM_STATE_REMOVE) && FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_PAGER))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            FLAG_TOGGLE (c->flags, CLIENT_FLAG_SKIP_PAGER);
            clientSetNetState (c);
        }
    }

    if ((first  == display_info->atoms[NET_WM_STATE_SKIP_TASKBAR]) ||
        (second == display_info->atoms[NET_WM_STATE_SKIP_TASKBAR]))
    {
        if ((action == NET_WM_STATE_ADD) && !FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_TASKBAR))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState (c);
        }
        else if ((action == NET_WM_STATE_REMOVE) && FLAG_TEST (c->flags, CLIENT_FLAG_SKIP_TASKBAR))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            FLAG_TOGGLE (c->flags, CLIENT_FLAG_SKIP_TASKBAR);
            clientSetNetState (c);
        }
        frameDraw (c, TRUE);
    }

    if ((first  == display_info->atoms[NET_WM_STATE_DEMANDS_ATTENTION]) ||
        (second == display_info->atoms[NET_WM_STATE_DEMANDS_ATTENTION]))
    {
        if ((action == NET_WM_STATE_ADD) && !FLAG_TEST (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION))
        {
            /* Do not apply NET_WM_STATE_DEMANDS_ATTENTION if client is already focused */
            if (c != clientGetFocusOrPending ())
            {
                FLAG_SET (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION);
                clientSetNetState (c);
            }
        }
        else if ((action == NET_WM_STATE_REMOVE) && FLAG_TEST (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION))
        {
            FLAG_UNSET (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION);
            clientSetNetState (c);
        }
        else if (action == NET_WM_STATE_TOGGLE)
        {
            /* Do not apply NET_WM_STATE_DEMANDS_ATTENTION if client is already focused */
            if (c != clientGetFocusOrPending () || !FLAG_TEST (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION))
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION);
                clientSetNetState (c);
            }
        }
    }
}

void
clientNetMoveResize (Client * c, XClientMessageEvent * ev)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    unsigned int button_mask;
    int x_root, y_root, dx, dy, action, button;
    int corner;
    gboolean resize; /* true == resize, false == move */
    XEvent *event;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientNetMoveResize");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    x_root = (int) ev->data.l[0];
    y_root = (int) ev->data.l[1];
    action = (int) ev->data.l[2];
    button = (int) ev->data.l[3];
    event  = (XEvent *) ev;

    if (button == 0)
    {
        button_mask = getMouseXY (screen_info, c->window, &dx, &dy);
        if (button_mask & Button1Mask)
        {
            button = Button1;
        }
        else if (button_mask & Button2Mask)
        {
            button = Button2;
        }
        else if (button_mask & Button3Mask)
        {
            button = Button3;
        }
        else
        {
            g_warning ("Could not determine the mouse button used");
            return;
        }
    }

    corner = CORNER_BOTTOM_RIGHT;
    resize = TRUE;

    event->xbutton.button = button;
    event->xbutton.x_root = event->xkey.x_root = x_root;
    event->xbutton.y_root = event->xkey.y_root = y_root;
    event->xbutton.time = event->xkey.time = myDisplayGetCurrentTime (display_info);

    switch (action)
    {
        /* Keyboard */
        case NET_WM_MOVERESIZE_SIZE_KEYBOARD:
            event->type = KeyPress;
            corner = CORNER_BOTTOM_RIGHT;
            resize = TRUE; /* Resize */
            break;
        case NET_WM_MOVERESIZE_MOVE_KEYBOARD:
            event->type = KeyPress;
            resize = FALSE; /* Move */
            break;

        /* Sides */
        case NET_WM_MOVERESIZE_SIZE_TOP:
            event->type = ButtonPress;
            corner = CORNER_COUNT + SIDE_TOP;
            resize = TRUE; /* Resize */
            break;
        case NET_WM_MOVERESIZE_SIZE_BOTTOM:
            event->type = ButtonPress;
            corner = CORNER_COUNT + SIDE_BOTTOM;
            resize = TRUE; /* Resize */
            break;
        case NET_WM_MOVERESIZE_SIZE_RIGHT:
            event->type = ButtonPress;
            corner = CORNER_COUNT + SIDE_RIGHT;
            resize = TRUE; /* Resize */
            break;
        case NET_WM_MOVERESIZE_SIZE_LEFT:
            event->type = ButtonPress;
            corner = CORNER_COUNT + SIDE_LEFT;
            resize = TRUE; /* Resize */
            break;

        /* Corners */
        case NET_WM_MOVERESIZE_SIZE_TOPLEFT:
            event->type = ButtonPress;
            corner = CORNER_TOP_LEFT;
            resize = TRUE; /* Resize */
            break;
        case NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
            event->type = ButtonPress;
            corner = CORNER_TOP_RIGHT;
            resize = TRUE; /* Resize */
            break;
        case NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
            event->type = ButtonPress;
            corner = CORNER_BOTTOM_LEFT;
            resize = TRUE; /* Resize */
            break;
        case NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
            event->type = ButtonPress;
            corner = CORNER_BOTTOM_RIGHT;
            resize = TRUE; /* Resize */
            break;
        case NET_WM_MOVERESIZE_MOVE:
        default:
            event->type = ButtonPress;
            resize = FALSE; /* Move */
            break;
    }
    
    if (!FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        if (resize && FLAG_TEST_ALL (c->xfwm_flags, XFWM_FLAG_HAS_RESIZE | XFWM_FLAG_IS_RESIZABLE))
        {
            clientResize (c, corner, event);
        }
        else if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MOVE))
        {
            clientMove (c, event);
        }
    }
}

void
clientUpdateFullscreenState (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    XWindowChanges wc;
    int layer;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateFullscreenState");
    TRACE ("Update fullscreen state for client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        GdkRectangle rect;
        gint monitor_nbr;
        int cx, cy;

        cx = frameX (c) + (frameWidth (c) / 2);
        cy = frameY (c) + (frameHeight (c) / 2);

        monitor_nbr = find_monitor_at_point (c->screen_info->gscr, cx, cy);
        gdk_screen_get_monitor_geometry (c->screen_info->gscr, monitor_nbr, &rect);

        c->fullscreen_old_x = c->x;
        c->fullscreen_old_y = c->y;
        c->fullscreen_old_width = c->width;
        c->fullscreen_old_height = c->height;
        c->fullscreen_old_layer = c->win_layer;

        wc.x = rect.x;
        wc.y = rect.y;
        wc.width = rect.width;
        wc.height = rect.height;
        layer = WIN_LAYER_ABOVE_DOCK;
    }
    else
    {
        wc.x = c->fullscreen_old_x;
        wc.y = c->fullscreen_old_y;
        wc.width = c->fullscreen_old_width;
        wc.height = c->fullscreen_old_height;
        layer = c->fullscreen_old_layer;
    }
    clientSetNetState (c);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MANAGED))
    {
        /*
           For some reason, the configure can generate EnterNotify events
           on lower windows, causing a nasty race cond with apps trying to
           grab focus in focus follow mouse mode. Grab the pointer to
           avoid these effects
         */
        myScreenGrabPointer (c->screen_info, EnterWindowMask, None, myDisplayGetCurrentTime (display_info));
        clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_FORCE_REDRAW);
        myScreenUngrabPointer (c->screen_info);
    }
    else
    {
        c->x = wc.x;
        c->y = wc.y;
        c->height = wc.height;
        c->width = wc.width;
    }
    clientSetLayer (c, layer);
}

void
clientGetNetWmType (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    int n_atoms;
    Atom *atoms;
    int i;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGetNetWmType");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    n_atoms = 0;
    atoms = NULL;

    if (!getAtomList (display_info, c->window, NET_WM_WINDOW_TYPE, &atoms, &n_atoms))
    {
        switch (c->win_layer)
        {
            case WIN_LAYER_DESKTOP:
                c->type_atom = display_info->atoms[NET_WM_WINDOW_TYPE_DESKTOP];
                break;
            case WIN_LAYER_DOCK:
                c->type_atom = display_info->atoms[NET_WM_WINDOW_TYPE_DOCK];
                break;
            case WIN_LAYER_NORMAL:
                c->type_atom = display_info->atoms[NET_WM_WINDOW_TYPE_NORMAL];
                break;
            default:
                c->type_atom = None;
                break;
        }
    }
    else
    {
        i = 0;
        while (i < n_atoms)
        {
            if ((atoms[i] == display_info->atoms[NET_WM_WINDOW_TYPE_DESKTOP]) ||
                (atoms[i] == display_info->atoms[NET_WM_WINDOW_TYPE_DOCK])    ||
                (atoms[i] == display_info->atoms[NET_WM_WINDOW_TYPE_TOOLBAR]) ||
                (atoms[i] == display_info->atoms[NET_WM_WINDOW_TYPE_MENU])    ||
                (atoms[i] == display_info->atoms[NET_WM_WINDOW_TYPE_DIALOG])  ||
                (atoms[i] == display_info->atoms[NET_WM_WINDOW_TYPE_NORMAL])  ||
                (atoms[i] == display_info->atoms[NET_WM_WINDOW_TYPE_UTILITY]) ||
                (atoms[i] == display_info->atoms[NET_WM_WINDOW_TYPE_SPLASH]))
            {
                c->type_atom = atoms[i];
                break;
            }
            ++i;
        }
        if (atoms)
        {
            XFree (atoms);
        }
    }
    clientWindowType (c);
}

void
clientGetInitialNetWmDesktop (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Client *c2;
    long val;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGetInitialNetWmDesktop");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    val = 0;

    /* This is to make sure that transient are shown with their "ancestor" window */
    c2 = clientGetTransient (c);
    if (c2)
    {
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_WORKSPACE_SET);
        c->win_workspace = c2->win_workspace;
        if (FLAG_TEST (c2->flags, CLIENT_FLAG_STICKY))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
            c->win_state |= WIN_STATE_STICKY;
        }
    }
    else
    {
        if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_SESSION_MANAGED)
            &&  !FLAG_TEST (c->xfwm_flags, XFWM_FLAG_WORKSPACE_SET))
        {
            FLAG_SET (c->xfwm_flags, XFWM_FLAG_WORKSPACE_SET);
            c->win_workspace = c->screen_info->current_ws;
        }
        if (getHint (display_info, c->window, NET_WM_DESKTOP, &val))
        {
            TRACE ("atom net_wm_desktop detected");
            if (val == (int) ALL_WORKSPACES)
            {
                if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_STICK) && !FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
                {
                    TRACE ("atom net_wm_desktop specifies window \"%s\" is sticky", c->name);
                    FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
                    c->win_state |= WIN_STATE_STICKY;
                }
                c->win_workspace = c->screen_info->current_ws;
            }
            else
            {
                TRACE ("atom net_wm_desktop specifies window \"%s\" is on desk %i", c->name, (int) val);
                c->win_workspace = (int) val;
            }
            FLAG_SET (c->xfwm_flags, XFWM_FLAG_WORKSPACE_SET);
        }
        else if (getHint (display_info, c->window, WIN_WORKSPACE, &val))
        {
            TRACE ("atom win_workspace specifies window \"%s\" is on desk %i", c->name, (int) val);
            c->win_workspace = (int) val;
            FLAG_SET (c->xfwm_flags, XFWM_FLAG_WORKSPACE_SET);
        }
    }
    TRACE ("initial desktop for window \"%s\" is %i", c->name, c->win_workspace);
    if (c->win_workspace > c->screen_info->workspace_count - 1)
    {
        TRACE ("value off limits, using %i instead", c->screen_info->workspace_count - 1);
        c->win_workspace = c->screen_info->workspace_count - 1;
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_WORKSPACE_SET);
    }
    TRACE ("initial desktop for window \"%s\" is %i", c->name, c->win_workspace);
    setHint (display_info, c->window, WIN_WORKSPACE, c->win_workspace);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        setHint (display_info, c->window, NET_WM_DESKTOP, (unsigned long) ALL_WORKSPACES);
    }
    else
    {
        setHint (display_info, c->window, NET_WM_DESKTOP, (unsigned long) c->win_workspace);
    }
}

void
clientSetNetClientList (ScreenInfo * screen_info, Atom a, GList * list)
{
    Window *listw;
    Window *index_dest;
    GList *index_src;
    gint size, i;

    TRACE ("entering clientSetNetClientList");

    size = g_list_length (list);
    if (size < 1)
    {
        XDeleteProperty (myScreenGetXDisplay (screen_info), screen_info->xroot, a);
        return;
    }

    listw = g_new (Window, size + 1);
    if (listw)
    {
        TRACE ("%i windows in list for %i clients", size, screen_info->client_count);
        for (i = 0, index_dest = listw, index_src = list; i < size;
            i++, index_dest++, index_src = g_list_next (index_src))
        {
            Client *c = (Client *) index_src->data;
            *index_dest = c->window;
        }
        XChangeProperty (myScreenGetXDisplay (screen_info), screen_info->xroot, a, XA_WINDOW, 32, PropModeReplace,
            (unsigned char *) listw, size);
        g_free (listw);
    }
}

gboolean
clientValidateNetStrut (Client * c)
{
    ScreenInfo *screen_info;
    gboolean valid;
    int max_value;
    int i;

    g_return_val_if_fail (c != NULL, TRUE);
    TRACE ("entering clientValidateNetStrut for \"%s\" (0x%lx)", c->name, c->window);
    screen_info = c->screen_info;
    max_value = MIN (screen_info->width, screen_info->height) / 4;
    valid = TRUE;

    for (i = 0; i < 4; i++)
    {
        if (c->struts[i] > max_value)
        {
           g_warning ("Strut value for application window 0x%lx changed from %d to %d", c->window, c->struts[i], max_value);
           c->struts[i] = max_value;
           valid = FALSE;
        }
    }

    return valid;
}

gboolean
clientGetNetStruts (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    unsigned long old_flags, new_flags;
    int old_struts[STRUTS_SIZE];
    gulong *struts;
    int nitems;
    int i;

    g_return_val_if_fail (c != NULL, FALSE);
    TRACE ("entering clientGetNetStruts for \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    struts = NULL;

    /* Save old values */
    old_flags = c->flags & (CLIENT_FLAG_HAS_STRUT | CLIENT_FLAG_HAS_STRUT_PARTIAL);
    for (i = 0; i < STRUTS_SIZE; i++)
    {
        old_struts[i] = c->struts[i];
        c->struts[i] = 0;
    }
    FLAG_UNSET (c->flags, CLIENT_FLAG_HAS_STRUT);
    FLAG_UNSET (c->flags, CLIENT_FLAG_HAS_STRUT_PARTIAL);

    if (getCardinalList (display_info, c->window, NET_WM_STRUT_PARTIAL, &struts, &nitems))
    {
        if (nitems != STRUTS_SIZE)
        {
            if (struts)
            {
                XFree (struts);
            }
            /* Restore old values */
            if (old_flags)
            {
                FLAG_SET (c->flags, old_flags);
                for (i = 0; i < STRUTS_SIZE; i++)
                {
                    c->struts[i] = old_struts[i];
                }
            }
            return FALSE;
        }

        FLAG_SET (c->flags, CLIENT_FLAG_HAS_STRUT);
        FLAG_SET (c->flags, CLIENT_FLAG_HAS_STRUT_PARTIAL);
        for (i = 0; i < STRUTS_SIZE; i++)
        {
            c->struts[i] = (int) struts[i];
        }

        XFree (struts);
    }
    else if (getCardinalList (display_info, c->window, NET_WM_STRUT, &struts, &nitems))
    {
        if (nitems != 4)
        {
            if (struts)
            {
                XFree (struts);
            }
            /* Restore old values */
            if (old_flags)
            {
                FLAG_SET (c->flags, old_flags);
                for (i = 0; i < STRUTS_SIZE; i++)
                {
                    c->struts[i] = old_struts[i];
                }
            }
            return FALSE;
        }

        FLAG_SET (c->flags, CLIENT_FLAG_HAS_STRUT);
        for (i = 0; i < 4; i++)
        {
            c->struts[i] = (int) struts[i];
        }
        for (i = 4; i < STRUTS_SIZE; i++)
        {
            c->struts[i] = 0;
        }
        /* Fill(in values as for partial struts */
        c->struts[STRUTS_TOP_START_X] = c->struts[STRUTS_BOTTOM_START_X] = 0;
        c->struts[STRUTS_TOP_END_X] = c->struts[STRUTS_BOTTOM_END_X] =
            c->screen_info->width;
        c->struts[STRUTS_LEFT_START_Y] = c->struts[STRUTS_RIGHT_START_Y] = 0;
        c->struts[STRUTS_LEFT_END_Y] = c->struts[STRUTS_RIGHT_END_Y] =
            c->screen_info->height;

        XFree (struts);
    }

    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUT))
    {
        clientValidateNetStrut (c);
    }

    /* check for a change in struts flags */
    new_flags = c->flags & (CLIENT_FLAG_HAS_STRUT | CLIENT_FLAG_HAS_STRUT_PARTIAL);
    if (old_flags != new_flags)
    {
        return TRUE;
    }

    /* Flags haven't changed, check values */
    if (new_flags)
    {
        for (i = 0; i < STRUTS_SIZE; i++)
        {
            if (old_struts[i] != c->struts[i])
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

void
clientSetNetActions (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Atom atoms[6];
    int i;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetNetActions");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    i = 0;

    atoms[i++] = display_info->atoms[NET_WM_ACTION_CLOSE];
    if (CLIENT_CAN_MAXIMIZE_WINDOW (c))
    {
        atoms[i++] = display_info->atoms[NET_WM_ACTION_MAXIMIZE_HORZ];
        atoms[i++] = display_info->atoms[NET_WM_ACTION_MAXIMIZE_VERT];
    }
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_STICK))
    {
        atoms[i++] = display_info->atoms[NET_WM_ACTION_CHANGE_DESKTOP];
        atoms[i++] = display_info->atoms[NET_WM_ACTION_STICK];
    }
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER))
    {
        atoms[i++] = display_info->atoms[NET_WM_ACTION_SHADE];
    }
    XChangeProperty (clientGetXDisplay (c), c->window, display_info->atoms[NET_WM_ALLOWED_ACTIONS],
                     XA_ATOM, 32, PropModeReplace, (unsigned char *) atoms, i);
}

void
clientWindowType (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    netWindowType old_type;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientWindowType");
    TRACE ("type for client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    old_type = c->type;
    c->initial_layer = c->win_layer;
    if (c->type_atom != None)
    {
        if (c->type_atom == display_info->atoms[NET_WM_WINDOW_TYPE_DESKTOP])
        {
            TRACE ("atom net_wm_window_type_desktop detected");
            c->type = WINDOW_DESKTOP;
            c->initial_layer = WIN_LAYER_DESKTOP;
            c->win_state |= WIN_STATE_STICKY;
            FLAG_SET (c->flags,
                CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_STICKY |
                CLIENT_FLAG_SKIP_TASKBAR);
            FLAG_UNSET (c->xfwm_flags,
                XFWM_FLAG_HAS_RESIZE | XFWM_FLAG_HAS_MOVE |
                XFWM_FLAG_HAS_HIDE | XFWM_FLAG_HAS_MAXIMIZE |
                XFWM_FLAG_HAS_MENU | XFWM_FLAG_HAS_STICK |
                XFWM_FLAG_HAS_BORDER);
        }
        else if (c->type_atom == display_info->atoms[NET_WM_WINDOW_TYPE_DOCK])
        {
            TRACE ("atom net_wm_window_type_dock detected");
            c->type = WINDOW_DOCK;
            c->initial_layer = WIN_LAYER_DOCK;
            FLAG_SET (c->flags,
                CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_SKIP_TASKBAR);
            FLAG_UNSET (c->xfwm_flags,
                XFWM_FLAG_HAS_BORDER |  XFWM_FLAG_HAS_MOVE |
                XFWM_FLAG_HAS_HIDE | XFWM_FLAG_HAS_MAXIMIZE |
                XFWM_FLAG_HAS_MENU | XFWM_FLAG_HAS_STICK);
        }
        else if (c->type_atom == display_info->atoms[NET_WM_WINDOW_TYPE_TOOLBAR])
        {
            TRACE ("atom net_wm_window_type_toolbar detected");
            c->type = WINDOW_TOOLBAR;
            c->initial_layer = WIN_LAYER_NORMAL;
            FLAG_SET (c->flags,
                CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_SKIP_TASKBAR);
            FLAG_UNSET (c->xfwm_flags,
                XFWM_FLAG_HAS_HIDE | XFWM_FLAG_HAS_MAXIMIZE |
                XFWM_FLAG_HAS_STICK);
        }
        else if (c->type_atom == display_info->atoms[NET_WM_WINDOW_TYPE_MENU])
        {
            TRACE ("atom net_wm_window_type_menu detected");
            c->type = WINDOW_MENU;
            c->initial_layer = WIN_LAYER_NORMAL;
            /* The policy here is unclear :
               http://mail.gnome.org/archives/wm-spec-list/2002-May/msg00001.html
               As it seems, GNOME and KDE don't treat menu the same way...
             */
            FLAG_SET (c->flags,
                CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_SKIP_TASKBAR);
            FLAG_UNSET (c->xfwm_flags,
                XFWM_FLAG_HAS_HIDE | XFWM_FLAG_HAS_MAXIMIZE |
                XFWM_FLAG_HAS_STICK);
        }
        else if (c->type_atom == display_info->atoms[NET_WM_WINDOW_TYPE_DIALOG])
        {
            TRACE ("atom net_wm_window_type_dialog detected");
            c->type = WINDOW_DIALOG;
            c->initial_layer = WIN_LAYER_NORMAL;
        }
        else if (c->type_atom == display_info->atoms[NET_WM_WINDOW_TYPE_NORMAL])
        {
            TRACE ("atom net_wm_window_type_normal detected");
            c->type = WINDOW_NORMAL;
            c->initial_layer = WIN_LAYER_NORMAL;
        }
        else if (c->type_atom == display_info->atoms[NET_WM_WINDOW_TYPE_UTILITY])
        {
            TRACE ("atom net_wm_window_type_utility detected");
            FLAG_SET (c->flags,
                CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_SKIP_TASKBAR);
            c->type = WINDOW_UTILITY;
            c->initial_layer = WIN_LAYER_NORMAL;
        }
        else if (c->type_atom == display_info->atoms[NET_WM_WINDOW_TYPE_SPLASH])
        {
            TRACE ("atom net_wm_window_type_splash detected");
            c->type = WINDOW_SPLASHSCREEN;
            c->initial_layer = WIN_LAYER_ABOVE_DOCK;
            FLAG_SET (c->flags,
                CLIENT_FLAG_SKIP_PAGER | CLIENT_FLAG_SKIP_TASKBAR);
            FLAG_UNSET (c->xfwm_flags,
                XFWM_FLAG_HAS_BORDER | XFWM_FLAG_HAS_HIDE |
                XFWM_FLAG_HAS_MENU | XFWM_FLAG_HAS_MOVE |
                XFWM_FLAG_HAS_RESIZE | XFWM_FLAG_HAS_STICK);
        }
    }
    else
    {
        TRACE ("no \"net\" atom detected");
        c->type = UNSET;
        c->initial_layer = c->win_layer;
    }

    if (clientIsValidTransientOrModal (c))
    {
        Client *c2;

        TRACE ("Window \"%s\" is a transient or a modal", c->name);

        c2 = clientGetHighestTransientOrModalFor (c);
        if ((c2) && (c->initial_layer < c2->win_layer))
        {
            c->initial_layer = c2->win_layer;
            TRACE ("Applied layer is %i", c->initial_layer);
        }
        FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_HAS_HIDE | XFWM_FLAG_HAS_STICK);
        FLAG_UNSET (c->flags, CLIENT_FLAG_STICKY);
    }
    if ((old_type != c->type) || (c->initial_layer != c->win_layer))
    {
        TRACE ("setting layer %i", c->initial_layer);
        clientSetNetState (c);
        clientSetLayer (c, c->initial_layer);
    }
}

void
clientUpdateAboveState (Client * c)
{
    int layer;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateAboveState");
    TRACE ("Update above state for client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
    {
        layer = WIN_LAYER_ABOVE_DOCK;
    }
    else
    {
        layer = c->initial_layer;
    }
    clientSetNetState (c);
    clientSetLayer (c, layer);
}

void
clientUpdateBelowState (Client * c)
{
    int layer;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateBelowState");
    TRACE ("Update below state for client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
    {
        layer = WIN_LAYER_BELOW;
    }
    else
    {
        layer = c->initial_layer;
    }
    clientSetNetState (c);
    clientSetLayer (c, layer);
}

