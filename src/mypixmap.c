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
        xfwm4    - (c) 2002-2003 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <libxfce4util/debug.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <gdk/gdkx.h>

#include "mypixmap.h"
#include "main.h"

static gboolean
myPixmapCompose (MyPixmap * pm, gchar * dir, gchar * file)
{
    gchar *filepng;
    gchar *filename;
    GdkPixbuf *alpha;
    GdkPixbuf *src;
    GdkPixmap *destw;
    GError *error = NULL;

    filepng = g_strdup_printf ("%s.%s", file, "png");
    filename = g_build_filename (dir, filepng, NULL);
    g_free (filepng);
    
    if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    {
        g_free (filename);
        return FALSE;
    }
    alpha = gdk_pixbuf_new_from_file (filename, &error);
    g_free (filename);
    if (error)
    {
	g_warning ("%s", error->message);
	g_error_free (error);
	return FALSE;
    }
    if (!gdk_pixbuf_get_has_alpha (alpha))
    {
	g_object_unref (alpha);
        return FALSE;
    }
    destw = gdk_pixmap_foreign_new (pm->pixmap);
    if (!destw)
    {
	DBG ("Cannot get pixmap");
	g_object_unref (alpha);
        return FALSE;
    }
    
    src = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE (destw), gdk_rgb_get_cmap (), 
                                        0, 0, 0, 0, pm->width, pm->height);
    gdk_pixbuf_composite (alpha, src, 0, 0, pm->width, pm->height,
                          0, 0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
    gdk_draw_pixbuf (GDK_DRAWABLE (destw), NULL, src, 0, 0, 0, 0,
                     pm->width, pm->height, GDK_RGB_DITHER_NONE, 0, 0);                 
    g_object_unref (alpha);
    g_object_unref (src);
    g_object_unref (destw);

    return TRUE;
}

gboolean
myPixmapLoad (Display * dpy, MyPixmap * pm, gchar * dir, gchar * file,
    XpmColorSymbol * cs, gint n)
{
    gchar *filename;
    gchar *filexpm;
    XpmAttributes attr;

    TRACE ("entering myPixmapLoad");

    g_return_val_if_fail (dir != NULL, FALSE);
    g_return_val_if_fail (file != NULL, FALSE);

    pm->pixmap = None;
    pm->mask = None;
    pm->width = 1;
    pm->height = 1;
    filexpm = g_strdup_printf ("%s.%s", file, "xpm");
    filename = g_build_filename (dir, filexpm, NULL);
    g_free (filexpm);
    attr.colorsymbols = cs;
    attr.numsymbols = n;
    attr.colormap = cmap;
    attr.closeness = 65535;
    attr.valuemask = XpmCloseness | XpmColormap | XpmSize;
    if (n > 0 && cs)
    {
        attr.valuemask = attr.valuemask | XpmColorSymbols;
    }
    if (XpmReadFileToPixmap (dpy, XDefaultRootWindow (dpy), filename,
            &pm->pixmap, &pm->mask, &attr))
    {
        TRACE ("%s not found", filename);
        g_free (filename);
        return FALSE;
    }
    pm->width = attr.width;
    pm->height = attr.height;
    XpmFreeAttributes (&attr);
    g_free (filename);

    /* Apply the alpha channel if available */
    myPixmapCompose (pm, dir, file);
    
    return TRUE;
}

void
myPixmapCreate (Display * dpy, MyPixmap * pm, gint width, gint height)
{
    TRACE ("entering myPixmapCreate, width=%i, height=%i", width, height);
    if ((width < 1) || (height < 1))
    {
        myPixmapInit (pm);
    }
    else
    {
        pm->pixmap = XCreatePixmap (dpy, root, width, height, depth);
        pm->mask = XCreatePixmap (dpy, pm->pixmap, width, height, 1);
        pm->width = width;
        pm->height = height;
    }
}

void
myPixmapInit (MyPixmap * pm)
{
    pm->pixmap = None;
    pm->mask = None;
    pm->width = 0;
    pm->height = 0;
}

void
myPixmapFree (Display * dpy, MyPixmap * pm)
{
    TRACE ("entering myPixmapFree");

    if (pm->pixmap != None)
    {
        XFreePixmap (dpy, pm->pixmap);
        pm->pixmap = None;
    }
    if (pm->mask != None)
    {
        XFreePixmap (dpy, pm->mask);
        pm->mask = None;
    }
}
