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
 
        xcompmgr - (c) 2003 Keith Packard
        xfwm4    - (c) 2004 Olivier Fourdan
 
 */

#ifndef INC_COMPOSITOR_H
#define INC_COMPOSITOR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>

#include "display.h"
#include "screen.h"
#include "client.h"

void     compositorMapWindow        (DisplayInfo *, Window);
void     compositorUnmapWindow      (DisplayInfo *, Window);
void     compositorAddWindow        (DisplayInfo *, Window, Client *c);
void     compositorRemoveWindow     (DisplayInfo *, Window);

void     compositorHandleEvent      (DisplayInfo *, XEvent *);
gboolean compositorCheckDamageEvent (DisplayInfo *display_info);
void     compositorInitDisplay      (DisplayInfo *);
void     compositorRepairDisplay    (DisplayInfo *);

void     compositorManageScreen     (ScreenInfo *);
void     compositorUnmanageScreen   (ScreenInfo *);
void     compositorRepairScreen     (DisplayInfo *);

void     compositorWindowSetOpacity (DisplayInfo *, Window, guint);
void     compositorDamageWindow     (DisplayInfo *, Window);
void     compositorResizeWindow     (DisplayInfo *, Window, gint, gint);

#endif /* INC_COMPOSITOR_H */
