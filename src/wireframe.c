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


        metacity - (c) 2001 Anders Carlsson, Havoc Pennington
        xfwm4    - (c) 2002-2011 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "screen.h"
#include "client.h"
#include "frame.h"
#include "wireframe.h"

#ifndef OUTLINE_WIDTH
#define OUTLINE_WIDTH 5
#endif

void
wireframeUpdate (Client *c, Window xwindow)
{
    ScreenInfo *screen_info;

    g_return_if_fail (c != NULL);
    g_return_if_fail (xwindow != None);

    TRACE ("entering wireframeUpdate 0x%lx", xwindow);
    screen_info = c->screen_info;
    XUnmapWindow (clientGetXDisplay (c), xwindow);
    XMoveResizeWindow (clientGetXDisplay (c), xwindow,
                       frameX (c), frameY (c), frameWidth (c), frameHeight (c));

    if ((frameWidth (c) > OUTLINE_WIDTH * 2) && (frameHeight (c) > OUTLINE_WIDTH * 2))
    {
        XRectangle xrect;
        Region inner_xregion;
        Region outer_xregion;

        inner_xregion = XCreateRegion ();
        outer_xregion = XCreateRegion ();

        xrect.x = 0;
        xrect.y = 0;
        xrect.width = frameWidth (c);
        xrect.height = frameHeight (c);
        XUnionRectWithRegion (&xrect, outer_xregion, outer_xregion);

        xrect.x += OUTLINE_WIDTH;
        xrect.y += OUTLINE_WIDTH;
        xrect.width -= OUTLINE_WIDTH * 2;
        xrect.height -= OUTLINE_WIDTH * 2;

        XUnionRectWithRegion (&xrect, inner_xregion, inner_xregion);

        XSubtractRegion (outer_xregion, inner_xregion, outer_xregion);

        XShapeCombineRegion (clientGetXDisplay (c), xwindow, ShapeBounding,
                             0, 0, outer_xregion, ShapeSet);

        XDestroyRegion (outer_xregion);
        XDestroyRegion (inner_xregion);
        XMapWindow (clientGetXDisplay (c), xwindow);

        XDrawRectangle (clientGetXDisplay (c), xwindow,
                        gdk_x11_gc_get_xgc (screen_info->white_gc),
                        0, 0, frameWidth (c) - 1, frameHeight (c) - 1);

        XDrawRectangle (clientGetXDisplay (c), xwindow,
                        gdk_x11_gc_get_xgc (screen_info->white_gc),
                        OUTLINE_WIDTH - 1, OUTLINE_WIDTH - 1,
                        frameWidth (c) - 2 * (OUTLINE_WIDTH - 1) - 1,
                        frameHeight (c)- 2 * (OUTLINE_WIDTH - 1) - 1);
    }
    else
    {
        /* Unset the shape */
        XShapeCombineMask (clientGetXDisplay (c), xwindow,
                           ShapeBounding, 0, 0, None, ShapeSet);
        XMapWindow (clientGetXDisplay (c), xwindow);

        XDrawRectangle (clientGetXDisplay (c), xwindow,
                        gdk_x11_gc_get_xgc (screen_info->white_gc),
                        0, 0, frameWidth (c) - 1, frameHeight (c) - 1);
    }
    XFlush (clientGetXDisplay (c));
}

Window
wireframeCreate (Client *c)
{
    ScreenInfo *screen_info;
    XSetWindowAttributes attrs;
    Window xwindow;

    g_return_val_if_fail (c != NULL, None);

    TRACE ("entering wireframeCreate");

    screen_info = c->screen_info;
    attrs.override_redirect = True;
    attrs.background_pixel = BlackPixel (clientGetXDisplay (c),
                                         screen_info->screen);
    xwindow = XCreateWindow (clientGetXDisplay (c),
                             screen_info->xroot,
                             frameX (c), frameY (c),
                             frameWidth (c), frameHeight (c),
                             0, CopyFromParent, CopyFromParent,
                             (Visual *) CopyFromParent,
                             CWOverrideRedirect | CWBackPixel, &attrs);
    wireframeUpdate (c, xwindow);

    return (xwindow);
}

void
wireframeDelete (ScreenInfo *screen_info, Window xwindow)
{
    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (xwindow != None);

    TRACE ("entering wireframeDelete 0x%lx", xwindow);
    XUnmapWindow (myScreenGetXDisplay (screen_info), xwindow);
    XDestroyWindow (myScreenGetXDisplay (screen_info), xwindow);
}
