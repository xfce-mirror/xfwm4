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
        Foundation, Inc., Inc., 51 Franklin Street, Fifth Floor, Boston,
        MA 02110-1301, USA.


        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2015 Olivier Fourdan

 */

#include <glib.h>
#include <gdk/gdk.h>
#include "screen.h"
#include "keyboard.h"
#include "mypixmap.h"
#include "hints.h"

#ifndef INC_SETTINGS_H
#define INC_SETTINGS_H

#ifndef DEFAULT_THEME
#define DEFAULT_THEME "Default"
#endif

#ifndef DEFAULT_HDPI_THEME
#define DEFAULT_HDPI_THEME "Default-xhdpi"
#endif

enum
{
    TITLE_SHADOW_NONE  = 0,
    TITLE_SHADOW_UNDER = 1,
    TITLE_SHADOW_FRAME = 2
};

/** All key definition prior to FIRST_KEY are not grabbed continuously */
#define FIRST_KEY KEY_ADD_ADJACENT_WORKSPACE
enum
{
    KEY_CANCEL = 0,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    /* FIRST_KEY starts here */
    KEY_ADD_ADJACENT_WORKSPACE,
    KEY_ADD_WORKSPACE,
    KEY_CLOSE_WINDOW,
    KEY_CYCLE_WINDOWS,
    KEY_CYCLE_REVERSE_WINDOWS,
    KEY_DEL_ACTIVE_WORKSPACE,
    KEY_DEL_WORKSPACE,
    KEY_DOWN_WORKSPACE,
    KEY_FILL_HORIZ,
    KEY_FILL_VERT,
    KEY_FILL_WINDOW,
    KEY_HIDE_WINDOW,
    KEY_LEFT_WORKSPACE,
    KEY_LOWER_WINDOW,
    KEY_MAXIMIZE_HORIZ,
    KEY_MAXIMIZE_VERT,
    KEY_MAXIMIZE_WINDOW,
    KEY_MOVE,
    KEY_MOVE_TO_MONITOR_DOWN,
    KEY_MOVE_TO_MONITOR_LEFT,
    KEY_MOVE_TO_MONITOR_RIGHT,
    KEY_MOVE_TO_MONITOR_UP,
    KEY_MOVE_DOWN_WORKSPACE,
    KEY_MOVE_LEFT_WORKSPACE,
    KEY_MOVE_NEXT_WORKSPACE,
    KEY_MOVE_PREV_WORKSPACE,
    KEY_MOVE_RIGHT_WORKSPACE,
    KEY_MOVE_UP_WORKSPACE,
    KEY_MOVE_WORKSPACE_1,
    KEY_MOVE_WORKSPACE_10,
    KEY_MOVE_WORKSPACE_11,
    KEY_MOVE_WORKSPACE_12,
    KEY_MOVE_WORKSPACE_2,
    KEY_MOVE_WORKSPACE_3,
    KEY_MOVE_WORKSPACE_4,
    KEY_MOVE_WORKSPACE_5,
    KEY_MOVE_WORKSPACE_6,
    KEY_MOVE_WORKSPACE_7,
    KEY_MOVE_WORKSPACE_8,
    KEY_MOVE_WORKSPACE_9,
    KEY_NEXT_WORKSPACE,
    KEY_POPUP_MENU,
    KEY_PREV_WORKSPACE,
    KEY_RAISE_WINDOW,
    KEY_RAISELOWER_WINDOW,
    KEY_RESIZE,
    KEY_RIGHT_WORKSPACE,
    KEY_SHADE_WINDOW,
    KEY_SHOW_DESKTOP,
    KEY_STICK_WINDOW,
    KEY_SWITCH_APPLICATION,
    KEY_SWITCH_WINDOW,
    KEY_TILE_DOWN,
    KEY_TILE_LEFT,
    KEY_TILE_RIGHT,
    KEY_TILE_UP,
    KEY_TILE_DOWN_LEFT,
    KEY_TILE_DOWN_RIGHT,
    KEY_TILE_UP_LEFT,
    KEY_TILE_UP_RIGHT,
    KEY_TOGGLE_ABOVE,
    KEY_TOGGLE_FULLSCREEN,
    KEY_UP_WORKSPACE,
    KEY_WORKSPACE_1,
    KEY_WORKSPACE_2,
    KEY_WORKSPACE_3,
    KEY_WORKSPACE_4,
    KEY_WORKSPACE_5,
    KEY_WORKSPACE_6,
    KEY_WORKSPACE_7,
    KEY_WORKSPACE_8,
    KEY_WORKSPACE_9,
    KEY_WORKSPACE_10,
    KEY_WORKSPACE_11,
    KEY_WORKSPACE_12,
    KEY_COUNT
};

enum
{
    ALIGN_LEFT,
    ALIGN_RIGHT,
    ALIGN_CENTER
};

enum
{
    ACTIVATE_ACTION_NONE,
    ACTIVATE_ACTION_BRING,
    ACTIVATE_ACTION_SWITCH
};

enum
{
    DOUBLE_CLICK_ACTION_NONE,
    DOUBLE_CLICK_ACTION_MAXIMIZE,
    DOUBLE_CLICK_ACTION_SHADE,
    DOUBLE_CLICK_ACTION_FILL,
    DOUBLE_CLICK_ACTION_ABOVE,
    DOUBLE_CLICK_ACTION_HIDE
};

enum
{
    PLACE_MOUSE,
    PLACE_CENTER
};

struct _Settings
{
    gchar  *option;
    GValue *value;
    GType   type;
    gboolean required;
};

struct _XfwmParams
{
    MyKey keys[KEY_COUNT];
    gchar button_layout[BUTTON_STRING_COUNT + 1];
    int xfwm_margins[4];
    int activate_action;
    int button_offset;
    int button_spacing;
    int cycle_tabwin_mode;
    int double_click_action;
    guint easy_click;
    int focus_delay;
    int frame_opacity;
    int frame_border_top;
    int inactive_opacity;
    int maximized_offset;
    int move_opacity;
    int placement_mode;
    int placement_ratio;
    int popup_opacity;
    int raise_delay;
    int resize_opacity;
    int shadow_delta_height;
    int shadow_delta_width;
    int shadow_delta_x;
    int shadow_delta_y;
    int shadow_opacity;
    int snap_width;
    int title_alignment;
    int title_horizontal_offset;
    int title_shadow[2];
    int wrap_resistance;
    unsigned int borderless_maximize : 1;
    unsigned int titleless_maximize : 1;
    unsigned int box_move : 1;
    unsigned int box_resize : 1;
    unsigned int click_to_focus : 1;
    unsigned int cycle_apps_only : 1;
    unsigned int cycle_draw_frame : 1;
    unsigned int cycle_raise : 1;
    unsigned int cycle_hidden : 1;
    unsigned int cycle_minimized : 1;
    unsigned int cycle_minimum : 1;
    unsigned int cycle_preview : 1;
    unsigned int cycle_workspaces : 1;
    unsigned int focus_hint : 1;
    unsigned int focus_new : 1;
    unsigned int full_width_title : 1;
    unsigned int horiz_scroll_opacity : 1;
    unsigned int mousewheel_rollup : 1;
    unsigned int prevent_focus_stealing : 1;
    unsigned int raise_on_click : 1;
    unsigned int raise_on_focus : 1;
    unsigned int raise_with_any_button : 1;
    unsigned int repeat_urgent_blink : 1;
    unsigned int scroll_workspaces : 1;
    unsigned int show_app_icon : 1;
    unsigned int show_dock_shadow : 1;
    unsigned int show_frame_shadow : 1;
    unsigned int show_popup_shadow : 1;
    unsigned int snap_resist : 1;
    unsigned int snap_to_border : 1;
    unsigned int snap_to_windows : 1;
    unsigned int tile_on_move : 1;
    unsigned int title_vertical_offset_active : 1;
    unsigned int title_vertical_offset_inactive : 1;
    unsigned int toggle_workspaces : 1;
    unsigned int unredirect_overlays : 1;
    unsigned int urgent_blink : 1;
    unsigned int use_compositing : 1;
    unsigned int wrap_cycle : 1;
    unsigned int wrap_layout : 1;
    unsigned int wrap_windows : 1;
    unsigned int wrap_workspaces : 1;
    unsigned int zoom_desktop : 1;
    unsigned int zoom_pointer : 1;
};

gboolean                 loadSettings                           (ScreenInfo *);
gboolean                 reloadSettings                         (DisplayInfo *,
                                                                 int);
gboolean                 initSettings                           (ScreenInfo *);
void                     closeSettings                          (ScreenInfo *);

#endif /* INC_SETTINGS_H */
