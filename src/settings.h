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
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gdk/gdk.h>
#include "client.h"
#include "keyboard.h"

typedef struct _MyColor MyColor;
struct _MyColor
{
    GdkColor col;
    GdkGC *gc;
    gboolean allocated;
};

typedef enum
{
  INT = 0,
  STRING = 1,
} SettingType;

typedef struct _Settings Settings;

struct _Settings
{
    gchar *option;
    gchar *nick;
    gchar *blurb;
    SettingType type;
    gchar *value;
    gboolean required;
};

extern MyKey keys[KEY_COUNT];
extern MyColor title_colors[2];
extern char button_layout[8];
extern int title_alignment;
extern int full_width_title;
extern int button_spacing;
extern int button_offset;
extern int title_vertical_offset;
extern int double_click_action;
extern int box_move;
extern int box_resize;
extern int click_to_focus;
extern int focus_new;
extern int focus_hint;
extern int raise_on_focus;
extern int raise_delay;
extern int snap_to_border;
extern int snap_width;
extern int dbl_click_time;
extern GC box_gc;
extern MyPixmap sides[3][2];
extern MyPixmap corners[4][2];
extern MyPixmap buttons[BUTTON_COUNT][3];
extern MyPixmap title[5][2];

void loadSettings(void);
void unloadSettings(void);
void reloadSettings(void);
void initSettings(void);

#endif /* __SETTINGS_H__ */
