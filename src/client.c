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

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

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
#include "moveresize.h"
#include "mypixmap.h"
#include "mywindow.h"
#include "netwm.h"
#include "placement.h"
#include "screen.h"
#include "session.h"
#include "settings.h"
#include "stacking.h"
#include "startup_notification.h"
#include "transients.h"
#include "workspaces.h"
#include "xsync.h"
#include "event_filter.h"

/* Event mask definition */

#define POINTER_EVENT_MASK \
    ButtonPressMask|\
    ButtonReleaseMask

#define FRAME_EVENT_MASK \
    SubstructureNotifyMask|\
    SubstructureRedirectMask|\
    PointerMotionMask|\
    ButtonMotionMask|\
    FocusChangeMask|\
    EnterWindowMask|\
    PropertyChangeMask

#define CLIENT_EVENT_MASK \
    StructureNotifyMask|\
    FocusChangeMask|\
    PropertyChangeMask

#define BUTTON_EVENT_MASK \
    EnterWindowMask|\
    LeaveWindowMask

/* Useful macros */
#define START_ICONIC(c) \
    ((c->wmhints) && \
     (c->wmhints->initial_state == IconicState) && \
     !clientIsValidTransientOrModal (c))

#define OPACITY_SET_STEP        (guint) 0x16000000
#define OPACITY_SET_MIN         (guint) 0x40000000

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

static gchar*
clientCreateTitleName (Client * c, gchar *name, gchar *hostname)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    gchar *title;

    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering clientCreateTitleName");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (strlen (hostname) && (display_info->hostname) && (g_strcasecmp (display_info->hostname, hostname)))
    {
        /* TRANSLATORS: "(on %s)" is like "running on" the name of the other host */
        title = g_strdup_printf (_("%s (on %s)"), name, hostname);
    }
    else
    {
        title = g_strdup (name);
    }

    return title;
}

void
clientUpdateName (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    gchar *hostname;
    gchar *wm_name;
    gchar *name;
    gboolean refresh;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientUpdateName");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    getWindowName (display_info, c->window, &wm_name);
    getWindowHostname (display_info, c->window, &hostname);
    refresh = FALSE;

    /* Update hostname too, as it's used when terminating a client */
    if (hostname)
    {
        if (c->hostname)
        {
            g_free (c->hostname);
        }
        c->hostname = hostname;
    }

    if (wm_name)
    {
        name = clientCreateTitleName (c, wm_name, hostname);
        g_free (wm_name);
        if (c->name)
        {
            if (strcmp (name, c->name))
            {
                refresh = TRUE;
                FLAG_SET (c->flags, CLIENT_FLAG_NAME_CHANGED);
            }
            g_free (c->name);
        }
        c->name = name;
    }

    if (refresh)
    {
        frameQueueDraw (c, TRUE);
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
            clientCoordGravitate (c, REMOVE, &c->x, &c->y);
            clientCoordGravitate (c, APPLY, &c->x, &c->y);
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
            frameQueueDraw (c, TRUE);
        }

    }
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

    TRACE ("entering urgent_cb, iteration %i", c->blink_iterations);

    c = (Client *) data;
    if (c != clientGetFocus ())
    {
        c->blink_iterations++;
        if (c->blink_iterations < (2 * MAX_BLINK_ITERATIONS + 1))
        {
            FLAG_TOGGLE (c->xfwm_flags, XFWM_FLAG_SEEN_ACTIVE);
            frameQueueDraw (c, FALSE);
        }
        else if (c->blink_iterations > (8 * MAX_BLINK_ITERATIONS))
        {
            c->blink_iterations = 0;
        }        
    }
    else if (c->blink_iterations)
    {
        c->blink_iterations = 0;
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
        frameQueueDraw (c, FALSE);
    }
    FLAG_UNSET (c->wm_flags, WM_FLAG_URGENT);

    c->blink_timeout_id = 0;
    c->blink_iterations = 0;
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
        frameQueueDraw (c, FALSE);
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
clientAdjustCoordGravity (Client * c, unsigned long *mask, XWindowChanges *wc)
{
    int tx, ty, dw, dh;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientCoordGravitate");

    tx = wc->x;
    ty = wc->y;
    clientCoordGravitate (c, APPLY, &tx, &ty);

    switch (c->gravity)
    {
        case CenterGravity:
            dw = (c->width  - wc->width)  / 2;
            dh = (c->height - wc->height) / 2;
            break;
        case NorthGravity:
            dw = (c->width - wc->width) / 2;
            dh = 0;
            break;
        case SouthGravity:
            dw = (c->width  - wc->width) / 2;
            dh = (c->height - wc->height);
            break;
        case EastGravity:
            dw = (c->width  - wc->width);
            dh = (c->height - wc->height) / 2;
            break;
        case WestGravity:
            dw = 0;
            dh = (c->height - wc->height) / 2;
            break;
        case NorthWestGravity:
            dw = 0;
            dh = 0;
            break;
        case NorthEastGravity:
            dw = (c->width - wc->width);
            dh = 0;
            break;
        case SouthWestGravity:
            dw = 0;
            dh = (c->height - wc->height);
            break;
        case SouthEastGravity:
            dw = (c->width  - wc->width);
            dh = (c->height - wc->height);
            break;
        default:
            dw = 0;
            dh = 0;
            break;
    }

    if (*mask & CWX)
    {
        wc->x = tx;
    }
    else if (*mask & CWWidth)
    {
        wc->x = c->x + dw;
        *mask |= CWX;
    }

    if (*mask & CWY)
    {
        wc->y = ty;
    }
    else if (*mask & CWHeight)
    {
        wc->y = c->y + dh;
        *mask |= CWY;
    }
}

#define WIN_MOVED   (mask & (CWX | CWY))
#define WIN_RESIZED (mask & (CWWidth | CWHeight))

static void
clientConfigureWindows (Client * c, XWindowChanges * wc, unsigned long mask, unsigned short flags)
{
    unsigned long change_mask_frame, change_mask_client;
    XWindowChanges change_values;
    DisplayInfo *display_info;
    ScreenInfo *screen_info;

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    change_mask_frame = mask & (CWX | CWY | CWWidth | CWHeight);
    change_mask_client = mask & (CWWidth | CWHeight);

    if ((WIN_RESIZED) || (flags & CFG_FORCE_REDRAW))
    {
        frameDraw (c, (flags & CFG_FORCE_REDRAW));
    }

    if (flags & CFG_FORCE_REDRAW)
    {
        change_mask_client |= (CWX | CWY);
    }

    if (change_mask_frame & (CWX | CWY | CWWidth | CWHeight))
    {
        change_values.x = frameX (c);
        change_values.y = frameY (c);
        change_values.width = frameWidth (c);
        change_values.height = frameHeight (c);
        XConfigureWindow (display_info->dpy, c->frame, change_mask_frame, &change_values);
    }

    if (change_mask_client & (CWX | CWY | CWWidth | CWHeight))
    {
        change_values.x = frameLeft (c);
        change_values.y = frameTop (c);
        change_values.width = c->width;
        change_values.height = c->height;
        XConfigureWindow (display_info->dpy, c->window, change_mask_client, &change_values);
    }
    if (WIN_RESIZED)
    {
        compositorResizeWindow (display_info, c->frame, frameX (c), frameY (c), frameWidth (c), frameHeight (c));
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

    px = c->x;
    py = c->y;
    pwidth = c->width;
    pheight = c->height;

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
        /* Keep fully visible only on resize */
        clientConstrainPos (c, (mask & (CWWidth | CWHeight)));

        if (c->x != px)
        {
            mask |= CWX;
        }
        else
        {
            mask &= ~CWX;
        }

        if (c->y != py)
        {
            mask |= CWY;
        }
        else
        {
            mask &= ~CWY;
        }

        if (c->width != pwidth)
        {
            mask |= CWWidth;
        }
        else
        {
            mask &= ~CWWidth;
        }
        if (c->height != pheight)
        {
            mask |= CWHeight;
        }
        else
        {
            mask &= ~CWHeight;
        }
    }

    clientConfigureWindows (c, wc, mask, flags);
    /*

      We reparent the client window. According to the ICCCM spec, the
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

        /* MWM hints can add or remove decorations, update NET_FRAME_EXTENTS accordingly */
        setNetFrameExtents (display_info,
                            c->window,
                            frameTop (c),
                            frameLeft (c),
                            frameRight (c),
                            frameBottom (c));
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
            frameQueueDraw (c, FALSE);
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
    /* Ping */
    FLAG_SET (c->wm_flags,
        (wm_protocols_flags & WM_PROTOCOLS_PING) ?
        WM_FLAG_PING : 0);
}

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
    if (clientGetDelayedFocus () == c)
    {
        clientClearDelayedFocus ();
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
    if (c->ping_timeout_id)
    {
        clientRemoveNetWMPing (c);
    }
    if (c->name)
    {
        g_free (c->name);
    }
    if (c->hostname)
    {
        g_free (c->hostname);
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
    if (c->dialog_pid)
    {
        kill (c->dialog_pid, SIGKILL);
    }
    if (c->dialog_fd >= 0)
    {
        close (c->dialog_fd);
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
        clientUpdateLayerState (c);
    }
    if (FLAG_TEST_AND_NOT (c->flags, CLIENT_FLAG_BELOW, CLIENT_FLAG_ABOVE))
    {
        TRACE ("Applying client's initial state: below");
        clientUpdateLayerState (c);
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
        TRACE ("Applying client's initial state: shaded");
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
            frameQueueDraw (c, FALSE);
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

    if (getHint (display_info, c->window, NET_WM_USER_TIME, (long *) &c->user_time) && (c->user_time != 0))
    {
        FLAG_SET (c->flags, CLIENT_FLAG_HAS_USER_TIME);
        myDisplayUpdateLastUserTime (display_info, c->user_time);
    }
}

void
clientAddUserTimeWin (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if ((c->user_time_win != None) && (c->user_time_win != c->window))
    {
        XSelectInput (display_info->dpy, c->user_time_win, PropertyChangeMask);
    }
}

void
clientRemoveUserTimeWin (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if ((c->user_time_win != None) && (c->user_time_win != c->window))
    {
        XSelectInput (display_info->dpy, c->user_time_win, NoEventMask);
    }
}

static void
clientUpdateIconPix (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    gint size;
    GdkPixbuf *icon;
    int i;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->window != None);

    TRACE ("entering clientUpdateIconPix for \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    for (i = 0; i < STATE_TOGGLED; i++)
    {
        xfwmPixmapFree (&c->appmenu[i]);
    }

    if (xfwmPixmapNone(&screen_info->buttons[MENU_BUTTON][ACTIVE]))
    {
        /* The current theme has no menu button */
        return;
    }

    for (i = 0; i < STATE_TOGGLED; i++)
    {
        if (!xfwmPixmapNone(&screen_info->buttons[MENU_BUTTON][i]))
        {
            xfwmPixmapDuplicate (&screen_info->buttons[MENU_BUTTON][i], &c->appmenu[i]);
        }
    }
    size = MIN (screen_info->buttons[MENU_BUTTON][ACTIVE].width,
                screen_info->buttons[MENU_BUTTON][ACTIVE].height);

    if (size > 1)
    {
        icon = getAppIcon (display_info, c->window, size, size);

        for (i = 0; i < STATE_TOGGLED; i++)
        {
            if (!xfwmPixmapNone(&c->appmenu[i]))
            {
                xfwmPixmapRenderGdkPixbuf (&c->appmenu[i], icon);
            }
        }
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
        frameQueueDraw (c, FALSE);
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

    /* Termination dialog */
    c->dialog_pid = 0;
    c->dialog_fd = -1;

    getWindowName (display_info, c->window, &c->name);
    getWindowHostname (display_info, c->window, &c->hostname);
    getTransientFor (display_info, screen_info->xroot, c->window, &c->transient_for);

    /* Initialize structure */
    c->size = NULL;
    c->flags = 0L;
    c->wm_flags = 0L;
    c->xfwm_flags = XFWM_FLAG_INITIAL_VALUES;
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

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        c->button_status[i] = BUTTON_STATE_NORMAL;
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

    /* Keep count of blinking iterations */
    c->blink_iterations = 0;

    if (getOpacityLock (display_info, c->window))
    {
        FLAG_SET (c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED);
    }

    /* Timout for asynchronous icon update */
    c->icon_timeout_id = 0;
    /* Timout for asynchronous frame update */
    c->frame_timeout_id = 0;
    /* Timeout for blinking on urgency */
    c->blink_timeout_id = 0;
    /* Ping timeout  */
    c->ping_timeout_id = 0;
    /* Ping timeout  */
    c->ping_time = 0;

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
    c->user_time_win = getNetWMUserTimeWindow(display_info, c->window);
    clientAddUserTimeWin (c);
    clientGetUserTime (c);

    /* Client PID */
    getHint (display_info, c->window, NET_WM_PID, (long *) &c->pid);
    TRACE ("Client \"%s\" (0x%lx) PID = %i", c->name, c->window, c->pid);

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
        clientCoordGravitate (c, APPLY, &c->x, &c->y);
        if ((attr.map_state == IsUnmapped))
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
    clientGrabButtons(c);

    /* Initialize per client menu button pixmap */

    for (i = 0; i < STATE_TOGGLED; i++)
    {
        xfwmPixmapInit (screen_info, &c->appmenu[i]);
    }

    for (i = 0; i < SIDE_TOP; i++) /* Keep SIDE_TOP for later */
    {
        xfwmWindowCreate (screen_info, c->visual, c->depth, c->frame,
            &c->sides[i], NoEventMask,
            myDisplayGetCursorResize(screen_info->display_info, CORNER_COUNT + i));
    }

    for (i = 0; i < CORNER_COUNT; i++)
    {
        xfwmWindowCreate (screen_info, c->visual, c->depth, c->frame,
            &c->corners[i], NoEventMask,
            myDisplayGetCursorResize(screen_info->display_info, i));
    }

    xfwmWindowCreate (screen_info, c->visual, c->depth, c->frame,
        &c->title, NoEventMask, None);

    /* create the top side window AFTER the title window since they overlap
       and the top side window should be on top */

    xfwmWindowCreate (screen_info, c->visual, c->depth, c->frame,
        &c->sides[SIDE_TOP], NoEventMask,
        myDisplayGetCursorResize(screen_info->display_info,
        CORNER_COUNT + SIDE_TOP));

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        xfwmWindowCreate (screen_info, c->visual, c->depth, c->frame,
            &c->buttons[i], BUTTON_EVENT_MASK, None);
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
            clientSetNetActions (c);
        }
    }
    else
    {
        clientRaise (c, None);
        setWMState (display_info, c->window, IconicState);
        clientSetNetActions (c);
    }

    if (!grabbed)
    {
        clientGrabMouseButton (c);
    }
    setNetFrameExtents (display_info, c->window, frameTop (c), frameLeft (c),
                                                 frameRight (c), frameBottom (c));
    clientSetNetState (c);

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
    clientRemoveUserTimeWin (c);
    clientUngrabButtons (c);
    XUnmapWindow (display_info->dpy, c->frame);
    clientCoordGravitate (c, REMOVE, &c->x, &c->y);
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

    for (i = 0; i < SIDE_COUNT; i++)
    {
        xfwmWindowDelete (&c->sides[i]);
    }
    for (i = 0; i < CORNER_COUNT; i++)
    {
        xfwmWindowDelete (&c->corners[i]);
    }
    for (i = 0; i < STATE_TOGGLED; i++)
    {
        xfwmPixmapFree (&c->appmenu[i]);
    }
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        xfwmWindowDelete (&c->buttons[i]);
    }
    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUT))
    {
        workspaceUpdateArea (c->screen_info);
    }
    XDestroyWindow (display_info->dpy, c->frame);

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
        c = myScreenGetClientFromWindow (screen_info, wins[i], SEARCH_FRAME);
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
clientGetFromWindow (Client *c, Window w, unsigned short mode)
{
    int b;

    g_return_val_if_fail (w != None, NULL);
    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering clientGetFromWindow");

    if (mode & SEARCH_WINDOW)
    {
        if (c->window == w)
        {
            TRACE ("found \"%s\" (mode WINDOW)", c->name);
            return (c);
        }
    }

    if (mode & SEARCH_FRAME)
    {
        if (c->frame == w)
        {
            TRACE ("found \"%s\" (mode FRAME)", c->name);
            return (c);
        }
    }

    if (mode & SEARCH_WIN_USER_TIME)
    {
        if (c->user_time_win == w)
        {
            TRACE ("found \"%s\" (mode WIN_USER_TIME)", c->name);
            return (c);
        }
    }

    if (mode & SEARCH_BUTTON)
    {
        for (b = 0; b < BUTTON_COUNT; b++)
        {
            if (MYWINDOW_XWINDOW(c->buttons[b]) == w)
            {
                TRACE ("found \"%s\" (mode BUTTON)", c->name);
                return (c);
            }
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
clientShowSingle (Client * c, gboolean deiconify)
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
    if (deiconify)
    {
        FLAG_UNSET (c->flags, CLIENT_FLAG_ICONIFIED);
        setWMState (display_info, c->window, NormalState);
    }
    clientSetNetActions (c);
    clientSetNetState (c);
}

void
clientShow (Client * c, gboolean deiconify)
{
    Client *c2;
    GList *list_of_windows;
    GList *index;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientShow \"%s\" (0x%lx)", c->name, c->window);

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
        clientShowSingle (c2, deiconify);
    }
    g_list_free (list_of_windows);

    /* Update working area as windows have been shown */
    workspaceUpdateArea (c->screen_info);
}

static void
clientHideSingle (Client * c, GList *exclude_list, gboolean iconify)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    TRACE ("hiding client \"%s\" (0x%lx)", c->name, c->window);
    clientPassFocus(c->screen_info, c, exclude_list);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
    {
        FLAG_UNSET (c->xfwm_flags, XFWM_FLAG_VISIBLE);
        c->ignore_unmap++;
        /* Adjust to urgency state as the window is not visible */
        clientUpdateUrgency (c);
    }
    XUnmapWindow (display_info->dpy, c->window);
    XUnmapWindow (display_info->dpy, c->frame);
    if (iconify)
    {
        FLAG_SET (c->flags, CLIENT_FLAG_ICONIFIED);
        setWMState (display_info, c->window, IconicState);
        if (!screen_info->show_desktop)
        {
            clientSetLast (c);
        }
    }
    clientSetNetActions (c);
    clientSetNetState (c);
}

void
clientHide (Client * c, int ws, gboolean iconify)
{
    Client *c2;
    GList *list_of_windows;
    GList *index;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientHide \"%s\" (0x%lx)", c->name, c->window);

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

        if (FLAG_TEST (c2->flags, CLIENT_FLAG_STICKY) && !iconify)
        {
            continue;
        }
        clientHideSingle (c2, list_of_windows, iconify);
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
    Client *sibling;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientActivate \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    sibling = clientGetTransientFor(c);
    if ((screen_info->current_ws == c->win_workspace) || (screen_info->params->activate_action != ACTIVATE_ACTION_NONE))
    {
        if (screen_info->current_ws != c->win_workspace)
        {
            if (screen_info->params->activate_action == ACTIVATE_ACTION_BRING)
            {
                clientSetWorkspace (c, screen_info->current_ws, TRUE);
            }
            else
            {
                workspaceSwitch (screen_info, c->win_workspace, NULL, FALSE, timestamp);
            }
        }
        clientRaise (sibling, None);
        clientShow (sibling, TRUE);
        clientClearAllShowDesktop (screen_info);
        clientSetFocus (screen_info, c, timestamp, NO_FOCUS_FLAG);
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
    Time timestamp;

    g_return_if_fail (c != NULL);

    TRACE ("entering clientClose");
    TRACE ("closing client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    timestamp = myDisplayGetCurrentTime (display_info);

    if (FLAG_TEST (c->wm_flags, WM_FLAG_DELETE))
    {
        sendClientMessage (screen_info, c->window, WM_DELETE_WINDOW, timestamp);
    }
    else
    {
        clientKill (c);
    }
    if (FLAG_TEST (c->wm_flags, WM_FLAG_PING))
    {
        clientSendNetWMPing (c, timestamp);
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
clientTerminate (Client * c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientTerminate");

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if ((c->hostname) && (c->pid > 0))
    {
        if (!strcmp (display_info->hostname, c->hostname))
        {
            TRACE ("Sending client %s (pid %i) signal SIGKILL\n", c->name, c->pid);

            if (kill (c->pid, SIGKILL) < 0)
            {
                g_warning ("Failed to kill client id %d: %s", c->pid, strerror (errno));
            }
        }
    }

    clientKill (c);
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
    clientSetNetState (c);
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
    clientSetNetState (c);
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
        clientSetWorkspace (c, screen_info->current_ws, TRUE);
    }
    clientSetNetState (c);
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
        clientSetWorkspace (c, screen_info->current_ws, TRUE);
    }
    clientSetNetState (c);
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

    if (!clientIsTransientOrModal (c) && (c->type == WINDOW_NORMAL))
    {
        FLAG_TOGGLE (c->flags, CLIENT_FLAG_FULLSCREEN);
        clientUpdateFullscreenState (c);
    }
}

void clientToggleLayerAbove (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleAbove");

    if (!clientIsTransientOrModal (c) &&
        !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        FLAG_UNSET (c->flags, CLIENT_FLAG_BELOW);
        FLAG_TOGGLE (c->flags, CLIENT_FLAG_ABOVE);
        clientUpdateLayerState (c);
    }
}

void clientToggleLayerBelow (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientToggleBelow");

    if (!clientIsTransientOrModal (c) &&
        !FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        FLAG_UNSET (c->flags, CLIENT_FLAG_ABOVE);
        FLAG_TOGGLE (c->flags, CLIENT_FLAG_BELOW);
        clientUpdateLayerState (c);
    }
}

void clientSetLayerNormal (Client * c)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering clientSetLayerNormal");

    if (!FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        FLAG_UNSET (c->flags, CLIENT_FLAG_ABOVE | CLIENT_FLAG_BELOW);
        clientUpdateLayerState (c);
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
    frameQueueDraw (c, FALSE);
    clientSetNetActions (c);
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

    /* 3) Update size and position fields */
    c->x = wc.x;
    c->y = wc.y;
    c->height = wc.height;
    c->width = wc.width;

    /* Maximizing may remove decoration on the side, update NET_FRAME_EXTENTS accordingly */
    setNetFrameExtents (display_info,
                        c->window,
                        frameTop (c),
                        frameLeft (c),
                        frameRight (c),
                        frameBottom (c));

    /* Maximized windows w/out border cannot be resized, update allowed actions */
    clientSetNetActions (c);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MANAGED) && (restore_position))
    {
        clientConfigure (c, &wc, CWWidth | CWHeight | CWX | CWY, CFG_FORCE_REDRAW);
    }
    clientSetNetState (c);
}

void
clientFill (Client * c, int fill_type)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Client *east_neighbour;
    Client *west_neighbour;
    Client *north_neighbour;
    Client *south_neighbour;
    Client *c2;
    GdkRectangle rect;
    XWindowChanges wc;
    unsigned short mask;
    int i, cx, cy, full_x, full_y, full_w, full_h;
    int tmp_x, tmp_y, tmp_w, tmp_h;
    gint monitor_nbr;

    g_return_if_fail (c != NULL);

    if (!CLIENT_CAN_FILL_WINDOW (c))
    {
        return;
    }

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    mask = 0;
    east_neighbour = NULL;
    west_neighbour = NULL;
    north_neighbour = NULL;
    south_neighbour = NULL;

    for (c2 = screen_info->clients, i = 0; i < screen_info->client_count; c2 = c2->next, i++)
    {

        /* Filter out all windows which are not visible, or not on the same layer
         * as well as the client window itself
         */
        if ((c != c2) && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_VISIBLE) && (c2->win_layer == c->win_layer))
        {
            /* Fill horizontally */
            if (fill_type & CLIENT_FILL_HORIZ)
            {
                /*
                 * check if the neigbour client (c2) is located
                 * east or west of our client.
                 */
                if (!(((frameY(c) + frameHeight(c)) < (frameY(c2) - frameTop(c2))) || ((frameY(c2) + frameHeight(c2)) < (frameY(c) - frameTop(c)))))
                {
                    if ((frameX(c2) + frameWidth(c2)) < frameX(c))
                    {
                        if (east_neighbour)
                        {
                            /* Check if c2 is closer to the client
                             * then the east neighbour already found
                             */
                            if ((frameX(east_neighbour) + frameWidth(east_neighbour)) < (frameX(c2) + frameWidth(c2)))
                            {
                                east_neighbour = c2;
                            }
                        }
                        else
                        {
                            east_neighbour = c2;
                        }
                    }
                    if ((frameX(c) + frameWidth(c)) < frameX(c2))
                    {
                        /* Check if c2 is closer to the client
                         * then the west neighbour already found
                         */
                        if (west_neighbour)
                        {
                            if (frameX(c2) < frameX(west_neighbour))
                            {
                                west_neighbour = c2;
                            }
                        }
                        else
                        {
                            west_neighbour = c2;
                        }
                    }
                }
            }

            /* Fill vertically */
            if (fill_type & CLIENT_FILL_VERT)
            {
                /* check if the neigbour client (c2) is located
                 * north or south of our client.
                 */
                if (!(((frameX(c) + frameWidth(c)) < frameX(c2)) || ((frameX(c2) + frameWidth(c2)) < frameX(c))))
                {
                    if ((frameY(c2) + frameHeight(c2)) < frameY(c))
                    {
                        if (north_neighbour)
                        {
                            /* Check if c2 is closer to the client
                             * then the north neighbour already found
                             */
                            if ((frameY(north_neighbour) + frameHeight(north_neighbour)) < (frameY(c2) + frameHeight(c2)))
                            {
                                north_neighbour = c2;
                            }
                        }
                        else
                        {
                            north_neighbour = c2;
                        }
                    }
                    if ((frameY(c) + frameHeight(c)) < frameY(c2))
                    {
                        if (south_neighbour)
                        {
                            /* Check if c2 is closer to the client
                             * then the south neighbour already found
                             */
                            if (frameY(c2) < frameY(south_neighbour))
                            {
                                south_neighbour = c2;
                            }
                        }
                        else
                        {
                            south_neighbour = c2;
                        }
                    }
                }
            }
        }
    }

    /* Compute the largest size available, based on struts, margins and Xinerama layout */
    tmp_x = frameX (c);
    tmp_y = frameY (c);
    tmp_h = frameHeight (c);
    tmp_w = frameWidth (c);

    cx = tmp_x + (tmp_w / 2);
    cy = tmp_y + (tmp_h / 2);

    monitor_nbr = find_monitor_at_point (screen_info->gscr, cx, cy);
    gdk_screen_get_monitor_geometry (screen_info->gscr, monitor_nbr, &rect);

    full_x = MAX (screen_info->params->xfwm_margins[STRUTS_LEFT], rect.x);
    full_y = MAX (screen_info->params->xfwm_margins[STRUTS_TOP], rect.y);
    full_w = MIN (screen_info->width - screen_info->params->xfwm_margins[STRUTS_RIGHT],
                  rect.x + rect.width) - full_x;
    full_h = MIN (screen_info->height - screen_info->params->xfwm_margins[STRUTS_BOTTOM],
                  rect.y + rect.height) - full_y;

    if ((fill_type & CLIENT_FILL) == CLIENT_FILL)
    {
        mask = CWX | CWY | CWHeight | CWWidth;
        /* Adjust size to the largest size available, not covering struts */
        clientMaxSpace (screen_info, &full_x, &full_y, &full_w, &full_h);
    }
    else if (fill_type & CLIENT_FILL_VERT)
    {
        mask = CWY | CWHeight;
        /* Adjust size to the tallest size available, for the current horizontal position/width */
        clientMaxSpace (screen_info, &tmp_x, &full_y, &tmp_w, &full_h);
    }
    else if (fill_type & CLIENT_FILL_HORIZ)
    {
        mask = CWX | CWWidth;
        /* Adjust size to the widest size available, for the current vertical position/height */
        clientMaxSpace (screen_info, &full_x, &tmp_y, &full_w, &tmp_h);
    }

    /* If there are neighbours, resize to their borders.
     * If not, resize to the largest size available taht you just have computed.
     */

    if (east_neighbour)
    {
        wc.x = frameX(east_neighbour) + frameWidth(east_neighbour) + frameLeft(c);
    }
    else
    {
        wc.x = full_x + frameLeft(c);
    }

    if (west_neighbour)
    {
        wc.width = full_x + frameX(west_neighbour) - frameRight(c) - wc.x;
    }
    else
    {
        wc.width = full_x + full_w - frameRight(c) - wc.x;
    }

    if (north_neighbour)
    {
        wc.y = frameY(north_neighbour) + frameHeight(north_neighbour) + frameTop(c);
    }
    else
    {
        wc.y = full_y + frameTop(c);
    }

    if (south_neighbour)
    {
        wc.height = full_y + frameY(south_neighbour) - frameBottom(c) - wc.y;
    }
    else
    {
        wc.height = full_y + full_h - frameBottom(c) - wc.y;
    }

    TRACE ("Fill size request: (%d,%d) %dx%d", wc.x, wc.y, wc.width, wc.height);
    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MANAGED))
    {
        clientConfigure(c, &wc, mask, NO_CFG_FLAG);
    }
}

void
clientUpdateOpacity (Client *c)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Client *focused;
    gboolean opaque;

    g_return_if_fail (c != NULL);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    if (!compositorIsUsable (display_info))
    {
        return;
    }

    focused = clientGetFocus ();
    opaque = (FLAG_TEST(c->type, WINDOW_TYPE_DONT_PLACE | WINDOW_TYPE_DONT_FOCUS)
              || (focused == c));

    clientSetOpacity (c, c->opacity, OPACITY_INACTIVE, opaque ? 0 : OPACITY_INACTIVE);
}

void
clientUpdateAllOpacity (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;
    Client *c;
    int i;

    g_return_if_fail (screen_info != NULL);

    display_info = screen_info->display_info;
    if (!compositorIsUsable (display_info))
    {
        return;
    }

    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, ++i)
    {
        clientUpdateOpacity (c);
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

    if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED))
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

    if ((c->opacity > OPACITY_SET_MIN) && !(FLAG_TEST (c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED)))
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

    if ((c->opacity < NET_WM_OPAQUE) && !(FLAG_TEST (c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED)))
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

    g_list_free (list_of_windows);
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

    switch (xevent->type)
    {
        case EnterNotify:
            if ((xevent->xcrossing.mode != NotifyGrab) && (xevent->xcrossing.mode != NotifyUngrab))
            {
                c->button_status[b] = BUTTON_STATE_PRESSED;
                frameQueueDraw (c, FALSE);
            }
            break;
        case LeaveNotify:
            if ((xevent->xcrossing.mode != NotifyGrab) && (xevent->xcrossing.mode != NotifyUngrab))
            {
                c->button_status[b] = BUTTON_STATE_NORMAL;
                frameQueueDraw (c, FALSE);
            }
            break;
        case ButtonRelease:
            pressed = FALSE;
            break;
        case UnmapNotify:
            if (xevent->xunmap.window == c->window)
            {
                pressed = FALSE;
                c->button_status[b] = BUTTON_STATE_NORMAL;
            }
            break;
        case KeyPress:
        case KeyRelease:
            break;
        default:
            status = EVENT_FILTER_CONTINUE;
            break;
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

    c->button_status[b] = BUTTON_STATE_PRESSED;
    frameQueueDraw (c, FALSE);

    TRACE ("entering button press loop");
    eventFilterPush (display_info->xfilter, clientButtonPressEventFilter, &passdata);
    gtk_main ();
    eventFilterPop (display_info->xfilter);
    TRACE ("leaving button press loop");

    XUngrabPointer (display_info->dpy, myDisplayGetCurrentTime (display_info));

    if (c->button_status[b] == BUTTON_STATE_PRESSED)
    {
        /*
         * Button was pressed at the time, means the pointer was still within
         * the button, so return to prelight if available, normal otherwise.
         */
        if (!xfwmPixmapNone(clientGetButtonPixmap(c, b, PRELIGHT)))
        {
            c->button_status[b] = BUTTON_STATE_PRELIGHT;
        }
        else
        {
            c->button_status[b] = BUTTON_STATE_NORMAL;
        }

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
        frameQueueDraw (c, FALSE);
    }
}

xfwmPixmap *
clientGetButtonPixmap (Client * c, int button, int state)
{
    ScreenInfo *screen_info;

    TRACE ("entering clientGetButtonPixmap button=%i, state=%i", button, state);
    screen_info = c->screen_info;
    switch (button)
    {
        case MENU_BUTTON:
            if ((screen_info->params->show_app_icon)
                && (!xfwmPixmapNone(&c->appmenu[state])))
            {
                return &c->appmenu[state];
            }
            break;
        case SHADE_BUTTON:
            if (FLAG_TEST (c->flags, CLIENT_FLAG_SHADED)
                && (!xfwmPixmapNone(&screen_info->buttons[SHADE_BUTTON][state + STATE_TOGGLED])))
            {
                return &screen_info->buttons[SHADE_BUTTON][state + STATE_TOGGLED];
            }
            return &screen_info->buttons[SHADE_BUTTON][state];
            break;
        case STICK_BUTTON:
            if (FLAG_TEST (c->flags, CLIENT_FLAG_STICKY)
                && (!xfwmPixmapNone(&screen_info->buttons[STICK_BUTTON][state + STATE_TOGGLED])))
            {
                return &screen_info->buttons[STICK_BUTTON][state + STATE_TOGGLED];
            }
            return &screen_info->buttons[STICK_BUTTON][state];
            break;
        case MAXIMIZE_BUTTON:
            if (FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED)
                && (!xfwmPixmapNone(&screen_info->buttons[MAXIMIZE_BUTTON][state + STATE_TOGGLED])))
            {
                return &screen_info->buttons[MAXIMIZE_BUTTON][state + STATE_TOGGLED];
            }
            return &screen_info->buttons[MAXIMIZE_BUTTON][state];
            break;
        default:
            break;
    }
    return &screen_info->buttons[button][state];
}

int
clientGetButtonState (Client *c, int button, int state)
{
    ScreenInfo *screen_info;

    screen_info = c->screen_info;

    if (state == INACTIVE)
    {
        return (state);
    }

    if ((c->button_status[button] == BUTTON_STATE_PRESSED) &&
        clientGetButtonPixmap (c, button, PRESSED))
    {
        return (PRESSED);
    }

    if ((c->button_status[button] == BUTTON_STATE_PRELIGHT) &&
        clientGetButtonPixmap (c, button, PRELIGHT))
    {
        return (PRELIGHT);
    }

    return (ACTIVE);
}


Client *
clientGetLeader (Client * c)
{
    TRACE ("entering clientGetLeader");
    g_return_val_if_fail (c != NULL, NULL);

    if (c->group_leader != None)
    {
        return myScreenGetClientFromWindow (c->screen_info, c->group_leader, SEARCH_WINDOW);
    }
    else if (c->client_leader != None)
    {
        return myScreenGetClientFromWindow (c->screen_info, c->client_leader, SEARCH_WINDOW);
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
