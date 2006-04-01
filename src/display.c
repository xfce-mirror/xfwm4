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
 
        xfwm4    - (c) 2002-2006 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/time.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libxfce4util/libxfce4util.h> 
#include <libxfcegui4/libxfcegui4.h>
#ifdef HAVE_RENDER
#include <X11/extensions/Xrender.h>
#endif

#include "spinning_cursor.h"
#include "display.h"
#include "screen.h"
#include "client.h"
#include "compositor.h"

static int
handleXError (Display * dpy, XErrorEvent * err)
{
#if DEBUG
    char buf[64];

    XGetErrorText (dpy, err->error_code, buf, 63);
    g_print ("XError: %s\n", buf);                                                  
    g_print ("==>  XID 0x%lx, Request %d, Error %d <==\n", 
              err->resourceid, err->request_code, err->error_code); 
#endif
    return 0;
}

static gboolean
myDisplayInitAtoms (DisplayInfo *display_info)
{
    static const char *atom_names[] = {
        "COMPOSITING_MANAGER",
        "GNOME_PANEL_DESKTOP_AREA",
        "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR",
        "KWM_WIN_ICON",
        "_MOTIF_WM_HINTS",
        "_NET_ACTIVE_WINDOW",
        "_NET_CLIENT_LIST",
        "_NET_CLIENT_LIST_STACKING",
        "_NET_CLOSE_WINDOW",
        "_NET_CURRENT_DESKTOP",
        "_NET_DESKTOP_GEOMETRY",
        "_NET_DESKTOP_LAYOUT",
        "_NET_DESKTOP_NAMES",
        "_NET_DESKTOP_VIEWPORT",
        "_NET_FRAME_EXTENTS",
        "_NET_NUMBER_OF_DESKTOPS",
        "_NET_REQUEST_FRAME_EXTENTS",
        "_NET_SHOWING_DESKTOP",
        "_NET_STARTUP_ID",
        "_NET_SUPPORTED",
        "_NET_SUPPORTING_WM_CHECK",
        "_NET_SYSTEM_TRAY_OPCODE",
        "_NET_WM_ACTION_CHANGE_DESKTOP",
        "_NET_WM_ACTION_CLOSE",
        "_NET_WM_ACTION_MAXIMIZE_HORZ",
        "_NET_WM_ACTION_MAXIMIZE_VERT",
        "_NET_WM_ACTION_MOVE",
        "_NET_WM_ACTION_RESIZE",
        "_NET_WM_ACTION_SHADE",
        "_NET_WM_ACTION_STICK",
        "_NET_WM_ALLOWED_ACTIONS",
        "_NET_WM_CONTEXT_HELP",
        "_NET_WM_DESKTOP",
        "_NET_WM_ICON",
        "_NET_WM_ICON_GEOMETRY",
        "_NET_WM_ICON_NAME",
        "_NET_WM_MOVERESIZE",
        "_NET_WM_NAME",
        "_NET_WM_WINDOW_OPACITY",
        "_NET_WM_STATE",
        "_NET_WM_STATE_ABOVE",
        "_NET_WM_STATE_BELOW",
        "_NET_WM_STATE_DEMANDS_ATTENTION",
        "_NET_WM_STATE_FULLSCREEN",
        "_NET_WM_STATE_HIDDEN",
        "_NET_WM_STATE_MAXIMIZED_HORZ",
        "_NET_WM_STATE_MAXIMIZED_VERT",
        "_NET_WM_STATE_MODAL",
        "_NET_WM_STATE_SHADED",
        "_NET_WM_STATE_SKIP_PAGER",
        "_NET_WM_STATE_SKIP_TASKBAR",
        "_NET_WM_STATE_STICKY",
        "_NET_WM_STRUT",
        "_NET_WM_STRUT_PARTIAL",
        "_NET_WM_USER_TIME",
        "_NET_WM_WINDOW_TYPE",
        "_NET_WM_WINDOW_TYPE_DESKTOP",
        "_NET_WM_WINDOW_TYPE_DIALOG",
        "_NET_WM_WINDOW_TYPE_DOCK",
        "_NET_WM_WINDOW_TYPE_MENU",
        "_NET_WM_WINDOW_TYPE_NORMAL",
        "_NET_WM_WINDOW_TYPE_SPLASH",
        "_NET_WM_WINDOW_TYPE_TOOLBAR",
        "_NET_WM_WINDOW_TYPE_UTILITY",
        "_NET_WORKAREA",
        "MANAGER",
        "PIXMAP",
        "SM_CLIENT_ID",
        "UTF8_STRING",
        "_WIN_CLIENT_LIST",
        "_WIN_DESKTOP_BUTTON_PROXY",
        "_WIN_HINTS",
        "_WIN_LAYER",
        "_WIN_PROTOCOLS",
        "_WIN_STATE",
        "_WIN_SUPPORTING_WM_CHECK",
        "_WIN_WORKSPACE",
        "_WIN_WORKSPACE_COUNT",
        "WM_CHANGE_STATE",
        "WM_CLIENT_LEADER",
        "WM_COLORMAP_WINDOWS",
        "WM_DELETE_WINDOW",
        "WM_HINTS",
        "WM_PROTOCOLS",
        "WM_STATE",
        "WM_TAKE_FOCUS",
        "WM_TRANSIENT_FOR",
        "WM_WINDOW_ROLE",
        "_XROOTPMAP_ID",
        "_XSETROOT_ID"
    };
    
    g_assert (NB_ATOMS == G_N_ELEMENTS (atom_names));
    return (XInternAtoms (display_info->dpy, 
                          (char **) atom_names, 
                          NB_ATOMS,
                          FALSE, display_info->atoms) != 0);
}

DisplayInfo *
myDisplayInit (GdkDisplay *gdisplay)
{
    DisplayInfo *display;
    int dummy;

    display = g_new0 (DisplayInfo, 1);

    display->gdisplay = gdisplay;
    display->dpy = (Display *) gdk_x11_display_get_xdisplay (gdisplay);

    XSetErrorHandler (handleXError);

    /* Initialize internal atoms */
    if (!myDisplayInitAtoms (display))
    {
        g_warning ("Some internal atoms were not properly created.");
    }

    /* Test XShape extension support */
    if (XShapeQueryExtension (display->dpy, 
                              &display->shape_event_base, 
                              &dummy))
    {
        display->have_shape = TRUE;
    }
    else
    {
        g_warning ("The display does not support the XShape extension.");
        display->have_shape = FALSE;
        display->shape_event_base = 0;
    }

#ifdef HAVE_RENDER
    if (XRenderQueryExtension (display->dpy,
                               &display->render_event_base,
                               &display->render_error_base))
    {
        display->have_render = TRUE;
    }
    else
    {
        g_warning ("The display does not support the XRender extension.");
        display->have_render = FALSE;
        display->render_event_base = 0;
        display->render_error_base = 0;
    }
#else
    display->have_render = FALSE;
#endif

#ifdef HAVE_RANDR
    if (XRRQueryExtension (display->dpy,
                            &display->xrandr_event_base, 
                            &display->xrandr_error_base))
    {
        display->have_xrandr = TRUE;
    }
    else
    {
        g_warning ("The display does not support the XRandr extension.");
        display->have_xrandr = FALSE;
        display->xrandr_event_base = 0;
        display->xrandr_error_base = 0;
    }
#else
    display->have_xrandr = FALSE;
#endif

    display->root_cursor = 
        XCreateFontCursor (display->dpy, XC_left_ptr);
    display->move_cursor = 
        XCreateFontCursor (display->dpy, XC_fleur);
    display->busy_cursor = 
        cursorCreateSpinning (display->dpy);
    display->resize_cursor[CORNER_TOP_LEFT] =
        XCreateFontCursor (display->dpy, XC_top_left_corner);
    display->resize_cursor[CORNER_TOP_RIGHT] =
        XCreateFontCursor (display->dpy, XC_top_right_corner);
    display->resize_cursor[CORNER_BOTTOM_LEFT] =
        XCreateFontCursor (display->dpy, XC_bottom_left_corner);
    display->resize_cursor[CORNER_BOTTOM_RIGHT] =
        XCreateFontCursor (display->dpy, XC_bottom_right_corner);
    display->resize_cursor[4 + SIDE_LEFT] = 
        XCreateFontCursor (display->dpy, XC_left_side);
    display->resize_cursor[4 + SIDE_RIGHT] = 
        XCreateFontCursor (display->dpy, XC_right_side);
    display->resize_cursor[4 + SIDE_TOP] = 
        XCreateFontCursor (display->dpy, XC_top_side);
    display->resize_cursor[4 + SIDE_BOTTOM] = 
        XCreateFontCursor (display->dpy, XC_bottom_side);

    display->xfilter = NULL;
    display->screens = NULL;
    display->clients = NULL;
    display->xgrabcount = 0;
    display->dbl_click_time = 300;
    display->nb_screens = 0;
    display->current_time = CurrentTime;
    
    compositorInitDisplay (display);

    return display;
}

DisplayInfo *
myDisplayClose (DisplayInfo *display)
{
    int i;

    XFreeCursor (display->dpy, display->busy_cursor);
    display->busy_cursor = None;
    XFreeCursor (display->dpy, display->move_cursor);
    display->move_cursor = None;
    XFreeCursor (display->dpy, display->root_cursor);
    display->root_cursor = None;
    
    for (i = 0; i < 7; i++)
    {
        XFreeCursor (display->dpy, display->resize_cursor[i]);
        display->resize_cursor[i] = None;
    }
    
    g_slist_free (display->clients);
    display->clients = NULL;
    
    g_slist_free (display->screens);
    display->screens = NULL;

    return display;
}

gboolean
myDisplayHaveShape (DisplayInfo *display)
{
    g_return_val_if_fail (display != NULL, FALSE);
    
    return (display->have_shape);
}

gboolean
myDisplayHaveRender (DisplayInfo *display)
{
    g_return_val_if_fail (display != NULL, FALSE);
    
    return (display->have_render);
}

Cursor 
myDisplayGetCursorBusy (DisplayInfo *display)
{
    g_return_val_if_fail (display, None);
    
    return display->busy_cursor;
}

Cursor 
myDisplayGetCursorMove  (DisplayInfo *display)
{
    g_return_val_if_fail (display, None);
    
    return display->move_cursor;
}

Cursor 
myDisplayGetCursorRoot (DisplayInfo *display)
{
    g_return_val_if_fail (display, None);
    
    return display->root_cursor;
}

Cursor 
myDisplayGetCursorResize (DisplayInfo *display, guint index)
{
    g_return_val_if_fail (display, None);
    g_return_val_if_fail (index < 8, None);

    return display->resize_cursor [index];
}


void
myDisplayGrabServer (DisplayInfo *display)
{
    g_return_if_fail (display);
    
    DBG ("entering myDisplayGrabServer");
    if (display->xgrabcount == 0)
    {
        DBG ("grabbing server");
        XGrabServer (display->dpy);
    }
    display->xgrabcount++;
    DBG ("grabs : %i", display->xgrabcount);
}

void
myDisplayUngrabServer (DisplayInfo *display)
{
    DBG ("entering myDisplayUngrabServer");
    display->xgrabcount = display->xgrabcount - 1;
    if (display->xgrabcount < 0)       /* should never happen */
    {
        display->xgrabcount = 0;
    }
    if (display->xgrabcount == 0)
    {
        DBG ("ungrabbing server");
        XUngrabServer (display->dpy);
        XFlush (display->dpy);
    }
    DBG ("grabs : %i", display->xgrabcount);
}

void 
myDisplayAddClient (DisplayInfo *display, Client *c)
{
    g_return_if_fail (c != None);
    g_return_if_fail (display != NULL);

    display->clients = g_slist_append (display->clients, c);
}

void 
myDisplayRemoveClient (DisplayInfo *display, Client *c)
{
    g_return_if_fail (c != None);
    g_return_if_fail (display != NULL);

    display->clients = g_slist_remove (display->clients, c);
}

Client *
myDisplayGetClientFromWindow (DisplayInfo *display, Window w, int mode)
{
    GSList *index;

    g_return_val_if_fail (w != None, NULL);
    g_return_val_if_fail (display != NULL, NULL);

    for (index = display->clients; index; index = g_slist_next (index))
    {
        Client *c = (Client *) index->data;
        switch (mode)
        {
            case WINDOW:
                if (c->window == w)
                {
                    TRACE ("found \"%s\" (mode WINDOW)", c->name);
                    return (c);
                }
                break;
            case FRAME:
                if (c->frame == w)
                {
                    TRACE ("found \"%s\" (mode FRAME)", c->name);
                    return (c);
                }
                break;
            case ANY:
            default:
                if ((c->frame == w) || (c->window == w))
                {
                    TRACE ("found \"%s\" (mode ANY)", c->name);
                    return (c);
                }
                break;
        }
    }
    TRACE ("no client found");

    return NULL;
}

void 
myDisplayAddScreen (DisplayInfo *display, ScreenInfo *screen)
{
    g_return_if_fail (screen != NULL);
    g_return_if_fail (display != NULL);

    display->screens = g_slist_append (display->screens, screen);
    display->nb_screens = display->nb_screens + 1;
}

void 
myDisplayRemoveScreen (DisplayInfo *display, ScreenInfo *screen)
{
    g_return_if_fail (screen != NULL);
    g_return_if_fail (display != NULL);

    display->screens = g_slist_remove (display->screens, screen);
    display->nb_screens = display->nb_screens - 1;
    if (display->nb_screens < 0)
    {
        display->nb_screens = 0;
    }
}

ScreenInfo *
myDisplayGetScreenFromRoot (DisplayInfo *display, Window root)
{
    GSList *index;

    g_return_val_if_fail (root != None, NULL);
    g_return_val_if_fail (display != NULL, NULL);

    for (index = display->screens; index; index = g_slist_next (index))
    {
        ScreenInfo *screen = (ScreenInfo *) index->data;
        if (screen->xroot == root)
        {
            return screen;
        }
    }
    TRACE ("myDisplayGetScreenFromRoot: no screen found");

    return NULL;
}

ScreenInfo *
myDisplayGetScreenFromNum (DisplayInfo *display, int num)
{
    GSList *index;

    g_return_val_if_fail (display != NULL, NULL);

    for (index = display->screens; index; index = g_slist_next (index))
    {
        ScreenInfo *screen = (ScreenInfo *) index->data;
        if (screen->screen == num)
        {
            return screen;
        }
    }
    TRACE ("myDisplayGetScreenFromNum: no screen found");

    return NULL;
}

Window
myDisplayGetRootFromWindow(DisplayInfo *display, Window w)
{
    XWindowAttributes attributes;
    gint test;

    g_return_val_if_fail (w != None, None);
    g_return_val_if_fail (display != NULL, None);

    gdk_error_trap_push ();
    XGetWindowAttributes(display->dpy, w, &attributes);
    test = gdk_error_trap_pop ();

    if (test)
    {
        TRACE ("myDisplayGetRootFromWindow: no root found for 0x%lx, error code %i", w, test);
        return None;
    }
    return attributes.root;
}

ScreenInfo *
myDisplayGetScreenFromWindow (DisplayInfo *display, Window w)
{
    Window root;

    g_return_val_if_fail (w != None, NULL);
    g_return_val_if_fail (display != NULL, NULL);

    root = myDisplayGetRootFromWindow (display, w);
    if (root != None)
    {
        return myDisplayGetScreenFromRoot (display, root);
    }
    TRACE ("myDisplayGetScreenFromWindow: no screen found");

    return NULL;
}

#ifdef ENABLE_KDE_SYSTRAY_PROXY
ScreenInfo *
myDisplayGetScreenFromSystray (DisplayInfo *display, Window w)
{
    GSList *index;

    g_return_val_if_fail (w != None, NULL);
    g_return_val_if_fail (display != NULL, NULL);

    for (index = display->screens; index; index = g_slist_next (index))
    {
        ScreenInfo *screen = (ScreenInfo *) index->data;
        if (screen->systray == w)
        {
            return screen;
        }
    }
    TRACE ("myDisplayGetScreenFromSystray: no screen found");

    return NULL;
}
#endif

ScreenInfo *
myDisplayGetDefaultScreen (DisplayInfo *display)
{
    GSList *index;
    ScreenInfo *screen = NULL;

    g_return_val_if_fail (display != NULL, NULL);

    index = display->screens;
    if (index)
    {
        screen = (ScreenInfo *) index->data;
    }

    return screen;
}

Time 
myDisplayUpdateCurentTime (DisplayInfo *display, XEvent *ev)
{
    g_return_val_if_fail (display != NULL, (Time) CurrentTime);
    
    switch (ev->type)
    {
        case KeyPress:
        case KeyRelease:
            display->current_time = (Time) ev->xkey.time;
            break;
        case ButtonPress:
        case ButtonRelease:
            display->current_time = (Time) ev->xbutton.time;
            break;
        case MotionNotify:
            display->current_time = (Time) ev->xmotion.time;
            break;
        case EnterNotify:
        case LeaveNotify:
            display->current_time = (Time) ev->xcrossing.time;
            break;
        case PropertyNotify:
            display->current_time = (Time) ev->xproperty.time;
            break;
        case SelectionClear:
            display->current_time = (Time) ev->xselectionclear.time;
            break;
        case SelectionRequest:
            display->current_time = (Time) ev->xselectionrequest.time;
            break;
        case SelectionNotify:
            display->current_time = (Time) ev->xselection.time;
            break;
        default:
            break;
    }
    return display->current_time;
}

Time 
myDisplayGetCurrentTime (DisplayInfo *display)
{
    g_return_val_if_fail (display != NULL, (Time) CurrentTime);

    return (Time) display->current_time;
}

static gdouble
get_time(void)
{
    struct timeval timev;

    gettimeofday(&timev, NULL);
    return (gdouble)timev.tv_sec + (((gdouble)timev.tv_usec) / 1000000);
}

gboolean
myDisplayTestXrender (DisplayInfo *display, gdouble min_time)
{
#ifdef HAVE_RENDER
    gdouble t1, t2, dt;
    Display *dpy;
    Picture picture1, picture2, picture3;
    XRenderPictFormat *format_src, *format_dst;
    Pixmap fillPixmap, rootPixmap;
    XRenderPictureAttributes pa;
    XSetWindowAttributes attrs;
    XImage *ximage;
    Window output;
    XRenderColor c;
    Visual *visual;
    Screen *screen;
    int x, y, w, h;
    int screen_number;
    int depth;
    int iterations;

    g_return_val_if_fail (display != NULL, FALSE);
    TRACE ("entering myDisplayTesxrender");

    c.alpha = 0x7FFF;
    c.red   = 0xFFFF;
    c.green = 0xFFFF;
    c.blue  = 0xFFFF;

    dpy = display->dpy;
    screen_number = DefaultScreen (dpy);
    screen = DefaultScreenOfDisplay (dpy);
    visual = DefaultVisual (dpy, screen_number);
    depth = DefaultDepth (dpy, screen_number);

    w = WidthOfScreen(screen) / 16;
    h = HeightOfScreen(screen) / 16;
    x = (WidthOfScreen(screen) - w);
    y = (HeightOfScreen(screen) - h);

    format_dst = XRenderFindVisualFormat (dpy, visual);
    g_return_val_if_fail (format_dst != NULL , FALSE);

    format_src = XRenderFindStandardFormat (dpy, PictStandardA8);
    g_return_val_if_fail (format_src != NULL , FALSE);

    ximage = XGetImage (dpy, 
                        DefaultRootWindow(dpy),
                        x, y, w, h,
                        AllPlanes, ZPixmap);
    g_return_val_if_fail (ximage != NULL , FALSE);

    rootPixmap = XCreatePixmap (dpy,
                                DefaultRootWindow(dpy),
                                w, h, depth);
    XPutImage (dpy, rootPixmap, 
               DefaultGC (dpy, screen_number), ximage,
               0, 0, 0, 0, w, h);
    XDestroyImage (ximage);

    attrs.override_redirect = TRUE;
    output = XCreateWindow (dpy,
                            DefaultRootWindow(dpy),
                            x, y, w, h, 
                            0, CopyFromParent, CopyFromParent,
                            (Visual *) CopyFromParent,
                            CWOverrideRedirect, &attrs);
    XMapRaised (dpy, output);

    fillPixmap = XCreatePixmap (dpy,
                                DefaultRootWindow(dpy),
                                1, 1, 8);

    t1 = get_time();
    
    pa.repeat = TRUE;
    picture1 = XRenderCreatePicture (dpy, 
                                     rootPixmap,
                                     format_dst, 0, NULL);
    picture2 = XRenderCreatePicture (dpy, 
                                     fillPixmap,
                                     format_src, CPRepeat, &pa);
    picture3 = XRenderCreatePicture (dpy, 
                                     output,
                                     format_dst, 0, NULL);
    XRenderComposite (dpy, PictOpSrc, 
                    picture1, None, picture3, 
                    0, 0, 0, 0, 0, 0, w, h);
    XRenderFillRectangle (dpy, PictOpSrc,
                          picture2, &c, 0, 0, 
                          1, 1);
    for (iterations = 0; iterations < 10; iterations++)
    {
        XRenderComposite (dpy, PictOpOver, 
                        picture1, picture2, picture3, 
                        0, 0, 0, 0, 0, 0, w, h);
        ximage = XGetImage (dpy, output, 
                            0, 0, 1, 1, 
                            AllPlanes, ZPixmap);
        if (ximage)
        {
                XDestroyImage (ximage);
        }
    }
    XRenderFreePicture (dpy, picture1);
    XRenderFreePicture (dpy, picture2);
    XRenderFreePicture (dpy, picture3);

    XFreePixmap (dpy, fillPixmap);
    XFreePixmap (dpy, rootPixmap);

    XDestroyWindow (dpy, output);

    t2 = get_time();
    dt = t2 - t1;
    TRACE ("Test duration: %3.6f sec.", dt);

    return (gboolean) (dt < min_time);
#else  /* HAVE_RENDER */
    return FALSE;
#endif /* HAVE_RENDER */
}
