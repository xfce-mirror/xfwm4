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

#ifndef INC_MISC_H
#define INC_MISC_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <glib.h>

void getMouseXY (Window, int *, int *);
Window getMouseWindow (Window);
GC createGC (Colormap, char *, int, XFontStruct *, int, gboolean);
void sendClientMessage (Window, Atom, Atom, Time);
void MyXGrabServer (void);
void MyXUngrabServer (void);
gboolean MyCheckWindow(Window);
Window setTmpEventWin (int, int, unsigned int, unsigned int, long);
void removeTmpEventWin (Window);
void placeSidewalks(gboolean);
Time stashEventTime (Time, XEvent *);

#endif /* INC_MISC_H */
