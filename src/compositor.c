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
        xfwm4    - (c) 2004 Olivier Fourdan

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
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

#define SHADOW_RADIUS   5
#define SHADOW_OPACITY  0.50
#define SHADOW_OFFSET_X (-SHADOW_RADIUS * 5 / 4)
#define SHADOW_OFFSET_Y (-SHADOW_RADIUS * 5 / 4)
#define OPAQUE          0xffffffff

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

    Picture picture;
    Picture alphaPict;
    Picture shadowPict;
    XserverRegion borderSize;
    XserverRegion extents;
    Picture shadow;
    gint shadow_dx;
    gint shadow_dy;
    gint shadow_width;
    gint shadow_height;
    gint mode;
    guint opacity;
    gboolean skipped;

    XserverRegion last_painted_extents;
    XserverRegion borderClip;
    
#if HAVE_NAME_WINDOW_PIXMAP
    Pixmap name_window_pixmap;
#endif /* HAVE_NAME_WINDOW_PIXMAP */  
};

static CWindow*
find_cwindow_in_screen (ScreenInfo *screen_info, Window id)
{
    GList *index;
    
    g_return_val_if_fail (id != None, NULL);
    g_return_val_if_fail (screen_info != NULL, NULL);

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
    gint gsize = screen_info->gaussianMap->size;
    gint ylimit, xlimit;
    gint swidth = width + gsize;
    gint sheight = height + gsize;
    gint center = gsize / 2;
    gint x, y;
    gint x_diff;
    gint opacity_gint = (gint) (opacity * 25);
    
    TRACE ("entering make_shadow");    
    data = g_malloc (swidth * sheight * sizeof (guchar));
    
    ximage = XCreateImage (myScreenGetXDisplay (screen_info),
                        DefaultVisual(myScreenGetXDisplay (screen_info), screen_info->screen),
                        8, ZPixmap, 0, (char *) data,
                        swidth, sheight, 8, swidth * sizeof (guchar));
    if (!ximage)
    {
        free (data);
        return NULL;
    }
    
    /*
    * Build the gaussian in sections
    */

    if (screen_info->gsize > 0)
    {
        d = screen_info->shadowTop[opacity_gint * (screen_info->gsize + 1) + screen_info->gsize];
    }
    else
    {
        d = sum_gaussian (screen_info->gaussianMap, opacity, center, center, width, height);
    }
    memset(data, d, sheight * swidth);
    
    d = sum_gaussian (screen_info->gaussianMap, opacity, center, center, width, height);
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
                d = screen_info->shadowCorner[opacity_gint * (screen_info->gsize + 1) 
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
                d = screen_info->shadowTop[opacity_gint * (screen_info->gsize + 1) + y];
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
            d = screen_info->shadowTop[opacity_gint * (screen_info->gsize + 1) + x];
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
shadow_picture (ScreenInfo *screen_info, gdouble opacity, Picture alpha_pict, 
                gint width, gint height, gint *wp, gint *hp)
{
    XImage *shadowImage;
    Pixmap shadowPixmap;
    Picture shadowPicture;
    XRenderPictFormat *render_format;
    GC gc;
    
    TRACE ("entering shadow_picture");    
    
    shadowImage = make_shadow (screen_info, opacity, width, height);

    if (!shadowImage)
    {
        return None;
    }

    shadowPixmap = XCreatePixmap (myScreenGetXDisplay (screen_info), screen_info->xroot, 
                                shadowImage->width, shadowImage->height, 8);

    if (!shadowPixmap)
    {
        XDestroyImage (shadowImage);
        return None;
    }

    render_format = XRenderFindStandardFormat (myScreenGetXDisplay (screen_info), 
                                            PictStandardA8);
    shadowPicture = XRenderCreatePicture (myScreenGetXDisplay (screen_info), 
                                        shadowPixmap, render_format, 0, 0);

    if (!shadowPicture)
    {
        XDestroyImage (shadowImage);
        XFreePixmap (myScreenGetXDisplay (screen_info), shadowPixmap);
        return None;
    }

    gc = XCreateGC (myScreenGetXDisplay (screen_info), shadowPixmap, 0, 0);
    
    if (!gc)
    {
        XDestroyImage (shadowImage);
        XFreePixmap (myScreenGetXDisplay (screen_info), shadowPixmap);
        XRenderFreePicture (myScreenGetXDisplay (screen_info), shadowPicture);
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

    TRACE ("entering solid_picture");    
    pixmap = XCreatePixmap (myScreenGetXDisplay (screen_info), 
                            screen_info->xroot, 1, 1, argb ? 32 : 8);
    if (!pixmap)
    {
        return None;
    }
    pa.repeat = True;
    render_format = XRenderFindStandardFormat (myScreenGetXDisplay (screen_info), 
                                    argb ? PictStandardARGB32 : PictStandardA8);
    picture = XRenderCreatePicture (myScreenGetXDisplay (screen_info), pixmap, 
                                    render_format, CPRepeat,  &pa);
    if (!picture)
    {
        XFreePixmap (myScreenGetXDisplay (screen_info), pixmap);
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
    DisplayInfo *display_info;
    ScreenInfo *screen_info;

    g_return_val_if_fail (cw != NULL, None);

    TRACE ("entering border_size");    
    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

    border = XFixesCreateRegionFromWindow (display_info->dpy, cw->id, WindowRegionBounding);
    XFixesTranslateRegion (display_info->dpy, border, 
                        cw->attr.x + cw->attr.border_width,
                        cw->attr.y + cw->attr.border_width);

    return border;
}

static Picture
root_tile (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;
    Display *dpy;
    Picture picture;
    Pixmap pixmap;
    Bool fill;
    XRenderPictureAttributes pa;
    XRenderPictFormat *format;
    gint p;
    Atom backgroundProps[2] = { misc_xrootpmap, misc_xsetroot };

    TRACE ("entering root_tile");    
    display_info = screen_info->display_info;
    dpy = display_info->dpy;
    pixmap = None;
    backgroundProps[0] = misc_xrootpmap;
    backgroundProps[1] = misc_xsetroot;
    
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
            (actual_type == misc_pixmap) && 
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
        fill = True;
    }
    pa.repeat = True;
    format = XRenderFindVisualFormat (dpy, DefaultVisual (dpy, screen_info->screen));
    picture = XRenderCreatePicture (dpy, pixmap, format, CPRepeat, &pa);
    if (fill)
    {
        XRenderColor c;
        
        c.red = c.green = c.blue = 0xd0d0;
        c.alpha = 0xffff;
        XRenderFillRectangle (dpy, PictOpSrc, picture, &c, 0, 0, 1, 1);
    }
    return picture;
}

static void
paint_root (ScreenInfo *screen_info)
{
    TRACE ("entering paint_root");    
    if (!(screen_info->rootTile))
    {
        screen_info->rootTile = root_tile (screen_info);
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
    ScreenInfo *screen_info;
    Client *c = NULL;
    XRectangle r;

    g_return_val_if_fail (cw != NULL, None);

    TRACE ("entering win_extents: 0x%lx", cw->id);    

    screen_info = cw->screen_info;
    r.x = cw->attr.x;
    r.y = cw->attr.y;
    r.width = cw->attr.width + cw->attr.border_width * 2;
    r.height = cw->attr.height + cw->attr.border_width * 2;
    
    if (cw->c)
    {
        c = cw->c;
        if (!FLAG_TEST (c->xfwm_flags, XFWM_FLAG_HAS_BORDER))
        {
            TRACE ("window 0x%lx has no border, no extents", cw->id);    
            return XFixesCreateRegion (myScreenGetXDisplay (screen_info), &r, 1);
        }
    }
    else
    {
        TRACE ("window 0x%lx has no client, no extents", cw->id);    
    }
    
    if ((cw->mode != WINDOW_ARGB) && (c))
    {
        XRectangle sr;

        TRACE ("window 0x%lx (%s) has extents", cw->id, c->name);    
        cw->shadow_dx = SHADOW_OFFSET_X;
        cw->shadow_dy = SHADOW_OFFSET_Y;
        if (!cw->shadow)
        {
            double opacity = SHADOW_OPACITY;
            if (cw->mode == WINDOW_TRANS)
            {
                opacity = opacity * ((double) cw->opacity) / ((double) OPAQUE);
            }
            cw->shadow = shadow_picture (screen_info, opacity, cw->alphaPict,
                                        cw->attr.width + cw->attr.border_width * 2,
                                        cw->attr.height + cw->attr.border_width * 2,
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

    return XFixesCreateRegion (myScreenGetXDisplay (screen_info), &r, 1);
}

static void
get_paint_bounds (CWindow *cw, gint *x, gint *y, gint *w, gint *h)
{
    g_return_if_fail (cw != NULL);

    TRACE ("entering get_paint_bounds");    
#if HAVE_NAME_WINDOW_PIXMAP
    *x = cw->attr.x;
    *y = cw->attr.y;
    *w = cw->attr.width + cw->attr.border_width * 2;
    *h = cw->attr.height + cw->attr.border_width * 2;
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
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    XRenderPictureAttributes pa;
    XRenderPictFormat *format;
    Drawable draw = cw->id;
    Picture p;
    
    screen_info = cw->screen_info;
    display_info = screen_info->display_info;
    dpy = display_info->dpy;

#if HAVE_NAME_WINDOW_PIXMAP
    if ((display_info->have_name_window_pixmap) && !(cw->name_window_pixmap))
    {
        cw->name_window_pixmap = XCompositeNameWindowPixmap (dpy, cw->id);
    }               
    if (cw->name_window_pixmap)
    {
        draw = cw->name_window_pixmap;
    }
#endif
    pa.subwindow_mode = IncludeInferiors;
    format = get_window_format (cw);
    if (format)
    {    
        return XRenderCreatePicture (dpy, draw, format, CPSubwindowMode, &pa);
    }
    return None;
}

static void
paint_all (ScreenInfo *screen_info, XserverRegion region)
{
    DisplayInfo *display_info;
    Display *dpy;
    GList *index;
    gint screen_width;
    gint screen_height;
    gint screen_number;
    Window xroot;

    TRACE ("entering paint_all");    
    display_info = screen_info->display_info;
    dpy = display_info->dpy;
    
    screen_width = gdk_screen_get_width (screen_info->gscr);
    screen_height = gdk_screen_get_height (screen_info->gscr);
    screen_number = screen_info->screen;
    xroot = screen_info->xroot;

    if (region == None)
    {
        XRectangle  r;
        
        TRACE ("region is empty, creating a whole screen region");
        r.x = 0;
        r.y = 0;
        r.width = screen_width;
        r.height = screen_height;
        region = XFixesCreateRegion (dpy, &r, 1);
    }

    /* Create root buffer if not done yet */
    if (screen_info->rootBuffer == None)
    {
        XRenderPictFormat *format;
        Pixmap  rootPixmap;
        Visual *visual;
        gint depth;
        
        TRACE ("rootBuffer is empty, creating rootBuffer");
                
        visual = DefaultVisual (dpy, screen_number);
        depth = DefaultDepth (dpy, screen_number);
        format = XRenderFindVisualFormat (dpy, visual);
        rootPixmap = XCreatePixmap (dpy, xroot, screen_width, screen_height, depth);
        screen_info->rootBuffer = XRenderCreatePicture (dpy, rootPixmap, format, 0, 0);
        XFreePixmap (dpy, rootPixmap);
    }
    
    XFixesSetPictureClipRegion (dpy, screen_info->rootPicture, 0, 0, region);
    
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
        
        cw->skipped = FALSE;    
        if (!(cw->picture))
        {
            cw->picture = get_window_picture (cw);
        }
        if (screen_info->clipChanged)
        {
            if (cw->borderSize)
            {
                XFixesDestroyRegion (dpy, cw->borderSize);
                cw->borderSize = None;
            }
            if (cw->extents)
            {
                XFixesDestroyRegion (dpy, cw->extents);
                cw->extents = None;
            }
            if (cw->borderClip)
            {
                XFixesDestroyRegion (dpy, cw->borderClip);
                cw->borderClip = None;
            }
        }
        if (!cw->borderSize)
        {    
            cw->borderSize = border_size (cw);
        }
        if (!cw->extents)
        {    
            cw->extents = win_extents (cw);
        }
        if (cw->mode == WINDOW_SOLID)
        {
            gint x, y, w, h;
            get_paint_bounds (cw, &x, &y, &w, &h);
            XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, region);
            XFixesSubtractRegion (dpy, region, region, cw->borderSize);
            XRenderComposite (dpy, PictOpSrc, cw->picture, None, screen_info->rootBuffer, 
                            0, 0, 0, 0, x, y, w, h);
        }
        if (!cw->borderClip)
        {
            cw->borderClip = XFixesCreateRegion (dpy, 0, 0);
            XFixesCopyRegion (dpy, cw->borderClip, region);
        }
    }
    
    XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, region);
    paint_root (screen_info);
        
    for (index = g_list_last(screen_info->cwindows); index; index = g_list_previous (index))
    {
        CWindow *cw = (CWindow *) index->data;

        TRACE ("painting backward 0x%lx", cw->id);
        
        if (cw->skipped)
        {
            TRACE ("skipped 0x%lx", cw->id);
            continue;
        }
        
        XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, cw->borderClip);
        if (cw->shadow)
        {
            XRenderComposite (dpy, PictOpOver, screen_info->blackPicture, cw->shadow, 
                            screen_info->rootBuffer, 0, 0, 0, 0, 
                            cw->attr.x + cw->shadow_dx,
                            cw->attr.y + cw->shadow_dy,
                            cw->shadow_width, cw->shadow_height);
        }
        if ((cw->opacity != OPAQUE) && !(cw->alphaPict))
        {
            cw->alphaPict = solid_picture (screen_info, FALSE, 
                                        (double) cw->opacity / OPAQUE, 0, 0, 0);
        }
        if (cw->mode == WINDOW_TRANS)
        {
            gint x, y, w, h;
            get_paint_bounds (cw, &x, &y, &w, &h);
            XRenderComposite (dpy, PictOpOver, cw->picture, cw->alphaPict, 
                            screen_info->rootBuffer, 0, 0, 0, 0, x, y, w, h);
        }
        else if (cw->mode == WINDOW_ARGB)
        {
            gint x, y, w, h;
            get_paint_bounds (cw, &x, &y, &w, &h);
            XRenderComposite (dpy, PictOpOver, cw->picture, cw->alphaPict, 
                            screen_info->rootBuffer, 0, 0, 0, 0, x, y, w, h);
        }
        XFixesDestroyRegion (dpy, cw->borderClip);
        cw->borderClip = None;
    }
    
    TRACE ("Copying data back to screen");
    XFixesSetPictureClipRegion (dpy, screen_info->rootBuffer, 0, 0, None);
    XRenderComposite (dpy, PictOpSrc, screen_info->rootBuffer, None, screen_info->rootPicture,
                        0, 0, 0, 0, 0, 0, screen_width, screen_height);
    XFixesDestroyRegion (dpy, region);
}

static void
add_damage (ScreenInfo *screen_info, XserverRegion damage)
{
    TRACE ("entering add_damage");    
    if (screen_info->allDamage)
    {
        XFixesUnionRegion (myScreenGetXDisplay (screen_info), 
                        screen_info->allDamage, 
                        screen_info->allDamage, 
                        damage);
        XFixesDestroyRegion (myScreenGetXDisplay (screen_info), damage);
    }
    else
    {
        screen_info->allDamage = damage;
    }
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
repair_win (CWindow *cw)
{
    XserverRegion   parts;
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    
    g_return_if_fail (cw != NULL);
    
    TRACE ("entering repair_win");    
    screen_info = cw->screen_info;
    display_info = screen_info->display_info;

    if (!(cw->damaged))
    {
        parts = win_extents (cw);
        XDamageSubtract (myScreenGetXDisplay (screen_info), cw->damage, None, None);
    }
    else
    {
        parts = XFixesCreateRegion (myScreenGetXDisplay (screen_info), NULL, 0);
        XDamageSubtract (myScreenGetXDisplay (screen_info), cw->damage, None, parts);
        XFixesTranslateRegion (myScreenGetXDisplay (screen_info), parts,
                            cw->attr.x + cw->attr.border_width,
                            cw->attr.y + cw->attr.border_width);

    }
    add_damage (cw->screen_info, parts);
    cw->damaged = TRUE;
}

static guint
get_opacity_prop(CWindow *cw, guint def)
{
    ScreenInfo *screen_info;
    Atom actual;
    gint format;
    unsigned long n, left;
    guchar *data;
    gint result;
    
    TRACE ("entering get_opacity_prop");    
    screen_info = cw->screen_info;
    result = XGetWindowProperty(myScreenGetXDisplay (screen_info), cw->id, net_wm_opacity, 
                    0L, 1L, False, XA_CARDINAL, &actual, &format, &n, &left, &data);
    if ((result == Success) && (data != None))
    {
        guint i;
        memcpy (&i, data, sizeof (guint));
        XFree( (void *) data);
        return i;
    }
    return def;
}

static double
get_opacity_percent(CWindow *cw, double def)
{
    guint opacity = get_opacity_prop (cw, (guint) (OPAQUE * def));

    TRACE ("entering get_opacity_percent");    
    return opacity * 1.0 / OPAQUE;
}

static void
determine_mode(CWindow *cw)
{
    ScreenInfo *screen_info;
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
    else if (cw->opacity != OPAQUE)
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
        damage = XFixesCreateRegion (myScreenGetXDisplay (screen_info), 0, 0);
        XFixesCopyRegion (myScreenGetXDisplay (screen_info), damage, cw->extents);
        add_damage (screen_info, damage);
    }
}

static void
expose_root (ScreenInfo *screen_info, XRectangle *rects, gint nrects)
{
    XserverRegion region;
    
    TRACE ("entering expose_root");    
    region = XFixesCreateRegion (myScreenGetXDisplay (screen_info), rects, nrects);
    add_damage (screen_info, region);
}

static void
map_win (CWindow *cw)
{
    TRACE ("entering map_win");
    g_return_if_fail (cw != NULL);
    
    cw->viewable = TRUE;
    cw->damaged = FALSE;
}

static void
unmap_win (CWindow *cw)
{
    ScreenInfo *screen_info;
    
    TRACE ("entering unmap_win");
    g_return_if_fail (cw != NULL);

    cw->damaged = FALSE;

    screen_info = cw->screen_info;
    cw->viewable = FALSE;
    
    if (cw->extents != None)
    {
        add_damage (screen_info, cw->extents);
        cw->extents = None;
    }
    free_win_data (cw, FALSE);  
    screen_info->clipChanged = TRUE;
}

static void
add_win (DisplayInfo *display_info, Window id, Client *c, Window above)
{
    ScreenInfo *screen_info;
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
        /* We must be notified of property changes for transparency, even if the win is not managed */
        XSelectInput (display_info->dpy, id, PropertyChangeMask | StructureNotifyMask);
    }

    if (!screen_info)
    {
        g_free (new);
        myDisplayUngrabServer (display_info);
        TRACE ("Couldn't get screen from window, 0x%lx not added", id);
        return;
    }
    
    if (id == screen_info->xroot)
    {
        g_free (new);
        myDisplayUngrabServer (display_info);
        TRACE ("Not adding root window, 0x%lx not added", id);
        return;
    }
    
    new->c = c;
    new->screen_info = screen_info;
    new->id = id;
    new->damaged = FALSE;
    new->viewable = (new->attr.map_state == IsViewable);
#if HAVE_NAME_WINDOW_PIXMAP
    if (display_info->have_name_window_pixmap)
    {
        new->name_window_pixmap = XCompositeNameWindowPixmap (display_info->dpy, id);
    }               
    else
    {
        new->name_window_pixmap = None;
    }               
#endif
    new->picture = get_window_picture (new);
    
    if (new->attr.class == InputOnly)
    {
        new->damage = None;
    }
    else
    {
        new->damage = XDamageCreate (myScreenGetXDisplay (screen_info), id, XDamageReportNonEmpty);
    }
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
    new->opacity = get_opacity_prop (new, OPAQUE);
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
restack_win (ScreenInfo *screen_info, CWindow *cw, Window above)
{
    Window previous_above = None;
    GList *sibling;
    GList *next;

    TRACE ("entering restack_win");
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

static void
destroy_win (ScreenInfo *screen_info, Window id, gboolean gone)
{
    CWindow *cw;

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
compositorRepairScreen (ScreenInfo *screen_info)
{
    if (screen_info->allDamage != None)
    {
        paint_all (screen_info, screen_info->allDamage);
        screen_info->allDamage = None;
        screen_info->clipChanged = FALSE;
    }
}

static void
compositorDoRepair (DisplayInfo *display_info)
{
    GSList *screens;

    TRACE ("entering compositorDoRepair");    
    
    g_return_if_fail (display_info);
    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");    
        return;
    }
    
    for (screens = display_info->screens; screens; screens = g_slist_next (screens))
    {
        compositorRepairScreen ((ScreenInfo *) screens->data);
    }
}

static void
compositorHandleDamage (DisplayInfo *display_info, XDamageNotifyEvent *ev)
{
    CWindow *cw;
        
    TRACE ("entering compositorHandleDamage");    
    TRACE ("Damaged window: 0x%lx", ev->drawable);    
    cw = find_cwindow_in_display (display_info, ev->drawable);
    if (cw)
    {
        repair_win (cw);
        compositorRepairScreen (cw->screen_info);
    }
}

static void
compositorHandlePropertyNotify (DisplayInfo *display_info, XPropertyEvent *ev)
{
    gint p;
    Atom backgroundProps[2] = { misc_xrootpmap, misc_xsetroot };
    
    TRACE ("entering compositorHandlePropertyNotify");    
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
                compositorRepairScreen (screen_info);

                return;
            }
        }
    }
    
    /* check if Trans property was changed */
    if (ev->atom == net_wm_opacity)
    {
        CWindow *cw = find_cwindow_in_display (display_info, ev->window);
        TRACE ("Opacity property changed for id 0x%lx", ev->window);    

        if (cw)
        {
            TRACE ("Opacity changed for 0x%lx", cw->id);    
            cw->opacity = get_opacity_prop (cw, OPAQUE);
            determine_mode(cw);
            if (cw->shadow)
            {
                XRenderFreePicture (myScreenGetXDisplay (cw->screen_info), cw->shadow);
                cw->shadow = None;
                cw->extents = win_extents (cw);
                compositorRepairScreen (cw->screen_info);

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
    
    TRACE ("entering compositorHandleExpose");
    g_return_if_fail (display_info);
    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");    
        return;
    }
    
    /* Get the screen structure from the root of the event */
    screen_info = myDisplayGetScreenFromRoot (display_info, ev->window);

    if (!screen_info)
    {
        return;
    }

    rect[0].x = ev->x;
    rect[0].y = ev->y;
    rect[0].width = ev->width;
    rect[0].height = ev->height;
    
    expose_root (screen_info, rect, 1);
    compositorRepairScreen (screen_info);
}

static void
compositorHandleConfigureNotify (DisplayInfo *display_info, XConfigureEvent *ev)
{
    XserverRegion damage = None;
    CWindow *cw;
    
    TRACE ("entering compositorHandleConfigureNotify");    
    g_return_if_fail (display_info);
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
            compositorRepairScreen (screen_info);
        }

        return;
    }
    
    damage = XFixesCreateRegion (display_info->dpy, 0, 0);
    if (cw->extents != None)
    {
        XFixesCopyRegion (display_info->dpy, damage, cw->extents);
    }

    cw->attr.x = ev->x;
    cw->attr.y = ev->y;
    if ((cw->attr.width != ev->width) || (cw->attr.height != ev->height))
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

    cw->attr.width = ev->width;
    cw->attr.height = ev->height;
    cw->attr.border_width = ev->border_width;
    cw->attr.override_redirect = ev->override_redirect;
    restack_win (cw->screen_info, cw, ev->above);

    if (damage)
    {
        XserverRegion extents = win_extents (cw);
        XFixesUnionRegion (display_info->dpy, damage, damage, extents);
        XFixesDestroyRegion (display_info->dpy, extents);
        add_damage (cw->screen_info, damage);
    }
    cw->screen_info->clipChanged = TRUE;
    compositorRepairScreen (cw->screen_info);
}

static void
compositorHandleCirculateNotify (DisplayInfo *display_info, XCirculateEvent *ev)
{
    CWindow *cw;
    CWindow *top;
    GList *first;
    Window above = None;

    TRACE ("entering compositorHandleCirculateNotify");
    g_return_if_fail (display_info);
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
    restack_win (cw->screen_info, cw, above);
    cw->screen_info->clipChanged = TRUE;
    compositorRepairScreen (cw->screen_info);
}

#endif /* HAVE_COMPOSITOR */

void
compositorWindowMap (DisplayInfo *display_info, Window id)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    TRACE ("entering compositorWindowMap: 0x%lx", id);
    g_return_if_fail (display_info);
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
            compositorRepairScreen (cw->screen_info);
        }
    }
    else
    {
        add_win (display_info, id, NULL, None);
        compositorDoRepair (display_info);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorWindowUnmap (DisplayInfo *display_info, Window id)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    TRACE ("entering compositorWindowUnmap: 0x%lx", id);
    g_return_if_fail (display_info);
    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");    
        return;
    }
    
    cw = find_cwindow_in_display (display_info, id);
    if (cw)
    {
        unmap_win (cw);
        compositorRepairScreen (cw->screen_info);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorAddWindow (DisplayInfo *display_info, Window id, Client *c)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;

    TRACE ("entering compositorAddWindow: 0x%lx", id);
    g_return_if_fail (display_info);
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
        add_win (display_info, id, c, None);
        compositorDoRepair (display_info);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorRemoveWindow (DisplayInfo *display_info, Window id)
{
#ifdef HAVE_COMPOSITOR
    CWindow *cw;
    
    TRACE ("entering compositorRemoveWindow: 0x%lx", id);
    g_return_if_fail (display_info);
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
        compositorRepairScreen (screen_info);
    }
#endif /* HAVE_COMPOSITOR */
}

void
compositorHandleEvent (DisplayInfo *display_info, XEvent *ev)
{
#ifdef HAVE_COMPOSITOR
    TRACE ("entering compositorHandleEvent");    
    
    if (!(display_info->enable_compositor))
    {
        TRACE ("compositor disabled");    
        return;
    }
    
    else if (ev->type == ConfigureNotify)
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

void
compositorInitDisplay (DisplayInfo *display_info)
{
#ifdef HAVE_COMPOSITOR
    TRACE ("entering compositorInitDisplay");    
    
    if (!XCompositeQueryExtension (display_info->dpy,
                                &display_info->composite_event_base,
                                &display_info->composite_error_base))
    {
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
        display_info->have_fixes = FALSE;
        display_info->fixes_event_base = 0;
        display_info->fixes_error_base = 0;
    }
    else
    {
        display_info->have_fixes = TRUE;
    }

    display_info->enable_compositor = ((display_info->have_composite) 
                                    && (display_info->have_damage) 
                                    && (display_info->have_fixes));
                                    
#else /* HAVE_COMPOSITOR */
    display_info->enable_compositor = FALSE;
#endif /* HAVE_COMPOSITOR */
}

void
compositorManageScreen (ScreenInfo *screen_info)
{
#ifdef HAVE_COMPOSITOR
    DisplayInfo *display_info;
    XRenderPictureAttributes pa;
    XRenderPictFormat *visual_format;

    TRACE ("entering compositorManageScreen");    
    
    display_info = screen_info->display_info;

    if (!(display_info->enable_compositor))
    {
        return;
    }

    screen_info->gsize = -1;
    screen_info->gaussianMap = make_gaussian_map(SHADOW_RADIUS);
    presum_gaussian (screen_info);
        
    pa.subwindow_mode = IncludeInferiors;
    visual_format = XRenderFindVisualFormat (display_info->dpy, 
                                            DefaultVisual (display_info->dpy, 
                                                            screen_info->screen));
    screen_info->rootPicture = XRenderCreatePicture (display_info->dpy, screen_info->xroot, 
                                                visual_format, CPSubwindowMode, &pa);
    screen_info->rootBuffer = None;
    screen_info->blackPicture = solid_picture (screen_info, TRUE, 1, 0, 0, 0);
    screen_info->rootTile = None;
    screen_info->clipChanged = TRUE;
    screen_info->allDamage = None;
    screen_info->cwindows = NULL;
    screen_info->clipChanged = FALSE;
    
    XCompositeRedirectSubwindows (display_info->dpy, screen_info->xroot, CompositeRedirectManual);
    paint_all (screen_info, None);    

    XSync (display_info->dpy, FALSE);
#endif /* HAVE_COMPOSITOR */
}

void
compositorUnmanageScreen (ScreenInfo *screen_info)
{
#ifdef HAVE_COMPOSITOR  
    DisplayInfo *display_info;
    GList *index;
    gint i = 0;
    
    TRACE ("entering compositorUnmanageScreen");    
    
    display_info = screen_info->display_info;
    if (!(display_info->enable_compositor))
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
compositorDamageClient (Client *c)
{
#ifdef HAVE_COMPOSITOR
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    
    g_return_if_fail (c != NULL);

    TRACE ("entering compositorDamageClient");    
    
    screen_info = c->screen_info;
    display_info = screen_info->display_info;
    if (display_info->enable_compositor)
    {
        CWindow *cw = find_cwindow_in_screen (screen_info, c->frame);

        if (cw)
        {
            repair_win (cw);
            compositorRepairScreen (cw->screen_info);
        }
    }
#endif /* HAVE_COMPOSITOR */
}
