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

#ifndef __MISC_H__
#define __MISC_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include "debug.h"

void getMouseXY(Window, int *, int *);
Window getMouseWindow(Window);
GC createGC(Colormap, char *, int, XFontStruct *, int);
void sendClientMessage(Window, Atom, long, int);
void MyXGrabServer(void);
void MyXUngrabServer(void);
Window setTmpEventWin(long);
void removeTmpEventWin(Window);

#endif /* __MISC_H__ */
