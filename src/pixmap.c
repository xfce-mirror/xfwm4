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
#include <glib.h>
#include "pixmap.h"
#include "main.h"
#include "debug.h"

gboolean loadPixmap(Display * dpy, MyPixmap * pm, gchar *dir, gchar *file, XpmColorSymbol * cs, gint n)
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
    {
        attr.valuemask = attr.valuemask | XpmColorSymbols;
    }
    if(XpmReadFileToPixmap (dpy, XDefaultRootWindow(dpy), filename, &pm->pixmap, &pm->mask, &attr))
    {
        return FALSE;
    }
    pm->width = attr.width;
    pm->height = attr.height;
    XpmFreeAttributes(&attr);
    return TRUE;
}

void createPixmap(Display * dpy, MyPixmap * pm, gint width, gint height)
{
    pm->pixmap = XCreatePixmap(dpy, root, width, height, depth);
    pm->mask   = XCreatePixmap(dpy, pm->pixmap, width, height, 1);
    pm->width  = width;
    pm->height = height;
}

void freePixmap(Display * dpy, MyPixmap * pm)
{
    DBG("entering freePixmap\n");

    if(pm->pixmap != None)
    {
        XFreePixmap(dpy, pm->pixmap);
    }
    if(pm->mask != None)
    {
        XFreePixmap(dpy, pm->mask);
    }
}
