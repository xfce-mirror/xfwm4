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
 
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifndef INC_DISPLAY_H
#define INC_DISPLAY_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libxfcegui4/libxfcegui4.h>

#define CORNER_TOP_LEFT                 0
#define CORNER_TOP_RIGHT                1
#define CORNER_BOTTOM_LEFT              2
#define CORNER_BOTTOM_RIGHT             3

#define SIDE_LEFT                       0
#define SIDE_RIGHT                      1
#define SIDE_BOTTOM                     2

#define ANY                             0
#define WINDOW                          1
#define FRAME                           2

typedef struct _Client            Client;
typedef struct _ClientPixmapCache ClientPixmapCache;
typedef struct _DisplayInfo       DisplayInfo;
typedef struct _XfwmColor         XfwmColor;
typedef struct _XfwmPixmap        XfwmPixmap;
typedef struct _XfwmParams        XfwmParams;
typedef struct _ScreenInfo        ScreenInfo;
typedef struct _Settings          Settings;

struct _DisplayInfo 
{
    GdkDisplay *gdisplay;
    Display *dpy;

    Cursor busy_cursor;
    Cursor move_cursor;
    Cursor root_cursor;
    Cursor resize_cursor[7];

    XfceFilterSetup *xfilter;
    GSList *screens;
    GSList *clients;

    int shape;
    int shape_event;
    int dbl_click_time;
};

DisplayInfo * myDisplayInit                 (GdkDisplay *); 
DisplayInfo * myDisplayClose                (DisplayInfo *);
Cursor        myDisplayGetCursorBusy        (DisplayInfo *);
Cursor        myDisplayGetCursorMove        (DisplayInfo *);
Cursor        myDisplayGetCursorRoot        (DisplayInfo *);
Cursor        myDisplayGetCursorResize      (DisplayInfo *, 
                                             guint);
void          myDisplayAddClient            (DisplayInfo *,
                                             Client *);
void          myDisplayRemoveClient         (DisplayInfo *, 
                                             Client *);
Client *      myDisplayGetClientFromWindow  (DisplayInfo *, 
                                             Window, 
                                             int);
void          myDisplayAddScreen            (DisplayInfo *, 
                                             ScreenInfo *);
void          myDisplayRemoveScreen         (DisplayInfo *, 
                                             ScreenInfo *);
ScreenInfo *  myDisplayGetScreenFromRoot    (DisplayInfo *, 
                                             Window);
ScreenInfo *  myDisplayGetScreenFromNum     (DisplayInfo *, 
                                             int);
Window        myDisplayGetRootFromWindow    (DisplayInfo *, 
                                             Window w);
ScreenInfo *  myDisplayGetScreenFromWindow  (DisplayInfo *, 
                                             Window w);
ScreenInfo *  myDisplayGetScreenFromSystray (DisplayInfo *, 
                                             Window);

#endif /* INC_DISPLAY_H */
