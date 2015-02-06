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


        Metacity - (c) 2001 Havoc Pennington
        libwnck  - (c) 2001 Havoc Pennington
        xfwm4    - (c) 2002-2015 Olivier Fourdan
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libxfce4util/libxfce4util.h>

#include "inline-default-icon.h"
#include "icons.h"
#include "display.h"
#include "screen.h"
#include "client.h"
#include "compositor.h"
#include "hints.h"

static void
downsize_ratio (guint *width, guint *height, guint dest_w, guint dest_h)
{
    gdouble ratio;
    guint size;

    g_return_if_fail (width != NULL);
    g_return_if_fail (height != NULL);
    g_return_if_fail (dest_w > 0 && dest_w > 0);

    size = MIN (dest_w, dest_h);
    if (*width > *height)
    {
        ratio = ((gdouble) *width) / size;
        *width = (guint) size;
        *height = (guint) (((gdouble) *height) / ratio);
    }
    else
    {
        ratio = ((gdouble) *height) / size;
        *height = (guint) size;
        *width = (guint) (((gdouble) *width) / ratio);
    }
}

/*
 * create a GdkPixbuf from inline data and scale it to a given size
 */
static GdkPixbuf *
inline_icon_at_size (const guint8 *data, guint width, guint height)
{
    GdkPixbuf *base;

    base = gdk_pixbuf_new_from_inline (-1, data, FALSE, NULL);

    g_return_val_if_fail (base, NULL);

    if ((width <= 0 || height <= 0) ||
        ((guint) gdk_pixbuf_get_width (base) == width &&
         (guint) gdk_pixbuf_get_height (base) == height))
    {
        return base;
    }
    else
    {
        GdkPixbuf *scaled;
        guint w, h;

        w = gdk_pixbuf_get_width (base);
        h = gdk_pixbuf_get_height (base);
        downsize_ratio (&w, &h, width, height);
        scaled = gdk_pixbuf_scale_simple (base, w, h, GDK_INTERP_BILINEAR);

        g_object_unref (G_OBJECT (base));

        return scaled;
    }
}


static gboolean
find_largest_sizes (gulong * data, gulong nitems, guint *width, guint *height)
{
    guint w, h;

    *width = 0;
    *height = 0;

    while (nitems > 0)
    {
        if (nitems < 3)
        {
            return FALSE;       /* no space for w, h */
        }

        w = data[0];
        h = data[1];

        if (nitems < (gulong) ((w * h) + 2))
        {
            return FALSE;       /* not enough data */
        }

        *width = MAX (w, *width);
        *height = MAX (h, *height);

        data += (w * h) + 2;
        nitems -= (w * h) + 2;
    }

    return TRUE;
}

static gboolean
find_best_size (gulong * data, gulong nitems, gint ideal_width, gint ideal_height,
                guint *width, guint *height, gulong ** start)
{
    gulong *best_start;
    guint ideal_size;
    guint w, h, best_size, this_size;
    guint best_w, best_h, max_width, max_height;

    *width = 0;
    *height = 0;
    *start = NULL;

    if (!find_largest_sizes (data, nitems, &max_width, &max_height))
    {
        return FALSE;
    }

    if (ideal_width < 0)
    {
        ideal_width = max_width;
    }
    if (ideal_height < 0)
    {
        ideal_height = max_height;
    }

    best_w = 0;
    best_h = 0;
    best_start = NULL;

    while (nitems > 0)
    {
        gboolean replace;

        replace = FALSE;

        if (nitems < 3)
        {
            return FALSE;       /* no space for w, h */
        }

        w = data[0];
        h = data[1];

        if (nitems < (gulong) ((w * h) + 2))
        {
            break;              /* not enough data */
        }

        if (best_start == NULL)
        {
            replace = TRUE;
        }
        else
        {
            /* work with averages */
            ideal_size = (ideal_width + ideal_height) / 2;
            best_size = (best_w + best_h) / 2;
            this_size = (w + h) / 2;

            if ((best_size < ideal_size) && (this_size >= ideal_size))
            {
                /* larger than desired is always better than smaller */
                replace = TRUE;
            }
            else if ((best_size < ideal_size) && (this_size > best_size))
            {
                /* if we have too small, pick anything bigger */
                replace = TRUE;
            }
            else if ((best_size > ideal_size) && (this_size >= ideal_size) && (this_size < best_size))
            {
                /* if we have too large, pick anything smaller but still >= the ideal */
                replace = TRUE;
            }
        }

        if (replace)
        {
            best_start = data + 2;
            best_w = w;
            best_h = h;
        }

        data += (w * h) + 2;
        nitems -= (w * h) + 2;
    }

    if (best_start)
    {
        *start = best_start;
        *width = best_w;
        *height = best_h;
        return TRUE;
    }

    return FALSE;
}

static void
argbdata_to_pixdata (gulong * argb_data, guint len, guchar ** pixdata)
{
    guchar *p;
    guint argb;
    guint rgba;
    guint i;

    *pixdata = g_new (guchar, len * 4);
    p = *pixdata;

    i = 0;
    while (i < len)
    {
        argb = argb_data[i];
        rgba = (argb << 8) | (argb >> 24);

        *p = rgba >> 24; ++p;
        *p = (rgba >> 16) & 0xff; ++p;
        *p = (rgba >> 8) & 0xff; ++p;
        *p = rgba & 0xff; ++p;

        ++i;
    }
}

static gboolean
read_rgb_icon (DisplayInfo *display_info, Window window, guint ideal_width, guint ideal_height,
               guint *width, guint *height, guchar ** pixdata)
{
    gulong nitems;
    gulong *data;
    gulong *best;
    guint w, h;

    data = NULL;

    if (!getRGBIconData (display_info, window, &data, &nitems))
    {
        return FALSE;
    }

    if (!find_best_size (data, nitems, ideal_width, ideal_height, &w, &h, &best))
    {
        XFree (data);
        return FALSE;
    }

    *width = w;
    *height = h;

    argbdata_to_pixdata (best, w * h, pixdata);

    XFree (data);

    return TRUE;
}

static void
get_pixmap_geometry (Display *dpy, Pixmap pixmap, guint *w, guint *h)
{
    Window root;
    guint border_width;
    guint depth;
    int x, y;

    XGetGeometry (dpy, pixmap, &root, &x, &y, w, h, &border_width, &depth);
}

static GdkPixbuf *
apply_mask (GdkPixbuf * pixbuf, GdkPixbuf * mask)
{
    GdkPixbuf *with_alpha;
    guchar *src;
    guchar *dest;
    guint w, h, i, j;
    guint src_stride, dest_stride;

    w = MIN (gdk_pixbuf_get_width (mask), gdk_pixbuf_get_width (pixbuf));
    h = MIN (gdk_pixbuf_get_height (mask), gdk_pixbuf_get_height (pixbuf));

    with_alpha = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);

    dest = gdk_pixbuf_get_pixels (with_alpha);
    src = gdk_pixbuf_get_pixels (mask);

    dest_stride = gdk_pixbuf_get_rowstride (with_alpha);
    src_stride = gdk_pixbuf_get_rowstride (mask);

    i = 0;
    while (i < h)
    {
        j = 0;
        while (j < w)
        {
            guchar *s = src + i * src_stride + j * 3;
            guchar *d = dest + i * dest_stride + j * 4;

            /* s[0] == s[1] == s[2], they are 255 if the bit was set, 0
             * otherwise
             */
            if (s[0] == 0)
            {
                d[3] = 0;       /* transparent */
            }
            else
            {
                d[3] = 255;     /* opaque */
            }
            ++j;
        }
        ++i;
    }

    return with_alpha;
}

static GdkColormap *
get_cmap (GdkPixmap * pixmap, GdkScreen *gscreen)
{
    GdkColormap *cmap;

    g_return_val_if_fail (pixmap != NULL, NULL);

    cmap = gdk_drawable_get_colormap (pixmap);
    if (cmap)
    {
        g_message ("Drawable colormap");
        g_object_ref (G_OBJECT (cmap));
    }
    else
    {
        if (gdk_drawable_get_depth (pixmap) == 1)
        {
            /* try null cmap */
            cmap = NULL;
        }
        else if ((gdk_drawable_get_depth (pixmap) == 32) && (gscreen != NULL))
        {
            /* Try ARGB cmap */
            cmap = gdk_screen_get_rgba_colormap(gscreen);
            g_object_ref (G_OBJECT (cmap));
        }
        else
        {
            /* Try system cmap */
            cmap = gdk_colormap_get_system ();
            g_object_ref (G_OBJECT (cmap));
        }
    }

    /* Be sure we aren't going to blow up due to visual mismatch */
    if (cmap && (gdk_colormap_get_visual (cmap)->depth != gdk_drawable_get_depth (pixmap)))
    {
        g_object_unref (G_OBJECT (cmap));
        cmap = NULL;
    }

    return cmap;
}

static GdkPixbuf *
get_pixbuf_from_pixmap (GdkScreen *gscreen, Pixmap xpixmap, guint src_x, guint src_y,
                        gint dest_x, gint dest_y, guint width, guint height)
{
    GdkDrawable *drawable;
    GdkPixbuf *retval;
    GdkColormap *cmap;

    retval = NULL;

    drawable = gdk_xid_table_lookup (xpixmap);

    if (drawable)
    {
        g_object_ref (G_OBJECT (drawable));
    }
    else
    {
        drawable = gdk_pixmap_foreign_new (xpixmap);
    }

    if (G_UNLIKELY(!drawable))
    {
        /* Pixmap is gone ?? */
        return NULL;
    }

    cmap = get_cmap (drawable, gscreen);
    retval = gdk_pixbuf_get_from_drawable (NULL, drawable, cmap, src_x, src_y,
                                           dest_x, dest_y, width, height);

    if (G_LIKELY(cmap))
    {
        g_object_unref (G_OBJECT (cmap));
    }
    g_object_unref (G_OBJECT (drawable));

    return retval;
}

static GdkPixbuf *
try_pixmap_and_mask (ScreenInfo *screen_info, Pixmap src_pixmap, Pixmap src_mask, guint width, guint height)
{
    GdkPixbuf *unscaled;
    GdkPixbuf *icon;
    GdkPixbuf *mask;
    guint w, h;

    if (src_pixmap == None)
    {
        return NULL;
    }

    gdk_error_trap_push ();
    get_pixmap_geometry (myScreenGetXDisplay(screen_info), src_pixmap, &w, &h);
    unscaled = get_pixbuf_from_pixmap (screen_info->gscr, src_pixmap, 0, 0, 0, 0, w, h);
    icon = NULL;
    mask = NULL;

    if (unscaled && src_mask)
    {
        get_pixmap_geometry (myScreenGetXDisplay(screen_info), src_mask, &w, &h);
        mask = get_pixbuf_from_pixmap (screen_info->gscr, src_mask, 0, 0, 0, 0, w, h);
    }
    gdk_error_trap_pop ();

    if (mask)
    {
        GdkPixbuf *masked;

        masked = apply_mask (unscaled, mask);
        g_object_unref (G_OBJECT (unscaled));
        unscaled = masked;

        g_object_unref (G_OBJECT (mask));
        mask = NULL;
    }

    if (unscaled)
    {
        downsize_ratio (&w, &h, width, height);
        icon = gdk_pixbuf_scale_simple (unscaled, w, h, GDK_INTERP_BILINEAR);
        g_object_unref (G_OBJECT (unscaled));
        return icon;
    }

    return NULL;
}

static void
free_pixels (guchar * pixels, gpointer data)
{
    g_free (pixels);
}

static GdkPixbuf *
scaled_from_pixdata (guchar * pixdata, guint w, guint h, guint dest_w, guint dest_h)
{
    GdkPixbuf *src;
    GdkPixbuf *dest;

    src = gdk_pixbuf_new_from_data (pixdata, GDK_COLORSPACE_RGB, TRUE, 8, w, h, w * 4, free_pixels, NULL);

    if (G_UNLIKELY (src == NULL))
    {
        return NULL;
    }

    if (w != dest_w || h != dest_h)
    {
        downsize_ratio (&w, &h, dest_w, dest_h);
        dest = gdk_pixbuf_scale_simple (src, w, h, GDK_INTERP_BILINEAR);
        g_object_unref (G_OBJECT (src));
    }
    else
    {
        dest = src;
    }

    return dest;
}

GdkPixbuf *
getAppIcon (ScreenInfo *screen_info, Window window, guint width, guint height)
{
    XWMHints *hints;
    Pixmap pixmap;
    Pixmap mask;
    guchar *pixdata;
    guint w, h;

    pixdata = NULL;
    pixmap = None;
    mask = None;

    if (read_rgb_icon (screen_info->display_info, window, width, height, &w, &h, &pixdata))
    {
        return scaled_from_pixdata (pixdata, w, h, width, height);
    }

    gdk_error_trap_push ();
    hints = XGetWMHints (myScreenGetXDisplay(screen_info), window);
    gdk_error_trap_pop ();

    if (hints)
    {
        if (hints->flags & IconPixmapHint)
        {
            pixmap = hints->icon_pixmap;
        }
        if (hints->flags & IconMaskHint)
        {
            mask = hints->icon_mask;
        }

        XFree (hints);
        hints = NULL;
    }

    if (pixmap != None)
    {
        GdkPixbuf *icon = try_pixmap_and_mask (screen_info, pixmap, mask, width, height);
        if (icon)
        {
            return icon;
        }
    }

    getKDEIcon (screen_info->display_info, window, &pixmap, &mask);
    if (pixmap != None)
    {
        GdkPixbuf *icon = try_pixmap_and_mask (screen_info, pixmap, mask, width, height);
        if (icon)
        {
            return icon;
        }
    }

    return inline_icon_at_size (default_icon_data, width, height);
}

GdkPixbuf *
getClientIcon (Client *c, guint width, guint height)
{
    ScreenInfo *screen_info;
    GdkPixbuf *app_content;
    GdkPixbuf *small_icon;
    GdkPixbuf *icon_pixbuf;
    GdkPixbuf *icon_pixbuf_stated;
    guint small_icon_size;
    guint app_icon_width, app_icon_height;
    Pixmap pixmap;

    g_return_val_if_fail (c != NULL, NULL);

    screen_info = c->screen_info;
    icon_pixbuf = NULL;
    app_icon_width = width; /* Set to 0 to use gdk pixbuf scaling */
    app_icon_height = height; /* Set to 0 to use gdk pixbuf scaling */

    icon_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    gdk_pixbuf_fill (icon_pixbuf, 0x00);

    pixmap = compositorGetWindowPixmapAtSize (screen_info, c->frame, &app_icon_width, &app_icon_height);
    if (pixmap != None)
    {
        app_content = try_pixmap_and_mask (screen_info, pixmap, None, width, height);
        XFreePixmap (myScreenGetXDisplay (screen_info), pixmap);
    }
    else
    {
        app_content = inline_icon_at_size (default_icon_data, width, height);
    }

    app_icon_width = (guint) gdk_pixbuf_get_width (app_content);
    app_icon_height = (guint) gdk_pixbuf_get_height (app_content);

    gdk_pixbuf_copy_area (app_content, 0, 0, app_icon_width, app_icon_height, icon_pixbuf,
                          (width - app_icon_width) / 2, (height - app_icon_height) / 2);
    g_object_unref (app_content);

    small_icon_size = MIN (width / 4, height / 4);
    small_icon_size = MIN (small_icon_size, 48);

    small_icon = getAppIcon (screen_info, c->window, small_icon_size, small_icon_size);

    gdk_pixbuf_composite (small_icon, icon_pixbuf,
                          (width - small_icon_size) / 2, height - small_icon_size,
                          small_icon_size, small_icon_size,
                          (width - small_icon_size) / 2, height - small_icon_size,
                          1.0, 1.0,
                          GDK_INTERP_BILINEAR,
                          0xff);

    g_object_unref (small_icon);

    if (FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
    {
        icon_pixbuf_stated = gdk_pixbuf_copy (icon_pixbuf);
        gdk_pixbuf_saturate_and_pixelate (icon_pixbuf, icon_pixbuf_stated, 0.55, TRUE);
        g_object_unref (icon_pixbuf);
        icon_pixbuf = icon_pixbuf_stated;
    }

    return icon_pixbuf;
}
