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

#ifdef GDK_MULTIHEAD_SAFE
#undef GDK_MULTIHEAD_SAFE
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <libxfce4util/debug.h>
#include <libxfcegui4/xinerama.h>

#include "main.h"
#include "client.h"
#include "misc.h"

static int xgrabcount = 0;

void
getMouseXY (Window w, int *x2, int *y2)
{
    Window w1, w2;
    gint x1, y1, m;

    TRACE ("entering getMouseXY");

    XQueryPointer (dpy, w, &w1, &w2, &x1, &y1, x2, y2, &m);
}

Window
getMouseWindow (Window w)
{
    Window w1, w2;
    int x1, y1, x2, y2, m;

    TRACE ("entering getMouseWindow");

    XQueryPointer (dpy, w, &w1, &w2, &x1, &y1, &x2, &y2, &m);
    return w2;
}

GC
createGC (Colormap cmap, char *col, int func, XFontStruct * font,
    int line_width, gboolean inc_sw)
{
    XGCValues gv;
    XColor xc1, xc2;
    GC gc;
    int mask;

    TRACE ("entering createGC");
    TRACE ("color=%s", col);

    mask = GCForeground | GCFunction;
    XAllocNamedColor (dpy, cmap, col, &xc1, &xc2);
    gv.foreground = xc2.pixel;
    gv.function = func;
    if (font)
    {
        gv.font = font->fid;
        mask = mask | GCFont;
    }
    if (inc_sw)
    {
        gv.subwindow_mode = IncludeInferiors;
        mask = mask | GCSubwindowMode;
    }
    if (line_width > -1)
    {
        gv.line_width = line_width;
        mask = mask | GCLineWidth;
    }
    gc = XCreateGC (dpy, XDefaultRootWindow (dpy), mask, &gv);
    return gc;
}

void
sendClientMessage (Window w, Atom a, long x, int mask)
{
    XEvent ev;

    TRACE ("entering sendClientMessage");

    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = a;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = x;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent (dpy, w, FALSE, mask, &ev);
}

void
MyXGrabServer (void)
{
    DBG ("entering MyXGrabServer");
    if (xgrabcount == 0)
    {
        DBG ("grabbing server");
        XGrabServer (dpy);
    }
    xgrabcount++;
    DBG ("grabs : %i", xgrabcount);
}

void
MyXUngrabServer (void)
{
    DBG ("entering MyXUngrabServer");
    if (--xgrabcount < 0)       /* should never happen */
    {
        xgrabcount = 0;
    }
    if (xgrabcount == 0)
    {
        DBG ("ungrabbing server");
        XUngrabServer (dpy);
        XFlush (dpy);
    }
    DBG ("grabs : %i", xgrabcount);
}

gboolean
MyCheckWindow(Window w)
{
    Window dummy_root;
    unsigned int dummy_width, dummy_height, dummy_depth, dummy_bw;
    int dummy_x, dummy_y;
    Status test;
    
    g_return_val_if_fail (w != None, FALSE);

    gdk_error_trap_push ();
    test = XGetGeometry (dpy, w, &dummy_root, &dummy_x, &dummy_y,
                         &dummy_width, &dummy_height, &dummy_bw, &dummy_depth);

    return (!gdk_error_trap_pop () && (test != 0));
}

Window
setTmpEventWin (int x, int y, unsigned int w, unsigned int h, long eventmask)
{
    Window win;
    XSetWindowAttributes attributes;

    attributes.event_mask = eventmask;
    attributes.override_redirect = TRUE;
    win = XCreateWindow (dpy, root, x, y, w, h, 0, 0, 
                         InputOnly, CopyFromParent,
                         CWEventMask | CWOverrideRedirect, &attributes);
    XMapRaised (dpy, win);
    XFlush (dpy);
    return (win);
}

void
removeTmpEventWin (Window w)
{
    XDestroyWindow (dpy, w);
}

void
placeSidewalks(gboolean activate)
{
    g_return_if_fail (sidewalk[0] != None);
    g_return_if_fail (sidewalk[1] != None);

    if (activate)
    {
        XMoveResizeWindow(dpy, sidewalk[0], 
                          0, 0,
                          1, MyDisplayFullHeight (dpy, screen));
        XMoveResizeWindow(dpy, sidewalk[1],
                          MyDisplayFullWidth (dpy, screen) - 1, 0, 
                          1, MyDisplayFullHeight (dpy, screen));
    }
    else
    {
        /* Place the windows off screen */
        XMoveResizeWindow(dpy, sidewalk[0], 
                          -1, 0,
                          1, MyDisplayFullHeight (dpy, screen));
        XMoveResizeWindow(dpy, sidewalk[1],
                          MyDisplayFullWidth (dpy, screen), 0, 
                          1, MyDisplayFullHeight (dpy, screen));
    }
}

