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

#ifndef INC_MYPIXMAP_H
#define INC_MYPIXMAP_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/xpm.h>
#include <glib.h>
#include "screen.h"

typedef struct
{
    ScreenData *md;
    Pixmap pixmap, mask;
    gint width, height;
}
MyPixmap;

gboolean myPixmapLoad (ScreenData *, MyPixmap *, gchar *, gchar *,
                       XpmColorSymbol *, gint);
void myPixmapCreate (ScreenData *, MyPixmap *, gint, gint);
void myPixmapInit (MyPixmap *);
void myPixmapFree (MyPixmap *);

#endif /* INC_MYPIXMAP_H */
