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
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/xpm.h>
#include <glib.h>
#include "screen.h"

#ifndef INC_MYPIXMAP_H
#define INC_MYPIXMAP_H

struct _XfwmPixmap
{
    ScreenInfo *screen_info;
    Pixmap pixmap, mask;
    gint width, height;
};

gboolean xfwmPixmapLoad   (ScreenInfo *, 
                           XfwmPixmap *, 
                           gchar *, 
                           gchar *,
                           XpmColorSymbol *, 
                           gint);
void xfwmPixmapCreate     (ScreenInfo *, 
                           XfwmPixmap *, 
                           gint, 
                           gint);
void xfwmPixmapInit       (ScreenInfo *, 
                           XfwmPixmap *);
void xfwmPixmapFree       (XfwmPixmap *);

#endif /* INC_MYPIXMAP_H */
