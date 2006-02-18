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
        xfwm4    - (c) 2002-2006 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include "screen.h"
#include "keyboard.h"
#include "mypixmap.h"
#include "hints.h"

#ifndef INC_SETTINGS_H
#define INC_SETTINGS_H

#define TITLE_1                         0
#define TITLE_2                         1
#define TITLE_3                         2
#define TITLE_4                         3
#define TITLE_5                         4

#define MENU_BUTTON                     0
#define STICK_BUTTON                    1
#define SHADE_BUTTON                    2
#define HIDE_BUTTON                     3
#define MAXIMIZE_BUTTON                 4
#define CLOSE_BUTTON                    5
#define TITLE_SEPARATOR                 6
#define BUTTON_COUNT                    6

#define ACTIVE                          0
#define INACTIVE                        1
#define PRESSED                         2
#define T_ACTIVE                        3
#define T_INACTIVE                      4
#define T_PRESSED                       5

#define KEY_MOVE_UP                     0
#define KEY_MOVE_DOWN                   1
#define KEY_MOVE_LEFT                   2
#define KEY_MOVE_RIGHT                  3
#define KEY_RESIZE_UP                   4
#define KEY_RESIZE_DOWN                 5
#define KEY_RESIZE_LEFT                 6
#define KEY_RESIZE_RIGHT                7
#define KEY_CYCLE_WINDOWS               8
#define KEY_CLOSE_WINDOW                9
#define KEY_HIDE_WINDOW                 10
#define KEY_MAXIMIZE_WINDOW             11
#define KEY_MAXIMIZE_VERT               12
#define KEY_MAXIMIZE_HORIZ              13
#define KEY_SHADE_WINDOW                14
#define KEY_NEXT_WORKSPACE              15
#define KEY_PREV_WORKSPACE              16
#define KEY_ADD_WORKSPACE               17
#define KEY_DEL_WORKSPACE               18
#define KEY_STICK_WINDOW                19
#define KEY_WORKSPACE_1                 20
#define KEY_WORKSPACE_2                 21
#define KEY_WORKSPACE_3                 22
#define KEY_WORKSPACE_4                 23
#define KEY_WORKSPACE_5                 24
#define KEY_WORKSPACE_6                 25
#define KEY_WORKSPACE_7                 26
#define KEY_WORKSPACE_8                 27
#define KEY_WORKSPACE_9                 28
#define KEY_WORKSPACE_10                29
#define KEY_WORKSPACE_11                30
#define KEY_WORKSPACE_12                31
#define KEY_MOVE_NEXT_WORKSPACE         32
#define KEY_MOVE_PREV_WORKSPACE         33
#define KEY_MOVE_WORKSPACE_1            34
#define KEY_MOVE_WORKSPACE_2            35
#define KEY_MOVE_WORKSPACE_3            36
#define KEY_MOVE_WORKSPACE_4            37
#define KEY_MOVE_WORKSPACE_5            38
#define KEY_MOVE_WORKSPACE_6            39
#define KEY_MOVE_WORKSPACE_7            40
#define KEY_MOVE_WORKSPACE_8            41
#define KEY_MOVE_WORKSPACE_9            42
#define KEY_MOVE_WORKSPACE_10           43
#define KEY_MOVE_WORKSPACE_11           44
#define KEY_MOVE_WORKSPACE_12           45
#define KEY_SHOW_DESKTOP                58
#define KEY_LOWER_WINDOW                59
#define KEY_RAISE_WINDOW                60
#define KEY_TOGGLE_FULLSCREEN           61
#define KEY_UP_WORKSPACE                62
#define KEY_DOWN_WORKSPACE              63
#define KEY_LEFT_WORKSPACE              64
#define KEY_RIGHT_WORKSPACE             65
#define KEY_MOVE_UP_WORKSPACE           66
#define KEY_MOVE_DOWN_WORKSPACE         67
#define KEY_MOVE_LEFT_WORKSPACE         68
#define KEY_MOVE_RIGHT_WORKSPACE        69
#define KEY_COUNT                       70

#define ALIGN_LEFT                      0
#define ALIGN_RIGHT                     1
#define ALIGN_CENTER                    2

#define ACTION_NONE                     0
#define ACTION_MAXIMIZE                 1
#define ACTION_SHADE                    2
#define ACTION_HIDE                     3

struct _XfwmColor
{
    GdkColor col;
    GdkGC *gc;
    gboolean allocated;
};

struct _Settings
{
    gchar *option;
    gchar *value;
    gboolean required;
};

struct _XfwmParams
{
    MyKey keys[KEY_COUNT];
    gchar button_layout[8];
    unsigned int xfwm_margins[4];
    int button_offset;
    int button_spacing;
    int double_click_action;
    int move_opacity;
    int placement_ratio;
    int popup_opacity;
    int raise_delay;
    int resize_opacity;
    int restore_on_move;
    int shadow_delta_height;
    int shadow_delta_width;
    int shadow_delta_x;
    int shadow_delta_y;
    int snap_width;
    int title_alignment;
    int title_horizontal_offset;
    int wrap_resistance;
    gboolean title_shadow[2];
    gboolean box_move;
    gboolean box_resize;
    gboolean click_to_focus;
    gboolean cycle_hidden;
    gboolean cycle_minimum;
    gboolean cycle_workspaces;
    gboolean easy_click;
    gboolean focus_hint;
    gboolean focus_new;
    gboolean full_width_title;
    gboolean prevent_focus_stealing;
    gboolean raise_on_click;
    gboolean raise_on_focus;
    gboolean raise_with_any_button;
    gboolean scroll_workspaces;
    gboolean show_app_icon;
    gboolean show_frame_shadow;
    gboolean show_popup_shadow;
    gboolean snap_to_border;
    gboolean snap_to_windows;
    gboolean snap_resist;
    gboolean title_vertical_offset_active;
    gboolean title_vertical_offset_inactive;
    gboolean toggle_workspaces;
    gboolean wrap_cycle;
    gboolean wrap_layout;
    gboolean wrap_windows;
    gboolean wrap_workspaces;
};

gboolean loadSettings   (ScreenInfo *);
gboolean reloadSettings (DisplayInfo *, 
                         int);
gboolean initSettings   (ScreenInfo *);
void     closeSettings  (ScreenInfo *);

#endif /* INC_SETTINGS_H */
