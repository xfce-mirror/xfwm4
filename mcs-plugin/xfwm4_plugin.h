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
 
        xfce4 mcs plugin   - (c) 2002 Olivier Fourdan
 
 */

#ifndef __XFWM4_PLUGIN_H
#define __XFWM4_PLUGIN_H

#define RCDIR   "settings"
#define RCFILE  "xfwm4.xml"
#define CHANNEL "xfwm4"
#define PLUGIN_NAME "xfwm4"

#define DEFAULT_THEME "Default"
#define DEFAULT_KEY_THEME "Default"
#define DEFAULT_LAYOUT "OTS|HMC"
#define DEFAULT_ACTION "maximize"
#define DEFAULT_ALIGN "center"
#define DEFAULT_FONT "Sans Bold 10"

#define DEFAULT_ICON_SIZE 48
#define MAX_ELEMENTS_BEFORE_SCROLLING 6

#define SUFFIX      "xfwm4"
#define KEY_SUFFIX  "xfwm4"
#define KEYTHEMERC  "keythemerc"
#define THEMERC     "themerc"

#define STATES 8
#define STATE_HIDDEN (STATES - 1)

#define BORDER 5


typedef struct _ThemeInfo ThemeInfo;
struct _ThemeInfo
{
    gchar *path;
    gchar *name;
    gboolean has_decoration;
    gboolean has_keybinding;
    gboolean set_layout;
    gboolean set_align;
    gboolean set_font;
    gboolean user_writable;
};


typedef struct _shortcut_tree_foreach_struct shortcut_tree_foreach_struct;
struct _shortcut_tree_foreach_struct
{
    gchar *key;
    gboolean modifier_control;
    gboolean modifier_shift;
    gboolean modifier_alt;
    gchar *path;
    gboolean found;
};

typedef struct _Itf Itf;
struct _Itf
{
    McsPlugin *mcs_plugin;

    GSList *click_focus_radio_group;

    GtkWidget *box_move_check;
    GtkWidget *box_resize_check;
    GtkWidget *click_focus_radio;
    GtkWidget *click_raise_check;
    GtkWidget *closebutton1;
    GtkWidget *helpbutton1;
    GtkWidget *dialog_action_area1;
    GtkWidget *dialog_header;
    GtkWidget *dialog_vbox;
    GtkWidget *focus_follow_mouse_radio;
    GtkWidget *focus_new_check;
    GtkWidget *font_button;
    GtkWidget *font_selection;
    GtkWidget *frame_layout;
    GtkWidget *frame_align;
    GtkWidget *raise_delay_scale;
    GtkWidget *raise_on_focus_check;
    GtkWidget *scrolledwindow1;
    GtkWidget *scrolledwindow2;
    GtkWidget *scrolledwindow3;
    GtkWidget *scrolledwindow4;
    GtkWidget *snap_to_border_check;
    GtkWidget *snap_to_windows_check;
    GtkWidget *snap_width_scale;
    GtkWidget *treeview1;
    GtkWidget *treeview2;
    GtkWidget *treeview3;
    GtkWidget *treeview4;
    GtkWidget *wrap_workspaces_check;
    GtkWidget *wrap_windows_check;
    GtkWidget *wrap_resistance_scale;
    GtkWidget *xfwm4_dialog;
};

enum
{
    COLUMN_COMMAND,
    COLUMN_SHORTCUT,
    NUM_COLUMNS
};

extern gchar *current_key_theme;
extern GList *keybinding_theme_list;

extern ThemeInfo *find_theme_info_by_name (const gchar *, GList *);

#endif
