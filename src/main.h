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

#ifndef __MAIN_H__
#define __MAIN_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "client.h"
#include "misc.h"
#include "hints.h"
#include "keyboard.h"
#include "pixmap.h"
#include "parserc.h"
#include "gtktoxevent.h"

/*
 *
 * Global variables
 *
 */

extern char *progname;
extern Display *dpy;
extern Window root, gnome_win;
extern int screen;
extern int depth;
extern Colormap cmap;
extern CARD32 margins[4];
extern int quit, reload;
extern int shape, shape_event;
extern Cursor resize_cursor[7], move_cursor, root_cursor;

/*
 *
 * Function prototypes
 *
 */

#endif /* __MAIN_H__ */
