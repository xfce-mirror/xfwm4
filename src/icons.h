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
 
        Metacity - (c) 2001 Havoc Pennington
        libwnck  - (c) 2001 Havoc Pennington
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef INC_ICONS_H
#define INC_ICONS_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

GdkPixbuf *getAppIcon (Display *, Window, int, int);

#endif /* INC_ICONS_H */
