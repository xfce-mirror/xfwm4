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
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h> 
#include <libxfcegui4/libxfcegui4.h>

#include "client.h"
#include "compositor.h"
#include "focus.h"
#include "frame.h"
#include "hints.h"
#include "icons.h"
#include "misc.h"
#include "mypixmap.h"
#include "mywindow.h"
#include "netwm.h"
#include "placement.h"
#include "poswin.h"
#include "screen.h"
#include "session.h"
#include "settings.h"
#include "stacking.h"
#include "startup_notification.h"
#include "tabwin.h"
#include "transients.h"
#include "wireframe.h"
#include "workspaces.h"

/* Event mask definition */

#define POINTER_EVENT_MASK \
    ButtonPressMask|\
    ButtonReleaseMask

#define FRAME_EVENT_MASK \
    SubstructureNotifyMask|\
    SubstructureRedirectMask|\
    EnterWindowMask|\
    PropertyChangeMask

#define CLIENT_EVENT_MASK \
    StructureNotifyMask|\
    FocusChangeMask|\
    PropertyChangeMask

/* Useful macros */
#define ACCEPT_INPUT(wmhints) \
    (!(screen_info->params->focus_hint) || \
     ((!(wmhints) || \
       ((wmhints) && !(wmhints->flags & InputHint)) || \
       ((wmhints) && (wmhints->flags & InputHint) && (wmhints->input)))))

#define START_ICONIC(c) \
    ((c->wmhints) && \
     (c->wmhints->initial_state == IconicState) && \
     !clientIsValidTransientOrModal (c))

/* You don't like that ? Me either, but, hell, it's the way glib lists are designed */
#define XWINDOW_TO_GPOINTER(w)  ((gpointer) (Window) (w))
#define GPOINTER_TO_XWINDOW(p)  ((Window) (p))

#ifndef EPSILON
#define EPSILON                 (1e-6)  
#endif

#ifndef MAX_RESIZES_PER_SECOND
#define MAX_RESIZES_PER_SECOND  20.0
#endif

typedef struct _MoveResizeData MoveResizeData;
struct _MoveResizeData
{
    gboolean use_keys;
    gboolean grab;
    int mx, my;
    int ox, oy;
    int oldw, oldh;
    int corner;
    Poswin *poswin;
    xfwmWindow tmp_event_window;
    Client *c;
};

typedef struct _ClientCycleData ClientCycleData;
struct _ClientCycleData
{
    Client *c;
    Tabwin *tabwin;
    Window wireframe;
    int cycle_range;
};

typedef struct _ButtonPressData ButtonPressData;
struct _ButtonPressData
{
    int b;
    Client *c;
};

Display *
clientGetXDisplay (Client * c)
{
    g_return_val_if_fail (c, NULL);

    return myScreenGetXDisplay (c->screen_info);
}            

/* 
 * The following two functions are to limit the number of updates 
 * during resize operations.
 * It's taken from Metacity
 */
void
clientClearLastOpTime (Client * c)
{
    g_return_if_fail (c != NULL);
    
    TRACE ("entering clientClearLastOpTime");
    c->last_op_time.tv_sec = 0;
    c->last_op_time.tv_usec = 0;
}

static gboolean
clientCheckLastOpTime (Client * c)
{
    GTimeVal current_time;
    double elapsed;
  
    g_return_val_if_fail (c != NULL, FALSE);

    g_get_current_time (&current_time);
    /* use milliseconds, 1000 milliseconds/second */
    elapsed = (((double)current_time.tv_sec - c->last_op_time.tv_sec) * G_USEC_PER_SEC +
                  (current_time.tv_usec - c->last_op_time.tv_usec)) / 1000.0;
    if (elapsed >= 0.0 && elapsed < (1000.0 / MAX_RESIZES_PER_SECOND))
    {
        return FALSE;
    }
    else if (elapsed < (0.0 - EPSILON))
    {
        /* clock screw */
        clientClearLastOpTime (c);
    }
    c->last_op_time = current_time;
  
    return TRUE;
}

void
clientInstallColormaps (Client * c)
{
    XWindowAttributes attr;
    gboolean installed = FALSE;
    int i;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientInstallColormaps");

    if (c->ncmap)
    {
        for (i = c->ncmap - 1; i >= 0; i--)
        {
            XGetWindowAttributes (clientGetXDisplay (c), c->cmap_windows[i], &attr);
            XInstallColormap (clientGetXDisplay (c), attr.colormap);
            if (c->cmap_windows[i] == c->window)
            {
                installed = TRUE;
            }
        }
    }
    if ((!installed) && (c->cmap))
    {
        XInstallColormap (clientGetXDisplay (c), c->cmap);
    }
}

void
clientUpdateColormaps (Client * c)
{
    XWindowAttributes attr;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateColormaps");

    if (c->ncmap)
    {
        XFree (c->cmap_windows);
        c->ncmap = 0;
    }
    if (!XGetWMColormapWindows (clientGetXDisplay (c), c->window, &c->cmap_windows, &c->ncmap))
    {
        c->cmap_windows = NULL;
        c->ncmap = 0;
    }
    c->cmap = attr.colormap;
}

void
clientUpdateAllFrames (ScreenInfo *screen_info, int mask)
{
    Client *c = NULL;
    int i;
    XWindowChanges wc;

    g_return_if_fail (screen_info != NULL);

    TRACE ("entering clientRedrawAllFrames");
    XGrabPointer (myScreenGetXDisplay (screen_info), screen_info->gnome_win, FALSE, EnterWindowMask, GrabModeAsync,
                       GrabModeAsync, screen_info->xroot, None, GDK_CURRENT_TIME);
    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, i++)
    {
        if (mask & UPDATE_KEYGRABS)
        {
            clientUngrabKeys (c);
            clientGrabKeys (c);
        }
        if (mask & UPDATE_CACHE)
        {
            clientClearPixmapCache (c);
        }
        if (mask & UPDATE_GRAVITY)
        {
            clientGravitate (c, REMOVE);
            clientGravitate (c, APPLY);
            wc.x = c->x;
            wc.y = c->y;
            wc.width = c->width;
            wc.height = c->height;
            clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_FORCE_REDRAW);
        }
        if (mask & UPDATE_FRAME)
        {
            frameDraw (c, FALSE, FALSE);
        }
    }
    XUngrabPointer (myScreenGetXDisplay (screen_info), GDK_CURRENT_TIME);
}

void
clientGrabKeys (Client * c)
{
    ScreenInfo *screen_info = NULL;
    
    g_return_if_fail (c != NULL);
    TRACE ("entering clientGrabKeys");
    TRACE ("grabbing keys for client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;

    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_ADD_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_CLOSE_WINDOW], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_CYCLE_WINDOWS], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_DEL_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_HIDE_WINDOW], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_LOWER_WINDOW], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MAXIMIZE_HORIZ], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MAXIMIZE_VERT], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MAXIMIZE_WINDOW], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_DOWN], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_LEFT], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_NEXT_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_PREV_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_RIGHT], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_UP], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_WORKSPACE_1], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_WORKSPACE_2], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_WORKSPACE_3], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_WORKSPACE_4], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_WORKSPACE_5], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_WORKSPACE_6], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_WORKSPACE_7], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_WORKSPACE_8], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_WORKSPACE_9], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_NEXT_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_PREV_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_RAISE_WINDOW], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_RESIZE_DOWN], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_RESIZE_LEFT], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_RESIZE_RIGHT], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_RESIZE_UP], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_SHADE_WINDOW], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_STICK_WINDOW], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_TOGGLE_FULLSCREEN], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_SHORTCUT_1], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_SHORTCUT_2], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_SHORTCUT_3], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_SHORTCUT_4], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_SHORTCUT_5], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_SHORTCUT_6], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_SHORTCUT_7], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_SHORTCUT_8], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_SHORTCUT_9], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_SHORTCUT_10], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_WORKSPACE_1], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_WORKSPACE_2], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_WORKSPACE_3], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_WORKSPACE_4], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_WORKSPACE_5], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_WORKSPACE_6], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_WORKSPACE_7], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_WORKSPACE_8], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_WORKSPACE_9], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_UP_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_DOWN_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_LEFT_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_RIGHT_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_UP_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_DOWN_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_LEFT_WORKSPACE], c->window);
    grabKey (clientGetXDisplay (c), &screen_info->params->keys[KEY_MOVE_RIGHT_WORKSPACE], c->window);
}

void
clientUngrabKeys (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientUngrabKeys");
    TRACE ("ungrabing keys for client \"%s\" (0x%lx)", c->name, c->window);

    ungrabKeys (clientGetXDisplay (c), c->window);
}

void
clientGrabButtons (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientGrabButtons");
    TRACE ("grabbing buttons for client \"%s\" (0x%lx)", c->name, c->window);
    
    grabButton(clientGetXDisplay (c), Button1, AltMask, c->window);
    grabButton(clientGetXDisplay (c), Button2, AltMask, c->window);
    grabButton(clientGetXDisplay (c), Button3, AltMask, c->window);
}

void
clientUngrabButtons (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientUngrabButtons");
    TRACE ("grabbing buttons for client \"%s\" (0x%lx)", c->name, c->window);
    
    XUngrabButton (clientGetXDisplay (c), AnyButton, AnyModifier, c->window);
}

static gboolean
urgent_cb (gpointer data)
{
    Client *c = (Client *) data;
    g_return_val_if_fail (c, FALSE);

    TRACE ("entering urgent_cb");

    if (c != clientGetFocus ())
    {
        FLAG_TOGGLE (c->xfwm_flags, XFWM_FLAG_SEEN_ACTIVE);
        frameDraw (c, FALSE, FALSE);
    }
    return (TRUE);
}

void
clientUpdateUrgency (Client *c)
{
    g_return_if_fail (c != NULL);
    
    TRACE ("entering clientUpdateUrgency");

    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_SEEN_ACTIVE);
    if (c->blink_timeout_id)
    {
        g_source_remove (c->blink_timeout_id);
        frameDraw (c, FALSE, FALSE);
    }
    FLAG_UNSET (c->wm_flags, WM_FLAG_URGENT);

    c->blink_timeout_id = 0;
    if ((c->wmhints) && (c->wmhints->flags & XUrgencyHint))
    {
        FLAG_SET (c->wm_flags, WM_FLAG_URGENT);
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
        {
            c->blink_timeout_id =  
                g_timeout_add_full (0, 500, (GtkFunction) urgent_cb,
                                            (gpointer) c, NULL);
        }
    }
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_SEEN_ACTIVE)
        && !FLAG_TEST (c->wm_flags, WM_FLAG_URGENT)
        && (c != clientGetFocus ()))
    {
        FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_SEEN_ACTIVE);
        frameDraw (c, FALSE, FALSE);
    }
}

void
clientCoordGravitate (Client * c, int mode, int *x, int *y)
{
    int dx = 0, dy = 0;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientCoordGravitate");

    c->gravity =
        c->size->flags & PWinGravity ? c->size->win_gravity : NorthWestGravity;
    switch (c->gravity)
    {
        case CenterGravity:
            dx = (c->border_width * 2) - ((frameLeft (c) +
                    frameRight (c)) / 2);
            dy = (c->border_width * 2) - ((frameTop (c) +
                    frameBottom (c)) / 2);
            break;
        case NorthGravity:
            dx = (c->border_width * 2) - ((frameLeft (c) +
                    frameRight (c)) / 2);
            dy = frameTop (c);
            break;
        case SouthGravity:
            dx = (c->border_width * 2) - ((frameLeft (c) +
                    frameRight (c)) / 2);
            dy = (c->border_width * 2) - frameBottom (c);
            break;
        case EastGravity:
            dx = (c->border_width * 2) - frameRight (c);
            dy = (c->border_width * 2) - ((frameTop (c) +
                    frameBottom (c)) / 2);
            break;
        case WestGravity:
            dx = frameLeft (c);
            dy = (c->border_width * 2) - ((frameTop (c) +
                    frameBottom (c)) / 2);
            break;
        case NorthWestGravity:
            dx = frameLeft (c);
            dy = frameTop (c);
            break;
        case NorthEastGravity:
            dx = (c->border_width * 2) - frameRight (c);
            dy = frameTop (c);
            break;
        case SouthWestGravity:
            dx = frameLeft (c);
            dy = (c->border_width * 2) - frameBottom (c);
            break;
        case SouthEastGravity:
            dx = (c->border_width * 2) - frameRight (c);
            dy = (c->border_width * 2) - frameBottom (c);
            break;
        default:
            dx = 0;
            dy = 0;
            break;
    }
    *x = *x + (dx * mode);
    *y = *y + (dy * mode);
}

void
clientGravitate (Client * c, int mode)
{
    int x, y;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGravitate");

    x = c->x;
    y = c->y;
    clientCoordGravitate (c, mode, &x, &y);
    c->x = x;
    c->y = y;
}

static void
clientSetWidth (Client * c, int w1)
{
    int w2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetWidth");
    TRACE ("setting width %i for client \"%s\" (0x%lx)", w1, c->name,
        c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        /* Bypass resize increment and max sizes for fullscreen */
        c->width = w1;
        return;
    }
    
    if ((c->size->flags & PResizeInc) && (c->size->width_inc))
    {
        w2 = (w1 - c->size->min_width) / c->size->width_inc;
        w1 = c->size->min_width + (w2 * c->size->width_inc);
    }
    if (c->size->flags & PMaxSize)
    {
        if (w1 > c->size->max_width)
        {
            w1 = c->size->max_width;
        }
    }
    if (c->size->flags & PMinSize)
    {
        if (w1 < c->size->min_width)
        {
            w1 = c->size->min_width;
        }
    }
    if (w1 < 1)
    {
        w1 = 1;
    }
    c->width = w1;
}

static void
clientSetHeight (Client * c, int h1)
{
    int h2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetHeight");
    TRACE ("setting height %i for client \"%s\" (0x%lx)", h1, c->name,
        c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        /* Bypass resize increment and max sizes for fullscreen */
        c->height = h1;
        return;
    }
    
    if ((c->size->flags & PResizeInc) && (c->size->height_inc))
    {
        h2 = (h1 - c->size->min_height) / c->size->height_inc;
        h1 = c->size->min_height + (h2 * c->size->height_inc);
    }
    if (c->size->flags & PMaxSize)
    {
        if (h1 > c->size->max_height)
        {
            h1 = c->size->max_height;
        }
    }
    if (c->size->flags & PMinSize)
    {
        if (h1 < c->size->min_height)
        {
            h1 = c->size->min_height;
        }
    }
    if (h1 < 1)
    {
        h1 = 1;
    }
    c->height = h1;
}
/* clientConstrainRatio - adjust the given width and height to account for
   the constraints imposed by size hints

   The aspect ratio stuff, is borrowed from uwm's CheckConsistency routine.
 */

#define MAKE_MULT(a,b) ((b==1) ? (a) : (((int)((a)/(b))) * (b)) )
static void
clientConstrainRatio (Client * c, int w1, int h1, int corner)
{

    g_return_if_fail (c != NULL);
    TRACE ("entering clientConstrainRatio");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    
    if (c->size->flags & PAspect)
    {
        int xinc, yinc, minx, miny, maxx, maxy, delta;

        xinc = c->size->width_inc;
        yinc = c->size->height_inc;
        minx = c->size->min_aspect.x;
        miny = c->size->min_aspect.y;
        maxx = c->size->max_aspect.x;
        maxy = c->size->max_aspect.y;

        if ((minx * h1 > miny * w1) && 
            (miny) && (corner == 4 + SIDE_BOTTOM))
        {
            /* Change width to match */
            delta = MAKE_MULT (minx * h1 /  miny - w1, xinc);
            if (!(c->size->flags & PMaxSize) || 
                (w1 + delta <= c->size->max_width))
            {
                w1 += delta;
            }
        }
        if ((minx * h1 > miny * w1) &&
            (minx))
        {
            delta = MAKE_MULT (h1 - w1 * miny / minx, yinc);
            if (!(c->size->flags & PMinSize) ||
                (h1 - delta >= c->size->min_height))
            {
                h1 -= delta;
            }
            else
            {
                delta = MAKE_MULT (minx * h1 / miny - w1, xinc);
                if (!(c->size->flags & PMaxSize) ||
                    (w1 + delta <= c->size->max_width))
                  w1 += delta;
            }
        }

        if ((maxx * h1 < maxy * w1) &&
            (maxx) &&
            ((corner == 4 + SIDE_LEFT) || (corner == 4 + SIDE_RIGHT)))
        {
            delta = MAKE_MULT (w1 * maxy / maxx - h1, yinc);
            if (!(c->size->flags & PMaxSize) ||
                (h1 + delta <= c->size->max_height))
            {
                h1 += delta;
            }
        }
        if ((maxx * h1 < maxy * w1) &&
             (maxy))
        {
            delta = MAKE_MULT (w1 - maxx * h1 / maxy, xinc);
            if (!(c->size->flags & PMinSize) ||
                (w1 - delta >= c->size->min_width))
            {
                w1 -= delta;
            }
            else
            {
                delta = MAKE_MULT (w1 * maxy / maxx - h1, yinc);
                if (!(c->size->flags & PMaxSize) ||
                    (h1 + delta <= c->size->max_height))
                {
                    h1 += delta;
                }
            }
        }
    }

    c->height = h1;
    c->width = w1;
}

void
clientConfigure (Client * c, XWindowChanges * wc, int mask, unsigned short flags)
{
    XConfigureEvent ce;
    gboolean moved = FALSE;
    gboolean resized = FALSE;
    
    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    TRACE ("entering clientConfigure");
    TRACE ("configuring client \"%s\" (0x%lx) %s, type %u", c->name,
        c->window, flags & CFG_CONSTRAINED ? "constrained" : "not contrained", c->type);

    if (mask & CWX)
    {
        moved = TRUE;
        if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING))
        {
            c->x = wc->x;
        }
    }
    if (mask & CWY)
    {
        moved = TRUE;
        if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING))
        {
            c->y = wc->y;
        }
    }
    if (mask & CWWidth)
    {
        resized = TRUE;
        clientSetWidth (c, wc->width);
    }
    if (mask & CWHeight)
    {
        resized = TRUE;
        clientSetHeight (c, wc->height);
    }
    if (mask & CWBorderWidth)
    {
        c->border_width = wc->border_width;
    }
    if (mask & CWStackMode)
    {
        switch (wc->stack_mode)
        {
            /* 
             * Limitation: we don't support neither sibling,
             * TopIf, BottomIf nor Opposite ... 
             */
            case Above:
                TRACE ("Above");
                clientRaise (c);
                break;
            case Below:
                /* Don't do below if the app specified a sibling
                 * since, as we don't honor sibling,  the window
                 * would be sent to bottom which is not what the 
                 * app meant at first....
                 */
                if (!(mask & CWSibling))
                {
                    TRACE ("Below");
                    clientLower (c);
                }
                break;
            case Opposite:
            case TopIf:
            case BottomIf:
            default:
                break;
        }
    }
    mask &= ~(CWStackMode | CWSibling);

    /* 
       Some broken apps try to achieve fullscreen by using static gravity and a
       (0,0) position, the second part of the test is for this case.
     */
    if ((flags & CFG_CONSTRAINED) && (mask & (CWX | CWY)) && (CONSTRAINED_WINDOW (c) 
         && !(c->gravity == StaticGravity && (c->x == 0) && (c->y == 0))))
    {
        clientConstrainPos (c, TRUE);
    }

    XMoveResizeWindow (clientGetXDisplay (c), c->frame, frameX (c), frameY (c), 
                            frameWidth (c), frameHeight (c));
    XMoveResizeWindow (clientGetXDisplay (c), c->window, frameLeft (c), frameTop (c), 
                            c->width, c->height);

    if (resized || (flags & CFG_FORCE_REDRAW))
    {
        frameDraw (c, FALSE, TRUE);
    }
    
    if ((flags & CFG_NOTIFY) ||
        ((flags & CFG_REQUEST) && !(moved || resized)) ||
        (moved && !resized))
    {
        DBG ("Sending ConfigureNotify");
        ce.type = ConfigureNotify;
        ce.display = clientGetXDisplay (c);
        ce.event = c->window;
        ce.window = c->window;
        ce.x = c->x;
        ce.y = c->y;
        ce.width = c->width;
        ce.height = c->height;
        ce.border_width = 0;
        ce.above = c->frame;
        ce.override_redirect = FALSE;
        XSendEvent (clientGetXDisplay (c), c->window, FALSE, StructureNotifyMask,
            (XEvent *) & ce);
    }
}

void
clientGetMWMHints (Client * c, gboolean update)
{
    PropMwmHints *mwm_hints;
    XWindowChanges wc;
    
    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);
    
    TRACE ("entering clientGetMWMHints client \"%s\" (0x%lx)", c->name,
        c->window);
    
    mwm_hints = getMotifHints (clientGetXDisplay (c), c->window);
    if (mwm_hints)
    {
        if (mwm_hints->flags & MWM_HINTS_DECORATIONS)
        {
            if (mwm_hints->decorations & MWM_DECOR_ALL)
            {
                FLAG_SET (c->xfwm_flags, XFWM_FLAG_HAS_BORDER | XFWM_FLAG_HAS_MENU);
            }
            else
            {
                FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_HAS_BORDER | XFWM_FLAG_HAS_MENU);
                FLAG_SET (c->xfwm_flags,
                    (mwm_hints->
                        decorations & (MWM_DECOR_TITLE | MWM_DECOR_BORDER)) ?
                    XFWM_FLAG_HAS_BORDER : 0);
                FLAG_SET (c->xfwm_flags,
                    (mwm_hints->
                        decorations & (MWM_DECOR_MENU)) ? XFWM_FLAG_HAS_MENU
                    : 0);
                /*
                   FLAG_UNSET(c->xfwm_flags, XFWM_FLAG_HAS_HIDE);
                   FLAG_UNSET(c->xfwm_flags, XFWM_FLAG_HAS_MAXIMIZE);
                   FLAG_SET(c->xfwm_flags, (mwm_hints->decorations & (MWM_DECOR_MINIMIZE)) ? XFWM_FLAG_HAS_HIDE : 0);
                   FLAG_SET(c->xfwm_flags, (mwm_hints->decorations & (MWM_DECOR_MAXIMIZE)) ? XFWM_FLAG_HAS_MAXIMIZE : 0);
                 */
            }
        }
        /* The following is from Metacity : */
        if (mwm_hints->flags & MWM_HINTS_FUNCTIONS)
        {
            if (!(mwm_hints->functions & MWM_FUNC_ALL))
            {
                FLAG_UNSET (c->xfwm_flags,
                    XFWM_FLAG_HAS_CLOSE | XFWM_FLAG_HAS_HIDE |
                    XFWM_FLAG_HAS_MAXIMIZE | XFWM_FLAG_HAS_MOVE |
                    XFWM_FLAG_HAS_RESIZE);
            }
            else
            {
                FLAG_SET (c->xfwm_flags,
                    XFWM_FLAG_HAS_CLOSE | XFWM_FLAG_HAS_HIDE |
                    XFWM_FLAG_HAS_MAXIMIZE | XFWM_FLAG_HAS_MOVE |
                    XFWM_FLAG_HAS_RESIZE);
            }

            if (mwm_hints->functions & MWM_FUNC_CLOSE)
            {
                FLAG_TOGGLE (c->xfwm_flags, XFWM_FLAG_HAS_CLOSE);
            }
            if (mwm_hints->functions & MWM_FUNC_MINIMIZE)
            {
                FLAG_TOGGLE (c->xfwm_flags, XFWM_FLAG_HAS_HIDE);
            }
            if (mwm_hints->functions & MWM_FUNC_MAXIMIZE)
            {
                FLAG_TOGGLE (c->xfwm_flags, XFWM_FLAG_HAS_MAXIMIZE);
            }
            if (mwm_hints->functions & MWM_FUNC_RESIZE)
            {
                FLAG_TOGGLE (c->xfwm_flags, XFWM_FLAG_HAS_RESIZE);
            }
            if (mwm_hints->functions & MWM_FUNC_MOVE)
            {
                FLAG_TOGGLE (c->xfwm_flags, XFWM_FLAG_HAS_MOVE);
            }
        }
        g_free (mwm_hints);
    }
    
    if (update)
    {
        /* EWMH window type takes precedences over Motif hints */ 
        clientWindowType (c);
        if (FLAG_TEST_ALL(c->xfwm_flags, XFWM_FLAG_HAS_BORDER | XFWM_FLAG_LEGACY_FULLSCREEN)
            && !FLAG_TEST(c->flags, CLIENT_FLAG_FULLSCREEN))
        {
            /* legacy app changed its decoration, put it back on regular layer */
            FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_LEGACY_FULLSCREEN);
            clientSetLayer (c, WIN_LAYER_NORMAL);
        }
        wc.x = c->x;
        wc.y = c->y;
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_FORCE_REDRAW);
    }
}

void
clientGetWMNormalHints (Client * c, gboolean update)
{
    unsigned long previous_value;
    long dummy = 0;
    XWindowChanges wc;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);
    
    TRACE ("entering clientGetWMNormalHints client \"%s\" (0x%lx)", c->name,
        c->window);

    if (!c->size)
    {
        c->size = XAllocSizeHints ();
    }
    g_assert (c->size);
    if (!XGetWMNormalHints (clientGetXDisplay (c), c->window, c->size, &dummy))
    {
        c->size->flags = 0;
    }
    
    previous_value = FLAG_TEST (c->xfwm_flags, XFWM_FLAG_IS_RESIZABLE);
    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_IS_RESIZABLE);

    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;

    if (!(c->size->flags & PMaxSize))
    {
        c->size->max_width = G_MAXINT;
        c->size->max_height = G_MAXINT;
        c->size->flags |= PMaxSize;
    }
    
    if (!(c->size->flags & PBaseSize))
    {
        c->size->base_width = 0;
        c->size->base_height = 0;
    }

    if (!(c->size->flags & PMinSize))
    {
        if ((c->size->flags & PBaseSize))
        {
            c->size->min_width = c->size->base_width;
            c->size->min_height = c->size->base_height;
        }
        else
        {
            c->size->min_width = 1;
            c->size->min_height = 1;
        }
        c->size->flags |= PMinSize;
    }

    if (c->size->flags & PResizeInc)
    {
        if (c->size->width_inc < 1)
        {
            c->size->width_inc = 1;
        }
        if (c->size->height_inc < 1)
        {
            c->size->height_inc = 1;
        }
    }
    else
    {
        c->size->width_inc = 1;
        c->size->height_inc = 1;
    }

    if (c->size->flags & PAspect)
    {
        if (c->size->min_aspect.x < 1)
        {
            c->size->min_aspect.x = 1;
        }
        if (c->size->min_aspect.y < 1)
        {
            c->size->min_aspect.y = 1;
        }
        if (c->size->max_aspect.x < 1)
        {
            c->size->max_aspect.x = 1;
        }
        if (c->size->max_aspect.y < 1)
        {
            c->size->max_aspect.y = 1;
        }
    }
    else
    {
        c->size->min_aspect.x = 1;
        c->size->min_aspect.y = 1;
        c->size->max_aspect.x = G_MAXINT;
        c->size->max_aspect.y = G_MAXINT;
    }

    if (c->size->min_width < 1)
    {
        c->size->min_width = 1;
    }
    if (c->size->min_height < 1)
    {
        c->size->min_height = 1;
    }
    if (c->size->max_width < 1)
    {
        c->size->max_width = 1;
    }
    if (c->size->max_height < 1)
    {
        c->size->max_height = 1;
    }
    if (wc.width > c->size->max_width)
    {
        wc.width = c->size->max_width;
    }
    if (wc.height > c->size->max_height)
    {
        wc.height = c->size->max_height;
    }
    if (wc.width < c->size->min_width)
    {
        wc.width = c->size->min_width;
    }
    if (wc.height < c->size->min_height)
    {
        wc.height = c->size->min_height;
    }

    if ((c->size->min_width < c->size->max_width) || 
        (c->size->min_height < c->size->max_height))
    {
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_IS_RESIZABLE);
    }
    
    if (update)
    {
        if ((c->width != wc.width) || (c->height != wc.height))
        {
            clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_CONSTRAINED);
        }
        else if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_IS_RESIZABLE) != previous_value)
        {
            frameDraw (c, TRUE, FALSE);
        }
    }
    else
    {
        c->width = wc.width;
        c->height = wc.height;
    }
}

void
clientGetWMProtocols (Client * c)
{
    unsigned int wm_protocols_flags = 0;
    
    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);
    
    TRACE ("entering clientGetWMProtocols client \"%s\" (0x%lx)", c->name,
        c->window);
    wm_protocols_flags = getWMProtocols (clientGetXDisplay (c), c->window);
    FLAG_SET (c->wm_flags,
        (wm_protocols_flags & WM_PROTOCOLS_DELETE_WINDOW) ?
        WM_FLAG_DELETE : 0);
    FLAG_SET (c->wm_flags,
        (wm_protocols_flags & WM_PROTOCOLS_TAKE_FOCUS) ?
        WM_FLAG_TAKEFOCUS : 0);
    /* KDE extension */
    FLAG_SET (c->wm_flags, 
        (wm_protocols_flags & WM_PROTOCOLS_CONTEXT_HELP) ?
        WM_FLAG_CONTEXT_HELP : 0);
}

static void
clientFree (Client * c)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientFree");
    TRACE ("freeing client \"%s\" (0x%lx)", c->name, c->window);

    if (clientGetFocus () == c)
    {
        clientClearFocus ();
    }
    if (clientGetLastRaise (c->screen_info) == c)
    {
        clientClearLastRaise (c->screen_info);
    }
    if (clientGetLastGrab () == c)
    {
        clientClearLastGrab ();
    }
    if (c->blink_timeout_id)
    {
        g_source_remove (c->blink_timeout_id);
    }
    if (c->name)
    {
        free (c->name);
    }
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    if (c->startup_id)
    {
        free (c->startup_id);
    }
#endif /* HAVE_LIBSTARTUP_NOTIFICATION */
    if (c->size)
    {
        XFree (c->size);
    }
    if (c->wmhints)
    {
        XFree (c->wmhints);
    }
    if ((c->ncmap > 0) && (c->cmap_windows))
    {
        XFree (c->cmap_windows);
    }
    if (c->class.res_name)
    {
        XFree (c->class.res_name);
    }
    if (c->class.res_class)
    {
        XFree (c->class.res_class);
    }

    g_free (c);
}

static void
clientGetWinState (Client * c)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientGetWinState");

    if (c->win_state & WIN_STATE_STICKY)
    {
        if (!clientIsValidTransientOrModal (c))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
        }
    }
    if (c->win_state & WIN_STATE_SHADED)
    {
        FLAG_SET (c->flags, CLIENT_FLAG_SHADED);
    }
    if (c->win_state & WIN_STATE_MAXIMIZED_HORIZ)
    {
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MAXIMIZE))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
        }
    }
    if (c->win_state & WIN_STATE_MAXIMIZED_VERT)
    {
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MAXIMIZE))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
        }
    }
    if (c->win_state & WIN_STATE_MAXIMIZED)
    {
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MAXIMIZE))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED);
        }
    }
}

static void
clientApplyInitialState (Client * c)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientApplyInitialState");

    /* We check that afterwards to make sure all states are now known */
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ | CLIENT_FLAG_MAXIMIZED_VERT))
    {
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MAXIMIZE))
        {
            unsigned long mode = 0;

            TRACE ("Applying client's initial state: maximized");
            if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ))
            {
                TRACE ("initial state: maximized horiz.");
                mode |= WIN_STATE_MAXIMIZED_HORIZ;
            }
            if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT))
            {
                TRACE ("initial state: maximized vert.");
                mode |= WIN_STATE_MAXIMIZED_VERT;
            }
            /* Unset fullscreen mode so that clientToggleMaximized() really change the state */
            FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED);
            clientToggleMaximized (c, mode);
        }
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        if (!clientIsValidTransientOrModal (c))
        {
            TRACE ("Applying client's initial state: fullscreen");
            clientUpdateFullscreenState (c);
        }
    }
    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_ABOVE, CLIENT_FLAG_BELOW))
    {
        if (!clientIsValidTransientOrModal (c))
        {
            TRACE ("Applying client's initial state: above");
            clientUpdateAboveState (c);
        }
    }
    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_BELOW, CLIENT_FLAG_ABOVE))
    {
        if (!clientIsValidTransientOrModal (c))
        {
            TRACE ("Applying client's initial state: below");
            clientUpdateBelowState (c);
        }
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY) && 
        FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_STICK))
    {
        if (!clientIsValidTransientOrModal (c))
        {
            TRACE ("Applying client's initial state: sticky");
            clientStick (c, TRUE);
        }
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        TRACE ("Applying client's initial state: sticky");
        clientShade (c);
    }
}

void
clientUpdateWinState (Client * c, XClientMessageEvent * ev)
{
    unsigned long action;
    Atom add_remove;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateWinState");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    action = ((XEvent *) ev)->xclient.data.l[0];
    add_remove = ((XEvent *) ev)->xclient.data.l[1];

    if (action & WIN_STATE_SHADED)
    {
        TRACE ("client \"%s\" (0x%lx) has received a win_state/shade event",
            c->name, c->window);
        if (add_remove == WIN_STATE_SHADED)
        {
            clientShade (c);
        }
        else
        {
            clientUnshade (c);
        }
    }
    else if ((action & WIN_STATE_STICKY)
             && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_STICK))
    {
        TRACE ("client \"%s\" (0x%lx) has received a win_state/stick event",
            c->name, c->window);
        if (!clientIsValidTransientOrModal (c))
        {
            if (add_remove == WIN_STATE_STICKY)
            {
                clientStick (c, TRUE);
            }
            else
            {
                clientUnstick (c, TRUE);
            }
            frameDraw (c, FALSE, FALSE);
        }
    }
    else if ((action & WIN_STATE_MAXIMIZED)
             && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MAXIMIZE))
    {
        TRACE ("client \"%s\" (0x%lx) has received a win_state/maximize event",
            c->name, c->window);
        clientToggleMaximized (c, add_remove);
    }
}

static gboolean
clientCheckShape (Client * c)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;
    int xws, yws, xbs, ybs;
    unsigned wws, hws, wbs, hbs;
    int boundingShaped, clipShaped;
    
    g_return_val_if_fail (c != NULL, FALSE);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    
    if (display_info->shape)
    {
        XShapeQueryExtents (display_info->dpy, c->window, &boundingShaped, &xws, &yws, &wws, 
                            &hws, &clipShaped, &xbs, &ybs, &wbs, &hbs);
        return (boundingShaped != 0);
    }
    return FALSE;
}

void
clientClearPixmapCache (Client * c)
{
    g_return_if_fail (c != NULL);

    xfwmPixmapFree (&c->pm_cache.pm_title[ACTIVE]);
    xfwmPixmapFree (&c->pm_cache.pm_title[INACTIVE]);
    xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_LEFT][ACTIVE]);
    xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_LEFT][INACTIVE]);
    xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_RIGHT][ACTIVE]);
    xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_RIGHT][INACTIVE]);
    xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_BOTTOM][ACTIVE]);
    xfwmPixmapFree (&c->pm_cache.pm_sides[SIDE_BOTTOM][INACTIVE]);
}

void
clientGetUserTime (Client * c)
{
    g_return_if_fail (c != NULL);
    
    if (getNetWMUserTime (clientGetXDisplay (c), c->window, &c->user_time))
    {
        FLAG_SET (c->flags, CLIENT_FLAG_HAS_USER_TIME);
    }
}

void
clientFrame (DisplayInfo *display_info, Window w, gboolean recapture)
{
    ScreenInfo *screen_info = NULL;
    XWindowAttributes attr;
    XWindowChanges wc;
    XSetWindowAttributes attributes;
    Client *c = NULL;
    gboolean shaped;
    unsigned long valuemask;
    int i;

    g_return_if_fail (w != None);
    g_return_if_fail (display_info != NULL);
    
    TRACE ("entering clientFrame");
    TRACE ("framing client (0x%lx)", w);

    gdk_error_trap_push ();
    myDisplayGrabServer (display_info);

    if (!XGetWindowAttributes (display_info->dpy, w, &attr))
    {
        TRACE ("Cannot get window attributes");
        myDisplayUngrabServer (display_info);
        gdk_error_trap_pop ();
        return;
    }
    
    screen_info = myDisplayGetScreenFromRoot (display_info, attr.root);
    if (!screen_info)
    {
        TRACE ("Cannot determine screen info from windows");
        myDisplayUngrabServer (display_info);
        gdk_error_trap_pop ();
        return;
    }

    if (w == screen_info->gnome_win)
    {
        TRACE ("Not managing our own event window");
        myDisplayUngrabServer (display_info);
        gdk_error_trap_pop ();
        return;
    }

    if (attr.override_redirect)
    {
        TRACE ("Override redirect window 0x%lx", w);
        compositorAddWindow (display_info, w, NULL);
        myDisplayUngrabServer (display_info);
        gdk_error_trap_pop ();
        return;
    }

    if (checkKdeSystrayWindow (display_info->dpy, w) && (screen_info->systray != None))
    {
        TRACE ("Not managing KDE systray windows");
        sendSystrayReqDock (display_info->dpy, w, screen_info->systray);
        myDisplayUngrabServer (display_info);
        gdk_error_trap_pop ();
        return;
    }

    c = g_new0 (Client, 1);
    if (!c)
    {
        TRACE ("Cannot allocate memory for the window structure");
        myDisplayUngrabServer (display_info);
        gdk_error_trap_pop ();
        return;
    }
    
    c->window = w;
    c->screen_info = screen_info;
    c->serial = screen_info->client_serial++;

    getWindowName (display_info->dpy, c->window, &c->name);
    TRACE ("name \"%s\"", c->name);
    getTransientFor (display_info->dpy, screen_info->screen, c->window, &c->transient_for);

    /* Initialize structure */
    c->size = NULL;
    c->flags = 0L;
    c->wm_flags = 0L;
    c->xfwm_flags = CLIENT_FLAG_INITIAL_VALUES;
    c->x = attr.x;
    c->y = attr.y;
    c->width = attr.width;
    c->height = attr.height;
    c->visual = attr.visual;
    clientGetWMNormalHints (c, FALSE);

    c->old_x = c->x;
    c->old_y = c->y;
    c->old_width = c->width;
    c->old_height = c->height;
    c->size->x = c->x;
    c->size->y = c->y;
    c->size->width = c->width;
    c->size->height = c->height;
    c->fullscreen_old_x = c->x;
    c->fullscreen_old_y = c->y;
    c->fullscreen_old_width = c->width;
    c->fullscreen_old_height = c->height;
    c->border_width = attr.border_width;
    c->cmap = attr.colormap;

    shaped = clientCheckShape(c);
    if (shaped)
    {
        FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_HAS_BORDER);
    }

    if (((c->size->flags & (PMinSize | PMaxSize)) != (PMinSize | PMaxSize))
        || (((c->size->flags & (PMinSize | PMaxSize)) ==
                (PMinSize | PMaxSize))
            && ((c->size->min_width < c->size->max_width)
                || (c->size->min_height < c->size->max_height))))
    {
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_IS_RESIZABLE);
    }

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        c->button_pressed[i] = FALSE;
    }

    if (!XGetWMColormapWindows (display_info->dpy, c->window, &c->cmap_windows, &c->ncmap))
    {
        c->ncmap = 0;
    }

    /* Opacity for compositing manager */
    c->opacity = NET_WM_OPAQUE;
    getOpacity (display_info->dpy, c->window, &c->opacity);

    /* Timeout for blinking on urgency */
    c->blink_timeout_id = 0;

    c->class.res_name = NULL;
    c->class.res_class = NULL;
    XGetClassHint (display_info->dpy, w, &c->class);
    c->wmhints = XGetWMHints (display_info->dpy, c->window);
    c->group_leader = None;
    if (c->wmhints)
    {
        if (c->wmhints->flags & WindowGroupHint)
        {
            c->group_leader = c->wmhints->window_group;
        }
    }
    c->client_leader = getClientLeader (display_info->dpy, c->window);
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    c->startup_id = NULL;
    getWindowStartupId (display_info->dpy, c->window, &c->startup_id);
#endif /* HAVE_LIBSTARTUP_NOTIFICATION */
    TRACE ("\"%s\" (0x%lx) initial map_state = %s",
                c->name, c->window,
                (attr.map_state == IsUnmapped) ?
                "IsUnmapped" :
                (attr.map_state == IsViewable) ?
                "IsViewable" :
                (attr.map_state == IsUnviewable) ?
                "IsUnviewable" :
                "(unknown)");
    if (attr.map_state != IsUnmapped)
    {
        /* Reparent will send us unmap/map events */
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_MAP_PENDING);
    }
    c->ignore_unmap = 0;
    c->type = UNSET;
    c->type_atom = None;

    FLAG_SET (c->flags, START_ICONIC (c) ? CLIENT_FLAG_ICONIFIED : 0);
    FLAG_SET (c->wm_flags, ACCEPT_INPUT (c->wmhints) ? WM_FLAG_INPUT : 0);

    clientGetWMProtocols (c);
    clientGetMWMHints (c, FALSE);
    getHint (display_info->dpy, w, win_hints, &c->win_hints);
    getHint (display_info->dpy, w, win_state, &c->win_state);
    if (!getHint (display_info->dpy, w, win_layer, &c->win_layer))
    {
        c->win_layer = WIN_LAYER_NORMAL;
    }

    /* Reload from session */
    if (sessionMatchWinToSM (c))
    {
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_SESSION_MANAGED);
    }
    
    /* Beware, order of calls is important here ! */
    sn_client_startup_properties (c);
    clientGetWinState (c);
    clientGetNetState (c);
    clientGetNetWmType (c);
    clientGetInitialNetWmDesktop (c);
    clientGetNetStruts (c);

    /* Fullscreen for older legacy apps */
    if ((c->x == 0) && (c->y == 0) &&
        (c->width == gdk_screen_get_width (screen_info->gscr)) &&
        (c->height == gdk_screen_get_height (screen_info->gscr)) &&
        !FLAG_TEST(c->xfwm_flags, XFWM_FLAG_HAS_BORDER) &&
        (c->win_layer == WIN_LAYER_NORMAL) &&
        (c->type == WINDOW_NORMAL))
    {
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_LEGACY_FULLSCREEN);
    }

    /* Once we know the type of window, we can initialize window position */
    if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_SESSION_MANAGED))
    {
        if ((attr.map_state != IsUnmapped))
        {
            clientGravitate (c, APPLY);
        }
        else
        {
            clientInitPosition (c);
        }
    }

    /* 
       We must call clientApplyInitialState() after having placed the
       window so that the inital position values are correctly set if the
       inital state is maximize or fullscreen
     */
    clientApplyInitialState (c);

    valuemask = CWEventMask;
    attributes.event_mask = (FRAME_EVENT_MASK | POINTER_EVENT_MASK);
    c->frame =
        XCreateWindow (display_info->dpy, screen_info->xroot, frameX (c), frameY (c), frameWidth (c),
        frameHeight (c), 0, CopyFromParent, InputOutput, CopyFromParent,
        valuemask, &attributes);
    XSetWindowBorderWidth (display_info->dpy, c->window, 0);
    XReparentWindow (display_info->dpy, c->window, c->frame, frameLeft (c), frameTop (c));
    
    valuemask = CWEventMask;
    attributes.event_mask = (CLIENT_EVENT_MASK);
    XChangeWindowAttributes (display_info->dpy, c->window, valuemask, &attributes);
    if ((shaped) && (display_info->shape))
    {
        XShapeSelectInput (display_info->dpy, c->window, ShapeNotifyMask);
    }

    clientAddToList (c);
    clientSetNetActions (c);
    clientGrabKeys (c);
    clientGrabButtons(c);

    /* Initialize pixmap caching */
    xfwmPixmapInit (screen_info, &c->pm_cache.pm_title[ACTIVE]);
    xfwmPixmapInit (screen_info, &c->pm_cache.pm_title[INACTIVE]);
    xfwmPixmapInit (screen_info, &c->pm_cache.pm_sides[SIDE_LEFT][ACTIVE]);
    xfwmPixmapInit (screen_info, &c->pm_cache.pm_sides[SIDE_LEFT][INACTIVE]);
    xfwmPixmapInit (screen_info, &c->pm_cache.pm_sides[SIDE_RIGHT][ACTIVE]);
    xfwmPixmapInit (screen_info, &c->pm_cache.pm_sides[SIDE_RIGHT][INACTIVE]);
    xfwmPixmapInit (screen_info, &c->pm_cache.pm_sides[SIDE_BOTTOM][ACTIVE]);
    xfwmPixmapInit (screen_info, &c->pm_cache.pm_sides[SIDE_BOTTOM][INACTIVE]);
    c->pm_cache.previous_width = -1;
    c->pm_cache.previous_height = -1;

    xfwmWindowCreate (display_info->dpy, c->frame, &c->sides[SIDE_LEFT],
        myDisplayGetCursorResize(screen_info->display_info, 4 + SIDE_LEFT));
    xfwmWindowCreate (display_info->dpy, c->frame, &c->sides[SIDE_RIGHT],
        myDisplayGetCursorResize(screen_info->display_info, 4 + SIDE_RIGHT));
    xfwmWindowCreate (display_info->dpy, c->frame, &c->sides[SIDE_BOTTOM],
        myDisplayGetCursorResize(screen_info->display_info, 4 + SIDE_BOTTOM));
    xfwmWindowCreate (display_info->dpy, c->frame, &c->corners[CORNER_BOTTOM_LEFT],
        myDisplayGetCursorResize(screen_info->display_info, CORNER_BOTTOM_LEFT));
    xfwmWindowCreate (display_info->dpy, c->frame, &c->corners[CORNER_BOTTOM_RIGHT],
        myDisplayGetCursorResize(screen_info->display_info, CORNER_BOTTOM_RIGHT));
    xfwmWindowCreate (display_info->dpy, c->frame, &c->corners[CORNER_TOP_LEFT],
        myDisplayGetCursorResize(screen_info->display_info, CORNER_TOP_LEFT));
    xfwmWindowCreate (display_info->dpy, c->frame, &c->corners[CORNER_TOP_RIGHT],
        myDisplayGetCursorResize(screen_info->display_info, CORNER_TOP_RIGHT));
    xfwmWindowCreate (display_info->dpy, c->frame, &c->title, None);
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        xfwmWindowCreate (display_info->dpy, c->frame, &c->buttons[i], None);
    }
    TRACE ("now calling configure for the new window \"%s\" (0x%lx)", c->name,
        c->window);
    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    clientConfigure (c, &wc, CWX | CWY | CWHeight | CWWidth, CFG_NOTIFY | CFG_FORCE_REDRAW);

    /* Clear time counter */
    clientClearLastOpTime (c);
    /* net_wm_user_time standard */
    clientGetUserTime (c);

    /* Notify the compositor about this new window */
    compositorAddWindow (display_info, c->frame, c);

    clientRaise (c);
    if (!FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
    {
        if ((c->win_workspace == screen_info->current_ws) || 
            FLAG_TEST(c->flags, CLIENT_FLAG_STICKY))
        {
            clientShow (c, TRUE);
            if (recapture)
            {
                clientSortRing(c);
            }
            else
            {
                clientFocusNew(c);
            }
        }
    }
    else
    {
        setWMState (display_info->dpy, c->window, IconicState);
        clientSetNetState (c);
    }
    
    /* Window is reparented now, so we can safely release the grab 
     * on the server 
     */
    myDisplayUngrabServer (display_info);
    gdk_error_trap_pop ();

    DBG ("client \"%s\" (0x%lx) is now managed", c->name, c->window);
    DBG ("client_count=%d", screen_info->client_count);
}

void
clientUnframe (Client * c, gboolean remap)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;
    int i;
    XEvent ev;
    gboolean reparented;

    TRACE ("entering clientUnframe");
    TRACE ("unframing client \"%s\" (0x%lx) [%s]", 
            c->name, c->window, remap ? "remap" : "no remap");

    g_return_if_fail (c != NULL);
    
    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    
    clientRemoveFromList (c);
    myDisplayGrabServer (display_info);
    gdk_error_trap_push ();
    clientUngrabKeys (c);
    clientGrabButtons (c);
    compositorRemoveWindow (display_info, c->frame);
    XUnmapWindow (display_info->dpy, c->frame);
    clientGravitate (c, REMOVE);
    XSelectInput (display_info->dpy, c->window, NoEventMask);
    reparented = XCheckTypedWindowEvent (display_info->dpy, c->window, ReparentNotify, &ev);

    if (remap || !reparented)
    {
        XReparentWindow (display_info->dpy, c->window, c->screen_info->xroot, c->x, c->y);
        XSetWindowBorderWidth (display_info->dpy, c->window, c->border_width);
        if (remap)
        {
            compositorAddWindow (display_info, c->window, NULL);
            XMapWindow (display_info->dpy, c->window);
        }
        else
        {
            XUnmapWindow (display_info->dpy, c->window);
            setWMState (display_info->dpy, c->window, WithdrawnState);
        }
    }

    if (!remap)
    {
        XDeleteProperty (display_info->dpy, c->window, net_wm_state);
        XDeleteProperty (display_info->dpy, c->window, win_state);
        XDeleteProperty (display_info->dpy, c->window, net_wm_desktop);
        XDeleteProperty (display_info->dpy, c->window, win_workspace);
        XDeleteProperty (display_info->dpy, c->window, win_layer);
        XDeleteProperty (display_info->dpy, c->window, net_wm_allowed_actions);
    }
    
    xfwmWindowDelete (&c->title);
    xfwmWindowDelete (&c->sides[SIDE_LEFT]);
    xfwmWindowDelete (&c->sides[SIDE_RIGHT]);
    xfwmWindowDelete (&c->sides[SIDE_BOTTOM]);
    xfwmWindowDelete (&c->sides[CORNER_BOTTOM_LEFT]);
    xfwmWindowDelete (&c->sides[CORNER_BOTTOM_RIGHT]);
    xfwmWindowDelete (&c->sides[CORNER_TOP_LEFT]);
    xfwmWindowDelete (&c->sides[CORNER_TOP_RIGHT]);
    clientClearPixmapCache (c);
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        xfwmWindowDelete (&c->buttons[i]);
    }
    XDestroyWindow (display_info->dpy, c->frame);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUT))
    {
        workspaceUpdateArea (c->screen_info);
    }
    
    myDisplayUngrabServer (display_info);
    gdk_error_trap_pop ();
    clientFree (c);
}

void
clientFrameAll (ScreenInfo *screen_info)
{
    DisplayInfo *display_info = NULL;

    unsigned int count, i;
    xfwmWindow shield;
    Window w1, w2, *wins = NULL;
    XWindowAttributes attr;

    TRACE ("entering clientFrameAll");

    display_info = screen_info->display_info;
    clientSetFocus (screen_info, NULL, GDK_CURRENT_TIME, NO_FOCUS_FLAG);
    xfwmWindowTemp (display_info->dpy, screen_info->xroot, &shield, 0, 0, 
                        gdk_screen_get_width (screen_info->gscr),
                        gdk_screen_get_height (screen_info->gscr), 
                        EnterWindowMask);

    XSync (display_info->dpy, FALSE);
    myDisplayGrabServer (display_info);
    XQueryTree (display_info->dpy, screen_info->xroot, &w1, &w2, &wins, &count);
    for (i = 0; i < count; i++)
    {
        XGetWindowAttributes (display_info->dpy, wins[i], &attr);
        if ((attr.map_state == IsViewable) && (attr.root == screen_info->xroot))
        {
            clientFrame (screen_info->display_info, wins[i], TRUE);
        }
    }
    if (wins)
    {
        XFree (wins);
    }
    clientFocusTop (screen_info, WIN_LAYER_NORMAL);
    xfwmWindowDelete (&shield);
    myDisplayUngrabServer (display_info);
    XSync (display_info->dpy, FALSE);
}

void
clientUnframeAll (ScreenInfo *screen_info)
{
    DisplayInfo *display_info = NULL;
    Client *c = NULL;
    unsigned int count, i;
    Window w1, w2, *wins = NULL;

    TRACE ("entering clientUnframeAll");

    display_info = screen_info->display_info;
    clientSetFocus (screen_info, NULL, GDK_CURRENT_TIME, FOCUS_IGNORE_MODAL);
    XSync (display_info->dpy, FALSE);
    myDisplayGrabServer (display_info);
    XQueryTree (display_info->dpy, screen_info->xroot, &w1, &w2, &wins, &count);
    for (i = 0; i < count; i++)
    {
        c = clientGetFromWindow (screen_info, wins[i], FRAME);
        if (c)
        {
            clientUnframe (c, TRUE);
        }
    }
    myDisplayUngrabServer (display_info);
    XSync(display_info->dpy, FALSE);
    if (wins)
    {
        XFree (wins);
    }
}

Client *
clientGetFromWindow (ScreenInfo *screen_info, Window w, int mode)
{
    Client *c = NULL;
    int i;

    g_return_val_if_fail (w != None, NULL);
    TRACE ("entering clientGetFromWindow");
    TRACE ("looking for (0x%lx)", w);

    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, i++)
    {
        switch (mode)
        {
            case WINDOW:
                if (c->window == w)
                {
                    TRACE ("found \"%s\" (mode WINDOW)", c->name);
                    return (c);
                }
                break;
            case FRAME:
                if (c->frame == w)
                {
                    TRACE ("found \"%s\" (mode FRAME)", c->name);
                    return (c);
                }
                break;
            case ANY:
            default:
                if ((c->frame == w) || (c->window == w))
                {
                    TRACE ("found \"%s\" (mode ANY)", c->name);
                    return (c);
                }
                break;
        }
    }
    TRACE ("no client found");

    return NULL;
}

static void
clientSetWorkspaceSingle (Client * c, int ws)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientSetWorkspaceSingle");

    if (ws > c->screen_info->workspace_count - 1)
    {
        ws = c->screen_info->workspace_count - 1;
        TRACE ("value off limits, using %i instead", ws);
    }

    if (c->win_workspace != ws)
    {
        TRACE ("setting client \"%s\" (0x%lx) to current_ws %d", c->name, c->window, ws);
        c->win_workspace = ws;
        setHint (clientGetXDisplay (c), c->window, win_workspace, ws);
        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
        {
            setHint (clientGetXDisplay (c), c->window, net_wm_desktop, (unsigned long) ALL_WORKSPACES);
        }
        else
        {
            setHint (clientGetXDisplay (c), c->window, net_wm_desktop, (unsigned long) ws);
        }
    }
    FLAG_SET (c->xfwm_flags, XFWM_FLAG_WORKSPACE_SET);
}

void
clientSetWorkspace (Client * c, int ws, gboolean manage_mapping)
{
    GList *list_of_windows = NULL;
    GList *index = NULL;
    Client *c2 = NULL;

    g_return_if_fail (c != NULL);

    TRACE ("entering clientSetWorkspace");

    list_of_windows = clientListTransientOrModal (c);
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if (c2->win_workspace != ws)
        {
            TRACE ("setting client \"%s\" (0x%lx) to current_ws %d", c->name, c->window, ws);
            clientSetWorkspaceSingle (c2, ws);
            if (manage_mapping && !clientIsValidTransientOrModal (c2)
                && !FLAG_TEST (c2->flags, CLIENT_FLAG_ICONIFIED))
            {
                if (FLAG_TEST (c2->flags, CLIENT_FLAG_STICKY))
                {
                    clientShow (c2, FALSE);
                }
                else
                {
                    if (ws == c2->screen_info->current_ws)
                    {
                        clientShow (c2, FALSE);
                    }
                    else
                    {
                        clientHide (c2, c2->screen_info->current_ws, FALSE);
                    }
                }
            }
        }
    }
    g_list_free (list_of_windows);
}

static void
clientShowSingle (Client * c, gboolean change_state)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;

    g_return_if_fail (c != NULL);
        
    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    myDisplayGrabServer (display_info);
    if ((c->win_workspace == screen_info->current_ws) || FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        TRACE ("showing client \"%s\" (0x%lx)", c->name, c->window);
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_VISIBLE);
        compositorWindowMap (display_info, c->frame);
        XMapWindow (display_info->dpy, c->frame);
        XMapWindow (display_info->dpy, c->window);
        /* Adjust to urgency state as the window is visible */
        clientUpdateUrgency (c);
    }
    if (change_state)
    {
        FLAG_UNSET (c->flags, CLIENT_FLAG_ICONIFIED);
        setWMState (display_info->dpy, c->window, NormalState);
        workspaceUpdateArea (screen_info);
    }
    myDisplayUngrabServer (display_info);
    clientSetNetState (c);
}

void
clientShow (Client * c, gboolean change_state)
{
    GList *list_of_windows = NULL;
    GList *index = NULL;
    Client *c2 = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientShow \"%s\" (0x%lx) [with %s]", 
           c->name, c->window,
           change_state ? "state change" : "no state change");
             
    list_of_windows = clientListTransientOrModal (c);
    for (index = g_list_last (list_of_windows); index; index = g_list_previous (index))
    {
        c2 = (Client *) index->data;
        clientSetWorkspaceSingle (c2, c->win_workspace);
        /* Ignore request before if the window is not yet managed */
        if (!FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_MANAGED))
        {
            continue;
        }
        clientShowSingle (c2, change_state);
    }
    g_list_free (list_of_windows);
}

static void
clientHideSingle (Client * c, gboolean change_state)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;

    g_return_if_fail (c != NULL);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    
    myDisplayGrabServer (display_info);
    TRACE ("hiding client \"%s\" (0x%lx)", c->name, c->window);
    clientPassFocus(c->screen_info, c, c);
    compositorWindowUnmap (display_info, c->frame);
    XUnmapWindow (display_info->dpy, c->frame);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
    {
        FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_VISIBLE);
        c->ignore_unmap++;
        /* Adjust to urgency state as the window is not visible */
        clientUpdateUrgency (c);
    }
    if (change_state)
    {
        FLAG_SET (c->flags, CLIENT_FLAG_ICONIFIED);
        XUnmapWindow (display_info->dpy, c->window);
        setWMState (display_info->dpy, c->window, IconicState);
        workspaceUpdateArea (c->screen_info);
    }
    myDisplayUngrabServer (display_info);
    clientSetNetState (c);
}

void
clientHide (Client * c, int ws, gboolean change_state)
{
    GList *list_of_windows = NULL;
    GList *index = NULL;
    Client *c2 = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientHide");

    list_of_windows = clientListTransientOrModal (c);
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;

        /* Ignore request before if the window is not yet managed */
        if (!FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_MANAGED))
        {
            continue;
        }

        /* ws is used when transitioning between desktops, to avoid
           hiding a transient for group that will be shown again on the new
           workspace (transient for groups can be transients for multiple 
           ancesors splitted across workspaces...)
         */
        if (clientIsTransientOrModalForGroup (c2)
            && clientTransientOrModalHasAncestor (c2, ws))
        {
            /* Other ancestors for that transient are still on screen, so don't
               hide it...
             */
            continue;
        }
        clientHideSingle (c2, change_state);
    }
    g_list_free (list_of_windows);
}

void
clientHideAll (Client * c, int ws)
{
    Client *c2 = NULL;
    ScreenInfo *screen_info = NULL;
    int i;

    g_return_if_fail (c != NULL);
    
    TRACE ("entering clientHideAll");

    screen_info = c->screen_info;
    for (c2 = c->next, i = 0; (c2) && (i < screen_info->client_count); c2 = c2->next, i++)
    {
        if (CLIENT_CAN_HIDE_WINDOW (c2)
            && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_HAS_BORDER)
            && !clientIsValidTransientOrModal (c2) && (c2 != c))
        {
            if (((!c) && (c2->win_workspace == ws)) || ((c)
                    && !clientIsTransientOrModalFor (c, c2)
                    && (c2->win_workspace == c->win_workspace)))
            {
                clientHide (c2, ws, TRUE);
            }
        }
    }
}

void
clientToggleShowDesktop (ScreenInfo *screen_info, gboolean show_desktop)
{
    GList *index = NULL;

    TRACE ("entering clientToggleShowDesktop");

    clientSetFocus (screen_info, NULL, GDK_CURRENT_TIME, FOCUS_IGNORE_MODAL);
    if (show_desktop)
    {
        for (index = screen_info->windows_stack; index; index = g_list_next (index))
        {
            Client *c = (Client *) index->data;
            if (CLIENT_CAN_HIDE_WINDOW (c)
                && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
                && !FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
            {
                {
                    FLAG_SET (c->xfwm_flags, XFWM_FLAG_WAS_SHOWN);
                    clientHide (c, c->win_workspace, TRUE);
                }
            }
        }
        clientFocusTop (screen_info, WIN_LAYER_DESKTOP);
    }
    else
    {
        for (index = g_list_last(screen_info->windows_stack); index; index = g_list_previous (index))
        {
            Client *c = (Client *) index->data;
            if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_WAS_SHOWN))
            {
                {
                    clientShow (c, TRUE);
                }
            }
            FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_WAS_SHOWN);
        }
        clientFocusTop (screen_info, WIN_LAYER_NORMAL);
    }
}

void
clientClose (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientClose");
    TRACE ("closing client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->wm_flags, WM_FLAG_DELETE))
    {
        sendClientMessage (c->screen_info, c->window, wm_protocols, wm_delete_window, GDK_CURRENT_TIME);
    }
    else
    {
        clientKill (c);
    }
}

void
clientKill (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientKill");
    TRACE ("killing client \"%s\" (0x%lx)", c->name, c->window);

    XKillClient (clientGetXDisplay (c), c->window);
}

void
clientEnterContextMenuState (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientEnterContextMenuState");
    TRACE ("Showing the what's this help for client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->wm_flags, WM_FLAG_CONTEXT_HELP))
    {
        sendClientMessage (c->screen_info, c->window, wm_protocols, kde_net_wm_context_help, GDK_CURRENT_TIME);
    }
}

void
clientSetLayer (Client * c, int l)
{
    GList *list_of_windows = NULL;
    GList *index = NULL;
    Client *c2 = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetLayer");

    list_of_windows = clientListTransientOrModal (c);
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if (c2->win_layer != l)
        {
            TRACE ("setting client \"%s\" (0x%lx) layer to %d", c2->name,
                c2->window, l);
            c2->win_layer = l;
            setHint (clientGetXDisplay (c2), c2->window, win_layer, l);
        }
    }
    g_list_free (list_of_windows);
    if (clientGetLastRaise (c->screen_info) == c)
    {
        clientClearLastRaise (c->screen_info);
    }
    clientRaise (c);
    clientPassGrabButton1 (c);
}

void
clientShade (Client * c)
{
    XWindowChanges wc;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleShaded");
    TRACE ("shading client \"%s\" (0x%lx)", c->name, c->window);

    if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
        || FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE
            ("cowardly refusing to shade \"%s\" (0x%lx) because it has no border",
            c->name, c->window);
        return;
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        TRACE ("\"%s\" (0x%lx) is already shaded", c->name, c->window);
        return;
    }

    c->win_state |= WIN_STATE_SHADED;
    FLAG_SET (c->flags, CLIENT_FLAG_SHADED);
    clientSetNetState (c);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MANAGED))
    {
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure (c, &wc, CWWidth | CWHeight, CFG_FORCE_REDRAW);
    }
}

void
clientUnshade (Client * c)
{
    XWindowChanges wc;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleShaded");
    TRACE ("shading/unshading client \"%s\" (0x%lx)", c->name, c->window);

    if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        TRACE ("\"%s\" (0x%lx) is not shaded", c->name, c->window);
        return;
    }
    c->win_state &= ~WIN_STATE_SHADED;
    FLAG_UNSET (c->flags, CLIENT_FLAG_SHADED);
    clientSetNetState (c);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MANAGED))
    {
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure (c, &wc, CWWidth | CWHeight, CFG_FORCE_REDRAW);
    }
}

void
clientToggleShaded (Client * c)
{
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        clientUnshade (c);
    }
    else
    {
        clientShade (c);
    }
}

void
clientStick (Client * c, gboolean include_transients)
{
    GList *list_of_windows = NULL;
    GList *index = NULL;
    Client *c2 = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientStick");

    if (include_transients)
    {
        list_of_windows = clientListTransientOrModal (c);
        for (index = list_of_windows; index; index = g_list_next (index))
        {
            c2 = (Client *) index->data;
            TRACE ("sticking client \"%s\" (0x%lx)", c2->name, c2->window);
            c2->win_state |= WIN_STATE_STICKY;
            FLAG_SET (c2->flags, CLIENT_FLAG_STICKY);
            setHint (clientGetXDisplay (c2), c2->window, net_wm_desktop,
                (unsigned long) ALL_WORKSPACES);
            clientSetNetState (c2);
        }
        clientSetWorkspace (c, c->screen_info->current_ws, TRUE);
        g_list_free (list_of_windows);
    }
    else
    {
        TRACE ("sticking client \"%s\" (0x%lx)", c->name, c->window);
        c->win_state |= WIN_STATE_STICKY;
        FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
        setHint (clientGetXDisplay (c), c->window, net_wm_desktop,
            (unsigned long) ALL_WORKSPACES);
        clientSetNetState (c);
        clientSetWorkspace (c, c->screen_info->current_ws, TRUE);
    }
}

void
clientUnstick (Client * c, gboolean include_transients)
{
    GList *list_of_windows = NULL;
    GList *index = NULL;
    Client *c2 = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUnstick");
    TRACE ("unsticking client \"%s\" (0x%lx)", c->name, c->window);

    if (include_transients)
    {
        list_of_windows = clientListTransientOrModal (c);
        for (index = list_of_windows; index; index = g_list_next (index))
        {
            c2 = (Client *) index->data;
            c2->win_state &= ~WIN_STATE_STICKY;
            FLAG_UNSET (c2->flags, CLIENT_FLAG_STICKY);
            setHint (clientGetXDisplay (c2), c2->window, net_wm_desktop,
                (unsigned long) c2->screen_info->current_ws);
            clientSetNetState (c2);
        }
        clientSetWorkspace (c, c->screen_info->current_ws, TRUE);
        g_list_free (list_of_windows);
    }
    else
    {
        c->win_state &= ~WIN_STATE_STICKY;
        FLAG_UNSET (c->flags, CLIENT_FLAG_STICKY);
        setHint (clientGetXDisplay (c), c->window, net_wm_desktop,
            (unsigned long) c->screen_info->current_ws);
        clientSetNetState (c);
        clientSetWorkspace (c, c->screen_info->current_ws, TRUE);
    }
}

void
clientToggleSticky (Client * c, gboolean include_transients)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleSticky");
    TRACE ("sticking/unsticking client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        clientUnstick (c, include_transients);
    }
    else
    {
        clientStick (c, include_transients);
    }
}

void clientToggleFullscreen (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleFullscreen");
    TRACE ("toggle fullscreen client \"%s\" (0x%lx)", c->name, c->window);

    /* Can we switch to full screen, does it make any sense? */
    if (!FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN) && (c->size->flags & PMaxSize))
    {
        GdkRectangle rect;
        gint monitor_nbr;
        int cx, cy;

        cx = frameX (c) + (frameWidth (c) / 2);
        cy = frameY (c) + (frameHeight (c) / 2);

        monitor_nbr = gdk_screen_get_monitor_at_point (c->screen_info->gscr, cx, cy);
        gdk_screen_get_monitor_geometry (c->screen_info->gscr, monitor_nbr, &rect);
        
        if ((c->size->max_width < rect.width) || (c->size->max_height < rect.height))
        {
            return;
        }
    }

    if (!clientIsValidTransientOrModal (c) && (c->type == WINDOW_NORMAL))
    {
        FLAG_TOGGLE (c->flags, CLIENT_FLAG_FULLSCREEN);
        clientUpdateFullscreenState (c);
    }
}

void clientToggleAbove (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleAbove");
    TRACE ("toggle above client \"%s\" (0x%lx)", c->name, c->window);

    if (!clientIsValidTransientOrModal (c) && !FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
    {
        FLAG_TOGGLE (c->flags, CLIENT_FLAG_ABOVE);
        clientUpdateAboveState (c);
    }
}

void clientToggleBelow (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleBelow");
    TRACE ("toggle below client \"%s\" (0x%lx)", c->name, c->window);

    if (!clientIsValidTransientOrModal (c) && !FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
    {
        FLAG_TOGGLE (c->flags, CLIENT_FLAG_BELOW);
        clientUpdateAboveState (c);
    }
}

void
clientRemoveMaximizeFlag (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientRemoveMaximizeFlag");
    TRACE ("Removing maximize flag on client \"%s\" (0x%lx)", c->name,
        c->window);

    c->win_state &= ~WIN_STATE_MAXIMIZED;
    FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED);
    frameDraw (c, FALSE, FALSE);
    clientSetNetState (c);
}

void
clientToggleMaximized (Client * c, int mode)
{
    ScreenInfo *screen_info = NULL;
    XWindowChanges wc;
    int cx, cy, full_x, full_y, full_w, full_h;
    GdkRectangle rect;
    gint monitor_nbr;
    
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleMaximized");
    TRACE ("maximzing/unmaximizing client \"%s\" (0x%lx)", c->name,
        c->window);

    screen_info = c->screen_info;

    if (!CLIENT_CAN_MAXIMIZE_WINDOW (c))
    {
        return;
    }

    cx = frameX (c) + (frameWidth (c) / 2);
    cy = frameY (c) + (frameHeight (c) / 2);

    monitor_nbr = gdk_screen_get_monitor_at_point (screen_info->gscr, cx, cy);
    gdk_screen_get_monitor_geometry (screen_info->gscr, monitor_nbr, &rect);

    full_x = MAX (screen_info->params->xfwm_margins[LEFT], rect.x);
    full_y = MAX (screen_info->params->xfwm_margins[TOP], rect.y);
    full_w = MIN (gdk_screen_get_width (screen_info->gscr) - screen_info->params->xfwm_margins[RIGHT], 
                  rect.x + rect.width) - full_x;
    full_h = MIN (gdk_screen_get_height (screen_info->gscr) - screen_info->params->xfwm_margins[BOTTOM], 
                  rect.y + rect.height) - full_y;

    /* Adjust size to the widest size available, not covering struts */
    clientMaxSpace (screen_info, &full_x, &full_y, &full_w, &full_h);

    if (mode & WIN_STATE_MAXIMIZED_HORIZ)
    {
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ))
        {
            c->old_x = c->x;
            c->old_width = c->width;
            wc.x = full_x + frameLeft (c);
            wc.width = full_w - frameLeft (c) - frameRight (c);
            c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
        }
        else
        {
            wc.x = c->old_x;
            wc.width = c->old_width;
            c->win_state &= ~WIN_STATE_MAXIMIZED_HORIZ;
            FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
        }
    }
    else
    {
        wc.x = c->x;
        wc.width = c->width;
    }

    if (mode & WIN_STATE_MAXIMIZED_VERT)
    {
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT))
        {
            c->old_y = c->y;
            c->old_height = c->height;
            wc.y = full_y + frameTop (c);
            wc.height = full_h - frameTop (c) - frameBottom (c);
            c->win_state |= WIN_STATE_MAXIMIZED_VERT;
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
        }
        else
        {
            wc.y = c->old_y;
            wc.height = c->old_height;
            c->win_state &= ~WIN_STATE_MAXIMIZED_VERT;
            FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
        }
    }
    else
    {
        wc.y = c->y;
        wc.height = c->height;
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
        XGrabPointer (clientGetXDisplay (c), screen_info->gnome_win, FALSE, EnterWindowMask, GrabModeAsync,
                           GrabModeAsync, screen_info->xroot, None, GDK_CURRENT_TIME);
        clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_NOTIFY);
        XUngrabPointer (clientGetXDisplay (c), GDK_CURRENT_TIME);
    }
    else
    {
        c->x = wc.x;
        c->y = wc.y;
        c->height = wc.height;
        c->width = wc.width;
    }
}

/* Xrandr stuff: on screen size change, make sure all clients are still visible */
void 
clientScreenResize(ScreenInfo *screen_info)
{
    Client *c = NULL;
    GList *index, *list_of_windows;
    XWindowChanges wc;
    
    list_of_windows = clientGetStackList (screen_info);

    if (!list_of_windows)
    {
        return;
    }
        
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        if (!CONSTRAINED_WINDOW (c))
        {
            continue;
        }
        wc.x = c->x;
        wc.y = c->y;
        clientConfigure (c, &wc, CWX | CWY, CFG_CONSTRAINED);
    }
    
    g_list_free (list_of_windows);
}

void
clientDrawOutline (Client * c)
{
    TRACE ("entering clientDrawOutline");

    XDrawRectangle (clientGetXDisplay (c), c->screen_info->xroot, c->screen_info->box_gc, frameX (c), frameY (c),
        frameWidth (c) - 1, frameHeight (c) - 1);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
        &&!FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN | CLIENT_FLAG_SHADED))
    {
        XDrawRectangle (clientGetXDisplay (c), c->screen_info->xroot, c->screen_info->box_gc, c->x, c->y, c->width - 1,
            c->height - 1);
    }
}

static void
clientSnapPosition (Client * c)
{
    Client *c2 = NULL;
    ScreenInfo *screen_info = NULL;
    int cx, cy, i, delta;
    int disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_x, frame_y, frame_height, frame_width;
    int frame_top, frame_left, frame_right, frame_bottom;
    int frame_x2, frame_y2;
    int best_frame_x, best_frame_y;
    int best_delta_x, best_delta_y;
    int c_frame_x1, c_frame_x2, c_frame_y1, c_frame_y2;
    GdkRectangle rect;
    gint monitor_nbr;
        
    g_return_if_fail (c != NULL);
    TRACE ("entering clientSnapPosition");
    TRACE ("Snapping client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    best_delta_x = screen_info->params->snap_width + 1;
    best_delta_y = screen_info->params->snap_width + 1;
    
    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_height = frameHeight (c);
    frame_width = frameWidth (c);
    frame_top = frameTop (c);
    frame_left = frameLeft (c);
    frame_right = frameRight (c);
    frame_bottom = frameBottom (c);

    cx = frame_x + (frame_width / 2);
    cy = frame_y + (frame_height / 2);

    frame_x2 = frame_x + frame_width;
    frame_y2 = frame_y + frame_height;
    best_frame_x = frame_x;
    best_frame_y = frame_y;

    monitor_nbr = gdk_screen_get_monitor_at_point (screen_info->gscr, cx, cy);
    gdk_screen_get_monitor_geometry (screen_info->gscr, monitor_nbr, &rect);

    disp_x = rect.x;
    disp_y = rect.y;
    disp_max_x = rect.x + rect.width;
    disp_max_y = rect.y + rect.height;
    
    if (screen_info->params->snap_to_border)
    {
        if (abs (disp_x - frame_x) < abs (disp_max_x - frame_x2))
        {
            best_delta_x = abs (disp_x - frame_x);
            best_frame_x = disp_x;
        }
        else
        {
            best_delta_x = abs (disp_max_x - frame_x2);
            best_frame_x = disp_max_x - frame_width;
        }

        if (abs (disp_y - frame_y) < abs (disp_max_y - frame_y2))
        {
            best_delta_y = abs (disp_y - frame_y);
            best_frame_y = disp_y;
        }
        else
        {
            best_delta_y = abs (disp_max_y - frame_y2);
            best_frame_y = disp_max_y - frame_height;
        }
    }

    for (c2 = screen_info->clients, i = 0; i < screen_info->client_count; c2 = c2->next, i++)
    {
        if (FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_VISIBLE)  && (c2 != c) && 
            (((screen_info->params->snap_to_windows) && (c2->win_layer == c->win_layer))
             || ((screen_info->params->snap_to_border) 
                  && FLAG_TEST (c2->flags, CLIENT_FLAG_HAS_STRUT)
                  && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_VISIBLE))))
        {
            c_frame_x1 = frameX (c2);
            c_frame_x2 = c_frame_x1 + frameWidth (c2);
            c_frame_y1 = frameY (c2);
            c_frame_y2 = c_frame_y1 + frameHeight (c2);

            if ((c_frame_y1 <= frame_y2) && (c_frame_y2 >= frame_y))
            {
                delta = abs (c_frame_x2 - frame_x);
                if (delta < best_delta_x)
                {
                    best_delta_x = delta;
                    best_frame_x = c_frame_x2;
                }

                delta = abs (c_frame_x1 - frame_x2);
                if (delta < best_delta_x)
                {
                    best_delta_x = delta;
                    best_frame_x = c_frame_x1 - frame_width;
                }
            }

            if ((c_frame_x1 <= frame_x2) && (c_frame_x2 >= frame_x))
            {
                delta = abs (c_frame_y2 - frame_y);
                if (delta < best_delta_y)
                {
                    best_delta_y = delta;
                    best_frame_y = c_frame_y2;
                }

                delta = abs (c_frame_y1 - frame_y2);
                if (delta < best_delta_y)
                {
                    best_delta_y = delta;
                    best_frame_y = c_frame_y1 - frame_height;
                }
            }
        }
    }

    if (best_delta_x <= screen_info->params->snap_width)
    {
        c->x = best_frame_x + frame_left;
    }
    if (best_delta_y <= screen_info->params->snap_width)
    {
        c->y = best_frame_y + frame_top;
    }
}

static XfceFilterStatus
clientMove_event_filter (XEvent * xevent, gpointer data)
{
    static int edge_scroll_x = 0;
    static int edge_scroll_y = 0;
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;
    XfceFilterStatus status = XEV_FILTER_STOP;
    MoveResizeData *passdata = (MoveResizeData *) data;
    Client *c = NULL;
    gboolean moving = TRUE;
    XWindowChanges wc;

    TRACE ("entering clientMove_event_filter");

    c = passdata->c;
    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (xevent->type == KeyPress)
    {
        if (passdata->use_keys)
        {
            if (!passdata->grab && screen_info->params->box_move)
            {
                myDisplayGrabServer (display_info);
                passdata->grab = TRUE;
                clientDrawOutline (c);
            }
            if (screen_info->params->box_move)
            {
                clientDrawOutline (c);
            }
            if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_LEFT].keycode)
            {
                c->x = c->x - 16;
            }
            else if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_RIGHT].keycode)
            {
                c->x = c->x + 16;
            }
            else if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_UP].keycode)
            {
                c->y = c->y - 16;
            }
            else if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_DOWN].keycode)
            {
                c->y = c->y + 16;
            }
            clientConstrainPos (c, FALSE);

#ifdef SHOW_POSITION
            if (passdata->poswin)
            {
                poswinSetPosition (passdata->poswin, c);
            }
#endif /* SHOW_POSITION */
            if (screen_info->params->box_move)
            {
                clientDrawOutline (c);
            }
            else
            {
                wc.x = c->x;
                wc.y = c->y;
                clientConfigure (c, &wc, CWX | CWY, NO_CFG_FLAG);
            }
        }
    }
    else if (xevent->type == KeyRelease)
    {
        if (passdata->use_keys)
        {
            if (IsModifierKey (XKeycodeToKeysym (display_info->dpy, xevent->xkey.keycode, 0)))
            {
                moving = FALSE;
            }
        }
    }
    else if (xevent->type == MotionNotify)
    {
        while (XCheckMaskEvent (display_info->dpy, ButtonMotionMask | PointerMotionMask, xevent))
            ; /* Skip event */
                    
        if (xevent->type == ButtonRelease)
        {
            moving = FALSE;
        }

        if (!passdata->grab && screen_info->params->box_move)
        {
            myDisplayGrabServer (display_info);
            passdata->grab = TRUE;
            clientDrawOutline (c);
        }
        if (screen_info->params->box_move)
        {
            clientDrawOutline (c);
        }

        if ((screen_info->workspace_count > 1) && !clientIsValidTransientOrModal (c))
        {
            if ((screen_info->params->wrap_windows) && (screen_info->params->wrap_resistance))
            {
                int msx, msy, maxx, maxy;

                msx = xevent->xmotion.x_root;
                msy = xevent->xmotion.y_root;
                maxx = gdk_screen_get_width (screen_info->gscr) - 1;
                maxy = gdk_screen_get_height (screen_info->gscr) - 1;

                if ((msx == 0) || (msx == maxx))
                {
                    edge_scroll_x++;
                }
                else
                {
                    edge_scroll_x = 0;
                }
                if ((msy == 0) || (msy == maxy))
                {
                    edge_scroll_y++;
                }
                else
                {
                    edge_scroll_y = 0;
                }

                if (edge_scroll_x > screen_info->params->wrap_resistance)
                {
                    edge_scroll_x = 0;
                    if (msx == 0)
                    {
                        if (workspaceMove (screen_info, 0, -1, c))
                        {
                            XWarpPointer (display_info->dpy, None, screen_info->xroot,
                                          0, 0, 0, 0, maxx - 10, msy);
                            msx = xevent->xmotion.x_root = maxx - 10;
                        }
                    }
                    else if (msx == maxx)
                    {
                        if (workspaceMove (screen_info, 0, 1, c))
                        {
                            XWarpPointer (display_info->dpy, None, screen_info->xroot, 
                                          0, 0, 0, 0, 10, msy);
                            msx = xevent->xmotion.x_root = 10;
                        }
                    }
                }
                if (edge_scroll_y > screen_info->params->wrap_resistance)
                {
                    edge_scroll_y = 0;
                    if (msy == 0)
                    {
                        if (workspaceMove (screen_info, -1, 0, c))
                        {
                            XWarpPointer (display_info->dpy, None, screen_info->xroot,
                                          0, 0, 0, 0, msx, maxy - 10);
                            msy = xevent->xmotion.y_root = maxy - 10;
                        }
                    }
                    else if (msy == maxy)
                    {
                        if (workspaceMove (screen_info, 1, 0, c))
                        {
                            XWarpPointer (display_info->dpy, None, screen_info->xroot, 
                                          0, 0, 0, 0, msx, 10);
                            msy = xevent->xmotion.y_root = 10;
                        }
                    }
                }
            }
        }

        c->x = passdata->ox + (xevent->xmotion.x_root - passdata->mx);
        c->y = passdata->oy + (xevent->xmotion.y_root - passdata->my);

        clientSnapPosition (c);
        clientConstrainPos (c, FALSE);

#ifdef SHOW_POSITION
        if (passdata->poswin)
        {
            poswinSetPosition (passdata->poswin, c);
        }
#endif /* SHOW_POSITION */
        if (screen_info->params->box_move)
        {
            clientDrawOutline (c);
        }
        else
        {
            wc.x = c->x;
            wc.y = c->y;
            clientConfigure (c, &wc, CWX | CWY, NO_CFG_FLAG);
        }
    }
    else if (xevent->type == ButtonRelease)
    {
        if (!passdata->use_keys)
        {
            moving = FALSE;
        }
    }
    else if (xevent->type == UnmapNotify && xevent->xunmap.window == c->window)
    {
        moving = FALSE;
    }
    else if ((xevent->type == EnterNotify) || (xevent->type == LeaveNotify))
    {
        /* Ignore enter/leave events */
    }
    else
    {
        status = XEV_FILTER_CONTINUE;
    }

    TRACE ("leaving clientMove_event_filter");

    if (!moving)
    {
        TRACE ("event loop now finished");
        edge_scroll_x = 0;
        edge_scroll_y = 0;
        gtk_main_quit ();
    }

    return status;
}

void
clientMove (Client * c, XEvent * e)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;
    XWindowChanges wc;
    MoveResizeData passdata;
    Cursor cursor = None;
    int g1 = GrabSuccess, g2 = GrabSuccess;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientDoMove");
    TRACE ("moving client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    passdata.c = c;
    passdata.ox = c->x;
    passdata.oy = c->y;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;

    xfwmWindowTemp (display_info->dpy, screen_info->xroot, &passdata.tmp_event_window, 0, 0, 
                        gdk_screen_get_width (screen_info->gscr),
                        gdk_screen_get_height (screen_info->gscr), 
                        ButtonMotionMask | ButtonReleaseMask);

    if (e->type == KeyPress)
    {
        cursor = None;
        passdata.use_keys = TRUE;
        passdata.mx = e->xkey.x_root;
        passdata.my = e->xkey.y_root;
    }
    else if (e->type == ButtonPress)
    {
        cursor = None;
        passdata.mx = e->xbutton.x_root;
        passdata.my = e->xbutton.y_root;
    }
    else
    {
        cursor = myDisplayGetCursorMove(display_info);
        getMouseXY (screen_info, screen_info->xroot, &passdata.mx, &passdata.my);
    }
    g1 = XGrabKeyboard (display_info->dpy, MYWINDOW_XWINDOW (passdata.tmp_event_window),
                        FALSE, GrabModeAsync, GrabModeAsync, GDK_CURRENT_TIME);
    g2 = XGrabPointer (display_info->dpy, MYWINDOW_XWINDOW (passdata.tmp_event_window),
                        FALSE, ButtonMotionMask | ButtonReleaseMask, GrabModeAsync,
                        GrabModeAsync, screen_info->xroot, cursor, GDK_CURRENT_TIME);

    if (((passdata.use_keys) && (g1 != GrabSuccess)) || (g2 != GrabSuccess))
    {
        TRACE ("grab failed in clientMove");
        gdk_beep ();
        if ((passdata.use_keys) && (g1 == GrabSuccess))
        {
            XUngrabKeyboard (display_info->dpy, GDK_CURRENT_TIME);
        }
        if (g2 == GrabSuccess)
        {
            XUngrabPointer (display_info->dpy, GDK_CURRENT_TIME);
        }
        xfwmWindowDelete (&passdata.tmp_event_window);
        return;
    }

#ifdef SHOW_POSITION
    passdata.poswin = poswinCreate(screen_info->gscr);
    poswinSetPosition (passdata.poswin, c);
    poswinShow (passdata.poswin);
#endif /* SHOW_POSITION */

    FLAG_SET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);
    TRACE ("entering move loop");
    xfce_push_event_filter (display_info->xfilter, clientMove_event_filter, &passdata);
    if (passdata.use_keys)
    {
        XPutBackEvent (display_info->dpy, e);
    }
    gtk_main ();
    xfce_pop_event_filter (display_info->xfilter);
    TRACE ("leaving move loop");
    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);
#ifdef SHOW_POSITION
    if (passdata.poswin)
    {
        poswinDestroy (passdata.poswin);
    }
#endif /* SHOW_POSITION */

    if (passdata.grab && screen_info->params->box_move)
    {
        clientDrawOutline (c);
    }
    wc.x = c->x;
    wc.y = c->y;
    clientConfigure (c, &wc, CWX | CWY, NO_CFG_FLAG);

    XUngrabKeyboard (display_info->dpy, GDK_CURRENT_TIME);
    XUngrabPointer (display_info->dpy, GDK_CURRENT_TIME);
    xfwmWindowDelete (&passdata.tmp_event_window);

    if (passdata.grab && screen_info->params->box_move)
    {
        myDisplayUngrabServer (display_info);
    }
}

static XfceFilterStatus
clientResize_event_filter (XEvent * xevent, gpointer data)
{
    Client *c = NULL;
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;
    XfceFilterStatus status = XEV_FILTER_STOP;
    MoveResizeData *passdata = (MoveResizeData *) data;
    gboolean resizing = TRUE;
    XWindowChanges wc;
    int prev_y = 0, prev_x = 0;
    int prev_height = 0, prev_width = 0;
    int cx, cy, disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_x, frame_y, frame_height, frame_width;
    int frame_top, frame_left, frame_right, frame_bottom;
    GdkRectangle rect;
    gint monitor_nbr;

    TRACE ("entering clientResize_event_filter");

    c = passdata->c;
    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    
    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_height = frameHeight (c);
    frame_width = frameWidth (c);
    frame_top = frameTop (c);
    frame_left = frameLeft (c);
    frame_right = frameRight (c);
    frame_bottom = frameBottom (c);

    cx = frame_x + (frame_width / 2);
    cy = frame_y + (frame_height / 2);

    monitor_nbr = gdk_screen_get_monitor_at_point (screen_info->gscr, cx, cy);
    gdk_screen_get_monitor_geometry (screen_info->gscr, monitor_nbr, &rect);

    disp_x = rect.x;
    disp_y = rect.y;
    disp_max_x = rect.x + rect.width;
    disp_max_y = rect.y + rect.height;

    if (xevent->type == KeyPress)
    {
        if (passdata->use_keys)
        {
            int key_width_inc, key_height_inc;
            int corner = -1;

            key_width_inc = c->size->width_inc;
            key_height_inc = c->size->height_inc;

            if (key_width_inc < 10)
            {
                key_width_inc = ((int) (10 / key_width_inc)) * key_width_inc;
            }

            if (key_height_inc < 10)
            {
                key_height_inc = ((int) (10 / key_height_inc)) * key_height_inc;
            }

            if (!passdata->grab && screen_info->params->box_resize)
            {
                myDisplayGrabServer (display_info);
                passdata->grab = TRUE;
                clientDrawOutline (c);
            }
            if (screen_info->params->box_resize)
            {
                clientDrawOutline (c);
            }
            /* Store previous height in case the resize hides the window behind the curtain */
            prev_width = c->width;
            prev_height = c->height;

            if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
                && (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_UP].keycode))
            {
                c->height = c->height - key_height_inc;
                corner = 4 + SIDE_BOTTOM;
            }
            else if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
                && (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_DOWN].keycode))
            {
                c->height = c->height + key_height_inc;
                corner = 4 + SIDE_BOTTOM;
            }
            else if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_LEFT].keycode)
            {
                c->width = c->width - key_width_inc;
                corner = 4 + SIDE_RIGHT;
            }
            else if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_RIGHT].keycode)
            {
                c->width = c->width + key_width_inc;
                corner = 4 + SIDE_RIGHT;
            }
            if (corner >= 0)
            {
                clientConstrainRatio (c, c->width, c->height, corner);
            }
            if (!clientCkeckTitle (c))
            {
                c->height = prev_height;
                c->width = prev_width;
            }
            else
            {
                if ((c->x + c->width < disp_x + CLIENT_MIN_VISIBLE)
                    || (c->x + c->width < screen_info->margins [LEFT] + CLIENT_MIN_VISIBLE))
                {
                    c->width = prev_width;
                }
                if ((c->y + c->height < disp_y + CLIENT_MIN_VISIBLE)
                    || (c->y + c->height < screen_info->margins [TOP] + CLIENT_MIN_VISIBLE))
                {
                    c->height = prev_height;
                }
            }
            if (passdata->poswin)
            {
                poswinSetPosition (passdata->poswin, c);
            }
            if (screen_info->params->box_resize)
            {
                clientDrawOutline (c);
            }
            else
            {
                wc.x = c->x;
                wc.y = c->y;
                wc.width = c->width;
                wc.height = c->height;
                clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, NO_CFG_FLAG);
            }
        }
    }
    else if (xevent->type == KeyRelease)
    {
        if (passdata->use_keys)
        {
            if (IsModifierKey (XKeycodeToKeysym (display_info->dpy, xevent->xkey.keycode, 0)))
            {
                resizing = FALSE;
            }
        }
    }
    else if (xevent->type == MotionNotify)
    {
        while (XCheckMaskEvent (display_info->dpy, ButtonMotionMask | PointerMotionMask, xevent))
            ; /* Skip event */
        
        if (xevent->type == ButtonRelease)
        {
            resizing = FALSE;
            clientClearLastOpTime (c);
        }

        if (!passdata->grab && screen_info->params->box_resize)
        {
            myDisplayGrabServer (display_info);
            passdata->grab = TRUE;
            clientDrawOutline (c);
        }
        if (screen_info->params->box_resize)
        {
            clientDrawOutline (c);
        }
        passdata->oldw = c->width;
        passdata->oldh = c->height;
        /* Store previous values in case the resize puts the window title off bounds */
        prev_x = c->x;
        prev_y = c->y;
        prev_width = c->width;
        prev_height = c->height;

        if ((passdata->corner == CORNER_TOP_LEFT)
            || (passdata->corner == CORNER_BOTTOM_LEFT)
            || (passdata->corner == 4 + SIDE_LEFT))
        {
            c->width = passdata->ox - (xevent->xmotion.x_root - passdata->mx);
        }
        else if ((passdata->corner == CORNER_BOTTOM_RIGHT)
            || (passdata->corner == CORNER_TOP_RIGHT)
            || (passdata->corner == 4 + SIDE_RIGHT))
        {
            c->width = passdata->ox + (xevent->xmotion.x_root - passdata->mx);
        }
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            if ((passdata->corner == CORNER_TOP_LEFT)
                || (passdata->corner == CORNER_TOP_RIGHT))
            {
                c->height = passdata->oy - (xevent->xmotion.y_root - passdata->my);
            }
            else if ((passdata->corner == CORNER_BOTTOM_RIGHT)
                || (passdata->corner == CORNER_BOTTOM_LEFT)
                || (passdata->corner == 4 + SIDE_BOTTOM))
            {
                c->height = passdata->oy + (xevent->xmotion.y_root - passdata->my);
            }
        }
        clientConstrainRatio (c, c->width, c->height, passdata->corner);
        
        clientSetWidth (c, c->width);
        if ((passdata->corner == CORNER_TOP_LEFT)
            || (passdata->corner == CORNER_BOTTOM_LEFT)
            || (passdata->corner == 4 + SIDE_LEFT))
        {
            c->x = c->x - (c->width - passdata->oldw);
            frame_x = frameX (c);
        }
        if (((passdata->corner == CORNER_TOP_LEFT) 
             || (passdata->corner == CORNER_TOP_RIGHT)) 
            && !clientCkeckTitle (c))
        {
            c->x = prev_x;
            c->width = prev_width;
        }

        clientSetHeight (c, c->height);
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
            && (passdata->corner == CORNER_TOP_LEFT
                || passdata->corner == CORNER_TOP_RIGHT))
        {
            c->y = c->y - (c->height - passdata->oldh);
            frame_y = frameY (c);
        }
        if (((passdata->corner == CORNER_TOP_LEFT) 
             || (passdata->corner == CORNER_TOP_RIGHT)) 
            && !clientCkeckTitle (c))
        {
            c->y = prev_y;
            c->height = prev_height;
        }

        if ((passdata->corner == CORNER_TOP_LEFT)
            || (passdata->corner == CORNER_TOP_RIGHT))
        {
            if ((c->y > disp_max_y - CLIENT_MIN_VISIBLE)
                || (c->y > gdk_screen_get_height (screen_info->gscr) 
                           - screen_info->margins [BOTTOM] - CLIENT_MIN_VISIBLE))
            {
                c->y = prev_y;
                c->height = prev_height;
            }
        }
        else if ((passdata->corner == CORNER_BOTTOM_LEFT)
            || (passdata->corner == CORNER_BOTTOM_RIGHT)
            || (passdata->corner == 4 + SIDE_BOTTOM))
        {
            if ((c->y + c->height < disp_y + CLIENT_MIN_VISIBLE)
                || (c->y + c->height < screen_info->margins [TOP] + CLIENT_MIN_VISIBLE))
            {
                c->height = prev_height;
            }
        }
        if ((passdata->corner == CORNER_TOP_LEFT)
            || (passdata->corner == CORNER_BOTTOM_LEFT)
            || (passdata->corner == 4 + SIDE_LEFT))
        {
            if ((c->x > disp_max_x - CLIENT_MIN_VISIBLE)
                || (c->x > gdk_screen_get_width (screen_info->gscr) 
                           - screen_info->margins [RIGHT] - CLIENT_MIN_VISIBLE))
            {
                c->x = prev_x;
                c->width = prev_width;
            }
        }
        else if ((passdata->corner == CORNER_TOP_RIGHT)
            || (passdata->corner == CORNER_BOTTOM_RIGHT)
            || (passdata->corner == 4 + SIDE_RIGHT))
        {
            if ((c->x + c->width < disp_x + CLIENT_MIN_VISIBLE)
                || (c->x + c->width < screen_info->margins [LEFT] + CLIENT_MIN_VISIBLE))
            {
                c->width = prev_width;
            }
        }

        if (passdata->poswin)
        {
            poswinSetPosition (passdata->poswin, c);
        }
        if (screen_info->params->box_resize)
        {
            clientDrawOutline (c);
        }
        else
        {
            if (clientCheckLastOpTime (c))
            {
                wc.x = c->x;
                wc.y = c->y;
                wc.width = c->width;
                wc.height = c->height;
                clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, NO_CFG_FLAG);
            }
        }
        
    }
    else if (xevent->type == ButtonRelease)
    {
        if (!passdata->use_keys)
        {
            resizing = FALSE;
        }
    }
    else if ((xevent->type == UnmapNotify) && (xevent->xunmap.window == c->window))
    {
        resizing = FALSE;
    }
    else if ((xevent->type == EnterNotify) || (xevent->type == LeaveNotify))
    {
        /* Ignore enter/leave events */
    }
    else
    {
        status = XEV_FILTER_CONTINUE;
    }

    TRACE ("leaving clientResize_event_filter");

    if (!resizing)
    {
        TRACE ("event loop now finished");
        gtk_main_quit ();
    }

    return status;
}

void
clientResize (Client * c, int corner, XEvent * e)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;
    XWindowChanges wc;
    MoveResizeData passdata;
    int g1 = GrabSuccess, g2 = GrabSuccess;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientResize");
    TRACE ("resizing client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    passdata.c = c;
    passdata.ox = c->width;
    passdata.oy = c->height;
    passdata.corner = CORNER_BOTTOM_RIGHT;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    passdata.corner = corner;

    xfwmWindowTemp (display_info->dpy, screen_info->xroot, &passdata.tmp_event_window, 0, 0, 
                        gdk_screen_get_width (screen_info->gscr),
                        gdk_screen_get_height (screen_info->gscr), 
                        ButtonMotionMask | ButtonReleaseMask);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
    {
        clientRemoveMaximizeFlag (c);
    }

    if (e->type == KeyPress)
    {
        passdata.use_keys = TRUE;
        passdata.mx = e->xkey.x_root;
        passdata.my = e->xkey.y_root;
    }
    else if (e->type == ButtonPress)
    {
        passdata.mx = e->xbutton.x_root;
        passdata.my = e->xbutton.y_root;
    }
    else
    {
        getMouseXY (screen_info, screen_info->xroot, &passdata.mx, &passdata.my);
    }

    g1 = XGrabKeyboard (display_info->dpy, MYWINDOW_XWINDOW (passdata.tmp_event_window),
                        FALSE, GrabModeAsync, GrabModeAsync, GDK_CURRENT_TIME);
    g2 = XGrabPointer (display_info->dpy, MYWINDOW_XWINDOW (passdata.tmp_event_window),
                        FALSE, ButtonMotionMask | ButtonReleaseMask, GrabModeAsync,
                        GrabModeAsync, screen_info->xroot, myDisplayGetCursorResize(display_info,
                        passdata.corner),
                        GDK_CURRENT_TIME);

    if (((passdata.use_keys) && (g1 != GrabSuccess)) || (g2 != GrabSuccess))
    {
        TRACE ("grab failed in clientResize");
        gdk_beep ();
        if ((passdata.use_keys) && (g1 == GrabSuccess))
        {
            XUngrabKeyboard (display_info->dpy, GDK_CURRENT_TIME);
        }
        if (g2 == GrabSuccess)
        {
            XUngrabPointer (display_info->dpy, GDK_CURRENT_TIME);
        }
        xfwmWindowDelete (&passdata.tmp_event_window);
        return;
    }

#ifndef SHOW_POSITION
    if ((c->size->width_inc > 1) || (c->size->height_inc > 1))
#endif /* SHOW_POSITION */
    {
        passdata.poswin = poswinCreate(screen_info->gscr);
        poswinSetPosition (passdata.poswin, c);
        poswinShow (passdata.poswin);
    }
#ifndef SHOW_POSITION
    else
    {
        passdata.poswin = NULL;
    }
#endif /* SHOW_POSITION */
    
    FLAG_SET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);
    TRACE ("entering resize loop");
    xfce_push_event_filter (display_info->xfilter, clientResize_event_filter, &passdata);
    if (passdata.use_keys)
    {
        XPutBackEvent (display_info->dpy, e);
    }
    gtk_main ();
    xfce_pop_event_filter (display_info->xfilter);
    TRACE ("leaving resize loop");
    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);
    
    if (passdata.poswin)
    {
        poswinDestroy (passdata.poswin);
    }
    if (passdata.grab && screen_info->params->box_resize)
    {
        clientDrawOutline (c);
    }

    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    clientConfigure (c, &wc, CWX | CWY | CWHeight | CWWidth, CFG_NOTIFY);

    XUngrabKeyboard (display_info->dpy, GDK_CURRENT_TIME);
    XUngrabPointer (display_info->dpy, GDK_CURRENT_TIME);
    xfwmWindowDelete (&passdata.tmp_event_window);

    if (passdata.grab && screen_info->params->box_resize)
    {
        myDisplayUngrabServer (display_info);
    }
}

static XfceFilterStatus
clientCycle_event_filter (XEvent * xevent, gpointer data)
{
    ClientCycleData *passdata = (ClientCycleData *) data;
    Client *c = passdata->c;
    ScreenInfo *screen_info = c->screen_info;
    XfceFilterStatus status = XEV_FILTER_STOP;
    KeyCode keycode = screen_info->params->keys[KEY_CYCLE_WINDOWS].keycode;
    int modifier = screen_info->params->keys[KEY_CYCLE_WINDOWS].modifier;
    gboolean key_pressed = ((xevent->type == KeyPress) && (xevent->xkey.keycode == keycode));
    gboolean cycling = TRUE;
    gboolean gone = FALSE;
    
    TRACE ("entering clientCycle_event_filter");
    switch (xevent->type)
    {
        case DestroyNotify:
            gone |= (c == clientGetFromWindow (screen_info, ((XDestroyWindowEvent *) xevent)->window, WINDOW));
            status = XEV_FILTER_CONTINUE;
            /* Walk through */
        case UnmapNotify:
            gone |= (c == clientGetFromWindow (screen_info, ((XUnmapEvent *) xevent)->window, WINDOW));
            status = XEV_FILTER_CONTINUE;
            /* Walk through */
        case KeyPress:
            if (gone || key_pressed)
            {
                if (key_pressed)
                {
                    /* If KEY_CYCLE_WINDOWS has Shift, then do not reverse */
                    if (!(modifier & ShiftMask) && (xevent->xkey.state & ShiftMask))
                    {
                        TRACE ("Cycle: previous");
                        c = clientGetPrevious (c, passdata->cycle_range);
                        passdata->c = c;
                    }
                    else
                    {
                        TRACE ("Cycle: next");
                        c = clientGetNext (c, passdata->cycle_range);
                        passdata->c = c;
                    }
                }
                else if (gone)
                {
                    /* Jump to the next one if the current has vanished! */
                    c = clientGetNext (c, passdata->cycle_range);
                    passdata->c = c;
                }

                if (c)
                {
                    GdkPixbuf *icon;
                    
                    wireframeUpdate (c, passdata->wireframe);
                    icon = getAppIcon (clientGetXDisplay (c), c->window, 32, 32);
                    tabwinSetLabel (passdata->tabwin, icon, 
                                    c->class.res_class, c->name);
                }
                else
                {
                    cycling = FALSE;
                }
            }
            break;
        case KeyRelease:
            {
                int keysym = XKeycodeToKeysym (clientGetXDisplay (c), xevent->xkey.keycode, 0);

                /* If KEY_CYCLE_WINDOWS has Shift, then stop cycling on Shift
                 * release.
                 */
                if (IsModifierKey (keysym)
                        && ( (screen_info->params->keys[KEY_CYCLE_WINDOWS].modifier
                             & ShiftMask)
                        || (keysym != XK_Shift_L && keysym != XK_Shift_R) ) )
                {
                    cycling = FALSE;
                }
            }
            break;
        case ButtonPress:
        case ButtonRelease:
        case EnterNotify:
        case LeaveNotify:
        case MotionNotify:
            break;
        default:
            status = XEV_FILTER_CONTINUE;
            break;
    }

    if (!cycling)
    {
        TRACE ("event loop now finished");
        gtk_main_quit ();
    }

    return status;
}

void
clientCycle (Client * c, XEvent * e)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;
    ClientCycleData passdata;
    int g1, g2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientCycle");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    g1 = XGrabKeyboard (display_info->dpy, screen_info->gnome_win, FALSE, GrabModeAsync, GrabModeAsync,
        GDK_CURRENT_TIME);
    g2 = XGrabPointer (display_info->dpy, screen_info->gnome_win, FALSE, NoEventMask, GrabModeAsync,
        GrabModeAsync, screen_info->xroot, None, GDK_CURRENT_TIME);
    if ((g1 != GrabSuccess) || (g2 != GrabSuccess))
    {
        TRACE ("grab failed in clientCycle");
        gdk_beep ();
        if (g1 == GrabSuccess)
        {
            XUngrabKeyboard (display_info->dpy, GDK_CURRENT_TIME);
        }
        if (g2 == GrabSuccess)
        {
            XUngrabPointer (display_info->dpy, GDK_CURRENT_TIME);
        }
        return;
    }

    if (screen_info->params->cycle_hidden)
    {
        passdata.cycle_range = INCLUDE_HIDDEN;
    }
    else
    {
        passdata.cycle_range = 0;
    }
    if (!screen_info->params->cycle_minimum)
    {
        passdata.cycle_range |= INCLUDE_SKIP_TASKBAR | INCLUDE_SKIP_PAGER;
    }
    passdata.c = clientGetNext (c, passdata.cycle_range);

    /* If there is one single client, and if it's hidden, use it */
    if ((passdata.c == NULL) && FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
    {
        passdata.c = c;
    }
    
    if (passdata.c)
    {
        GdkPixbuf *icon;
        
        TRACE ("entering cycle loop");
        passdata.wireframe = wireframeCreate (passdata.c);
        icon = getAppIcon (display_info->dpy, passdata.c->window, 32, 32);
        passdata.tabwin = tabwinCreate (passdata.c->screen_info->gscr, icon,
                               passdata.c->class.res_class, passdata.c->name);
        xfce_push_event_filter (display_info->xfilter, clientCycle_event_filter, &passdata);
        gtk_main ();
        xfce_pop_event_filter (display_info->xfilter);
        wireframeDelete (screen_info, passdata.wireframe);
        TRACE ("leaving cycle loop");
        tabwinDestroy (passdata.tabwin);
        g_free (passdata.tabwin);
    }
    XUngrabKeyboard (display_info->dpy, GDK_CURRENT_TIME);
    XUngrabPointer (display_info->dpy, GDK_CURRENT_TIME);

    if (passdata.c)
    {
        Client *focused;
        
        focused = clientGetFocus ();
        clientShow (passdata.c, TRUE);
        clientSetFocus (passdata.c->screen_info, passdata.c, GDK_CURRENT_TIME, NO_FOCUS_FLAG);
        if (focused)
        {
            clientAdjustFullscreenLayer (focused, FALSE);
        }
        clientRaise (passdata.c);
        clientPassGrabButton1 (passdata.c);
    }
}

static XfceFilterStatus
clientButtonPress_event_filter (XEvent * xevent, gpointer data)
{
    XfceFilterStatus status = XEV_FILTER_STOP;
    gboolean pressed = TRUE;
    Client *c = ((ButtonPressData *) data)->c;
    int b = ((ButtonPressData *) data)->b;

    if (xevent->type == EnterNotify)
    {
        c->button_pressed[b] = TRUE;
        frameDraw (c, FALSE, FALSE);
    }
    else if (xevent->type == LeaveNotify)
    {
        c->button_pressed[b] = FALSE;
        frameDraw (c, FALSE, FALSE);
    }
    else if (xevent->type == ButtonRelease)
    {
        pressed = FALSE;
    }
    else if ((xevent->type == UnmapNotify)
        && (xevent->xunmap.window == c->window))
    {
        pressed = FALSE;
        c->button_pressed[b] = FALSE;
    }
    else if ((xevent->type == KeyPress) || (xevent->type == KeyRelease))
    {
    }
    else
    {
        status = XEV_FILTER_CONTINUE;
    }

    if (!pressed)
    {
        TRACE ("event loop now finished");
        gtk_main_quit ();
    }

    return status;
}

void
clientButtonPress (Client * c, Window w, XButtonEvent * bev)
{
    ScreenInfo *screen_info = NULL;
    DisplayInfo *display_info = NULL;
    ButtonPressData passdata;
    int b, g1;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientButtonPress");

    for (b = 0; b < BUTTON_COUNT; b++)
    {
        if (MYWINDOW_XWINDOW (c->buttons[b]) == w)
        {
            break;
        }
    }

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    g1 = XGrabPointer (display_info->dpy, w, FALSE,
        ButtonReleaseMask | EnterWindowMask | LeaveWindowMask, GrabModeAsync,
        GrabModeAsync, screen_info->xroot, None, GDK_CURRENT_TIME);

    if (g1 != GrabSuccess)
    {
        TRACE ("grab failed in clientButtonPress");
        gdk_beep ();
        if (g1 == GrabSuccess)
        {
            XUngrabKeyboard (display_info->dpy, GDK_CURRENT_TIME);
        }
        return;
    }

    passdata.c = c;
    passdata.b = b;

    c->button_pressed[b] = TRUE;
    frameDraw (c, FALSE, FALSE);

    TRACE ("entering button press loop");
    xfce_push_event_filter (display_info->xfilter, clientButtonPress_event_filter, &passdata);
    gtk_main ();
    xfce_pop_event_filter (display_info->xfilter);
    TRACE ("leaving button press loop");

    XUngrabPointer (display_info->dpy, GDK_CURRENT_TIME);

    if (c->button_pressed[b])
    {
        c->button_pressed[b] = FALSE;
        switch (b)
        {
            case HIDE_BUTTON:
                if (CLIENT_CAN_HIDE_WINDOW (c))
                {
                    clientHide (c, c->win_workspace, TRUE);
                }
                break;
            case CLOSE_BUTTON:
                clientClose (c);
                break;
            case MAXIMIZE_BUTTON:
                if (CLIENT_CAN_MAXIMIZE_WINDOW (c))
                {
                    unsigned long mode = c->win_state & WIN_STATE_MAXIMIZED;

                    if (bev->button == Button1)
                    {
                        clientToggleMaximized (c,
                            mode ? mode : WIN_STATE_MAXIMIZED);
                    }
                    else if (bev->button == Button2)
                    {
                        clientToggleMaximized (c,
                            mode ? mode : WIN_STATE_MAXIMIZED_VERT);
                    }
                    else if (bev->button == Button3)
                    {
                        clientToggleMaximized (c,
                            mode ? mode : WIN_STATE_MAXIMIZED_HORIZ);
                    }
                }
                break;
            case SHADE_BUTTON:
                clientToggleShaded (c);
                break;
            case STICK_BUTTON:
                clientToggleSticky (c, TRUE);
                break;
            default:
                break;
        }
        frameDraw (c, FALSE, FALSE);
    }
}

Client *
clientGetLeader (Client * c)
{
    Client *c2 = NULL;

    TRACE ("entering clientGetLeader");
    g_return_val_if_fail (c != NULL, NULL);

    if (c->group_leader != None)
    {
        c2 = clientGetFromWindow (c->screen_info, c->group_leader, WINDOW);
    }
    else if (c->client_leader != None)
    {
        c2 = clientGetFromWindow (c->screen_info, c->client_leader, WINDOW);
    }
    return c2;
}

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
char *
clientGetStartupId (Client * c)
{
    TRACE ("entering clientStartupId");
    g_return_val_if_fail (c != NULL, NULL);

    if (c->startup_id)
    {
        return c->startup_id;
    }
    else
    {
        Client *c2 = NULL;

        c2 = clientGetLeader (c);
        if (c2)
        {
            return c2->startup_id;
        }
    }
    return NULL;
}
#endif /* HAVE_LIBSTARTUP_NOTIFICATION */
