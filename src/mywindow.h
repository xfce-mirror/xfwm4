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
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2003 Olivier Fourdan
 
 */

#ifndef INC_MYWINDOW_H
#define INC_MYWINDOW_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <glib.h>

#define MYWINDOW_XWINDOW(w) (w.window)

typedef struct _myWindow myWindow;
struct _myWindow
{
    Display *dpy;
    Window window;
    int x, y;
    int w, h;
    gboolean map;
};

void myWindowCreate (Display *, Window, myWindow *, Cursor);
void myWindowDelete (myWindow *);
void myWindowShow (myWindow *, int, int, int, int, gboolean);
void myWindowHide (myWindow *);

#endif /* INC_MYWINDOW_H */
