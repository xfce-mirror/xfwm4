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
 
        metacity - (c) 2001 Anders Carlsson, Havoc Pennington
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef INC_WIREFRAME_H
#define INC_WIREFRAME_H

#include <X11/Xlib.h>
#include "screen.h"
#include "client.h"

void wireframeUpdate (Client *c, Window xwindow);
Window wireframeCreate (Client *c);
void wireframeDelete (ScreenInfo *screen_info, Window xwindow);

#endif /* INC_WIREFRAME_H */
