/*
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
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <libxfce4util/libxfce4util.h> 
#include <stdlib.h>
#include <stdio.h>

#include "mypixmap.h"

static gboolean
myPixmapCompose (MyPixmap * pm, gchar * dir, gchar * file)
{
    gchar *filepng;
    gchar *filename;
    GdkPixbuf *alpha;
    GdkPixbuf *src;
    GdkPixmap *destw;
    GdkColormap *cmap;
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
    destw = gdk_xid_table_lookup (pm->pixmap);
    if (destw)
    {
        g_object_ref (G_OBJECT (destw));
    }
    else
    {
         destw = gdk_pixmap_foreign_new (pm->pixmap);
    }
    
    if (!destw)
    {
        DBG ("Cannot get pixmap");
        g_object_unref (alpha);
        return FALSE;
    }

    cmap = gdk_drawable_get_colormap (destw);
    if (cmap)
    {
        g_object_ref (G_OBJECT (cmap));
    }
    if (cmap == NULL)
    {
        if (gdk_drawable_get_depth (destw) == 1)
        {
            cmap = NULL;
        }
        else
        {
            cmap = gdk_screen_get_rgb_colormap (pm->md->gscr);
            g_object_ref (G_OBJECT (cmap));
        }
    }

    if (cmap && (gdk_colormap_get_visual (cmap)->depth != gdk_drawable_get_depth (destw)))
    {
        g_object_unref (G_OBJECT (cmap));
        cmap = NULL;
    }

    src = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE (destw), cmap, 
                                        0, 0, 0, 0, pm->width, pm->height);
    gdk_pixbuf_composite (alpha, src, 0, 0, pm->width, pm->height,
                          0, 0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
    gdk_draw_pixbuf (GDK_DRAWABLE (destw), NULL, src, 0, 0, 0, 0,
                     pm->width, pm->height, GDK_RGB_DITHER_NONE, 0, 0);                 

    if (cmap)
    {
        g_object_unref (G_OBJECT (cmap));
    }
    g_object_unref (alpha);
    g_object_unref (src);
    g_object_unref (destw);

    return TRUE;
}

gboolean
myPixmapLoad (ScreenData * md, MyPixmap * pm, gchar * dir, gchar * file,
    XpmColorSymbol * cs, gint n)
{
    gchar *filename;
    gchar *filexpm;
    XpmAttributes attr;

    TRACE ("entering myPixmapLoad");

    g_return_val_if_fail (dir != NULL, FALSE);
    g_return_val_if_fail (file != NULL, FALSE);

    pm->md = md;
    pm->pixmap = None;
    pm->mask = None;
    pm->width = 1;
    pm->height = 1;
    filexpm = g_strdup_printf ("%s.%s", file, "xpm");
    filename = g_build_filename (dir, filexpm, NULL);
    g_free (filexpm);
    attr.colorsymbols = cs;
    attr.numsymbols = n;
    attr.colormap = pm->md->cmap;
    attr.closeness = 65535;
    attr.valuemask = XpmCloseness | XpmColormap | XpmSize;
    if (n > 0 && cs)
    {
        attr.valuemask = attr.valuemask | XpmColorSymbols;
    }
    if (XpmReadFileToPixmap (md->dpy, md->xroot, filename, &pm->pixmap, &pm->mask, &attr))
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
myPixmapCreate (ScreenData * md, MyPixmap * pm, gint width, gint height)
{
    TRACE ("entering myPixmapCreate, width=%i, height=%i", width, height);
    if ((width < 1) || (height < 1) || (!md))
    {
        myPixmapInit (pm);
    }
    else
    {
        pm->md = md;
        pm->pixmap = XCreatePixmap (md->dpy, md->xroot, width, height, md->depth);
        pm->mask = XCreatePixmap (md->dpy, pm->pixmap, width, height, 1);
        pm->width = width;
        pm->height = height;
    }
}

void
myPixmapInit (MyPixmap * pm)
{
    pm->md = NULL;
    pm->pixmap = None;
    pm->mask = None;
    pm->width = 0;
    pm->height = 0;
}

void
myPixmapFree (MyPixmap * pm)
{
    TRACE ("entering myPixmapFree");

    if (pm->pixmap != None)
    {
        XFreePixmap (pm->md->dpy, pm->pixmap);
        pm->pixmap = None;
    }
    if (pm->mask != None)
    {
        XFreePixmap (pm->md->dpy, pm->mask);
        pm->mask = None;
    }
}
