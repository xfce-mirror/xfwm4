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

/* THIS CODE IS NOT FINISHED YET, IT WON'T COMPILE, PLEASE BE PATIENT */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include "display.h"
#include "client.h"
#include "frame.h"

#ifdef HAVE_COMPOSITE_EXTENSIONS

#define SHADOW_OFFSET 3
#define FRAME_INTERVAL_MILLISECONDS ((int)(1000.0/40.0))

static void
compositorWindowFree (Client *c)
{
    Display *dpy;

    g_return_if_fail (c != NULL);
    g_return_if_fail (c->damage != NULL);
  
    dpy = clientGetXDisplay (c);
    g_return_if_fail (dpy != NULL);

    XDamageDestroy (dpy, c->damage);

#if HAVE_NAME_WINDOW_PIXMAP
    if (c->name_window_pixmap != None)
    {    
        XFreePixmap (dpy, c->name_window_pixmap);
    }
#endif
}

static void
compositorRemoveRepairIdle (DisplayInfo *display)
{
    if (display_info->repair_idle != 0)
    {
        g_source_remove (display_info->repair_idle);
        display_info->repair_idle = 0;
    }

    if (display_info->repair_timeout != 0)
    {
        g_source_remove (display_info->repair_timeout);
        display_info->repair_timeout = 0;
    }
}

static XserverRegion
compositorWindowExtents (Client *c)
{
    XRectangle r;
    Display *dpy;

    g_return_if_fail (c != NULL);

    dpy = clientGetXDisplay (c);
    g_return_if_fail (dpy != NULL);

    r.x = c->x;
    r.y = c->y;
    r.width = c->width;
    r.height = c->height;

    r.width += SHADOW_OFFSET;
    r.height += SHADOW_OFFSET;
  
    return XFixesCreateRegion (dpy, &r, 1);
}

static void
compositorWindowGetPaintBounds (Client *c, int *x, int *y, int *w, int *h)
{
    g_return_if_fail (c != NULL);

#ifdef HAVE_NAME_WINDOW_PIXMAP
    if (c->name_window_pixmap != None)
    {
        *x = frameX(c);
        *y = frameY(c);
        *w = frameWidth(c);
        *h = frameHeight(c);
    }
  else
#endif
    {
        *x = c->x + c->border_width;
        *y = c->y + c->border_width;
        *w = c->width;
        *h = c->height;
    }
}

static void
compositorPaintScreen (ScreenInfo *screen_info, XserverRegion damage_region)
{
    XserverRegion region;
    Picture buffer_picture;
    Pixmap buffer_pixmap;
    Display *dpy;
    XRenderPictFormat *format;
    GList *index;
    GC gc;
    gint screen_width;
    gint screen_height;
    Window xroot;

    dpy = myScreenGetXDisplay (screen_info);
    screen_width = gdk_screen_get_width (screen_info->gscr);
    screen_height = gdk_screen_get_height (screen_info->gscr);
    screen_number = screen_info->screen;
    xroot = screen_info->xroot;

    if (damage_region == None)
    {
        XRectangle  r;

        r.x = 0;
        r.y = 0;
        r.width = screen_width;
        r.height = screen_height;
        region = XFixesCreateRegion (dpy, &r, 1);
    }
    else
    {
        region = XFixesCreateRegion (dpy, NULL, 0);
        XFixesCopyRegion (dpy, region, damage_region);
    }

    
    buffer_pixmap = XCreatePixmap (dpy, xroot, screen_width, screen_height,
                                   DefaultDepth (dpy, screen_number));

    gc = XCreateGC (dpy, buffer_pixmap, 0, NULL);
    XSetForeground (dpy, gc, WhitePixel (dpy, screen_info->screen));
    XFixesSetGCClipRegion (dpy, gc, 0, 0, region);
    XFillRectangle (dpy, buffer_pixmap, gc, 0, 0, screen_width, screen_height);
    format = XRenderFindVisualFormat (dpy, DefaultVisual (dpy, screen_number));
    buffer_picture = XRenderCreatePicture (dpy, buffer_pixmap, format, 0, 0);
    XFixesSetPictureClipRegion (dpy, buffer_picture, 0, 0, region);

    for (index = screen_info->windows_stack; index; index = g_list_next (index))
    {
        XRenderColor shadow_color;
        Client *c = (Client *) index->data;
        int x, y, w, h;

        shadow_color.red = 0;
        shadow_color.green = 0;
        shadow_color.blue = 0;
        shadow_color.alpha = 0x90c0;

        if ((!c->viewable) || (c->picture == None))
        {
            continue;
        }
      
        if (c->last_painted_extents)
        {
            XFixesDestroyRegion (dpy, c->last_painted_extents);
        }
        c->last_painted_extents = compositorWindowExtents (c);

        compositorWindowGetPaintBounds (c, &x, &y, &w, &h);
        if (FLAG_TEST (c->xfwm_flags, XFWM_FLAG_MOVING_RESIZING))
        {
          
            XRenderComposite (dpy, PictOpOver, c->picture,
                              screen_info->trans_picture, buffer_picture,
                              0, 0, 0, 0, x, y, w, h);
        }
        else
        {
            /* Draw window normally */
            /* TBD: create a drop shadow */
            XRenderFillRectangle (dpy, PictOpOver, buffer_picture,
                                &shadow_color,
                                frameX(c) + SHADOW_OFFSET,
                                frameY(c) + SHADOW_OFFSET,
                                frameWidth(c), frameHeight(c));

            XRenderComposite (dpy, PictOpSrc, c->picture,
                              None, buffer_picture,
                              0, 0, 0, 0, x, y, w, h);
        }
    }

    /* Copy buffer to root window */
    XFixesSetPictureClipRegion (dpy, screen_info->root_picture, 0, 0, region);
    /* XFixesSetPictureClipRegion (dpy, buffer_picture, 0, 0, None); */
    XRenderComposite (dpy, PictOpSrc, buffer_picture, None,
                      screen_info->root_picture,
                      0, 0, 0, 0, 0, 0, screen_width, screen_height);
  
    XFixesDestroyRegion (dpy, region);
    XFreePixmap (dpy, buffer_pixmap);
    XRenderFreePicture (dpy, buffer_picture);
    XFreeGC (dpy, gc);
}

static void
compositorDoRepair (DisplayInfo *display_info)
{
    GSList *screens;

    g_return_val_if_fail (display_info, FALSE);
    
    for (screens = display_info->screens; screens; screens = g_slist_next (screens))
    {
        ScreenInfo *screen_info = (ScreenInfo *) screens->data;
        if (screen_info->damage_region != None)
        {
            compositorPaintScreen (screen_info, screen_info->damage_region);
            XFixesDestroyRegion (display_info->dpy, screen_info->damage_region);
            screen_info->damage_region = None;
        }
    }
    compositorRemoveRepairIdle (display_info);
}

static gboolean
compositorRepairIdleFunc (gpointer data)
{
    DisplayInfo *display_info = (DisplayInfo *) data;

    display_info->repair_idle = 0;
    compositorDoRepair (display_info);
  
    return FALSE;
}

static gboolean
compositorRepairTimeoutFunc (gpointer data)
{
    DisplayInfo *display_info = (DisplayInfo *) data;

    display_info->repair_timeout = 0;
    compositorDoRepair (display_info);
  
    return FALSE;
}

static void
compositorEnsureRepairIdle (DisplayInfo *display_info)
{
    if (display_info->repair_idle != 0)
    {
        return;
    }
    display_info->repair_idle = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                                                 compositorRepairIdleFunc, 
                                                 display_info, NULL);
    display_info->repair_timeout = g_timeout_add (FRAME_INTERVAL_MILLISECONDS,
                                                  compositorRepairTimeoutFunc, 
                                                  display_info));
}

static void
compositorMergeAndDestroyDamageRegion (ScreenInfo *screen_info, XserverRegion region)
{
    DisplayInfo *display_info = screen_info->display_info;

    if (screen_info->damage_region != None)
    {
        XFixesUnionRegion (display_info->dpy,
                           screen_info->damage_region,
                           region, screen_info->damage_region);
        XFixesDestroyRegion (display_info->dpy, region);
    }
    else
    {
        screen_info->damage_region = region;
    }

    compositorEnsureRepairIdle (display_info);
}

static void
compositorMergeDamageRegion (ScreenInfo *screen_info, XserverRegion region)
{
    DisplayInfo *display_info = screen_info->display_info;

    if (screen_info->damage_region != None)
    {
        screen_info->damage_region = XFixesCreateRegion (display_info->dpy, NULL, 0);
    }
    XFixesUnionRegion (display_info->dpy,
                     screen_info->damage_region,
                     region, screen_info->damage_region);

    compositorEnsureRepairIdle (display_info);
}

static void
compositorHandleDamageNotify (DisplayInfo *display_info, XDamageNotifyEvent *ev)
{
    Client *c;
    XserverRegion region;
  
    c = myDisplayGetClientFromWindow (display_info, ev->window, ANY);
    if (!c)
    {
        return;
    }

    region = XFixesCreateRegion (display_info->dpy, NULL, 0);

    gdk_error_trap_push ();
    XDamageSubtract (display_info->dpy, c->damage, None, region);
    gdk_error_trap_pop ();

    XFixesTranslateRegion (display_info->dpy, region, frameX (c), frameY (c));
  
    compositorMergeAndDestroyDamageRegion (c->screen_info, region);
}

static void
compositorHandleConfigureNotify (DisplayInfo *display_info, XConfigureEvent *ev)
{
    Client *c;
    XserverRegion region;
  
    c = myDisplayGetClientFromWindow (display_info, ev->window, ANY);
    if (!c)
    {
        return;
    }

    if (c->screen_info->xroot != event->event)
    {
        return;
    }
  
    if (c->last_painted_extents)
    {
        compositorMergeAndDestroyDamageRegion (screen_info, c->last_painted_extents);
        c->last_painted_extents = None;
    }

    region = compositorWindowExtents (c);
    compositorMergeDamageRegion (screen_info, region);
    XFixesDestroyRegion (display_info->dpy, region);
}

static void
compositorHandleExpose (DisplayInfo *display_info, XExposeEvent *ev)
{
    ScreenInfo *screen_info = NULL;
    XserverRegion region;
    XRectangle r;

    screen_info = myDisplayGetScreenFromRoot (display_info, ev->window);
    if (!(screen_info) || (screen_info->root_picture == None))
    {
        return;
    }

    r.x = 0;
    r.y = 0;
    r.width = gdk_screen_get_width (screen_info->gscr);;
    r.height = gdk_screen_get_height (screen_info->gscr);;
    region = XFixesCreateRegion (display_info->dpy, &r, 1);
  
    compositorMergeAndDestroyDamageRegion (screen_info, region);
}

static void
compositorHandleMap (DisplayInfo *display_info, XMapEvent *ev)
{
    ScreenInfo *screen_info = NULL;
    Client *c;

    screen_info = myDisplayGetScreenFromRoot (display_info, ev->event);
    if (!(screen_info) || (screen_info->root_picture == None))
    {
        return;
    }
  
    c = myDisplayGetClientFromWindow (display_info, ev->window, ANY);
    if (c)
    {
        c->viewable = TRUE;
    }
}

static void
compositorHandleUnmap (DisplayInfo *display_info, XUnmapEvent *ev)
{
    ScreenInfo *screen_info = NULL;
    Client *c;

    screen_info = myDisplayGetScreenFromRoot (display_info, ev->event);
    if (!(screen_info) || (screen_info->root_picture == None))
    {
        return;
    }
  
    c = myDisplayGetClientFromWindow (display_info, ev->window, ANY);
    if (c)
    {
        c->viewable = FALSE;

        if (c->last_painted_extents)
        {
            compositorMergeAndDestroyDamageRegion (screen_info, c->last_painted_extents);
            c->last_painted_extents = None;
        }
    }
}
#endif /* HAVE_COMPOSITE_EXTENSIONS */

void
compositorHandleEvent (DisplayInfo *display_info, XEvent *ev)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    if (!(display_info->enable_compositor))
    {
        return;
    }
    if (ev->type == (display_info->damage_event_base + XDamageNotify))
    {
        compositorHandleDamageNotify (display_info, (XDamageNotifyEvent*) ev);
    }
    else if (ev->type == ConfigureNotify)
    {
        compositorHandleConfigureNotify (display_info, (XConfigureEvent*) ev);
    }
    else if (ev->type == Expose)
    {
        compositorHandleExpose (display_info, (XExposeEvent*) ev);
    }
    else if (ev->type == UnmapNotify)
    {
        compositorHandleUnmap (display_info, (XUnmapEvent*) ev);
    }
    else if (ev->type == MapNotify)
    {
        compositorHandleMap (display_info, (XMapEvent*) ev);
    }
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
compositorAddWindow (Client *c, gboolean viewable)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    Damage damage;
    XRenderPictFormat *format;
    XRenderPictureAttributes pa;
    XserverRegion region;
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
 
    g_return_if_fail (c != NULL);

    if (!(display_info->enable_compositor))
    {
        return;
    }
    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    gdk_error_trap_push ();
    damage = XDamageCreate (display_info->dpy, c->window, XDamageReportNonEmpty);
    gdk_error_trap_pop ();

    if (damage == None)
    {
        return;
    }  

    c->damage = damage;
#if HAVE_NAME_WINDOW_PIXMAP
    if (display_info->have_name_window_pixmap)
    {
        gdk_error_trap_push ();
        c->name_window_pixmap = XCompositeNameWindowPixmap (display_info->dpy,
                                                            cwindow->window);
        gdk_error_trap_pop ();
    }
#endif /* HAVE_NAME_WINDOW_PIXMAP */
  
    /* viewable == mapped for the root window, since root can't be unmapped */
    c->viewable = viewable;
  
    pa.subwindow_mode = IncludeInferiors;

    format = XRenderFindVisualFormat (display_info->dpy, c->visual);
    c->picture = XRenderCreatePicture (display_info->dpy,
#if HAVE_NAME_WINDOW_PIXMAP
    c->picture = XRenderCreatePicture (display_info->dpy, 
                                       (c->pixmap != None ? c->pixmap : c->window),
                                       format, CPSubwindowMode, &pa);
#else
    c->picture = XRenderCreatePicture (display_info->dpy, c->window,
                                       format, CPSubwindowMode, &pa);
#endif /* HAVE_NAME_WINDOW_PIXMAP */
  
  region = compositorWindowExtents (c);
  compositorMergeAndDestroyDamageRegion (screen_info, region);
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
compositorRemoveWindow (Client *c)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
 
    g_return_if_fail (c != NULL);

    if (!(display_info->enable_compositor))
    {
        return;
    }
    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (c->last_painted_extents)
    {
        compositorMergeAndDestroyDamageRegion (screen_info,
                                               c->last_painted_extents);
        c->last_painted_extents = None;
    }  
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
compositorManageScreen (ScreenInfo *screen_info)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    DisplayInfo *display_info;
    XRenderPictureAttributes pa;
    XRectangle r;
    XRenderColor c;
    XRenderPictFormat *visual_format;
  
    if (!(display_info->enable_compositor))
    {
        return;
    }

    display_info = screen_info->display_info;

    XCompositeRedirectSubwindows (display_info->dpy, screen_info->xroot, CompositeRedirectManual);
    pa.subwindow_mode = IncludeInferiors;
    visual_format = XRenderFindVisualFormat (display_info->dpy, 
                                             DefaultVisual (display_info->dpy, 
                                                            screen_info->screen));
    screen_info->root_picture = XRenderCreatePicture (display_info->dpy, screen_info->xroot, 
                                                 visual_format, CPSubwindowMode, &pa);

    screen_info->trans_pixmap = XCreatePixmap (display_info->dpy, screen_info->xroot, 1, 1, 8);

    pa.repeat = True;
    visual_format = XRenderFindStandardFormat (display_info->dpy, PictStandardA8);
    screen_info->trans_picture = XRenderCreatePicture (display_info->dpy, screen_info->trans_pixmap, 
                                                       visual_format, CPRepeat, &pa);
  
    c.red = c.green = c.blue = 0;
    c.alpha = 0xc0c0;
    XRenderFillRectangle (display_info->dpy, PictOpSrc, screen_info->trans_picture, &c, 0, 0, 1, 1);
  
    /* Damage the whole screen */
    r.x = 0;
    r.y = 0;
    r.width = gdk_screen_get_width (screen_info->gscr);;
    r.height = gdk_screen_get_height (screen_info->gscr);;
    compositorMergeAndDestroyDamageRegion (screen_info, XFixesCreateRegion (display_info->dpy, &r, 1));
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
compositorUnmanageScreen (ScreenInfo *screen_info)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS  
    DisplayInfo *display_info;
    GList *index;
  
    if (!(display_info->enable_compositor))
    {
        return;
    }

    display_info = screen_info->display_info;

    XRenderFreePicture (display_info->dpy, screen_info->root_picture);
    screen_info->root_picture = None;
    XRenderFreePicture (display_info->dpy, screen_info->trans_picture);
    screen_info->trans_picture = None;
    XFreePixmap (display_info->dpy, screen_info->trans_pixmap);
    screen_info->trans_pixmap = None;
  
    for (index = screen_info->windows_stack; index; index = g_list_next (index))
    {
        Client *c = (Client *) index->data;
        compositorRemoveWindow (c);
    }
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}

void
compositorDamageClient (Client *c)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
    DisplayInfo *display_info;
    ScreenInfo *screen_info;
    Window xwindow;
  
    g_return_if_fail (c != NULL);

    if (!(display_info->enable_compositor))
    {
        return;
    }

    screen_info = c->screen_info;
    display_info = screen_info->display_info;

    if (screen_info->root_picture == None)
    {    
        return;
    }

    compositorMergeAndDestroyDamageRegion (screen_info, compositorWindowExtents (c));
#endif /* HAVE_COMPOSITE_EXTENSIONS */
}
