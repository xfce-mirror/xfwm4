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
        xfwm4    - (c) 2002-2006 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include <config.h>
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
#include "event_filter.h"

/* Event mask definition */

#define POINTER_EVENT_MASK \
    ButtonPressMask|\
    ButtonReleaseMask

#define FRAME_EVENT_MASK \
    SubstructureNotifyMask|\
    SubstructureRedirectMask|\
    FocusChangeMask|\
    EnterWindowMask|\
    PropertyChangeMask

#define CLIENT_EVENT_MASK \
    StructureNotifyMask|\
    FocusChangeMask|\
    PropertyChangeMask

/* Useful macros */
#define START_ICONIC(c) \
    ((c->wmhints) && \
     (c->wmhints->initial_state == IconicState) && \
     !clientIsValidTransientOrModal (c))

#define OPACITY_SET_STEP        (guint) 0x16000000
#define OPACITY_SET_MIN         (guint) 0x40000000

typedef struct _MoveResizeData MoveResizeData;
struct _MoveResizeData
{
    Client *c;
    gboolean use_keys;
    gboolean grab;
    gboolean is_transient;
    gboolean move_resized;
    gboolean released;
    int button;
    int cancel_x, cancel_y; /* for cancellation (either position or size) */
    int cancel_workspace;
    int mx, my;
    int ox, oy;
    int oldw, oldh;
    int corner;
    Poswin *poswin;
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

/* Forward decl */
static void 
clientUpdateIconPix (Client * c);

Display *
clientGetXDisplay (Client * c)
{
    g_return_val_if_fail (c, NULL);

    return myScreenGetXDisplay (c->screen_info);
}

void
clientInstallColormaps (Client * c)
{
    XWindowAttributes attr;
    gboolean installed;
    int i;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientInstallColormaps");

    installed = FALSE;
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
}

void
clientUpdateName (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    gchar *name;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateName");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    getWindowName (display_info, c->window, &name);
    if (name)
    {
        if (c->name)
        {
            if (strcmp (name, c->name))
            {
                g_free (c->name);
                c->name = name;
                FLAG_SET (c->flags, CLIENT_FLAG_NAME_CHANGED);
                frameQueueDraw (c);
            }
        }
    }
}

void
clientUpdateAllFrames (ScreenInfo *screen_info, int mask)
{
    Client *c;
    XWindowChanges wc;
    int i;

    g_return_if_fail (screen_info != NULL);

    TRACE ("entering clientRedrawAllFrames");
    myScreenGrabPointer (screen_info, EnterWindowMask, None, myDisplayGetCurrentTime (screen_info->display_info));

    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, i++)
    {
        unsigned long configure_flags = 0L;

        if (mask & UPDATE_BUTTON_GRABS)
        {
            clientUngrabButtons (c);
            clientGrabButtons (c);
            clientGrabMouseButton (c);
        }
        if (mask & UPDATE_CACHE)
        {
            clientUpdateIconPix (c);
        }
        if (mask & UPDATE_GRAVITY)
        {
            clientGravitate (c, REMOVE);
            clientGravitate (c, APPLY);
            setNetFrameExtents (screen_info->display_info,
                                c->window,
                                frameTop (c),
                                frameLeft (c),
                                frameRight (c),
                                frameBottom (c));
            configure_flags |= CFG_FORCE_REDRAW;
            mask &= ~UPDATE_FRAME;
        }
        if (mask & UPDATE_MAXIMIZE)
        {
            unsigned long maximization_flags = 0L;

            /* Recompute size and position of maximized windows */
            if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ | CLIENT_FLAG_MAXIMIZED_VERT))
            {
                 maximization_flags |= FLAG_TEST (c->flags,
                     CLIENT_FLAG_MAXIMIZED_HORIZ) ? WIN_STATE_MAXIMIZED_HORIZ : 0;
                 maximization_flags |= FLAG_TEST (c->flags,
                     CLIENT_FLAG_MAXIMIZED_VERT) ? WIN_STATE_MAXIMIZED_VERT : 0;

                 /* Force an update by clearing the internal flags */
                 FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ | CLIENT_FLAG_MAXIMIZED_VERT);
                 clientToggleMaximized (c, maximization_flags, FALSE);

                 configure_flags |= CFG_FORCE_REDRAW;
                 mask &= ~UPDATE_FRAME;
            }
        }
        if (configure_flags != 0L)
        {
             wc.x = c->x;
             wc.y = c->y;
             wc.width = c->width;
             wc.height = c->height;
             clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, configure_flags);
        }
        if (mask & UPDATE_FRAME)
        {
            frameDraw (c, TRUE);
        }

    }
    myScreenUngrabPointer (screen_info);
}

void
clientGrabButtons (Client * c)
{
    ScreenInfo *screen_info;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientGrabButtons");
    TRACE ("grabbing buttons for client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    if (screen_info->params->easy_click)
    {
        grabButton(clientGetXDisplay (c), AnyButton, screen_info->params->easy_click, c->window);
    }
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
    Client *c;

    TRACE ("entering urgent_cb");

    c = (Client *) data;
    if (c != clientGetFocus ())
    {
        FLAG_TOGGLE (c->xfwm_flags, XFWM_FLAG_SEEN_ACTIVE);
        frameDraw (c, FALSE);
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
        frameDraw (c, FALSE);
    }
    FLAG_UNSET (c->wm_flags, WM_FLAG_URGENT);

    c->blink_timeout_id = 0;
    if ((c->wmhints) && (c->wmhints->flags & XUrgencyHint))
    {
        FLAG_SET (c->wm_flags, WM_FLAG_URGENT);
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
        {
            c->blink_timeout_id =
                g_timeout_add_full (G_PRIORITY_DEFAULT, 
                                    CLIENT_BLINK_TIMEOUT, 
                                    (GtkFunction) urgent_cb,
                                    (gpointer) c, NULL);
        }
    }
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_SEEN_ACTIVE)
        && !FLAG_TEST (c->wm_flags, WM_FLAG_URGENT)
        && (c != clientGetFocus ()))
    {
        FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_SEEN_ACTIVE);
        frameDraw (c, FALSE);
    }
}

void
clientCoordGravitate (Client * c, int mode, int *x, int *y)
{
    int dx, dy;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientCoordGravitate");

    c->gravity = c->size->flags & PWinGravity ? c->size->win_gravity : NorthWestGravity;
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
clientComputeWidth (Client * c, int *w)
{
    int w2;

    g_return_if_fail (c != NULL);
    g_return_if_fail (w != NULL);
    TRACE ("entering clientComputeWidth");

    /* Bypass resize increment and max sizes for fullscreen */
    if (!FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN)
        && !(FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
             && (c->screen_info->params->borderless_maximize)))
    {
        if ((c->size->flags & PResizeInc) && (c->size->width_inc))
        {
            w2 = (*w - c->size->min_width) / c->size->width_inc;
            *w = c->size->min_width + (w2 * c->size->width_inc);
        }
        if (c->size->flags & PMaxSize)
        {
            if (*w > c->size->max_width)
            {
                *w = c->size->max_width;
            }
        }
    }

    if (c->size->flags & PMinSize)
    {
        if (*w < c->size->min_width)
        {
            *w = c->size->min_width;
        }
    }
    if (*w < 1)
    {
        *w = 1;
    }
}

static void
clientSetWidth (Client * c, int w)
{
    int temp;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetWidth");
    TRACE ("setting width %i for client \"%s\" (0x%lx)", w, c->name, c->window);

    temp = w;
    clientComputeWidth (c, &temp);
    c->width = temp;
}

static void
clientComputeHeight (Client * c, int *h)
{
    int h2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientComputeHeight");

    /* Bypass resize increment and max sizes for fullscreen */
    if (!FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN)
        && !(FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
             && (c->screen_info->params->borderless_maximize)))
    {
        if ((c->size->flags & PResizeInc) && (c->size->height_inc))
        {
            h2 = (*h - c->size->min_height) / c->size->height_inc;
            *h = c->size->min_height + (h2 * c->size->height_inc);
        }
        if (c->size->flags & PMaxSize)
        {
            if (*h > c->size->max_height)
            {
                *h = c->size->max_height;
            }
        }
    }

    if (c->size->flags & PMinSize)
    {
        if (*h < c->size->min_height)
        {
            *h = c->size->min_height;
        }
    }
    if (*h < 1)
    {
        *h = 1;
    }
}

static void
clientSetHeight (Client * c, int h)
{
    int temp;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetHeight");
    TRACE ("setting height %i for client \"%s\" (0x%lx)", h, c->name, c->window);

    temp = h;
    clientComputeHeight (c, &temp);
    c->height = temp;
}

/* clientConstrainRatio - adjust the given width and height to account for
   the constraints imposed by size hints

   The aspect ratio stuff, is borrowed from uwm's CheckConsistency routine.
 */

#define MAKE_MULT(a,b) ((b==1) ? (a) : (((int)((a)/(b))) * (b)) )
static void
clientConstrainRatio (Client * c, int *w, int *h, int corner)
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

        if ((minx * *h > miny * *w) && (miny) &&
            ((corner == CORNER_COUNT + SIDE_TOP) || (corner == CORNER_COUNT + SIDE_BOTTOM)))
        {
            /* Change width to match */
            delta = MAKE_MULT (minx * *h /  miny - *w, xinc);
            if (!(c->size->flags & PMaxSize) ||
                (*w + delta <= c->size->max_width))
            {
                *w += delta;
            }
        }
        if ((minx * *h > miny * *w) && (minx))
        {
            delta = MAKE_MULT (*h - *w * miny / minx, yinc);
            if (!(c->size->flags & PMinSize) ||
                (*h - delta >= c->size->min_height))
            {
                *h -= delta;
            }
            else
            {
                delta = MAKE_MULT (minx * *h / miny - *w, xinc);
                if (!(c->size->flags & PMaxSize) ||
                    (*w + delta <= c->size->max_width))
                  *w += delta;
            }
        }

        if ((maxx * *h < maxy * *w) && (maxx) &&
            ((corner == CORNER_COUNT + SIDE_LEFT) || (corner == CORNER_COUNT + SIDE_RIGHT)))
        {
            delta = MAKE_MULT (*w * maxy / maxx - *h, yinc);
            if (!(c->size->flags & PMaxSize) ||
                (*h + delta <= c->size->max_height))
            {
                *h += delta;
            }
        }
        if ((maxx * *h < maxy * *w) && (maxy))
        {
            delta = MAKE_MULT (*w - maxx * *h / maxy, xinc);
            if (!(c->size->flags & PMinSize) ||
                (*w - delta >= c->size->min_width))
            {
                *w -= delta;
            }
            else
            {
                delta = MAKE_MULT (*w * maxy / maxx - *h, yinc);
                if (!(c->size->flags & PMaxSize) ||
                    (*h + delta <= c->size->max_height))
                {
                    *h += delta;
                }
            }
        }
    }
}

#define WIN_MOVED   (mask & (CWX | CWY))
#define WIN_RESIZED (mask & (CWWidth | CWHeight))

static void
clientConfigureWindows (Client * c, XWindowChanges * wc, unsigned long mask, unsigned short flags)
{
    unsigned long change_mask;
    XWindowChanges change_values;

    change_mask = (mask & (CWX | CWY | CWWidth | CWHeight));
    if (flags & CFG_FORCE_REDRAW)
    {
        change_mask |= (CWX | CWY);
    }

    if (change_mask & (CWX | CWY | CWWidth | CWHeight))
    {
        change_values.x = frameX (c);
        change_values.y = frameY (c);
        change_values.width = frameWidth (c);
        change_values.height = frameHeight (c);
        XConfigureWindow (clientGetXDisplay (c), c->frame, change_mask, &change_values);

        if (WIN_RESIZED || (flags & CFG_FORCE_REDRAW))
        {
            frameDraw (c, (flags & CFG_FORCE_REDRAW));
        }

        change_values.x = frameLeft (c);
        change_values.y = frameTop (c);
        change_values.width = c->width;
        change_values.height = c->height;
        XConfigureWindow (clientGetXDisplay (c), c->window, change_mask, &change_values);
    }
}

void
clientConfigure (Client * c, XWindowChanges * wc, unsigned long mask, unsigned short flags)
{
    XConfigureEvent ce;
    int px, py, pwidth, pheight;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    TRACE ("entering clientConfigure");
    TRACE ("configuring client \"%s\" (0x%lx) %s, type %u", c->name,
        c->window, flags & CFG_CONSTRAINED ? "constrained" : "not contrained", c->type);

    if (mask & CWX)
    {
        if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING))
        {
            c->x = wc->x;
        }
    }
    if (mask & CWY)
    {
        if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING))
        {
            c->y = wc->y;
        }
    }
    if (mask & CWWidth)
    {
        clientSetWidth (c, wc->width);
    }
    if (mask & CWHeight)
    {
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
             * Limitation: we don't support neither
             * TopIf, BottomIf nor Opposite ...
             */
            case Above:
                TRACE ("Above");
                if (mask & CWSibling)
                {
                    clientRaise (c, wc->sibling);
                }
                else
                {
                    clientRaise (c, None);
                }
                break;
            case Below:
                TRACE ("Below");
                if (mask & CWSibling)
                {
                    clientLower (c, wc->sibling);
                }
                else
                {
                    clientLower (c, None);
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

    /* Keep control over what the application does. However, some broken apps try
       to achieve fullscreen by using static gravity and a (0,0) position, the
       second part of the test is for this case.
     */
    if (((flags & (CFG_CONSTRAINED | CFG_REQUEST)) == (CFG_CONSTRAINED | CFG_REQUEST))
         && CONSTRAINED_WINDOW (c)
         && !((c->gravity == StaticGravity) && (c->x == 0) && (c->y == 0)))
    {
        px = c->x;
        py = c->y;
        pwidth = c->width;
        pheight = c->height;

        /* Keep fully visible only on resize */
        clientConstrainPos (c, (mask & (CWWidth | CWHeight)));

        if (c->x != px)
        {
            mask |= CWX;
        }
        if (c->y != py)
        {
            mask |= CWY;
        }

        if (c->width != pwidth)
        {
            mask |= CWWidth;
        }
        if (c->height != pheight)
        {
            mask |= CWHeight;
        }
    }

    clientConfigureWindows (c, wc, mask, flags);
    /*

      We reparent to client window. According to the ICCCM spec, the
      WM must send a senthetic event when the window is moved and not resized.

      But, since we reparent the window, we must also send a synthetic
      configure event when the window is moved and resized.

      See this thread for the rational:
      http://www.mail-archive.com/wm-spec-list@gnome.org/msg00379.html

      And specifically this post from Carsten Haitzler:
      http://www.mail-archive.com/wm-spec-list@gnome.org/msg00382.html

     */
    if ((WIN_MOVED) || (flags & CFG_NOTIFY) ||
        ((flags & CFG_REQUEST) && !(WIN_MOVED || WIN_RESIZED)))
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
        XSendEvent (clientGetXDisplay (c), c->window, FALSE,
                    StructureNotifyMask, (XEvent *) & ce);
    }
#undef WIN_MOVED
#undef WIN_RESIZED
}

void
clientGetMWMHints (Client * c, gboolean update)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    PropMwmHints *mwm_hints;
    XWindowChanges wc;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    TRACE ("entering clientGetMWMHints client \"%s\" (0x%lx)", c->name,
        c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    mwm_hints = getMotifHints (display_info, c->window);
    if (mwm_hints)
    {
        if ((mwm_hints->flags & MWM_HINTS_DECORATIONS))
        {
            if (!FLAG_TEST (c->flags, CLIENT_FLAG_HAS_SHAPE))
            {
                if (mwm_hints->decorations & MWM_DECOR_ALL)
                {
                    FLAG_SET (c->xfwm_flags, XFWM_FLAG_HAS_BORDER | XFWM_FLAG_HAS_MENU);
                }
                else
                {
                    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_HAS_BORDER | XFWM_FLAG_HAS_MENU);
                    FLAG_SET (c->xfwm_flags, (mwm_hints-> decorations & (MWM_DECOR_TITLE | MWM_DECOR_BORDER))
                                             ? XFWM_FLAG_HAS_BORDER : 0);
                    FLAG_SET (c->xfwm_flags, (mwm_hints->decorations & (MWM_DECOR_MENU))
                                             ? XFWM_FLAG_HAS_MENU : 0);
                    /*
                       FLAG_UNSET(c->xfwm_flags, XFWM_FLAG_HAS_HIDE);
                       FLAG_UNSET(c->xfwm_flags, XFWM_FLAG_HAS_MAXIMIZE);
                       FLAG_SET(c->xfwm_flags, (mwm_hints->decorations & (MWM_DECOR_MINIMIZE)) ? XFWM_FLAG_HAS_HIDE : 0);
                       FLAG_SET(c->xfwm_flags, (mwm_hints->decorations & (MWM_DECOR_MAXIMIZE)) ? XFWM_FLAG_HAS_MAXIMIZE : 0);
                     */
                }
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
    XWindowChanges wc;
    unsigned long previous_value;
    long dummy;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    TRACE ("entering clientGetWMNormalHints client \"%s\" (0x%lx)", c->name,
        c->window);

    if (!c->size)
    {
        c->size = XAllocSizeHints ();
    }
    g_assert (c->size);

    dummy = 0;
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
            frameQueueDraw (c);
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
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    unsigned int wm_protocols_flags;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    TRACE ("entering clientGetWMProtocols client \"%s\" (0x%lx)", c->name,
        c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    wm_protocols_flags = getWMProtocols (display_info, c->window);
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

#ifdef HAVE_XSYNC
static void
clientIncrementXSyncValue (Client *c)
{
    XSyncValue add;
    int overflow;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->xsync_counter != None);

    TRACE ("entering clientIncrementXSyncValue");

    XSyncIntToValue (&add, 1);
    XSyncValueAdd (&c->xsync_value, c->xsync_value, add, &overflow);
}

static gboolean
clientCreateXSyncAlarm (Client *c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    XSyncAlarmAttributes values;

    g_return_val_if_fail (c != NULL, FALSE);
    g_return_val_if_fail (c->xsync_counter != None, FALSE);

    TRACE ("entering clientCreateXSyncAlarm");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    XSyncIntToValue (&c->xsync_value, 0);
    XSyncSetCounter (display_info->dpy, c->xsync_counter, c->xsync_value);
    XSyncIntToValue (&values.trigger.wait_value, 1);
    XSyncIntToValue (&values.delta, 1);

    values.trigger.counter = c->xsync_counter;
    values.trigger.value_type = XSyncAbsolute;
    values.trigger.test_type = XSyncPositiveComparison;
    values.events = True;

    c->xsync_alarm = XSyncCreateAlarm (display_info->dpy,
                                       XSyncCACounter | 
                                       XSyncCADelta | 
                                       XSyncCAEvents | 
                                       XSyncCATestType | 
                                       XSyncCAValue | 
                                       XSyncCAValueType,
                                       &values);
    return (c->xsync_alarm != None);
}

static void
clientDestroyXSyncAlarm (Client *c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->xsync_alarm != None);

    TRACE ("entering clientClearXSyncAlarm");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    XSyncDestroyAlarm (display_info->dpy, c->xsync_alarm);
    c->xsync_alarm = None;
}

static void
clientXSyncClearTimeout (Client * c)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientXSyncClearTimeout");

    if (c->xsync_timeout_id)
    {
        g_source_remove (c->xsync_timeout_id);
        c->xsync_timeout_id = 0;
    }
}

static gboolean
clientXSyncTimeout (gpointer data)
{
    Client *c;
    XWindowChanges wc;

    TRACE ("entering clientXSyncTimeout");

    c = (Client *) data;
    if (c)
    {
        TRACE ("XSync timeout for client \"%s\" (0x%lx)", c->name, c->window);
        clientXSyncClearTimeout (c);
        c->xsync_waiting = FALSE;
        c->xsync_enabled = FALSE;

        wc.x = c->x;
        wc.y = c->y;
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, NO_CFG_FLAG);
    }
    return (TRUE);
}

static void
clientXSyncResetTimeout (Client * c)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientXSyncResetTimeout");

    clientXSyncClearTimeout (c);
    c->xsync_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT, 
                                              CLIENT_XSYNC_TIMEOUT, 
                                              (GtkFunction) clientXSyncTimeout, 
                                              (gpointer) c, NULL);
}

void
clientXSyncRequest (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    TRACE ("entering clientXSyncRequest");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    clientIncrementXSyncValue (c);
    sendXSyncRequest (display_info, c->window, c->xsync_value);
    clientXSyncResetTimeout (c);
    c->xsync_waiting = TRUE;
}

static gboolean
clientXSyncEnable (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_val_if_fail (c != NULL, FALSE);

    TRACE ("entering clientXSyncEnable");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    c->xsync_enabled = FALSE;
    if (display_info->have_xsync)
    {
        if ((c->xsync_counter) && (c->xsync_alarm))
        {
            c->xsync_enabled = TRUE;
        }
    }
    return (c->xsync_enabled);
}
#endif /* HAVE_XSYNC */

static void
clientFree (Client * c)
{
    g_return_if_fail (c != NULL);

    TRACE ("entering clientFree");
    TRACE ("freeing client \"%s\" (0x%lx)", c->name, c->window);

    clientClearFocus (c);
    if (clientGetLastRaise (c->screen_info) == c)
    {
        clientClearLastRaise (c->screen_info);
    }
    if (clientGetLastUngrab () == c)
    {
        clientClearLastUngrab ();
    }
    if (c->blink_timeout_id)
    {
        g_source_remove (c->blink_timeout_id);
    }
    if (c->icon_timeout_id)
    {
        g_source_remove (c->icon_timeout_id);
    }
    if (c->frame_timeout_id)
    {
        g_source_remove (c->frame_timeout_id);
    }
    if (c->name)
    {
        g_free (c->name);
    }
#ifdef HAVE_XSYNC
    if (c->xsync_alarm != None)
    {
        clientDestroyXSyncAlarm (c);
    }
    if (c->xsync_timeout_id)
    {
        g_source_remove (c->xsync_timeout_id);
    }
#endif /* HAVE_XSYNC */
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    if (c->startup_id)
    {
        g_free (c->startup_id);
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
    if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
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
            clientToggleMaximized (c, mode, FALSE);
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
        TRACE ("Applying client's initial state: above");
        clientUpdateAboveState (c);
    }
    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_BELOW, CLIENT_FLAG_ABOVE))
    {
        TRACE ("Applying client's initial state: below");
        clientUpdateBelowState (c);
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
            frameDraw (c, FALSE);
        }
    }
    else if ((action & WIN_STATE_MAXIMIZED)
             && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MAXIMIZE))
    {
        TRACE ("client \"%s\" (0x%lx) has received a win_state/maximize event",
            c->name, c->window);
        clientToggleMaximized (c, add_remove, TRUE);
    }
}

static gboolean
clientCheckShape (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    int xws, yws, xbs, ybs;
    unsigned wws, hws, wbs, hbs;
    int boundingShaped, clipShaped;

    g_return_val_if_fail (c != NULL, FALSE);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (display_info->have_shape)
    {
        XShapeQueryExtents (display_info->dpy, c->window, &boundingShaped, &xws, &yws, &wws,
                            &hws, &clipShaped, &xbs, &ybs, &wbs, &hbs);
        return (boundingShaped != 0);
    }
    return FALSE;
}

static void
clientGetUserTime (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (getNetWMUserTime (display_info, c->window, &c->user_time))
    {
        FLAG_SET (c->flags, CLIENT_FLAG_HAS_USER_TIME);
        myDisplaySetLastUserTime (display_info, c->user_time);
    }
}

static void
clientUpdateIconPix (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    gint size;
    GdkPixbuf *icon;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    TRACE ("entering clientUpdateIconPix for \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    xfwmPixmapFree (&c->appmenu[ACTIVE]);
    xfwmPixmapFree (&c->appmenu[INACTIVE]);
    xfwmPixmapFree (&c->appmenu[PRESSED]);

    if (screen_info->buttons[MENU_BUTTON][ACTIVE].pixmap == None)
    {
        /* The current theme has no menu button */
        return;
    }

    xfwmPixmapDuplicate (&screen_info->buttons[MENU_BUTTON][ACTIVE],
                         &c->appmenu[ACTIVE]);
    xfwmPixmapDuplicate (&screen_info->buttons[MENU_BUTTON][INACTIVE],
                         &c->appmenu[INACTIVE]);
    xfwmPixmapDuplicate (&screen_info->buttons[MENU_BUTTON][PRESSED],
                         &c->appmenu[PRESSED]);

    size = MIN (screen_info->buttons[MENU_BUTTON][ACTIVE].width,
                screen_info->buttons[MENU_BUTTON][ACTIVE].height);

    if (size > 1)
    {
        icon = getAppIcon (display_info, c->window, size, size);

        xfwmPixmapRenderGdkPixbuf (&c->appmenu[ACTIVE], icon);
        xfwmPixmapRenderGdkPixbuf (&c->appmenu[INACTIVE], icon);
        xfwmPixmapRenderGdkPixbuf (&c->appmenu[PRESSED], icon);

        g_object_unref (icon);
    }
}

static gboolean
update_icon_idle_cb (gpointer data)
{
    Client *c;
    
    TRACE ("entering update_icon_idle_cb");

    c = (Client *) data;
    g_return_val_if_fail (c, FALSE);

    clientUpdateIconPix (c);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
    {
        frameDraw (c, FALSE);
    }
    c->icon_timeout_id = 0;

    return FALSE;
}

void
clientUpdateIcon (Client * c)
{
    g_return_if_fail (c);

    TRACE ("entering clientUpdateIcon for \"%s\" (0x%lx)", c->name, c->window);

    if (c->icon_timeout_id == 0)
    {
        c->icon_timeout_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, 
                                              update_icon_idle_cb, c, NULL);
    }
}

Client *
clientFrame (DisplayInfo *display_info, Window w, gboolean recapture)
{
    ScreenInfo *screen_info;
    XWindowAttributes attr;
    XWindowChanges wc;
    XSetWindowAttributes attributes;
    Client *c = NULL;
    gboolean shaped;
    gboolean grabbed;
    unsigned long valuemask;
    int i;

    g_return_val_if_fail (w != None, NULL);
    g_return_val_if_fail (display_info != NULL, NULL);

    TRACE ("entering clientFrame");
    TRACE ("framing client (0x%lx)", w);

    gdk_error_trap_push ();
    myDisplayGrabServer (display_info);

    if (!XGetWindowAttributes (display_info->dpy, w, &attr))
    {
        TRACE ("Cannot get window attributes");
        myDisplayUngrabServer (display_info);
        gdk_error_trap_pop ();
        return NULL;
    }

    screen_info = myDisplayGetScreenFromRoot (display_info, attr.root);
    if (!screen_info)
    {
        TRACE ("Cannot determine screen info from windows");
        myDisplayUngrabServer (display_info);
        gdk_error_trap_pop ();
        return NULL;
    }

    if (w == screen_info->xfwm4_win)
    {
        TRACE ("Not managing our own event window");
        compositorAddWindow (display_info, w, NULL);
        myDisplayUngrabServer (display_info);
        gdk_error_trap_pop ();
        return NULL;
    }

#ifdef ENABLE_KDE_SYSTRAY_PROXY
    if (checkKdeSystrayWindow (display_info, w))
    {
        TRACE ("Detected KDE systray windows");
        if (screen_info->systray != None)
        {
            sendSystrayReqDock (display_info, w, screen_info->systray);
            myDisplayUngrabServer (display_info);
            gdk_error_trap_pop ();
            return NULL;
        }
        TRACE ("No systray found for this screen");
    }
#endif /* ENABLE_KDE_SYSTRAY_PROXY */

    if (attr.override_redirect)
    {
        TRACE ("Override redirect window 0x%lx", w);
        compositorAddWindow (display_info, w, NULL);
        myDisplayUngrabServer (display_info);
        gdk_error_trap_pop ();
        return NULL;
    }

    c = g_new0 (Client, 1);
    if (!c)
    {
        TRACE ("Cannot allocate memory for the window structure");
        myDisplayUngrabServer (display_info);
        gdk_error_trap_pop ();
        return NULL;
    }

    c->window = w;
    c->screen_info = screen_info;
    c->serial = screen_info->client_serial++;

    getWindowName (display_info, c->window, &c->name);
    TRACE ("name \"%s\"", c->name);
    getTransientFor (display_info, screen_info->xroot, c->window, &c->transient_for);

    /* Initialize structure */
    c->size = NULL;
    c->flags = 0L;
    c->wm_flags = 0L;
    c->xfwm_flags = CLIENT_FLAG_INITIAL_VALUES;
    c->x = attr.x;
    c->y = attr.y;
    c->width = attr.width;
    c->height = attr.height;

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    c->startup_id = NULL;
#endif /* HAVE_LIBSTARTUP_NOTIFICATION */

#ifdef HAVE_XSYNC
    c->xsync_waiting = FALSE;
    c->xsync_enabled = FALSE;
    c->xsync_counter = None;
    c->xsync_alarm = None;
    c->xsync_timeout_id = 0;
    if (display_info->have_xsync)
    {
        getXSyncCounter (display_info, c->window, &c->xsync_counter);
        if ((c->xsync_counter) && clientCreateXSyncAlarm (c))
        {
            c->xsync_enabled = TRUE;
        }
    }
#endif /* HAVE_XSYNC */

    clientGetWMNormalHints (c, FALSE);

    c->size->x = c->x;
    c->size->y = c->y;
    c->size->width = c->width;
    c->size->height = c->height;
    c->previous_width = -1;
    c->previous_height = -1;
    c->border_width = attr.border_width;
    c->cmap = attr.colormap;

    shaped = clientCheckShape(c);
    if (shaped)
    {
        FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_HAS_BORDER);
        FLAG_SET (c->flags, CLIENT_FLAG_HAS_SHAPE);
    }

    if (((c->size->flags & (PMinSize | PMaxSize)) != (PMinSize | PMaxSize))
        || (((c->size->flags & (PMinSize | PMaxSize)) ==
                (PMinSize | PMaxSize))
            && ((c->size->min_width < c->size->max_width)
                || (c->size->min_height < c->size->max_height))))
    {
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_IS_RESIZABLE);
    }

    for (i = 0; i < BUTTON_LAST; i++)
    {
        c->button_pressed[i] = FALSE;
    }

    if (!XGetWMColormapWindows (display_info->dpy, c->window, &c->cmap_windows, &c->ncmap))
    {
        c->ncmap = 0;
    }

    /* Opacity for compositing manager */
    c->opacity = NET_WM_OPAQUE;
    getOpacity (display_info, c->window, &c->opacity);
    c->opacity_applied = c->opacity;
    c->opacity_flags = 0;

    c->opacity_locked = getOpacityLock (display_info, c->window);

    /* Timout for asynchronous icon update */
    c->icon_timeout_id = 0;
    /* Timout for asynchronous frame update */
    c->frame_timeout_id = 0;
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
    c->client_leader = getClientLeader (display_info, c->window);

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
    FLAG_SET (c->wm_flags, HINTS_ACCEPT_INPUT (c->wmhints) ? WM_FLAG_INPUT : 0);

    clientGetWMProtocols (c);
    clientGetMWMHints (c, FALSE);
    getHint (display_info, w, WIN_HINTS, (long *) &c->win_hints);
    getHint (display_info, w, WIN_STATE, (long *) &c->win_state);
    if (!getHint (display_info, w, WIN_LAYER, (long *) &c->win_layer))
    {
        c->win_layer = WIN_LAYER_NORMAL;
    }
    c->fullscreen_old_layer = c->win_layer;

    /* net_wm_user_time standard */
    clientGetUserTime (c);

    /* Apply startup notification properties if available */
    sn_client_startup_properties (c);

    /* Reload from session */
    if (sessionMatchWinToSM (c))
    {
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_SESSION_MANAGED);
    }

    /* Beware, order of calls is important here ! */
    clientGetWinState (c);
    clientGetNetState (c);
    clientGetNetWmType (c);
    clientGetInitialNetWmDesktop (c);
    /* workarea will be updated when shown, no need to worry here */
    clientGetNetStruts (c);

    /* Fullscreen for older legacy apps */
    if ((c->x <= 0) && (c->y <= 0) &&
        (c->width >= screen_info->width) &&
        (c->height >= screen_info->height) &&
        !FLAG_TEST(c->xfwm_flags, XFWM_FLAG_HAS_BORDER) &&
        !FLAG_TEST (c->flags, CLIENT_FLAG_BELOW | CLIENT_FLAG_ABOVE | CLIENT_FLAG_FULLSCREEN) &&
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
       Initialize "old" fields once the position is ensured, to avoid
       initially maximized or fullscreen windows being placed offscreen
       once de-maximized
     */
    c->old_x = c->x;
    c->old_y = c->y;
    c->old_width = c->width;
    c->old_height = c->height;
    c->fullscreen_old_x = c->x;
    c->fullscreen_old_y = c->y;
    c->fullscreen_old_width = c->width;
    c->fullscreen_old_height = c->height;

    /*
       We must call clientApplyInitialState() after having placed the
       window so that the inital position values are correctly set if the
       inital state is maximize or fullscreen
     */
    clientApplyInitialState (c);

    valuemask = CWEventMask|CWBitGravity|CWWinGravity;
    attributes.event_mask = (FRAME_EVENT_MASK | POINTER_EVENT_MASK);
    attributes.win_gravity = StaticGravity;
    attributes.bit_gravity = StaticGravity;

#ifdef HAVE_RENDER
    if (display_info->have_render)
    {
        c->visual = attr.visual;
        c->depth  = attr.depth;
        attributes.colormap = attr.colormap;
        valuemask |= CWColormap;
    }
    else
    {
        c->visual = screen_info->visual;
        c->depth  = screen_info->depth;
    }

    if (c->depth == 32)
    {
        attributes.background_pixmap = None;
        attributes.border_pixel = 0;
        attributes.background_pixel = 0;
        valuemask |= CWBackPixmap|CWBackPixel|CWBorderPixel;
    }
#else  /* HAVE_RENDER */
    /* We don't support multiple depth/visual w/out render */
    c->visual = screen_info->visual;
    c->depth  = screen_info->depth;
#endif /* HAVE_RENDER */

    c->frame =
        XCreateWindow (display_info->dpy, screen_info->xroot, 0, 0, 1, 1, 0,
        c->depth, InputOutput, c->visual, valuemask, &attributes);

    XSelectInput (display_info->dpy, c->window, 0);
    XSetWindowBorderWidth (display_info->dpy, c->window, 0);
    if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        XUnmapWindow (display_info->dpy, c->window);
    }
    XReparentWindow (display_info->dpy, c->window, c->frame, frameLeft (c), frameTop (c));
    valuemask = CWEventMask;
    attributes.event_mask = (CLIENT_EVENT_MASK);
    XChangeWindowAttributes (display_info->dpy, c->window, valuemask, &attributes);
    XSelectInput (display_info->dpy, c->window, CLIENT_EVENT_MASK);
    if (display_info->have_shape)
    {
        XShapeSelectInput (display_info->dpy, c->window, ShapeNotifyMask);
    }

    clientAddToList (c);
    clientSetNetActions (c);
    clientGrabButtons(c);

    /* Initialize per client menu button pixmap */
    xfwmPixmapInit (screen_info, &c->appmenu[ACTIVE]);
    xfwmPixmapInit (screen_info, &c->appmenu[INACTIVE]);
    xfwmPixmapInit (screen_info, &c->appmenu[PRESSED]);

    xfwmWindowCreate (screen_info, c->visual, c->depth, c->frame,
        &c->sides[SIDE_LEFT],
        myDisplayGetCursorResize(screen_info->display_info, CORNER_COUNT + SIDE_LEFT));
    xfwmWindowCreate (screen_info,  c->visual, c->depth, c->frame,
        &c->sides[SIDE_RIGHT],
        myDisplayGetCursorResize(screen_info->display_info, CORNER_COUNT + SIDE_RIGHT));
    xfwmWindowCreate (screen_info,  c->visual, c->depth, c->frame,
        &c->sides[SIDE_BOTTOM],
        myDisplayGetCursorResize(screen_info->display_info, CORNER_COUNT + SIDE_BOTTOM));
    xfwmWindowCreate (screen_info,  c->visual, c->depth, c->frame,
        &c->corners[CORNER_BOTTOM_LEFT],
        myDisplayGetCursorResize(screen_info->display_info, CORNER_BOTTOM_LEFT));
    xfwmWindowCreate (screen_info,  c->visual, c->depth, c->frame,
        &c->corners[CORNER_BOTTOM_RIGHT],
        myDisplayGetCursorResize(screen_info->display_info, CORNER_BOTTOM_RIGHT));
    xfwmWindowCreate (screen_info,  c->visual, c->depth, c->frame,
        &c->corners[CORNER_TOP_LEFT],
        myDisplayGetCursorResize(screen_info->display_info, CORNER_TOP_LEFT));
    xfwmWindowCreate (screen_info,  c->visual, c->depth, c->frame,
        &c->corners[CORNER_TOP_RIGHT],
        myDisplayGetCursorResize(screen_info->display_info, CORNER_TOP_RIGHT));
    xfwmWindowCreate (screen_info,  c->visual, c->depth, c->frame,
        &c->title, None);
    for (i = 0; i < BUTTON_LAST; i++)
    {
        xfwmWindowCreate (screen_info,  c->visual, c->depth, c->frame,
            &c->buttons[i], None);
    }
    clientUpdateIconPix (c);

    /* Put the window on top to avoid XShape, that speeds up hw accelerated
       GL apps dramatically */
    XRaiseWindow (display_info->dpy, c->window);

    TRACE ("now calling configure for the new window \"%s\" (0x%lx)", c->name, c->window);
    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    clientConfigure (c, &wc, CWX | CWY | CWHeight | CWWidth, CFG_NOTIFY | CFG_FORCE_REDRAW);

    /* Notify the compositor about this new window */
    compositorAddWindow (display_info, c->frame, c);

    grabbed = FALSE;
    if (!FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
    {
        if ((c->win_workspace == screen_info->current_ws) ||
            FLAG_TEST(c->flags, CLIENT_FLAG_STICKY))
        {
            if (recapture)
            {
                clientRaise (c, None);
                clientShow (c, TRUE);
                clientSortRing(c);
            }
            else
            {
                clientFocusNew(c);
                grabbed = TRUE;
            }
        }
        else
        {
            clientRaise (c, None);
            clientInitFocusFlag (c);
        }
    }
    else
    {
        clientRaise (c, None);
        setWMState (display_info, c->window, IconicState);
        clientSetNetState (c);
    }

    if (!grabbed)
    {
        clientGrabMouseButton (c);
    }
    setNetFrameExtents (display_info, c->window, frameTop (c), frameLeft (c),
                                                 frameRight (c), frameBottom (c));

    /* Window is reparented now, so we can safely release the grab
     * on the server
     */
    myDisplayUngrabServer (display_info);
    gdk_error_trap_pop ();

    DBG ("client \"%s\" (0x%lx) is now managed", c->name, c->window);
    DBG ("client_count=%d", screen_info->client_count);

    return c;
}

void
clientUnframe (Client * c, gboolean remap)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    XEvent ev;
    int i;
    gboolean reparented;

    TRACE ("entering clientUnframe");
    TRACE ("unframing client \"%s\" (0x%lx) [%s]",
            c->name, c->window, remap ? "remap" : "no remap");

    g_return_if_fail (c != NULL);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    clientRemoveFromList (c);
    compositorSetClient (display_info, c->frame, NULL);

    myDisplayGrabServer (display_info);
    gdk_error_trap_push ();
    clientUngrabButtons (c);
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
            setWMState (display_info, c->window, WithdrawnState);
        }
    }

    if (!remap)
    {
        XDeleteProperty (display_info->dpy, c->window,
                         display_info->atoms[NET_WM_STATE]);
        XDeleteProperty (display_info->dpy, c->window,
                         display_info->atoms[WIN_STATE]);
        XDeleteProperty (display_info->dpy, c->window,
                         display_info->atoms[NET_WM_DESKTOP]);
        XDeleteProperty (display_info->dpy, c->window,
                         display_info->atoms[WIN_WORKSPACE]);
        XDeleteProperty (display_info->dpy, c->window,
                         display_info->atoms[WIN_LAYER]);
        XDeleteProperty (display_info->dpy, c->window,
                         display_info->atoms[NET_WM_ALLOWED_ACTIONS]);
    }

    xfwmWindowDelete (&c->title);
    xfwmWindowDelete (&c->sides[SIDE_LEFT]);
    xfwmWindowDelete (&c->sides[SIDE_RIGHT]);
    xfwmWindowDelete (&c->sides[SIDE_BOTTOM]);
    xfwmWindowDelete (&c->sides[CORNER_BOTTOM_LEFT]);
    xfwmWindowDelete (&c->sides[CORNER_BOTTOM_RIGHT]);
    xfwmWindowDelete (&c->sides[CORNER_TOP_LEFT]);
    xfwmWindowDelete (&c->sides[CORNER_TOP_RIGHT]);

    xfwmPixmapFree (&c->appmenu[ACTIVE]);
    xfwmPixmapFree (&c->appmenu[INACTIVE]);
    xfwmPixmapFree (&c->appmenu[PRESSED]);

    for (i = 0; i < BUTTON_LAST; i++)
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
    DisplayInfo *display_info;
    XWindowAttributes attr;
    xfwmWindow shield;
    Window w1, w2, *wins;
    unsigned int count, i;

    TRACE ("entering clientFrameAll");

    display_info = screen_info->display_info;
    clientSetFocus (screen_info, NULL, myDisplayGetCurrentTime (display_info), NO_FOCUS_FLAG);
    xfwmWindowTemp (screen_info,
                    NULL, 0,
                    screen_info->xroot,
                    &shield,
                    0, 0,
                    screen_info->width,
                    screen_info->height,
                    EnterWindowMask,
                    FALSE);

    XSync (display_info->dpy, FALSE);
    myDisplayGrabServer (display_info);
    XQueryTree (display_info->dpy, screen_info->xroot, &w1, &w2, &wins, &count);
    for (i = 0; i < count; i++)
    {
        XGetWindowAttributes (display_info->dpy, wins[i], &attr);
        if ((attr.map_state == IsViewable) && (attr.root == screen_info->xroot))
        {
            Client *c = clientFrame (display_info, wins[i], TRUE);
            if ((c) && ((screen_info->params->raise_on_click) || (screen_info->params->click_to_focus)))
            {
                clientGrabMouseButton (c);
            }
        }
        else
        {
             compositorAddWindow (display_info, wins[i], NULL);
        }
    }
    if (wins)
    {
        XFree (wins);
    }
    clientFocusTop (screen_info, WIN_LAYER_NORMAL, myDisplayGetCurrentTime (display_info));
    xfwmWindowDelete (&shield);
    myDisplayUngrabServer (display_info);
    XSync (display_info->dpy, FALSE);
}

void
clientUnframeAll (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;
    Client *c;
    Window w1, w2, *wins;
    unsigned int count, i;

    TRACE ("entering clientUnframeAll");

    display_info = screen_info->display_info;
    clientSetFocus (screen_info, NULL, myDisplayGetCurrentTime (display_info), FOCUS_IGNORE_MODAL);
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
    Client *c;
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
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);

    TRACE ("entering clientSetWorkspaceSingle");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (ws > screen_info->workspace_count - 1)
    {
        ws = screen_info->workspace_count - 1;
        TRACE ("value off limits, using %i instead", ws);
    }

    if (c->win_workspace != ws)
    {
        TRACE ("setting client \"%s\" (0x%lx) to current_ws %d", c->name, c->window, ws);
        c->win_workspace = ws;
        setHint (display_info, c->window, WIN_WORKSPACE, ws);
        if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
        {
            setHint (display_info, c->window, NET_WM_DESKTOP, (unsigned long) ALL_WORKSPACES);
        }
        else
        {
            setHint (display_info, c->window, NET_WM_DESKTOP, (unsigned long) ws);
        }
    }
    FLAG_SET (c->xfwm_flags, XFWM_FLAG_WORKSPACE_SET);
}

void
clientSetWorkspace (Client * c, int ws, gboolean manage_mapping)
{
    Client *c2;
    GList *list_of_windows;
    GList *index;
    int previous_ws;

    g_return_if_fail (c != NULL);

    TRACE ("entering clientSetWorkspace");

    list_of_windows = clientListTransientOrModal (c);
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;

        if (c2->win_workspace != ws)
        {
            TRACE ("setting client \"%s\" (0x%lx) to current_ws %d", c->name, c->window, ws);

            previous_ws = c2->win_workspace;
            clientSetWorkspaceSingle (c2, ws);

            if (manage_mapping && !clientIsValidTransientOrModal (c2) && !FLAG_TEST (c2->flags, CLIENT_FLAG_ICONIFIED))
            {
                if (previous_ws == c2->screen_info->current_ws)
                {
                    clientHide (c2, c2->screen_info->current_ws, FALSE);
                }
                if (FLAG_TEST (c2->flags, CLIENT_FLAG_STICKY) || (ws == c2->screen_info->current_ws))
                {
                    clientShow (c2, FALSE);
                }
            }
        }
    }
    g_list_free (list_of_windows);
}

static void
clientShowSingle (Client * c, gboolean change_state)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if ((c->win_workspace == screen_info->current_ws) || FLAG_TEST (c->flags, CLIENT_FLAG_STICKY))
    {
        TRACE ("showing client \"%s\" (0x%lx)", c->name, c->window);
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_VISIBLE);
        XMapWindow (display_info->dpy, c->frame);
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            XMapWindow (display_info->dpy, c->window);
        }
        /* Adjust to urgency state as the window is visible */
        clientUpdateUrgency (c);
    }
    if (change_state)
    {
        FLAG_UNSET (c->flags, CLIENT_FLAG_ICONIFIED);
        setWMState (display_info, c->window, NormalState);
    }
    clientSetNetState (c);
}

void
clientShow (Client * c, gboolean change_state)
{
    Client *c2;
    GList *list_of_windows;
    GList *index;

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

    /* Update working area as windows have been shown */
    workspaceUpdateArea (c->screen_info);
}

static void
clientHideSingle (Client * c, gboolean change_state)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    TRACE ("hiding client \"%s\" (0x%lx)", c->name, c->window);
    clientPassFocus(c->screen_info, c, c);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
    {
        FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_VISIBLE);
        c->ignore_unmap++;
        /* Adjust to urgency state as the window is not visible */
        clientUpdateUrgency (c);
    }
    XUnmapWindow (display_info->dpy, c->window);
    XUnmapWindow (display_info->dpy, c->frame);
    if (change_state)
    {
        FLAG_SET (c->flags, CLIENT_FLAG_ICONIFIED);
        setWMState (display_info, c->window, IconicState);
    }
    clientSetNetState (c);
}

void
clientHide (Client * c, int ws, gboolean change_state)
{
    Client *c2;
    GList *list_of_windows;
    GList *index;

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
        if (clientIsTransientOrModal (c2) &&
            clientTransientOrModalHasAncestor (c2, ws))
        {
            /* Other ancestors for that transient are still on screen, so don't
               hide it...
             */
            continue;
        }
        clientHideSingle (c2, change_state);
    }
    g_list_free (list_of_windows);

    /* Update working area as windows have been hidden */
    workspaceUpdateArea (c->screen_info);
}

void
clientHideAll (Client * c, int ws)
{
    ScreenInfo *screen_info;
    Client *c2;
    int i;

    g_return_if_fail (c != NULL);

    TRACE ("entering clientHideAll");

    screen_info = c->screen_info;
    for (c2 = c->next, i = 0; (c2) && (i < screen_info->client_count); c2 = c2->next, i++)
    {
        if (CLIENT_CAN_HIDE_WINDOW (c2)
            && !clientIsValidTransientOrModal (c2) && (c2 != c))
        {
            if (((!c) && (c2->win_workspace == ws))
                 || ((c) && !clientIsTransientOrModalFor (c, c2)
                         && (c2->win_workspace == c->win_workspace)))
            {
                clientHide (c2, ws, TRUE);
            }
        }
    }
}

void
clientClearAllShowDesktop (ScreenInfo *screen_info)
{
    TRACE ("entering clientClearShowDesktop");

    if (screen_info->show_desktop)
    {
        GList *index = NULL;

        for (index = screen_info->windows_stack; index; index = g_list_next (index))
        {
            Client *c = (Client *) index->data;
            FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_WAS_SHOWN);
        }
        screen_info->show_desktop = FALSE;
        sendRootMessage (screen_info, NET_SHOWING_DESKTOP, screen_info->show_desktop,
                         myDisplayGetCurrentTime (screen_info->display_info));
    }
}

void
clientToggleShowDesktop (ScreenInfo *screen_info)
{
    GList *index;

    TRACE ("entering clientToggleShowDesktop");

    clientSetFocus (screen_info, NULL,
                    myDisplayGetCurrentTime (screen_info->display_info),
                    FOCUS_IGNORE_MODAL);
    if (screen_info->show_desktop)
    {
        for (index = screen_info->windows_stack; index; index = g_list_next (index))
        {
            Client *c = (Client *) index->data;
            if ((c->type & WINDOW_REGULAR_FOCUSABLE) 
                && !FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED | CLIENT_FLAG_SKIP_TASKBAR))
            {
                FLAG_SET (c->xfwm_flags, XFWM_FLAG_WAS_SHOWN);
                clientHide (c, c->win_workspace, TRUE);
            }
        }
        clientFocusTop (screen_info, WIN_LAYER_DESKTOP, myDisplayGetCurrentTime (screen_info->display_info));
    }
    else
    {
        for (index = g_list_last(screen_info->windows_stack); index; index = g_list_previous (index))
        {
            Client *c = (Client *) index->data;
            if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_WAS_SHOWN))
            {
                clientShow (c, TRUE);
            }
            FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_WAS_SHOWN);
        }
        clientFocusTop (screen_info, WIN_LAYER_NORMAL, myDisplayGetCurrentTime (screen_info->display_info));
    }
}

void
clientActivate (Client * c, Time timestamp)
{
    ScreenInfo *screen_info;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientActivate \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    if ((screen_info->current_ws == c->win_workspace) || (screen_info->params->bring_on_activate))
    {
        clientSetWorkspace (c, screen_info->current_ws, TRUE);
        clientShow (c, TRUE);
        clientClearAllShowDesktop (screen_info);
        clientSetFocus (screen_info, c, timestamp, NO_FOCUS_FLAG);
        clientRaise (c, None);
    }
    else
    {
        TRACE ("Setting WM_STATE_DEMANDS_ATTENTION flag on \"%s\" (0x%lx)", c->name, c->window); 
        FLAG_SET (c->flags, CLIENT_FLAG_DEMANDS_ATTENTION);
        clientSetNetState (c);
    }
}

void
clientClose (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);

    TRACE ("entering clientClose");
    TRACE ("closing client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (FLAG_TEST (c->wm_flags, WM_FLAG_DELETE))
    {
        sendClientMessage (screen_info, c->window, WM_DELETE_WINDOW,
                           myDisplayGetCurrentTime (display_info));
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
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);

    TRACE ("entering clientEnterContextMenuState");
    TRACE ("Showing the what's this help for client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (FLAG_TEST (c->wm_flags, WM_FLAG_CONTEXT_HELP))
    {
        sendClientMessage (c->screen_info, c->window, NET_WM_CONTEXT_HELP,
                           myDisplayGetCurrentTime (display_info));
    }
}

void
clientSetLayer (Client * c, int l)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    GList *list_of_windows = NULL;
    GList *index = NULL;
    Client *c2 = NULL;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetLayer");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    list_of_windows = clientListTransientOrModal (c);
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c2 = (Client *) index->data;
        if (c2->win_layer != l)
        {
            TRACE ("setting client \"%s\" (0x%lx) layer to %d", c2->name,
                c2->window, l);
            c2->win_layer = l;
            setHint (display_info, c2->window, WIN_LAYER, l);
        }
    }
    g_list_free (list_of_windows);
    if (clientGetLastRaise (c->screen_info) == c)
    {
        clientClearLastRaise (c->screen_info);
    }
    clientRaise (c, None);
}

void
clientShade (Client * c)
{
    XWindowChanges wc;
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    unsigned long mask;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleShaded");
    TRACE ("shading client \"%s\" (0x%lx)", c->name, c->window);

    if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER)
        || FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE ("cowardly refusing to shade \"%s\" (0x%lx) because it has no border", c->name, c->window);
        return;
    }
    else if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        TRACE ("\"%s\" (0x%lx) is already shaded", c->name, c->window);
        return;
    }

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    c->win_state |= WIN_STATE_SHADED;
    FLAG_SET (c->flags, CLIENT_FLAG_SHADED);
    clientSetNetState (c);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MANAGED))
    {
        mask = (CWWidth | CWHeight);
        if (clientConstrainPos (c, FALSE))
        {
            wc.x = c->x;
            wc.y = c->y;
            mask |= (CWX | CWY);
        }
        
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
        {
            c->ignore_unmap++;
        }
        /*
         * Shading unmaps the client window. We therefore have to transfer focus to its frame
         * so that focus doesn't return to root. clientSetFocus() will take care of focusing
         * the window frame since the SHADED flag is now set.
         */
        if (c == clientGetFocus ())
        {
            clientSetFocus (screen_info, c, myDisplayGetCurrentTime (display_info), FOCUS_FORCE);
        }
        XUnmapWindow (display_info->dpy, c->window);

        wc.width = c->width;
        wc.height = c->height;
        clientConfigure (c, &wc, mask, CFG_FORCE_REDRAW);
    }
}

void
clientUnshade (Client * c)
{
    XWindowChanges wc;
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleShaded");
    TRACE ("shading/unshading client \"%s\" (0x%lx)", c->name, c->window);

    if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
    {
        TRACE ("\"%s\" (0x%lx) is not shaded", c->name, c->window);
        return;
    }

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    c->win_state &= ~WIN_STATE_SHADED;
    FLAG_UNSET (c->flags, CLIENT_FLAG_SHADED);
    clientSetNetState (c);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MANAGED))
    {
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
        {
            XMapWindow (display_info->dpy, c->window);
        }
        /*
         * Unshading will show the client window, so we need to focus it when unshading.
         */
        if (c == clientGetFocus ())
        {
            clientSetFocus (screen_info, c, myDisplayGetCurrentTime (display_info), FOCUS_FORCE);
        }

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
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Client *c2;
    GList *list_of_windows;
    GList *index;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientStick");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (include_transients)
    {
        list_of_windows = clientListTransientOrModal (c);
        for (index = list_of_windows; index; index = g_list_next (index))
        {
            c2 = (Client *) index->data;
            TRACE ("sticking client \"%s\" (0x%lx)", c2->name, c2->window);
            c2->win_state |= WIN_STATE_STICKY;
            FLAG_SET (c2->flags, CLIENT_FLAG_STICKY);
            setHint (display_info, c2->window, NET_WM_DESKTOP, (unsigned long) ALL_WORKSPACES);
            clientSetNetState (c2);
        }
        clientSetWorkspace (c, screen_info->current_ws, TRUE);
        g_list_free (list_of_windows);
    }
    else
    {
        TRACE ("sticking client \"%s\" (0x%lx)", c->name, c->window);
        c->win_state |= WIN_STATE_STICKY;
        FLAG_SET (c->flags, CLIENT_FLAG_STICKY);
        setHint (display_info, c->window, NET_WM_DESKTOP, (unsigned long) ALL_WORKSPACES);
        clientSetNetState (c);
        clientSetWorkspace (c, screen_info->current_ws, TRUE);
    }
}

void
clientUnstick (Client * c, gboolean include_transients)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Client *c2;
    GList *list_of_windows;
    GList *index;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUnstick");
    TRACE ("unsticking client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (include_transients)
    {
        list_of_windows = clientListTransientOrModal (c);
        for (index = list_of_windows; index; index = g_list_next (index))
        {
            c2 = (Client *) index->data;
            c2->win_state &= ~WIN_STATE_STICKY;
            FLAG_UNSET (c2->flags, CLIENT_FLAG_STICKY);
            setHint (display_info, c2->window, NET_WM_DESKTOP, (unsigned long) screen_info->current_ws);
            clientSetNetState (c2);
        }
        clientSetWorkspace (c, screen_info->current_ws, TRUE);
        g_list_free (list_of_windows);
    }
    else
    {
        c->win_state &= ~WIN_STATE_STICKY;
        FLAG_UNSET (c->flags, CLIENT_FLAG_STICKY);
        setHint (display_info, c->window, NET_WM_DESKTOP, (unsigned long) screen_info->current_ws);
        clientSetNetState (c);
        clientSetWorkspace (c, screen_info->current_ws, TRUE);
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

        monitor_nbr = find_monitor_at_point (c->screen_info->gscr, cx, cy);
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

    if (!FLAG_TEST (c->flags, CLIENT_FLAG_BELOW))
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

    if (!FLAG_TEST (c->flags, CLIENT_FLAG_ABOVE))
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
    frameDraw (c, FALSE);
    clientSetNetState (c);
}

static void
clientNewMaxState (Client * c, XWindowChanges *wc, int mode)
{
    /*
     * We treat differently full maximization from vertical or horizontal maximization.
     * This is for usability concerns, otherwise maximization acts like a toggle,
     * switching from horizontal to vertical instead of switching to full maximization.
     *
     * The full size is not computed yet, as full maximization removes borders
     * while either horizontal or vertical maximization still shows decorations...
     */

    if ((mode & WIN_STATE_MAXIMIZED) == WIN_STATE_MAXIMIZED)
    {
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED))
        {
            c->win_state |= WIN_STATE_MAXIMIZED;
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED);
            return;
        }
    }

    if (mode & WIN_STATE_MAXIMIZED_HORIZ)
    {
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ))
        {
            c->win_state |= WIN_STATE_MAXIMIZED_HORIZ;
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
            return;
        }
    }

    if (mode & WIN_STATE_MAXIMIZED_VERT)
    {
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT))
        {
            c->win_state |= WIN_STATE_MAXIMIZED_VERT;
            FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
            return;
        }
    }

    c->win_state &= ~WIN_STATE_MAXIMIZED;
    FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED);
    wc->x = c->old_x;
    wc->y = c->old_y;
    wc->width = c->old_width;
    wc->height = c->old_height;
}

static void
clientNewMaxSize (Client * c, XWindowChanges *wc)
{
    ScreenInfo *screen_info;
    GdkRectangle rect;
    int cx, cy, full_x, full_y, full_w, full_h;
    int tmp_x, tmp_y, tmp_w, tmp_h;
    gint monitor_nbr;

    tmp_x = frameX (c);
    tmp_y = frameY (c);
    tmp_h = frameHeight (c);
    tmp_w = frameWidth (c);

    cx = tmp_x + (tmp_w / 2);
    cy = tmp_y + (tmp_h / 2);

    screen_info = c->screen_info;

    monitor_nbr = find_monitor_at_point (screen_info->gscr, cx, cy);
    gdk_screen_get_monitor_geometry (screen_info->gscr, monitor_nbr, &rect);

    full_x = MAX (screen_info->params->xfwm_margins[STRUTS_LEFT], rect.x);
    full_y = MAX (screen_info->params->xfwm_margins[STRUTS_TOP], rect.y);
    full_w = MIN (screen_info->width - screen_info->params->xfwm_margins[STRUTS_RIGHT],
                  rect.x + rect.width) - full_x;
    full_h = MIN (screen_info->height - screen_info->params->xfwm_margins[STRUTS_BOTTOM],
                  rect.y + rect.height) - full_y;

    if (FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED))
    {
        /* Adjust size to the largest size available, not covering struts */
        clientMaxSpace (screen_info, &full_x, &full_y, &full_w, &full_h);
        wc->x = full_x + frameLeft (c);
        wc->y = full_y + frameTop (c);
        wc->width = full_w - frameLeft (c) - frameRight (c);
        wc->height = full_h - frameTop (c) - frameBottom (c);

        return;
    }

    if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ))
    {
        /* Adjust size to the widest size available, for the current vertical position/height */
        clientMaxSpace (screen_info, &full_x, &tmp_y, &full_w, &tmp_h);
        wc->x = full_x + frameLeft (c);
        wc->width = full_w - frameLeft (c) - frameRight (c);

        return;
    }

    if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT))
    {
        /* Adjust size to the tallest size available, for the current horizontal position/width */
        clientMaxSpace (screen_info, &tmp_x, &full_y, &tmp_w, &full_h);
        wc->y = full_y + frameTop (c);
        wc->height = full_h - frameTop (c) - frameBottom (c);

        return;
    }
}

void
clientToggleMaximized (Client * c, int mode, gboolean restore_position)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;

    XWindowChanges wc;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleMaximized");
    TRACE ("maximzing/unmaximizing client \"%s\" (0x%lx)", c->name, c->window);

    if (!CLIENT_CAN_MAXIMIZE_WINDOW (c))
    {
        return;
    }

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;

    if (!FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED) && (restore_position))
    {
        c->old_x = c->x;
        c->old_y = c->y;
        c->old_width = c->width;
        c->old_height = c->height;
    }

    /* 1) Compute the new state */
    clientNewMaxState (c, &wc, mode);

    /* 2) Compute the new size, based on the state */
    clientNewMaxSize (c, &wc);

    /* 3) Update window state fields */
    clientSetNetState (c);

    /* 4) Update size and position fields */
    c->x = wc.x;
    c->y = wc.y;
    c->height = wc.height;
    c->width = wc.width;

    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MANAGED) && (restore_position))
    {
        /*
           For some reason, the configure can generate EnterNotify events
           on lower windows, causing a nasty race cond with apps trying to
           grab focus in focus follow mouse mode. Grab the pointer to
           avoid these effects
         */
        myScreenGrabPointer (c->screen_info, EnterWindowMask, None, myDisplayGetCurrentTime (display_info));
        clientConfigure (c, &wc, CWWidth | CWHeight | CWX | CWY, CFG_FORCE_REDRAW);
        myScreenUngrabPointer (c->screen_info);
    }
}

void
clientUpdateOpacity (ScreenInfo *screen_info, Client *focus)
{
    DisplayInfo *display_info;
    Client *c;
    int i;

    display_info = screen_info->display_info;
    if (!compositorIsUsable (display_info))
    {
        return;
    }

    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, ++i)
    {
        gboolean o = FLAG_TEST(c->type, WINDOW_TYPE_DONT_PLACE | WINDOW_TYPE_DONT_FOCUS)
                     || (focus == c)
                     || (focus && ((focus->transient_for == c->window) || (focus->window == c->transient_for)))
                     || (focus && (clientIsModalFor (c, focus) || clientIsModalFor (focus, c)));

        clientSetOpacity (c, c->opacity, OPACITY_INACTIVE, o ? 0 : OPACITY_INACTIVE);
    }
}

void
clientSetOpacity (Client *c, guint opacity, guint clear, guint xor)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    guint applied;

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (!compositorIsUsable (display_info))
    {
        return;
    }

    c->opacity_flags = (c->opacity_flags & ~clear) ^ xor;

    if (c->opacity_locked)
    {
        applied = c->opacity;
    }
    else
    {
        long long multiplier = 1, divisor = 1;

        c->opacity = applied = opacity;

        if (FLAG_TEST (c->opacity_flags, OPACITY_MOVE))
        {
            multiplier *= c->screen_info->params->move_opacity;
            divisor *= 100;
        }
        if (FLAG_TEST (c->opacity_flags, OPACITY_RESIZE))
        {
            multiplier *= c->screen_info->params->resize_opacity;
            divisor *= 100;
        }
        if (FLAG_TEST (c->opacity_flags, OPACITY_INACTIVE))
        {
            multiplier *= c->screen_info->params->inactive_opacity;
            divisor *= 100;
        }

        applied = (guint) ((long long) applied * multiplier / divisor);
    }

    if (applied != c->opacity_applied)
    {
        c->opacity_applied = applied;
        compositorWindowSetOpacity (display_info, c->frame, applied);
    }
}

void
clientDecOpacity (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (!compositorIsUsable (display_info))
    {
        return;
    }

    if ((c->opacity > OPACITY_SET_MIN) && !(c->opacity_locked ))
    {
         clientSetOpacity (c, c->opacity - OPACITY_SET_STEP, 0, 0);
    }
}

void
clientIncOpacity (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (!compositorIsUsable (display_info))
    {
        return;
    }

    if ((c->opacity < NET_WM_OPAQUE) && !(c->opacity_locked ))
    {
         guint opacity = c->opacity + OPACITY_SET_STEP;

         if (opacity < OPACITY_SET_MIN)
         {
             opacity = NET_WM_OPAQUE;
         }
         clientSetOpacity (c, opacity, 0, 0);
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

    /* Revalidate client struts */
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        c = (Client *) index->data;
        if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUT))
        {
            clientValidateNetStrut (c);
        }
    }

    myScreenGrabPointer (screen_info, EnterWindowMask, None, myDisplayGetCurrentTime (screen_info->display_info));
    for (index = list_of_windows; index; index = g_list_next (index))
    {
        unsigned long maximization_flags = 0L;

        c = (Client *) index->data;
        if (!CONSTRAINED_WINDOW (c))
        {
            continue;
        }

        /* Recompute size and position of maximized windows */
        if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ | CLIENT_FLAG_MAXIMIZED_VERT))
        {
             /* Too bad, the flags used internally are different from the WIN_STATE_* bits */
             maximization_flags |= FLAG_TEST (c->flags,
                 CLIENT_FLAG_MAXIMIZED_HORIZ) ? WIN_STATE_MAXIMIZED_HORIZ : 0;
             maximization_flags |= FLAG_TEST (c->flags,
                 CLIENT_FLAG_MAXIMIZED_VERT) ? WIN_STATE_MAXIMIZED_VERT : 0;

             /* Force an update by clearing the internal flags */
             FLAG_UNSET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ | CLIENT_FLAG_MAXIMIZED_VERT);
             clientToggleMaximized (c, maximization_flags, FALSE);

             wc.x = c->x;
             wc.y = c->y;
             wc.width = c->width;
             wc.height = c->height;
             clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, CFG_NOTIFY);
        }
        else
        {
             wc.x = c->x;
             wc.y = c->y;
             clientConfigure (c, &wc, CWX | CWY, CFG_CONSTRAINED | CFG_REQUEST);
        }
    }
    myScreenUngrabPointer (screen_info);

    g_list_free (list_of_windows);
}

static void
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
clientSnapPosition (Client * c, int prev_x, int prev_y)
{
    ScreenInfo *screen_info;
    Client *c2;
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

    monitor_nbr = find_monitor_at_point (screen_info->gscr, cx, cy);
    gdk_screen_get_monitor_geometry (screen_info->gscr, monitor_nbr, &rect);

    disp_x = rect.x;
    disp_y = rect.y;
    disp_max_x = rect.x + rect.width;
    disp_max_y = rect.y + rect.height;

    if (screen_info->params->snap_to_border)
    {
        if (abs (disp_x - frame_x) < abs (disp_max_x - frame_x2))
        {
            if (!screen_info->params->snap_resist || ((frame_x <= disp_x) && (c->x < prev_x)))
            {
                best_delta_x = abs (disp_x - frame_x);
                best_frame_x = disp_x;
            }
        }
        else
        {
            if (!screen_info->params->snap_resist || ((frame_x2 >= disp_max_x) && (c->x > prev_x)))
            {
                best_delta_x = abs (disp_max_x - frame_x2);
                best_frame_x = disp_max_x - frame_width;
            }
        }

        if (abs (disp_y - frame_y) < abs (disp_max_y - frame_y2))
        {
            if (!screen_info->params->snap_resist || ((frame_y <= disp_y) && (c->y < prev_y)))
            {
                best_delta_y = abs (disp_y - frame_y);
                best_frame_y = disp_y;
            }
        }
        else
        {
            if (!screen_info->params->snap_resist || ((frame_y2 >= disp_max_y) && (c->y > prev_y)))
            {
                best_delta_y = abs (disp_max_y - frame_y2);
                best_frame_y = disp_max_y - frame_height;
            }
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
                    if (!screen_info->params->snap_resist || ((frame_x <= c_frame_x2) && (c->x < prev_x)))
                    {
                        best_delta_x = delta;
                        best_frame_x = c_frame_x2;
                    }
                }

                delta = abs (c_frame_x1 - frame_x2);
                if (delta < best_delta_x)
                {
                    if (!screen_info->params->snap_resist || ((frame_x2 >= c_frame_x1) && (c->x > prev_x)))
                    {
                        best_delta_x = delta;
                        best_frame_x = c_frame_x1 - frame_width;
                    }
                }
            }

            if ((c_frame_x1 <= frame_x2) && (c_frame_x2 >= frame_x))
            {
                delta = abs (c_frame_y2 - frame_y);
                if (delta < best_delta_y)
                {
                    if (!screen_info->params->snap_resist || ((frame_y <= c_frame_y2) && (c->y < prev_y)))
                    {
                        best_delta_y = delta;
                        best_frame_y = c_frame_y2;
                    }
                }

                delta = abs (c_frame_y1 - frame_y2);
                if (delta < best_delta_y)
                {
                    if (!screen_info->params->snap_resist || ((frame_y2 >= c_frame_y1) && (c->y > prev_y)))
                    {
                        best_delta_y = delta;
                        best_frame_y = c_frame_y1 - frame_height;
                    }
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

static eventFilterStatus
clientButtonReleaseFilter (XEvent * xevent, gpointer data)
{
    MoveResizeData *passdata = (MoveResizeData *) data;

    TRACE ("entering clientButtonReleaseFilter");

    if ((xevent->type == ButtonRelease) &&
        (xevent->xbutton.button == passdata->button))
    {
        gtk_main_quit ();
        return EVENT_FILTER_STOP;
    }

    return EVENT_FILTER_CONTINUE;
}

static eventFilterStatus
clientMoveEventFilter (XEvent * xevent, gpointer data)
{
    static int edge_scroll_x = 0;
    static int edge_scroll_y = 0;
    static gboolean toggled_maximize = FALSE;
    static Time lastresist = (Time) 0;
    unsigned long configure_flags;
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    eventFilterStatus status = EVENT_FILTER_STOP;
    MoveResizeData *passdata = (MoveResizeData *) data;
    Client *c = NULL;
    gboolean moving = TRUE;
    gboolean warp_pointer = FALSE;
    XWindowChanges wc;
    int prev_x, prev_y;

    TRACE ("entering clientMoveEventFilter");

    c = passdata->c;
    prev_x=c->x;
    prev_y=c->y;
    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    configure_flags = NO_CFG_FLAG;

    /* Update the display time */
    myDisplayUpdateCurrentTime (display_info, xevent);

    if (xevent->type == KeyPress)
    {
        while (XCheckMaskEvent (display_info->dpy, KeyPressMask, xevent))
        {
            /* Update the display time */
            myDisplayUpdateCurrentTime (display_info, xevent);
        }

        if ((passdata->use_keys) && !FLAG_TEST_ALL(c->flags, CLIENT_FLAG_MAXIMIZED))
        {
            int key_move = 16;
            unsigned int edge;

            if ((screen_info->params->snap_to_border) || (screen_info->params->snap_to_windows))
            {
                key_move = MAX (16, screen_info->params->snap_width + 1);
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
            if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_LEFT].keycode)
            {
                c->x = c->x - key_move;
            }
            else if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_RIGHT].keycode)
            {
                c->x = c->x + key_move;
            }
            else if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_UP].keycode)
            {
                c->y = c->y - key_move;
            }
            else if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_DOWN].keycode)
            {
                c->y = c->y + key_move;
            }

            clientSnapPosition (c, prev_x, prev_y);
            edge = clientConstrainPos (c, FALSE);
            if ((edge) && (screen_info->params->wrap_windows))
            {
                int maxx, maxy, maxw, maxh;

                maxx = 0;
                maxy = 0;
                maxw = screen_info->width;
                maxh = screen_info->height;
                clientMaxSpace (screen_info, &maxx, &maxy, &maxw, &maxh);

                if (edge & CLIENT_CONSTRAINED_TOP)
                {
                    if (workspaceMove (screen_info, -1, 0, c, xevent->xkey.time))
                    {
                        c->y = maxy + maxh;
                    }
                }
                else if (edge & CLIENT_CONSTRAINED_BOTTOM)
                {
                    if (workspaceMove (screen_info, 1, 0, c, xevent->xkey.time))
                    {
                        c->y = maxy + frameTop (c);
                    }
                }

                if (edge & CLIENT_CONSTRAINED_LEFT)
                {
                    if (workspaceMove (screen_info, 0, -1, c, xevent->xkey.time))
                    {
                        c->x = maxx + maxw - frameWidth (c) + frameRight (c);
                    }
                }
                else if (edge & CLIENT_CONSTRAINED_RIGHT)
                {
                    if (workspaceMove (screen_info, 0, 1, c, xevent->xkey.time))
                    {
                        c->x = maxx + frameLeft (c);
                    }
                }
            }
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
                clientConfigure (c, &wc, CWX | CWY, configure_flags);
            }
        }
    }
    else if (xevent->type == KeyRelease)
    {
        if (xevent->xkey.keycode == screen_info->params->keys[KEY_CANCEL].keycode)
        {
            moving = FALSE;
            passdata->released = passdata->use_keys;

            if (screen_info->params->box_move)
            {
                clientDrawOutline (c);
            }

            c->x = passdata->cancel_x;
            c->y = passdata->cancel_y;

            if (screen_info->params->box_move)
            {
                clientDrawOutline (c);
            }
            else
            {
                wc.x = c->x;
                wc.y = c->y;
                clientConfigure (c, &wc, CWX | CWY, configure_flags);
            }
            if (screen_info->current_ws != passdata->cancel_workspace)
            {
                workspaceSwitch (screen_info, passdata->cancel_workspace, c, FALSE, xevent->xkey.time);
            }
            if (toggled_maximize)
            {
                toggled_maximize = FALSE;
                clientToggleMaximized (c, WIN_STATE_MAXIMIZED, FALSE);
                configure_flags = CFG_FORCE_REDRAW;
                passdata->move_resized = TRUE;
            }
        }
        else if (passdata->use_keys)
        {
            if (IsModifierKey (XLookupKeysym (&xevent->xkey, 0)))
            {
                moving = FALSE;
            }
        }
    }
    else if (xevent->type == ButtonRelease)
    {
        if (!passdata->use_keys)
        {
            moving = FALSE;
            passdata->released = (xevent->xbutton.button == passdata->button);
        }
    }
    else if (xevent->type == MotionNotify)
    {
        while (XCheckMaskEvent (display_info->dpy, ButtonMotionMask, xevent))
        {
            /* Update the display time */
            myDisplayUpdateCurrentTime (display_info, xevent);
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
        if ((screen_info->workspace_count > 1) && !(passdata->is_transient))
        {
            if ((screen_info->params->wrap_windows) && (screen_info->params->wrap_resistance))
            {
                int msx, msy, maxx, maxy;
                int rx, ry;

                msx = xevent->xmotion.x_root;
                msy = xevent->xmotion.y_root;
                maxx = screen_info->width - 1;
                maxy = screen_info->height - 1;
                rx = 0;
                ry = 0;
                warp_pointer = FALSE;

                if ((msx == 0) || (msx == maxx))
                {
                    if ((xevent->xmotion.time - lastresist) > 250)  /* ms */
                    {
                        edge_scroll_x = 0;
                    }
                    else
                    {
                        edge_scroll_x++;
                    }
                    if (msx == 0)
                    {
                        rx = 1;
                    }
                    else
                    {
                        rx = -1;
                    }
                    warp_pointer = TRUE;
                    lastresist = xevent->xmotion.time;
                }
                if ((msy == 0) || (msy == maxy))
                {
                    if ((xevent->xmotion.time - lastresist) > 250)  /* ms */
                    {
                        edge_scroll_y = 0;
                    }
                    else
                    {
                        edge_scroll_y++;
                    }
                    if (msy == 0)
                    {
                        ry = 1;
                    }
                    else
                    {
                        ry = -1;
                    }
                    warp_pointer = TRUE;
                    lastresist = xevent->xmotion.time;
                }

                if (edge_scroll_x > screen_info->params->wrap_resistance)
                {
                    edge_scroll_x = 0;
                    if ((msx == 0) || (msx == maxx))
                    {
                        if (msx == 0)
                        {
                            if (workspaceMove (screen_info, 0, -1, c, xevent->xmotion.time))
                            {
                                rx = 4 * maxx / 5;
                            }
                        }
                        else
                        {
                            if (workspaceMove (screen_info, 0, 1, c, xevent->xmotion.time))
                            {
                                rx = -4 * maxx / 5;
                            }
                        }
                        warp_pointer = TRUE;
                    }
                    lastresist = (Time) 0;
                }
                if (edge_scroll_y > screen_info->params->wrap_resistance)
                {
                    edge_scroll_y = 0;
                    if ((msy == 0) || (msy == maxy))
                    {
                        if (msy == 0)
                        {
                            if (workspaceMove (screen_info, -1, 0, c, xevent->xmotion.time))
                            {
                                ry = 4 * maxy / 5;
                            }
                        }
                        else
                        {
                            if (workspaceMove (screen_info, 1, 0, c, xevent->xmotion.time))
                            {
                                ry = -4 * maxy / 5;
                            }
                        }
                        warp_pointer = TRUE;
                    }
                    lastresist = (Time) 0;
                }

                if (warp_pointer)
                {
                    XWarpPointer (display_info->dpy, None, None, 0, 0, 0, 0, rx, ry);
                    XFlush (display_info->dpy);
                    msx += rx;
                    msy += ry;
                }

                xevent->xmotion.x_root = msx;
                xevent->xmotion.y_root = msy;
            }
        }

        if (FLAG_TEST_ALL(c->flags, CLIENT_FLAG_MAXIMIZED)
            && (screen_info->params->restore_on_move))
        {
            if (xevent->xmotion.y_root - passdata->my > 15)
            {
                /* to keep the distance from the edges of the window proportional. */
                double xratio, yratio;

                xratio = (xevent->xmotion.x_root - c->x)/(double)c->width;
                yratio = (xevent->xmotion.y_root - c->y)/(double)c->width;

                clientToggleMaximized (c, WIN_STATE_MAXIMIZED, FALSE);
                passdata->move_resized = TRUE;
                passdata->ox = c->x;
                passdata->mx = CLAMP(c->x + c->width * xratio, c->x, c->x + c->width);
                passdata->oy = c->y;
                passdata->my = c->y - frameTop(c) / 2;
                configure_flags = CFG_FORCE_REDRAW;
                toggled_maximize = TRUE;
            }
            else
            {
                xevent->xmotion.x_root = c->x - passdata->ox + passdata->mx;
                xevent->xmotion.y_root = c->y - passdata->oy + passdata->my;
            }
        }

        c->x = passdata->ox + (xevent->xmotion.x_root - passdata->mx);
        c->y = passdata->oy + (xevent->xmotion.y_root - passdata->my);

        clientSnapPosition (c, prev_x, prev_y);
        if (screen_info->params->restore_on_move)
        {
            if ((clientConstrainPos (c, FALSE) & CLIENT_CONSTRAINED_TOP) && toggled_maximize)
            {
                clientToggleMaximized (c, WIN_STATE_MAXIMIZED, FALSE);
                configure_flags = CFG_FORCE_REDRAW;
                toggled_maximize = FALSE;
                passdata->move_resized = TRUE;
                /*
                   Update "passdata->my" to the current value to
                   allow "restore on move" to keep working next time
                 */
                passdata->my = c->y - frameTop(c) / 2;
            }
        }
        else
        {
            clientConstrainPos(c, FALSE);
        }

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
            int changes = CWX | CWY;

            if (passdata->move_resized)
            {
                wc.width = c->width;
                wc.height = c->height;
                changes |= CWWidth | CWHeight;
                passdata->move_resized = FALSE;
            }

            wc.x = c->x;
            wc.y = c->y;
            clientConfigure (c, &wc, changes, configure_flags);
        }
    }
    else if ((xevent->type == UnmapNotify) && (xevent->xunmap.window == c->window))
    {
        moving = FALSE;
    }
    else if ((xevent->type == EnterNotify) || (xevent->type == LeaveNotify))
    {
        /* Ignore enter/leave events */
    }
    else
    {
        status = EVENT_FILTER_CONTINUE;
    }

    TRACE ("leaving clientMoveEventFilter");

    if (!moving)
    {
        TRACE ("event loop now finished");
        edge_scroll_x = 0;
        edge_scroll_y = 0;
        toggled_maximize = FALSE;
        gtk_main_quit ();
    }

    return status;
}

void
clientMove (Client * c, XEvent * ev)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    XWindowChanges wc;
    MoveResizeData passdata;
    int changes;
    gboolean g1, g2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientDoMove");
    TRACE ("moving client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    changes = CWX | CWY;

    passdata.c = c;
    passdata.cancel_x = passdata.ox = c->x;
    passdata.cancel_y = passdata.oy = c->y;
    passdata.cancel_workspace = c->win_workspace;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    passdata.released = FALSE;
    passdata.button = ev->xbutton.button;
    passdata.is_transient = clientIsValidTransientOrModal (c);
    passdata.move_resized = FALSE;

    if (ev->type == KeyPress)
    {
        passdata.released = passdata.use_keys = TRUE;
        passdata.mx = ev->xkey.x_root;
        passdata.my = ev->xkey.y_root;
    }
    else if (ev->type == ButtonPress)
    {
        passdata.mx = ev->xbutton.x_root;
        passdata.my = ev->xbutton.y_root;
    }
    else
    {
        getMouseXY (screen_info, screen_info->xroot, &passdata.mx, &passdata.my);
    }

    g1 = myScreenGrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
    g2 = myScreenGrabPointer (screen_info, ButtonMotionMask | ButtonReleaseMask,
                              myDisplayGetCursorMove (display_info), 
                              myDisplayGetCurrentTime (display_info));
    if (!g1 || !g2)
    {
        TRACE ("grab failed in clientMove");

        gdk_beep ();
        myScreenUngrabKeyboard (screen_info);
        myScreenUngrabPointer (screen_info);

        return;
    }

    passdata.poswin = NULL;
#ifdef SHOW_POSITION
    passdata.poswin = poswinCreate(screen_info->gscr);
    poswinSetPosition (passdata.poswin, c);
    poswinShow (passdata.poswin);
#endif /* SHOW_POSITION */

    /* Set window translucent while moving, looks nice */
    if ((screen_info->params->move_opacity < 100) && !(screen_info->params->box_move) && !(c->opacity_locked))
    {
        clientSetOpacity (c, c->opacity, OPACITY_MOVE, OPACITY_MOVE);
    }

    /*
     * Need to remove the sidewalk windows while moving otherwise
     * the motion events aren't reported on screen edges
     */
    placeSidewalks(screen_info, FALSE);

    FLAG_SET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);
    TRACE ("entering move loop");
    eventFilterPush (display_info->xfilter, clientMoveEventFilter, &passdata);
    if (passdata.use_keys)
    {
        XPutBackEvent (display_info->dpy, ev);
    }
    gtk_main ();
    eventFilterPop (display_info->xfilter);
    TRACE ("leaving move loop");
    FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);

    /* Put back the sidewalks as they ought to be */
    placeSidewalks(screen_info, screen_info->params->wrap_workspaces);

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
    /* Set window opacity to its original value */
    clientSetOpacity (c, c->opacity, OPACITY_MOVE, 0);

    wc.x = c->x;
    wc.y = c->y;
    if (passdata.move_resized)
    {
        wc.width = c->width;
        wc.height = c->height;
        changes |= CWWidth | CWHeight;
    }
    clientConfigure (c, &wc, changes, NO_CFG_FLAG);

    myScreenUngrabKeyboard (screen_info);
    if (!passdata.released)
    {
        /* If this is a drag-move, wait for the button to be released.
         * If we don't, we might get release events in the wrong place.
         */
        eventFilterPush (display_info->xfilter, clientButtonReleaseFilter, &passdata);
        gtk_main ();
        eventFilterPop (display_info->xfilter);
    }
    myScreenUngrabPointer (screen_info);

    if (passdata.grab && screen_info->params->box_move)
    {
        myDisplayUngrabServer (display_info);
    }
}

static void
clientResizeConfigure (Client *c, int px, int py, int pw, int ph)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    XWindowChanges wc;
    unsigned long value_mask;

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    value_mask = 0L;
    if (c->x != px)
    {
        value_mask |= CWX;
    }
    if (c->y != py)
    {
        value_mask |= CWY;
    }
    if (c->width != pw)
    {
        value_mask |= CWWidth;
    }
    if (c->height != ph)
    {
        value_mask |= CWHeight;
    }
    if (!value_mask)
    {
        return;
    }

#ifdef HAVE_XSYNC
    if (!c->xsync_waiting)
    {
        if ((display_info->have_xsync) && (c->xsync_enabled) && (c->xsync_counter)
            && (value_mask & (CWWidth | CWHeight)))
        {
            clientXSyncRequest (c);
        }
#endif /* HAVE_XSYNC */
        wc.x = c->x;
        wc.y = c->y;
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure (c, &wc, value_mask, NO_CFG_FLAG);
#ifdef HAVE_XSYNC
    }
#endif /* HAVE_XSYNC */
}

static eventFilterStatus
clientResizeEventFilter (XEvent * xevent, gpointer data)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Client *c;
    GdkRectangle rect;
    MoveResizeData *passdata;
    eventFilterStatus status;
    int prev_x, prev_y, prev_width, prev_height;
    int cx, cy, disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_x, frame_y, frame_height, frame_width;
    int frame_top, frame_left, frame_right, frame_bottom;
    int move_top, move_bottom, move_left, move_right;
    int temp;
    gint monitor_nbr;
    gint min_visible;
    gboolean resizing;

    TRACE ("entering clientResizeEventFilter");

    passdata = (MoveResizeData *) data;
    c = passdata->c;
    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    status = EVENT_FILTER_STOP;
    resizing = TRUE;

    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_height = frameHeight (c);
    frame_width = frameWidth (c);
    frame_top = frameTop (c);
    frame_left = frameLeft (c);
    frame_right = frameRight (c);
    frame_bottom = frameBottom (c);
    min_visible = MAX (frame_top, CLIENT_MIN_VISIBLE);

    cx = frame_x + (frame_width / 2);
    cy = frame_y + (frame_height / 2);

    move_top = ((passdata->corner == CORNER_TOP_RIGHT)
            || (passdata->corner == CORNER_TOP_LEFT)
            || (passdata->corner == CORNER_COUNT + SIDE_TOP)) ?
        1 : 0;
    move_bottom = ((passdata->corner == CORNER_BOTTOM_RIGHT)
            || (passdata->corner == CORNER_BOTTOM_LEFT)
            || (passdata->corner == CORNER_COUNT + SIDE_BOTTOM)) ?
        1 : 0;
    move_right = ((passdata->corner == CORNER_TOP_RIGHT)
            || (passdata->corner == CORNER_BOTTOM_RIGHT)
            || (passdata->corner == CORNER_COUNT + SIDE_RIGHT)) ?
        1 : 0;
    move_left = ((passdata->corner == CORNER_TOP_LEFT)
            || (passdata->corner == CORNER_BOTTOM_LEFT)
            || (passdata->corner == CORNER_COUNT + SIDE_LEFT)) ?
        1 : 0;

    monitor_nbr = find_monitor_at_point (screen_info->gscr, cx, cy);
    gdk_screen_get_monitor_geometry (screen_info->gscr, monitor_nbr, &rect);

    disp_x = rect.x;
    disp_y = rect.y;
    disp_max_x = rect.x + rect.width;
    disp_max_y = rect.y + rect.height;

    /* Store previous values in case the resize puts the window title off bounds */
    prev_x = c->x;
    prev_y = c->y;
    prev_width = c->width;
    prev_height = c->height;

    /* Update the display time */
    myDisplayUpdateCurrentTime (display_info, xevent);

    if (xevent->type == KeyPress)
    {
        while (XCheckMaskEvent (display_info->dpy, KeyPressMask, xevent))
        {
            /* Update the display time */
            myDisplayUpdateCurrentTime (display_info, xevent);
        }

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

            if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
                && (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_UP].keycode))
            {
                c->height = c->height - key_height_inc;
                corner = CORNER_COUNT + SIDE_BOTTOM;
            }
            else if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
                && (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_DOWN].keycode))
            {
                c->height = c->height + key_height_inc;
                corner = CORNER_COUNT + SIDE_BOTTOM;
            }
            else if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_LEFT].keycode)
            {
                c->width = c->width - key_width_inc;
                corner = CORNER_COUNT + SIDE_RIGHT;
            }
            else if (xevent->xkey.keycode == screen_info->params->keys[KEY_MOVE_RIGHT].keycode)
            {
                c->width = c->width + key_width_inc;
                corner = CORNER_COUNT + SIDE_RIGHT;
            }
            if (corner >= 0)
            {
                clientConstrainRatio (c, &c->width, &c->height, corner);
            }
            if ((c->x + c->width < disp_x + min_visible)
                || (c->x + c->width < screen_info->margins [STRUTS_LEFT] + min_visible))
            {
                c->width = prev_width;
            }
            if ((c->y + c->height < disp_y + min_visible)
                || (c->y + c->height < screen_info->margins [STRUTS_TOP] + min_visible))
            {
                c->height = prev_height;
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
                clientResizeConfigure (c, prev_x, prev_y, prev_width, prev_height);
            }
        }
    }
    else if (xevent->type == KeyRelease)
    {
        if (xevent->xkey.keycode == screen_info->params->keys[KEY_CANCEL].keycode)
        {
            resizing = FALSE;
            passdata->released = passdata->use_keys;

            if (screen_info->params->box_resize)
            {
                clientDrawOutline (c);
            }

            /* restore the pre-resize position & size */
            if (move_left)
            {
                c->x += c->width - passdata->cancel_x;
            }
            if (move_top)
            {
                c->y += c->height - passdata->cancel_y;
            }
            c->width = passdata->cancel_x;
            c->height = passdata->cancel_y;
            if (screen_info->params->box_resize)
            {
                clientDrawOutline (c);
            }
            else
            {
                clientResizeConfigure (c, prev_x, prev_y, prev_width, prev_height);
            }
        }
        else if (passdata->use_keys)
        {
            if (IsModifierKey (XLookupKeysym (&xevent->xkey, 0)))
            {
                resizing = FALSE;
            }
        }
    }
    else if (xevent->type == MotionNotify)
    {
        while (XCheckMaskEvent (display_info->dpy, ButtonMotionMask | PointerMotionMask, xevent))
        {
            /* Update the display time */
            myDisplayUpdateCurrentTime (display_info, xevent);
        }

        if (xevent->type == ButtonRelease)
        {
            resizing = FALSE;
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

        if (move_left)
        {
            c->width = passdata->ox - (xevent->xmotion.x_root - passdata->mx);
        }
        else if (move_right)
        {
            c->width = passdata->ox + (xevent->xmotion.x_root - passdata->mx);
        }
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            if (move_top)
            {
                c->height = passdata->oy - (xevent->xmotion.y_root - passdata->my);
            }
            else if (move_bottom)
            {
                c->height = passdata->oy + (xevent->xmotion.y_root - passdata->my);
            }
        }
        clientConstrainRatio (c, &c->width, &c->height, passdata->corner);

        clientSetWidth (c, c->width);
        if (move_left)
        {
            c->x = c->x - (c->width - passdata->oldw);
            frame_x = frameX (c);
        }

        clientSetHeight (c, c->height);
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED) && move_top)
        {
            c->y = c->y - (c->height - passdata->oldh);
            frame_y = frameY (c);
        }

        if (move_top)
        {
            if ((c->y > MAX (disp_max_y - min_visible, screen_info->height - screen_info->margins [STRUTS_BOTTOM] - min_visible))
                || (!clientCkeckTitle (c) && (frame_y < screen_info->margins [STRUTS_TOP])))
            {
                temp = c->y + c->height;
                c->y = CLAMP (c->y, screen_info->margins [STRUTS_TOP] + frame_top, 
                         MAX (disp_max_y - min_visible,  screen_info->height - screen_info->margins [STRUTS_BOTTOM] - min_visible));
                clientSetHeight (c, temp - c->y);
                c->y = temp - c->height;
            }
            else if (frame_y < 0)
            {
                temp = c->y + c->height;
                c->y = frame_top;
                clientSetHeight (c, temp - c->y);
                c->y = temp - c->height;
            }
        }
        else if (move_bottom)
        {
            if (c->y + c->height < MAX (disp_y + min_visible, screen_info->margins [STRUTS_TOP] + min_visible))
            {
                temp = MAX (disp_y + min_visible, screen_info->margins [STRUTS_TOP] + min_visible);
                clientSetHeight (c, temp - c->y);
            }
        }
        if (move_left)
        {
            if (c->x > MIN (disp_max_x - min_visible, screen_info->width - screen_info->margins [STRUTS_RIGHT] - min_visible))
            {
                temp = c->x + c->width;
                c->x = MIN (disp_max_x - min_visible, screen_info->width - screen_info->margins [STRUTS_RIGHT] - min_visible);
                clientSetWidth (c, temp - c->x);
                c->x = temp - c->width;
            }
        }
        else if (move_right)
        {
            if (c->x + c->width < MAX (disp_x + min_visible, screen_info->margins [STRUTS_LEFT] + min_visible))
            {
                temp = MAX (disp_x + min_visible, screen_info->margins [STRUTS_LEFT] + min_visible);
                clientSetWidth (c, temp - c->x);
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
            clientResizeConfigure (c, prev_x, prev_y, prev_width, prev_height);
        }

    }
    else if (xevent->type == ButtonRelease)
    {
        if (!passdata->use_keys)
        {
            resizing = FALSE;
            passdata->released = (xevent->xbutton.button == passdata->button);
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
        status = EVENT_FILTER_CONTINUE;
    }

    TRACE ("leaving clientResizeEventFilter");

    if (!resizing)
    {
        TRACE ("event loop now finished");
        gtk_main_quit ();
    }

    return status;
}

void
clientResize (Client * c, int corner, XEvent * ev)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    XWindowChanges wc;
    MoveResizeData passdata;
    int w_orig, h_orig;
    gboolean g1, g2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientResize");
    TRACE ("resizing client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
        && (screen_info->params->borderless_maximize))
    {
        return;
    }

    passdata.c = c;
    passdata.cancel_x = passdata.ox = c->width;
    passdata.cancel_y = passdata.oy = c->height;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    passdata.released = FALSE;
    passdata.button = ev->xbutton.button;
    passdata.corner = corner;
    w_orig = c->width;
    h_orig = c->height;

    if (ev->type == KeyPress)
    {
        passdata.released = passdata.use_keys = TRUE;
        passdata.mx = ev->xkey.x_root;
        passdata.my = ev->xkey.y_root;
    }
    else if (ev->type == ButtonPress)
    {
        passdata.mx = ev->xbutton.x_root;
        passdata.my = ev->xbutton.y_root;
    }
    else
    {
        getMouseXY (screen_info, screen_info->xroot, &passdata.mx, &passdata.my);
    }

    g1 = myScreenGrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
    g2 = myScreenGrabPointer (screen_info, ButtonMotionMask | ButtonReleaseMask,
                              myDisplayGetCursorResize(display_info, passdata.corner),
                              myDisplayGetCurrentTime (display_info));

    if (!g1 || !g2)
    {
        TRACE ("grab failed in clientResize");

        gdk_beep ();
        myScreenUngrabKeyboard (screen_info);
        myScreenUngrabPointer (screen_info);

        return;
    }

    passdata.poswin = NULL;
#ifndef SHOW_POSITION
    if ((c->size->width_inc > 1) || (c->size->height_inc > 1))
#endif /* SHOW_POSITION */
    {
        passdata.poswin = poswinCreate(screen_info->gscr);
        poswinSetPosition (passdata.poswin, c);
        poswinShow (passdata.poswin);
    }

#ifdef HAVE_XSYNC
    clientXSyncEnable (c);
#endif /* HAVE_XSYNC */

    /* Set window translucent while resizing, doesn't looks too nice  :( */
    if ((screen_info->params->resize_opacity < 100) && !(screen_info->params->box_resize) && !(c->opacity_locked))
    {
        clientSetOpacity (c, c->opacity, OPACITY_RESIZE, OPACITY_RESIZE);
    }

    FLAG_SET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);
    TRACE ("entering resize loop");
    eventFilterPush (display_info->xfilter, clientResizeEventFilter, &passdata);
    if (passdata.use_keys)
    {
        XPutBackEvent (display_info->dpy, ev);
    }
    gtk_main ();
    eventFilterPop (display_info->xfilter);
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
    /* Set window opacity to its original value */
    clientSetOpacity (c, c->opacity, OPACITY_RESIZE, 0);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED) &&
        ((w_orig != c->width) || (h_orig != c->height)))
    {
        clientRemoveMaximizeFlag (c);
    }

    wc.x = c->x;
    wc.y = c->y;
    wc.width = c->width;
    wc.height = c->height;
    clientConfigure (c, &wc, CWX | CWY | CWHeight | CWWidth, NO_CFG_FLAG);
#ifdef HAVE_XSYNC
    clientXSyncClearTimeout (c);
    c->xsync_waiting = FALSE;
#endif /* HAVE_XSYNC */

    myScreenUngrabKeyboard (screen_info);
    if (!passdata.released)
    {
        /* If this is a drag-resize, wait for the button to be released.
         * If we don't, we might get release events in the wrong place.
         */
        eventFilterPush (display_info->xfilter, clientButtonReleaseFilter, &passdata);
        gtk_main ();
        eventFilterPop (display_info->xfilter);
    }
    myScreenUngrabPointer (screen_info);

    if (passdata.grab && screen_info->params->box_resize)
    {
        myDisplayUngrabServer (display_info);
    }
}

static eventFilterStatus
clientCycleEventFilter (XEvent * xevent, gpointer data)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    ClientCycleData *passdata;
    Client *c, *removed;
    eventFilterStatus status;
    KeyCode cycle;
    KeyCode cancel;
    int modifier;
    gboolean key_pressed, cycling, gone;

    TRACE ("entering clientCycleEventFilter");

    passdata = (ClientCycleData *) data;
    if (passdata->c == NULL)
    {
        return EVENT_FILTER_CONTINUE;
        /*  will never be executed */
        /* gtk_main_quit (); */
    }

    c = passdata->c;
    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    cycle = screen_info->params->keys[KEY_CYCLE_WINDOWS].keycode;
    cancel = screen_info->params->keys[KEY_CANCEL].keycode;
    modifier = screen_info->params->keys[KEY_CYCLE_WINDOWS].modifier;
    key_pressed = ((xevent->type == KeyPress) && ((xevent->xkey.keycode == cycle) ||
                                                  (xevent->xkey.keycode == cancel)));
    status = EVENT_FILTER_STOP;
    cycling = TRUE;
    gone = FALSE;

    /* Update the display time */
    myDisplayUpdateCurrentTime (display_info, xevent);

    switch (xevent->type)
    {
        case DestroyNotify:
            removed = clientGetFromWindow (screen_info, ((XDestroyWindowEvent *) xevent)->window, WINDOW);
            gone |= (c == removed);
            c = tabwinRemoveClient(passdata->tabwin, removed);
            passdata->c = c;
            status = EVENT_FILTER_CONTINUE;
            /* Walk through */
        case UnmapNotify:
            removed = clientGetFromWindow (screen_info, ((XUnmapEvent *) xevent)->window, WINDOW);
            gone |= (c == removed);
            c = tabwinRemoveClient(passdata->tabwin, removed);
            passdata->c = c;
            status = EVENT_FILTER_CONTINUE;
            /* Walk through */
        case KeyPress:
            if (gone || key_pressed)
            {
                if (key_pressed)
                {
                    Client *c2 = NULL;

                    if (xevent->xkey.keycode == cancel)
                    {
                        c2 = tabwinGetHead (passdata->tabwin);
                        cycling = FALSE;
                    }
                    /* If KEY_CYCLE_WINDOWS has Shift, then do not reverse */
                    else if (!(modifier & ShiftMask) && (xevent->xkey.state & ShiftMask))
                    {
                        TRACE ("Cycle: previous");
                        c2 = tabwinSelectPrev(passdata->tabwin);
                    }
                    else
                    {
                        TRACE ("Cycle: next");
                        c2 = tabwinSelectNext(passdata->tabwin);
                    }
                    if (c2)
                    {
                        c = c2;
                        passdata->c = c;
                    }

                    /* If last key press event had not our modifier pressed, finish cycling */
                    if (!(xevent->xkey.state & modifier))
                    {
                        cycling = FALSE;
                    }
                }

                if (cycling)
                {
                    if (c)
                    {
                        wireframeUpdate (c, passdata->wireframe);
                    }
                    else
                    {
                        cycling = FALSE;
                    }
                }
            }
            break;
        case KeyRelease:
            {
                int keysym = XLookupKeysym (&xevent->xkey, 0);

                if (!(xevent->xkey.state & modifier) ||
                    (IsModifierKey(keysym) && (keysym != XK_Shift_L) && (keysym != XK_Shift_R)))
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
            status = EVENT_FILTER_CONTINUE;
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
clientCycle (Client * c, XEvent * ev)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    ClientCycleData passdata;
    gboolean g1, g2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientCycle");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    g1 = myScreenGrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
    g2 = myScreenGrabPointer (screen_info, NoEventMask,  None, myDisplayGetCurrentTime (display_info));

    if (!g1 || !g2)
    {
        TRACE ("grab failed in clientCycle");

        gdk_beep ();
        myScreenUngrabKeyboard (screen_info);
        myScreenUngrabPointer (screen_info);

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
    if (screen_info->params->cycle_workspaces)
    {
        passdata.cycle_range |= INCLUDE_ALL_WORKSPACES;
    }
    passdata.c = clientGetNext (c, passdata.cycle_range);

    /* If there is one single client, and if it's eligible for focus, use it */
    if ((passdata.c == NULL) && (c != clientGetFocus()) &&
        clientSelectMask (c, passdata.cycle_range, WINDOW_REGULAR_FOCUSABLE))
    {
        passdata.c = c;
    }

    if (passdata.c)
    {
        GdkPixbuf *icon;

        TRACE ("entering cycle loop");
        passdata.wireframe = wireframeCreate (passdata.c);
        icon = getAppIcon (display_info, passdata.c->window, 32, 32);
        passdata.tabwin = tabwinCreate (passdata.c->screen_info->gscr, c,
                                        passdata.c, passdata.cycle_range,
                                        screen_info->params->cycle_workspaces);
        eventFilterPush (display_info->xfilter, clientCycleEventFilter, &passdata);
        gtk_main ();
        eventFilterPop (display_info->xfilter);
        wireframeDelete (screen_info, passdata.wireframe);
        TRACE ("leaving cycle loop");
        tabwinDestroy (passdata.tabwin);
        g_free (passdata.tabwin);
    }

    if (passdata.c)
    {
        Client *focused;
        int workspace;

        c = passdata.c;
        workspace = c->win_workspace;
        focused = clientGetFocus ();

        if (workspace != screen_info->current_ws)
        {
            workspaceSwitch (screen_info, workspace, c, FALSE, myDisplayGetCurrentTime (display_info));
        }

        if ((focused) && (passdata.c != focused))
        {
            clientClearAllShowDesktop (screen_info);
            clientAdjustFullscreenLayer (focused, FALSE);
        }

        clientShow (c, TRUE);
        clientSetFocus (screen_info, c, myDisplayGetCurrentTime (display_info), NO_FOCUS_FLAG);
        clientRaise (c, None);
    }

    myScreenUngrabKeyboard (screen_info);
    myScreenUngrabPointer (screen_info);
}

static eventFilterStatus
clientButtonPressEventFilter (XEvent * xevent, gpointer data)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Client *c;
    ButtonPressData *passdata;
    eventFilterStatus status;
    int b;
    gboolean pressed;

    passdata = (ButtonPressData *) data;
    c = passdata->c;
    b = passdata->b;

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    /* Update the display time */
    myDisplayUpdateCurrentTime (display_info, xevent);

    status = EVENT_FILTER_STOP;
    pressed = TRUE;

    if (xevent->type == EnterNotify)
    {
        c->button_pressed[b] = TRUE;
        frameDraw (c, FALSE);
    }
    else if (xevent->type == LeaveNotify)
    {
        c->button_pressed[b] = FALSE;
        frameDraw (c, FALSE);
    }
    else if (xevent->type == ButtonRelease)
    {
        pressed = FALSE;
    }
    else if ((xevent->type == UnmapNotify) && (xevent->xunmap.window == c->window))
    {
        pressed = FALSE;
        c->button_pressed[b] = FALSE;
    }
    else if ((xevent->type == KeyPress) || (xevent->type == KeyRelease))
    {
    }
    else
    {
        status = EVENT_FILTER_CONTINUE;
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
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    ButtonPressData passdata;
    int b, g1;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientButtonPress");

    for (b = 0; b < BUTTON_LAST; b++)
    {
        if (MYWINDOW_XWINDOW (c->buttons[b]) == w)
        {
            break;
        }
    }

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    g1 = XGrabPointer (display_info->dpy, w, FALSE,
                       ButtonReleaseMask | EnterWindowMask | LeaveWindowMask,
                       GrabModeAsync, GrabModeAsync,
                       screen_info->xroot, None,
                       myDisplayGetCurrentTime (display_info));

    if (g1 != GrabSuccess)
    {
        TRACE ("grab failed in clientButtonPress");
        gdk_beep ();
        if (g1 == GrabSuccess)
        {
            XUngrabKeyboard (display_info->dpy, myDisplayGetCurrentTime (display_info));
        }
        return;
    }

    passdata.c = c;
    passdata.b = b;

    c->button_pressed[b] = TRUE;
    frameDraw (c, FALSE);

    TRACE ("entering button press loop");
    eventFilterPush (display_info->xfilter, clientButtonPressEventFilter, &passdata);
    gtk_main ();
    eventFilterPop (display_info->xfilter);
    TRACE ("leaving button press loop");

    XUngrabPointer (display_info->dpy, myDisplayGetCurrentTime (display_info));

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
                    if (bev->button == Button1)
                    {
                        clientToggleMaximized (c, WIN_STATE_MAXIMIZED, TRUE);
                    }
                    else if (bev->button == Button2)
                    {
                        clientToggleMaximized (c, WIN_STATE_MAXIMIZED_VERT, TRUE);
                    }
                    else if (bev->button == Button3)
                    {
                        clientToggleMaximized (c, WIN_STATE_MAXIMIZED_HORIZ, TRUE);
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
        frameDraw (c, FALSE);
    }
}

Client *
clientGetLeader (Client * c)
{
    TRACE ("entering clientGetLeader");
    g_return_val_if_fail (c != NULL, NULL);

    if (c->group_leader != None)
    {
        return clientGetFromWindow (c->screen_info, c->group_leader, WINDOW);
    }
    else if (c->client_leader != None)
    {
        return clientGetFromWindow (c->screen_info, c->client_leader, WINDOW);
    }
    return NULL;
}

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
char *
clientGetStartupId (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    gboolean got_startup_id;

    g_return_val_if_fail (c != NULL, NULL);
    g_return_val_if_fail (c->window != None, NULL);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    got_startup_id = FALSE;

    if (c->startup_id)
    {
        return (c->startup_id);
    }

    got_startup_id = getWindowStartupId (display_info, c->window, &c->startup_id);

    if (!got_startup_id && (c->client_leader))
    {
        got_startup_id = getWindowStartupId (display_info, c->client_leader, &c->startup_id);
    }

    if (!got_startup_id && (c->group_leader))
    {
        got_startup_id = getWindowStartupId (display_info, c->group_leader, &c->startup_id);
    }

    return (c->startup_id);
}
#endif /* HAVE_LIBSTARTUP_NOTIFICATION */
