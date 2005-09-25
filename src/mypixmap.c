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
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2005 Olivier Fourdan
 
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

static void
xfwmPixmapRefreshPict (xfwmPixmap * pm)
{
#ifdef HAVE_RENDER
    ScreenInfo * screen_info = pm->screen_info;

    if (!pm->pict_format)
    {
        pm->pict_format = XRenderFindVisualFormat (myScreenGetXDisplay (screen_info), 
                                                   screen_info->visual);
    }

    if (pm->pict != None)
    {
        XRenderFreePicture (myScreenGetXDisplay(pm->screen_info), pm->pict);
        pm->pict == None;
    }

    if ((pm->pixmap) && (pm->pict_format))
    {
        pm->pict = XRenderCreatePicture (myScreenGetXDisplay (screen_info), 
                                         pm->pixmap, pm->pict_format, 0, NULL);
    }
#endif
}

static gboolean
xfwmPixmapCompose (xfwmPixmap * pm, gchar * dir, gchar * file)
{
    gchar *filepng;
    gchar *filename;
    GdkPixbuf *alpha;
    GdkPixbuf *src;
    GdkPixmap *destw;
    GdkVisual *gvisual;
    GdkColormap *cmap;
    GError *error = NULL;
    gint width, height;

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
        g_warning ("Cannot get pixmap");
        g_object_unref (alpha);
        return FALSE;
    }

    gvisual = gdk_screen_get_system_visual (pm->screen_info->gscr);
    cmap = gdk_x11_colormap_foreign_new (gvisual, pm->screen_info->cmap);    

    if (!cmap)
    {
        g_warning ("Cannot create colormap");
        g_object_unref (alpha);
        g_object_unref (destw);
        return FALSE;
    }

    width = MIN (gdk_pixbuf_get_width (alpha), pm->width);
    height = MIN (gdk_pixbuf_get_height (alpha), pm->height);
    
    src = gdk_pixbuf_get_from_drawable(NULL, GDK_DRAWABLE (destw), cmap, 
                                        0, 0, 0, 0, pm->width, pm->height);
    gdk_pixbuf_composite (alpha, src, 0, 0, width, height,
                          0, 0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
    gdk_draw_pixbuf (GDK_DRAWABLE (destw), NULL, src, 0, 0, 0, 0,
                     pm->width, pm->height, GDK_RGB_DITHER_NONE, 0, 0);

    g_object_unref (cmap);
    g_object_unref (alpha);
    g_object_unref (src);
    g_object_unref (destw);

    return TRUE;
}

gboolean
xfwmPixmapLoad (ScreenInfo * screen_info, xfwmPixmap * pm, gchar * dir, gchar * file, XpmColorSymbol * cs, gint n)
{
    gchar *filename;
    gchar *filexpm;
    XpmAttributes attr;

    TRACE ("entering xfwmPixmapLoad");

    g_return_val_if_fail (dir != NULL, FALSE);
    g_return_val_if_fail (file != NULL, FALSE);

    pm->screen_info = screen_info;
    pm->pixmap = None;
    pm->mask = None;
    pm->width = 1;
    pm->height = 1;
    filexpm = g_strdup_printf ("%s.%s", file, "xpm");
    filename = g_build_filename (dir, filexpm, NULL);
    g_free (filexpm);
    attr.colorsymbols = cs;
    attr.numsymbols = n;
    attr.depth = screen_info->depth;
    attr.colormap = pm->screen_info->cmap;
    attr.closeness = 65535;
    attr.valuemask = XpmCloseness | XpmColormap | XpmSize | XpmDepth;
    if ((n > 0) && (cs != NULL))
    {
        attr.valuemask = attr.valuemask | XpmColorSymbols;
    }
    if (XpmReadFileToPixmap (myScreenGetXDisplay (screen_info), 
                             screen_info->xroot, filename, 
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
    xfwmPixmapCompose (pm, dir, file);
    
#ifdef HAVE_RENDER
    xfwmPixmapRefreshPict (pm);
#endif

    return TRUE;
}

void
xfwmPixmapCreate (ScreenInfo * screen_info, xfwmPixmap * pm, 
                  gint width, gint height)
{
    TRACE ("entering xfwmPixmapCreate, width=%i, height=%i", width, height);
    g_return_if_fail (screen_info != NULL);

    if ((width < 1) || (height < 1))
    {
        xfwmPixmapInit (screen_info, pm);
    }
    else
    {
        pm->screen_info = screen_info;
        pm->pixmap = XCreatePixmap (myScreenGetXDisplay (screen_info), 
                                    screen_info->xroot, 
                                    width, height, screen_info->depth);
        pm->mask = XCreatePixmap (myScreenGetXDisplay (screen_info), 
                                  pm->pixmap, width, height, 1);
        pm->width = width;
        pm->height = height;
    }
#ifdef HAVE_RENDER
    xfwmPixmapRefreshPict (pm);
#endif
}

void
xfwmPixmapInit (ScreenInfo * screen_info, xfwmPixmap * pm)
{
    pm->screen_info = screen_info;
    pm->pixmap = None;
    pm->mask = None;
    pm->width = 0;
    pm->height = 0;
#ifdef HAVE_RENDER
    pm->pict_format = XRenderFindVisualFormat (myScreenGetXDisplay (screen_info), 
                                               screen_info->visual);
    pm->pict = None;
#endif
}

void
xfwmPixmapFree (xfwmPixmap * pm)
{
    
    TRACE ("entering xfwmPixmapFree");
    
    if (pm->pixmap != None)
    {
        XFreePixmap (myScreenGetXDisplay(pm->screen_info), pm->pixmap);
        pm->pixmap = None;
    }
    if (pm->mask != None)
    {
        XFreePixmap (myScreenGetXDisplay(pm->screen_info), pm->mask);
        pm->mask = None;
    }
#ifdef HAVE_RENDER
    if (pm->pict != None)
    {
        XRenderFreePicture (myScreenGetXDisplay(pm->screen_info), pm->pict);
        pm->pict = None;
    }
#endif
}

static void
xfwmPixmapFillRectangle (Display *dpy, int screen, Pixmap pm, Drawable d, 
                         int x, int y, int width, int height)
{
    XGCValues gv;
    GC gc;
    unsigned long mask;

    TRACE ("entering fillRectangle");

    if ((width < 1) || (height < 1))
    {
        return;
    }
    gv.fill_style = FillTiled;
    gv.tile = pm;
    gv.ts_x_origin = x;
    gv.ts_y_origin = y;
    gv.foreground = WhitePixel (dpy, screen);
    if (gv.tile != None)
    {
        mask = GCTile | GCFillStyle | GCTileStipXOrigin;
    }
    else
    {
        mask = GCForeground;
    }
    gc = XCreateGC (dpy, d, mask, &gv);
    XFillRectangle (dpy, d, gc, x, y, width, height);
    XFreeGC (dpy, gc);
}

void
xfwmPixmapFill (xfwmPixmap * src, xfwmPixmap * dst, 
                gint x, gint y, gint width, gint height)
{
    TRACE ("entering xfwmWindowFill");

    if ((width < 1) || (height < 1))
    {
        return;
    }

    xfwmPixmapFillRectangle (myScreenGetXDisplay (src->screen_info), 
                             src->screen_info->screen,  
                             src->pixmap, dst->pixmap, x, y, width, height);
    xfwmPixmapFillRectangle (myScreenGetXDisplay (src->screen_info), 
                             src->screen_info->screen,  
                             src->mask, dst->mask, x, y, width, height);
#ifdef HAVE_RENDER
    xfwmPixmapRefreshPict (dst);
#endif
}
