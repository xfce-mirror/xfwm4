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
 
        xfwm4    - (c) 2003 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <libxfce4util/debug.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include "mywindow.h"
#include "main.h"

void
myWindowCreate (Display * dpy, Window parent, myWindow * win, Cursor cursor)
{
    TRACE ("entering myWindowCreate");

    win->window = XCreateSimpleWindow (dpy, parent, 0, 0, 1, 1, 0, 0, 0);
    TRACE ("Created XID 0x%lx", win->window);
    if (cursor != None)
    {
        XDefineCursor (dpy, win->window, cursor);
    }
    win->map = FALSE;
    win->dpy = dpy;
    win->x = 0;
    win->y = 0;
    win->w = 1;
    win->h = 1;
}

void
myWindowDelete (myWindow * win)
{
    TRACE ("entering myWindowDelete");

    if (win->window != None)
    {
        XDestroyWindow (win->dpy, win->window);
        win->window = None;
    }
    win->map = FALSE;
}

void
myWindowShow (myWindow * win, int x, int y, int width, int height,
    gboolean refresh)
{
    TRACE ("entering myWindowShow");

    if (!(win->window))
    {
        return;
    }
    if ((width < 1) || (height < 1))
    {
        myWindowHide (win);
        return;
    }
    if (!(win->map))
    {
        XMapWindow (win->dpy, win->window);
        win->map = TRUE;
    }
    TRACE ("Showing XID 0x%lx", win->window);
    if (((x != win->x) || (y != win->y)) && ((width != win->w)
            || (height != win->h)))
    {
        XMoveResizeWindow (win->dpy, win->window, x, y, (unsigned int) width,
            (unsigned int) height);
        win->x = x;
        win->y = y;
        win->w = width;
        win->h = height;
    }
    else if ((x != win->x) || (y != win->y))
    {
        XMoveWindow (win->dpy, win->window, x, y);
        if (refresh)
        {
            XClearWindow (win->dpy, win->window);
        }
        win->x = x;
        win->y = y;
    }
    else if ((width != win->w) || (height != win->h))
    {
        XResizeWindow (win->dpy, win->window, (unsigned int) width,
            (unsigned int) height);
        win->w = width;
        win->h = height;
    }
    else if (refresh)
    {
        XClearWindow (win->dpy, win->window);
    }
}

void
myWindowHide (myWindow * win)
{
    TRACE ("entering myWindowHide");

    if (win->map)
    {
        XUnmapWindow (win->dpy, win->window);
        win->map = FALSE;
    }
}
