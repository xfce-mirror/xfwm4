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

#ifndef __PIXMAP_H__
#define __PIXMAP_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/xpm.h>
#include <glib.h>

typedef struct
{
    Pixmap pixmap, mask;
    gint width, height;
}
MyPixmap;

gboolean loadPixmap(Display *, MyPixmap *, gchar *, gchar *, XpmColorSymbol *, gint);
void freePixmap(Display *, MyPixmap *);
void scalePixmap(Display *, MyPixmap *, MyPixmap *, gint width, gint height);

#endif /* __PIXMAP_H__ */
