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
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>
#include <libxfce4util/libxfce4util.h> 

#include "screen.h"
#include "misc.h"
#include "client.h"
#include "placement.h"
#include "transients.h"
#include "workspaces.h"
#include "frame.h"
#include "netwm.h"

static unsigned long overlapX (int x0, int x1, int tx0, int tx1);
static unsigned long overlapY (int y0, int y1, int ty0, int ty1);
static unsigned long overlap (int x0, int y0, int x1, int y1, 
                              int tx0, int ty0, int tx1, int ty1);

static unsigned long
clientStrutAreaOverlap (int x, int y, int w, int h, Client * c)
{
    unsigned long sigma = 0;
    
    if (FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUT)
        && FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE))
    {
        sigma = overlap (x, y, x + w, y + h, 
                         0, c->struts[LEFT_START_Y], 
                         c->struts[LEFT], 
                         c->struts[LEFT_END_Y])
              + overlap (x, y, x + w, y + h,
                         gdk_screen_get_width (c->screen_info->gscr) - c->struts[RIGHT], 
                         c->struts[RIGHT_START_Y],
                         gdk_screen_get_width (c->screen_info->gscr), c->struts[RIGHT_END_Y])
              + overlap (x, y, x + w, y + h,
                         c->struts[TOP_START_X], 0, 
                         c->struts[TOP_END_X], 
                         c->struts[TOP])
              + overlap (x, y, x + w, y + h,
                         c->struts[BOTTOM_START_X], 
                         gdk_screen_get_height (c->screen_info->gscr) - c->struts[BOTTOM],
                         c->struts[BOTTOM_END_X], 
                         gdk_screen_get_height (c->screen_info->gscr));
    }
    return sigma;
}

/* Compute rectangle overlap area */

static unsigned long
overlapX (int x0, int x1, int tx0, int tx1)
{
    if (tx0 > x0)
    {
        x0 = tx0;
    }
    if (tx1 < x1)
    {
        x1 = tx1;
    }
    if (x1 <= x0)
    {
        return 0;
    }
    return (x1 - x0);
}

static unsigned long
overlapY (int y0, int y1, int ty0, int ty1)
{
    if (ty0 > y0)
    {
        y0 = ty0;
    }
    if (ty1 < y1)
    {
        y1 = ty1;
    }
    if (y1 <= y0)
    {
        return 0;
    }
    return (y1 - y0);
}

static unsigned long
overlap (int x0, int y0, int x1, int y1, int tx0, int ty0, int tx1, int ty1)
{
    /* Compute overlapping box */
    return (overlapX (x0, x1, tx0, tx1) * overlapY (y0, y1, ty0, ty1));
}

static void
clientAutoMaximize (Client * c)
{
    ScreenInfo *screen_info = NULL;
    GdkRectangle rect;
    gint monitor_nbr;
    int cx, cy;

    cx = frameX (c) + (frameWidth (c) / 2);
    cy = frameY (c) + (frameHeight (c) / 2);

    screen_info = c->screen_info;
    monitor_nbr = find_monitor_at_point (screen_info->gscr, cx, cy);
    gdk_screen_get_monitor_geometry (screen_info->gscr, monitor_nbr, &rect);

    if (!FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ) && (frameWidth (c) > rect.width))
    {
        FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_HORIZ);
    }

    if (!FLAG_TEST (c->flags, CLIENT_FLAG_MAXIMIZED_VERT) && (frameHeight (c) > rect.height))
    {
        FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED_VERT);
    }
}

void 
clientMaxSpace (ScreenInfo *screen_info, int *x, int *y, int *w, int *h)
{
    Client *c2 = NULL;
    int i, delta = 0;
    
    g_return_if_fail (x != NULL);
    g_return_if_fail (y != NULL);
    g_return_if_fail (w != NULL);
    g_return_if_fail (h != NULL);

    for (c2 = screen_info->clients, i = 0; i < screen_info->client_count; c2 = c2->next, i++)
    {
        if (FLAG_TEST (c2->flags, CLIENT_FLAG_HAS_STRUT)
            && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_VISIBLE))
        {
            /* Left */
            if (overlap (*x, *y, *x + *w, *y + *h, 
                         0, c2->struts[LEFT_START_Y], c2->struts[LEFT], c2->struts[LEFT_END_Y]))
            {
                delta = c2->struts[LEFT] - *x;
                *x = *x + delta;
                *w = *w - delta;
            }

            /* Right */
            if (overlap (*x, *y, *x + *w, *y + *h, 
                         gdk_screen_get_width (c2->screen_info->gscr) - c2->struts[RIGHT], c2->struts[RIGHT_START_Y],
                         gdk_screen_get_width (c2->screen_info->gscr), c2->struts[RIGHT_END_Y]))
            {
                delta = (*x + *w) - gdk_screen_get_width (c2->screen_info->gscr) + c2->struts[RIGHT];
                *w = *w - delta;
            }

            /* Top */
            if (overlap (*x, *y, *x + *w, *y + *h, 
                         c2->struts[TOP_START_X], 0, c2->struts[TOP_END_X], c2->struts[TOP]))
            {
                delta = c2->struts[TOP] - *y;
                *y = *y + delta;
                *h = *h - delta;
            }

            /* Bottom */
            if (overlap (*x, *y, *x + *w, *y + *h, 
                         c2->struts[BOTTOM_START_X], gdk_screen_get_height (c2->screen_info->gscr) - c2->struts[BOTTOM],
                         c2->struts[BOTTOM_END_X], gdk_screen_get_height (c2->screen_info->gscr)))
            {
                delta = (*y + *h) - gdk_screen_get_height (c2->screen_info->gscr) + c2->struts[BOTTOM];
                *h = *h - delta;
            }
        }
    }
}

gboolean
clientCkeckTitle (Client * c)
{
    Client *c2 = NULL;
    ScreenInfo *screen_info = NULL;
    int i, frame_x, frame_y, frame_width, frame_top;
    
    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_width = frameWidth (c);
    frame_top = frameTop (c);

    /* Struts and other partial struts */
    screen_info = c->screen_info;
    for (c2 = screen_info->clients, i = 0; i < screen_info->client_count; c2 = c2->next, i++)
    {
        if ((c2 != c) && clientStrutAreaOverlap(frame_x, frame_y, frame_width, frame_top, c2))
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* clientConstrainPos() is used when moving windows
   to ensure that the window stays accessible to the user
   
   Returns the position in which the window was constrained.
    CLIENT_CONSTRAINED_TOP    = 1<<0
    CLIENT_CONSTRAINED_BOTTOM = 1<<1
    CLIENT_CONSTRAINED_LEFT   = 1<<2
    CLIENT_CONSTRAINED_RIGHT  = 1<<3

 */
unsigned int
clientConstrainPos (Client * c, gboolean show_full)
{
    Client *c2 = NULL;
    ScreenInfo *screen_info = NULL;
    int i, cx, cy, disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_height, frame_width, frame_top, frame_left;
    int frame_x, frame_y, frame_visible;
    unsigned int ret = 0;
    GdkRectangle rect;
    gint monitor_nbr;

    g_return_val_if_fail (c != NULL, 0);
    TRACE ("entering clientConstrainPos %s",
        show_full ? "(with show full)" : "(w/out show full)");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    /* We use a bunch of local vars to reduce the overhead of calling other functions all the time */
    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_height = frameHeight (c);
    frame_width = frameWidth (c);
    frame_top = frameTop (c);
    frame_left = frameLeft (c);
    frame_visible = (frame_top ? frame_top : frame_height);

    cx = frame_x + (frame_width / 2);
    cy = frame_y + (frame_height / 2);

    screen_info = c->screen_info;
    monitor_nbr = find_monitor_at_point (screen_info->gscr, cx, cy);
    gdk_screen_get_monitor_geometry (screen_info->gscr, monitor_nbr, &rect);

    disp_x = rect.x;
    disp_y = rect.y;
    disp_max_x = rect.x + rect.width;
    disp_max_y = rect.y + rect.height;

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE ("ignoring constrained for client \"%s\" (0x%lx)", c->name,
            c->window);
        return 0;
    }
    if (show_full)
    {
        if (frame_x + frame_width > disp_max_x)
        {
            c->x = disp_max_x - frame_width + frame_left;
            frame_x = frameX (c);
            ret |= CLIENT_CONSTRAINED_RIGHT;
        }
        if (frame_x < disp_x)
        {
            c->x = disp_x + frame_left;
            frame_x = frameX (c);
            ret |= CLIENT_CONSTRAINED_LEFT;
        }
        if (frame_y + frame_height > disp_max_y)
        {
            c->y = disp_max_y - frame_height + frame_top;
            frame_y = frameY (c);
            ret |= CLIENT_CONSTRAINED_BOTTOM;
        }
        if (frame_y < disp_y)
        {
            c->y = disp_y + frame_top;
            frame_y = frameY (c);
            ret |= CLIENT_CONSTRAINED_TOP;
        }
        /* Struts and other partial struts */

        for (c2 = screen_info->clients, i = 0; i < screen_info->client_count; c2 = c2->next, i++)
        {
            if (FLAG_TEST (c2->flags, CLIENT_FLAG_HAS_STRUT)
                && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_VISIBLE)
                && (c2 != c))
            {
                /* Right */
                if (overlapY (frame_y, frame_y + frame_height,
                              c2->struts[RIGHT_START_Y], c2->struts[RIGHT_END_Y]))
                {
                    if (overlapX (frame_x, frame_x + frame_width,
                                  gdk_screen_get_width (screen_info->gscr) - c2->struts[RIGHT],
                                  gdk_screen_get_width (screen_info->gscr)))
                    {
                        c->x = gdk_screen_get_width (screen_info->gscr) - c2->struts[RIGHT] - frame_width + frame_left;
                        frame_x = frameX (c);
                        ret |= CLIENT_CONSTRAINED_RIGHT;
                    }
                }

                /* Bottom */
                if (overlapX (frame_x, frame_x + frame_width,
                              c2->struts[BOTTOM_START_X], c2->struts[BOTTOM_END_X]))
                {
                    if (overlapY (frame_y, frame_y + frame_height,
                                  gdk_screen_get_height (screen_info->gscr) - c2->struts[BOTTOM],
                                  gdk_screen_get_height (screen_info->gscr)))
                    {
                        c->y = gdk_screen_get_height (screen_info->gscr) - c2->struts[BOTTOM] - frame_height + frame_top;
                        frame_y = frameY (c);
                        ret |= CLIENT_CONSTRAINED_BOTTOM;

                    }
                }
            }
        }

        for (c2 = screen_info->clients, i = 0; i < screen_info->client_count; c2 = c2->next, i++)
        {
            if (FLAG_TEST (c2->flags, CLIENT_FLAG_HAS_STRUT)
                && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_VISIBLE)
                && (c2 != c))
            {
                /* Left */
                if (overlapY (frame_y, frame_y + frame_height,
                              c2->struts[LEFT_START_Y], c2->struts[LEFT_END_Y]))
                {
                    if (overlapX (frame_x, frame_x + frame_width,
                                  0, c2->struts[LEFT]))
                    {
                        c->x = c2->struts[LEFT] + frame_left;
                        frame_x = frameX (c);
                        ret |= CLIENT_CONSTRAINED_LEFT;
                    }
                }

                /* Top */
                if (overlapX (frame_x,
                              frame_x + frame_width,
                              c2->struts[TOP_START_X],
                              c2->struts[TOP_END_X]))
                {
                    if (overlapY (frame_y, frame_y + frame_height,
                                  0, c2->struts[TOP]))
                    {
                        c->y = c2->struts[TOP] + frame_top;
                        frame_y = frameY (c);
                        ret |= CLIENT_CONSTRAINED_TOP;
                    }
                }
            }
        }
    }
    else
    {
        if (frame_x + frame_width < disp_x + CLIENT_MIN_VISIBLE)
        {
            c->x = disp_x + CLIENT_MIN_VISIBLE - frame_width + frame_left;
            frame_x = frameX (c);
             ret |= CLIENT_CONSTRAINED_LEFT;
        }
        if (frame_x + CLIENT_MIN_VISIBLE > disp_max_x)
        {
            c->x = disp_max_x - CLIENT_MIN_VISIBLE + frame_left;
            frame_x = frameX (c);
             ret |= CLIENT_CONSTRAINED_RIGHT;
        }
        if (frame_y + frame_height < disp_y + CLIENT_MIN_VISIBLE)
        {
            c->y = disp_y + CLIENT_MIN_VISIBLE  - frame_height + frame_top;
            frame_y = frameY (c);
             ret |= CLIENT_CONSTRAINED_TOP;
        }
        if (frame_y + CLIENT_MIN_VISIBLE > disp_max_y)
        {
            c->y = disp_max_y - CLIENT_MIN_VISIBLE + frame_top;
            frame_y = frameY (c);
            ret |= CLIENT_CONSTRAINED_BOTTOM;            
        }
        if ((frame_y < disp_y) && (frame_y > disp_y - frame_top))
        {
            c->y = disp_y + frame_top;
            frame_y = frameY (c);
            ret |= CLIENT_CONSTRAINED_TOP;           
        }
        /* Struts and other partial struts */
        for (c2 = screen_info->clients, i = 0; i < screen_info->client_count; c2 = c2->next, i++)
        {
            if (FLAG_TEST (c2->flags, CLIENT_FLAG_HAS_STRUT)
                && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_VISIBLE)
                && (c2 != c))
            {
                /* Right */
                if (overlapY (frame_y, frame_y + frame_height,
                              c2->struts[RIGHT_START_Y], c2->struts[RIGHT_END_Y]))
                {
                    if (frame_x > gdk_screen_get_width (screen_info->gscr) - c2->struts[RIGHT] - CLIENT_MIN_VISIBLE)
                    {
                        c->x = gdk_screen_get_width (screen_info->gscr) - c2->struts[RIGHT] - CLIENT_MIN_VISIBLE + frame_left;
                        frame_x = frameX (c);
                        ret |= CLIENT_CONSTRAINED_RIGHT;
                    }
                }

                /* Left */
                if (overlapY (frame_y, frame_y + frame_height,
                              c2->struts[LEFT_START_Y], c2->struts[LEFT_END_Y]))
                {
                    if (frame_x + frame_width < c2->struts[LEFT] + CLIENT_MIN_VISIBLE)
                    {
                        c->x = c2->struts[LEFT] + CLIENT_MIN_VISIBLE - frame_width + frame_left;
                        frame_x = frameX (c);
                        ret |= CLIENT_CONSTRAINED_LEFT;
                    }
                }

                /* Bottom */
                if (overlapX (frame_x, frame_x + frame_width,
                              c2->struts[BOTTOM_START_X], c2->struts[BOTTOM_END_X]))
                {
                    if (frame_y > gdk_screen_get_height (screen_info->gscr) - c2->struts[BOTTOM] - CLIENT_MIN_VISIBLE)
                    {
                        c->y = gdk_screen_get_height (screen_info->gscr) - c2->struts[BOTTOM] - CLIENT_MIN_VISIBLE + frame_top;
                        frame_y = frameY (c);
                        ret |= CLIENT_CONSTRAINED_BOTTOM;
                    }
                }

                /* Top */
                if (overlapX (frame_x, frame_x + frame_width,
                              c2->struts[TOP_START_X], c2->struts[TOP_END_X]))
                {
                    if (overlapY (frame_y, frame_y + frame_visible, 0, c2->struts[TOP]))
                    {
                        c->y = c2->struts[TOP] + frame_top;
                        frame_y = frameY (c);
                        ret |= CLIENT_CONSTRAINED_TOP;
                    }
                    if (frame_y + frame_height < c2->struts[TOP] + CLIENT_MIN_VISIBLE)
                    {
                        c->y = c2->struts[TOP] + CLIENT_MIN_VISIBLE - frame_height + frame_top;
                        frame_y = frameY (c);
                        ret |= CLIENT_CONSTRAINED_TOP;
                    }
                }
            }
        }
    }
    return ret;
}

/* clientKeepVisible is used at initial mapping, to make sure
   the window is visible on screen. It also does coordonate
   translation in Xinerama to center window on physical screen
   Not to be confused with clientConstrainPos()
 */
void
clientKeepVisible (Client * c)
{
    int cx, cy;
    int diff_x, diff_y;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientKeepVisible");
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    cx = frameX (c) + (frameWidth (c) / 2);
    cy = frameY (c) + (frameHeight (c) / 2);
    
    /* Translate coodinates to center on physical screen */

    diff_x = abs (c->size->x - ((gdk_screen_get_width (c->screen_info->gscr) - c->width) / 2));
    diff_y = abs (c->size->y - ((gdk_screen_get_height (c->screen_info->gscr) - c->height) / 2));

    if (((gdk_screen_get_n_monitors (c->screen_info->gscr) > 1) && (diff_x < 25) && (diff_y < 25)) ||
        ((frameX (c) == 0) && (frameY (c) == 0) && (c->type & (WINDOW_TYPE_DIALOG)) && !clientIsTransient (c)))
    {
        GdkRectangle rect;
        gint monitor_nbr;
        
        /* We consider that the windows is centered on screen,
         * Thus, will move it so its center on the current
         * physical screen
         */
        getMouseXY (c->screen_info, c->screen_info->xroot, &cx, &cy);
        
        monitor_nbr = find_monitor_at_point (c->screen_info->gscr, cx, cy);
        gdk_screen_get_monitor_geometry (c->screen_info->gscr, monitor_nbr, &rect);

        c->x = rect.x + (rect.width - c->width) / 2;
        c->y = rect.y + (rect.height - c->height) / 2;
    }
    clientConstrainPos (c, TRUE);
}

static void
smartPlacement (Client * c, int full_x, int full_y, int full_w, int full_h)
{
    Client *c2;
    ScreenInfo *screen_info;
    int test_x = 0, test_y = 0;
    int xmax, ymax, best_x, best_y, i;
    int frame_x, frame_y, frame_height, frame_width, frame_left, frame_top;
    unsigned long best_overlaps = 0;
    gboolean first = TRUE;

    g_return_if_fail (c != NULL);
    TRACE ("entering smartPlacement");

    screen_info = c->screen_info;
    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_height = frameHeight (c);
    frame_width = frameWidth (c);
    frame_left = frameLeft(c);
    frame_top = frameTop (c);

    xmax = full_x + full_w - c->width - frameRight (c);
    ymax = full_y + full_h - c->height - frameBottom (c);
    best_x = full_x + frameLeft (c);
    best_y = full_y + frameTop (c);

    for (test_y = full_y + frameTop (c); test_y <= ymax;
        test_y += 8)
    {
        for (test_x = full_x + frameLeft (c);
            test_x <= xmax; test_x += 8)
        {
            unsigned long count_overlaps = 0;
            TRACE ("analyzing %i clients", screen_info->client_count);
            for (c2 = screen_info->clients, i = 0; i < screen_info->client_count; c2 = c2->next, i++)
            {
                if ((c2 != c) && (c2->type != WINDOW_DESKTOP)
                    && (c->win_workspace == c2->win_workspace)
                    && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_VISIBLE))
                {
                    count_overlaps += overlap (test_x - frame_left, 
                                               test_y - frame_top, 
                                               test_x - frame_left + frame_width,
                                               test_y - frame_top + frame_height, 
                                               frameX (c2), 
                                               frameY (c2),
                                               frameX (c2) + frameWidth (c2),
                                               frameY (c2) + frameHeight (c2));
                }
            }
            TRACE ("overlaps so far is %u", (unsigned int) count_overlaps);
            if (count_overlaps == 0)
            {
                TRACE ("overlaps is 0 so it's the best we can get");
                c->x = test_x;
                c->y = test_y;
                clientAutoMaximize (c);

                return;
            }
            else if ((count_overlaps < best_overlaps) || (first))
            {
                TRACE ("overlaps %u is better than the best we have %u",
                    (unsigned int) count_overlaps,
                    (unsigned int) best_overlaps);
                best_x = test_x;
                best_y = test_y;
                best_overlaps = count_overlaps;
            }
            if (first)
            {
                first = FALSE;
            }
        }
    }
    c->x = best_x;
    c->y = best_y;
}

static void
centerPlacement (Client * c, int full_x, int full_y, int full_w, int full_h)
{
    g_return_if_fail (c != NULL);
    TRACE ("entering centerPlacement");

    c->x = MAX (full_x + frameLeft(c) + (full_w - frameWidth(c)) / 2, full_x + frameLeft(c));
    c->y = MAX (full_y + frameTop(c) + (full_h - frameHeight(c)) / 2, full_y + frameTop(c));
}

void
clientInitPosition (Client * c)
{
    Client *c2;
    ScreenInfo *screen_info;
    int full_x, full_y, full_w, full_h, msx, msy;
    GdkRectangle rect;
    gint monitor_nbr;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientInitPosition");

    clientGravitate (c, APPLY);

    screen_info = c->screen_info;
    if ((c->size->flags & (PPosition | USPosition)) ||
        (c->type & (WINDOW_TYPE_DONT_PLACE)) ||
        ((c->type & (WINDOW_TYPE_DIALOG)) && !clientIsTransient (c)))
    {
        if (CONSTRAINED_WINDOW (c))
        {
            clientKeepVisible (c);
            clientAutoMaximize (c);
        }

        return;
    }

    if (clientIsTransient (c) && (c2 = clientGetTransient (c)))
    {
        /* Center transient relative to their parent window */
        c->x = c2->x + (c2->width - c->width) / 2;
        c->y = c2->y + (c2->height - c->height) / 2;
        if (CONSTRAINED_WINDOW (c))
        {
            clientKeepVisible (c);
            clientAutoMaximize (c);
        }

        return;
    }

    getMouseXY (screen_info, screen_info->xroot, &msx, &msy);

    monitor_nbr = find_monitor_at_point (screen_info->gscr, msx, msy);
    gdk_screen_get_monitor_geometry (screen_info->gscr, monitor_nbr, &rect);
    
    full_x = MAX (screen_info->params->xfwm_margins[LEFT], rect.x);
    full_y = MAX (screen_info->params->xfwm_margins[TOP], rect.y);
    full_w = MIN (gdk_screen_get_width (screen_info->gscr) - screen_info->params->xfwm_margins[RIGHT], 
                  rect.x + rect.width) - full_x;
    full_h = MIN (gdk_screen_get_height (screen_info->gscr) - screen_info->params->xfwm_margins[BOTTOM], 
                  rect.y + rect.height) - full_y;

    /* Adjust size to the widest size available, not covering struts */
    clientMaxSpace (screen_info, &full_x, &full_y, &full_w, &full_h);

    /* 
       If the windows is smaller than the given ratio of the available screen area, 
       or if the window is larger than the screen area or if the given ratio is higher 
       than 100% place the window at the center.
       Otherwise, place the window "smartly", using the good old CPU consuming algorithm...
     */
    if ((screen_info->params->placement_ratio > 100) ||
        ((frameWidth(c) >= full_w) && (frameHeight(c) >= full_h)) || 
        (100  * frameWidth(c) * frameHeight(c)) < (screen_info->params->placement_ratio * full_w * full_h))
    {
        centerPlacement (c, full_x, full_y, full_w, full_h);
    }
    else
    {
        smartPlacement (c, full_x, full_y, full_w, full_h);
    }

    clientAutoMaximize (c);
}
