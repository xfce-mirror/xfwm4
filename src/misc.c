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
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <libxfce4util/libxfce4util.h> 

#include "display.h"
#include "screen.h"
#include "mywindow.h"
#include "client.h"
#include "misc.h"

void
getMouseXY (ScreenInfo *screen_info, Window w, int *x2, int *y2)
{
    Window w1, w2;
    gint x1, y1, m;

    TRACE ("entering getMouseXY");

    XQueryPointer (myScreenGetXDisplay (screen_info), w, &w1, &w2, &x1, &y1, x2, y2, &m);
}

Window
getMouseWindow (ScreenInfo *screen_info, Window w)
{
    Window w1, w2;
    int x1, y1, x2, y2, m;

    TRACE ("entering getMouseWindow");

    XQueryPointer (myScreenGetXDisplay (screen_info), w, &w1, &w2, &x1, &y1, &x2, &y2, &m);
    return w2;
}

GC
createGC (ScreenInfo *screen_info, char *col, int func, XFontStruct * font,
    int line_width, gboolean inc_sw)
{
    XGCValues gv;
    XColor xc1, xc2;
    GC gc;
    int mask;

    TRACE ("entering createGC");
    TRACE ("color=%s", col);

    mask = GCForeground | GCFunction;
    XAllocNamedColor (myScreenGetXDisplay (screen_info), screen_info->cmap, col, &xc1, &xc2);
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
    gc = XCreateGC (myScreenGetXDisplay (screen_info), screen_info->xroot, mask, &gv);
    return gc;
}

void
sendClientMessage (ScreenInfo *screen_info, Window w, Atom a, Atom x, Time timestamp)
{
    XClientMessageEvent ev;

    TRACE ("entering sendClientMessage");

    ev.type = ClientMessage;
    ev.window = w;
    ev.message_type = a;
    ev.format = 32;
    ev.data.l[0] = x;
    ev.data.l[1] = timestamp;
    XSendEvent (myScreenGetXDisplay (screen_info), w, FALSE, 0L, (XEvent *)&ev);
}

/*
 * it's safer to grab the display before calling this routine
 * Returns true if the given window is present and mapped on root 
 */
gboolean
checkWindowOnRoot(ScreenInfo *screen_info, Window w)
{
    DisplayInfo *display_info = NULL;
    Window dummy_root, parent;
    Window *wins = NULL;
    unsigned int count;
    Status test;
    
    g_return_val_if_fail (screen_info != NULL, FALSE);
    g_return_val_if_fail (w != None, FALSE);

    display_info = screen_info->display_info;
    gdk_error_trap_push ();
    test = XQueryTree(display_info->dpy, w, &dummy_root, &parent, &wins, &count);
    if (wins)
    {
        XFree (wins);
    }
    return (!gdk_error_trap_pop () && (test != 0) && (dummy_root == parent));
}

void
placeSidewalks(ScreenInfo *screen_info, gboolean activate)
{
    g_return_if_fail (MYWINDOW_XWINDOW (screen_info->sidewalk[0]) != None);
    g_return_if_fail (MYWINDOW_XWINDOW (screen_info->sidewalk[1]) != None);

    if (activate)
    {
        xfwmWindowShow (&screen_info->sidewalk[0], 
                      0, 0,
                      1, gdk_screen_get_height (screen_info->gscr), FALSE);
        xfwmWindowShow (&screen_info->sidewalk[1],
                      gdk_screen_get_width (screen_info->gscr) - 1, 0, 
                      1, gdk_screen_get_height (screen_info->gscr), FALSE);
    }
    else
    {
        /* Place the windows off screen */
        xfwmWindowShow (&screen_info->sidewalk[0], 
                      -1, 0,
                       1, gdk_screen_get_height (screen_info->gscr), FALSE);
        xfwmWindowShow (&screen_info->sidewalk[1],
                      gdk_screen_get_width (screen_info->gscr), 0, 
                      1, gdk_screen_get_height (screen_info->gscr), FALSE);
    }
}
