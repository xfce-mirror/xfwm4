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

        xcompmgr - (c) 2003 Keith Packard
        metacity - (c) 2003, 2004 Red Hat, Inc.
        xfwm4    - (c) 2004 Olivier Fourdan

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include <glib.h>
#include <math.h>
#include <libxfce4util/libxfce4util.h>

#include "display.h"
#include "screen.h"
#include "client.h"
#include "frame.h"
#include "hints.h"

#ifdef HAVE_COMPOSITOR

#define WINDOW_SOLID    0
#define WINDOW_TRANS    1
#define WINDOW_ARGB     2

#ifndef SHADOW_RADIUS
#define SHADOW_RADIUS   6
#endif /* SHADOW_RADIUS */

#ifndef SHADOW_OPACITY
#define SHADOW_OPACITY  0.66
#endif /* SHADOW_OPACITY */

#define SHADOW_OFFSET_X (-SHADOW_RADIUS * 5 / 4)
#define SHADOW_OFFSET_Y (-SHADOW_RADIUS * 5 / 4)

typedef struct _CWindow CWindow;
struct _CWindow
{
    ScreenInfo *screen_info;
    Client *c;
    Window id;
    XWindowAttributes attr;

    Damage damage;
    gboolean damaged;
    gboolean viewable;

#if HAVE_NAME_WINDOW_PIXMAP
    Pixmap name_window_pixmap;
#endif /* HAVE_NAME_WINDOW_PIXMAP */
    Picture picture;
    Picture shadow;
    Picture alphaPict;
    Picture shadowPict;
    XserverRegion borderSize;
    XserverRegion borderClip;
    XserverRegion extents;
    gint shadow_dx;
    gint shadow_dy;
    gint shadow_width;
    gint shadow_height;
    gint mode;
    guint opacity;
    gboolean skipped;
};

static CWindow*
find_cwindow_in_screen (ScreenInfo *screen_info, Window id)
{
    GList *index;

    g_return_val_if_fail (id != None, NULL);
    g_return_val_if_fail (screen_info != NULL, NULL);
    TRACE ("entering find_cwindow_in_screen");

    for (index = screen_info->cwindows; index; index = g_list_next (index))
    {
        CWindow *cw = (CWindow *) index->data;
        if (cw->id == id)
        {
            return cw;
        }
    }
    return NULL;
}

static CWindow*
find_cwindow_in_display (DisplayInfo *display_info, Window id)
{
    GSList *index;

    g_return_val_if_fail (id != None, NULL);
    g_return_val_if_fail (display_info != NULL, NULL);
    TRACE ("entering find_cwindow_in_display");

    for (index = display_info->screens; index; index = g_slist_next (index))
    {
        ScreenInfo *screen_info = (ScreenInfo *) index->data;
        CWindow *cw = find_cwindow_in_screen (screen_info, id);
        if (cw)
        {
            return (cw);
        }
    }
    return NULL;
}

static gdouble
gaussian (gdouble r, gdouble x, gdouble y)
{
    return ((1 / (sqrt (2 * G_PI * r))) *
            exp ((- (x * x + y * y)) / (2 * r * r)));
}

static gaussian_conv *
make_gaussian_map (gdouble r)
{
    gaussian_conv *c;
    gint size = ((gint) ceil ((r * 3)) + 1) & ~1;
    gint center = size / 2;
    gint x, y;
    gdouble t;
    gdouble g;

    TRACE ("entering make_gaussian_map");

    c = g_malloc (sizeof (gaussian_conv) + size * size * sizeof (gdouble));
    c->size = size;
    c->data = (gdouble *) (c + 1);
    t = 0.0;

    for (y = 0; y < size; y++)
    {
        for (x = 0; x < size; x++)
        {
            g = gaussian (r, (gdouble) (x - center), (gdouble) (y - center));
            t += g;
            c->data[y * size + x] = g;
        }
    }

    for (y = 0; y < size; y++)
    {
        for (x = 0; x < size; x++)
        {
            c->data[y*size + x] /= t;
        }
    }

    return c;
}

/*
* A picture will help
*
*      -center   0                width  width+center
*  -center +-----+-------------------+-----+
*          |     |                   |     |
*          |     |                   |     |
*        0 +-----+-------------------+-----+
*          |     |                   |     |
*          |     |                   |     |
*          |     |                   |     |
*   height +-----+-------------------+-----+
*          |     |                   |     |
* height+  |     |                   |     |
*  center  +-----+-------------------+-----+
*/

static guchar
sum_gaussian (gaussian_conv *map, gdouble opacity, gint x, gint y, gint width, gint height)
{
    gint fx, fy;
    gdouble *g_data;
    gdouble *g_line = map->data;
    gint g_size = map->size;
    gint center = g_size / 2;
    gint fx_start, fx_end;
    gint fy_start, fy_end;
    gdouble v;

    g_return_val_if_fail (map != NULL, (guchar) 255.0);
    TRACE ("entering sum_gaussian");

    fx_start = center - x;
    if (fx_start < 0)
    {
        fx_start = 0;
    }
    fx_end = width + center - x;
    if (fx_end > g_size)
    {
        fx_end = g_size;
    }

    fy_start = center - y;
    if (fy_start < 0)
    {
        fy_start = 0;
    }
    fy_end = height + center - y;
    if (fy_end > g_size)
    {
        fy_end = g_size;
    }
    g_line = g_line + fy_start * g_size + fx_start;

    v = 0;
    for (fy = fy_start; fy < fy_end; fy++)
    {
        g_data = g_line;
        g_line += g_size;

        for (fx = fx_start; fx < fx_end; fx++)
        {
            v += *g_data++;
        }
    }
    if (v > 1)
    {
        v = 1;
    }

    return ((guchar) (v * opacity * 255.0));
}

/* precompute shadow corners and sides to save time for large windows */
static void
presum_gaussian (ScreenInfo *screen_info)
{
    gint center;
    gint opacity, x, y;
    gaussian_conv * map;

    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (screen_info->gaussianMap != NULL);
    TRACE ("entering presum_gaussian");

    map = screen_info->gaussianMap;
    screen_info->gsize = map->size;
    center = map->size / 2;

    if (screen_info->shadowCorner)
    {
        g_free (screen_info->shadowCorner);
    }
    if (screen_info->shadowTop)
    {
        g_free (screen_info->shadowTop);
    }

    screen_info->shadowCorner = (guchar *) (g_malloc ((screen_info->gsize + 1)
                                                    * (screen_info->gsize + 1) * 26));
    screen_info->shadowTop = (guchar *) (g_malloc ((screen_info->gsize + 1) * 26));

    for (x = 0; x <= screen_info->gsize; x++)
    {
        screen_info->shadowTop[25 * (screen_info->gsize + 1) + x] =
            sum_gaussian (map, 1, x - center, center,
                                screen_info->gsize * 2,
                                screen_info->gsize * 2);

        for(opacity = 0; opacity < 25; opacity++)
        {
            screen_info->shadowTop[opacity * (screen_info->gsize + 1) + x] =
                screen_info->shadowTop[25 * (screen_info->gsize + 1) + x] * opacity / 25;
        }

        for(y = 0; y <= x; y++)
        {
            screen_info->shadowCorner[25 * (screen_info->gsize + 1)
                                        * (screen_info->gsize + 1)
                                    + y * (screen_info->gsize + 1) + x]
                = sum_gaussian (map, 1, x - center,
                                        y - center,
                                        screen_info->gsize * 2,
                                        screen_info->gsize * 2);
            screen_info->shadowCorner[25 * (screen_info->gsize + 1)
                                        * (screen_info->gsize + 1)
                                    + x * (screen_info->gsize + 1) + y]
                = screen_info->shadowCorner[25 * (screen_info->gsize + 1)
                                            * (screen_info->gsize + 1)
                                        + y * (screen_info->gsize + 1) + x];

            for(opacity = 0; opacity < 25; opacity++)
            {
                screen_info->shadowCorner[opacity * (screen_info->gsize + 1)
                                                * (screen_info->gsize + 1)
                                            + y * (screen_info->gsize + 1) + x]
                    = screen_info->shadowCorner[opacity * (screen_info->gsize + 1)
                                                        * (screen_info->gsize + 1)
                                                    + x * (screen_info->gsize + 1) + y]
                    = screen_info->shadowCorner[25 * (screen_info->gsize + 1)
                                                * (screen_info->gsize + 1)
                                            + y * (screen_info->gsize + 1) + x] * opacity / 25;
            }
        }
    }
}

static XImage *
make_shadow (ScreenInfo *screen_info, gdouble opacity, gint width, gint height)
{
    XImage *ximage;
    guchar *data;
    guchar d;
    gint gsize;
    gint ylimit, xlimit;
    gint swidth;
    gint sheight;
    gint center;
    gint x, y;
    gint x_diff;
    gint opacity_int;

    g_return_val_if_fail (screen_info != NULL, NULL);
    TRACE ("entering make_shadow");

    gsize = screen_info->gaussianMap->size;
    swidth = width + gsize - screen_info->params->shadow_delta_width - screen_info->params->shadow_delta_x;
    sheight = height + gsize - screen_info->params->shadow_delta_height - screen_info->params->shadow_delta_y;
    center = gsize / 2;
    opacity_int = (gint) (opacity * 25);

    if ((swidth < 1) || (sheight < 1))
    {
        return NULL;
    }

    data = g_malloc (swidth * sheight * sizeof (guchar));

    ximage = XCreateImage (myScreenGetXDisplay (screen_info),
                        DefaultVisual(myScreenGetXDisplay (screen_info), screen_info->screen),
                        8, ZPixmap, 0, (char *) data,
                        swidth, sheight, 8, swidth * sizeof (guchar));
    if (ximage == NULL)
    {
        g_free (data);
        g_warning ("(ximage != NULL) failed");
        return NULL;
    }

    /*
    * Build the gaussian in sections
    */

    if (screen_info->gsize > 0)
    {
        d = screen_info->shadowTop[opacity_int * (screen_info->gsize + 1) + screen_info->gsize];
    }
    else
    {
        d = sum_gaussian (screen_info->gaussianMap, opacity, center, center, width, height);
    }
    memset(data, d, sheight * swidth);

    /*
    * corners
    */

    ylimit = gsize;
    if (ylimit > sheight / 2)
    {
        ylimit = (sheight + 1) / 2;
    }
    xlimit = gsize;
    if (xlimit > swidth / 2)
    {
        xlimit = (swidth + 1) / 2;
    }
    for (y = 0; y < ylimit; y++)
    {
        for (x = 0; x < xlimit; x++)
        {
            if ((xlimit == screen_info->gsize) && (ylimit == screen_info->gsize))
            {
                d = screen_info->shadowCorner[opacity_int * (screen_info->gsize + 1)
                                                        * (screen_info->gsize + 1)
                                                    + y * (screen_info->gsize + 1) + x];
            }
            else
            {
                d = sum_gaussian (screen_info->gaussianMap, opacity,
                                x - center, y - center, width, height);
            }

            data[y * swidth + x] = d;
            data[(sheight - y - 1) * swidth + x] = d;
            data[(sheight - y - 1) * swidth + (swidth - x - 1)] = d;
            data[y * swidth + (swidth - x - 1)] = d;
        }
    }

    /*
    * top/bottom
    */

    x_diff = swidth - (gsize * 2);
    if (x_diff > 0 && ylimit > 0)
    {
        for (y = 0; y < ylimit; y++)
        {
            if (ylimit == screen_info->gsize)
            {
                d = screen_info->shadowTop[opacity_int * (screen_info->gsize + 1) + y];
            }
            else
            {
                d = sum_gaussian (screen_info->gaussianMap, opacity, center, y - center, width, height);
            }
            memset (&data[y * swidth + gsize], d, x_diff);
            memset (&data[(sheight - y - 1) * swidth + gsize], d, x_diff);
        }
    }

    /*
    * sides
    */

    for (x = 0; x < xlimit; x++)
    {
        if (xlimit == screen_info->gsize)
        {
            d = screen_info->shadowTop[opacity_int * (screen_info->gsize + 1) + x];
        }
        else
        {
            d = sum_gaussian (screen_info->gaussianMap, opacity, x - center, center, width, height);
        }
        for (y = gsize; y < sheight - gsize; y++)
        {
            data[y * swidth + x] = d;
            data[y * swidth + (swidth - x - 1)] = d;
        }
    }

    return ximage;
}

static Picture
shadow_picture (ScreenInfo *screen_info, gdouble opacity,
                gint width, gint height, gint *wp, gint *hp)
{
    XImage *shadowImage;
    Pixmap shadowPixmap;
    Picture shadowPicture;
    XRenderPictFormat *render_format;
    GC gc;

    g_return_val_if_fail (screen_info != NULL, None);
    TRACE ("entering shadow_picture");

    render_format = XRenderFindStandardFormat (myScreenGetXDisplay (screen_info), PictStandardA8);
    g_return_val_if_fail (render_format != NULL, None);

    shadowImage = make_shadow (screen_info, opacity, width, height);
    if (shadowImage == NULL)
    {
        *wp = *hp = 0;
        return (None);
    }

    shadowPixmap = XCreatePixmap (myScreenGetXDisplay (screen_info), screen_info->xroot,
                                shadowImage->width, shadowImage->height, 8);
    if (shadowPixmap == None)
    {
        XDestroyImage (shadowImage);
        g_warning ("(shadowPixmap != None) failed");
        return None;
    }

    shadowPicture = XRenderCreatePicture (myScreenGetXDisplay (screen_info),
                                        shadowPixmap, render_format, 0, 0);
    if (shadowPicture == None)
    {
        XDestroyImage (shadowImage);
        XFreePixmap (myScreenGetXDisplay (screen_info), shadowPixmap);
        g_warning ("(shadowPicture != None) failed");
        return None;
    }

    gc = XCreateGC (myScreenGetXDisplay (screen_info), shadowPixmap, 0, 0);
    if (gc == None)
    {
        XDestroyImage (shadowImage);
        XFreePixmap (myScreenGetXDisplay (screen_info), shadowPixmap);
        XRenderFreePicture (myScreenGetXDisplay (screen_info), shadowPicture);
        g_warning ("(gc != None) failed");
        return None;
    }

    XPutImage (myScreenGetXDisplay (screen_info), shadowPixmap, gc, shadowImage, 0, 0, 0, 0,
            shadowImage->width, shadowImage->height);

    *wp = shadowImage->width;
    *hp = shadowImage->height;

    XFreeGC (myScreenGetXDisplay (screen_info), gc);
    XDestroyImage (shadowImage);
    XFreePixmap (myScreenGetXDisplay (screen_info), shadowPixmap);

    return shadowPicture;
}

static Picture
solid_picture (ScreenInfo *screen_info, gboolean argb,
               gdouble a, gdouble r, gdouble g, gdouble b)
{
    Pixmap pixmap;
    Picture picture;
    XRenderPictureAttributes pa;
    XRenderPictFormat *render_format;
    XRenderColor c;

    g_return_val_if_fail (screen_info, None);
    TRACE ("entering solid_picture");

    render_format = XRenderFindStandardFormat (myScreenGetXDisplay (screen_info),
                                    argb ? PictStandardARGB32 : PictStandardA8);
    g_return_val_if_fail (render_format != NULL , None);

    pixmap = XCreatePixmap (myScreenGetXDisplay (screen_info),
                            screen_info->xroot, 1, 1, argb ? 32 : 8);
    g_return_val_if_fail (pixmap != None, None);

    pa.repeat = True;
    picture = XRenderCreatePicture (myScreenGetXDisplay (screen_info), pixmap,
                                    render_format, CPRepeat,  &pa);
    if (picture == None)
    {
        XFreePixmap (myScreenGetXDisplay (screen_info), pixmap);
        g_warning ("(picture != None) failed");
        return None;
    }

    c.alpha = a * 0xffff;
    c.red = r * 0xffff;
    c.green = g * 0xffff;
    c.blue = b * 0xffff;

    XRenderFillRectangle (myScreenGetXDisplay (screen_info), PictOpSrc,
                          picture, &c, 0, 0, 1, 1);
    XFreePixmap (myScreenGetXDisplay (screen_info), pixmap);

    return picture;
}

static XserverRegion
border_size (CWindow *cw)
{
    XserverRegion border;
    DisplayInfo *display_info = NULL;
    ScreenInfo *screen_info = NULL;

    g_return_val_if_fail (cw != NULL, None);
    TRACE ("entering border_size");

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

    border = XFixesCreateRegionFromWindow (display_info->dpy, cw->id, WindowRegionBounding);
    g_return_val_if_fail (border != None, None);

    XFixesTranslateRegion (display_info->dpy, border,
                           cw->attr.x + cw->attr.border_width,
                           cw->attr.y + cw->attr.border_width);

    return border;
}

static Picture
root_tile (ScreenInfo *screen_info)
{
    DisplayInfo *display_info = NULL;
    Display *dpy;
    Picture picture;
    Pixmap pixmap;
    Bool fill;
    XRenderPictureAttributes pa;
    XRenderPictFormat *format;
    gint p;
    Atom backgroundProps[2];

    g_return_val_if_fail (screen_info != NULL, None);
    TRACE ("entering root_tile");

    display_info = screen_info->display_info;
    dpy = display_info->dpy;
    pixmap = None;
    backgroundProps[0] = display_info->atoms[XROOTPMAP];
    backgroundProps[1] = display_info->atoms[XSETROOT];

    for (p = 0; p < 2; p++)
    {
        Atom actual_type;
        gint actual_format;
        unsigned long nitems;
        unsigned long bytes_after;
        guchar *prop;
        gint result;

        result = XGetWindowProperty (dpy, screen_info->xroot, backgroundProps[p],
                                0, 4, False, AnyPropertyType,
                                &actual_type, &actual_format, &nitems, &bytes_after, &prop);

        if ((result == Success) &&
            (actual_type == display_info->atoms[PIXMAP]) &&
            (actual_format == 32) &&
            (nitems == 1))
        {
            memcpy (&pixmap, prop, 4);
            XFree (prop);
            fill = False;
            break;
        }
    }

    if (!pixmap)
    {
        pixmap = XCreatePixmap (dpy, screen_info->xroot, 1, 1,
                                DefaultDepth (dpy, screen_info->screen));
        g_return_val_if_fail (pixmap != None, None);
        fill = True;
    }
    pa.repeat = True;
    format = XRenderFindVisualFormat (dpy, DefaultVisual (dpy, screen_info->screen));
    g_return_val_if_fail (format != NULL, None);

    picture = XRenderCreatePicture (dpy, pixmap, format, CPRepeat, &pa);
    if ((picture != None) && (fill))
    {
        XRenderColor c;

        c.red = c.green = c.blue = 0xd0d0;
        c.alpha = 0xffff;
        XRenderFillRectangle (dpy, PictOpSrc, picture, &c, 0, 0, 1, 1);
    }
    return picture;
}

static Picture
create_root_buffer (ScreenInfo *screen_info)
{
    Picture pict;
    XRenderPictFormat *format;
    Pixmap  rootPixmap;
    Visual *visual;
    gint depth;
    gint screen_width;
    gint screen_height;
    gint screen_number;

    g_return_val_if_fail (screen_info != NULL, None);
    TRACE ("entering create_root_buffer");

    screen_width = gdk_screen_get_width (screen_info->gscr);
    screen_height = gdk_screen_get_height (screen_info->gscr);
    screen_number = screen_info->screen;
    visual = DefaultVisual (myScreenGetXDisplay (screen_info), screen_number);
    depth = DefaultDepth (myScreenGetXDisplay (screen_info), screen_number);

    format = XRenderFindVisualFormat (myScreenGetXDisplay (screen_info), visual);
    g_return_val_if_fail (format != NULL, None);

    rootPixmap = XCreatePixmap (myScreenGetXDisplay (screen_info),
                                screen_info->xroot,
                                screen_width, screen_height, depth);
    g_return_val_if_fail (rootPixmap != None, None);

    pict = XRenderCreatePicture (myScreenGetXDisplay (screen_info),
                                 rootPixmap, format, 0, NULL);
    XFreePixmap (myScreenGetXDisplay (screen_info), rootPixmap);

    return pict;
}

static void
paint_root (ScreenInfo *screen_info)
{
    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (screen_info->rootBuffer != None);

    TRACE ("entering paint_root");
    if (screen_info->rootTile == None)
    {
        screen_info->rootTile = root_tile (screen_info);
        g_return_if_fail (screen_info->rootTile != None);
    }

    XRenderComposite (myScreenGetXDisplay (screen_info), PictOpSrc,
                    screen_info->rootTile, None, screen_info->rootBuffer,
                    0, 0, 0, 0, 0, 0,
                    gdk_screen_get_width (screen_info->gscr),
                    gdk_screen_get_height (screen_info->gscr));
}

static XserverRegion
win_extents (CWindow *cw)
{
    ScreenInfo *screen_info = NULL;
    Client *c;
    XRectangle r;

    g_return_val_if_fail (cw != NULL, None);
    TRACE ("entering win_extents: 0x%lx", cw->id);

    screen_info = cw->screen_info;
    c = cw->c;
    r.x = cw->attr.x;
    r.y = cw->attr.y;
    r.width = cw->attr.width + cw->attr.border_width * 2;
    r.height = cw->attr.height + cw->attr.border_width * 2;

    if (c)
    {
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER) && (cw->mode != WINDOW_ARGB))
        {
            XRectangle sr;

            TRACE ("window 0x%lx (%s) has extents", cw->id, c->name);
            cw->shadow_dx = SHADOW_OFFSET_X + screen_info->params->shadow_delta_x;
            cw->shadow_dy = SHADOW_OFFSET_Y + screen_info->params->shadow_delta_y;
            if (!(cw->shadow))
            {
                double opacity = SHADOW_OPACITY;
                if (cw->mode == WINDOW_TRANS)
                {
                    opacity = opacity * ((double) cw->opacity) / ((double) NET_WM_OPAQUE);
                }
                cw->shadow = shadow_picture (screen_info, opacity,
                                             cw->attr.width + 2 * cw->attr.border_width,
                                             cw->attr.height + 2 * cw->attr.border_width,
                                             &cw->shadow_width, &cw->shadow_height);
            }

            sr.x = cw->attr.x + cw->shadow_dx;
            sr.y = cw->attr.y + cw->shadow_dy;
            sr.width = cw->shadow_width;
            sr.height = cw->shadow_height;

            if (sr.x < r.x)
            {
                r.width = (r.x + r.width) - sr.x;
                r.x = sr.x;
            }
            if (sr.y < r.y)
            {
                r.height = (r.y + r.height) - sr.y;
                r.y = sr.y;
            }
            if (sr.x + sr.width > r.x + r.width)
            {
                r.width = sr.x + sr.width - r.x;
            }
            if (sr.y + sr.height > r.y + r.height)
            {
                r.height = sr.y + sr.height - r.y;
            }
        }
    }

    return XFixesCreateRegion (myScreenGetXDisplay (screen_info), &r, 1);
}

static void
get_paint_bounds (CWindow *cw, gint *x, gint *y, gint *w, gint *h)
{
    TRACE ("entering get_paint_bounds");

#if HAVE_NAME_WINDOW_PIXMAP
    *x = cw->attr.x;
    *y = cw->attr.y;
    *w = cw->attr.width + 2 * cw->attr.border_width;
    *h = cw->attr.height + 2 * cw->attr.border_width;
#else
    *x = cw->attr.x + cw->attr.border_width;
    *y = cw->attr.y + cw->attr.border_width;
    *w = cw->attr.width;
    *h = cw->attr.height;
#endif
}

static XRenderPictFormat *
get_window_format (CWindow *cw)
{
    XRenderPictFormat *format;

    g_return_val_if_fail (cw != NULL, NULL);
    TRACE ("entering get_window_format");

    format = XRenderFindVisualFormat (myScreenGetXDisplay (cw->screen_info), cw->attr.visual);
    if (!format)
    {
        format = XRenderFindVisualFormat (myScreenGetXDisplay (cw->screen_info),
                                          DefaultVisual (myScreenGetXDisplay (cw->screen_info),
                                                         cw->screen_info->screen));
    }

    return format;
}

static Picture
get_window_picture (CWindow *cw)
{
    Display *dpy;
    DisplayInfo *display_info = NULL;
    ScreenInfo *screen_info = NULL;
    XRenderPictureAttributes pa;
    XRenderPictFormat *format;
    Drawable draw;

    g_return_val_if_fail (cw != NULL, None);
    TRACE ("entering get_window_picture");

    draw = cw->id;
    screen_info = cw->screen_info;
    display_info = screen_info->display_info;
    dpy = display_info->dpy;

#if HAVE_NAME_WINDOW_PIXMAP
    if ((display_info->have_name_window_pixmap) && (cw->name_window_pixmap == None))
    {
        cw->name_window_pixmap = XCompositeNameWindowPixmap (dpy, cw->id);
    }
    if (cw->name_window_pixmap != None)
    {
        draw = cw->name_window_pixmap;
    }
#endif
    format = get_window_format (cw);
    if (format)
    {
        pa.subwindow_mode = IncludeInferiors;
        return XRenderCreatePicture (dpy, draw, format, CPSubwindowMode, &pa);
    }
    return None;
}

static void
paint_all (ScreenInfo *screen_info, XserverRegion region)
{
    XserverRegion dest_region;
    DisplayInfo *display_info = NULL;
    Display *dpy;
    GList *index;
    gint screen_width;
    gint screen_height;
    gint screen_number;
    Window xroot;

    TRACE ("entering paint_all");
    g_return_if_fail (screen_info);


    display_info = screen_info->display_info;
    dpy = display_info->dpy;
    screen_width = gdk_screen_get_width (screen_info->gscr);
    screen_height = gdk_screen_get_height (screen_info->gscr);
    screen_number = screen_info->screen;
    xroot = screen_info->xroot;
    dest_region = None;

    if (region == None)
    {
        XRectangle  r;

        TRACE ("region is empty, creating a whole screen region");
        r.x = 0;
        r.y = 0;
        r.width = screen_width;
        r.height = screen_height;
        region = XFixesCreateRegion (dpy, &r, 1);
        dest_region = None;
    }
    else
    {
        dest_region = XFixesCreateRegion (dpy, NULL, 0);
        XFixesCopyRegion (dpy, dest_region, region);
    }

    /* Create root buffer if not done yet */
    if (screen_info->rootBuffer == None)
    {
        screen_info->rootBuffer = create_root_buffer (screen_info);
        g_return_if_fail (screen_info->rootBuffer != None);
    }

    XFixesSetPictureClipRegion (dpy, screen_info->rootPicture, 0, 0, region);

    /*
     * Painting from top to bottom, reducing the clipping area at each iteration.
     * Only the opaque windows are painted 1st.
     */
    for (index = screen_info->cwindows; index; index = g_list_next (index))
    {
        CWindow *cw = (CWindow *) index->data;

        TRACE ("painting forward 0x%lx", cw->id);

        if (!(cw->damaged) || !(cw->viewable))
        {
            TRACE ("skipped, not damaged or not viewable 0x%lx", cw->id);
            cw->skipped = TRUE;
            continue;
        }

        if ((cw->attr.x + cw->attr.width < 1) || (cw->attr.y + cw->attr.height < 1) ||
            (cw->attr.x >= screen_width) || (cw->attr.y >= screen_height))
        {
            TRACE ("skipped, off screen 0x%lx", cw->id);
            cw->skipped = TRUE;
            continue;
        }

        if (cw->extents == None)
        {
            cw->extents = win_extents (cw);
        }
        if (cw->borderSize == None)
        {
            cw->borderSize = border_size (cw);
        }
        if (cw->picture == None)
        {
            cw->picture = get_window_picture (cw);
        }
        if (cw->mode == WINDOW_SOLID)
        {
            gint x, y, w, h;
            get_paint_bounds (cw, &x, &y, &w, &h);
            XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, region);
            XRenderComposite (dpy, PictOpSrc, cw->picture, None, screen_info->rootBuffer,
                              0, 0, 0, 0, x, y, w, h);
            XFixesSubtractRegion (dpy, region, region, cw->borderSize);
        }
        if (cw->borderClip == None)
        {
            cw->borderClip = XFixesCreateRegion (dpy, 0, 0);
            XFixesCopyRegion (dpy, cw->borderClip, region);
        }

        cw->skipped = FALSE;
    }

    /*
     * region has changed because of the XFixesSubtractRegion (),
     * reapply clipping for the last iteration.
     */
    XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, region);
    paint_root (screen_info);

    /*
     * Painting from bottom to top, translucent windows and shadows are painted now...
     */
    for (index = g_list_last(screen_info->cwindows); index; index = g_list_previous (index))
    {
        CWindow *cw = (CWindow *) index->data;

        TRACE ("painting backward 0x%lx", cw->id);

        if (cw->skipped)
        {
            TRACE ("skipped 0x%lx", cw->id);
            continue;
        }

        if (cw->shadow)
        {
            XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, cw->borderClip);
            XRenderComposite (dpy, PictOpOver, screen_info->blackPicture, cw->shadow,
                              screen_info->rootBuffer, 0, 0, 0, 0,
                              cw->attr.x + cw->shadow_dx,
                              cw->attr.y + cw->shadow_dy,
                              cw->shadow_width, cw->shadow_height);
        }
        if (cw->mode != WINDOW_SOLID)
        {
            gint x, y, w, h;

            if ((cw->opacity != NET_WM_OPAQUE) && !(cw->alphaPict))
            {
                cw->alphaPict = solid_picture (screen_info, FALSE,
                                               (double) cw->opacity / NET_WM_OPAQUE, 0, 0, 0);
            }

            XFixesIntersectRegion (dpy, cw->borderClip, cw->borderClip, cw->borderSize);
            XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, cw->borderClip);

            /* cw->alphaPict can be None in the case of ARGB windows, that's ok */
            get_paint_bounds (cw, &x, &y, &w, &h);
            XRenderComposite (dpy, PictOpOver, cw->picture, cw->alphaPict,
                              screen_info->rootBuffer, 0, 0, 0, 0, x, y, w, h);
        }
        if (cw->borderClip)
        {
            XFixesDestroyRegion (dpy, cw->borderClip);
            cw->borderClip = None;
        }
    }
    XFixesDestroyRegion (dpy, region);

    TRACE ("Copying data back to screen");
    XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, dest_region);
    XRenderComposite (dpy, PictOpSrc, screen_info->rootBuffer, None, screen_info->rootPicture,
                      0, 0, 0, 0, 0, 0, screen_width, screen_height);
    if (dest_region != None)
    {
        XFixesDestroyRegion (dpy, dest_region);
    }
}

static void
repair_screen (ScreenInfo *screen_info)
{
    g_return_if_fail (screen_info);
    TRACE ("entering repair_screen");

    if (screen_info->allDamage != None)
    {
        paint_all (screen_info, screen_info->allDamage);
        screen_info->allDamage = None;
    }
}

static void
remove_timeouts (DisplayInfo *display_info)
{
    if (display_info->compositor_idle_id != 0)
    {
        g_source_remove (display_info->compositor_idle_id);
        display_info->compositor_idle_id = 0;
    }

    if (display_info->compositor_timeout_id != 0)
    {
        g_source_remove (display_info->compositor_timeout_id);
        display_info->compositor_timeout_id = 0;
    }
}

static void
repair_display (DisplayInfo *display_info)
{
    GSList *screens;

    g_return_if_fail (display_info);
    TRACE ("entering repair_display");

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    remove_timeouts (display_info);
    for (screens = display_info->screens; screens; screens = g_slist_next (screens))
    {
        repair_screen ((ScreenInfo *) screens->data);
    }
}

static gboolean
compositor_idle_cb (gpointer data)
{
    DisplayInfo *display_info = (DisplayInfo *) data;

    display_info->compositor_idle_id = 0;
    repair_display (display_info);

    return FALSE;
}

static gboolean
compositor_timeout_cb (gpointer data)
{
    DisplayInfo *display_info = (DisplayInfo *) data;

    display_info->compositor_timeout_id = 0;
    repair_display (display_info);

    return FALSE;
}

static void
add_repair (DisplayInfo *display_info)
{
    if (display_info->compositor_idle_id != 0)
    {
        return;
    }

    display_info->compositor_idle_id =
        g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                         compositor_idle_cb, display_info, NULL);

    if (display_info->compositor_timeout_id != 0)
    {
        return;
    }

    display_info->compositor_timeout_id = 
        g_timeout_add (50 /* ms */, 
                       compositor_timeout_cb, display_info);

}

static void
free_win_data (CWindow *cw, gboolean delete)
{
#if HAVE_NAME_WINDOW_PIXMAP
    if (cw->name_window_pixmap)
    {
        XFreePixmap (myScreenGetXDisplay (cw->screen_info), cw->name_window_pixmap);
        cw->name_window_pixmap = None;
    }
#endif

    if (cw->picture)
    {
        XRenderFreePicture (myScreenGetXDisplay (cw->screen_info), cw->picture);
        cw->picture = None;
    }

    if (cw->alphaPict)
    {
        XRenderFreePicture (myScreenGetXDisplay (cw->screen_info), cw->alphaPict);
        cw->alphaPict = None;
    }

    if (cw->shadowPict)
    {
        XRenderFreePicture (myScreenGetXDisplay (cw->screen_info), cw->shadowPict);
        cw->shadowPict = None;
    }
    if ((delete) && (cw->damage != None))
    {
        XDamageDestroy (myScreenGetXDisplay (cw->screen_info), cw->damage);
        cw->damage = None;
    }

    if (cw->borderSize)
    {
        XFixesDestroyRegion (myScreenGetXDisplay (cw->screen_info), cw->borderSize);
        cw->borderSize = None;
    }

    if (cw->shadow)
    {
        XRenderFreePicture (myScreenGetXDisplay (cw->screen_info), cw->shadow);
        cw->shadow = None;
    }

    if (cw->borderClip)
    {
        XFixesDestroyRegion (myScreenGetXDisplay (cw->screen_info), cw->borderClip);
        cw->borderClip = None;
    }

    if (delete)
    {
        g_free (cw);
    }
}

static void
add_damage (ScreenInfo *screen_info, XserverRegion damage)
{
    TRACE ("entering add_damage");

    if (damage == None)
    {
        return;
    }

    if (screen_info->allDamage != None)
    {
        XFixesUnionRegion (myScreenGetXDisplay (screen_info),
                           screen_info->allDamage,
                           damage,
                           screen_info->allDamage);
        XFixesDestroyRegion (myScreenGetXDisplay (screen_info), damage);
    }
    else
    {
        screen_info->allDamage = damage;
    }

    add_repair (screen_info->display_info);
}

static void
repair_win (CWindow *cw)
{
    XserverRegion   parts;
    DisplayInfo *display_info = NULL;
    ScreenInfo *screen_info = NULL;

    g_return_if_fail (cw != NULL);

    TRACE ("entering repair_win");
    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

    if (!(cw->damage))
    {
        /* Input Only have no damage */
        return;
    }

    if (!(cw->damaged))
    {
        parts = win_extents (cw);
        XDamageSubtract (myScreenGetXDisplay (screen_info), cw->damage, None, None);
    }
    else
    {
        parts = XFixesCreateRegion (myScreenGetXDisplay (screen_info), NULL, 0);
        if (parts)
        {
            XDamageSubtract (myScreenGetXDisplay (screen_info), cw->damage, None, parts);
            XFixesTranslateRegion (myScreenGetXDisplay (screen_info), parts,
                                   cw->attr.x + cw->attr.border_width,
                                   cw->attr.y + cw->attr.border_width);
        }
    }

    if (parts)
    {
        cw->damaged = TRUE;
        add_damage (cw->screen_info, parts);
    }
}

static void
damage_win (CWindow *cw)
{
#ifdef HAVE_COMPOSITOR
    XserverRegion extents;

    g_return_if_fail (cw != NULL);
    TRACE ("entering damage_win");

    extents = win_extents (cw);
    add_damage (cw->screen_info, extents);
#endif /* HAVE_COMPOSITOR */
}

static void
determine_mode(CWindow *cw)
{
    ScreenInfo *screen_info = NULL;
    XRenderPictFormat *format = NULL;

    TRACE ("entering determine_mode");
    screen_info = cw->screen_info;
    if (cw->alphaPict)
    {
        XRenderFreePicture (myScreenGetXDisplay (screen_info), cw->alphaPict);
        cw->alphaPict = None;
    }
    if (cw->shadowPict)
    {
        XRenderFreePicture (myScreenGetXDisplay (screen_info), cw->shadowPict);
        cw->shadowPict = None;
    }

    if (cw->attr.class != InputOnly)
    {
        format = XRenderFindVisualFormat (myScreenGetXDisplay (screen_info), cw->attr.visual);
    }

    if ((format) && (format->type == PictTypeDirect) && (format->direct.alphaMask))
    {
        cw->mode = WINDOW_ARGB;
    }
    else if (cw->opacity != NET_WM_OPAQUE)
    {
        cw->mode = WINDOW_TRANS;
    }
    else
    {
        cw->mode = WINDOW_SOLID;
    }

    if (cw->extents)
    {
        XserverRegion damage;
        damage = XFixesCreateRegion (myScreenGetXDisplay (screen_info), NULL, 0);
        if (damage != None)
        {
            XFixesCopyRegion (myScreenGetXDisplay (screen_info), damage, cw->extents);
            add_damage (screen_info, damage);
        }
    }
}

static void
expose_area (ScreenInfo *screen_info, XRectangle *rects, gint nrects)
{
    XserverRegion region;

    g_return_if_fail (rects != NULL);
    g_return_if_fail (nrects > 0);

    TRACE ("entering expose_area");
    region = XFixesCreateRegion (myScreenGetXDisplay (screen_info), rects, nrects);
    if (region != None)
    {
        add_damage (screen_info, region);
    }
}

static void
set_win_opacity (CWindow *cw, guint opacity)
{
    TRACE ("entering set_win_opacity");
    g_return_if_fail (cw != NULL);

    cw->opacity = opacity;
    determine_mode(cw);
    if (cw->shadow)
    {
        XRenderFreePicture (myScreenGetXDisplay (cw->screen_info), cw->shadow);
        cw->shadow = None;
        if (cw->extents)
        {
            XFixesDestroyRegion (myScreenGetXDisplay (cw->screen_info), cw->extents);
        }
        cw->extents = win_extents (cw);
        add_repair (cw->screen_info->display_info);
    }
}

static void
map_win (CWindow *cw)
{
    ScreenInfo *screen_info = NULL;

    TRACE ("entering map_win");
    g_return_if_fail (cw != NULL);

    cw->viewable = TRUE;
    cw->damaged = FALSE;
    screen_info = cw->screen_info;
    add_repair (cw->screen_info->display_info);
}

static void
unmap_win (CWindow *cw)
{
    ScreenInfo *screen_info = NULL;

    TRACE ("entering unmap_win");
    g_return_if_fail (cw != NULL);

    cw->damaged = FALSE;
    screen_info = cw->screen_info;
    cw->viewable = FALSE;

    if (cw->extents != None)
    {
        add_damage (screen_info, cw->extents);
        /* cw->extents is freed by add_damage () */
        cw->extents = None;
    }
    free_win_data (cw, FALSE);
}

static void
add_win (DisplayInfo *display_info, Window id, Client *c, Window above, guint opacity)
{
    ScreenInfo *screen_info = NULL;
    CWindow *new;
    Status test;
    gboolean inserted = FALSE;

    TRACE ("entering add_win: 0x%lx", id);

    new = g_new0 (CWindow, 1);

    gdk_error_trap_push ();
    myDisplayGrabServer (display_info);
    test = XGetWindowAttributes (display_info->dpy, id, &new->attr);

    if (gdk_error_trap_pop () || !test)
    {
        g_free (new);
        myDisplayUngrabServer (display_info);
        TRACE ("An error occured getting window attributes, 0x%lx not added", id);
        return;
    }

    if (c)
    {
        screen_info = c->screen_info;
    }
    else
    {
        screen_info = myDisplayGetScreenFromRoot (display_info, new->attr.root);
    }

    if (!screen_info)
    {
        g_free (new);
        myDisplayUngrabServer (display_info);
        TRACE ("Couldn't get screen from window, 0x%lx not added", id);
        return;
    }

    if (!(screen_info->compositor_active))
    {
        g_free (new);
        myDisplayUngrabServer (display_info);
        TRACE ("Compositor not active on screen %i, 0x%lx not added", screen_info->screen, id);
        return;
    }

    if (id == screen_info->xroot)
    {
        g_free (new);
        myDisplayUngrabServer (display_info);
        TRACE ("Not adding root window, 0x%lx not added", id);
        return;
    }

    if (c == NULL)
    {
        /* We must be notified of property changes for transparency, even if the win is not managed */
        XSelectInput (display_info->dpy, id, PropertyChangeMask | StructureNotifyMask);
    }

    /* Same for shape events */
    if (display_info->shape)
    {
        XShapeSelectInput (display_info->dpy, id, ShapeNotifyMask);
    }

    if (new->attr.class == InputOnly)
    {
        new->damage = None;
    }
    else
    {
        new->damage = XDamageCreate (myScreenGetXDisplay (screen_info), id, XDamageReportNonEmpty);
    }

    new->c = c;
    new->screen_info = screen_info;
    new->id = id;
    new->damaged = FALSE;
    new->viewable = (new->attr.map_state == IsViewable);
#if HAVE_NAME_WINDOW_PIXMAP
    new->name_window_pixmap = None;
#endif
    new->picture = None;
    new->alphaPict = None;
    new->shadowPict = None;
    new->borderSize = None;
    new->extents = None;
    new->shadow = None;
    new->shadow_dx = 0;
    new->shadow_dy = 0;
    new->shadow_width = 0;
    new->shadow_height = 0;
    new->borderClip = None;
    new->opacity = opacity;
    getOpacity (display_info, id, &new->opacity);
    determine_mode (new);

    if (above != None)
    {
        GList *index;

        for (index = screen_info->cwindows; index; index = g_list_next (index))
        {
            CWindow *cw2 = (CWindow *) index->data;
            if (cw2->id == above)
            {
                break;
            }
        }

        if (index != NULL)
        {
            screen_info->cwindows =  g_list_insert_before (screen_info->cwindows, index, new);
            inserted = TRUE;
        }
    }

    if (!inserted)
    {
        screen_info->cwindows = g_list_prepend (screen_info->cwindows, new);
    }

    if (new->viewable)
    {
        map_win (new);
    }

    myDisplayUngrabServer (display_info);
}

void
restack_win (CWindow *cw, Window above)
{
    ScreenInfo *screen_info = NULL;
    Window previous_above = None;
    GList *sibling;
    GList *next;

    g_return_if_fail (cw != NULL);
    TRACE ("entering restack_win");

    screen_info = cw->screen_info;
    sibling = g_list_find (screen_info->cwindows, (gconstpointer) cw);
    next = g_list_next (sibling);

    if (next)
    {
        CWindow *ncw = (CWindow *) next;
        previous_above = ncw->id;
    }

    if (previous_above != above)
    {
        GList *index;

        screen_info->cwindows = g_list_delete_link (screen_info->cwindows, sibling);
        sibling = NULL;

        for (index = screen_info->cwindows; index; index = g_list_next (index))
        {
            CWindow *cw2 = (CWindow *) index->data;
            if (cw2->id == above)
            {
                break;
            }
        }

        if (index != NULL)
        {
            screen_info->cwindows =  g_list_insert_before (screen_info->cwindows, index, cw);
        }
        else
        {
            screen_info->cwindows =  g_list_prepend (screen_info->cwindows, cw);
        }
    }
}

void
resize_win (CWindow *cw, gint x, gint y, gint width, gint height, gint bw, gboolean shape_notify)
{
    XserverRegion extents;

    g_return_if_fail (cw != NULL);
    TRACE ("entering resize_win");

    extents = win_extents (cw);
    add_damage (cw->screen_info, extents);

    if (!(shape_notify) && (x == cw->attr.x) && (y == cw->attr.y) &&
        (width == cw->attr.width) && (height == cw->attr.height) &&
        (bw == cw->attr.border_width))
    {
        return;
    }
    
    TRACE ("resizing 0x%lx, (%i,%i) %ix%i", cw->id, x, y, width, height);
    if (cw->extents)
    {
        XFixesDestroyRegion (myScreenGetXDisplay (cw->screen_info), cw->extents);
        cw->extents = None;
    }

    if (cw->borderSize != None)
    {
        XFixesDestroyRegion (myScreenGetXDisplay (cw->screen_info), cw->borderSize);
        cw->borderSize = None;
    }

    if ((cw->attr.width != width) || (cw->attr.height != height))
    {

#if HAVE_NAME_WINDOW_PIXMAP
        if (cw->name_window_pixmap)
        {
            XFreePixmap (myScreenGetXDisplay (cw->screen_info), cw->name_window_pixmap);
            cw->name_window_pixmap = None;
        }
#endif
        if (cw->picture)
        {
            XRenderFreePicture (myScreenGetXDisplay (cw->screen_info), cw->picture);
            cw->picture = None;
        }

        if (cw->shadow)
        {
            XRenderFreePicture (myScreenGetXDisplay (cw->screen_info), cw->shadow);
            cw->shadow = None;
        }

        if (cw->borderSize != None)
        {
            XFixesDestroyRegion (myScreenGetXDisplay (cw->screen_info), cw->borderSize);
            cw->borderSize = None;
        }

        if (cw->extents)
        {
            XFixesDestroyRegion (myScreenGetXDisplay (cw->screen_info), cw->extents);
            cw->extents = None;
        }
    }

    cw->attr.x = x;
    cw->attr.y = y;
    cw->attr.width = width;
    cw->attr.height = height;
    cw->attr.border_width = bw;

    extents = win_extents (cw);
    add_damage (cw->screen_info, extents);
}

static void
destroy_win (ScreenInfo *screen_info, Window id, gboolean gone)
{
    CWindow *cw;

    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering destroy_win: 0x%lx %s", id, gone ? "gone" : "not gone" );

    cw = find_cwindow_in_screen (screen_info, id);
    if (cw)
    {
        if (!gone)
        {
            unmap_win (cw);
        }
        screen_info->cwindows = g_list_remove (screen_info->cwindows, (gconstpointer) cw);
        free_win_data (cw, TRUE);
        TRACE ("window 0x%lx removed", id);
    }
}

static void
compositorHandleDamage (DisplayInfo *display_info, XDamageNotifyEvent *ev)
{
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleDamage for 0x%lx", ev->drawable);

    cw = find_cwindow_in_display (display_info, ev->drawable);
    if (cw)
    {
        repair_win (cw);
        add_repair (display_info);
    }
}

static void
compositorHandlePropertyNotify (DisplayInfo *display_info, XPropertyEvent *ev)
{
    gint p;
    Atom backgroundProps[2];

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandlePropertyNotify for 0x%lx", ev->window);
 
    backgroundProps[0] = display_info->atoms[XROOTPMAP];
    backgroundProps[1] = display_info->atoms[XSETROOT];

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    for (p = 0; p < 2; p++)
    {
        if (ev->atom == backgroundProps[p])
        {
            ScreenInfo *screen_info = myDisplayGetScreenFromRoot (display_info, ev->window);
            if ((screen_info) && (screen_info->rootTile))
            {
                XClearArea (myScreenGetXDisplay (screen_info), screen_info->xroot, 0, 0, 0, 0, True);
                XRenderFreePicture (myScreenGetXDisplay (screen_info), screen_info->rootTile);
                screen_info->rootTile = None;
                add_repair (display_info);

                return;
            }
        }
    }

    /* check if Trans property was changed */
    if (ev->atom == display_info->atoms[NET_WM_OPACITY])
    {
        CWindow *cw = find_cwindow_in_display (display_info, ev->window);
        TRACE ("Opacity property changed for id 0x%lx", ev->window);

        if (cw)
        {
            TRACE ("Opacity changed for 0x%lx", cw->id);
            if (!getOpacity (display_info, cw->id, &cw->opacity))
            {
                /* The property was removed */
                cw->opacity = NET_WM_OPAQUE;
            }
            set_win_opacity (cw, cw->opacity);
        }
    }
    else
    {
        TRACE ("No compositor property changed for id 0x%lx", ev->window);
    }

}

static void
compositorHandleExpose (DisplayInfo *display_info, XExposeEvent *ev)
{
    ScreenInfo *screen_info = NULL;
    XRectangle rect[1];
    CWindow *cw;

    TRACE ("entering compositorHandleExpose for 0x%lx", ev->window);
    g_return_if_fail (display_info);
    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    cw = find_cwindow_in_display (display_info, ev->window);
    if (cw != NULL)
    {
        screen_info = cw->screen_info;
    }
    else
    {
        /* Get the screen structure from the root of the event */
        screen_info = myDisplayGetScreenFromRoot (display_info, ev->window);
        if (!screen_info)
        {
            return;
        }
    }

    rect[0].x = ev->x;
    rect[0].y = ev->y;
    rect[0].width = ev->width;
    rect[0].height = ev->height;

    expose_area (screen_info, rect, 1);
}

static void
compositorHandleConfigureNotify (DisplayInfo *display_info, XConfigureEvent *ev)
{
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleConfigureNotify for 0x%lx", ev->window);

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    cw = find_cwindow_in_display (display_info, ev->window);
    if (!cw)
    {
        ScreenInfo *screen_info = myDisplayGetScreenFromWindow (display_info, ev->window);

        if ((screen_info) && (ev->window == screen_info->xroot))
        {
            if (screen_info->rootBuffer)
            {
                XRenderFreePicture (display_info->dpy, screen_info->rootBuffer);
                screen_info->rootBuffer = None;
            }
            add_repair (display_info);
        }

        return;
    }

    restack_win (cw, ev->above);
    resize_win (cw, ev->x, ev->y, ev->width, ev->height, ev->border_width, FALSE);
}

static void
compositorHandleCirculateNotify (DisplayInfo *display_info, XCirculateEvent *ev)
{
    CWindow *cw;
    CWindow *top;
    GList *first;
    Window above = None;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleCirculateNotify for 0x%lx", ev->window);

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    cw = find_cwindow_in_display (display_info, ev->window);
    if (!cw)
    {
        return;
    }

    first = cw->screen_info->cwindows;
    top = (CWindow *) first;

    if ((ev->place == PlaceOnTop) && (top))
    {
        above = top->id;
    }
    else
    {
        above = None;
    }
    restack_win (cw, above);
    add_repair (display_info);
}

#endif /* HAVE_COMPOSITOR */

void
compositorWindowSetOpacity (DisplayInfo *display_info, Window id, guint opacity)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering compositorSetOpacity for 0x%lx", id);

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        set_win_opacity (cw, opacity);
    }
#endif
}

void
compositorMapWindow (DisplayInfo *display_info, Window id)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering compositorMapWindow for 0x%lx", id);

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        if (!(cw->viewable))
        {
            map_win (cw);
        }
    }
    else
    {
        add_win (display_info, id, NULL, None, NET_WM_OPAQUE);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorUnmapWindow (DisplayInfo *display_info, Window id)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering compositorUnmapWindow for 0x%lx", id);

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        unmap_win (cw);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorAddWindow (DisplayInfo *display_info, Window id, Client *c)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering compositorAddWindow for 0x%lx", id);

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        cw->c = c;
    }
    else
    {
        guint opacity;

        opacity = ((c != NULL) ? c->opacity : NET_WM_OPAQUE);
        add_win (display_info, id, c, None, opacity);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorRemoveWindow (DisplayInfo *display_info, Window id)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering compositorRemoveWindow: 0x%lx", id);

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        ScreenInfo *screen_info = cw->screen_info;
        destroy_win (screen_info, id, FALSE);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorHandleEvent (DisplayInfo *display_info, XEvent *ev)
{
#ifdef HAVE_COMPOSITOR
    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleEvent");

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    if (ev->type == ConfigureNotify)
    {
        compositorHandleConfigureNotify (display_info, (XConfigureEvent *) ev);
    }
    else if (ev->type == Expose)
    {
        compositorHandleExpose (display_info, (XExposeEvent *) ev);
    }
    else if (ev->type == CirculateNotify)
    {
        compositorHandleCirculateNotify (display_info, (XCirculateEvent *) ev);
    }
    else if (ev->type == PropertyNotify)
    {
        compositorHandlePropertyNotify (display_info, (XPropertyEvent *) ev);
    }
    else if (ev->type == (display_info->damage_event_base + XDamageNotify))
    {
        compositorHandleDamage (display_info, (XDamageNotifyEvent *) ev);
    }
#endif /* HAVE_COMPOSITOR */
}

gboolean
compositorCheckDamageEvent (DisplayInfo *display_info)
{
#ifdef HAVE_COMPOSITOR
    XEvent ev;

    g_return_val_if_fail (display_info != NULL, FALSE);
    TRACE ("entering compositorCheckDamageEvent");

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return FALSE;
    }

    if (XCheckTypedEvent (display_info->dpy, (display_info->damage_event_base + XDamageNotify), &ev))
    {
        compositorHandleDamage (display_info, (XDamageNotifyEvent *) &ev);
        return TRUE;
    }
#endif /* HAVE_COMPOSITOR */
    return FALSE;
}

void
compositorInitDisplay (DisplayInfo *display_info)
{
#ifdef HAVE_COMPOSITOR

    g_return_if_fail (display_info != NULL);
    TRACE ("entering compositorInitDisplay");

    if (!XCompositeQueryExtension (display_info->dpy,
                                &display_info->composite_event_base,
                                &display_info->composite_error_base))
    {
        g_warning ("%s: The display does not support the XComposite extension.", PACKAGE);
        display_info->have_composite = FALSE;
        display_info->composite_event_base = 0;
        display_info->composite_error_base = 0;
    }
    else
    {
        display_info->have_composite = TRUE;
    }
    if (!XDamageQueryExtension (display_info->dpy,
                            &display_info->damage_event_base,
                            &display_info->damage_error_base))
    {
        g_warning ("%s: The display does not support the XDamage extension.", PACKAGE);
        display_info->have_damage = FALSE;
        display_info->damage_event_base = 0;
        display_info->damage_error_base = 0;
    }
    else
    {
        display_info->have_damage = TRUE;
    }
    if (!XFixesQueryExtension (display_info->dpy,
                            &display_info->fixes_event_base,
                            &display_info->fixes_error_base))
    {
        g_warning ("%s: The display does not support the XFixes extension.", PACKAGE);
        display_info->have_fixes = FALSE;
        display_info->fixes_event_base = 0;
        display_info->fixes_error_base = 0;
    }
    else
    {
        display_info->have_fixes = TRUE;
    }

    display_info->compositor_idle_id = 0;
    display_info->compositor_timeout_id = 0;

    display_info->enable_compositor = ((display_info->have_composite)
                                    && (display_info->have_damage)
                                    && (display_info->have_fixes));
    if (!display_info->enable_compositor)
    {
        g_warning ("%s: Compositing manager disabled.", PACKAGE);
    }
#else /* HAVE_COMPOSITOR */
    display_info->enable_compositor = FALSE;
#endif /* HAVE_COMPOSITOR */
}

void
compositorRepairDisplay (DisplayInfo *display_info)
{
#ifdef HAVE_COMPOSITOR

    g_return_if_fail (display_info != NULL);
    TRACE ("entering compositorInitDisplay");

    if (!(display_info->enable_compositor))
    {
        return;
    }
    repair_display (display_info);
#endif /* HAVE_COMPOSITOR */
}

gboolean
compositorManageScreen (ScreenInfo *screen_info, gboolean manual_redirect)
{
#ifdef HAVE_COMPOSITOR
    DisplayInfo *display_info = NULL;
    XRenderPictureAttributes pa;
    XRenderPictFormat *visual_format;

    g_return_val_if_fail (screen_info != NULL, FALSE);
    TRACE ("entering compositorManageScreen");

    display_info = screen_info->display_info;
    screen_info->compositor_active = FALSE;

    if (!(display_info->enable_compositor))
    {
        return FALSE;
    }

    gdk_error_trap_push ();
    if (manual_redirect)
    {
        XCompositeRedirectSubwindows (display_info->dpy, screen_info->xroot, CompositeRedirectManual);
    }
    else
    {
        XCompositeRedirectSubwindows (display_info->dpy, screen_info->xroot, CompositeRedirectAutomatic);
    }
    XSync (display_info->dpy, FALSE);
    if (gdk_error_trap_pop ())
    {
        g_warning ("%s: Another compositing manager is running on screen %i", PACKAGE, screen_info->screen);
        return FALSE;
    }

    if (!manual_redirect)
    {
        /* That's enough for automatic compositing */
        return TRUE;
    }

    visual_format = XRenderFindVisualFormat (display_info->dpy,
                                             DefaultVisual (display_info->dpy,
                                                            screen_info->screen));
    if (!visual_format)
    {
        g_warning ("%s: Cannot find visual format on screen %i", PACKAGE, screen_info->screen);
        return FALSE;
    }

    pa.subwindow_mode = IncludeInferiors;
    screen_info->rootPicture = XRenderCreatePicture (display_info->dpy, screen_info->xroot,
                                                     visual_format, CPSubwindowMode, &pa);

    if (screen_info->rootPicture == None)
    {
        g_warning ("%s: Cannot create root picture on screen %i", PACKAGE, screen_info->screen);
        return FALSE;
    }

    screen_info->gsize = -1;
    screen_info->gaussianMap = make_gaussian_map(SHADOW_RADIUS);
    presum_gaussian (screen_info);
    screen_info->rootBuffer = None;
    screen_info->blackPicture = solid_picture (screen_info, TRUE, 1, 0, 0, 0);
    screen_info->rootTile = None;
    screen_info->allDamage = None;
    screen_info->cwindows = NULL;
    screen_info->compositor_active = TRUE;

    return TRUE;
#else
    return FALSE;
#endif /* HAVE_COMPOSITOR */
}

void
compositorUnmanageScreen (ScreenInfo *screen_info)
{
#ifdef HAVE_COMPOSITOR
    DisplayInfo *display_info = NULL;
    GList *index;
    gint i = 0;

    g_return_if_fail (screen_info != NULL);
    TRACE ("entering compositorUnmanageScreen");

    display_info = screen_info->display_info;
    if (!(display_info->enable_compositor))
    {
        return;
    }

    if (!(screen_info->compositor_active))
    {
        return;
    }

    for (index = screen_info->cwindows; index; index = g_list_next (index))
    {
        CWindow *cw2 = (CWindow *) index->data;
        free_win_data (cw2, TRUE);
        i++;
    }
    g_list_free (screen_info->cwindows);
    screen_info->cwindows = NULL;
    TRACE ("Compositor: removed %i window(s) remaining", i);

    if (screen_info->rootPicture)
    {
        XRenderFreePicture (display_info->dpy, screen_info->rootPicture);
        screen_info->rootPicture = None;
    }
    if (screen_info->blackPicture)
    {
        XRenderFreePicture (display_info->dpy, screen_info->blackPicture);
        screen_info->blackPicture = None;
    }

    if (screen_info->shadowTop)
    {
        g_free (screen_info->shadowTop);
        screen_info->shadowTop = NULL;
    }

    if (screen_info->shadowCorner)
    {
        g_free (screen_info->shadowCorner);
        screen_info->shadowCorner = NULL;
    }

    if (screen_info->gaussianMap)
    {
        g_free (screen_info->gaussianMap);
        screen_info->gaussianMap = NULL;
    }

    screen_info->gsize = -1;
#endif /* HAVE_COMPOSITOR */
}

void
compositorRepairScreen (ScreenInfo *screen_info)
{
#ifdef HAVE_COMPOSITOR
    DisplayInfo *display_info = NULL;

    g_return_if_fail (screen_info != NULL);
    TRACE ("entering compositorRepairScreen");

    display_info = screen_info->display_info;
    if (!(display_info->enable_compositor))
    {
        return;
    }

    if (!(screen_info->compositor_active))
    {
        return;
    }

    repair_screen (screen_info);
#endif /* HAVE_COMPOSITOR */
}

void
compositorDamageWindow (DisplayInfo *display_info, Window id)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering compositorDamageWindow: 0x%lx", id);

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        damage_win (cw);
        add_repair (display_info);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorUpdateWindow (DisplayInfo *display_info, Window id, gint new_width, gint new_height, gboolean shape_notify)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering compositorUpdateWindow: 0x%lx", id);

    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");
        return;
    }

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        TRACE ("Resizing Ox%ld to (%i, %i)", cw->id, new_width, new_height);
        resize_win (cw, cw->attr.x, cw->attr.y, new_width, new_height, cw->attr.border_width, shape_notify);
    }
#endif /* HAVE_COMPOSITOR */
}
