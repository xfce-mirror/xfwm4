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
        Metacity - (c) 2001 Havoc Pennington
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifndef INC_HINTS_H
#define INC_HINTS_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <X11/Xatom.h>
#include <glib.h>
#include <gdk/gdk.h>

#define MWM_HINTS_ELEMENTS                      3L

#define MWM_HINTS_FUNCTIONS                     (1L << 0)
#define MWM_HINTS_DECORATIONS                   (1L << 1)
#define MWM_HINTS_INPUT_MODE                    (1L << 2)
#define MWM_HINTS_STATUS                        (1L << 3)

#define MWM_FUNC_ALL                            (1L << 0)
#define MWM_FUNC_RESIZE                         (1L << 1)
#define MWM_FUNC_MOVE                           (1L << 2)
#define MWM_FUNC_MINIMIZE                       (1L << 3)
#define MWM_FUNC_MAXIMIZE                       (1L << 4)
#define MWM_FUNC_CLOSE                          (1L << 5)

#define MWM_DECOR_ALL                           (1L << 0)
#define MWM_DECOR_BORDER                        (1L << 1)
#define MWM_DECOR_RESIZEH                       (1L << 2)
#define MWM_DECOR_TITLE                         (1L << 3)
#define MWM_DECOR_MENU                          (1L << 4)
#define MWM_DECOR_MINIMIZE                      (1L << 5)
#define MWM_DECOR_MAXIMIZE                      (1L << 6)

#define MWM_INPUT_MODELESS                      0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL     1
#define MWM_INPUT_SYSTEM_MODAL                  2
#define MWM_INPUT_FULL_APPLICATION_MODAL        3
#define MWM_INPUT_APPLICATION_MODAL             MWM_INPUT_PRIMARY_APPLICATION_MODAL

#define MWM_TEAROFF_WINDOW                      (1L<<0)

#define WIN_STATE_STICKY                        (1L<<0)
#define WIN_STATE_MAXIMIZED_VERT                (1L<<2)
#define WIN_STATE_MAXIMIZED_HORIZ               (1L<<3)
#define WIN_STATE_MAXIMIZED                     ((1L<<2)|(1L<<3))
#define WIN_STATE_SHADED                        (1L<<5)

#define WIN_HINTS_SKIP_FOCUS                    (1L<<0)
#define WIN_HINTS_SKIP_TASKBAR                  (1L<<2)

#define WM_PROTOCOLS_TAKE_FOCUS                 (1L<<0)
#define WM_PROTOCOLS_DELETE_WINDOW              (1L<<1)
#define WM_PROTOCOLS_CONTEXT_HELP               (1L<<2)

#define WIN_LAYER_DESKTOP                       0
#define WIN_LAYER_BELOW                         2
#define WIN_LAYER_NORMAL                        4
#define WIN_LAYER_ONTOP                         6
#define WIN_LAYER_DOCK                          8
#define WIN_LAYER_ABOVE_DOCK                    10

#define NET_WM_MOVERESIZE_SIZE_TOPLEFT          0
#define NET_WM_MOVERESIZE_SIZE_TOP              1
#define NET_WM_MOVERESIZE_SIZE_TOPRIGHT         2
#define NET_WM_MOVERESIZE_SIZE_RIGHT            3
#define NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT      4
#define NET_WM_MOVERESIZE_SIZE_BOTTOM           5
#define NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT       6
#define NET_WM_MOVERESIZE_SIZE_LEFT             7
#define NET_WM_MOVERESIZE_MOVE                  8

#define NET_WM_STATE_REMOVE                     0
#define NET_WM_STATE_ADD                        1
#define NET_WM_STATE_TOGGLE                     2

#define LEFT                            0
#define RIGHT                           1
#define TOP                             2
#define BOTTOM                          3
#define LEFT_START_Y                    4
#define LEFT_END_Y                      5
#define RIGHT_START_Y                   6
#define RIGHT_END_Y                     7
#define TOP_START_X                     8
#define TOP_END_X                       9
#define BOTTOM_START_X                  10
#define BOTTOM_END_X                    11

typedef struct
{
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
}
PropMwmHints;

extern Atom gnome_panel_desktop_area;
extern Atom motif_wm_hints;
extern Atom sm_client_id;
extern Atom win_client_list;
extern Atom win_desktop_button_proxy;
extern Atom win_hints;
extern Atom win_layer;
extern Atom win_protocols;
extern Atom win_state;
extern Atom win_supporting_wm_check;
extern Atom win_workspace;
extern Atom win_workspace_count;
extern Atom wm_change_state;
extern Atom wm_client_leader;
extern Atom wm_colormap_windows;
extern Atom wm_delete_window;
extern Atom wm_protocols;
extern Atom wm_state;
extern Atom wm_takefocus;
extern Atom wm_transient_for;
extern Atom wm_window_role;

/* freedesktop.org protocol */

extern Atom net_active_window;
extern Atom net_client_list;
extern Atom net_client_list_stacking;
extern Atom net_close_window;
extern Atom net_current_desktop;
extern Atom net_desktop_geometry;
extern Atom net_desktop_viewport;
extern Atom net_desktop_names;
extern Atom net_number_of_desktops;
extern Atom net_showing_desktop;
extern Atom net_startup_id;
extern Atom net_supported;
extern Atom net_supporting_wm_check;
extern Atom net_wm_action_change_desktop;
extern Atom net_wm_action_close;
extern Atom net_wm_action_maximize_horz;
extern Atom net_wm_action_maximize_vert;
extern Atom net_wm_action_move;
extern Atom net_wm_action_resize;
extern Atom net_wm_action_shade;
extern Atom net_wm_action_stick;
extern Atom net_wm_allowed_actions;
extern Atom net_wm_desktop;
extern Atom net_wm_icon;
extern Atom net_wm_icon_geometry;
extern Atom net_wm_icon_name;
extern Atom net_wm_moveresize;
extern Atom net_wm_name;
extern Atom net_wm_state;
extern Atom net_wm_state_above;
extern Atom net_wm_state_below;
extern Atom net_wm_state_fullscreen;
extern Atom net_wm_state_hidden;
extern Atom net_wm_state_maximized_horz;
extern Atom net_wm_state_maximized_vert;
extern Atom net_wm_state_modal;
extern Atom net_wm_state_shaded;
extern Atom net_wm_state_skip_pager;
extern Atom net_wm_state_skip_taskbar;
extern Atom net_wm_state_sticky;
extern Atom net_wm_strut;
extern Atom net_wm_strut_partial;
extern Atom net_wm_window_type;
extern Atom net_wm_window_type_desktop;
extern Atom net_wm_window_type_dialog;
extern Atom net_wm_window_type_dock;
extern Atom net_wm_window_type_menu;
extern Atom net_wm_window_type_normal;
extern Atom net_wm_window_type_splashscreen;
extern Atom net_wm_window_type_toolbar;
extern Atom net_wm_window_type_utility;
extern Atom net_wm_user_time;
extern Atom net_workarea;

/* KDE extension */
extern Atom kde_net_wm_context_help;
extern Atom kde_net_wm_system_tray_window_for;
extern Atom kwm_win_icon;

/* Systray similation for older KDE apps */
extern Atom net_system_tray_manager;
extern Atom net_system_tray_selection;
extern Atom net_system_tray_opcode;

void initICCCMHints (Display *);
unsigned long getWMState (Display *, Window);
void setWMState (Display *, Window, unsigned long);
void initMotifHints (Display *);
PropMwmHints *getMotifHints (Display *, Window);
unsigned int getWMProtocols (Display *, Window);
void initGnomeHints (Display *);
void initKDEHints (Display *);
Atom initSystrayHints (Display *, int);
gboolean getHint (Display *, Window, Atom, long *);
void setHint (Display *, Window, Atom, long);
void getGnomeDesktopMargins (Display *, int, int *);
void setGnomeProtocols (Display *, int, Window);
void initNetHints (Display * dpy);
void setNetSupportedHint (Display *, int, Window);
gboolean getAtomList (Display *, Window, Atom, Atom **, int *);
gboolean getCardinalList (Display *, Window, Atom, unsigned long **, int *);
void setNetWorkarea (Display *, int, int, int, int, int *);
void initNetDesktopInfo (Display *, int, int, int, int);
void set_utf8_string_hint (Display *, Window, Atom, const char *);
void getTransientFor (Display *, int, Window, Window *);
void getWindowName (Display *, Window, char **);
gboolean getUTF8String (Display *, Window, Atom, char **, int *);
void getWindowName (Display *, Window, char **);
gboolean checkKdeSystrayWindow(Display *, Window);
void sendSystrayReqDock(Display *, Window, Window);
Window getSystrayWindow (Display *, Atom);
gboolean getWindowRole (Display *, Window, char **);
Window getClientLeader (Display *, Window);
gboolean getNetWMUserTime (Display *, Window, Time *);
gboolean getClientID (Display *, Window, char **);
gboolean getWindowCommand (Display *, Window, char ***, int *);
gboolean getKDEIcon (Display *, Window, Pixmap *, Pixmap *);
gboolean getRGBIconData (Display *, Window, unsigned long **, unsigned long *);

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
gboolean getWindowStartupId (Display *, Window, char **);
#endif

#endif /* INC_HINTS_H */
