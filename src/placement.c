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
 
	xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>
#include <libxfce4util/debug.h>

#include "main.h"
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
    
    if (FLAG_TEST_ALL (c->flags, CLIENT_FLAG_HAS_STRUT | CLIENT_FLAG_VISIBLE))
    {
	sigma = overlap (x, y, x + w, y + h, 
		         0, c->struts[LEFT_START_Y], 
			 c->struts[LEFT], 
			 c->struts[LEFT_END_Y])
	      + overlap (x, y, x + w, y + h,
		         MyDisplayFullWidth (dpy, screen) - c->struts[RIGHT], 
			 c->struts[RIGHT_START_Y],
		         MyDisplayFullWidth (dpy, screen), c->struts[RIGHT_END_Y])
	      + overlap (x, y, x + w, y + h,
		         c->struts[TOP_START_X], 0, 
		         c->struts[TOP_END_X], 
			 c->struts[TOP])
	      + overlap (x, y, x + w, y + h,
		         c->struts[BOTTOM_START_X], 
			 MyDisplayFullHeight (dpy, screen) - c->struts[BOTTOM],
		         c->struts[BOTTOM_END_X], 
			 MyDisplayFullHeight (dpy, screen));
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

void 
clientMaxSpace (int *x, int *y, int *w, int *h)
{
    Client *c2;
    int i, delta = 0;
    
    g_return_if_fail (x != NULL);
    g_return_if_fail (y != NULL);
    g_return_if_fail (w != NULL);
    g_return_if_fail (h != NULL);

    for (c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
    {
	if (FLAG_TEST_ALL (c2->flags, CLIENT_FLAG_HAS_STRUT | CLIENT_FLAG_VISIBLE))
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
			 MyDisplayFullWidth (dpy, screen) - c2->struts[RIGHT], c2->struts[RIGHT_START_Y],
			 MyDisplayFullWidth (dpy, screen), c2->struts[RIGHT_END_Y]))
	    {
		delta = (*x + *w) - MyDisplayFullWidth (dpy, screen) + c2->struts[RIGHT];
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
			 c2->struts[BOTTOM_START_X], MyDisplayFullHeight (dpy, screen) - c2->struts[BOTTOM],
			 c2->struts[BOTTOM_END_X], MyDisplayFullHeight (dpy, screen)))
	    {
		delta = (*y + *h) - MyDisplayFullHeight (dpy, screen) + c2->struts[BOTTOM];
		*h = *h - delta;
	    }
	}
    }
}

gboolean
clientCkeckTitle (Client * c)
{
    Client *c2;
    int i, frame_x, frame_y, frame_width, frame_top;

    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_width = frameWidth (c);
    frame_top = frameTop (c);

    /* Struts and other partial struts */
    for (c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
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
 */
void
clientConstrainPos (Client * c, gboolean show_full)
{
    Client *c2;
    int i, cx, cy, disp_x, disp_y, disp_max_x, disp_max_y;
    int frame_x, frame_y, frame_height, frame_width, frame_top, frame_left;

    g_return_if_fail (c != NULL);
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

    cx = frame_x + (frame_width / 2);
    cy = frame_y + (frame_height / 2);

    disp_x = MyDisplayX (cx, cy);
    disp_y = MyDisplayY (cx, cy);
    disp_max_x = MyDisplayMaxX (dpy, screen, cx, cy);
    disp_max_y = MyDisplayMaxY (dpy, screen, cx, cy);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_FULLSCREEN))
    {
	TRACE ("ignoring constrained for client \"%s\" (0x%lx)", c->name,
	    c->window);
	return;
    }
    if (show_full)
    {
	if (frame_x + frame_width > disp_max_x)
	{
	    c->x = disp_max_x - frame_width + frame_left;
	}
	if (frame_x < disp_x)
	{
	    c->x = disp_x + frame_left;
	}
	if (frame_y + frame_height > disp_max_y)
	{
	    c->y = disp_max_y - frame_height + frame_top;
	}
	if (frame_y < disp_y)
	{
	    c->y = disp_y + frame_top;
	}
    }
    else
    {
	if (frame_x + CLIENT_MIN_VISIBLE > disp_max_x)
	{
	    c->x = disp_max_x - CLIENT_MIN_VISIBLE + frame_left;
	    frame_x = frameX (c);
	}
	if (frame_x + frame_width < disp_x + CLIENT_MIN_VISIBLE)
	{
	    c->x = disp_x + CLIENT_MIN_VISIBLE - frame_width + frame_left;
	    frame_x = frameX (c);
	}
	if (frame_y + CLIENT_MIN_VISIBLE > disp_max_y)
	{
	    c->y = disp_max_y - CLIENT_MIN_VISIBLE + frame_top;
	    frame_y = frameY (c);
	}
	if ((frame_y < disp_y) && (frame_y > disp_y - frame_top))
	{
	    c->y = disp_y + frame_top;
	    frame_y = frameY (c);
	}
	if (frame_y + frame_height < disp_y + CLIENT_MIN_VISIBLE)
	{
	    c->y = disp_y + CLIENT_MIN_VISIBLE  - frame_height + frame_top;
	    frame_y = frameY (c);
	}
    }

    /* Struts and other partial struts */
    for (c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
    {
	if (FLAG_TEST_ALL (c2->flags, CLIENT_FLAG_HAS_STRUT | CLIENT_FLAG_VISIBLE) && (c2 != c))
	{
	    /* Left */
	    if (overlapY (frame_y, frame_y + frame_height, c2->struts[LEFT_START_Y], c2->struts[LEFT_END_Y]))
	    {
		if (frame_x + frame_width < c2->struts[LEFT] + CLIENT_MIN_VISIBLE)
		{
		    c->x = c2->struts[LEFT] + CLIENT_MIN_VISIBLE - frame_width + frame_left;
		    frame_x = frameX (c);
		}
	    }

	    /* Right */
	    if (overlapY (frame_y, frame_y + frame_height, c2->struts[RIGHT_START_Y], c2->struts[RIGHT_END_Y]))
	    {
		if (frame_x > MyDisplayFullWidth (dpy, screen) - c2->struts[RIGHT] - CLIENT_MIN_VISIBLE)
		{
		    c->x = MyDisplayFullWidth (dpy, screen) - c2->struts[RIGHT] - CLIENT_MIN_VISIBLE + frame_left;
		    frame_x = frameX (c);
		}
	    }

	    /* Top */
	    if (overlapX (frame_x, frame_x + frame_width, c2->struts[TOP_START_X], c2->struts[TOP_END_X]))
	    {
		if (overlapY (frame_y, frame_y + frame_top, 0, c2->struts[TOP]))
		{
		    c->y = c2->struts[TOP] + frame_top;
		    frame_y = frameY (c);
		}
	    }

	    /* Bottom */
	    if (overlapX (frame_x, frame_x + frame_width, c2->struts[BOTTOM_START_X], c2->struts[BOTTOM_END_X]))
	    {
		if (frame_y > MyDisplayFullHeight (dpy, screen) - c2->struts[BOTTOM] - CLIENT_MIN_VISIBLE)
		{
		    c->y = MyDisplayFullHeight (dpy, screen) - c2->struts[BOTTOM] - CLIENT_MIN_VISIBLE + frame_top;
		    frame_y = frameY (c);
		}
	    }
	}
    }
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

    diff_x = abs (c->size->x - ((MyDisplayFullWidth (dpy, screen) - c->width) / 2));
    diff_y = abs (c->size->y - ((MyDisplayFullHeight (dpy, screen) - c->height) / 2));

    if (((use_xinerama) && (diff_x < 25) && (diff_y < 25)) ||
	((frameX (c) == 0) && (frameY (c) == 0) && (c->type & (WINDOW_TYPE_DIALOG)) && !clientIsTransient (c)))
    {
	/* We consider that the windows is centered on screen,
	 * Thus, will move it so its center on the current
	 * physical screen
	 */
	getMouseXY (root, &cx, &cy);
	
	c->x =
	    MyDisplayX (cx, cy) + (MyDisplayWidth (dpy, screen, cx,
		cy) - c->width) / 2;
	c->y =
	    MyDisplayY (cx, cy) + (MyDisplayHeight (dpy, screen, cx,
		cy) - c->height) / 2;
    }
    clientConstrainPos (c, TRUE);
}

void
clientInitPosition (Client * c)
{
    int test_x = 0, test_y = 0;
    Client *c2;
    int xmax, ymax, best_x, best_y, i, msx, msy;
    int left, right, top, bottom;
    int frame_x, frame_y, frame_height, frame_width, frame_left, frame_top;
    gdouble best_overlaps = 0.0;
    gboolean first = TRUE;

    g_return_if_fail (c != NULL);
    TRACE ("entering clientInitPosition");

    clientGravitate (c, APPLY);

    if ((c->size->flags & (PPosition | USPosition)) ||
	(c->type & (WINDOW_TYPE_DONT_PLACE)) ||
	((c->type & (WINDOW_TYPE_DIALOG)) && !clientIsTransient (c)))
    {
	if (CONSTRAINED_WINDOW (c))
	{
	    clientKeepVisible (c);
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
	}
	return;
    }

    getMouseXY (root, &msx, &msy);
    left   = (isLeftMostHead (dpy, screen, msx, msy) ? params.xfwm_margins[LEFT] : 0);
    right  = (isRightMostHead (dpy, screen, msx, msy) ? params.xfwm_margins[RIGHT] : 0);
    top    = (isTopMostHead (dpy, screen, msx, msy) ? params.xfwm_margins[TOP] : 0);
    bottom = (isBottomMostHead (dpy, screen, msx, msy) ? params.xfwm_margins[BOTTOM] : 0);

    frame_x = frameX (c);
    frame_y = frameY (c);
    frame_height = frameHeight (c);
    frame_width = frameWidth (c);
    frame_left = frameLeft(c);
    frame_top = frameTop (c);

    xmax = MyDisplayMaxX (dpy, screen, msx, msy) - c->width - frameRight (c) - right;
    ymax = MyDisplayMaxY (dpy, screen, msx, msy) - c->height - frameBottom (c) - bottom;
    best_x = MyDisplayX (msx, msy) + frameLeft (c);
    best_y = MyDisplayY (msx, msy) + frameTop (c);

    for (test_y = MyDisplayY (msx, msy) + frameTop (c); test_y <= ymax;
	test_y += 8)
    {
	for (test_x = MyDisplayX (msx, msy) + frameLeft (c) + left;
	    test_x <= xmax; test_x += 8)
	{
	    gdouble count_overlaps = 0.0;
	    TRACE ("analyzing %i clients", client_count);
	    for (c2 = clients, i = 0; i < client_count; c2 = c2->next, i++)
	    {
		if ((c2 != c) && (c2->type != WINDOW_DESKTOP)
		    && (c->win_workspace == c2->win_workspace)
		    && FLAG_TEST (c2->flags, CLIENT_FLAG_VISIBLE))
		{
		    count_overlaps += 
		             (gdouble) overlap (test_x - frame_left, 
						test_y - frame_top, 
						test_x - frame_left + frame_width,
						test_y - frame_top + frame_height, 
						frameX (c2), 
						frameY (c2),
						frameX (c2) + frameWidth (c2),
						frameY (c2) + frameHeight (c2))
			  +  (gdouble) clientStrutAreaOverlap (test_x - frame_left,
			                                       test_y - frame_top,
							       test_x - frame_left + frame_width,
							       test_y, c2) * 1000.0
			  +  (gdouble) clientStrutAreaOverlap (test_x - frame_left,
			                                       test_y - frame_top,
							       test_x - frame_left + frame_width,
							       test_y - frame_top + frame_height, c2) * 100.0;
		}
	    }
	    if (count_overlaps == 0.0)
	    {
		TRACE ("overlaps is 0 so it's the best we can get");
		c->x = test_x;
		c->y = test_y;
		return;
	    }
	    else if ((count_overlaps < best_overlaps) || (first))
	    {
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
    return;
}

