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
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "main.h"
#include "client.h"
#include "misc.h"
#include "debug.h"

static gint xgrabcount = 0;

void getMouseXY(Window w, int *x2, int *y2)
{
    Window w1, w2;
    gint x1, y1, m;

    DBG("entering getMouseXY\n");

    XQueryPointer(dpy, w, &w1, &w2, &x1, &y1, x2, y2, &m);
}

Window getMouseWindow(Window w)
{
    Window w1, w2;
    int x1, y1, x2, y2, m;

    DBG("entering getMouseWindow\n");

    XQueryPointer(dpy, w, &w1, &w2, &x1, &y1, &x2, &y2, &m);
    return w2;
}

GC createGC(Colormap cmap, char *col, int func, XFontStruct * font, int inc_sw)
{
    XGCValues gv;
    XColor xc1, xc2;
    GC gc;
    int mask;

    DBG("entering createGC\n");
    DBG("color=%s\n", col);

    mask = GCForeground | GCFunction;
    XAllocNamedColor(dpy, cmap, col, &xc1, &xc2);
    gv.foreground = xc2.pixel;
    gv.function = func;
    if(font)
    {
        gv.font = font->fid;
        mask = mask | GCFont;
    }
    if(inc_sw)
    {
        gv.subwindow_mode = IncludeInferiors;
        mask = mask | GCSubwindowMode;
    }
    gc = XCreateGC(dpy, XDefaultRootWindow(dpy), mask, &gv);
    return gc;
}

void sendClientMessage(Window w, Atom a, long x, int mask)
{
    XEvent ev;

    DBG("entering sendClientMessage\n");

    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = a;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = x;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, w, False, mask, &ev);
}

void MyXGrabServer(void)
{
    DBG("entering MyXGrabServer\n");
    if(xgrabcount == 0)
    {
        DBG("grabbing server\n");
        XGrabServer(dpy);
    }
    xgrabcount++;
    DBG("grabs : %i\n", xgrabcount);
}

void MyXUngrabServer(void)
{
    DBG("entering MyXUngrabServer\n");
    if(--xgrabcount < 0)        /* should never happen */
    {
        xgrabcount = 0;
    }
    if(xgrabcount == 0)
    {
        DBG("ungrabbing server\n");
        XUngrabServer(dpy);
    }
    DBG("grabs : %i\n", xgrabcount);
}

Window setTmpEventWin(long eventmask)
{
    Window w;
    
    XSetWindowAttributes attributes;
    attributes.event_mask = eventmask;
    attributes.override_redirect = True;
    w = XCreateWindow(dpy, root, 0, 0, XDisplayWidth(dpy, screen), XDisplayHeight(dpy, screen), 0, 0, InputOnly, CopyFromParent, CWEventMask, &attributes);
    XMapRaised(dpy, w);
    XSync (dpy, 0);
    return (w);
}

void removeTmpEventWin(Window w)
{
    XDestroyWindow (dpy, w);
}
