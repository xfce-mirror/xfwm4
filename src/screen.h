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
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifndef INC_MAIN_H
#define INC_MAIN_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libxfcegui4/libxfcegui4.h>

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif

#include "mywindow.h"

typedef struct ScreenData
{
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    SnMonitorContext *sn_context;
    GSList *startup_sequences;
    guint startup_sequence_timeout;
#endif
    Colormap cmap;
    Cursor busy_cursor;
    Cursor move_cursor;
    Cursor resize_cursor[7];
    Cursor root_cursor;
    Display *dpy;
    GdkDisplay *gdisplay;
    GdkScreen *gscr;
    XfceFilterSetup *xfilter;
    Screen *xscreen;
    Window gnome_win;
    Window xroot;
    myWindow sidewalk[2];
    Window systray;
    int depth;
    int gnome_margins[4];
    int margins[4];
    int quit;
    int reload;
    int screen;
    int shape;
    int shape_event;
    int current_ws;
} 
ScreenData;

#endif /* INC_SCREEN_H */
