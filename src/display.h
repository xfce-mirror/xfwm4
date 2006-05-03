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
 
        xfwm4    - (c) 2002-2006 Olivier Fourdan
 
 */

#ifndef INC_DISPLAY_H
#define INC_DISPLAY_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>

#ifdef HAVE_RANDR
#include <X11/extensions/Xrandr.h>
#endif

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

#include "event_filter.h"

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

enum 
{
    COMPOSITING_MANAGER = 0,
    GNOME_PANEL_DESKTOP_AREA,
    KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR,
    KWM_WIN_ICON,
    MOTIF_WM_HINTS,
    NET_ACTIVE_WINDOW,
    NET_CLIENT_LIST,
    NET_CLIENT_LIST_STACKING,
    NET_CLOSE_WINDOW,
    NET_CURRENT_DESKTOP,
    NET_DESKTOP_GEOMETRY,
    NET_DESKTOP_LAYOUT,
    NET_DESKTOP_NAMES,
    NET_DESKTOP_VIEWPORT,
    NET_FRAME_EXTENTS,
    NET_NUMBER_OF_DESKTOPS,
    NET_REQUEST_FRAME_EXTENTS,
    NET_SHOWING_DESKTOP,
    NET_STARTUP_ID,
    NET_SUPPORTED,
    NET_SUPPORTING_WM_CHECK,
    NET_SYSTEM_TRAY_OPCODE,
    NET_WM_ACTION_CHANGE_DESKTOP,
    NET_WM_ACTION_CLOSE,
    NET_WM_ACTION_MAXIMIZE_HORZ,
    NET_WM_ACTION_MAXIMIZE_VERT,
    NET_WM_ACTION_MOVE,
    NET_WM_ACTION_RESIZE,
    NET_WM_ACTION_SHADE,
    NET_WM_ACTION_STICK,
    NET_WM_ALLOWED_ACTIONS,
    NET_WM_CONTEXT_HELP,
    NET_WM_DESKTOP,
    NET_WM_ICON,
    NET_WM_ICON_GEOMETRY,
    NET_WM_ICON_NAME,
    NET_WM_MOVERESIZE,
    NET_WM_NAME,
    NET_WM_WINDOW_OPACITY,
    NET_WM_STATE,
    NET_WM_STATE_ABOVE,
    NET_WM_STATE_BELOW,
    NET_WM_STATE_DEMANDS_ATTENTION,
    NET_WM_STATE_FULLSCREEN,
    NET_WM_STATE_HIDDEN,
    NET_WM_STATE_MAXIMIZED_HORZ,
    NET_WM_STATE_MAXIMIZED_VERT,
    NET_WM_STATE_MODAL,
    NET_WM_STATE_SHADED,
    NET_WM_STATE_SKIP_PAGER,
    NET_WM_STATE_SKIP_TASKBAR,
    NET_WM_STATE_STICKY,
    NET_WM_STRUT,
    NET_WM_STRUT_PARTIAL,
    NET_WM_USER_TIME,
    NET_WM_WINDOW_TYPE,
    NET_WM_WINDOW_TYPE_DESKTOP,
    NET_WM_WINDOW_TYPE_DIALOG,
    NET_WM_WINDOW_TYPE_DOCK,
    NET_WM_WINDOW_TYPE_MENU,
    NET_WM_WINDOW_TYPE_NORMAL,
    NET_WM_WINDOW_TYPE_SPLASH,
    NET_WM_WINDOW_TYPE_TOOLBAR,
    NET_WM_WINDOW_TYPE_UTILITY,
    NET_WORKAREA,
    MANAGER,
    PIXMAP,
    SM_CLIENT_ID,
    UTF8_STRING,
    WIN_CLIENT_LIST,
    WIN_DESKTOP_BUTTON_PROXY,
    WIN_HINTS,
    WIN_LAYER,
    WIN_PROTOCOLS,
    WIN_STATE,
    WIN_SUPPORTING_WM_CHECK,
    WIN_WORKSPACE,
    WIN_WORKSPACE_COUNT,
    WM_CHANGE_STATE,
    WM_CLIENT_LEADER,
    WM_CLIENT_MACHINE,
    WM_COLORMAP_WINDOWS,
    WM_DELETE_WINDOW,
    WM_HINTS,
    WM_PROTOCOLS,
    WM_STATE,
    WM_TAKE_FOCUS,
    WM_TRANSIENT_FOR,
    WM_WINDOW_ROLE,
    XROOTPMAP,
    XSETROOT,
    NB_ATOMS
};

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

    XfwmFilterSetup *xfilter;
    GSList *screens;
    GSList *clients;

    gboolean have_shape;
    gboolean have_render;
    gboolean have_xrandr;
    gint shape_event_base;
    gint dbl_click_time;
    gint xgrabcount;
    gint nb_screens;
    gchar* hostname;

    Time current_time;

    gboolean enable_compositor;
#ifdef HAVE_RENDER 
    gint render_error_base;
    gint render_event_base;
#endif
#ifdef HAVE_RANDR 
    gint xrandr_error_base;
    gint xrandr_event_base;
#endif
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
#ifdef ENABLE_KDE_SYSTRAY_PROXY
ScreenInfo *  myDisplayGetScreenFromSystray (DisplayInfo *, 
                                             Window);
#endif
ScreenInfo *  myDisplayGetDefaultScreen     (DisplayInfo *);
Time          myDisplayUpdateCurentTime     (DisplayInfo *, 
                                             XEvent *);
Time          myDisplayGetCurrentTime       (DisplayInfo *);
gboolean      myDisplayTestXrender          (DisplayInfo *,
                                             gdouble);

#endif /* INC_DISPLAY_H */
