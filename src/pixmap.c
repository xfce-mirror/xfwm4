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
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include "pixmap.h"
#include "debug.h"

gboolean loadPixmap(Display * dpy, MyPixmap * pm, gchar *dir, gchar *file,
               XpmColorSymbol * cs, gint n)
{
    gchar filename[512];
    XpmAttributes attr;

    DBG("entering loadPixmap\n");

    pm->pixmap = None;
    pm->mask = None;
    pm->width = 1;
    pm->height = 1;
    g_snprintf(filename, sizeof(filename), "%s/%s", dir, file);
    attr.colorsymbols = cs;
    attr.numsymbols = n;
    attr.valuemask = XpmSize;
    if(n > 0 && cs)
        attr.valuemask = attr.valuemask | XpmColorSymbols;
    if(XpmReadFileToPixmap
       (dpy, XDefaultRootWindow(dpy), filename, &pm->pixmap, &pm->mask, &attr))
        return FALSE;
    pm->width = attr.width;
    pm->height = attr.height;
    XpmFreeAttributes(&attr);
    return TRUE;
}

void freePixmap(Display * dpy, MyPixmap * pm)
{
    DBG("entering freePixmap\n");

    if(pm->pixmap != None)
        XFreePixmap(dpy, pm->pixmap);
    if(pm->mask != None)
        XFreePixmap(dpy, pm->mask);
}

void scalePixmap(Display * dpy, MyPixmap * src, MyPixmap * dst, gint width, gint height)
{
    XpmImage xi_src, xi_dst;
    gint x, y, sx, sy, *src_data, *dst_data;

    DBG("entering scalePixmap\n");

    if(width < 1)
        width = 1;
    if(height < 1)
        height = 1;

    XpmCreateXpmImageFromPixmap(dpy, src->pixmap, src->mask, &xi_src, NULL);
    dst->width = width;
    dst->height = height;
    xi_dst.width = width;
    xi_dst.height = height;
    xi_dst.cpp = xi_src.cpp;
    xi_dst.ncolors = xi_src.ncolors;
    xi_dst.colorTable = xi_src.colorTable;
    xi_dst.data = malloc(sizeof(int) * (xi_dst.width * xi_dst.height));
    dst_data = xi_dst.data;
    src_data = xi_src.data;
    for(y = 0; y < xi_dst.height; y++)
    {
        dst_data = xi_dst.data + (y * xi_dst.width);
        for(x = 0; x < xi_dst.width; x++)
        {
            sx = (x * xi_src.width) / xi_dst.width;
            sy = (y * xi_src.height) / xi_dst.height;
            *dst_data = *(src_data + sx + (sy * xi_src.width));
            dst_data++;
        }
    }
    XpmCreatePixmapFromXpmImage(dpy, DefaultRootWindow(dpy), &xi_dst,
                                &dst->pixmap, &dst->mask, NULL);
    free(xi_dst.data);
    XpmFreeXpmImage(&xi_src);
}
