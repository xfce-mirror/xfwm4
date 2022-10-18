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

#ifndef INC_WIREFRAME_H
#define INC_WIREFRAME_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <gdk/gdk.h>

typedef struct _ScreenInfo ScreenInfo;
typedef struct _Client Client;

typedef struct _WireFrame WireFrame;
struct _WireFrame
{
    ScreenInfo *screen_info;
    Window xwindow;
    gboolean mapped;
    gboolean force_redraw;
    GdkRectangle geometry;
    Colormap xcolormap;
    cairo_surface_t *surface;
    cairo_t *cr;
    GdkRGBA border_color;
    GdkRGBA background_color;
};

void                     clientWireframeUpdate                  (Client *,
                                                                 WireFrame *);
WireFrame *              clientWireframeCreate                  (Client *);
WireFrame *              wireframeCreate                        (ScreenInfo *,
                                                                 GdkRectangle,
                                                                 GdkRGBA,
                                                                 GdkRGBA);
void                     wireframeDelete                        (WireFrame *);
void                     wireframeRedraw                        (WireFrame *);
void                     wireframeSetGeometry                   (WireFrame *,
                                                                 GdkRectangle);

#endif /* INC_WIREFRAME_H */
