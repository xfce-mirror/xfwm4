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


        xfwm4    - (c) 2002-2022 Olivier Fourdan

 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>
#include <libxfce4util/libxfce4util.h>

#include <common/xfwm-common.h>

#include "screen.h"
#include "misc.h"
#include "client.h"
#include "placement.h"
#include "transients.h"
#include "workspaces.h"
#include "frame.h"
#include "netwm.h"

#define USE_CLIENT_STRUTS(c) (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE) && \
                              FLAG_TEST (c->flags, CLIENT_FLAG_HAS_STRUT))

#define DOCK_RESERVE(c) (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_VISIBLE) && \
                         (c->type == WINDOW_DOCK) && \
                         (c->screen_info->params->dock_reserve))

/* Compute rectangle overlap area */

static inline unsigned long
segment_overlap (int x0, int x1, int tx0, int tx1)
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

static inline unsigned long
overlap (int x0, int y0, int x1, int y1, int tx0, int ty0, int tx1, int ty1)
{
    /* Compute overlapping box */
    return (segment_overlap (x0, x1, tx0, tx1)
            * segment_overlap (y0, y1, ty0, ty1));
}

static inline void
set_rectangle (GdkRectangle * rect, gint x, gint y, gint width, gint height)
{
    if (rect == NULL)
        return;

    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;
}

gboolean
strutsToRectangles (Client *c,
                    GdkRectangle *left,
                    GdkRectangle *right,
                    GdkRectangle *top,
                    GdkRectangle *bottom)
{
    ScreenInfo *screen_info;

    g_return_val_if_fail (c != NULL, FALSE);
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;

    /* always zero out, even if we the window doesn't have struts, so callers may
       don't need to check for the retval explicitly. */
    set_rectangle(left,   0, 0, 0, 0);
    set_rectangle(right,  0, 0, 0, 0);
    set_rectangle(top,    0, 0, 0, 0);
    set_rectangle(bottom, 0, 0, 0, 0);

    if (!USE_CLIENT_STRUTS (c))
    {
        return FALSE;
    }

    set_rectangle (left,
                   0,
                   c->struts[STRUTS_LEFT_START_Y],
                   c->struts[STRUTS_LEFT],
                   c->struts[STRUTS_LEFT_END_Y] - c->struts[STRUTS_LEFT_START_Y] + 1);
    set_rectangle (right,
                   screen_info->width - c->struts[STRUTS_RIGHT],
                   c->struts[STRUTS_RIGHT_START_Y],
                   c->struts[STRUTS_RIGHT],
                   c->struts[STRUTS_RIGHT_END_Y] - c->struts[STRUTS_RIGHT_START_Y] + 1);
    set_rectangle (top,
                   c->struts[STRUTS_TOP_START_X],
                   0,
                   c->struts[STRUTS_TOP_END_X] - c->struts[STRUTS_TOP_START_X] + 1,
                   c->struts[STRUTS_TOP]);
    set_rectangle (bottom,
                   c->struts[STRUTS_BOTTOM_START_X],
                   screen_info->height - c->struts[STRUTS_BOTTOM],
                   c->struts[STRUTS_BOTTOM_END_X] - c->struts[STRUTS_BOTTOM_START_X] + 1,
                   c->struts[STRUTS_BOTTOM]);

    return TRUE;
}

static gboolean
areasOnSameMonitor (ScreenInfo *screen_info,
                    GdkRectangle *area1,
                    GdkRectangle *area2)
{
    GdkRectangle monitor;
    int num_monitors, i;

    g_return_val_if_fail (screen_info != NULL, FALSE);
    g_return_val_if_fail (area1 != NULL, FALSE);
    g_return_val_if_fail (area2 != NULL, FALSE);

    num_monitors = xfwm_get_n_monitors (screen_info->gscr);
    for (i = 0; i < num_monitors; i++)
    {
        xfwm_get_monitor_geometry (screen_info->gscr, i, &monitor, TRUE);
        if (gdk_rectangle_intersect (area1, &monitor, NULL) &&
            gdk_rectangle_intersect (area2, &monitor, NULL))
        {
            return TRUE;
        }
    }

    return FALSE;
}

gboolean
clientsHaveOverlap (Client *c1, Client *c2)
{
    GdkRectangle win1;
    GdkRectangle win2;

    if (c1->screen_info != c2->screen_info)
    {
        return FALSE;
    }

    set_rectangle (&win1,
                   frameExtentX (c1),
                   frameExtentY (c1),
                   frameExtentWidth (c1),
                   frameExtentHeight (c1));

    set_rectangle (&win2,
                   frameExtentX (c2),
                   frameExtentY (c2),
                   frameExtentWidth (c2),
                   frameExtentHeight (c2));

    return gdk_rectangle_intersect (&win1, &win2, NULL);
}

GdkRectangle
clientMaxSpaceForGeometry (Client *c, GdkRectangle rect)
{
    GdkRectangle dest = { 0 };

    g_return_val_if_fail (c != NULL, dest);

    myScreenMaxSpaceForGeometry (c->screen_info, &rect, &dest);
    clientMaxSpace (c, &dest);
    return dest;
}

static void
applyClientStrutstoArea (Client *c, GdkRectangle *area)
{
    GdkRectangle top, left, right, bottom, new_area;

    g_return_if_fail (c != NULL);
    g_return_if_fail (area != NULL);

    new_area = *area;

    if (strutsToRectangles (c, &left, &right, &top, &bottom))
    {
        new_area = xfwm_rect_shrink_reserved(new_area, left);
        new_area = xfwm_rect_shrink_reserved(new_area, right);
        new_area = xfwm_rect_shrink_reserved(new_area, top);
        new_area = xfwm_rect_shrink_reserved(new_area, bottom);
    }

    if (DOCK_RESERVE(c))
    {
        GdkRectangle r = (GdkRectangle) {
            .x      = frameExtentX (c),
            .y      = frameExtentY (c),
            .width  = frameExtentWidth (c),
            .height = frameExtentHeight (c),
        };
        new_area = xfwm_rect_shrink_reserved(new_area, r);
    }

    *area = new_area;
}

void
geometryMaxSpace (ScreenInfo *screen_info, GdkRectangle *area)
{
    Client *c;
    unsigned int i;

    TRACE ("entering");

    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, i++)
    {
        GdkRectangle win = (GdkRectangle) {
            .x      = frameExtentX (c),
            .y      = frameExtentY (c),
            .width  = frameExtentWidth (c),
            .height = frameExtentHeight (c)
        };
        if (areasOnSameMonitor (screen_info, area, &win) && gdk_rectangle_intersect (&win, area, NULL))
        {
            applyClientStrutstoArea (c, area);
        }
    }
}

void
clientMaxSpace (Client *c, GdkRectangle *area)
{
    ScreenInfo *screen_info;
    Client *c2;
    guint i;

    g_return_if_fail (c != NULL);
    g_return_if_fail (area != NULL);

    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;

    for (c2 = screen_info->clients, i = 0; i < screen_info->client_count; c2 = c2->next, i++)
    {
        applyClientStrutstoArea (c2, area);
    }
}

static cairo_region_t *getAvailableScreen(Client *c)
{
    cairo_region_t *available = cairo_region_create();
    GdkRectangle m;
    ScreenInfo *screen_info = c->screen_info;
    GdkScreen *gscr = screen_info->gscr;
    guint i;
    Client *c2;

    for (i=0; xfwm_get_monitor_geometry(gscr, i, &m, FALSE); i++)
    {
        cairo_region_union_rectangle(available, &m);
    }

    for (c2 = screen_info->clients, i = 0; i < screen_info->client_count; c2 = c2->next, i++)
    {
        GdkRectangle top, left, right, bottom;
        if (c2 == c)
            continue;

        if (strutsToRectangles (c2, &left, &right, &top, &bottom))
        {
            if (left.width && left.height) {
                cairo_region_subtract_rectangle(available, &left);
            }
            if (right.width && right.height) {
                cairo_region_subtract_rectangle(available, &right);
            }
            if (top.width && top.height) {
                cairo_region_subtract_rectangle(available, &top);
            }
            if (bottom.width && bottom.height) {
                cairo_region_subtract_rectangle(available, &bottom);
            }
        }

        if (DOCK_RESERVE(c2)) {
            GdkRectangle win = (GdkRectangle) {
                .x = frameExtentX (c),
                .y = frameExtentY (c),
                .width  = frameExtentWidth (c),
                .height = frameExtentHeight (c),
            };
            DBG("adding dock window: %d %d %d %d", win.x, win.y, win.width, win.height);
            cairo_region_subtract_rectangle(available, &win);
        }
    }

    return available;
}

static cairo_region_t *clientGetVisible(Client *c, GdkRectangle win)
{
    cairo_region_t *visible = getAvailableScreen(c);

    /* compute what's still visible from the window */
    cairo_region_intersect_rectangle(visible, &win);
    return visible;
}

static gboolean check_keep_visible(cairo_region_t *visible, GdkPoint keep)
{
    int num_visible = cairo_region_num_rectangles(visible);
    for (int x=0; x<num_visible; x++) {
        GdkRectangle r;
        cairo_region_get_rectangle(visible, x, &r);
        if ((r.width >= keep.x) && (r.height >= keep.y))
            return TRUE;
    }
    return FALSE;
}

static gboolean try_solution(Client *c, GdkRectangle frame, GdkPoint keep, int x, int y)
{
    gboolean ret = FALSE;
    cairo_region_t *region;

    frame.x += x;
    frame.y += y;

    region = clientGetVisible(c, frame);
    ret = check_keep_visible(region, keep);
    cairo_region_destroy(region);
    return ret;
}

static inline int gt0(int x)
{
    return (x > 0 ? x : 0);
}

static int check_visible(Client *c, GdkRectangle frame, GdkPoint keep, gboolean resize)
{
    cairo_region_t *visible = clientGetVisible(c, frame);
    int num_visible = cairo_region_num_rectangles(visible);
    int ret = 0;
    int solution = INT_MAX;

    if (check_keep_visible(visible, keep)) {
        ret = 0;
        goto out;
    }

    for (int x=0; x<num_visible; x++) {
        GdkRectangle r;
        int dist_x = gt0(keep.x - r.width);
        int dist_y = gt0(keep.y - r.height);
        cairo_region_get_rectangle(visible, x, &r);
        if (try_solution(c, frame, keep, dist_x, 0) && (dist_x < solution)) {
            solution = dist_x;
            ret = CLIENT_CONSTRAINED_LEFT;
        }
        if (try_solution(c, frame, keep, -dist_x, 0) && (dist_x < solution)) {
            solution = dist_x;
            ret = CLIENT_CONSTRAINED_RIGHT;
        }
        if (try_solution(c, frame, keep, 0, dist_y) && (dist_y < solution)) {
            solution = dist_y;
            ret = CLIENT_CONSTRAINED_TOP;
        }
        if (try_solution(c, frame, keep, 0, -dist_y) && (dist_y < solution)) {
            solution = dist_y;
            ret = CLIENT_CONSTRAINED_BOTTOM;
        }
    }

    switch (ret) {
        case CLIENT_CONSTRAINED_LEFT:
            c->x += solution;
        break;
        case CLIENT_CONSTRAINED_RIGHT:
            c->x -= solution;
        break;
        case CLIENT_CONSTRAINED_TOP:
            c->y += solution;
        break;
        case CLIENT_CONSTRAINED_BOTTOM:
            c->y -= solution;
        break;
        default:
            if (resize) {
                c->width = c->applied_geometry.width;
                c->height = c->applied_geometry.height;
            } else {
                c->x = c->applied_geometry.x;
                c->y = c->applied_geometry.y;
            }
        break;
    }

out:
    cairo_region_destroy(visible);
    return ret;
}

/* clientConstrainPos() is used when moving windows
   to ensure that the window stays accessible to the user

   Returns the position in which the window was constrained.
    CLIENT_CONSTRAINED_TOP    = 1<<0
    CLIENT_CONSTRAINED_BOTTOM = 1<<1
    CLIENT_CONSTRAINED_LEFT   = 1<<2
    CLIENT_CONSTRAINED_RIGHT  = 1<<3

   FIXME: seems to ignore borders when called by clientInitPosition()

    @param c            the client object
    @param show_full    window shall remain fully visible
    @param resize       triggered by a window resize
 */
unsigned int
clientConstrainPos (Client * c, gboolean show_full, gboolean resize)
{
    ScreenInfo *screen_info;
    gint frame_top;
    gint title_visible;
    GdkRectangle win;
    GdkPoint keep_visible;

    g_return_val_if_fail (c != NULL, 0);

    TRACE ("client \"%s\" (0x%lx) %s", c->name, c->window,
        show_full ? "(with show full)" : "(w/out show full)");

    screen_info = c->screen_info;

    /* We use a bunch of local vars to reduce the overhead of calling other functions all the time */
    frame_top = frameExtentTop (c);
    set_rectangle (&win, frameExtentX (c), frameExtentY (c), frameExtentWidth (c), frameExtentHeight (c));

    title_visible = frame_top;
    if (title_visible <= 0)
    {
        /* CSD window, use the title height from the theme */
        title_visible = frameDecorationTop (screen_info);
    }

    if (show_full) {
        keep_visible.x = win.width;
        keep_visible.y = win.height;
    } else {
        keep_visible.x = keep_visible.y = MAX (title_visible, CLIENT_MIN_VISIBLE);
    }

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
        TRACE ("ignoring constrained for client \"%s\" (0x%lx)", c->name,
            c->window);
        return 0;
    }

    return check_visible(c, win, keep_visible, resize);
}

/* clientKeepVisible is used at initial mapping, to make sure
   the window is visible on screen. It also does coordonate
   translation in Xinerama to center window on physical screen
   Not to be confused with clientConstrainPos()
 */
static void
clientKeepVisible (Client * c, gint n_monitors, GdkRectangle *monitor_rect)
{
    gboolean centered;
    int diff_x, diff_y;

    g_return_if_fail (c != NULL);
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    centered = FALSE;
    /* We only center dialogs */
    if (c->type & (WINDOW_TYPE_DIALOG))
    {
        if ((c->size->x == 0) && (c->size->y == 0))
        {
            /* Dialogs that place themselves in (0,0) will be centered */
            centered = TRUE;
        }
        else if ((n_monitors > 1) && (c->size->x > 0) && (c->size->y > 0))
        {
            /* Check if the window is centered on the whole screen */
            diff_x = ABS(c->size->x - ((c->screen_info->width - c->size->width) / 2));
            diff_y = ABS(c->size->y - ((c->screen_info->height - c->size->height) / 2));
            centered = ((diff_x < 25) && (diff_y < 25));
        }
    }
    if (centered)
    {
        /* We consider that the windows is centered on screen,
         * Thus, will move it so its center on the current
         * physical screen
         */
        c->x = monitor_rect->x + (monitor_rect->width - c->width) / 2;
        c->y = monitor_rect->y + (monitor_rect->height - c->height) / 2;
    }
    clientConstrainPos (c, TRUE, FALSE);
}

static void
clientAutoMaximize (Client * c, int full_w, int full_h)
{
    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN) ||
        !FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER))
    {
        /*
         * Fullscreen or undecorated windows should not be
         * automatically maximized...
         */
        return;
    }

    if ((frameExtentWidth (c) >= full_w) && (frameExtentHeight (c) >= full_h))
    {
        DBG ("The application \"%s\" has requested a window size (%ux%u) "
             "equal or larger than the actual size available in the workspace (%ux%u), "
             "the window will be set as maximized.", c->name,
             frameExtentWidth (c), frameExtentHeight (c),
             full_w, full_h);
        clientSaveSizePos (c);
        FLAG_SET (c->flags, CLIENT_FLAG_MAXIMIZED | CLIENT_FLAG_RESTORE_SIZE_POS);
    }
}

static void
smartPlacement (Client * c, int full_x, int full_y, int full_w, int full_h)
{
    Client *c2;
    ScreenInfo *screen_info;
    gfloat best_overlaps;
    guint i;
    gint test_x, test_y, xmax, ymax, best_x, best_y;
    gint frame_height, frame_width, frame_left, frame_top;
    gint c2_x, c2_y;
    gint xmin, ymin;

    g_return_if_fail (c != NULL);

    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    frame_height = frameExtentHeight (c);
    frame_width = frameExtentWidth (c);
    frame_left = frameExtentLeft(c);
    frame_top = frameExtentTop (c);

    /* max coordinates (bottom-right) */
    xmax = full_x + full_w - c->width - frameExtentRight (c);
    ymax = full_y + full_h - c->height - frameExtentBottom (c);

    /* min coordinates (top-left) */
    xmin = full_x + frameExtentLeft (c);
    ymin = full_y + frameExtentTop (c);

    /* start with worst-case position at top-left */
    best_overlaps = G_MAXFLOAT;
    best_x = xmin;
    best_y = ymin;

    TRACE ("analyzing %i clients", screen_info->client_count);

    test_y = ymin;
    do
    {
        gint next_test_y = G_MAXINT;
        gboolean first_test_x = TRUE;

        TRACE ("testing y position %d", test_y);

        test_x = xmin;
        do
        {
            gfloat count_overlaps = 0.0;
            gint next_test_x = G_MAXINT;
            gint c2_next_test_x;
            gint c2_next_test_y;
            gint c2_frame_height;
            gint c2_frame_width;

            TRACE ("testing x position %d", test_x);

            for (c2 = screen_info->clients, i = 0; i < screen_info->client_count; c2 = c2->next, i++)
            {
                if ((c2 != c) && (c2->type != WINDOW_DESKTOP)
                    && (c->win_workspace == c2->win_workspace)
                    && FLAG_TEST (c2->xfwm_flags, XFWM_FLAG_VISIBLE))
                {
                    c2_x = frameExtentX (c2);
                    c2_frame_width = frameExtentWidth (c2);
                    if (c2_x >= full_x + full_w
                        || c2_x + c2_frame_width < full_x)
                    {
                        /* skip clients on right-of or left-of monitor */
                        continue;
                    }

                    c2_y = frameExtentY (c2);
                    c2_frame_height = frameExtentHeight (c2);
                    if (c2_y >= full_y + full_h
                        || c2_y + c2_frame_height < full_y)
                    {
                        /* skip clients on above-of or below-of monitor */
                        continue;
                    }

                    count_overlaps += overlap (test_x - frame_left,
                                               test_y - frame_top,
                                               test_x - frame_left + frame_width,
                                               test_y - frame_top + frame_height,
                                               c2_x,
                                               c2_y,
                                               c2_x + c2_frame_width,
                                               c2_y + c2_frame_height);

                    /* find the next x boundy for the step */
                    if (test_x > c2_x)
                    {
                        /* test location is beyond the x of the window,
                         * take the window right corner as next target */
                        c2_x += c2_frame_width;
                    }
                    c2_next_test_x = MIN (c2_x, xmax);
                    if (c2_next_test_x < next_test_x
                        && c2_next_test_x > test_x)
                    {
                        /* set new optimal next x step position */
                        next_test_x = c2_next_test_x;
                    }

                    if (first_test_x)
                    {
                        /* find the next y boundry step */
                        if (test_y > c2_y)
                        {
                            /* test location is beyond the y of the window,
                             * take the window bottom corner as next target */
                            c2_y += c2_frame_height;
                        }
                        c2_next_test_y = MIN (c2_y, ymax);
                        if (c2_next_test_y < next_test_y
                            && c2_next_test_y > test_y)
                        {
                            /* set new optimal next y step position */
                            next_test_y = c2_next_test_y;
                        }
                    }
                }
            }

            /* don't look for the next y boundry this x row */
            first_test_x = FALSE;

            if (count_overlaps < best_overlaps)
            {
                /* found position with less overlap */
                best_x = test_x;
                best_y = test_y;
                best_overlaps = count_overlaps;

                if (count_overlaps == 0.0f)
                {
                    /* overlap is ideal, stop searching */
                    TRACE ("found position without overlap");
                    goto found_best;
                }
            }

            if (G_LIKELY (next_test_x != G_MAXINT))
            {
                test_x = MAX (next_test_x, next_test_x + frameExtentLeft (c));
                if (test_x > xmax)
                {
                   /* always clamp on the monitor */
                   test_x = xmax;
                }
            }
            else
            {
                test_x++;
            }
        }
        while (test_x <= xmax);

        if (G_LIKELY (next_test_y != G_MAXINT))
        {
            test_y = MAX (next_test_y, next_test_y + frameExtentTop (c));
            if (test_y > ymax)
            {
                /* always clamp on the monitor */
                test_y = ymax;
            }
        }
        else
        {
            test_y++;
        }
    }
    while (test_y <= ymax);

    found_best:

    TRACE ("overlaps %f at %d,%d (x,y)", best_overlaps, best_x, best_y);

    c->x = best_x;
    c->y = best_y;
}

static void
centerPlacement (Client * c, int full_x, int full_y, int full_w, int full_h)
{
    g_return_if_fail (c != NULL);

    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    c->x = MAX (full_x + frameExtentLeft(c) + (full_w - frameExtentWidth(c)) / 2, full_x + frameExtentLeft(c));
    c->y = MAX (full_y + frameExtentTop(c) + (full_h - frameExtentHeight(c)) / 2, full_y + frameExtentTop(c));
}

static void
mousePlacement (Client * c, int full_x, int full_y, int full_w, int full_h, int mx, int my)
{
    g_return_if_fail (c != NULL);

    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    c->x = mx + frameExtentLeft(c) - frameExtentWidth(c) / 2;
    c->y = my + frameExtentTop(c) - frameExtentHeight(c) / 2;

    c->x = MIN (c->x, full_x + full_w - frameExtentWidth(c) + frameExtentLeft(c));
    c->y = MIN (c->y, full_y + full_h - frameExtentHeight(c) + frameExtentTop(c));

    c->x = MAX (c->x, full_x + frameExtentLeft(c));
    c->y = MAX (c->y, full_y + frameExtentTop(c));
}

void
clientInitPosition (Client * c)
{
    ScreenInfo *screen_info;
    Client *c2;
    GdkRectangle rect, full;
    int msx, msy;
    gint n_monitors;
    gboolean place;
    gboolean position;
    gboolean is_transient;

    g_return_if_fail (c != NULL);

    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    msx = 0;
    msy = 0;
    position = (c->size->flags & (PPosition | USPosition));

    n_monitors = myScreenGetNumMonitors (c->screen_info);
    getMouseXY (screen_info, &msx, &msy);
    myScreenFindMonitorAtPoint (screen_info, msx, msy, &rect);
    is_transient = clientIsTransient (c);

    if (position || is_transient || (c->type & (WINDOW_TYPE_DONT_PLACE | WINDOW_TYPE_DIALOG)))
    {
        if (!position && is_transient && (c2 = clientGetTransient (c)))
        {
            /* Center transient relative to their parent window */
            c->x = c2->x + (c2->width - c->width) / 2;
            c->y = c2->y + (c2->height - c->height) / 2;
        }

        if (position && n_monitors > 1)
        {
            msx = frameExtentX (c) + (frameExtentWidth (c) / 2);
            msy = frameExtentY (c) + (frameExtentHeight (c) / 2);
            myScreenFindMonitorAtPoint (screen_info, msx, msy, &rect);
        }

        if (CONSTRAINED_WINDOW (c))
        {
            clientKeepVisible (c, n_monitors, &rect);
        }
        place = FALSE;
    }
    else
    {
        place = TRUE;
    }

    /* Adjust size to the widest size available, not covering struts */
    myScreenMaxSpaceForGeometry (c->screen_info, &rect, &full);
    geometryMaxSpace(c->screen_info, &full);

    /*
       If the windows is smaller than the given ratio of the available screen area,
       or if the window is larger than the screen area or if the given ratio is higher
       than 100% place the window at the center.
       Otherwise, place the window "smartly", using the good old CPU consuming algorithm...
     */
    if (place)
    {
        if ((screen_info->params->placement_ratio >= 100) ||
            (100 * frameExtentWidth(c) * frameExtentHeight(c)) <
                (screen_info->params->placement_ratio * full.width * full.height))
        {
            if (screen_info->params->placement_mode == PLACE_MOUSE)
            {
                mousePlacement (c, full.x, full.y, full.width, full.height, msx, msy);
            }
            else
            {
                centerPlacement (c, full.x, full.y, full.width, full.height);
            }
        }
        else if ((frameExtentWidth(c) >= full.width) && (frameExtentHeight(c) >= full.height))
        {
            centerPlacement (c, full.x, full.y, full.width, full.height);
        }
        else
        {
            smartPlacement (c, full.x, full.y, full.width, full.height);
        }
    }

    if (c->type & WINDOW_REGULAR_FOCUSABLE)
    {
        clientAutoMaximize (c, full.width, full.height);
    }
}

void
clientFill (Client * c, int fill_type)
{
    ScreenInfo *screen_info;
    Client *east_neighbour;
    Client *west_neighbour;
    Client *north_neighbour;
    Client *south_neighbour;
    Client *c2;
    GdkRectangle rect, full, tmp;
    XWindowChanges wc;
    unsigned short mask;
    guint i;
    gint cx, cy;

    g_return_if_fail (c != NULL);

    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    /* don't change anything if the window isn't managed by us */
    if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MANAGED))
    {
        return;
    }

    if (!CLIENT_CAN_FILL_WINDOW (c))
    {
        return;
    }

    screen_info = c->screen_info;
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
                if (segment_overlap (frameExtentY(c), frameExtentY(c) + frameExtentHeight(c), frameExtentY(c2), frameExtentY(c2) + frameExtentHeight(c2)))
                {
                    if ((frameExtentX(c2) + frameExtentWidth(c2)) <= frameExtentX(c))
                    {
                        if (west_neighbour)
                        {
                            /* Check if c2 is closer to the client
                             * then the west neighbour already found
                             */
                            if ((frameExtentX(west_neighbour) + frameExtentWidth(west_neighbour)) < (frameExtentX(c2) + frameExtentWidth(c2)))
                            {
                                west_neighbour = c2;
                            }
                        }
                        else
                        {
                            west_neighbour = c2;
                        }
                    }
                    if ((frameExtentX(c) + frameExtentWidth(c)) <= frameExtentX(c2))
                    {
                        /* Check if c2 is closer to the client
                         * then the west neighbour already found
                         */
                        if (east_neighbour)
                        {
                            if (frameExtentX(c2) < frameExtentX(east_neighbour))
                            {
                                east_neighbour = c2;
                            }
                        }
                        else
                        {
                            east_neighbour = c2;
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
                if (segment_overlap (frameExtentX(c), frameExtentX(c) + frameExtentWidth(c), frameExtentX(c2), frameExtentX(c2) + frameExtentWidth(c2)))
                {
                    if ((frameExtentY(c2) + frameExtentHeight(c2)) <= frameExtentY(c))
                    {
                        if (north_neighbour)
                        {
                            /* Check if c2 is closer to the client
                             * then the north neighbour already found
                             */
                            if ((frameExtentY(north_neighbour) + frameExtentHeight(north_neighbour)) < (frameExtentY(c2) + frameExtentHeight(c2)))
                            {
                                north_neighbour = c2;
                            }
                        }
                        else
                        {
                            north_neighbour = c2;
                        }
                    }
                    if ((frameExtentY(c) + frameExtentHeight(c)) <= frameExtentY(c2))
                    {
                        if (south_neighbour)
                        {
                            /* Check if c2 is closer to the client
                             * then the south neighbour already found
                             */
                            if (frameExtentY(c2) < frameExtentY(south_neighbour))
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
    set_rectangle (&tmp,
                   frameExtentX (c),
                   frameExtentY (c),
                   frameExtentWidth (c),
                   frameExtentHeight (c));

    cx = tmp.x + (tmp.width / 2);
    cy = tmp.y + (tmp.height / 2);

    myScreenFindMonitorAtPoint (screen_info, cx, cy, &rect);
    myScreenMaxSpaceForGeometry (screen_info, &rect, &full);
    geometryMaxSpace(c->screen_info, &full);

    if ((fill_type & CLIENT_FILL) == CLIENT_FILL)
    {
        mask = CWX | CWY | CWHeight | CWWidth;
        /* Adjust size to the largest size available, not covering struts */
        clientMaxSpace (c, &full);
    }
    else if (fill_type & CLIENT_FILL_VERT)
    {
        mask = CWY | CWHeight;
        /* Adjust size to the tallest size available, for the current horizontal position/width */
        clientMaxSpace (c, &tmp);
        full.y = tmp.y;
        full.height = tmp.height;
    }
    else if (fill_type & CLIENT_FILL_HORIZ)
    {
        mask = CWX | CWWidth;
        /* Adjust size to the widest size available, for the current vertical position/height */
        clientMaxSpace (c, &tmp);
        full.x = tmp.x;
        full.width = tmp.width;
    }

    /* If there are neighbours, resize to their borders.
     * If not, resize to the largest size available that you just have computed.
     */

    wc.x = full.x + frameExtentLeft(c);
    if (west_neighbour)
    {
        wc.x += MAX (frameExtentX(west_neighbour) + frameExtentWidth(west_neighbour) - full.x, 0);
    }

    wc.width = full.width - frameExtentRight(c) - (wc.x - full.x);
    if (east_neighbour)
    {
        wc.width -= MAX (full.width - (frameExtentX(east_neighbour) - full.x), 0);
    }

    wc.y = full.y + frameExtentTop(c);
    if (north_neighbour)
    {
        wc.y += MAX (frameExtentY(north_neighbour) + frameExtentHeight(north_neighbour) - full.y, 0);
    }

    wc.height = full.height - frameExtentBottom(c) - (wc.y - full.y);
    if (south_neighbour)
    {
        wc.height -= MAX (full.height - (frameExtentY(south_neighbour) - full.y), 0);
    }

    TRACE ("fill size request: (%d,%d) %dx%d", wc.x, wc.y, wc.width, wc.height);
    clientConfigure(c, &wc, mask, NO_CFG_FLAG);
}
