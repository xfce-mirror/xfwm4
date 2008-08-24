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
#
#include "client.h"
#include "focus.h"
#include "frame.h"
#include "placement.h"
#include "poswin.h"
#include "screen.h"
#include "settings.h"
#include "transients.h"
#include "event_filter.h"
#include "workspaces.h"
#include "xsync.h"

#define MOVERESIZE_EVENT_MASK \
    PointerMotionMask | \
    ButtonMotionMask | \
    ButtonReleaseMask | \
    LeaveWindowMask

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
    int cancel_x, cancel_y;
    int cancel_w, cancel_h;
    int cancel_workspace;
    int mx, my;
    int ox, oy;
    int ow, oh;
    int oldw, oldh;
    int handle;
    Poswin *poswin;
};

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

void
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

void
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

static void
clientMovePointer (DisplayInfo *display_info, gint dx, gint dy, guint repeat)
{
    gint i;
    for (i = 0; i < repeat; ++i)
    {
        XWarpPointer (display_info->dpy, None, None, 0, 0, 0, 0, dx, dy);
    }
}

static void
clientSetHandle(MoveResizeData *passdata, int handle)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Client *c;
    int px, py;

    c = passdata->c;
    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    switch (handle)
    {
        case CORNER_BOTTOM_LEFT:
            px = frameX(c) + frameLeft(c) / 2;
            py = frameY(c) + frameHeight(c) - frameBottom(c) / 2;
            break;
        case CORNER_BOTTOM_RIGHT:
            px = frameX(c) + frameWidth(c) - frameRight(c) / 2;
            py = frameY(c) + frameHeight(c) - frameBottom(c) / 2;
            break;
        case CORNER_TOP_LEFT:
            px = frameX(c) + frameLeft(c) / 2;
            py = frameY(c);
            break;
        case CORNER_TOP_RIGHT:
            px = frameX(c) + frameWidth(c) - frameRight(c) / 2;
            py = frameY(c);
            break;
        case CORNER_COUNT + SIDE_LEFT:
            px = frameX(c) + frameLeft(c) / 2;
            py = frameY(c) + frameHeight(c) / 2;
            break;
        case CORNER_COUNT + SIDE_RIGHT:
            px = frameX(c) + frameWidth(c) - frameRight(c) / 2;
            py = frameY(c) + frameHeight(c) / 2;
            break;
        case CORNER_COUNT + SIDE_TOP:
            px = frameX(c) + frameWidth(c) / 2;
            py = frameY(c);
            break;
        case CORNER_COUNT + SIDE_BOTTOM:
            px = frameX(c) + frameWidth(c) / 2;
            py = frameY(c) + frameHeight(c) - frameBottom(c) / 2;
            break;
        default:
            px = frameX(c) + frameWidth(c) / 2;
            py = frameY(c) + frameHeight(c) / 2;
            break;
    }

    XWarpPointer (display_info->dpy, None, screen_info->xroot, 0, 0, 0, 0, px, py);
    /* Update internal data */
    passdata->handle = handle;
    passdata->mx = px;
    passdata->my = py;
    passdata->ox = c->x;
    passdata->oy = c->y;
    passdata->ow = c->width;
    passdata->oh = c->height;
}

/* clientConstrainRatio - adjust the given width and height to account for
   the constraints imposed by size hints

   The aspect ratio stuff, is borrowed from uwm's CheckConsistency routine.
 */

#define MAKE_MULT(a,b) ((b==1) ? (a) : (((int)((a)/(b))) * (b)) )
void
clientConstrainRatio (Client * c, int *w, int *h, int handle)
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
            ((handle == CORNER_COUNT + SIDE_TOP) || (handle == CORNER_COUNT + SIDE_BOTTOM)))
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
            ((handle == CORNER_COUNT + SIDE_LEFT) || (handle == CORNER_COUNT + SIDE_RIGHT)))
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
        int key_move;

        while (XCheckMaskEvent (display_info->dpy, KeyPressMask, xevent))
        {
            /* Update the display time */
            myDisplayUpdateCurrentTime (display_info, xevent);
        }

        key_move = 16;
        if ((screen_info->params->snap_to_border) || (screen_info->params->snap_to_windows))
        {
            key_move = MAX (key_move, screen_info->params->snap_width + 1);
        }
        if (xevent->xkey.keycode == screen_info->params->keys[KEY_LEFT].keycode)
        {
            clientMovePointer (display_info, -1, 0, key_move);
        }
        else if (xevent->xkey.keycode == screen_info->params->keys[KEY_RIGHT].keycode)
        {
            clientMovePointer (display_info, 1, 0, key_move);
        }
        else if (xevent->xkey.keycode == screen_info->params->keys[KEY_UP].keycode)
        {
            clientMovePointer (display_info, 0, -1, key_move);
        }
        else if (xevent->xkey.keycode == screen_info->params->keys[KEY_DOWN].keycode)
        {
            clientMovePointer (display_info, 0, 1, key_move);
        }
        else if (xevent->xkey.keycode == screen_info->params->keys[KEY_CANCEL].keycode)
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
        else
        {
            moving = FALSE;
        }
    }
    else if (xevent->type == ButtonRelease)
    {
        moving = FALSE;
        passdata->released = passdata->use_keys || (xevent->xbutton.button == passdata->button);
    }
    else if (xevent->type == MotionNotify)
    {
        while (XCheckMaskEvent (display_info->dpy, PointerMotionMask | ButtonMotionMask, xevent))
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
    else if (xevent->type == EnterNotify)
    {
        /* Ignore enter events */
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

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        return;
    }

    if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MOVE))
    {
        return;
    }

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
    passdata.button = 0;
    passdata.is_transient = clientIsValidTransientOrModal (c);
    passdata.move_resized = FALSE;

    if (ev && (ev->type == ButtonPress))
    {
        passdata.button = ev->xbutton.button;
        passdata.mx = ev->xbutton.x_root;
        passdata.my = ev->xbutton.y_root;
    }
    else
    {
        clientSetHandle(&passdata, NO_HANDLE);
        passdata.released = passdata.use_keys = TRUE;
    }

    g1 = myScreenGrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
    g2 = myScreenGrabPointer (screen_info, MOVERESIZE_EVENT_MASK,
                              myDisplayGetCursorMove (display_info),
                              myDisplayGetCurrentTime (display_info));
    if (!g1 || !g2)
    {
        TRACE ("grab failed in clientMove");

        gdk_beep ();
        myScreenUngrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
        myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));

        return;
    }

    passdata.poswin = NULL;
#ifdef SHOW_POSITION
    passdata.poswin = poswinCreate(screen_info->gscr);
    poswinSetPosition (passdata.poswin, c);
    poswinShow (passdata.poswin);
#endif /* SHOW_POSITION */

    /* Set window translucent while moving */
    if ((screen_info->params->move_opacity < 100) &&
        !(screen_info->params->box_move) &&
        !FLAG_TEST (c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED))
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

    if (!passdata.released)
    {
        /* If this is a drag-move, wait for the button to be released.
         * If we don't, we might get release events in the wrong place.
         */
        eventFilterPush (display_info->xfilter, clientButtonReleaseFilter, &passdata);
        gtk_main ();
        eventFilterPop (display_info->xfilter);
    }
    myScreenUngrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
    myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));

    if (passdata.grab && screen_info->params->box_move)
    {
        myDisplayUngrabServer (display_info);
    }
}

static gboolean
clientChangeHandle(MoveResizeData *passdata, int handle)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Client *c;
    Cursor cursor;
    gboolean grab;

    c = passdata->c;
    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    clientSetHandle(passdata, handle);
    if ((handle > NO_HANDLE) && (handle <= HANDLES_COUNT))
    {
        cursor = myDisplayGetCursorResize (display_info, handle);
    }
    else
    {
        cursor = myDisplayGetCursorMove (display_info);
    }
    grab = myScreenChangeGrabPointer (screen_info, MOVERESIZE_EVENT_MASK,
                                      cursor, myDisplayGetCurrentTime (display_info));

    return (grab);
}

static void
clientResizeConfigure (Client *c, int px, int py, int pw, int ph)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    XWindowChanges wc;

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

#ifdef HAVE_XSYNC
    if (c->xsync_waiting)
    {
        return;
    }
    else
    {
        if ((display_info->have_xsync) && (c->xsync_enabled) && (c->xsync_counter))
        {
            clientXSyncRequest (c);
        }
#endif /* HAVE_XSYNC */
        wc.x = c->x;
        wc.y = c->y;
        wc.width = c->width;
        wc.height = c->height;
        clientConfigure (c, &wc, CWX | CWY | CWWidth | CWHeight, NO_CFG_FLAG);
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

    move_top = ((passdata->handle == CORNER_TOP_RIGHT)
            || (passdata->handle == CORNER_TOP_LEFT)
            || (passdata->handle == CORNER_COUNT + SIDE_TOP)) ?
        1 : 0;
    move_bottom = ((passdata->handle == CORNER_BOTTOM_RIGHT)
            || (passdata->handle == CORNER_BOTTOM_LEFT)
            || (passdata->handle == CORNER_COUNT + SIDE_BOTTOM)) ?
        1 : 0;
    move_right = ((passdata->handle == CORNER_TOP_RIGHT)
            || (passdata->handle == CORNER_BOTTOM_RIGHT)
            || (passdata->handle == CORNER_COUNT + SIDE_RIGHT)) ?
        1 : 0;
    move_left = ((passdata->handle == CORNER_TOP_LEFT)
            || (passdata->handle == CORNER_BOTTOM_LEFT)
            || (passdata->handle == CORNER_COUNT + SIDE_LEFT)) ?
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
        int key_width_inc, key_height_inc;

        while (XCheckMaskEvent (display_info->dpy, KeyPressMask, xevent))
        {
            /* Update the display time */
            myDisplayUpdateCurrentTime (display_info, xevent);
        }

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

        if (xevent->xkey.keycode == screen_info->params->keys[KEY_UP].keycode)
        {
            if ((passdata->handle == CORNER_COUNT + SIDE_BOTTOM) ||
                (passdata->handle == CORNER_COUNT + SIDE_TOP))
            {
                clientMovePointer (display_info, 0, -1, key_height_inc);
            }
            else
            {
                clientChangeHandle (passdata, CORNER_COUNT + SIDE_TOP);
            }
        }
        else if (xevent->xkey.keycode == screen_info->params->keys[KEY_DOWN].keycode)
        {
            if ((passdata->handle == CORNER_COUNT + SIDE_BOTTOM) ||
                (passdata->handle == CORNER_COUNT + SIDE_TOP))
            {
                clientMovePointer (display_info, 0, 1, key_height_inc);
            }
            else
            {
                clientChangeHandle (passdata, CORNER_COUNT + SIDE_BOTTOM);
            }
        }
        else if (xevent->xkey.keycode == screen_info->params->keys[KEY_LEFT].keycode)
        {
            if ((passdata->handle == CORNER_COUNT + SIDE_LEFT) ||
                (passdata->handle == CORNER_COUNT + SIDE_RIGHT))
            {
                clientMovePointer (display_info, -1, 0, key_width_inc);
            }
            else
            {
                clientChangeHandle (passdata, CORNER_COUNT + SIDE_LEFT);
            }
        }
        else if (xevent->xkey.keycode == screen_info->params->keys[KEY_RIGHT].keycode)
        {
            if ((passdata->handle == CORNER_COUNT + SIDE_LEFT) ||
                (passdata->handle == CORNER_COUNT + SIDE_RIGHT))
            {
                clientMovePointer (display_info, 1, 0, key_width_inc);
            }
            else
            {
                clientChangeHandle (passdata, CORNER_COUNT + SIDE_RIGHT);
            }
        }
        else if (xevent->xkey.keycode == screen_info->params->keys[KEY_CANCEL].keycode)
        {
            resizing = FALSE;
            passdata->released = passdata->use_keys;

            if (screen_info->params->box_resize)
            {
                clientDrawOutline (c);
            }

            /* restore the pre-resize position & size */
            c->x = passdata->cancel_x;
            c->y = passdata->cancel_y;
            c->width = passdata->cancel_w;
            c->height = passdata->cancel_h;
            if (screen_info->params->box_resize)
            {
                clientDrawOutline (c);
            }
            else
            {
                clientResizeConfigure (c, prev_x, prev_y, prev_width, prev_height);
            }
        }
        else
        {
            resizing = FALSE;
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
            c->width = passdata->ow - (xevent->xmotion.x_root - passdata->mx);
        }
        else if (move_right)
        {
            c->width = passdata->ow + (xevent->xmotion.x_root - passdata->mx);
        }
        if (!FLAG_TEST (c->flags, CLIENT_FLAG_SHADED))
        {
            if (move_top)
            {
                c->height = passdata->oh - (xevent->xmotion.y_root - passdata->my);
            }
            else if (move_bottom)
            {
                c->height = passdata->oh + (xevent->xmotion.y_root - passdata->my);
            }
        }
        clientConstrainRatio (c, &c->width, &c->height, passdata->handle);

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
        resizing = FALSE;
        passdata->released = (passdata->use_keys || (xevent->xbutton.button == passdata->button));
    }
    else if ((xevent->type == UnmapNotify) && (xevent->xunmap.window == c->window))
    {
        resizing = FALSE;
    }
    else if (xevent->type == EnterNotify)
    {
        /* Ignore enter events */
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
clientResize (Client * c, int handle, XEvent * ev)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    XWindowChanges wc;
    MoveResizeData passdata;
    int w_orig, h_orig;
    Cursor cursor;
    gboolean g1, g2;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientResize");
    TRACE ("resizing client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        return;
    }

    if (FLAG_TEST_ALL (c->flags, CLIENT_FLAG_MAXIMIZED)
        && (screen_info->params->borderless_maximize))
    {
        return;
    }

    if (!FLAG_TEST_ALL (c->xfwm_flags, XFWM_FLAG_HAS_RESIZE | XFWM_FLAG_IS_RESIZABLE))
    {
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_MOVE))
        {
            clientMove (c, ev);
        }
        return;
    }

    passdata.c = c;
    passdata.cancel_x = passdata.ox = c->x;
    passdata.cancel_y = passdata.oy = c->y;
    passdata.cancel_w = passdata.ow = c->width;
    passdata.cancel_h = passdata.oh = c->height;
    passdata.use_keys = FALSE;
    passdata.grab = FALSE;
    passdata.released = FALSE;
    passdata.button = 0;
    passdata.handle = handle;
    w_orig = c->width;
    h_orig = c->height;

    if (ev && (ev->type == ButtonPress))
    {
        passdata.button = ev->xbutton.button;
        passdata.mx = ev->xbutton.x_root;
        passdata.my = ev->xbutton.y_root;
    }
    else
    {
        clientSetHandle (&passdata, handle);
        passdata.released = passdata.use_keys = TRUE;
    }
    if ((handle > NO_HANDLE) && (handle <= HANDLES_COUNT))
    {
        cursor = myDisplayGetCursorResize (display_info, passdata.handle);
    }
    else
    {
        cursor = myDisplayGetCursorMove (display_info);
    }

    g1 = myScreenGrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
    g2 = myScreenGrabPointer (screen_info, MOVERESIZE_EVENT_MASK,
                              cursor, myDisplayGetCurrentTime (display_info));

    if (!g1 || !g2)
    {
        TRACE ("grab failed in clientResize");

        gdk_beep ();
        myScreenUngrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
        myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));

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

    /* Set window translucent while resizing */
    if ((screen_info->params->resize_opacity < 100) &&
        !(screen_info->params->box_resize) &&
        !FLAG_TEST (c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED))
    {
        clientSetOpacity (c, c->opacity, OPACITY_RESIZE, OPACITY_RESIZE);
    }

    FLAG_SET (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING);
    TRACE ("entering resize loop");
    eventFilterPush (display_info->xfilter, clientResizeEventFilter, &passdata);
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

    if (!passdata.released)
    {
        /* If this is a drag-resize, wait for the button to be released.
         * If we don't, we might get release events in the wrong place.
         */
        eventFilterPush (display_info->xfilter, clientButtonReleaseFilter, &passdata);
        gtk_main ();
        eventFilterPop (display_info->xfilter);
    }
    myScreenUngrabKeyboard (screen_info, myDisplayGetCurrentTime (display_info));
    myScreenUngrabPointer (screen_info, myDisplayGetCurrentTime (display_info));

    if (passdata.grab && screen_info->params->box_resize)
    {
        myDisplayUngrabServer (display_info);
    }
}
