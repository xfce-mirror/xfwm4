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
 
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libxfce4util/libxfce4util.h> 
#include <libxfcegui4/libxfcegui4.h>
#include "spinning_cursor.h"
#include "display.h"
#include "screen.h"
#include "client.h"

static int
handleXError (Display * dpy, XErrorEvent * err)
{
#if DEBUG            
    char buf[64];

    XGetErrorText (dpy, err->error_code, buf, 63);
    g_print ("XError: %s\n", buf);                                                  
    g_print ("==>  XID Ox%lx, Request %d, Error %d <==\n", 
              err->resourceid, err->request_code, err->error_code); 
#endif
    return 0;
}

DisplayInfo *
myDisplayInit (GdkDisplay *gdisplay)
{
    DisplayInfo *display;
    int dummy;
    
    display = g_new0 (DisplayInfo, 1);
    
    display->gdisplay = gdisplay;
    display->dpy = (Display *) gdk_x11_display_get_xdisplay (gdisplay);

    display->shape = 
        XShapeQueryExtension (display->dpy, &display->shape_event, &dummy);
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
    display->resize_cursor[4 + SIDE_BOTTOM] = 
        XCreateFontCursor (display->dpy, XC_bottom_side);

    display->xfilter = NULL;
    display->screens = NULL;
    display->clients = NULL;
    display->xgrabcount = 0;
    display->dbl_click_time = 300;

    initICCCMHints (display->dpy);
    initMotifHints (display->dpy);
    initGnomeHints (display->dpy);
    initNetHints   (display->dpy);
    initKDEHints   (display->dpy);

    XSetErrorHandler (handleXError);
    
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
    g_return_val_if_fail (index < 7, None);

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
}

void 
myDisplayRemoveScreen (DisplayInfo *display, ScreenInfo *screen)
{
    g_return_if_fail (screen != NULL);
    g_return_if_fail (display != NULL);

    display->screens = g_slist_remove (display->screens, screen);
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
    GSList *index;
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

