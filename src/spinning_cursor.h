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
 
        Metacity - (c) 2003 Havoc Pennington
        xfwm4    - (c) 2003 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifndef INC_SPINNING_CURSOR_H
#define INC_SPINNING_CURSOR_H

#include <X11/Xlib.h>

Cursor cursorCreateSpinning (Display * dpy, Window window);

#endif /* INC_SPINNING_CURSOR_H */
