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
        xfwm4    - (c) 2002-2015 Olivier Fourdan

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
#include "compositor.h"
#include "frame.h"
#include "ui_style.h"
#include "wireframe.h"

#ifndef OUTLINE_WIDTH
#define OUTLINE_WIDTH 5
#endif

#ifndef OUTLINE_WIDTH_CAIRO
#define OUTLINE_WIDTH_CAIRO 2
#endif

static void
wireframeDrawXlib (WireFrame *wireframe, ScreenInfo *screen_info, Display *display)
{
    GdkRectangle geo = wireframe->geometry;

    wireframe->mapped = FALSE;
    XUnmapWindow (display, wireframe->xwindow);
    XMoveResizeWindow (display, wireframe->xwindow, geo.x, geo.y,
                       geo.width, geo.height);

    if ((geo.width > OUTLINE_WIDTH * 2) &&
        (geo.height > OUTLINE_WIDTH * 2))
    {
        XRectangle xrect = { .width = geo.width, .height = geo.height };
        Region inner_xregion;
        Region outer_xregion;

        inner_xregion = XCreateRegion ();
        outer_xregion = XCreateRegion ();

        XUnionRectWithRegion (&xrect, outer_xregion, outer_xregion);

        xrect.x += OUTLINE_WIDTH;
        xrect.y += OUTLINE_WIDTH;
        xrect.width -= OUTLINE_WIDTH * 2;
        xrect.height -= OUTLINE_WIDTH * 2;

        XUnionRectWithRegion (&xrect, inner_xregion, inner_xregion);

        XSubtractRegion (outer_xregion, inner_xregion, outer_xregion);

        XShapeCombineRegion (display, wireframe->xwindow, ShapeBounding,
                             0, 0, outer_xregion, ShapeSet);

        XDestroyRegion (outer_xregion);
        XDestroyRegion (inner_xregion);
        XMapWindow (display, wireframe->xwindow);
        wireframe->mapped = TRUE;

        XDrawRectangle (display, wireframe->xwindow, screen_info->box_gc,
                        0, 0, geo.width - 1, geo.height - 1);

        XDrawRectangle (display, wireframe->xwindow,
                        screen_info->box_gc,
                        OUTLINE_WIDTH - 1, OUTLINE_WIDTH - 1,
                        geo.width - 2 * (OUTLINE_WIDTH - 1) - 1,
                        geo.height - 2 * (OUTLINE_WIDTH - 1) - 1);
    }
    else
    {
        /* Unset the shape */
        XShapeCombineMask (display, wireframe->xwindow,
                           ShapeBounding, 0, 0, None, ShapeSet);
        XMapWindow (display, wireframe->xwindow);
        wireframe->mapped = TRUE;

        XDrawRectangle (display, wireframe->xwindow, screen_info->box_gc,
                        0, 0, geo.width - 1, geo.height - 1);
    }
}

static void
wireframeDrawCairo (WireFrame *wireframe, ScreenInfo *screen_info, Display *display)
{
    GdkRectangle geo = wireframe->geometry;

    XMoveResizeWindow (display, wireframe->xwindow, geo.x, geo.y,
                       geo.width, geo.height);

    if (!wireframe->mapped)
    {
        XMapWindow (display, wireframe->xwindow);
        wireframe->mapped = TRUE;
    }

    if (!wireframe->force_redraw)
    {
        /* Moving only */
        return;
    }

    cairo_xlib_surface_set_size(wireframe->surface, geo.width, geo.height);
    XClearWindow (display, wireframe->xwindow);
    cairo_set_source_rgba (wireframe->cr, wireframe->color.red,
                           wireframe->color.green, wireframe->color.blue,
                           wireframe->color.alpha);
    cairo_paint (wireframe->cr);

    cairo_set_source_rgba (wireframe->cr, wireframe->color.red,
                           wireframe->color.green, wireframe->color.blue, 1.0);
    cairo_rectangle (wireframe->cr,
                     OUTLINE_WIDTH_CAIRO / 2, OUTLINE_WIDTH_CAIRO / 2,
                     geo.width - OUTLINE_WIDTH_CAIRO,
                     geo.height - OUTLINE_WIDTH_CAIRO);
    cairo_stroke (wireframe->cr);
    /* Force a resize of the compositor window to avoid flickering */
    compositorResizeWindow (screen_info->display_info, wireframe->xwindow,
                            geo.x, geo.y, geo.width, geo.height);
}

void
wireframeUpdate (Client *c, WireFrame *wireframe)
{
    ScreenInfo *screen_info;
    Display *display;
    GdkRectangle geo;

    g_return_if_fail (c != NULL);
    g_return_if_fail (wireframe != NULL);
    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    geo = frameExtentGeometry (c);

    /* when moved, we have to force a full redraw */
    wireframe->force_redraw |= ((geo.width != wireframe->geometry.width) ||
                                (geo.height != wireframe->geometry.height));

    wireframe->geometry = geo;

    screen_info = wireframe->screen_info;
    display = myScreenGetXDisplay (screen_info);

    if (compositorIsActive (screen_info))
    {
         wireframeDrawCairo (wireframe, screen_info, display);
    }
    else
    {
         wireframeDrawXlib (wireframe, screen_info, display);
    }
    XFlush (display);
    wireframe->force_redraw = FALSE;
}

static void
wireframeInitColor (WireFrame *wireframe)
{
    GdkRGBA rgba;

    if (getUIStyleColor (myScreenGetGtkWidget (wireframe->screen_info), "bg", "selected", &rgba))
    {
        wireframe->color = rgba;
        wireframe->color.alpha = (compositorIsActive (wireframe->screen_info) ? 0.5 : 1.0);
    }
}

static void wireframeSetHints (WireFrame *wireframe)
{
    XClassHint hint = {
        .res_name = "xfwm4-wireframe",
        .res_class = "Xfwm4",
    };

    XSetClassHint (myScreenGetXDisplay(wireframe->screen_info),
                   wireframe->xwindow, &hint);
}

WireFrame *
wireframeCreate (Client *c)
{
    ScreenInfo *screen_info;
    Display *display;
    WireFrame *wireframe;
    XSetWindowAttributes attrs;
    XVisualInfo xvisual_info;
    Visual *xvisual;
    int depth;
    GdkRectangle geometry;

    g_return_val_if_fail (c != NULL, None);

    TRACE ("client \"%s\" (0x%lx)", c->name, c->window);

    screen_info = c->screen_info;
    display = myScreenGetXDisplay (screen_info);

    wireframe = g_new0 (WireFrame, 1);
    wireframe->screen_info = screen_info;
    wireframe->color.alpha = (compositorIsActive (screen_info) ? 0.5 : 1.0);
    wireframe->force_redraw = TRUE;

    if (compositorIsActive (screen_info) &&
        XMatchVisualInfo (display, screen_info->screen,
                          32, TrueColor, &xvisual_info))
    {
        xvisual = xvisual_info.visual;
        depth = xvisual_info.depth;
        wireframe->xcolormap = XCreateColormap (display,
                                                screen_info->xroot,
                                                xvisual, AllocNone);
    }
    else
    {
        xvisual = screen_info->visual;
        depth = screen_info->depth;
        wireframe->xcolormap = screen_info->cmap;
    }

    geometry = frameExtentGeometry (c);

    attrs.override_redirect = True;
    attrs.colormap = wireframe->xcolormap;
    attrs.background_pixel = BlackPixel (display, screen_info->screen);
    attrs.border_pixel = BlackPixel (display, screen_info->screen);
    wireframe->xwindow = XCreateWindow (display, screen_info->xroot,
                                        geometry.x, geometry.y,
                                        geometry.width, geometry.height,
                                        0, depth, InputOutput, xvisual,
                                        CWOverrideRedirect | CWColormap | CWBackPixel | CWBorderPixel,
                                        &attrs);

    if (compositorIsActive (screen_info))
    {
        /* Cairo */
        wireframeInitColor (wireframe);
        wireframe->surface = cairo_xlib_surface_create (display,
                                                        wireframe->xwindow, xvisual,
                                                        geometry.width, geometry.height);
        wireframe->cr = cairo_create (wireframe->surface);
        cairo_set_line_width (wireframe->cr, OUTLINE_WIDTH_CAIRO);
        cairo_set_line_join (wireframe->cr, CAIRO_LINE_JOIN_MITER);
    }

    wireframeSetHints (wireframe);
    wireframeUpdate (c, wireframe);

    return (wireframe);
}

void
wireframeDelete (WireFrame *wireframe)
{
    ScreenInfo *screen_info;
    Display *display;

    g_return_if_fail (wireframe != None);
    TRACE ("entering");

    screen_info = wireframe->screen_info;
    display = myScreenGetXDisplay (screen_info);

    XUnmapWindow (display, wireframe->xwindow);
    if (wireframe->cr)
    {
        cairo_destroy (wireframe->cr);
    }
    if (wireframe->surface)
    {
        cairo_surface_destroy (wireframe->surface);
    }
    if (wireframe->xcolormap != screen_info->cmap)
    {
        XFreeColormap (display, wireframe->xcolormap);
    }
    XDestroyWindow (display, wireframe->xwindow);
    g_free (wireframe);
}
