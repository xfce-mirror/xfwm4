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
        xfwm4    - (c) 2002-2003 Olivier Fourdan
 
 */

#ifndef INC_SETTINGS_H
#define INC_SETTINGS_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include "keyboard.h"
#include "mypixmap.h"

#define CORNER_TOP_LEFT                 0
#define CORNER_TOP_RIGHT                1
#define CORNER_BOTTOM_LEFT              2
#define CORNER_BOTTOM_RIGHT             3

#define SIDE_LEFT                       0
#define SIDE_RIGHT                      1
#define SIDE_BOTTOM                     2

#define TITLE_1                         0
#define TITLE_2                         1
#define TITLE_3                         2
#define TITLE_4                         3
#define TITLE_5                         4

#define HIDE_BUTTON                     0
#define SHADE_BUTTON                    1
#define MAXIMIZE_BUTTON                 2
#define CLOSE_BUTTON                    3
#define STICK_BUTTON                    4
#define MENU_BUTTON                     5
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
#define KEY_MOVE_NEXT_WORKSPACE         29
#define KEY_MOVE_PREV_WORKSPACE         30
#define KEY_MOVE_WORKSPACE_1            31
#define KEY_MOVE_WORKSPACE_2            32
#define KEY_MOVE_WORKSPACE_3            33
#define KEY_MOVE_WORKSPACE_4            34
#define KEY_MOVE_WORKSPACE_5            35
#define KEY_MOVE_WORKSPACE_6            36
#define KEY_MOVE_WORKSPACE_7            37
#define KEY_MOVE_WORKSPACE_8            38
#define KEY_MOVE_WORKSPACE_9            39
#define KEY_SHORTCUT_1                  40
#define KEY_SHORTCUT_2                  41
#define KEY_SHORTCUT_3                  42
#define KEY_SHORTCUT_4                  43
#define KEY_SHORTCUT_5                  44
#define KEY_SHORTCUT_6                  45
#define KEY_SHORTCUT_7                  46
#define KEY_SHORTCUT_8                  47
#define KEY_SHORTCUT_9                  48
#define KEY_SHORTCUT_10                 49
#define KEY_COUNT                       50
#define NB_KEY_SHORTCUTS                10

#define ALIGN_LEFT                      0
#define ALIGN_RIGHT                     1
#define ALIGN_CENTER                    2

#define ACTION_NONE                     0
#define ACTION_MAXIMIZE                 1
#define ACTION_SHADE                    2
#define ACTION_HIDE                     3

typedef struct _MyColor MyColor;
struct _MyColor
{
    GdkColor col;
    GdkGC *gc;
    gboolean allocated;
};

typedef struct _Settings Settings;
struct _Settings
{
    gchar *option;
    gchar *value;
    gboolean required;
};

typedef struct _Params Params;
struct _Params
{
    MyColor title_colors[2];
    MyKey keys[KEY_COUNT];
    MyPixmap buttons[BUTTON_COUNT][6];
    MyPixmap corners[4][2];
    MyPixmap sides[3][2];
    MyPixmap title[5][2];
    gchar button_layout[8];
    gchar *shortcut_exec[NB_KEY_SHORTCUTS];
    unsigned int xfwm_margins[4];
    int button_offset;
    int button_spacing;
    int dbl_click_time;
    int double_click_action;
    int raise_delay;
    int snap_width;
    int title_alignment;
    int title_horizontal_offset;
    int workspace_count;
    int wrap_resistance;
    gboolean title_shadow[2];
    gboolean box_move;
    gboolean box_resize;
    gboolean click_to_focus;
    gboolean focus_hint;
    gboolean focus_new;
    gboolean full_width_title;
    gboolean raise_on_click;
    gboolean raise_on_focus;
    gboolean snap_to_border;
    gboolean snap_to_windows;
    gboolean title_vertical_offset_active;
    gboolean title_vertical_offset_inactive;
    gboolean wrap_workspaces;
    GC box_gc;
    GdkGC *black_gc;
    GdkGC *white_gc;
};

extern Params params;

gboolean loadSettings (void);
void unloadSettings (void);
gboolean reloadSettings (int);
gboolean initSettings (void);

#endif /* INC_SETTINGS_H */
