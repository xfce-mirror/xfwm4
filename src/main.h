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

#ifndef INC_MAIN_H
#define INC_MAIN_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libxfcegui4/libxfcegui4.h>
#include "client.h"
#include "misc.h"
#include "hints.h"
#include "keyboard.h"
#include "mypixmap.h"
#include "parserc.h"

/*
 *
 * Global variables
 *
 */

typedef struct MainData
{
    Colormap cmap;
    Cursor busy_cursor;
    Cursor move_cursor;
    Cursor resize_cursor[7];
    Cursor root_cursor;
    Display *dpy;
    GdkDisplay *gdisplay;
    GdkScreen *gscr;
    XfceFilterSetup *gtox_data;
    Screen *xscreen;
    Window gnome_win;
    Window xroot;
    Window sidewalk[2];
    Window systray;
    char *progname;
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
MainData;

extern MainData *md;

#endif /* INC_MAIN_H */
