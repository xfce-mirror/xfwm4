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
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef GDK_MULTIHEAD_SAFE
#undef GDK_MULTIHEAD_SAFE
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib.h>

#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfcegui4/libxfcegui4.h>

#include "main.h"
#include "misc.h"
#include "client.h"
#include "frame.h"
#include "hints.h"
#include "workspaces.h"
#include "mypixmap.h"
#include "mywindow.h"
#include "settings.h"
#include "tabwin.h"
#include "poswin.h"
#include "session.h"
#include "netwm.h"
#include "focus.h"
#include "transients.h"
#include "stacking.h"
#include "placement.h"
#include "startup_notification.h"

/* Event mask definition */

#define POINTER_EVENT_MASK \
    ButtonPressMask|\
    ButtonReleaseMask

#define FRAME_EVENT_MASK \
    SubstructureNotifyMask|\
    SubstructureRedirectMask|\
    EnterWindowMask

#define CLIENT_EVENT_MASK \
    StructureNotifyMask|\
    FocusChangeMask|\
    PropertyChangeMask

/* Useful macros */
#define ACCEPT_INPUT(wmhints) \
    (!(params.focus_hint) || \
     ((!(wmhints) || \
       ((wmhints) && !(wmhints->flags & InputHint)) || \
       ((wmhints) && (wmhints->flags & InputHint) && (wmhints->input)))))

#define START_ICONIC(c) \
    ((c->wmhints) && \
     (c->wmhints->initial_state == IconicState) && \
     !clientIsTransientOrModal(c))

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
    Window tmp_event_window;
    Client *c;
};

typedef struct _ClientCycleData ClientCycleData;
struct _ClientCycleData
{
    Client *c;
    Tabwin *tabwin;
    int cycle_range;
};

typedef struct _ButtonPressData ButtonPressData;
struct _ButtonPressData
{
    int b;
    Client *c;
};

Client *clients = NULL;
unsigned int client_count = 0;
unsigned long client_serial = 0;

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
            XGetWindowAttributes (dpy, c->cmap_windows[i], &attr);
            XInstallColormap (dpy, attr.colormap);
            if (c->cmap_windows[i] == c->window)
            {
                installed = TRUE;
            }
        }
    }
    if ((!installed) && (c->cmap))
    {
        XInstallColormap (dpy, c->cmap);
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
    }
    if (!XGetWMColormapWindows (dpy, c->window, &c->cmap_windows, &c->ncmap))
    {
        c->ncmap = 0;
    }
    c->cmap = attr.colormap;
}

void
clientUpdateAllFrames (int mask)
{
    Client *c;
    int i;
    XWindowChanges wc;

    TRACE ("entering clientRedrawAllFrames");
    XGrabPointer (dpy, gnome_win, FALSE, EnterWindowMask, GrabModeAsync,
                       GrabModeAsync, None, None, CurrentTime);
    for (c = clients, i = 0; i < client_count; c = c->next, i++)
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
    XUngrabPointer (dpy, CurrentTime);
}

void
clientGrabKeys (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientGrabKeys");
    TRACE ("grabbing keys for client \"%s\" (0x%lx)", c->name, c->window);

    grabKey (dpy, &params.keys[KEY_MOVE_UP], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_DOWN], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_LEFT], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_RIGHT], c->window);
    grabKey (dpy, &params.keys[KEY_RESIZE_UP], c->window);
    grabKey (dpy, &params.keys[KEY_RESIZE_DOWN], c->window);
    grabKey (dpy, &params.keys[KEY_RESIZE_LEFT], c->window);
    grabKey (dpy, &params.keys[KEY_RESIZE_RIGHT], c->window);
    grabKey (dpy, &params.keys[KEY_CLOSE_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_HIDE_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_MAXIMIZE_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_MAXIMIZE_VERT], c->window);
    grabKey (dpy, &params.keys[KEY_MAXIMIZE_HORIZ], c->window);
    grabKey (dpy, &params.keys[KEY_SHADE_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_STICK_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_RAISE_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_LOWER_WINDOW], c->window);
    grabKey (dpy, &params.keys[KEY_CYCLE_WINDOWS], c->window);
    grabKey (dpy, &params.keys[KEY_NEXT_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_PREV_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_ADD_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_DEL_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_1], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_2], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_3], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_4], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_5], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_6], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_7], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_8], c->window);
    grabKey (dpy, &params.keys[KEY_WORKSPACE_9], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_NEXT_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_PREV_WORKSPACE], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_1], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_2], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_3], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_4], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_5], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_6], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_7], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_8], c->window);
    grabKey (dpy, &params.keys[KEY_MOVE_WORKSPACE_9], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_1], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_2], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_3], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_4], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_5], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_6], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_7], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_8], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_9], c->window);
    grabKey (dpy, &params.keys[KEY_SHORTCUT_10], c->window);
}

void
clientUngrabKeys (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientUngrabKeys");
    TRACE ("ungrabing keys for client \"%s\" (0x%lx)", c->name, c->window);

    ungrabKeys (dpy, c->window);
}

void
clientGrabButtons (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientGrabButtons");
    TRACE ("grabbing buttons for client \"%s\" (0x%lx)", c->name, c->window);
    
    grabButton(dpy, Button1, AltMask, c->window);
    grabButton(dpy, Button2, AltMask, c->window);
    grabButton(dpy, Button3, AltMask, c->window);
}

void
clientUngrabButtons (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientUngrabButtons");
    TRACE ("grabbing buttons for client \"%s\" (0x%lx)", c->name, c->window);
    
    XUngrabButton (dpy, AnyButton, AnyModifier, c->window);
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
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_MOVING_RESIZING))
        {
            c->x = wc->x;
        }
    }
    if (mask & CWY)
    {
        moved = TRUE;
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_MOVING_RESIZING))
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

    if ((flags & CFG_CONSTRAINED) && (mask & (CWX | CWY)) && 
         CONSTRAINED_WINDOW (c))
    {
        clientConstrainPos (c, TRUE);
    }

    XMoveResizeWindow (dpy, c->frame, frameX (c), frameY (c), 
                            frameWidth (c), frameHeight (c));
    XMoveResizeWindow (dpy, c->window, frameLeft (c), frameTop (c), 
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
        ce.display = dpy;
        ce.event = c->window;
        ce.window = c->window;
        ce.x = c->x;
        ce.y = c->y;
        ce.width = c->width;
        ce.height = c->height;
        ce.border_width = 0;
        ce.above = c->frame;
        ce.override_redirect = FALSE;
        XSendEvent (dpy, c->window, FALSE, StructureNotifyMask,
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
    
    mwm_hints = getMotifHints (dpy, c->window);
    if (mwm_hints)
    {
        if (mwm_hints->flags & MWM_HINTS_DECORATIONS)
        {
            if (mwm_hints->decorations & MWM_DECOR_ALL)
            {
                FLAG_SET (c->flags,
                    CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_MENU);
            }
            else
            {
                FLAG_UNSET (c->flags,
                    CLIENT_FLAG_HAS_BORDER | CLIENT_FLAG_HAS_MENU);
                FLAG_SET (c->flags,
                    (mwm_hints->
                        decorations & (MWM_DECOR_TITLE | MWM_DECOR_BORDER)) ?
                    CLIENT_FLAG_HAS_BORDER : 0);
                FLAG_SET (c->flags,
                    (mwm_hints->
                        decorations & (MWM_DECOR_MENU)) ? CLIENT_FLAG_HAS_MENU
                    : 0);
                /*
                   FLAG_UNSET(c->flags, CLIENT_FLAG_HAS_HIDE);
                   FLAG_UNSET(c->flags, CLIENT_FLAG_HAS_MAXIMIZE);
                   FLAG_SET(c->flags, (mwm_hints->decorations & (MWM_DECOR_MINIMIZE)) ? CLIENT_FLAG_HAS_HIDE : 0);
                   FLAG_SET(c->flags, (mwm_hints->decorations & (MWM_DECOR_MAXIMIZE)) ? CLIENT_FLAG_HAS_MAXIMIZE : 0);
                 */
            }
        }
        /* The following is from Metacity : */
        if (mwm_hints->flags & MWM_HINTS_FUNCTIONS)
        {
            if (!(mwm_hints->functions & MWM_FUNC_ALL))
            {
                FLAG_UNSET (c->flags,
                    CLIENT_FLAG_HAS_CLOSE | CLIENT_FLAG_HAS_HIDE |
                    CLIENT_FLAG_HAS_MAXIMIZE | CLIENT_FLAG_HAS_MOVE |
                    CLIENT_FLAG_HAS_RESIZE);
            }
            else
            {
                FLAG_SET (c->flags,
                    CLIENT_FLAG_HAS_CLOSE | CLIENT_FLAG_HAS_HIDE |
                    CLIENT_FLAG_HAS_MAXIMIZE | CLIENT_FLAG_HAS_MOVE |
                    CLIENT_FLAG_HAS_RESIZE);
            }

            if (mwm_hints->functions & MWM_FUNC_CLOSE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_HAS_CLOSE);
            }
            if (mwm_hints->functions & MWM_FUNC_MINIMIZE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_HAS_HIDE);
            }
            if (mwm_hints->functions & MWM_FUNC_MAXIMIZE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_HAS_MAXIMIZE);
            }
            if (mwm_hints->functions & MWM_FUNC_RESIZE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_HAS_RESIZE);
            }
            if (mwm_hints->functions & MWM_FUNC_MOVE)
            {
                FLAG_TOGGLE (c->flags, CLIENT_FLAG_HAS_MOVE);
            }
        }
        g_free (mwm_hints);
    }
    
    if (update)
    {
        /* EWMH window type takes precedences over Motif hints */ 
        clientWindowType (c);
        if (FLAG_TEST_AND_NOT(c->flags, CLIENT_FLAG_HAS_BORDER, CLIENT_FLAG_FULLSCREEN) && 
           (c->legacy_fullscreen))
        {
            /* legacy app changed its decoration, put it back on regular layer */
            c->legacy_fullscreen = FALSE;
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
    if (!XGetWMNormalHints (dpy, c->window, c->size, &dummy))
    {
        c->size->flags = 0;
    }
    
    previous_value = FLAG_TEST (c->flags, CLIENT_FLAG_IS_RESIZABLE);
    FLAG_UNSET (c->flags, CLIENT_FLAG_IS_RESIZABLE);

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
        FLAG_SET (c->flags, CLIENT_FLAG_IS_RESIZABLE);
    }
    
    if (update)
    {
        if ((c->width != wc.width) || (c->height != wc.height))
        {
            clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_CONSTRAINED);
        }
        else if (FLAG_TEST (c->flags, CLIENT_FLAG_IS_RESIZABLE) != previous_value)
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
    wm_protocols_flags = getWMProtocols (dpy, c->window);
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
    if (c->ncmap > 0)
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
        if (!clientIsTransientOrModal (c))
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
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
        }
    }
    if (c->win_state & WIN_STATE_MAXIMIZED_VERT)
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
        {
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
        }
    }
    if (c->win_state & WIN_STATE_MAXIMIZED)
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
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
    if (FLAG_TEST (c->flags,
            CLIENT_FLAG_MAXIMIZED_HORIZ | CLIENT_FLAG_MAXIMIZED_VERT))
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
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
        if (!clientIsTransientOrModal (c))
        {
            TRACE ("Applying client's initial state: fullscreen");
            clientUpdateFullscreenState (c);
        }
    }
    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_ABOVE, CLIENT_FLAG_BELOW))
    {
        if (!clientIsTransientOrModal (c))
        {
            TRACE ("Applying client's initial state: above");
            clientUpdateAboveState (c);
        }
    }
    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_BELOW, CLIENT_FLAG_ABOVE))
    {
        if (!clientIsTransientOrModal (c))
        {
            TRACE ("Applying client's initial state: below");
            clientUpdateBelowState (c);
        }
    }
    if (FLAG_TEST_ALL (c->flags, CLIENT_FLAG_STICKY | CLIENT_FLAG_HAS_STICK))
    {
        if (!clientIsTransientOrModal (c))
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
        && FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STICK))
    {
        TRACE ("client \"%s\" (0x%lx) has received a win_state/stick event",
            c->name, c->window);
        if (!clientIsTransientOrModal (c))
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
        && FLAG_TEST (c->flags, CLIENT_FLAG_HAS_MAXIMIZE))
    {
        TRACE
            ("client \"%s\" (0x%lx) has received a win_state/maximize event",
            c->name, c->window);
        clientToggleMaximized (c, add_remove);
    }
}

static gboolean
clientCheckShape (Client * c)
{
    int xws, yws, xbs, ybs;
    unsigned wws, hws, wbs, hbs;
    int boundingShaped, clipShaped;
    
    g_return_val_if_fail (c != NULL, FALSE);
    if (shape)
    {
        XShapeQueryExtents (dpy, c->window, &boundingShaped, &xws, &yws, &wws, 
                            &hws, &clipShaped, &xbs, &ybs, &wbs, &hbs);
        return (boundingShaped != 0);
    }
    return FALSE;
}

void
clientClearPixmapCache (Client * c)
{
    g_return_if_fail (c != NULL);

    myPixmapFree (dpy, &c->pm_cache.pm_title[ACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_title[INACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_LEFT][ACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_LEFT][INACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_RIGHT][ACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_RIGHT][INACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_BOTTOM][ACTIVE]);
    myPixmapFree (dpy, &c->pm_cache.pm_sides[SIDE_BOTTOM][INACTIVE]);
}

void
clientGetUserTime (Client * c)
{
    g_return_if_fail (c != NULL);
    
    if (getNetWMUserTime (dpy, c->window, &c->user_time))
    {
        FLAG_SET (c->flags, CLIENT_FLAG_HAS_USER_TIME);
    }
}

void
clientFrame (Window w, gboolean recapture)
{
    XWindowAttributes attr;
    XWindowChanges wc;
    XSetWindowAttributes attributes;
    Client *c;
    unsigned long valuemask;
    int i;

    g_return_if_fail (w != None);
    TRACE ("entering clientFrame");
    TRACE ("framing client (0x%lx)", w);

    if (w == gnome_win)
    {
        TRACE ("Not managing our own window");
        return;
    }

    gdk_error_trap_push ();
    if (checkKdeSystrayWindow (dpy, w) && (systray != None))
    {
        TRACE ("Not managing KDE systray windows");
        sendSystrayReqDock (dpy, w, systray);
        gdk_error_trap_pop ();
        return;
    }

    if (!XGetWindowAttributes (dpy, w, &attr))
    {
        TRACE ("Cannot get window attributes");
        gdk_error_trap_pop ();
        return;
    }

    if (attr.override_redirect)
    {
        TRACE ("Not managing override_redirect windows");
        gdk_error_trap_pop ();
        return;
    }

    c = g_new(Client, 1);
    if (!c)
    {
        TRACE ("Cannot allocate memory for the window structure");
        gdk_error_trap_pop ();
        return;
    }
    
    c->window = w;
    c->serial = client_serial++;

    getWindowName (dpy, c->window, &c->name);
    TRACE ("name \"%s\"", c->name);
    getTransientFor (dpy, screen, c->window, &c->transient_for);

    /* Initialize structure */
    c->size = NULL;
    c->flags = CLIENT_FLAG_INITIAL_VALUES;
    c->wm_flags = 0L;
    c->x = attr.x;
    c->y = attr.y;
    c->width = attr.width;
    c->height = attr.height;
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
    
    if (clientCheckShape(c))
    {
        FLAG_UNSET (c->flags, CLIENT_FLAG_HAS_BORDER);
    }

    if (((c->size->flags & (PMinSize | PMaxSize)) != (PMinSize | PMaxSize))
        || (((c->size->flags & (PMinSize | PMaxSize)) ==
                (PMinSize | PMaxSize))
            && ((c->size->min_width < c->size->max_width)
                || (c->size->min_height < c->size->max_height))))
    {
        FLAG_SET (c->flags, CLIENT_FLAG_IS_RESIZABLE);
    }

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        c->button_pressed[i] = FALSE;
    }

    if (!XGetWMColormapWindows (dpy, c->window, &c->cmap_windows, &c->ncmap))
    {
        c->ncmap = 0;
    }

    c->class.res_name = NULL;
    c->class.res_class = NULL;
    XGetClassHint (dpy, w, &c->class);
    c->wmhints = XGetWMHints (dpy, c->window);
    c->group_leader = None;
    if (c->wmhints)
    {
        if (c->wmhints->flags & WindowGroupHint)
        {
            c->group_leader = c->wmhints->window_group;
        }
    }
    c->client_leader = getClientLeader (dpy, c->window);
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    c->startup_id = NULL;
    getWindowStartupId (dpy, c->window, &c->startup_id);
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
        /* XReparentWindow() will cause an UnmapNotify followed by 
         * a MapNotify. 
         * Unset the "mapped" flag to avoid a transition to 
         * withdrawn state.
         */ 
        XUnmapWindow (dpy, c->window);
        FLAG_SET (c->flags, CLIENT_FLAG_MAP_PENDING);
    }

    c->ignore_unmap = 0;
    c->type = UNSET;
    c->type_atom = None;

    FLAG_SET (c->flags, START_ICONIC (c) ? CLIENT_FLAG_HIDDEN : 0);
    FLAG_SET (c->wm_flags, ACCEPT_INPUT (c->wmhints) ? WM_FLAG_INPUT : 0);

    clientGetWMProtocols (c);
    clientGetMWMHints (c, FALSE);
    getHint (dpy, w, win_hints, &c->win_hints);
    getHint (dpy, w, win_state, &c->win_state);
    if (!getHint (dpy, w, win_layer, &c->win_layer))
    {
        c->win_layer = WIN_LAYER_NORMAL;
    }

    /* Reload from session */
    if (sessionMatchWinToSM (c))
    {
        FLAG_SET (c->flags, CLIENT_FLAG_SESSION_MANAGED);
    }

    /* Beware, order of calls is important here ! */
    sn_client_startup_properties (c);
    clientGetWinState (c);
    clientGetNetState (c);
    clientGetNetWmType (c);
    clientGetInitialNetWmDesktop (c);
    clientGetNetStruts (c);

    c->legacy_fullscreen = FALSE;
    /* Fullscreen for older legacy apps */
    if ((c->x == 0) && (c->y == 0) &&
        (c->width == MyDisplayFullWidth (dpy, screen)) &&
        (c->height == MyDisplayFullHeight (dpy, screen)) &&
        !FLAG_TEST(c->flags, CLIENT_FLAG_HAS_BORDER) &&
        (c->win_layer == WIN_LAYER_NORMAL) &&
        (c->type == WINDOW_NORMAL))
    {
        c->legacy_fullscreen = TRUE;
    }

    /* Once we know the type of window, we can initialize window position */
    if (!FLAG_TEST (c->flags, CLIENT_FLAG_SESSION_MANAGED))
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

    /* We must call clientApplyInitialState() after having placed the
       window so that the inital position values are correctly set if the
       inital state is maximize or fullscreen
     */
    clientApplyInitialState (c);

    if (!recapture)
    {
        myXGrabServer ();
    }
    if (!myCheckWindow(w))
    {
        TRACE ("Client has vanished");
        clientFree(c);
        if (!recapture)
        {
            myXUngrabServer ();
        }
        gdk_error_trap_pop ();
        return;
    }

    valuemask = CWEventMask;
    attributes.event_mask = (FRAME_EVENT_MASK | POINTER_EVENT_MASK);
    c->frame =
        XCreateWindow (dpy, root, frameX (c), frameY (c), frameWidth (c),
        frameHeight (c), 0, CopyFromParent, InputOutput, CopyFromParent,
        valuemask, &attributes);
    XSetWindowBorderWidth (dpy, c->window, 0);
    XReparentWindow (dpy, c->window, c->frame, frameLeft (c), frameTop (c));
    
    valuemask = CWEventMask;
    attributes.event_mask = (CLIENT_EVENT_MASK);
    XChangeWindowAttributes (dpy, c->window, valuemask, &attributes);
    if (shape)
    {
        XShapeSelectInput (dpy, c->window, ShapeNotifyMask);
    }
    if (!recapture)
    {
        /* Window is reparented now, so we can safely release the grab 
         * on the server 
         */
        myXUngrabServer ();
    }   

    clientAddToList (c);
    clientSetNetActions (c);
    clientGrabKeys (c);
    clientGrabButtons(c);

    /* Initialize pixmap caching */
    myPixmapInit (&c->pm_cache.pm_title[ACTIVE]);
    myPixmapInit (&c->pm_cache.pm_title[INACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_LEFT][ACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_LEFT][INACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_RIGHT][ACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_RIGHT][INACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_BOTTOM][ACTIVE]);
    myPixmapInit (&c->pm_cache.pm_sides[SIDE_BOTTOM][INACTIVE]);
    c->pm_cache.previous_width = -1;
    c->pm_cache.previous_height = -1;

    myWindowCreate (dpy, c->frame, &c->sides[SIDE_LEFT],
        resize_cursor[4 + SIDE_LEFT]);
    myWindowCreate (dpy, c->frame, &c->sides[SIDE_RIGHT],
        resize_cursor[4 + SIDE_RIGHT]);
    myWindowCreate (dpy, c->frame, &c->sides[SIDE_BOTTOM],
        resize_cursor[4 + SIDE_BOTTOM]);
    myWindowCreate (dpy, c->frame, &c->corners[CORNER_BOTTOM_LEFT],
        resize_cursor[CORNER_BOTTOM_LEFT]);
    myWindowCreate (dpy, c->frame, &c->corners[CORNER_BOTTOM_RIGHT],
        resize_cursor[CORNER_BOTTOM_RIGHT]);
    myWindowCreate (dpy, c->frame, &c->corners[CORNER_TOP_LEFT],
        resize_cursor[CORNER_TOP_LEFT]);
    myWindowCreate (dpy, c->frame, &c->corners[CORNER_TOP_RIGHT],
        resize_cursor[CORNER_TOP_RIGHT]);
    myWindowCreate (dpy, c->frame, &c->title, None);
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        myWindowCreate (dpy, c->frame, &c->buttons[i], None);
    }
    TRACE ("now calling configure for the new window \"%s\" (0x%lx)", c->name,
        c->window);
    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    clientConfigure (c, &wc, CWX | CWY | CWHeight | CWWidth, CFG_NOTIFY | CFG_FORCE_REDRAW);
    clientApplyStackList ();
    clientSetLastRaise (c);

    /* Clear time counter */
    clientClearLastOpTime (c);
    /* net_wm_user_time standard */
    clientGetUserTime (c);

    /* First map is used to bypass the caching system at first map */
    c->first_map = TRUE;
    if (!FLAG_TEST (c->flags, CLIENT_FLAG_HIDDEN))
    {
        if ((c->win_workspace == workspace) || 
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
        setWMState (dpy, c->window, IconicState);
        clientSetNetState (c);
    }
    gdk_error_trap_pop ();

    DBG ("client \"%s\" (0x%lx) is now managed", c->name, c->window);
    DBG ("client_count=%d", client_count);
}

void
clientUnframe (Client * c, gboolean remap)
{
    int i;
    XEvent ev;
    gboolean reparented;

    TRACE ("entering clientUnframe");
    TRACE ("unframing client \"%s\" (0x%lx) [%s]", 
            c->name, c->window, remap ? "remap" : "no remap");

    g_return_if_fail (c != NULL);
    if (clientGetFocus () == c)
    {
        clientClearFocus ();
    }
    if (clientGetLastRaise () == c)
    {
        clientClearLastRaise ();
    }
    if (clientGetLastGrab () == c)
    {
        clientClearLastGrab ();
    }

    clientRemoveFromList (c);
    myXGrabServer ();
    gdk_error_trap_push ();
    clientUngrabKeys (c);
    clientGrabButtons (c);
    XUnmapWindow (dpy, c->window);
    XUnmapWindow (dpy, c->frame);
    clientGravitate (c, REMOVE);
    XSelectInput (dpy, c->window, NoEventMask);
    reparented = XCheckTypedWindowEvent (dpy, c->window, ReparentNotify, &ev);

    if (remap || !reparented)
    {
        XReparentWindow (dpy, c->window, root, c->x, c->y);
        XSetWindowBorderWidth (dpy, c->window, c->border_width);
        if (remap)
        {
            XMapWindow (dpy, c->window);
        }
        else
        {
            setWMState (dpy, c->window, WithdrawnState);
        }
    }

    if (!remap)
    {
        XDeleteProperty (dpy, c->window, net_wm_state);
        XDeleteProperty (dpy, c->window, win_state);
        XDeleteProperty (dpy, c->window, net_wm_desktop);
        XDeleteProperty (dpy, c->window, win_workspace);
        XDeleteProperty (dpy, c->window, net_wm_allowed_actions);
    }
    
    myWindowDelete (&c->title);
    myWindowDelete (&c->sides[SIDE_LEFT]);
    myWindowDelete (&c->sides[SIDE_RIGHT]);
    myWindowDelete (&c->sides[SIDE_BOTTOM]);
    myWindowDelete (&c->sides[CORNER_BOTTOM_LEFT]);
    myWindowDelete (&c->sides[CORNER_BOTTOM_RIGHT]);
    myWindowDelete (&c->sides[CORNER_TOP_LEFT]);
    myWindowDelete (&c->sides[CORNER_TOP_RIGHT]);
    clientClearPixmapCache (c);
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        myWindowDelete (&c->buttons[i]);
    }
    XDestroyWindow (dpy, c->frame);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUT))
    {
        workspaceUpdateArea (margins, gnome_margins);
    }
    
    myXUngrabServer ();
    gdk_error_trap_pop ();
    clientFree (c);
}

void
clientFrameAll ()
{
    unsigned int count, i;
    Window shield, w1, w2, *wins = NULL;
    XWindowAttributes attr;

    TRACE ("entering clientFrameAll");

    /* Since this fn is called at startup, it's safe to initialize some vars here */
    client_count = 0;
    clients = NULL;

    clientSetFocus (NULL, NO_FOCUS_FLAG);
    shield =
        setTmpEventWin (0, 0, 
                        MyDisplayFullWidth (dpy, screen),
                        MyDisplayFullHeight (dpy, screen), 
                        EnterWindowMask);

    XSync (dpy, FALSE);
    myXGrabServer ();
    XQueryTree (dpy, root, &w1, &w2, &wins, &count);
    for (i = 0; i < count; i++)
    {
        XGetWindowAttributes (dpy, wins[i], &attr);
        if ((!(attr.override_redirect)) && (attr.map_state == IsViewable))
        {
            clientFrame (wins[i], TRUE);
        }
    }
    if (wins)
    {
        XFree (wins);
    }
    clientFocusTop (WIN_LAYER_NORMAL);
    removeTmpEventWin (shield);
    myXUngrabServer ();
    XSync (dpy, FALSE);
}

void
clientUnframeAll ()
{
    Client *c;
    unsigned int count, i;
    Window w1, w2, *wins = NULL;

    TRACE ("entering clientUnframeAll");

    clientSetFocus (NULL, FOCUS_IGNORE_MODAL);
    XSync (dpy, FALSE);
    myXGrabServer ();
    XQueryTree (dpy, root, &w1, &w2, &wins, &count);
    for (i = 0; i < count; i++)
    {
        c = clientGetFromWindow (wins[i], FRAME);
        if (c)
        {
            clientUnframe (c, TRUE);
        }
    }
    myXUngrabServer ();
    XSync(dpy, FALSE);
    if (wins)
    {
        XFree (wins);
    }
}

Client *
clientGetFromWindow (Window w, int mode)
{
    Client *c;
    int i;

    g_return_val_if_fail (w != None, NULL);
    TRACE ("entering clientGetFromWindow");
    TRACE ("looking for (0x%lx)", w);

    for (c = clients, i = 0; i < client_count; c = c->next, i++)
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

    if (ws > params.workspace_count - 1)
    {
        ws = params.workspace_count - 1;
        TRACE ("value off limits, using %i instead", ws);
    }

    if (c->win_workspace != ws)
    {
        TRACE ("setting client \"%s\" (0x%lx) to workspace %d", c->name,
            c->window, ws);
        c->win_workspace = ws;
        setHint (dpy, c->window, win_workspace, ws);
        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
        {
            setHint (dpy, c->window, net_wm_desktop, (unsigned long) ALL_WORKSPACES);
        }
        else
        {
            setHint (dpy, c->window, net_wm_desktop, (unsigned long) ws);
        }
    }
    FLAG_SET (c->flags, CLIENT_FLAG_WORKSPACE_SET);
}

void
clientSetWorkspace (Client * c, int ws, gboolean manage_mapping)
{
    GList *list_of_windows = NULL;
    GList *index;
    Client *c2;

    g_return_if_fail (c != NULL);

    TRACE ("entering clientSetWorkspace");

    list_of_windows = clientListTransientOrModal (c);
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if (c2->win_workspace != ws)
        {
            TRACE ("setting client \"%s\" (0x%lx) to workspace %d", c->name,
                c->window, ws);
            clientSetWorkspaceSingle (c2, ws);
            if (manage_mapping && !clientIsTransientOrModal (c2)
                && !FLAG_TEST (c2->flags, CLIENT_FLAG_HIDDEN))
            {
                if (FLAG_TEST (c2->flags, CLIENT_FLAG_STICKY))
                {
                    clientShow (c2, FALSE);
                }
                else
                {
                    if (ws == workspace)
                    {
                        clientShow (c2, FALSE);
                    }
                    else
                    {
                        clientHide (c2, workspace, FALSE);
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
    g_return_if_fail (c != NULL);
    myXGrabServer ();
    if ((c->win_workspace == workspace)
        || FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        TRACE ("showing client \"%s\" (0x%lx)", c->name, c->window);
        FLAG_SET (c->flags, CLIENT_FLAG_VISIBLE);
        XMapWindow (dpy, c->frame);
        XMapWindow (dpy, c->window);
    }
    if (change_state)
    {
        FLAG_UNSET (c->flags, CLIENT_FLAG_HIDDEN);
        setWMState (dpy, c->window, NormalState);
        workspaceUpdateArea (margins, gnome_margins);
    }
    myXUngrabServer ();
    clientSetNetState (c);
}

void
clientShow (Client * c, gboolean change_state)
{
    GList *list_of_windows = NULL;
    GList *index;
    Client *c2;

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
        if (!FLAG_TEST (c2->flags, CLIENT_FLAG_MANAGED))
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
    g_return_if_fail (c != NULL);
    myXGrabServer ();
    TRACE ("hiding client \"%s\" (0x%lx)", c->name, c->window);
    clientPassFocus(c);
    XUnmapWindow (dpy, c->window);
    XUnmapWindow (dpy, c->frame);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_VISIBLE))
    {
        FLAG_UNSET (c->flags, CLIENT_FLAG_VISIBLE);
        c->ignore_unmap++;
    }
    if (change_state)
    {
        FLAG_SET (c->flags, CLIENT_FLAG_HIDDEN);
        setWMState (dpy, c->window, IconicState);
        workspaceUpdateArea (margins, gnome_margins);
    }
    myXUngrabServer ();
    clientSetNetState (c);
}

void
clientHide (Client * c, int ws, gboolean change_state)
{
    GList *list_of_windows = NULL;
    GList *index;
    Client *c2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientHide");

    list_of_windows = clientListTransientOrModal (c);
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;

        /* Ignore request before if the window is not yet managed */
        if (!FLAG_TEST (c2->flags, CLIENT_FLAG_MANAGED))
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
    Client *c2;
    int i;

    TRACE ("entering clientHideAll");

    for (c2 = c->next, i = 0; (c2) && (i < client_count); c2 = c2->next, i++)
    {
        if (CLIENT_CAN_HIDE_WINDOW (c2)
            && FLAG_TEST (c2->flags, CLIENT_FLAG_HAS_BORDER)
            && !clientIsTransientOrModal (c2) && (c2 != c))
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
clientToggleShowDesktop (gboolean show_desktop)
{
    GList *index;

    TRACE ("entering clientToggleShowDesktop");

    clientSetFocus (NULL, FOCUS_IGNORE_MODAL);
    if (show_desktop)
    {
        for (index = windows_stack; index; index = g_list_next (index))
	{
            Client *c = (Client *) index->data;
            if (CLIENT_CAN_HIDE_WINDOW (c)
        	&& FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER, CLIENT_FLAG_HIDDEN))
            {
        	{
		    FLAG_SET (c->flags, CLIENT_FLAG_WAS_SHOWN);
                    clientHide (c, c->win_workspace, TRUE);
        	}
            }
	}
        clientFocusTop (WIN_LAYER_DESKTOP);
    }
    else
    {
        for (index = g_list_last(windows_stack); index; index = g_list_previous (index))
	{
            Client *c = (Client *) index->data;
            if (FLAG_TEST (c->flags, CLIENT_FLAG_WAS_SHOWN))
            {
        	{
                    clientShow (c, TRUE);
        	}
            }
	    FLAG_UNSET (c->flags, CLIENT_FLAG_WAS_SHOWN);
	}
        clientFocusTop (WIN_LAYER_NORMAL);
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
        sendClientMessage (c->window, wm_protocols, wm_delete_window, CurrentTime);
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

    XKillClient (dpy, c->window);
}

void
clientEnterContextMenuState (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientEnterContextMenuState");
    TRACE ("Showing the what's this help for client \"%s\" (0x%lx)", c->name, c->window);

    if (FLAG_TEST (c->wm_flags, WM_FLAG_CONTEXT_HELP))
    {
        sendClientMessage (c->window, wm_protocols, kde_net_wm_context_help, CurrentTime);
    }
}

void
clientSetLayer (Client * c, int l)
{
    GList *list_of_windows = NULL;
    GList *index;
    Client *c2;

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
            setHint (dpy, c2->window, win_layer, l);
        }
    }
    g_list_free (list_of_windows);
    if (clientGetLastRaise () == c)
    {
        clientClearLastRaise ();
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

    if (!FLAG_TEST (c->flags, CLIENT_FLAG_HAS_BORDER)
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
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
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
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
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
    GList *index;
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
            setHint (dpy, c2->window, net_wm_desktop,
                (unsigned long) ALL_WORKSPACES);
            clientSetNetState (c2);
        }
        clientSetWorkspace (c, workspace, TRUE);
        g_list_free (list_of_windows);
    }
    else
    {
        TRACE ("sticking client \"%s\" (0x%lx)", c->name, c->window);
        c->win_state |= WIN_STATE_STICKY;
        FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
        setHint (dpy, c->window, net_wm_desktop,
            (unsigned long) ALL_WORKSPACES);
        clientSetNetState (c);
        clientSetWorkspace (c, workspace, TRUE);
    }
}

void
clientUnstick (Client * c, gboolean include_transients)
{
    GList *list_of_windows = NULL;
    GList *index;
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
            setHint (dpy, c2->window, net_wm_desktop,
                (unsigned long) workspace);
            clientSetNetState (c2);
        }
        clientSetWorkspace (c, workspace, TRUE);
        g_list_free (list_of_windows);
    }
    else
    {
        c->win_state &= ~WIN_STATE_STICKY;
        FLAG_UNSET (c->flags, CLIENT_FLAG_STICKY);
        setHint (dpy, c->window, net_wm_desktop,
            (unsigned long) workspace);
        clientSetNetState (c);
        clientSetWorkspace (c, workspace, TRUE);
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

    if (!clientIsTransientOrModal (c))
    {
        FLAG_TOGGLE (c->flags, CLIENT_FLAG_FULLSCREEN);
        clientUpdateAboveState (c);
    }
}

void clientToggleAbove (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleAbove");
    TRACE ("toggle above client \"%s\" (0x%lx)", c->name, c->window);

    if (!clientIsTransientOrModal (c) && !FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
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

    if (!clientIsTransientOrModal (c) && !FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
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
    XWindowChanges wc;
    int cx, cy, left, right, top, bottom;
    int full_x, full_y, full_w, full_h;
    
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleMaximized");
    TRACE ("maximzing/unmaximizing client \"%s\" (0x%lx)", c->name,
        c->window);

    if (!CLIENT_CAN_MAXIMIZE_WINDOW (c))
    {
        return;
    }

    cx = frameX (c) + (frameWidth (c) / 2);
    cy = frameY (c) + (frameHeight (c) / 2);

    left   = (isLeftMostHead (dpy, screen, cx, cy) ? params.xfwm_margins[LEFT] : 0);
    right  = (isRightMostHead (dpy, screen, cx, cy) ? params.xfwm_margins[RIGHT] : 0);
    top    = (isTopMostHead (dpy, screen, cx, cy) ? params.xfwm_margins[TOP] : 0);
    bottom = (isBottomMostHead (dpy, screen, cx, cy) ? params.xfwm_margins[BOTTOM] : 0);

    full_x = MyDisplayX (cx, cy) + left;
    full_y = MyDisplayY (cx, cy) + top;
    full_w = MyDisplayWidth (dpy, screen, cx, cy) - left - right;
    full_h = MyDisplayHeight (dpy, screen, cx, cy) - top - bottom;
    clientMaxSpace (&full_x, &full_y, &full_w, &full_h);

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
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MANAGED))
    {
        /* 
           For some reason, the configure can generate EnterNotify events
           on lower windows, causing a nasty race cond with apps trying to
           grab focus in focus follow mouse mode. Grab the pointer to
           avoid these effects
         */
        XGrabPointer (dpy, gnome_win, FALSE, EnterWindowMask, GrabModeAsync,
                           GrabModeAsync, None, None, CurrentTime);
        clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_NOTIFY);
        XUngrabPointer (dpy, CurrentTime);
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
clientScreenResize(void)
{
    Client *c = NULL;
    GList *index, *list_of_windows;
    XWindowChanges wc;
    
    list_of_windows = clientGetStackList ();

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

    XDrawRectangle (dpy, root, params.box_gc, frameX (c), frameY (c),
        frameWidth (c) - 1, frameHeight (c) - 1);
    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_HAS_BORDER,
            CLIENT_FLAG_FULLSCREEN | CLIENT_FLAG_SHADED))
    {
        XDrawRectangle (dpy, root, params.box_gc, c->x, c->y, c->width - 1,
            c->height - 1);
    }
}

static void
clientSnapPosition (Client * c)
{
    Client *c2;
    int cx, cy, i, delta;
    int disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_x, frame_y, frame_height, frame_width;
    int frame_top, frame_left, frame_right, frame_bottom;
    int frame_x2, frame_y2;
    int best_frame_x, best_frame_y;
    int best_delta_x = params.snap_width + 1;
    int best_delta_y = params.snap_width + 1;
    int c_frame_x1, c_frame_x2, c_frame_y1, c_frame_y2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSnapPosition");
    TRACE ("Snapping client \"%s\" (0x%lx)", c->name, c->window);

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

    disp_x = MyDisplayX (cx, cy);
    disp_y = MyDisplayY (cx, cy);
    disp_max_x = MyDisplayMaxX (dpy, screen, cx, cy);
    disp_max_y = MyDisplayMaxY (dpy, screen, cx, cy);

    if (params.snap_to_border)
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

    for (c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
    {
        if (FLAG_TEST (c2->flags, CLIENT_FLAG_VISIBLE)  && (c2 != c) && 
	    (((params.snap_to_windows) && (c2->win_layer == c->win_layer))
	     || ((params.snap_to_border) && FLAG_TEST_ALL (c2->flags, CLIENT_FLAG_HAS_STRUT | CLIENT_FLAG_VISIBLE))))
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

    if (best_delta_x <= params.snap_width)
    {
        c->x = best_frame_x + frame_left;
    }
    if (best_delta_y <= params.snap_width)
    {
        c->y = best_frame_y + frame_top;
    }
}

static GtkToXEventFilterStatus
clientMove_event_filter (XEvent * xevent, gpointer data)
{
    static int edge_scroll_x = 0;
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    MoveResizeData *passdata = (MoveResizeData *) data;
    Client *c = passdata->c;
    gboolean moving = TRUE;
    XWindowChanges wc;

    TRACE ("entering clientMove_event_filter");

    if (xevent->type == KeyPress)
    {
        if (passdata->use_keys)
        {
            if (!passdata->grab && params.box_move)
            {
                myXGrabServer ();
                passdata->grab = TRUE;
                clientDrawOutline (c);
            }
            if (params.box_move)
            {
                clientDrawOutline (c);
            }
            if (xevent->xkey.keycode == params.keys[KEY_MOVE_LEFT].keycode)
            {
                c->x = c->x - 16;
            }
            else if (xevent->xkey.keycode == params.keys[KEY_MOVE_RIGHT].keycode)
            {
                c->x = c->x + 16;
            }
            else if (xevent->xkey.keycode == params.keys[KEY_MOVE_UP].keycode)
            {
                c->y = c->y - 16;
            }
            else if (xevent->xkey.keycode == params.keys[KEY_MOVE_DOWN].keycode)
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
            if (params.box_move)
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
            if (IsModifierKey (XKeycodeToKeysym (dpy, xevent->xkey.keycode, 0)))
            {
                moving = FALSE;
            }
        }
    }
    else if (xevent->type == MotionNotify)
    {
        while (XCheckMaskEvent (dpy, ButtonMotionMask | PointerMotionMask, xevent))
            ; /* Skip event */
	            
        if (xevent->type == ButtonRelease)
        {
            moving = FALSE;
        }

        if (!passdata->grab && params.box_move)
        {
            myXGrabServer ();
            passdata->grab = TRUE;
            clientDrawOutline (c);
        }
        if (params.box_move)
        {
            clientDrawOutline (c);
        }

        if ((params.workspace_count > 1) && !clientIsTransientOrModal (c))
        {
            if (params.workspace_count && params.wrap_windows
                && params.wrap_resistance)
            {
                int msx, msy, max;

                msx = xevent->xmotion.x_root;
                msy = xevent->xmotion.y_root;
                max = MyDisplayFullWidth (dpy, screen) - 1;

                if ((msx == 0) || (msx == max))
                {
                    edge_scroll_x++;
                }
                else
                {
                    edge_scroll_x = 0;
                }
                if (edge_scroll_x > params.wrap_resistance)
                {
                    edge_scroll_x = 0;
                    if (msx == 0)
                    {
                        XWarpPointer (dpy, None, root, 0, 0, 0, 0, max - 10,
                            msy);
                        msx = xevent->xmotion.x_root = max - 10;
                        workspaceSwitch (workspace - 1, c);
                    }
                    else if (msx == max)
                    {
                        XWarpPointer (dpy, None, root, 0, 0, 0, 0, 10, msy);
                        msx = xevent->xmotion.x_root = 10;
                        workspaceSwitch (workspace + 1, c);
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
        if (params.box_move)
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
        gtk_main_quit ();
    }

    return status;
}

void
clientMove (Client * c, XEvent * e)
{
    XWindowChanges wc;
    MoveResizeData passdata;
    Cursor cursor = None;
    int g1 = GrabSuccess, g2 = GrabSuccess;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientDoMove");
    TRACE ("moving client \"%s\" (0x%lx)", c->name, c->window);

    passdata.c = c;
    passdata.ox = c->x;
    passdata.oy = c->y;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    /*
       The following trick is experimental, based on a patch for Kwin 3.1alpha1 by aviv bergman

       It is supposed to reduce latency during move/resize by mapping a screen large window that
       receives all pointer events.

       Original mail message is available here :

       http://www.xfree86.org/pipermail/xpert/2002-July/019434.html

       Note:

       I'm not sure it makes any difference, but who knows... It doesn' t hurt.
     */

    passdata.tmp_event_window =
        setTmpEventWin (0, 0, 
                        MyDisplayFullWidth (dpy, screen),
                        MyDisplayFullHeight (dpy, screen), 
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
        cursor = move_cursor;
        getMouseXY (root, &passdata.mx, &passdata.my);
    }
    g1 = XGrabKeyboard (dpy, passdata.tmp_event_window, FALSE,
        GrabModeAsync, GrabModeAsync, GDK_CURRENT_TIME);
    g2 = XGrabPointer (dpy, passdata.tmp_event_window, FALSE,
        ButtonMotionMask | ButtonReleaseMask, GrabModeAsync,
        GrabModeAsync, None, cursor, GDK_CURRENT_TIME);

    if (((passdata.use_keys) && (g1 != GrabSuccess)) || (g2 != GrabSuccess))
    {
        TRACE ("grab failed in clientMove");
        gdk_beep ();
        if ((passdata.use_keys) && (g1 == GrabSuccess))
        {
            XUngrabKeyboard (dpy, CurrentTime);
        }
        if (g2 == GrabSuccess)
        {
            XUngrabPointer (dpy, CurrentTime);
        }
        removeTmpEventWin (passdata.tmp_event_window);
        return;
    }

#ifdef SHOW_POSITION
    passdata.poswin = poswinCreate();
    poswinSetPosition (passdata.poswin, c);
    poswinShow (passdata.poswin);
#endif /* SHOW_POSITION */

    FLAG_SET (c->flags, CLIENT_FLAG_MOVING_RESIZING);
    TRACE ("entering move loop");
    pushEventFilter (clientMove_event_filter, &passdata);
    if (passdata.use_keys)
    {
        XPutBackEvent (dpy, e);
    }
    gtk_main ();
    popEventFilter ();
    TRACE ("leaving move loop");
    FLAG_UNSET (c->flags, CLIENT_FLAG_MOVING_RESIZING);
#ifdef SHOW_POSITION
    if (passdata.poswin)
    {
        poswinDestroy (passdata.poswin);
    }
#endif /* SHOW_POSITION */

    if (passdata.grab && params.box_move)
    {
        clientDrawOutline (c);
    }
    wc.x = c->x;
    wc.y = c->y;
    clientConfigure (c, &wc, CWX | CWY, NO_CFG_FLAG);

    XUngrabKeyboard (dpy, GDK_CURRENT_TIME);
    XUngrabPointer (dpy, GDK_CURRENT_TIME);
    removeTmpEventWin (passdata.tmp_event_window);

    if (passdata.grab && params.box_move)
    {
        myXUngrabServer ();
    }
}

static GtkToXEventFilterStatus
clientResize_event_filter (XEvent * xevent, gpointer data)
{
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    MoveResizeData *passdata = (MoveResizeData *) data;
    Client *c = passdata->c;
    gboolean resizing = TRUE;
    XWindowChanges wc;
    int prev_y = 0, prev_x = 0;
    int prev_height = 0, prev_width = 0;
    int cx, cy, disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_x, frame_y, frame_height, frame_width;
    int frame_top, frame_left, frame_right, frame_bottom;

    TRACE ("entering clientResize_event_filter");
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

    disp_x = MyDisplayX (cx, cy);
    disp_y = MyDisplayY (cx, cy);
    disp_max_x = MyDisplayMaxX (dpy, screen, cx, cy);
    disp_max_y = MyDisplayMaxY (dpy, screen, cx, cy);

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

            if (!passdata->grab && params.box_resize)
            {
                myXGrabServer ();
                passdata->grab = TRUE;
                clientDrawOutline (c);
            }
            if (params.box_resize)
            {
                clientDrawOutline (c);
            }
            /* Store previous height in case the resize hides the window behind the curtain */
            prev_width = c->width;
            prev_height = c->height;

            if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
                && (xevent->xkey.keycode == params.keys[KEY_MOVE_UP].keycode))
            {
                c->height = c->height - key_height_inc;
                corner = 4 + SIDE_BOTTOM;
            }
            else if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
                && (xevent->xkey.keycode == params.keys[KEY_MOVE_DOWN].keycode))
            {
                c->height = c->height + key_height_inc;
                corner = 4 + SIDE_BOTTOM;
            }
            else if (xevent->xkey.keycode == params.keys[KEY_MOVE_LEFT].keycode)
            {
                c->width = c->width - key_width_inc;
                corner = 4 + SIDE_RIGHT;
            }
            else if (xevent->xkey.keycode == params.keys[KEY_MOVE_RIGHT].keycode)
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
	            || (c->x + c->width < margins [LEFT] + CLIENT_MIN_VISIBLE))
        	{
                    c->width = prev_width;
        	}
        	if ((c->y + c->height < disp_y + CLIENT_MIN_VISIBLE)
	            || (c->y + c->height < margins [TOP] + CLIENT_MIN_VISIBLE))
        	{
                    c->height = prev_height;
        	}
	    }
            if (passdata->poswin)
            {
                poswinSetPosition (passdata->poswin, c);
            }
            if (params.box_resize)
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
            if (IsModifierKey (XKeycodeToKeysym (dpy, xevent->xkey.keycode, 0)))
            {
                resizing = FALSE;
            }
        }
    }
    else if (xevent->type == MotionNotify)
    {
        while (XCheckMaskEvent (dpy, ButtonMotionMask | PointerMotionMask, xevent))
            ; /* Skip event */
        
        if (xevent->type == ButtonRelease)
        {
            resizing = FALSE;
            clientClearLastOpTime (c);
        }

        if (!passdata->grab && params.box_resize)
        {
            myXGrabServer ();
            passdata->grab = TRUE;
            clientDrawOutline (c);
        }
        if (params.box_resize)
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
        clientSetWidth (c, c->width);
        clientSetHeight (c, c->height);
        clientConstrainRatio (c, c->width, c->height, passdata->corner);

        if ((passdata->corner == CORNER_TOP_LEFT)
            || (passdata->corner == CORNER_BOTTOM_LEFT)
            || (passdata->corner == 4 + SIDE_LEFT))
        {
            c->x = c->x - (c->width - passdata->oldw);
            frame_x = frameX (c);
        }
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
            && (passdata->corner == CORNER_TOP_LEFT
                || passdata->corner == CORNER_TOP_RIGHT))
        {
            c->y = c->y - (c->height - passdata->oldh);
            frame_y = frameY (c);
        }
	if (!clientCkeckTitle (c))
	{
            c->x = prev_x;
            c->y = prev_y;
            c->height = prev_height;
            c->width = prev_width;
	}
	else
	{
            if ((passdata->corner == CORNER_TOP_LEFT)
        	|| (passdata->corner == CORNER_TOP_RIGHT))
            {
        	if ((c->y > disp_max_y - CLIENT_MIN_VISIBLE)
	            || (c->y > MyDisplayFullHeight (dpy, screen) 
		               - margins [BOTTOM] - CLIENT_MIN_VISIBLE))
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
	            || (c->y + c->height < margins [TOP] + CLIENT_MIN_VISIBLE))
        	{
                    c->height = prev_height;
        	}
            }
            if ((passdata->corner == CORNER_TOP_LEFT)
        	|| (passdata->corner == CORNER_BOTTOM_LEFT)
        	|| (passdata->corner == 4 + SIDE_LEFT))
            {
        	if ((c->x > disp_max_x - CLIENT_MIN_VISIBLE)
	            || (c->x > MyDisplayFullWidth (dpy, screen) 
		               - margins [RIGHT] - CLIENT_MIN_VISIBLE))
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
	            || (c->x + c->width < margins [LEFT] + CLIENT_MIN_VISIBLE))
        	{
                    c->width = prev_width;
        	}
            }
	}
        if (passdata->poswin)
        {
            poswinSetPosition (passdata->poswin, c);
        }
        if (params.box_resize)
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
    XWindowChanges wc;
    MoveResizeData passdata;
    int g1 = GrabSuccess, g2 = GrabSuccess;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientResize");
    TRACE ("resizing client \"%s\" (0x%lx)", c->name, c->window);

    passdata.c = c;
    passdata.ox = c->width;
    passdata.oy = c->height;
    passdata.corner = CORNER_BOTTOM_RIGHT;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    passdata.corner = corner;
    passdata.tmp_event_window =
        setTmpEventWin (0, 0, 
                        MyDisplayFullWidth (dpy, screen),
                        MyDisplayFullHeight (dpy, screen), 
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
        getMouseXY (root, &passdata.mx, &passdata.my);
    }
    g1 = XGrabKeyboard (dpy, passdata.tmp_event_window, FALSE,
        GrabModeAsync, GrabModeAsync, GDK_CURRENT_TIME);
    g2 = XGrabPointer (dpy, passdata.tmp_event_window, FALSE,
        ButtonMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync,
        None, resize_cursor[passdata.corner], GDK_CURRENT_TIME);

    if (((passdata.use_keys) && (g1 != GrabSuccess)) || (g2 != GrabSuccess))
    {
        TRACE ("grab failed in clientResize");
        gdk_beep ();
        if ((passdata.use_keys) && (g1 == GrabSuccess))
        {
            XUngrabKeyboard (dpy, CurrentTime);
        }
        if (g2 == GrabSuccess)
        {
            XUngrabPointer (dpy, CurrentTime);
        }
        removeTmpEventWin (passdata.tmp_event_window);
        return;
    }

#ifndef SHOW_POSITION
    if ((c->size->width_inc > 1) || (c->size->height_inc > 1))
#endif /* SHOW_POSITION */
    {
        passdata.poswin = poswinCreate();
        poswinSetPosition (passdata.poswin, c);
        poswinShow (passdata.poswin);
    }
#ifndef SHOW_POSITION
    else
    {
        passdata.poswin = NULL;
    }
#endif /* SHOW_POSITION */
    
    FLAG_SET (c->flags, CLIENT_FLAG_MOVING_RESIZING);
    TRACE ("entering resize loop");
    pushEventFilter (clientResize_event_filter, &passdata);
    if (passdata.use_keys)
    {
        XPutBackEvent (dpy, e);
    }
    gtk_main ();
    popEventFilter ();
    TRACE ("leaving resize loop");
    FLAG_UNSET (c->flags, CLIENT_FLAG_MOVING_RESIZING);
    
    if (passdata.poswin)
    {
        poswinDestroy (passdata.poswin);
    }
    if (passdata.grab && params.box_resize)
    {
        clientDrawOutline (c);
    }

    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    clientConfigure (c, &wc, CWX | CWY | CWHeight | CWWidth, CFG_NOTIFY);

    XUngrabKeyboard (dpy, GDK_CURRENT_TIME);
    XUngrabPointer (dpy, GDK_CURRENT_TIME);
    removeTmpEventWin (passdata.tmp_event_window);

    if (passdata.grab && params.box_resize)
    {
        myXUngrabServer ();
    }
}

static GtkToXEventFilterStatus
clientCycle_event_filter (XEvent * xevent, gpointer data)
{
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
    gboolean cycling = TRUE;
    gboolean gone = FALSE;
    ClientCycleData *passdata = (ClientCycleData *) data;

    TRACE ("entering clientCycle_event_filter");
    switch (xevent->type)
    {
        case DestroyNotify:
            gone |=
                (passdata->c ==
                clientGetFromWindow (((XDestroyWindowEvent *) xevent)->window,
                    WINDOW));
            status = XEV_FILTER_CONTINUE;
        case UnmapNotify:
            gone |=
                (passdata->c ==
                clientGetFromWindow (((XUnmapEvent *) xevent)->window,
                    WINDOW));
            status = XEV_FILTER_CONTINUE;
        case KeyPress:
            if (gone
                || (xevent->xkey.keycode ==
                    params.keys[KEY_CYCLE_WINDOWS].keycode))
            {
                /* Hide frame draw */
                clientDrawOutline (passdata->c);

                /* If KEY_CYCLE_WINDOWS has Shift, then do not reverse */
                if (!(params.keys[KEY_CYCLE_WINDOWS].modifier & ShiftMask)
                        && xevent->xkey.state & ShiftMask) {
                    passdata->c =
                        clientGetPrevious (passdata->c, passdata->cycle_range);
                }
                else
                {
                    passdata->c =
                        clientGetNext (passdata->c, passdata->cycle_range);
                }

                if (passdata->c)
                {
                    /* Redraw frame draw */
                    clientDrawOutline (passdata->c);
                    tabwinSetLabel (passdata->tabwin, passdata->c->name);
                }
                else
                {
                    cycling = FALSE;
                }
            }
            break;
        case KeyRelease:
            {
                int keysym = XKeycodeToKeysym (dpy, xevent->xkey.keycode, 0);

                /* If KEY_CYCE_WINDOWS has Shift, then stop cycling on Shift
                 * release.
                 */
                if (IsModifierKey (keysym)
                        && ( (params.keys[KEY_CYCLE_WINDOWS].modifier
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
    ClientCycleData passdata;
    int g1, g2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientCycle");

    g1 = XGrabKeyboard (dpy, gnome_win, FALSE, GrabModeAsync, GrabModeAsync,
        GDK_CURRENT_TIME);
    g2 = XGrabPointer (dpy, gnome_win, FALSE, NoEventMask, GrabModeAsync,
        GrabModeAsync, None, None, GDK_CURRENT_TIME);
    if ((g1 != GrabSuccess) || (g2 != GrabSuccess))
    {
        TRACE ("grab failed in clientCycle");
        gdk_beep ();
        if (g1 == GrabSuccess)
        {
            XUngrabKeyboard (dpy, CurrentTime);
        }
        if (g2 == GrabSuccess)
        {
            XUngrabPointer (dpy, CurrentTime);
        }
        return;
    }

    myXGrabServer ();
    if (params.cycle_minimum)
    {
        passdata.cycle_range = INCLUDE_HIDDEN;
    }
    else
    {
        passdata.cycle_range = INCLUDE_HIDDEN | INCLUDE_SKIP_TASKBAR | INCLUDE_SKIP_PAGER;
    }
    passdata.c = clientGetNext (c, passdata.cycle_range);
    if (passdata.c)
    {
        passdata.tabwin = tabwinCreate (passdata.c->name);
        TRACE ("entering cycle loop");
        /* Draw frame draw */
        clientDrawOutline (passdata.c);
        pushEventFilter (clientCycle_event_filter, &passdata);
        gtk_main ();
        popEventFilter ();
        if (passdata.c)
        {
            /* Hide frame draw */
            clientDrawOutline (passdata.c);
        }
        TRACE ("leaving cycle loop");
        tabwinDestroy (passdata.tabwin);
        g_free (passdata.tabwin);
    }
    myXUngrabServer ();
    XUngrabKeyboard (dpy, GDK_CURRENT_TIME);
    XUngrabPointer (dpy, GDK_CURRENT_TIME);

    if (passdata.c)
    {
        clientShow (passdata.c, TRUE);
        clientRaise (passdata.c);
        clientSetFocus (passdata.c, NO_FOCUS_FLAG);
        clientPassGrabButton1 (passdata.c);
    }
}

static GtkToXEventFilterStatus
clientButtonPress_event_filter (XEvent * xevent, gpointer data)
{
    GtkToXEventFilterStatus status = XEV_FILTER_STOP;
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
    int b, g1;
    ButtonPressData passdata;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientButtonPress");

    for (b = 0; b < BUTTON_COUNT; b++)
    {
        if (MYWINDOW_XWINDOW (c->buttons[b]) == w)
        {
            break;
        }
    }

    g1 = XGrabPointer (dpy, w, FALSE,
        ButtonReleaseMask | EnterWindowMask | LeaveWindowMask, GrabModeAsync,
        GrabModeAsync, None, None, CurrentTime);

    if (g1 != GrabSuccess)
    {
        TRACE ("grab failed in clientButtonPress");
        gdk_beep ();
        if (g1 == GrabSuccess)
        {
            XUngrabKeyboard (dpy, CurrentTime);
        }
        return;
    }

    passdata.c = c;
    passdata.b = b;

    c->button_pressed[b] = TRUE;
    frameDraw (c, FALSE, FALSE);

    TRACE ("entering button press loop");
    pushEventFilter (clientButtonPress_event_filter, &passdata);
    gtk_main ();
    popEventFilter ();
    TRACE ("leaving button press loop");

    XUngrabPointer (dpy, CurrentTime);

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
        c2 = clientGetFromWindow (c->group_leader, WINDOW);
    }
    else if (c->client_leader != None)
    {
        c2 = clientGetFromWindow (c->client_leader, WINDOW);
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
