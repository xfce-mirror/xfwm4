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

#ifdef HAVE_COMPOSITOR
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#if COMPOSITE_MAJOR > 0 || COMPOSITE_MINOR >= 2
#ifndef HAVE_NAME_WINDOW_PIXMAP
#define HAVE_NAME_WINDOW_PIXMAP 1
#endif /* HAVE_NAME_WINDOW_PIXMAP */
#endif /* COMPOSITE_MAJOR > 0 || COMPOSITE_MINOR >= 2 */
#endif /* HAVE_COMPOSITOR */

#include <gtk/gtk.h>
#include <glib.h>
#include <libxfcegui4/libxfcegui4.h>

#define CORNER_TOP_LEFT                                         0
#define CORNER_TOP_RIGHT                                        1
#define CORNER_BOTTOM_LEFT                                      2
#define CORNER_BOTTOM_RIGHT                                     3

#define SIDE_LEFT                                               0
#define SIDE_RIGHT                                              1
#define SIDE_BOTTOM                                             2
#define SIDE_TOP                                                3

#define ANY                                                     0
#define WINDOW                                                  1
#define FRAME                                                   2

#define GNOME_PANEL_DESKTOP_AREA                                0
#define KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR                       1
#define KWM_WIN_ICON                                            2
#define MOTIF_WM_HINTS                                          3
#define NET_ACTIVE_WINDOW                                       4
#define NET_CLIENT_LIST                                         5
#define NET_CLIENT_LIST_STACKING                                6
#define NET_CLOSE_WINDOW                                        7
#define NET_CURRENT_DESKTOP                                     8
#define NET_DESKTOP_GEOMETRY                                    9
#define NET_DESKTOP_LAYOUT                                      10
#define NET_DESKTOP_NAMES                                       11
#define NET_DESKTOP_VIEWPORT                                    12
#define NET_FRAME_EXTENTS                                       13
#define NET_NUMBER_OF_DESKTOPS                                  14
#define NET_REQUEST_FRAME_EXTENTS                               15
#define NET_SHOWING_DESKTOP                                     16
#define NET_STARTUP_ID                                          17
#define NET_SUPPORTED                                           18
#define NET_SUPPORTING_WM_CHECK                                 19
#define NET_SYSTEM_TRAY_OPCODE                                  20
#define NET_WM_ACTION_CHANGE_DESKTOP                            21
#define NET_WM_ACTION_CLOSE                                     22
#define NET_WM_ACTION_MAXIMIZE_HORZ                             23
#define NET_WM_ACTION_MAXIMIZE_VERT                             24
#define NET_WM_ACTION_MOVE                                      25
#define NET_WM_ACTION_RESIZE                                    26
#define NET_WM_ACTION_SHADE                                     27
#define NET_WM_ACTION_STICK                                     28
#define NET_WM_ALLOWED_ACTIONS                                  29
#define NET_WM_CONTEXT_HELP                                     30
#define NET_WM_DESKTOP                                          31
#define NET_WM_ICON                                             32
#define NET_WM_ICON_GEOMETRY                                    33
#define NET_WM_ICON_NAME                                        34
#define NET_WM_MOVERESIZE                                       35
#define NET_WM_NAME                                             36
#define NET_WM_WINDOW_OPACITY                                   37
#define NET_WM_STATE                                            38
#define NET_WM_STATE_ABOVE                                      39
#define NET_WM_STATE_BELOW                                      40
#define NET_WM_STATE_DEMANDS_ATTENTION                          41
#define NET_WM_STATE_FULLSCREEN                                 42
#define NET_WM_STATE_HIDDEN                                     43
#define NET_WM_STATE_MAXIMIZED_HORZ                             44
#define NET_WM_STATE_MAXIMIZED_VERT                             45
#define NET_WM_STATE_MODAL                                      46
#define NET_WM_STATE_SHADED                                     47
#define NET_WM_STATE_SKIP_PAGER                                 48
#define NET_WM_STATE_SKIP_TASKBAR                               49
#define NET_WM_STATE_STICKY                                     50
#define NET_WM_STRUT                                            51
#define NET_WM_STRUT_PARTIAL                                    52
#define NET_WM_USER_TIME                                        53
#define NET_WM_WINDOW_TYPE                                      54
#define NET_WM_WINDOW_TYPE_DESKTOP                              55
#define NET_WM_WINDOW_TYPE_DIALOG                               56
#define NET_WM_WINDOW_TYPE_DOCK                                 57
#define NET_WM_WINDOW_TYPE_MENU                                 58
#define NET_WM_WINDOW_TYPE_NORMAL                               59
#define NET_WM_WINDOW_TYPE_SPLASH                               60
#define NET_WM_WINDOW_TYPE_TOOLBAR                              61
#define NET_WM_WINDOW_TYPE_UTILITY                              62
#define NET_WORKAREA                                            63
#define MANAGER                                                 64
#define PIXMAP                                                  65
#define SM_CLIENT_ID                                            66
#define UTF8_STRING                                             67
#define WIN_CLIENT_LIST                                         68
#define WIN_DESKTOP_BUTTON_PROXY                                69
#define WIN_HINTS                                               70
#define WIN_LAYER                                               71
#define WIN_PROTOCOLS                                           72
#define WIN_STATE                                               73
#define WIN_SUPPORTING_WM_CHECK                                 74
#define WIN_WORKSPACE                                           75
#define WIN_WORKSPACE_COUNT                                     76
#define WM_CHANGE_STATE                                         77
#define WM_CLIENT_LEADER                                        78
#define WM_COLORMAP_WINDOWS                                     79
#define WM_DELETE_WINDOW                                        80
#define WM_HINTS                                                81
#define WM_PROTOCOLS                                            82
#define WM_STATE                                                83
#define WM_TAKE_FOCUS                                           84
#define WM_TRANSIENT_FOR                                        85
#define WM_WINDOW_ROLE                                          86
#define XROOTPMAP                                               87
#define XSETROOT                                                88

#define NB_ATOMS                                                89

typedef struct _Client            Client;
typedef struct _ClientPixmapCache ClientPixmapCache;
typedef struct _DisplayInfo       DisplayInfo;
typedef struct _XfwmColor         XfwmColor;
typedef struct _xfwmPixmap        xfwmPixmap;
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
    Cursor resize_cursor[8];
    
    Atom atoms[NB_ATOMS];

    XfceFilterSetup *xfilter;
    GSList *screens;
    GSList *clients;

    gboolean have_shape;
    gboolean have_render;
    gint shape_event_base;
    gint render_error_base;
    gint render_event_base;
    gint dbl_click_time;
    gint xgrabcount;
    gint nb_screens;

    Time current_time;

    gboolean enable_compositor;

#ifdef HAVE_COMPOSITOR
    gint composite_error_base;
    gint composite_event_base;
    gint damage_error_base;
    gint damage_event_base;
    gint fixes_error_base;
    gint fixes_event_base;

    gboolean have_composite;
    gboolean have_damage;
    gboolean have_fixes;

    guint compositor_idle_id;
    guint compositor_timeout_id;

#if HAVE_NAME_WINDOW_PIXMAP
    gboolean have_name_window_pixmap;
#endif /* HAVE_NAME_WINDOW_PIXMAP */

#endif /* HAVE_COMPOSITOR */
};

DisplayInfo * myDisplayInit                 (GdkDisplay *); 
DisplayInfo * myDisplayClose                (DisplayInfo *);
gboolean      myDisplayHaveShape            (DisplayInfo *);
gboolean      myDisplayHaveRender           (DisplayInfo *);
Cursor        myDisplayGetCursorBusy        (DisplayInfo *);
Cursor        myDisplayGetCursorMove        (DisplayInfo *);
Cursor        myDisplayGetCursorRoot        (DisplayInfo *);
Cursor        myDisplayGetCursorResize      (DisplayInfo *, 
                                             guint);
void          myDisplayGrabServer           (DisplayInfo *);
void          myDisplayUngrabServer         (DisplayInfo *);
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
Time          myDisplayUpdateCurentTime     (DisplayInfo *, 
                                             XEvent *);
Time          myDisplayGetCurrentTime       (DisplayInfo *);

#endif /* INC_DISPLAY_H */
