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


        xcompmgr - (c) 2003 Keith Packard
        metacity - (c) 2003, 2004 Red Hat, Inc.
        xfwm4    - (c) 2005-2011 Olivier Fourdan

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

#include <glib.h>
#include <math.h>
#include <string.h>
#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_LIBDRM
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <drm.h>
#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif
#endif /* HAVE_LIBDRM */

#include "display.h"
#include "screen.h"
#include "client.h"
#include "frame.h"
#include "hints.h"
#include "compositor.h"

#ifdef HAVE_COMPOSITOR

#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>

#ifndef SHADOW_RADIUS
#define SHADOW_RADIUS   12
#endif /* SHADOW_RADIUS */

#ifndef SHADOW_OFFSET_X
#define SHADOW_OFFSET_X (-3 * SHADOW_RADIUS / 2)
#endif /* SHADOW_OFFSET_X */

#ifndef SHADOW_OFFSET_Y
#define SHADOW_OFFSET_Y (-3 * SHADOW_RADIUS / 2)
#endif /* SHADOW_OFFSET_Y */

/* Some convenient macros */
#define WIN_HAS_CLIENT(cw)              (cw->c)
#define WIN_HAS_FRAME(cw)               (WIN_HAS_CLIENT(cw) && FLAG_TEST (cw->c->xfwm_flags, XFWM_FLAG_HAS_BORDER) && \
                                         !FLAG_TEST (cw->c->flags, CLIENT_FLAG_FULLSCREEN))
#define WIN_NO_SHADOW(cw)               ((cw->c) && \
                                           (FLAG_TEST (cw->c->flags, CLIENT_FLAG_FULLSCREEN | CLIENT_FLAG_BELOW) || \
                                            (cw->c->type & WINDOW_DESKTOP)))
#define WIN_IS_DOCK(cw)                 (WIN_HAS_CLIENT(cw) && (cw->c->type & WINDOW_DOCK))
#define WIN_IS_OVERRIDE(cw)             (cw->attr.override_redirect)
#define WIN_IS_ARGB(cw)                 (cw->argb)
#define WIN_IS_OPAQUE(cw)               ((cw->opacity == NET_WM_OPAQUE) && !WIN_IS_ARGB(cw))
#define WIN_IS_NATIVE_OPAQUE(cw)        ((cw->native_opacity) && !WIN_IS_ARGB(cw))
#define WIN_IS_FULLSCREEN(cw)           ((cw->attr.x <= 0) && \
                                           (cw->attr.y <= 0) && \
                                           (cw->attr.width + 2 * cw->attr.border_width >= cw->screen_info->width) && \
                                           (cw->attr.height + 2 * cw->attr.border_width >= cw->screen_info->height))
#define WIN_IS_SHAPED(cw)               ((WIN_HAS_CLIENT(cw) && FLAG_TEST (cw->c->flags, CLIENT_FLAG_HAS_SHAPE)) || \
                                           (WIN_IS_OVERRIDE(cw) && (cw->shaped)))
#define WIN_IS_VIEWABLE(cw)             (cw->viewable)
#define WIN_HAS_DAMAGE(cw)              (cw->damage)
#define WIN_IS_VISIBLE(cw)              (WIN_IS_VIEWABLE(cw) && WIN_HAS_DAMAGE(cw))
#define WIN_IS_DAMAGED(cw)              (cw->damaged)
#define WIN_IS_REDIRECTED(cw)           (cw->redirected)

/* Set TIMEOUT_REPAINT to 0 to disable timeout repaint */
#define TIMEOUT_REPAINT       10 /* msec */
#define TIMEOUT_REPAINT_MIN    1
#define TIMEOUT_REPAINT_MAX   20
#define TIMEOUT_DRI           10 /* seconds */

#define DRM_CARD0             "/dev/dri/card0"

typedef struct _CWindow CWindow;
struct _CWindow
{
    ScreenInfo *screen_info;
    Client *c;
    Window id;
    XWindowAttributes attr;

    gboolean damaged;
    gboolean viewable;
    gboolean shaped;
    gboolean redirected;
    gboolean fulloverlay;
    gboolean argb;
    gboolean skipped;
    gboolean native_opacity;
    gboolean opacity_locked;

    Damage damage;
#if HAVE_NAME_WINDOW_PIXMAP
    Pixmap name_window_pixmap;
#endif /* HAVE_NAME_WINDOW_PIXMAP */
    Picture picture;
    Picture shadow;
    Picture alphaPict;
    Picture shadowPict;
    Picture alphaBorderPict;

    XserverRegion borderSize;
    XserverRegion clientSize;
    XserverRegion borderClip;
    XserverRegion extents;

    gint shadow_dx;
    gint shadow_dy;
    gint shadow_width;
    gint shadow_height;

    guint opacity;
};

static CWindow*
find_cwindow_in_screen (ScreenInfo *screen_info, Window id)
{
    GList *list;

    g_return_val_if_fail (id != None, NULL);
    g_return_val_if_fail (screen_info != NULL, NULL);
    TRACE ("entering find_cwindow_in_screen");

    for (list = screen_info->cwindows; list; list = g_list_next (list))
    {
        CWindow *cw = (CWindow *) list->data;
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
    GSList *list;

    g_return_val_if_fail (id != None, NULL);
    g_return_val_if_fail (display_info != NULL, NULL);
    TRACE ("entering find_cwindow_in_display");

    for (list = display_info->screens; list; list = g_slist_next (list))
    {
        ScreenInfo *screen_info = (ScreenInfo *) list->data;
        CWindow *cw = find_cwindow_in_screen (screen_info, id);
        if (cw)
        {
            return (cw);
        }
    }
    return NULL;
}

static gboolean
is_shaped (DisplayInfo *display_info, Window id)
{
    int xws, yws, xbs, ybs;
    unsigned wws, hws, wbs, hbs;
    int boundingShaped, clipShaped;

    g_return_val_if_fail (display_info != NULL, FALSE);

    if (display_info->have_shape)
    {
        XShapeQueryExtents (display_info->dpy, id, &boundingShaped, &xws, &yws, &wws,
                            &hws, &clipShaped, &xbs, &ybs, &wbs, &hbs);
        return (boundingShaped != 0);
    }
    return FALSE;
}

static gboolean
is_fullscreen (CWindow *cw)
{
    GdkRectangle rect;

    /* First, check the good old way, the window is larger than the screen size */
    if (WIN_IS_FULLSCREEN(cw))
    {
        return TRUE;
    }

    /* Next check against the monitors which compose the entire screen */
    myScreenFindMonitorAtPoint (cw->screen_info,
                                cw->attr.x + (cw->attr.width + 2 * cw->attr.border_width) / 2,
                                cw->attr.y + (cw->attr.height + 2 * cw->attr.border_width) / 2,
                                &rect);

    return ((cw->attr.x == rect.x) &&
            (cw->attr.y == rect.y) &&
            (cw->attr.width + 2 * cw->attr.border_width == rect.width) &&
            (cw->attr.height + 2 * cw->attr.border_width == rect.height));
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
    gint size, center;
    gint x, y;
    gdouble t;
    gdouble g;

    TRACE ("entering make_gaussian_map");

    size = ((gint) ceil ((r * 3)) + 1) & ~1;
    center = size / 2;
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
    gdouble *g_data, *g_line;
    gdouble v;
    gint fx, fy;
    gint fx_start, fx_end;
    gint fy_start, fy_end;
    gint g_size, center;

    g_return_val_if_fail (map != NULL, (guchar) 255.0);
    TRACE ("entering sum_gaussian");

    g_line = map->data;
    g_size = map->size;
    center = g_size / 2;
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
    screen_info->gaussianSize = map->size;
    center = map->size / 2;

    if (screen_info->shadowCorner)
    {
        g_free (screen_info->shadowCorner);
    }
    if (screen_info->shadowTop)
    {
        g_free (screen_info->shadowTop);
    }

    screen_info->shadowCorner = (guchar *) (g_malloc ((screen_info->gaussianSize + 1)
                                                    * (screen_info->gaussianSize + 1) * 26));
    screen_info->shadowTop = (guchar *) (g_malloc ((screen_info->gaussianSize + 1) * 26));

    for (x = 0; x <= screen_info->gaussianSize; x++)
    {
        screen_info->shadowTop[25 * (screen_info->gaussianSize + 1) + x] =
            sum_gaussian (map, 1, x - center, center,
                          screen_info->gaussianSize * 2,
                          screen_info->gaussianSize * 2);

        for(opacity = 0; opacity < 25; opacity++)
        {
            screen_info->shadowTop[opacity * (screen_info->gaussianSize + 1) + x] =
                screen_info->shadowTop[25 * (screen_info->gaussianSize + 1) + x] * opacity / 25;
        }

        for(y = 0; y <= x; y++)
        {
            screen_info->shadowCorner[25 * (screen_info->gaussianSize + 1)
                                         * (screen_info->gaussianSize + 1)
                                     + y * (screen_info->gaussianSize + 1) + x]
                = sum_gaussian (map, 1, x - center,
                                        y - center,
                                        screen_info->gaussianSize * 2,
                                        screen_info->gaussianSize * 2);
            screen_info->shadowCorner[25 * (screen_info->gaussianSize + 1)
                                         * (screen_info->gaussianSize + 1)
                                     + x * (screen_info->gaussianSize + 1) + y]
                = screen_info->shadowCorner[25 * (screen_info->gaussianSize + 1)
                                               * (screen_info->gaussianSize + 1)
                                           + y * (screen_info->gaussianSize + 1) + x];

            for(opacity = 0; opacity < 25; opacity++)
            {
                screen_info->shadowCorner[opacity * (screen_info->gaussianSize + 1)
                                                  * (screen_info->gaussianSize + 1)
                                              + y * (screen_info->gaussianSize + 1) + x]
                    = screen_info->shadowCorner[opacity * (screen_info->gaussianSize + 1)
                                                        * (screen_info->gaussianSize + 1)
                                                    + x * (screen_info->gaussianSize + 1) + y]
                    = screen_info->shadowCorner[25 * (screen_info->gaussianSize + 1)
                                                   * (screen_info->gaussianSize + 1)
                                               + y * (screen_info->gaussianSize + 1) + x] * opacity / 25;
            }
        }
    }
}

static XImage *
make_shadow (ScreenInfo *screen_info, gdouble opacity, gint width, gint height)
{
    DisplayInfo *display_info;
    XImage *ximage;
    guchar *data;
    guchar d;
    gint gaussianSize;
    gint ylimit, xlimit;
    gint swidth;
    gint sheight;
    gint center;
    gint x, y;
    gint x_diff;
    gint opacity_int;
    gint x_swidth;
    gint y_swidth;

    g_return_val_if_fail (screen_info != NULL, NULL);
    TRACE ("entering make_shadow");

    display_info = screen_info->display_info;
    gaussianSize = screen_info->gaussianMap->size;
    swidth = width + gaussianSize - screen_info->params->shadow_delta_width - screen_info->params->shadow_delta_x;
    sheight = height + gaussianSize - screen_info->params->shadow_delta_height - screen_info->params->shadow_delta_y;
    center = gaussianSize / 2;
    opacity_int = (gint) (opacity * 25);

    if ((swidth < 1) || (sheight < 1))
    {
        return NULL;
    }

    data = g_malloc (swidth * sheight * sizeof (guchar));

    ximage = XCreateImage (display_info->dpy,
                        DefaultVisual(display_info->dpy, screen_info->screen),
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

    if (screen_info->gaussianSize > 0)
    {
        d = screen_info->shadowTop[opacity_int * (screen_info->gaussianSize + 1) + screen_info->gaussianSize];
    }
    else
    {
        d = sum_gaussian (screen_info->gaussianMap, opacity, center, center, width, height);
    }
    memset(data, d, sheight * swidth);

    /*
    * corners
    */

    ylimit = gaussianSize;
    if (ylimit > sheight / 2)
    {
        ylimit = (sheight + 1) / 2;
    }
    xlimit = gaussianSize;
    if (xlimit > swidth / 2)
    {
        xlimit = (swidth + 1) / 2;
    }
    for (y = 0; y < ylimit; y++)
    {
        for (x = 0; x < xlimit; x++)
        {
            if ((xlimit == screen_info->gaussianSize) && (ylimit == screen_info->gaussianSize))
            {
                d = screen_info->shadowCorner[opacity_int * (screen_info->gaussianSize + 1)
                                                        * (screen_info->gaussianSize + 1)
                                                    + y * (screen_info->gaussianSize + 1) + x];
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

    x_diff = swidth - (gaussianSize * 2);
    if (x_diff > 0 && ylimit > 0)
    {
        for (y = 0; y < ylimit; y++)
        {
            if (ylimit == screen_info->gaussianSize)
            {
                d = screen_info->shadowTop[opacity_int * (screen_info->gaussianSize + 1) + y];
            }
            else
            {
                d = sum_gaussian (screen_info->gaussianMap, opacity, center, y - center, width, height);
            }
            memset (&data[y * swidth + gaussianSize], d, x_diff);
            memset (&data[(sheight - y - 1) * swidth + gaussianSize], d, x_diff);
        }
    }

    /*
    * sides
    */

    for (x = 0; x < xlimit; x++)
    {
        if (xlimit == screen_info->gaussianSize)
        {
            d = screen_info->shadowTop[opacity_int * (screen_info->gaussianSize + 1) + x];
        }
        else
        {
            d = sum_gaussian (screen_info->gaussianMap, opacity, x - center, center, width, height);
        }
        x_swidth = swidth - x - 1;
        for (y = gaussianSize; y < sheight - gaussianSize; y++)
        {
            y_swidth = y * swidth;
            data[y_swidth + x] = d;
            data[y_swidth + x_swidth] = d;
        }
    }

    return ximage;
}

static Picture
shadow_picture (ScreenInfo *screen_info, gdouble opacity,
                gint width, gint height, gint *wp, gint *hp)
{
    DisplayInfo *display_info;
    XImage *shadowImage;
    Pixmap shadowPixmap;
    Picture shadowPicture;
    XRenderPictFormat *render_format;
    GC gc;

    g_return_val_if_fail (screen_info != NULL, None);
    TRACE ("entering shadow_picture");

    display_info = screen_info->display_info;
    render_format = XRenderFindStandardFormat (display_info->dpy, PictStandardA8);
    g_return_val_if_fail (render_format != NULL, None);

    shadowImage = make_shadow (screen_info, opacity, width, height);
    if (shadowImage == NULL)
    {
        *wp = *hp = 0;
        g_warning ("(shadowImage != NULL) failed");
        return (None);
    }

    shadowPixmap = XCreatePixmap (display_info->dpy, screen_info->output,
                                shadowImage->width, shadowImage->height, 8);
    if (shadowPixmap == None)
    {
        XDestroyImage (shadowImage);
        g_warning ("(shadowPixmap != None) failed");
        return None;
    }

    shadowPicture = XRenderCreatePicture (display_info->dpy,
                                        shadowPixmap, render_format, 0, NULL);
    if (shadowPicture == None)
    {
        XDestroyImage (shadowImage);
        XFreePixmap (display_info->dpy, shadowPixmap);
        g_warning ("(shadowPicture != None) failed");
        return None;
    }

    gc = XCreateGC (display_info->dpy, shadowPixmap, 0, NULL);
    XPutImage (display_info->dpy, shadowPixmap, gc, shadowImage, 0, 0, 0, 0,
            shadowImage->width, shadowImage->height);

    *wp = shadowImage->width;
    *hp = shadowImage->height;

    XFreeGC (display_info->dpy, gc);
    XDestroyImage (shadowImage);
    XFreePixmap (display_info->dpy, shadowPixmap);

    return shadowPicture;
}

static Picture
solid_picture (ScreenInfo *screen_info, gboolean argb,
               gdouble a, gdouble r, gdouble g, gdouble b)
{
    DisplayInfo *display_info;
    Pixmap pixmap;
    Picture picture;
    XRenderPictureAttributes pa;
    XRenderPictFormat *render_format;
    XRenderColor c;

    g_return_val_if_fail (screen_info, None);
    TRACE ("entering solid_picture");

    display_info = screen_info->display_info;
    render_format = XRenderFindStandardFormat (display_info->dpy,
                                    argb ? PictStandardARGB32 : PictStandardA8);
    g_return_val_if_fail (render_format != NULL , None);

    pixmap = XCreatePixmap (display_info->dpy,
                            screen_info->output, 1, 1, argb ? 32 : 8);
    g_return_val_if_fail (pixmap != None, None);

    pa.repeat = TRUE;
    picture = XRenderCreatePicture (display_info->dpy, pixmap,
                                    render_format, CPRepeat, &pa);
    if (picture == None)
    {
        XFreePixmap (display_info->dpy, pixmap);
        g_warning ("(picture != None) failed");
        return None;
    }

    c.alpha = a * 0xffff;
    c.red   = r * 0xffff;
    c.green = g * 0xffff;
    c.blue  = b * 0xffff;

    XRenderFillRectangle (display_info->dpy, PictOpSrc,
                          picture, &c, 0, 0, 1, 1);
    XFreePixmap (display_info->dpy, pixmap);

    return picture;
}

static XserverRegion
client_size (CWindow *cw)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    XserverRegion border;

    g_return_val_if_fail (cw != NULL, None);
    TRACE ("entering client_size");

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;
    border = None;

    if (WIN_HAS_FRAME(cw))
    {
        XRectangle  r;
        Client *c;

        c = cw->c;
        r.x = frameX (c) + frameLeft (c);
        r.y = frameY (c) + frameTop (c);
        r.width = frameWidth (c) - frameLeft (c) - frameRight (c);
        r.height = frameHeight (c) - frameTop (c) - frameBottom (c);
        border = XFixesCreateRegion (display_info->dpy, &r, 1);
    }

    return border;
}

static XserverRegion
border_size (CWindow *cw)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    XserverRegion border;

    g_return_val_if_fail (cw != NULL, None);
    TRACE ("entering border_size");

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;
    border = XFixesCreateRegionFromWindow (display_info->dpy,
                                           cw->id, WindowRegionBounding);
    g_return_val_if_fail (border != None, None);
    XFixesTranslateRegion (display_info->dpy, border,
                           cw->attr.x + cw->attr.border_width,
                           cw->attr.y + cw->attr.border_width);

    return border;
}

static void
free_win_data (CWindow *cw, gboolean delete)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

#if HAVE_NAME_WINDOW_PIXMAP
    if (cw->name_window_pixmap)
    {
        XFreePixmap (display_info->dpy, cw->name_window_pixmap);
        cw->name_window_pixmap = None;
    }
#endif

    if (cw->picture)
    {
        XRenderFreePicture (display_info->dpy, cw->picture);
        cw->picture = None;
    }

    if (cw->shadow)
    {
        XRenderFreePicture (display_info->dpy, cw->shadow);
        cw->shadow = None;
    }

    if (cw->alphaPict)
    {
        XRenderFreePicture (display_info->dpy, cw->alphaPict);
        cw->alphaPict = None;
    }

    if (cw->shadowPict)
    {
        XRenderFreePicture (display_info->dpy, cw->shadowPict);
        cw->shadowPict = None;
    }

    if (cw->alphaBorderPict)
    {
        XRenderFreePicture (display_info->dpy, cw->alphaBorderPict);
        cw->alphaBorderPict = None;
    }

    if (cw->borderSize)
    {
        XFixesDestroyRegion (display_info->dpy, cw->borderSize);
        cw->borderSize = None;
    }

    if (cw->clientSize)
    {
        XFixesDestroyRegion (display_info->dpy, cw->clientSize);
        cw->clientSize = None;
    }

    if (cw->borderClip)
    {
        XFixesDestroyRegion (display_info->dpy, cw->borderClip);
        cw->borderClip = None;
    }

    if (cw->extents)
    {
        XFixesDestroyRegion (display_info->dpy, cw->extents);
        cw->extents = None;
    }

    if (delete)
    {
        if (cw->damage)
        {
            XDamageDestroy (display_info->dpy, cw->damage);
            cw->damage = None;
        }

        g_free (cw);
    }
}

static Picture
root_tile (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;
    Display *dpy;
    Picture picture = None;
#ifdef MONITOR_ROOT_PIXMAP
    Pixmap pixmap;
    XRenderPictureAttributes pa;
    XRenderPictFormat *format;
    gint p;
    Atom backgroundProps[2];
#endif

    g_return_val_if_fail (screen_info != NULL, None);
    TRACE ("entering root_tile");

    display_info = screen_info->display_info;
    dpy = display_info->dpy;
#ifdef MONITOR_ROOT_PIXMAP
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
            pa.repeat = TRUE;
            format = XRenderFindVisualFormat (dpy, DefaultVisual (dpy, screen_info->screen));
            g_return_val_if_fail (format != NULL, None);
            picture = XRenderCreatePicture (dpy, pixmap, format, CPRepeat, &pa);
            break;
        }
    }
#endif
    if (picture == None)
    {
        XRenderColor c;

        /* Background default to just plain ugly grey */
        c.red   = 0x7f00;
        c.green = 0x7f00;
        c.blue  = 0x7f00;
        c.alpha = 0xffff;
        picture = XRenderCreateSolidFill (dpy, &c);
    }
    return picture;
}

static Picture
create_root_buffer (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;
    Picture pict;
    XRenderPictFormat *format;
    Pixmap rootPixmap;
    Visual *visual;
    gint depth;
    gint screen_width;
    gint screen_height;
    gint screen_number;

    g_return_val_if_fail (screen_info != NULL, None);
    TRACE ("entering create_root_buffer");

    display_info = screen_info->display_info;
    screen_width = screen_info->width;
    screen_height = screen_info->height;
    screen_number = screen_info->screen;
    visual = DefaultVisual (display_info->dpy, screen_number);
    depth = DefaultDepth (display_info->dpy, screen_number);

    format = XRenderFindVisualFormat (display_info->dpy, visual);
    g_return_val_if_fail (format != NULL, None);

    rootPixmap = XCreatePixmap (display_info->dpy,
                                screen_info->output,
                                screen_width, screen_height, depth);
    g_return_val_if_fail (rootPixmap != None, None);

    pict = XRenderCreatePicture (display_info->dpy,
                                 rootPixmap, format, 0, NULL);
    XFreePixmap (display_info->dpy, rootPixmap);

    return pict;
}

static void
paint_root (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;

    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (screen_info->rootBuffer != None);

    TRACE ("entering paint_root");
    if (screen_info->rootTile == None)
    {
        screen_info->rootTile = root_tile (screen_info);
        g_return_if_fail (screen_info->rootTile != None);
    }

    display_info = screen_info->display_info;
    XRenderComposite (display_info->dpy, PictOpSrc,
                      screen_info->rootTile, None, screen_info->rootBuffer,
                      0, 0, 0, 0, 0, 0,
                      screen_info->width,
                      screen_info->height);
}

static XserverRegion
win_extents (CWindow *cw)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    XRectangle r;

    g_return_val_if_fail (cw != NULL, None);
    TRACE ("entering win_extents: 0x%lx", cw->id);

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;
    r.x = cw->attr.x;
    r.y = cw->attr.y;
    r.width = cw->attr.width + cw->attr.border_width * 2;
    r.height = cw->attr.height + cw->attr.border_width * 2;

    /*
       We apply a shadow to the window if:
       - It's a window with a frame and the user asked for shadows under regular
         windows,
       - it's an override redirect window that is not shaped, not an argb and
         the user asked for shadows on so called "popup" windows.
     */

    if ((screen_info->params->show_popup_shadow &&
              WIN_IS_OVERRIDE(cw) &&
              !(WIN_IS_ARGB(cw) || WIN_IS_SHAPED(cw)) &&
              !WIN_IS_FULLSCREEN(cw)) ||
          (screen_info->params->show_frame_shadow &&
              !WIN_IS_OVERRIDE(cw) &&
              !WIN_NO_SHADOW(cw) &&
              !WIN_IS_DOCK(cw) &&
              (WIN_HAS_FRAME(cw) || !(WIN_IS_ARGB(cw) || WIN_IS_SHAPED(cw)))) ||
          (screen_info->params->show_dock_shadow &&
              WIN_IS_DOCK(cw) &&
              !WIN_NO_SHADOW(cw) &&
              !WIN_IS_OVERRIDE(cw) &&
              (!(WIN_IS_ARGB(cw) || WIN_IS_SHAPED(cw)))))
    {
        XRectangle sr;

        TRACE ("window 0x%lx has extents", cw->id);
        cw->shadow_dx = SHADOW_OFFSET_X + screen_info->params->shadow_delta_x;
        cw->shadow_dy = SHADOW_OFFSET_Y + screen_info->params->shadow_delta_y;

        if (!(cw->shadow))
        {
            double shadow_opacity;
            shadow_opacity = (double) screen_info->params->frame_opacity
                           * (screen_info->params->shadow_opacity / 100.0)
                           * cw->opacity
                           / (NET_WM_OPAQUE * 100.0);

            cw->shadow = shadow_picture (screen_info, shadow_opacity,
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
    else if (cw->shadow)
    {
        XRenderFreePicture (display_info->dpy, cw->shadow);
        cw->shadow = None;
    }
    return XFixesCreateRegion (display_info->dpy, &r, 1);
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
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    XRenderPictFormat *format;

    g_return_val_if_fail (cw != NULL, NULL);
    TRACE ("entering get_window_format");

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;
    format = XRenderFindVisualFormat (display_info->dpy, cw->attr.visual);
    if (!format)
    {
        format = XRenderFindVisualFormat (display_info->dpy,
                                          DefaultVisual (display_info->dpy,
                                                         screen_info->screen));
    }

    return format;
}

static Picture
get_window_picture (CWindow *cw)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    XRenderPictureAttributes pa;
    XRenderPictFormat *format;
    Drawable draw;

    g_return_val_if_fail (cw != NULL, None);
    TRACE ("entering get_window_picture");

    draw = cw->id;
    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

#if HAVE_NAME_WINDOW_PIXMAP
    if ((display_info->have_name_window_pixmap) && (cw->name_window_pixmap == None))
    {
        cw->name_window_pixmap = XCompositeNameWindowPixmap (display_info->dpy, cw->id);
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
        return XRenderCreatePicture (display_info->dpy, draw, format, CPSubwindowMode, &pa);
    }
    return None;
}

static void
unredirect_win (CWindow *cw)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (cw != NULL);
    TRACE ("entering unredirect_win");

    if (WIN_IS_REDIRECTED(cw))
    {
        screen_info = cw->screen_info;
        display_info = screen_info->display_info;

        free_win_data (cw, FALSE);
        cw->redirected = FALSE;

        XCompositeUnredirectWindow (display_info->dpy, cw->id, display_info->composite_mode);
        TRACE ("Window 0x%lx unredirected, wins_unredirected is %i", cw->id, screen_info->wins_unredirected);
    }
}

static void
paint_win (CWindow *cw, XserverRegion region, gboolean solid_part)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    gboolean paint_solid;

    g_return_if_fail (cw != NULL);
    TRACE ("entering paint_win: 0x%lx", cw->id);

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;
    paint_solid = ((solid_part) && WIN_IS_OPAQUE(cw));

    if (WIN_HAS_FRAME(cw) && (screen_info->params->frame_opacity < 100))
    {
        int frame_x, frame_y, frame_width, frame_height;
        int frame_top, frame_bottom, frame_left, frame_right;

        frame_x = frameX (cw->c);
        frame_y = frameY (cw->c);
        frame_width = frameWidth (cw->c);
        frame_height = frameHeight (cw->c);
        frame_top = frameTop (cw->c);
        frame_bottom = frameBottom (cw->c);
        frame_left = frameLeft (cw->c);
        frame_right = frameRight (cw->c);

        if (!solid_part)
        {
            if (!cw->alphaBorderPict)
            {
                double frame_opacity;
                frame_opacity = (double) cw->opacity
                                         * screen_info->params->frame_opacity
                                         / (NET_WM_OPAQUE * 100.0);

                cw->alphaBorderPict = solid_picture (screen_info,
                                                     FALSE,
                                                     frame_opacity,
                                                     0.0, /* red   */
                                                     0.0, /* green */
                                                     0.0  /* blue  */);
            }

            /* Top Border (title bar) */
            XRenderComposite (display_info->dpy, PictOpOver, cw->picture, cw->alphaBorderPict,
                              screen_info->rootBuffer,
                              0, 0,
                              0, 0,
                              frame_x, frame_y,
                              frame_width, frame_top);

            /* Bottom Border */
            XRenderComposite (display_info->dpy, PictOpOver, cw->picture, cw->alphaBorderPict,
                              screen_info->rootBuffer,
                              0, frame_height - frame_bottom,
                              0, 0,
                              frame_x, frame_y + frame_height - frame_bottom,
                              frame_width, frame_bottom);
            /* Left Border */
            XRenderComposite (display_info->dpy, PictOpOver, cw->picture, cw->alphaBorderPict,
                              screen_info->rootBuffer,
                              0, frame_top,
                              0, 0,
                              frame_x, frame_y + frame_top,
                              frame_left, frame_height - frame_top - frame_bottom);

            /* Right Border */
            XRenderComposite (display_info->dpy, PictOpOver, cw->picture, cw->alphaBorderPict,
                              screen_info->rootBuffer,
                              frame_width - frame_right, frame_top,
                              0, 0,
                              frame_x + frame_width - frame_right,
                              frame_y + frame_top, frame_right,
                              frame_height - frame_top - frame_bottom);
        }
        /* Client Window */
        if (paint_solid)
        {
            XRectangle  r;
            XserverRegion client_region;

            XFixesSetPictureClipRegion (display_info->dpy, screen_info->rootBuffer, 0, 0, region);
            XRenderComposite (display_info->dpy, PictOpSrc, cw->picture, None,
                              screen_info->rootBuffer,
                              frame_left, frame_top,
                              0, 0,
                              frame_x + frame_left, frame_y + frame_top,
                              frame_width - frame_left - frame_right, frame_height - frame_top - frame_bottom);

            r.x = frame_x + frame_left;
            r.y = frame_y + frame_top;
            r.width = frame_width - frame_left - frame_right;
            r.height = frame_height - frame_top - frame_bottom;
            client_region = XFixesCreateRegion (display_info->dpy, &r, 1);
            XFixesSubtractRegion (display_info->dpy, region, region, client_region);
            XFixesDestroyRegion (display_info->dpy, client_region);
        }
        else if (!solid_part)
        {
            XRenderComposite (display_info->dpy, PictOpOver, cw->picture, cw->alphaPict,
                              screen_info->rootBuffer,
                              frame_left, frame_top,
                              0, 0,
                              frame_x + frame_left, frame_y + frame_top,
                              frame_width - frame_left - frame_right, frame_height - frame_top - frame_bottom);
        }
    }
    else
    {
        gint x, y, w, h;

        get_paint_bounds (cw, &x, &y, &w, &h);
        if (paint_solid)
        {
            XFixesSetPictureClipRegion (display_info->dpy, screen_info->rootBuffer, 0, 0, region);
            XRenderComposite (display_info->dpy, PictOpSrc, cw->picture, None, screen_info->rootBuffer,
                              0, 0, 0, 0, x, y, w, h);
            XFixesSubtractRegion (display_info->dpy, region, region, cw->borderSize);
        }
        else if (!solid_part)
        {
            XRenderComposite (display_info->dpy, PictOpOver, cw->picture, cw->alphaPict, screen_info->rootBuffer,
                              0, 0, 0, 0, x, y, w, h);
        }
    }
}

#if HAVE_LIBDRM
#if TIMEOUT_REPAINT

static void
open_dri (ScreenInfo *screen_info)
{
    screen_info->dri_fd = open (DRM_CARD0, O_RDWR);
    if (screen_info->dri_fd == -1)
    {
        g_warning ("Error opening %s: %s", DRM_CARD0, g_strerror (errno));
    }
}

static void
close_dri (ScreenInfo *screen_info)
{
    if (screen_info->dri_fd != -1)
    {
        close (screen_info->dri_fd);
        screen_info->dri_fd = -1;
    }
}

static gboolean
dri_enabled (ScreenInfo *screen_info)
{
    return (screen_info->dri_fd != -1 && screen_info->params->sync_to_vblank);
}

static void
wait_vblank (ScreenInfo *screen_info)
{
    int retval;
    drm_wait_vblank_t vblank;

    if (screen_info->dri_time > g_get_monotonic_time())
    {
        return;
    }

    vblank.request.sequence = 1;
    vblank.request.type = _DRM_VBLANK_RELATIVE;
    if (screen_info->dri_secondary)
    {
        vblank.request.type |= _DRM_VBLANK_SECONDARY;
    }

    do
    {
       retval = ioctl (screen_info->dri_fd, DRM_IOCTL_WAIT_VBLANK, &vblank);
       vblank.request.type &= ~_DRM_VBLANK_RELATIVE;
    }
    while (retval == -1 && errno == EINTR);

    screen_info->vblank_time = g_get_monotonic_time ();

    if (retval == -1)
    {
        if (screen_info->dri_success)
        {
            screen_info->dri_success = FALSE;
            g_warning ("Error waiting on vblank with DRI: %s", g_strerror (errno));
        }

        /* if getting the vblank fails, try to get it from the other output */
        screen_info->dri_secondary = !screen_info->dri_secondary;

        /* the output that we tried to get the vblank from might be disabled,
           if that's the case, the device needs to be reopened, or it will continue to fail */
        close_dri (screen_info);
        open_dri (screen_info);

        /* retry in 10 seconds */
        screen_info->dri_time = g_get_monotonic_time() + TIMEOUT_DRI * 1000000;
    }
    else if (!screen_info->dri_success)
    {
        g_message ("Using vertical blank of %s DRI output",
                   screen_info->dri_secondary ? "secondary" : "primary");

        screen_info->dri_success = TRUE;
    }
}

#endif /* TIMEOUT_REPAINT */

#endif /* HAVE_LIBDRM */

#ifdef HAVE_RANDR
static void
get_refresh_rate (ScreenInfo* screen_info)
{
    gint refresh_rate;
    XRRScreenConfiguration* randr_info;

    randr_info = XRRGetScreenInfo (screen_info->display_info->dpy, screen_info->xroot);
    refresh_rate = XRRConfigCurrentRate (randr_info);
    XRRFreeScreenConfigInfo (randr_info);

    if (refresh_rate != screen_info->refresh_rate)
    {
        DBG ("Detected refreshrate: %i hertz", refresh_rate);
        screen_info->refresh_rate = refresh_rate;
    }
}
#endif /* HAVE_RANDR */

static void
paint_all (ScreenInfo *screen_info, XserverRegion region)
{
    DisplayInfo *display_info;
    XserverRegion paint_region;
    Display *dpy;
    GList *list;
    gint screen_width;
    gint screen_height;
    CWindow *cw;

#ifdef HAVE_LIBDRM
#if TIMEOUT_REPAINT
    gboolean use_dri;
#endif /* TIMEOUT_REPAINT */
#endif /* HAVE_LIBDRM */

    TRACE ("entering paint_all");
    g_return_if_fail (screen_info);

    display_info = screen_info->display_info;
    dpy = display_info->dpy;
    screen_width = screen_info->width;
    screen_height = screen_info->height;

    /* Create root buffer if not done yet */
    if (screen_info->rootBuffer == None)
    {
        screen_info->rootBuffer = create_root_buffer (screen_info);
        g_return_if_fail (screen_info->rootBuffer != None);

        memset(screen_info->transform.matrix, 0, 9);
        screen_info->transform.matrix[0][0] = 1<<16;
        screen_info->transform.matrix[1][1] = 1<<16;
        screen_info->transform.matrix[2][2] = 1<<16;
        screen_info->zoomed = 0;
    }

    /* Copy the original given region */
    paint_region = XFixesCreateRegion (dpy, NULL, 0);
    XFixesCopyRegion (dpy, paint_region, region);

    /*
     * Painting from top to bottom, reducing the clipping area at each iteration.
     * Only the opaque windows are painted 1st.
     */
    for (list = screen_info->cwindows; list; list = g_list_next (list))
    {
        cw = (CWindow *) list->data;
        TRACE ("painting forward 0x%lx", cw->id);
        if (!WIN_IS_VISIBLE(cw) || !WIN_IS_DAMAGED(cw))
        {
            TRACE ("skipped, not damaged or not viewable 0x%lx", cw->id);
            cw->skipped = TRUE;
            continue;
        }

        if (!WIN_IS_REDIRECTED(cw))
        {
            TRACE ("skipped, not redirected 0x%lx", cw->id);
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
        if (cw->clientSize == None)
        {
            cw->clientSize = client_size (cw);
        }
        if (cw->picture == None)
        {
            cw->picture = get_window_picture (cw);
        }
        if (WIN_IS_OPAQUE(cw))
        {
            paint_win (cw, paint_region, TRUE);
        }
        if (cw->borderClip == None)
        {
            cw->borderClip = XFixesCreateRegion (dpy, NULL, 0);
            XFixesCopyRegion (dpy, cw->borderClip, paint_region);
        }

        cw->skipped = FALSE;
    }

    /*
     * region has changed because of the XFixesSubtractRegion (),
     * reapply clipping for the last iteration.
     */
    XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, paint_region);
    paint_root (screen_info);

    /*
     * Painting from bottom to top, translucent windows and shadows are painted now...
     */
    for (list = g_list_last(screen_info->cwindows); list; list = g_list_previous (list))
    {
        XserverRegion shadowClip;

        cw = (CWindow *) list->data;
        shadowClip = None;
        TRACE ("painting backward 0x%lx", cw->id);

        if (cw->skipped)
        {
            TRACE ("skipped 0x%lx", cw->id);
            continue;
        }

        if (cw->shadow)
        {
            shadowClip = XFixesCreateRegion(dpy, NULL, 0);
            XFixesSubtractRegion (dpy, shadowClip, cw->borderClip, cw->borderSize);

            XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, shadowClip);
            XRenderComposite (dpy, PictOpOver, screen_info->blackPicture, cw->shadow,
                              screen_info->rootBuffer, 0, 0, 0, 0,
                              cw->attr.x + cw->shadow_dx,
                              cw->attr.y + cw->shadow_dy,
                              cw->shadow_width, cw->shadow_height);
        }

        if (cw->picture)
        {
            if ((cw->opacity != NET_WM_OPAQUE) && !(cw->alphaPict))
            {
                cw->alphaPict = solid_picture (screen_info, FALSE,
                                               (double) cw->opacity / NET_WM_OPAQUE,
                                               0.0, /* red   */
                                               0.0, /* green */
                                               0.0  /* blue  */);
            }
            XFixesIntersectRegion (dpy, cw->borderClip, cw->borderClip, cw->borderSize);
            XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, cw->borderClip);
            paint_win (cw, paint_region, FALSE);
        }

        if (shadowClip)
        {
            XFixesDestroyRegion (dpy, shadowClip);
        }

        if (cw->borderClip)
        {
            XFixesDestroyRegion (dpy, cw->borderClip);
            cw->borderClip = None;
        }
    }

    TRACE ("Copying data back to screen");
    if(screen_info->zoomed)
    {
        /* Fixme: copy back whole screen if zoomed 
           It would be better to scale the clipping region if possible. */
        XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, None);
    }
    else
    {
        /* Set clipping back to the given region */
        XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, region);
    }
#ifdef HAVE_LIBDRM
#if TIMEOUT_REPAINT
    use_dri = dri_enabled (screen_info);
    
    if (use_dri)
    {
        /* sync all previous rendering commands, tell xlib to render the pixmap
         * onto the root window, wait for the vblank, then flush, this minimizes
         * tearing*/
        XFlush (dpy);
    }
#endif /* TIMEOUT_REPAINT */
#endif /* HAVE_LIBDRM */

    XRenderComposite (dpy, PictOpSrc, screen_info->rootBuffer, None, screen_info->rootPicture,
                      0, 0, 0, 0, 0, 0, screen_width, screen_height);

#ifdef HAVE_LIBDRM
#if TIMEOUT_REPAINT
    if (use_dri)
    {
        wait_vblank (screen_info);
        XFlush (dpy);
    }
#endif /* TIMEOUT_REPAINT */
#endif /* HAVE_LIBDRM */

    XFixesDestroyRegion (dpy, paint_region);
}

#if TIMEOUT_REPAINT
static void
remove_timeouts (ScreenInfo *screen_info)
{
    if (screen_info->compositor_timeout_id != 0)
    {
        g_source_remove (screen_info->compositor_timeout_id);
        screen_info->compositor_timeout_id = 0;
    }
}
#endif /* TIMEOUT_REPAINT */

static void
repair_screen (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;

    g_return_if_fail (screen_info);
    TRACE ("entering repair_screen");

    if (!screen_info->compositor_active)
    {
        return;
    }

#if TIMEOUT_REPAINT
    remove_timeouts (screen_info);
#endif /* TIMEOUT_REPAINT */

    display_info = screen_info->display_info;
    if (screen_info->allDamage != None)
    {
        paint_all (screen_info, screen_info->allDamage);
        XFixesDestroyRegion (display_info->dpy, screen_info->allDamage);
        screen_info->allDamage = None;
    }
}

#if TIMEOUT_REPAINT
static gboolean
compositor_timeout_cb (gpointer data)
{
    ScreenInfo *screen_info;

    screen_info = (ScreenInfo *) data;
    screen_info->compositor_timeout_id = 0;
    repair_screen (screen_info);

    return FALSE;
}
#endif /* TIMEOUT_REPAINT */

static void
add_repair (ScreenInfo *screen_info)
{
#if TIMEOUT_REPAINT
#ifdef HAVE_LIBDRM
    gint64 interval;
#endif /* HAVE_LIBDRM */

    if (screen_info->compositor_timeout_id != 0)
    {
        return;
    }

#ifdef HAVE_LIBDRM
    if (dri_enabled (screen_info))
    {
        /* schedule the next render to be half a refresh period after the last vertical blank,
           but at least 1 ms in the future so that all queued events can be processed,
           and to reduce latency if we didn't render for a while */
#ifdef HAVE_RANDR
        if (screen_info->refresh_rate > 0)
        {
            interval = (screen_info->vblank_time + 500000 / screen_info->refresh_rate -
                        g_get_monotonic_time ()) / 1000;
        }
        else
#endif /* HAVE_RANDR */
        {
            interval = TIMEOUT_REPAINT - ((g_get_monotonic_time () - screen_info->vblank_time) / 1000);
        }

        if (interval > TIMEOUT_REPAINT_MAX)
        {
            interval = TIMEOUT_REPAINT_MAX;
        }
        else if (interval < TIMEOUT_REPAINT_MIN)
        {
            interval = TIMEOUT_REPAINT_MIN;
        }
    }
    else
    {
        interval = TIMEOUT_REPAINT;
    }
#endif /* HAVE_LIBDRM */

    screen_info->compositor_timeout_id =
#ifdef HAVE_LIBDRM
        g_timeout_add (interval,
                       compositor_timeout_cb, screen_info);
#else
        g_timeout_add (TIMEOUT_REPAINT,
                       compositor_timeout_cb, screen_info);
#endif /*HAVE_LIBDRM */

#endif /* TIMEOUT_REPAINT */
}

#if TIMEOUT_REPAINT == 0
static void
repair_display (DisplayInfo *display_info)
{
    GSList *screens;

    g_return_if_fail (display_info);
    TRACE ("entering repair_display");

    for (screens = display_info->screens; screens; screens = g_slist_next (screens))
    {
        repair_screen ((ScreenInfo *) screens->data);
    }
}
#endif

static void
add_damage (ScreenInfo *screen_info, XserverRegion damage)
{
    DisplayInfo *display_info;

    TRACE ("entering add_damage");

    if (damage == None)
    {
        return;
    }

    display_info = screen_info->display_info;
    if (screen_info->allDamage != None)
    {
        XFixesUnionRegion (display_info->dpy,
                           screen_info->allDamage,
                           screen_info->allDamage,
                           damage);
        XFixesDestroyRegion (display_info->dpy, damage);
    }
    else
    {
        screen_info->allDamage = damage;
    }

    /* The per-screen allDamage region is freed by repair_screen () */
    add_repair (screen_info);
}

static void
fix_region (CWindow *cw, XserverRegion region)
{
    GList *list;
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

    /* Exclude opaque windows in front of the given area */
    for (list = screen_info->cwindows; list; list = g_list_next (list))
    {
        CWindow *cw2;

        cw2 = (CWindow *) list->data;
        if (cw2 == cw)
        {
            break;
        }
        else if (WIN_IS_OPAQUE(cw2) && WIN_IS_VISIBLE(cw2))
        {
            /* Make sure the window's areas are up-to-date... */
            if (cw2->borderSize == None)
            {
                cw2->borderSize = border_size (cw2);
            }
            if (cw2->clientSize == None)
            {
                cw2->clientSize = client_size (cw2);
            }
            /* ...before subtracting them from the damaged zone. */
            if ((cw2->clientSize) && (screen_info->params->frame_opacity < 100))
            {
                XFixesSubtractRegion (display_info->dpy, region,
                                     region, cw2->clientSize);
            }
            else if (cw2->borderSize)
            {
                XFixesSubtractRegion (display_info->dpy, region,
                                     region, cw2->borderSize);
            }
        }
    }
}

static void
repair_win (CWindow *cw, XRectangle *r)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    XserverRegion parts;

    g_return_if_fail (cw != NULL);

    TRACE ("entering repair_win");
    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

    if (!(cw->damage))
    {
        /* Input Only have no damage */
        return;
    }

    if (cw->damaged)
    {
        parts = XFixesCreateRegion (display_info->dpy, NULL, 0);
        /* Copy the damage region to parts, subtracting it from the window's damage */
        XDamageSubtract (display_info->dpy, cw->damage, None, parts);
        XFixesTranslateRegion (display_info->dpy, parts,
                               cw->attr.x + cw->attr.border_width,
                               cw->attr.y + cw->attr.border_width);
    }
    else
    {
        parts = win_extents (cw);
        /* Subtract all damage from the window's damage */
        XDamageSubtract (display_info->dpy, cw->damage, None, None);
    }

    if (parts)
    {
        fix_region (cw, parts);
        /* parts region will be destroyed by add_damage () */
        add_damage (cw->screen_info, parts);
        cw->damaged = TRUE;
    }
}

static void
damage_screen (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;
    XserverRegion region;
    XRectangle  r;

    display_info = screen_info->display_info;
    r.x = 0;
    r.y = 0;
    r.width = screen_info->width;
    r.height = screen_info->height;
    region = XFixesCreateRegion (display_info->dpy, &r, 1);
    /* region will be freed by add_damage () */
    add_damage (screen_info, region);
}

static void
damage_win (CWindow *cw)
{
    XserverRegion extents;

    g_return_if_fail (cw != NULL);
    TRACE ("entering damage_win");

    extents = win_extents (cw);
    fix_region (cw, extents);
    /* extents region will be freed by add_damage () */
    add_damage (cw->screen_info, extents);
}

static void
update_extents (CWindow *cw)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;

    g_return_if_fail (cw != NULL);
    TRACE ("entering update_extents");

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

    if (WIN_IS_VISIBLE(cw))
    {
        damage_win (cw);
    }

    if (cw->extents)
    {
        XFixesDestroyRegion (display_info->dpy, cw->extents);
        cw->extents = None;
    }
}

static void
determine_mode (CWindow *cw)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    XRenderPictFormat *format;

    TRACE ("entering determine_mode");
    screen_info = cw->screen_info;
    display_info = screen_info->display_info;
    format = NULL;

    if (cw->alphaPict)
    {
        XRenderFreePicture (display_info->dpy, cw->alphaPict);
        cw->alphaPict = None;
    }
    if (cw->shadowPict)
    {
        XRenderFreePicture (display_info->dpy, cw->shadowPict);
        cw->shadowPict = None;
    }

    if (cw->alphaBorderPict)
    {
        XRenderFreePicture (display_info->dpy, cw->alphaBorderPict);
        cw->alphaBorderPict = None;
    }

    format = XRenderFindVisualFormat (display_info->dpy, cw->attr.visual);
    cw->argb = ((format) && (format->type == PictTypeDirect) && (format->direct.alphaMask));

    if (cw->extents)
    {
        XserverRegion damage;

        damage = XFixesCreateRegion (display_info->dpy, NULL, 0);
        XFixesCopyRegion (display_info->dpy, damage, cw->extents);
        fix_region (cw, damage);
        /* damage region will be destroyed by add_damage () */
        add_damage (screen_info, damage);
    }
}

static void
expose_area (ScreenInfo *screen_info, XRectangle *rects, gint nrects)
{
    DisplayInfo *display_info;
    XserverRegion region;

    g_return_if_fail (rects != NULL);
    g_return_if_fail (nrects > 0);
    TRACE ("entering expose_area");

    display_info = screen_info->display_info;
    region = XFixesCreateRegion (display_info->dpy, rects, nrects);
    /* region will be destroyed by add_damage () */
    add_damage (screen_info, region);
}

static void
set_win_opacity (CWindow *cw, guint opacity)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;

    g_return_if_fail (cw != NULL);
    TRACE ("entering set_win_opacity");

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

    cw->opacity = opacity;
    determine_mode(cw);
    if (cw->shadow)
    {
        XRenderFreePicture (display_info->dpy, cw->shadow);
        cw->shadow = None;
        if (cw->extents)
        {
            XFixesDestroyRegion (display_info->dpy, cw->extents);
        }
        cw->extents = win_extents (cw);
        add_repair (screen_info);
    }
}

static void
map_win (CWindow *cw)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (cw != NULL);
    TRACE ("entering map_win 0x%lx", cw->id);

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

    if (!WIN_IS_REDIRECTED(cw))
    {
        cw->fulloverlay = is_fullscreen(cw);
        if (cw->fulloverlay)
        {
            /*
             * To be safe, we only count the fullscreen un-redirected windows.
             * We do not want a smaller override redirect such as a tooltip
             * for example to prevent the overlay to be remapped and leave
             * a black screen until the tooltip is unmapped...
             */
            screen_info->wins_unredirected++;
            TRACE ("Mapping fullscreen window 0x%lx, wins_unredirected increased to %i", cw->id, screen_info->wins_unredirected);
        }
        TRACE ("Mapping unredirected window 0x%lx, wins_unredirected is now %i", cw->id, screen_info->wins_unredirected);
#if HAVE_OVERLAYS
        if ((screen_info->wins_unredirected == 1) && (display_info->have_overlays))
        {
            TRACE ("Unmapping overlay window");
            XUnmapWindow (myScreenGetXDisplay (screen_info), screen_info->overlay);
        }
#endif /* HAVE_OVERLAYS */
        return;
    }

    cw->viewable = TRUE;
    cw->damaged = FALSE;

    /* Check for new windows to un-redirect. */
    if (WIN_HAS_DAMAGE(cw) && WIN_IS_OVERRIDE(cw) &&
        WIN_IS_NATIVE_OPAQUE(cw) && WIN_IS_REDIRECTED(cw) && !WIN_IS_SHAPED(cw)
        && ((screen_info->wins_unredirected > 0) || is_fullscreen(cw)))
    {
        /* Make those opaque, we don't want them to be transparent */
        cw->opacity = NET_WM_OPAQUE;
        if (screen_info->params->unredirect_overlays)
        {
            TRACE ("Unredirecting toplevel window 0x%lx", cw->id);
            unredirect_win (cw);
        }
    }
}

static void
unmap_win (CWindow *cw)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;

    g_return_if_fail (cw != NULL);
    TRACE ("entering unmap_win 0x%lx", cw->id);

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

    if (!WIN_IS_REDIRECTED(cw) && (screen_info->wins_unredirected > 0))
    {
        if (cw->fulloverlay)
        {
            screen_info->wins_unredirected--;
            TRACE ("Unmapping fullscreen window 0x%lx, wins_unredirected decreased to %i", cw->id, screen_info->wins_unredirected);
        }
        TRACE ("Unmapped window 0x%lx, wins_unredirected is now %i", cw->id, screen_info->wins_unredirected);
        if (!screen_info->wins_unredirected)
        {
            /* Restore the overlay if that was the last unredirected window */
#if HAVE_OVERLAYS
            if (display_info->have_overlays)
            {
                TRACE ("Remapping overlay window");
                XMapWindow (myScreenGetXDisplay (screen_info), screen_info->overlay);
            }
#endif /* HAVE_OVERLAYS */
            damage_screen (screen_info);
       }
    }
    else if (WIN_IS_VISIBLE(cw))
    {
        damage_win (cw);
    }

    cw->viewable = FALSE;
    cw->damaged = FALSE;
    cw->redirected = TRUE;
    cw->fulloverlay = FALSE;

    free_win_data (cw, FALSE);
}

static void
init_opacity (CWindow *cw)
{
    ScreenInfo *screen_info;
    DisplayInfo *display_info;
    Client *c;

    TRACE ("set_opacity");
    g_return_if_fail (cw != NULL);

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;
    c = cw->c;

    cw->native_opacity = FALSE;
    if (c)
    {
        cw->opacity_locked = FLAG_TEST (c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED);
        cw->opacity = c->opacity_applied;
        cw->native_opacity = WIN_IS_OPAQUE(cw);
    }
    else if (getOpacity (display_info, cw->id, &cw->opacity))
    {
        cw->native_opacity = WIN_IS_OPAQUE(cw);
        cw->opacity_locked = getOpacityLock (display_info, cw->id);
    }
    else
    {
        cw->opacity = (double) (screen_info->params->popup_opacity / 100.0) * NET_WM_OPAQUE;
        cw->native_opacity = TRUE;
        cw->opacity_locked = getOpacityLock (display_info, cw->id);
    }
}

static void
add_win (DisplayInfo *display_info, Window id, Client *c)
{
    ScreenInfo *screen_info;
    CWindow *new;

    TRACE ("entering add_win: 0x%lx", id);

    new = g_new0 (CWindow, 1);

    myDisplayGrabServer (display_info);
    if (!XGetWindowAttributes (display_info->dpy, id, &new->attr))
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

    if (c == NULL)
    {
        /* We must be notified of property changes for transparency, even if the win is not managed */
        XSelectInput (display_info->dpy, id, new->attr.your_event_mask | PropertyChangeMask | StructureNotifyMask);
    }

    /* Listen for XShape events if applicable */
    if (display_info->have_shape)
    {
        XShapeSelectInput (display_info->dpy, id, ShapeNotifyMask);
    }

    new->c = c;
    new->screen_info = screen_info;
    new->id = id;
    new->damaged = FALSE;
    new->redirected = TRUE;
    new->fulloverlay = FALSE;
    new->shaped = is_shaped (display_info, id);
    new->viewable = (new->attr.map_state == IsViewable);

    if ((new->attr.class != InputOnly)
#if HAVE_OVERLAYS
         && ((!display_info->have_overlays) || (id != screen_info->overlay))
#endif
         && (id != screen_info->output))
    {
        new->damage = XDamageCreate (display_info->dpy, id, XDamageReportNonEmpty);
    }
    else
    {
        new->damage = None;
    }
#if HAVE_NAME_WINDOW_PIXMAP
    new->name_window_pixmap = None;
#endif
    new->picture = None;
    new->alphaPict = None;
    new->alphaBorderPict = None;
    new->shadowPict = None;
    new->borderSize = None;
    new->clientSize = None;
    new->extents = None;
    new->shadow = None;
    new->shadow_dx = 0;
    new->shadow_dy = 0;
    new->shadow_width = 0;
    new->shadow_height = 0;
    new->borderClip = None;

    init_opacity (new);
    determine_mode (new);

    /* Insert window at top of stack */
    screen_info->cwindows = g_list_prepend (screen_info->cwindows, new);

    if (WIN_IS_VISIBLE(new))
    {
        map_win (new);
    }

    TRACE ("window 0x%lx added", id);

    myDisplayUngrabServer (display_info);
}

static void
restack_win (CWindow *cw, Window above)
{
    ScreenInfo *screen_info;
    Window previous_above;
    GList *sibling;
    GList *next;

    g_return_if_fail (cw != NULL);
    TRACE ("entering restack_win, 0x%lx above 0x%lx", cw->id, above);

    screen_info = cw->screen_info;
    sibling = g_list_find (screen_info->cwindows, (gconstpointer) cw);
    next = g_list_next (sibling);
    previous_above = None;

    if (next)
    {
        CWindow *ncw = (CWindow *) next;
        previous_above = ncw->id;
    }

    /* If above is set to None, the window whose state was changed is on
     * the bottom of the stack with respect to sibling.
     */
    if (above == None)
    {
        /* Insert at bottom of window stack */
        screen_info->cwindows = g_list_delete_link (screen_info->cwindows, sibling);
        screen_info->cwindows = g_list_append (screen_info->cwindows, cw);
    }
    else if (previous_above != above)
    {
        GList *list;

        for (list = screen_info->cwindows; list; list = g_list_next (list))
        {
            CWindow *cw2 = (CWindow *) list->data;
            if (cw2->id == above)
            {
                break;
            }
        }

        if (list != NULL)
        {
            screen_info->cwindows = g_list_delete_link (screen_info->cwindows, sibling);
            screen_info->cwindows = g_list_insert_before (screen_info->cwindows, list, cw);
        }
    }
}

static void
resize_win (CWindow *cw, gint x, gint y, gint width, gint height, gint bw)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    XserverRegion damage;

    g_return_if_fail (cw != NULL);
    TRACE ("entering resize_win");
    TRACE ("resizing 0x%lx, (%i,%i) %ix%i", cw->id, x, y, width, height);

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;
    damage = None;

    if (WIN_IS_VISIBLE(cw))
    {
        damage = XFixesCreateRegion (display_info->dpy, NULL, 0);
        if (cw->extents)
        {
            XFixesCopyRegion (display_info->dpy, damage, cw->extents);
        }
    }

    if (cw->extents)
    {
        XFixesDestroyRegion (display_info->dpy, cw->extents);
        cw->extents = None;
    }

    if ((cw->attr.width != width) || (cw->attr.height != height))
    {
#if HAVE_NAME_WINDOW_PIXMAP
        if (cw->name_window_pixmap)
        {
            XFreePixmap (display_info->dpy, cw->name_window_pixmap);
            cw->name_window_pixmap = None;
        }
#endif
        if (cw->picture)
        {
            XRenderFreePicture (display_info->dpy, cw->picture);
            cw->picture = None;
        }

        if (cw->shadow)
        {
            XRenderFreePicture (display_info->dpy, cw->shadow);
            cw->shadow = None;
        }
    }

    if ((cw->attr.width != width) || (cw->attr.height != height) ||
        (cw->attr.x != x) || (cw->attr.y != y))
    {
        if (cw->borderSize)
        {
            XFixesDestroyRegion (display_info->dpy, cw->borderSize);
            cw->borderSize = None;
        }

        if (cw->clientSize)
        {
            XFixesDestroyRegion (display_info->dpy, cw->clientSize);
            cw->clientSize = None;
        }
    }

    cw->attr.x = x;
    cw->attr.y = y;
    cw->attr.width = width;
    cw->attr.height = height;
    cw->attr.border_width = bw;

    if (damage)
    {
        cw->extents = win_extents (cw);
        XFixesUnionRegion (display_info->dpy, damage, damage, cw->extents);

        fix_region (cw, damage);
        /* damage region will be destroyed by add_damage () */
        add_damage (screen_info, damage);
    }
}

static void
reshape_win (CWindow *cw)
{
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    XserverRegion damage;

    g_return_if_fail (cw != NULL);
    TRACE ("entering reshape_win");

    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

    damage = None;

    if (WIN_IS_VISIBLE(cw))
    {
        damage = XFixesCreateRegion (display_info->dpy, NULL, 0);
        if (cw->extents)
        {
            XFixesCopyRegion (display_info->dpy, damage, cw->extents);
        }
    }

    if (cw->extents)
    {
        XFixesDestroyRegion (display_info->dpy, cw->extents);
        cw->extents = None;
    }

    if (cw->shadow)
    {
        XRenderFreePicture (display_info->dpy, cw->shadow);
        cw->shadow = None;
    }

    if (cw->borderSize)
    {
        XFixesDestroyRegion (display_info->dpy, cw->borderSize);
        cw->borderSize = None;
    }

    if (cw->clientSize)
    {
        XFixesDestroyRegion (display_info->dpy, cw->clientSize);
        cw->clientSize = None;
    }

    if (damage)
    {
        cw->extents = win_extents (cw);
        XFixesUnionRegion (display_info->dpy, damage, damage, cw->extents);

        /* A shape notify will likely change the shadows too, so clear the extents */
        XFixesDestroyRegion (display_info->dpy, cw->extents);
        cw->extents = None;

        fix_region (cw, damage);
        /* damage region will be destroyed by add_damage () */
        add_damage (screen_info, damage);
    }
}

static void
destroy_win (DisplayInfo *display_info, Window id)
{
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering destroy_win: 0x%lx", id);

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        ScreenInfo *screen_info;

        if (WIN_IS_VIEWABLE (cw))
        {
            unmap_win (cw);
        }
        screen_info = cw->screen_info;
        screen_info->cwindows = g_list_remove (screen_info->cwindows, (gconstpointer) cw);

        free_win_data (cw, TRUE);
    }
}

static void
recenter_zoomed_area (ScreenInfo *screen_info, int x_root, int y_root)
{
    int zf = screen_info->transform.matrix[0][0];
    double zoom = XFixedToDouble(zf);
    Display *dpy = screen_info->display_info->dpy;

    if(screen_info->zoomed)
    {
        int xp = x_root * (1-zoom);
        int yp = y_root * (1-zoom);
        screen_info->transform.matrix[0][2] = (xp<<16);
        screen_info->transform.matrix[1][2] = (yp<<16);
    }

    if(zf > (1<<14) && zf < (1<<16))
        XRenderSetPictureFilter(dpy, screen_info->rootBuffer, "bilinear", NULL, 0);
    else
        XRenderSetPictureFilter(dpy, screen_info->rootBuffer, "nearest", NULL, 0);

    XRenderSetPictureTransform(dpy, screen_info->rootBuffer, &screen_info->transform);

    damage_screen(screen_info);
}

static gboolean
zoom_timeout_cb (gpointer data)
{
    ScreenInfo   *screen_info;
    Window       root_return;
    Window       child_return;
    int          x_root, y_root;
    int          x_win, y_win;
    unsigned int mask;
    static int   x_old=-1, y_old=-1;

    screen_info = (ScreenInfo *) data;
    
    if(!screen_info->zoomed)
    {
        screen_info->zoom_timeout_id = 0;
        return FALSE; /* stop calling this callback */
    }

    XQueryPointer (screen_info->display_info->dpy, screen_info->xroot,
			    &root_return, &child_return,
			    &x_root, &y_root, &x_win, &y_win, &mask);
    if(x_old != x_root || y_old != y_root)
    {
        x_old = x_root; y_old = y_root;
        recenter_zoomed_area(screen_info, x_root, y_root);
    }
    return TRUE;
}

static void
compositorHandleDamage (DisplayInfo *display_info, XDamageNotifyEvent *ev)
{
    ScreenInfo *screen_info;
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleDamage for 0x%lx", ev->drawable);

    /*
      ev->drawable is the window ID of the damaged window
      ev->geometry is the geometry of the damaged window
      ev->area     is the bounding rect for the damaged area
      ev->damage   is the damage handle returned by XDamageCreate()
     */

    cw = find_cwindow_in_display (display_info, ev->drawable);
    if ((cw) && WIN_IS_REDIRECTED(cw))
    {
        screen_info = cw->screen_info;
        repair_win (cw, &ev->area);
        screen_info->damages_pending = ev->more;
    }
}

static void
compositorHandlePropertyNotify (DisplayInfo *display_info, XPropertyEvent *ev)
{
#ifdef MONITOR_ROOT_PIXMAP
    gint p;
    Atom backgroundProps[2];
#endif

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandlePropertyNotify for 0x%lx", ev->window);

#ifdef MONITOR_ROOT_PIXMAP
    backgroundProps[0] = display_info->atoms[XROOTPMAP];
    backgroundProps[1] = display_info->atoms[XSETROOT];

    for (p = 0; p < 2; p++)
    {
        if (ev->atom == backgroundProps[p] && ev->state == PropertyNewValue)
        {
            ScreenInfo *screen_info = myDisplayGetScreenFromRoot (display_info, ev->window);
            if ((screen_info) && (screen_info->rootTile))
            {
                XClearArea (display_info->dpy, screen_info->output, 0, 0, 0, 0, TRUE);
                XRenderFreePicture (display_info->dpy, screen_info->rootTile);
                screen_info->rootTile = None;
                damage_screen (screen_info);

                return;
            }
        }
    }
#endif

    /* check if Trans property was changed */
    if (ev->atom == display_info->atoms[NET_WM_WINDOW_OPACITY])
    {
        CWindow *cw = find_cwindow_in_display (display_info, ev->window);
        TRACE ("Opacity property changed for id 0x%lx", ev->window);

        if (cw)
        {
            Client *c = cw->c;

            TRACE ("Opacity changed for 0x%lx", cw->id);
            if (!getOpacity (display_info, cw->id, &cw->opacity))
            {
                /* The property was removed */
                cw->opacity = NET_WM_OPAQUE;
            }
            set_win_opacity (cw, cw->opacity);
            cw->native_opacity = WIN_IS_OPAQUE(cw);

            /* Transset changes the property on the frame, not the client
               window. We need to check and update the client "opacity"
               value accordingly.
              */
            if (c)
            {
                if (c->opacity != cw->opacity)
                {
                    clientSetOpacity (c, cw->opacity, 0, 0);
                }
            }
        }
    }
    else if (ev->atom == display_info->atoms[NET_WM_WINDOW_OPACITY_LOCKED])
    {
        CWindow *cw = find_cwindow_in_display (display_info, ev->window);
        TRACE ("Opacity locking property changed for id 0x%lx", ev->window);

        if (cw)
        {
            cw->opacity_locked = getOpacityLock (display_info, cw->id);
            if (cw->c)
            {
                if (cw->opacity_locked)
                {
                    FLAG_SET (cw->c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED);
                }
                else
                {
                    FLAG_UNSET (cw->c->xfwm_flags, XFWM_FLAG_OPACITY_LOCKED);
                }
            }
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
    ScreenInfo *screen_info;
    XRectangle rect[1];
    CWindow *cw;

    g_return_if_fail (display_info);
    TRACE ("entering compositorHandleExpose for 0x%lx", ev->window);

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

    cw = find_cwindow_in_display (display_info, ev->window);
    if (cw)
    {
        restack_win (cw, ev->above);
        resize_win (cw, ev->x, ev->y, ev->width, ev->height, ev->border_width);
    }
}

static void
compositorHandleCirculateNotify (DisplayInfo *display_info, XCirculateEvent *ev)
{
    CWindow *cw;
    CWindow *top;
    GList *first;
    Window above;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleCirculateNotify for 0x%lx", ev->window);

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
}

static void
compositorHandleCreateNotify (DisplayInfo *display_info, XCreateWindowEvent *ev)
{
    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleCreateNotify for 0x%lx", ev->window);

    /*
       We are only interested in top level windows, other will
       be caught by the WM.
     */

    if (myDisplayGetScreenFromRoot (display_info, ev->parent) != NULL)
    {
        if (!find_cwindow_in_display (display_info, ev->window))
        {
            add_win (display_info, ev->window, NULL);
        }
    }
}

static void
compositorHandleReparentNotify (DisplayInfo *display_info, XReparentEvent *ev)
{
    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleReparentNotify for 0x%lx", ev->window);

    if (myDisplayGetScreenFromRoot (display_info, ev->parent) != NULL)
    {
        add_win (display_info, ev->window, NULL);
    }
    else
    {
        destroy_win (display_info, ev->window);
    }
}

static void
compositorHandleDestroyNotify (DisplayInfo *display_info, XDestroyWindowEvent *ev)
{
    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleDestroyNotify for 0x%lx", ev->window);

    destroy_win (display_info, ev->window);
}

static void
compositorHandleMapNotify (DisplayInfo *display_info, XMapEvent *ev)
{
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleMapNotify for 0x%lx", ev->window);

    cw = find_cwindow_in_display (display_info, ev->window);
    if (cw)
    {
        map_win (cw);
    }
}

static void
compositorHandleUnmapNotify (DisplayInfo *display_info, XUnmapEvent *ev)
{
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleUnmapNotify for 0x%lx", ev->window);

    if (ev->from_configure)
    {
        TRACE ("Ignoring UnmapNotify caused by parent's resize");
        return;
    }

    cw = find_cwindow_in_display (display_info, ev->window);
    if (cw)
    {
        if (WIN_IS_VIEWABLE (cw))
        {
            unmap_win (cw);
        }
    }
}

static void
compositorHandleShapeNotify (DisplayInfo *display_info, XShapeEvent *ev)
{
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleShapeNotify for 0x%lx", ev->window);

    cw = find_cwindow_in_display (display_info, ev->window);
    if (cw)
    {
        if (ev->kind == ShapeBounding)
        {
            if (!(ev->shaped) && (cw->shaped))
            {
                cw->shaped = FALSE;
            }
            reshape_win  (cw);
            if ((ev->shaped) && !(cw->shaped))
            {
                cw->shaped = TRUE;
            }
        }
    }
}

#ifdef HAVE_RANDR
static void
compositorHandleRandrNotify (DisplayInfo *display_info, XRRScreenChangeNotifyEvent *ev)
{
    ScreenInfo *screen_info;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (ev != NULL);
    TRACE ("entering compositorHandleRandrNotify for 0x%lx", ev->window);

    screen_info = myDisplayGetScreenFromRoot (display_info, ev->window);
    if (screen_info)
        get_refresh_rate (screen_info);

    XRRUpdateConfiguration ((XEvent *) ev);
}
#endif /* HAVE_RANDR */

static void
compositorSetCMSelection (ScreenInfo *screen_info, Window w)
{
    DisplayInfo *display_info;
    gchar selection[32];
    Atom a;

    g_return_if_fail (screen_info != NULL);

    display_info = screen_info->display_info;
    /* Newer EWMH standard property "_NET_WM_CM_S<n>" */
    g_snprintf (selection, sizeof (selection), "_NET_WM_CM_S%d", screen_info->screen);
    a = XInternAtom (display_info->dpy, selection, FALSE);
    setXAtomManagerOwner (display_info, a, screen_info->xroot, w);

    /* Older property "COMPOSITING_MANAGER" */
    setAtomIdManagerOwner (display_info, COMPOSITING_MANAGER, screen_info->xroot, w);
}

#endif /* HAVE_COMPOSITOR */

gboolean
compositorIsUsable (DisplayInfo *display_info)
{
#ifdef HAVE_COMPOSITOR
    if (!display_info->enable_compositor)
    {
        TRACE ("compositor disabled");
        return FALSE;
    }
    else if (display_info->composite_mode != CompositeRedirectManual)
    {
        TRACE ("compositor not set to manual redirect mode");
        return FALSE;
    }
    return TRUE;
#else
    return FALSE;
#endif /* HAVE_COMPOSITOR */
}

void
compositorAddWindow (DisplayInfo *display_info, Window id, Client *c)
{
#ifdef HAVE_COMPOSITOR
    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering compositorAddWindow for 0x%lx", id);

    if (!compositorIsUsable (display_info))
    {
        return;
    }

    if (!compositorSetClient (display_info, id, c))
    {
        add_win (display_info, id, c);
    }
#endif /* HAVE_COMPOSITOR */
}

gboolean
compositorSetClient (DisplayInfo *display_info, Window id, Client *c)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    g_return_val_if_fail (display_info != NULL, FALSE);
    g_return_val_if_fail (id != None, FALSE);
    TRACE ("entering compositorSetClient: 0x%lx", id);

    if (!compositorIsUsable (display_info))
    {
        return FALSE;
    }

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        if (cw->c != c)
        {
            update_extents (cw);
            cw->c = c;
        }
        return TRUE;
    }
#endif /* HAVE_COMPOSITOR */
    return FALSE;
}

void
compositorRemoveWindow (DisplayInfo *display_info, Window id)
{
#ifdef HAVE_COMPOSITOR
    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering compositorRemoveWindow: 0x%lx", id);

    if (!compositorIsUsable (display_info))
    {
        return;
    }

    destroy_win (display_info, id);
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

    if (!compositorIsUsable (display_info))
    {
        return;
    }

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        /* that will also damage the window */
        update_extents (cw);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorResizeWindow (DisplayInfo *display_info, Window id, int x, int y, int width, int height)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering compositorDamageWindow: 0x%lx", id);

    if (!compositorIsUsable (display_info))
    {
        return;
    }

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        resize_win (cw, x, y, width, height, 0);
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

    if (!compositorIsUsable (display_info))
    {
        return;
    }
    else if (ev->type == CreateNotify)
    {
        compositorHandleCreateNotify (display_info, (XCreateWindowEvent *) ev);
    }
    else if (ev->type == DestroyNotify)
    {
        compositorHandleDestroyNotify (display_info, (XDestroyWindowEvent *) ev);
    }
    else if (ev->type == ConfigureNotify)
    {
        compositorHandleConfigureNotify (display_info, (XConfigureEvent *) ev);
    }
    else if (ev->type == ReparentNotify)
    {
        compositorHandleReparentNotify (display_info, (XReparentEvent *) ev);
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
    else if (ev->type == MapNotify)
    {
        compositorHandleMapNotify (display_info, (XMapEvent *) ev);
    }
    else if (ev->type == UnmapNotify)
    {
        compositorHandleUnmapNotify (display_info, (XUnmapEvent *) ev);
    }
    else if (ev->type == (display_info->damage_event_base + XDamageNotify))
    {
        compositorHandleDamage (display_info, (XDamageNotifyEvent *) ev);
    }
    else if (ev->type == (display_info->shape_event_base + ShapeNotify))
    {
        compositorHandleShapeNotify (display_info, (XShapeEvent *) ev);
    }
#ifdef HAVE_RANDR
    else if (ev->type == (display_info->xrandr_event_base + RRScreenChangeNotify))
    {
        compositorHandleRandrNotify (display_info, (XRRScreenChangeNotifyEvent *) ev);
    }
#endif /* HAVE_RANDR */

#if TIMEOUT_REPAINT == 0
    repair_display (display_info);
#endif /* TIMEOUT_REPAINT */

#endif /* HAVE_COMPOSITOR */
}

void
compositorZoomIn (ScreenInfo *screen_info, XButtonEvent *ev)
{
#ifdef HAVE_COMPOSITOR
    screen_info->transform.matrix[0][0] -= 4096;
    screen_info->transform.matrix[1][1] -= 4096;

    if(screen_info->transform.matrix[0][0] < (1<<10))
    {
        screen_info->transform.matrix[0][0] = (1<<10);
        screen_info->transform.matrix[1][1] = (1<<10);
    }

    screen_info->zoomed = 1;
    if(!screen_info->zoom_timeout_id)
    {
        int timeout_rate = 30; /* per second */
#ifdef HAVE_RANDR
        if (screen_info->display_info->have_xrandr)
        {
            timeout_rate = screen_info->refresh_rate/2;
            if(timeout_rate < 1)
                timeout_rate = 30;
        }
#endif /* HAVE_RANDR */
        screen_info->zoom_timeout_id = g_timeout_add ((1000/timeout_rate), zoom_timeout_cb, screen_info);
    }
    recenter_zoomed_area(screen_info, ev->x_root, ev->y_root);
#endif /* HAVE_COMPOSITOR */
}

void
compositorZoomOut (ScreenInfo *screen_info, XButtonEvent *ev)
{
#ifdef HAVE_COMPOSITOR
    if(screen_info->zoomed)
    {
        screen_info->transform.matrix[0][0] += 4096;
        screen_info->transform.matrix[1][1] += 4096;

        if(screen_info->transform.matrix[0][0] >= (1<<16))
        {
            screen_info->transform.matrix[0][0] = (1<<16);
            screen_info->transform.matrix[1][1] = (1<<16);
            screen_info->zoomed = 0;
            screen_info->transform.matrix[0][2] = 0;
            screen_info->transform.matrix[1][2] = 0;
        }
        recenter_zoomed_area(screen_info, ev->x_root, ev->y_root);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorInitDisplay (DisplayInfo *display_info)
{
#ifdef HAVE_COMPOSITOR
    int composite_major, composite_minor;

    composite_major = 0;
    composite_minor = 0;

    if (!XCompositeQueryExtension (display_info->dpy,
                                &display_info->composite_event_base,
                                &display_info->composite_error_base))
    {
        g_warning ("The display does not support the XComposite extension.");
        display_info->have_composite = FALSE;
        display_info->composite_event_base = 0;
        display_info->composite_error_base = 0;
    }
    else
    {
        display_info->have_composite = TRUE;
        XCompositeQueryVersion (display_info->dpy, &composite_major, &composite_minor);
#if DEBUG
        g_print ("composite event base: %i\n", display_info->composite_event_base);
        g_print ("composite error base: %i\n", display_info->composite_error_base);
        g_print ("composite version: %i.%i\n", composite_major, composite_minor);
#endif
    }

    if (!XDamageQueryExtension (display_info->dpy,
                            &display_info->damage_event_base,
                            &display_info->damage_error_base))
    {
        g_warning ("The display does not support the XDamage extension.");
        display_info->have_damage = FALSE;
        display_info->damage_event_base = 0;
        display_info->damage_error_base = 0;
    }
    else
    {
        display_info->have_damage = TRUE;
#if DEBUG
        g_print ("damage event base: %i\n", display_info->damage_event_base);
        g_print ("damage error base: %i\n", display_info->damage_error_base);
#endif
    }

    if (!XFixesQueryExtension (display_info->dpy,
                            &display_info->fixes_event_base,
                            &display_info->fixes_error_base))
    {
        g_warning ("The display does not support the XFixes extension.");
        display_info->have_fixes = FALSE;
        display_info->fixes_event_base = 0;
        display_info->fixes_error_base = 0;
    }
    else
    {
        display_info->have_fixes = TRUE;
#if DEBUG
        g_print ("fixes event base: %i\n", display_info->fixes_event_base);
        g_print ("fixes error base: %i\n", display_info->fixes_error_base);
#endif /* DEBUG */
    }

    display_info->enable_compositor = ((display_info->have_render)
                                    && (display_info->have_composite)
                                    && (display_info->have_damage)
                                    && (display_info->have_fixes));
    if (!display_info->enable_compositor)
    {
        g_warning ("Compositing manager disabled.");
    }

    display_info->composite_mode = 0;
#if HAVE_NAME_WINDOW_PIXMAP
    display_info->have_name_window_pixmap = ((composite_major > 0) || (composite_minor >= 2));
#else  /* HAVE_NAME_WINDOW_PIXMAP */
    display_info->have_name_window_pixmap = FALSE;
#endif /* HAVE_NAME_WINDOW_PIXMAP */

#if HAVE_OVERLAYS
    display_info->have_overlays = ((composite_major > 0) || (composite_minor >= 3));
#endif /* HAVE_OVERLAYS */

#else /* HAVE_COMPOSITOR */
    display_info->enable_compositor = FALSE;
#endif /* HAVE_COMPOSITOR */
}

void
compositorSetCompositeMode (DisplayInfo *display_info, gboolean use_manual_redirect)
{
#ifdef HAVE_COMPOSITOR
    g_return_if_fail (display_info != NULL);
    TRACE ("entering compositorSetCompositeMode");

    if (use_manual_redirect)
    {
        display_info->composite_mode = CompositeRedirectManual;
    }
    else
    {
        display_info->composite_mode = CompositeRedirectAutomatic;
    }
#endif /* HAVE_COMPOSITOR */
}

gboolean
compositorManageScreen (ScreenInfo *screen_info)
{
#ifdef HAVE_COMPOSITOR
    DisplayInfo *display_info;
    XRenderPictureAttributes pa;
    XRenderPictFormat *visual_format;

    g_return_val_if_fail (screen_info != NULL, FALSE);
    TRACE ("entering compositorManageScreen");

    display_info = screen_info->display_info;
    screen_info->compositor_active = FALSE;

    gdk_error_trap_push ();
    XCompositeRedirectSubwindows (display_info->dpy, screen_info->xroot, display_info->composite_mode);
    XSync (display_info->dpy, FALSE);

    if (gdk_error_trap_pop ())
    {
        g_warning ("Another compositing manager is running on screen %i", screen_info->screen);
        return FALSE;
    }

    if (display_info->composite_mode == CompositeRedirectAutomatic)
    {
        /* That's enough for automatic compositing */
        TRACE ("Automatic compositing enabled");
        return TRUE;
    }

    visual_format = XRenderFindVisualFormat (display_info->dpy,
                                             DefaultVisual (display_info->dpy,
                                                            screen_info->screen));
    if (!visual_format)
    {
        g_warning ("Cannot find visual format on screen %i", screen_info->screen);
        return FALSE;
    }

    screen_info->output = screen_info->xroot;
#if HAVE_OVERLAYS
    if (display_info->have_overlays)
    {
        screen_info->overlay = XCompositeGetOverlayWindow (display_info->dpy, screen_info->xroot);
        if (screen_info->overlay != None)
        {
            XSetWindowAttributes attributes;

            screen_info->root_overlay = XCreateWindow (display_info->dpy, screen_info->overlay,
                                                       0, 0, screen_info->width, screen_info->height, 0, screen_info->depth,
                                                       InputOutput, screen_info->visual, 0, &attributes);
            XMapWindow (display_info->dpy, screen_info->root_overlay);
            XRaiseWindow (display_info->dpy, screen_info->overlay);
            XShapeCombineRectangles (display_info->dpy, screen_info->overlay,
                                     ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
            XShapeCombineRectangles (display_info->dpy, screen_info->root_overlay,
                                     ShapeInput, 0, 0, NULL, 0, ShapeSet, Unsorted);
            screen_info->output = screen_info->root_overlay;
            TRACE ("Overlay enabled");
        }
        else
        {
            /* Something is wrong with overlay support */
            TRACE ("Cannot get root window overlay, overlay support disabled");
            display_info->have_overlays = FALSE;
        }
    }
#endif /* HAVE_OVERLAYS */

    pa.subwindow_mode = IncludeInferiors;
    screen_info->rootPicture = XRenderCreatePicture (display_info->dpy, screen_info->output,
                                                     visual_format, CPSubwindowMode, &pa);

    if (screen_info->rootPicture == None)
    {
        g_warning ("Cannot create root picture on screen %i", screen_info->screen);
        return FALSE;
    }

    screen_info->gaussianSize = -1;
    screen_info->gaussianMap = make_gaussian_map(SHADOW_RADIUS);
    presum_gaussian (screen_info);
    screen_info->rootBuffer = None;
    /* Change following argb values to play with shadow colors */
    screen_info->blackPicture = solid_picture (screen_info,
                                               TRUE,
                                               1.0, /* alpha */
                                               0.0, /* red   */
                                               0.0, /* green */
                                               0.0  /* blue  */);
    screen_info->rootTile = None;
    screen_info->allDamage = None;
    screen_info->cwindows = NULL;
    screen_info->compositor_active = TRUE;
    screen_info->wins_unredirected = 0;
    screen_info->compositor_timeout_id = 0;
    screen_info->zoomed = 0;
    screen_info->zoom_timeout_id = 0;
    screen_info->damages_pending = FALSE;

    XClearArea (display_info->dpy, screen_info->output, 0, 0, 0, 0, TRUE);
    compositorSetCMSelection (screen_info, screen_info->xfwm4_win);
    TRACE ("Manual compositing enabled");

#ifdef HAVE_LIBDRM
    open_dri (screen_info);
    screen_info->dri_success = TRUE;
    screen_info->dri_secondary = FALSE;
    screen_info->dri_time = 0;
    screen_info->vblank_time = 0;

#ifdef HAVE_RANDR
    if (display_info->have_xrandr)
    {
        get_refresh_rate(screen_info);
        XRRSelectInput(display_info->dpy, screen_info->xroot, RRScreenChangeNotifyMask);
    }
#endif /* HAVE_RANDR */
#endif /* HAVE_LIBDRM */

    return TRUE;
#else
    return FALSE;
#endif /* HAVE_COMPOSITOR */
}

void
compositorUnmanageScreen (ScreenInfo *screen_info)
{
#ifdef HAVE_COMPOSITOR
    DisplayInfo *display_info;
    GList *list;
    gint i;

    g_return_if_fail (screen_info != NULL);
    TRACE ("entering compositorUnmanageScreen");

    display_info = screen_info->display_info;
    if (!compositorIsUsable (display_info))
    {
        return;
    }

    if (!(screen_info->compositor_active))
    {
        TRACE ("Compositor not active on screen %i", screen_info->screen);
        return;
    }
    screen_info->compositor_active = FALSE;

#if TIMEOUT_REPAINT
    remove_timeouts (screen_info);
#endif /* TIMEOUT_REPAINT */

    i = 0;
    for (list = screen_info->cwindows; list; list = g_list_next (list))
    {
        CWindow *cw2 = (CWindow *) list->data;
        free_win_data (cw2, TRUE);
        i++;
    }
    g_list_free (screen_info->cwindows);
    screen_info->cwindows = NULL;
    TRACE ("Compositor: removed %i window(s) remaining", i);

#if HAVE_OVERLAYS
    if (display_info->have_overlays)
    {
        XDestroyWindow (display_info->dpy, screen_info->root_overlay);
        screen_info->root_overlay = None;

        XCompositeReleaseOverlayWindow (display_info->dpy, screen_info->overlay);
        screen_info->overlay = None;
    }
#endif /* HAVE_OVERLAYS */

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

    screen_info->gaussianSize = -1;
    screen_info->wins_unredirected = 0;

    XCompositeUnredirectSubwindows (display_info->dpy, screen_info->xroot,
                                    display_info->composite_mode);

    compositorSetCMSelection (screen_info, None);

#ifdef HAVE_LIBDRM
    close_dri (screen_info);

#ifdef HAVE_RANDR
    if (display_info->have_xrandr)
    {
        XRRSelectInput (display_info->dpy, screen_info->xroot, 0);
    }
#endif /* HAVE_RANDR */
#endif /* HAVE_LIBDRM */

#endif /* HAVE_COMPOSITOR */
}

void
compositorAddAllWindows (ScreenInfo *screen_info)
{
#ifdef HAVE_COMPOSITOR
    DisplayInfo *display_info;
    Window w1, w2, *wins;
    unsigned int count, i;

    TRACE ("entering compositorAddScreen");

    display_info = screen_info->display_info;
    if (!compositorIsUsable (display_info))
    {
        return;
    }

    myDisplayGrabServer (display_info);
    XQueryTree (display_info->dpy, screen_info->xroot, &w1, &w2, &wins, &count);

    for (i = 0; i < count; i++)
    {
        Client *c;

        c = myScreenGetClientFromWindow (screen_info, wins[i], SEARCH_FRAME);
        compositorAddWindow (display_info, wins[i], c);
    }
    if (wins)
    {
        XFree (wins);
    }
    myDisplayUngrabServer (display_info);
#endif /* HAVE_COMPOSITOR */
}

gboolean
compositorActivateScreen (ScreenInfo *screen_info, gboolean active)
{
#ifdef HAVE_COMPOSITOR
    DisplayInfo *display_info;

    g_return_val_if_fail (screen_info != NULL, FALSE);
    TRACE ("entering compositorActivateScreen");

    display_info = screen_info->display_info;
    if (!compositorIsUsable (display_info))
    {
        return FALSE;
    }

    if (screen_info->compositor_active == active)
    {
        return FALSE;
    }

    if (active)
    {
        compositorManageScreen (screen_info);
        compositorAddAllWindows (screen_info);
    }
    else
    {
        compositorUnmanageScreen (screen_info);
    }

    return TRUE;
#else /* HAVE_COMPOSITOR */
    return FALSE;
#endif /* HAVE_COMPOSITOR */
}

void
compositorUpdateScreenSize (ScreenInfo *screen_info)
{
#ifdef HAVE_COMPOSITOR
    DisplayInfo *display_info;

    g_return_if_fail (screen_info != NULL);
    TRACE ("entering compositorUpdateScreenSize");

    display_info = screen_info->display_info;
    if (!compositorIsUsable (display_info))
    {
        return;
    }
#if HAVE_OVERLAYS
    if (display_info->have_overlays)
    {
        XResizeWindow (display_info->dpy, screen_info->overlay, screen_info->width, screen_info->height);
        XResizeWindow (display_info->dpy, screen_info->root_overlay, screen_info->width, screen_info->height);
    }
#endif
    if (screen_info->rootBuffer)
    {
        XRenderFreePicture (display_info->dpy, screen_info->rootBuffer);
        screen_info->rootBuffer = None;
    }
    damage_screen (screen_info);
#endif /* HAVE_COMPOSITOR */
}

void
compositorWindowSetOpacity (DisplayInfo *display_info, Window id, guint opacity)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    g_return_if_fail (display_info != NULL);
    g_return_if_fail (id != None);
    TRACE ("entering compositorSetOpacity for 0x%lx", id);

    if (!compositorIsUsable (display_info))
    {
        return;
    }

    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        set_win_opacity (cw, opacity);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorRebuildScreen (ScreenInfo *screen_info)
{
#ifdef HAVE_COMPOSITOR
    DisplayInfo *display_info;
    GList *list;

    g_return_if_fail (screen_info != NULL);
    TRACE ("entering compositorRepairScreen");

    display_info = screen_info->display_info;
    if (!compositorIsUsable (display_info))
    {
        return;
    }

    for (list = screen_info->cwindows; list; list = g_list_next (list))
    {
        CWindow *cw2 = (CWindow *) list->data;
        free_win_data (cw2, FALSE);
        init_opacity (cw2);
    }
    damage_screen (screen_info);
#endif /* HAVE_COMPOSITOR */
}

gboolean
compositorTestServer (DisplayInfo *display_info)
{
#ifdef HAVE_COMPOSITOR
    char *vendor;

    g_return_val_if_fail (display_info != NULL, FALSE);
    TRACE ("entering compositorTestServer");

    vendor = ServerVendor (display_info->dpy);

    if (vendor && (!strstr ("X.Org", vendor)))
    {
        /*
           Check the version, X.org 6.8.x has some bugs that makes
           it not suitable for use with xfwm4 compositor
         */
        if ((VendorRelease(display_info->dpy) / 10) <= 68)
        {
            return FALSE;
        }
    }
    return TRUE;
#else /* HAVE_COMPOSITOR */
    return FALSE;
#endif /* HAVE_COMPOSITOR */
}
