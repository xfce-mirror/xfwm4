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
        Metacity - (c) 2001 Havoc Pennington
        xfwm4    - (c) 2002-2003 Olivier Fourdan
 
 */

#ifndef INC_HINTS_H
#define INC_HINTS_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xatom.h>
#include <glib.h>

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

typedef struct
{
    CARD32 flags;
    CARD32 functions;
    CARD32 decorations;
    INT32 inputMode;
    CARD32 status;
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
extern Atom wm_window_role;

/* freedesktop.org protocol */

extern Atom net_active_window;
extern Atom net_client_list;
extern Atom net_client_list_stacking;
extern Atom net_close_window;
extern Atom net_current_desktop;
extern Atom net_desktop_geometry;
extern Atom net_desktop_viewport;
extern Atom net_number_of_desktops;
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
extern Atom net_wm_window_type;
extern Atom net_wm_window_type_desktop;
extern Atom net_wm_window_type_dialog;
extern Atom net_wm_window_type_dock;
extern Atom net_wm_window_type_menu;
extern Atom net_wm_window_type_normal;
extern Atom net_wm_window_type_splashscreen;
extern Atom net_wm_window_type_toolbar;
extern Atom net_wm_window_type_utility;
extern Atom net_workarea;


void initICCCMHints(Display *);
unsigned long getWMState(Display *, Window);
void setWMState(Display *, Window, unsigned long state);
void initMotifHints(Display *);
PropMwmHints *getMotifHints(Display *, Window);
unsigned int getWMProtocols(Display *, Window);
void initGnomeHints(Display *);
gboolean getGnomeHint(Display *, Window, Atom, long *);
void setGnomeHint(Display *, Window, Atom, long);
void getGnomeDesktopMargins(Display *, int, CARD32 *);
void setGnomeProtocols(Display *, int, Window);
void initNetHints(Display * dpy);
void set_net_supported_hint(Display *, int, Window);
gboolean get_atom_list(Display *, Window, Atom, Atom **, int *);
gboolean get_cardinal_list(Display *, Window, Atom, unsigned long **, int *);
void set_net_workarea(Display *, int, int, CARD32 *);
void init_net_desktop_params(Display *, int, int);
void set_utf8_string_hint(Display *, Window, Atom, const char *);
void getTransientFor(Display * dpy, Window w, Window * transient_for);
void getWindowName(Display * dpy, Window w, char **name);
gboolean get_utf8_string(Display *, Window, Atom, char **);
void getWindowName(Display *, Window, char **);
gboolean getWindowRole(Display *, Window, char **);
Window getClientLeader(Display *, Window);
gboolean getClientID(Display *, Window, char **);
gboolean getWindowCommand(Display *, Window, char ***, int *);
#ifdef HAVE_STARTUP_NOTIFICATION
gboolean getWindowStartupId(Display *, Window, char **);
#endif

#define setNetHint setGnomeHint
#define getNetHint getGnomeHint

#endif /* INC_HINTS_H */
