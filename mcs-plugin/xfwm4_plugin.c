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

        gnome theme-switcher capplet - (c) Jonathan Blandford <jrb@gnome.org>
        xfce4 mcs plugin   - (c) 2002 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include <libxfce4mcs/mcs-common.h>
#include <libxfce4mcs/mcs-manager.h>
#include <xfce-mcs-manager/manager-plugin.h>
#include <libxfcegui4/libxfcegui4.h>
#include "inline-icon.h"

#define _(String) String

#define CHANNEL "xfwm4"
#define RCFILE  "xfwm4.xml"
#define PLUGIN_NAME "xfwm4"

#define DEFAULT_THEME "microdeck2"
#define DEFAULT_KEY_THEME "default.keys"
#define DEFAULT_LAYOUT "OTS|HMC"
#define DEFAULT_ACTION "maximize"
#define DEFAULT_ALIGN "center"

#define DEFAULT_ICON_SIZE 48
#define MAX_ELEMENTS_BEFORE_SCROLLING 6
#ifndef DATADIR
#define DATADIR "/usr/local/share"
#endif

#ifdef HAVE_GDK_PIXBUF_NEW_FROM_STREAM
#define gdk_pixbuf_new_from_inline gdk_pixbuf_new_from_stream
#endif

#define STATES 8
#define STATE_HIDDEN (STATES - 1)

typedef enum
{
    DECORATION_THEMES = 0,
    KEYBINDING_THEMES = 1
}
ThemeType;

enum
{
    TITLE = 0,
    MENU,
    STICK,
    SHADE,
    HIDE,
    MAXIMIZE,
    CLOSE,
    END
};

typedef struct _TitleRadioButton TitleRadioButton;
struct _TitleRadioButton
{
    GtkWidget *radio_buttons[STATES];
    guint active;
    GSList *radio_group;
};

typedef struct _TitleButton TitleButton;
struct _TitleButton
{
    gchar *label;
    gchar code;
};

typedef struct _TitleButtonData TitleButtonData;
struct _TitleButtonData
{
    guint row;
    guint col;
    gpointer data;
};

typedef struct _RadioTmpl RadioTmpl;
struct _RadioTmpl
{
    gchar *label;
    gchar *action;
    GtkWidget *radio_button;
};

TitleButton title_buttons[] = {
    {"Title", '|'},
    {"Menu", 'O'},
    {"Stick", 'T'},
    {"Shade", 'S'},
    {"Hide", 'H'},
    {"Maximize", 'M'},
    {"Close", 'C'}
};

RadioTmpl dbl_click_values[] = {
    {"Shade window", "shade", NULL},
    {"Hide window", "hide", NULL},
    {"Maximize window", "maximize", NULL},
    {"Nothing", "none", NULL},
    {NULL, NULL, NULL}
};

RadioTmpl title_align_values[] = {
    {"Left", "left", NULL},
    {"Center", "center", NULL},
    {"Right", "right", NULL},
    {NULL, NULL, NULL}
};

enum
{
    THEME_NAME_COLUMN,
    N_COLUMNS
};

typedef struct _ThemeInfo ThemeInfo;
struct _ThemeInfo
{
    gchar *path;
    gchar *name;
    guint has_decoration:1;
    guint has_keybinding:1;
    guint user_writable:1;
};

typedef struct _Itf Itf;
struct _Itf
{
    McsPlugin *mcs_plugin;

    GSList *click_focus_radio_group;

    GtkWidget *dialog1;
    GtkWidget *dialog_vbox1;
    GtkWidget *dialog_header;
    GtkWidget *notebook1;
    GtkWidget *hbox1;
    GtkWidget *frame1;
    GtkWidget *vbox1;
    GtkWidget *hbox2;
    GtkWidget *scrolledwindow1;
    GtkWidget *treeview1;
    GtkWidget *label4;
    GtkWidget *vbox2;
    GtkWidget *frame2;
    GtkWidget *label5;
    GtkWidget *label1;
    GtkWidget *hbox3;
    GtkWidget *frame3;
    GtkWidget *vbox3;
    GtkWidget *hbox4;
    GtkWidget *scrolledwindow2;
    GtkWidget *treeview2;
    GtkWidget *label32;
    GtkWidget *vbox4;
    GtkWidget *frame4;
    GtkWidget *hbox5;
    GtkWidget *click_focus_radio;
    GtkWidget *focus_follow_mouse_radio;
    GtkWidget *label33;
    GtkWidget *frame5;
    GtkWidget *focus_new_check;
    GtkWidget *label34;
    GtkWidget *frame6;
    GtkWidget *vbox6;
    GtkWidget *raise_on_focus_check;
    GtkWidget *table2;
    GtkWidget *label37;
    GtkWidget *label38;
    GtkWidget *label39;
    GtkWidget *raise_delay_scale;
    GtkWidget *label35;
    GtkWidget *frame13;
    GtkWidget *click_raise_check;
    GtkWidget *label49;
    GtkWidget *label2;
    GtkWidget *vbox5;
    GtkWidget *frame8;
    GtkWidget *vbox7;
    GtkWidget *snap_to_border_check;
    GtkWidget *table3;
    GtkWidget *label41;
    GtkWidget *label42;
    GtkWidget *label43;
    GtkWidget *snap_width_scale;
    GtkWidget *label40;
    GtkWidget *frame9;
    GtkWidget *wrap_workspaces_check;
    GtkWidget *label44;
    GtkWidget *frame10;
    GtkWidget *box_resize_check;
    GtkWidget *label45;
    GtkWidget *frame11;
    GtkWidget *box_move_check;
    GtkWidget *label46;
    GtkWidget *frame12;
    GtkWidget *label47;
    GtkWidget *frame14;
    GtkWidget *label48;
    GtkWidget *label3;
    GtkWidget *dialog_action_area1;
    GtkWidget *closebutton1;
};

static void create_channel(McsPlugin * mcs_plugin);
static gboolean write_options(McsPlugin * mcs_plugin);
static void run_dialog(McsPlugin * mcs_plugin);

static gboolean setting_model = FALSE;
static gboolean is_running = FALSE;
static gchar *current_theme = NULL;
static gchar *current_key_theme = NULL;
static gchar *current_layout = NULL;
static gchar *dbl_click_action = NULL;
static gchar *title_align = NULL;
static gboolean click_to_focus = TRUE;
static gboolean focus_new = TRUE;
static gboolean focus_raise = FALSE;
static gboolean raise_on_click = TRUE;
static gboolean snap_to_border = TRUE;
static gboolean wrap_workspaces = TRUE;
static gboolean box_move = FALSE;
static gboolean box_resize = FALSE;
static int raise_delay;
static int snap_width;
static TitleRadioButton title_radio_buttons[END];

GList *decoration_theme_list = NULL;
GList *keybinding_theme_list = NULL;

static GdkPixbuf *default_icon_at_size(int width, int height)
{

    GdkPixbuf *base;

    base = gdk_pixbuf_new_from_inline(-1, default_icon_data, FALSE, NULL);

    g_assert(base);

    if((width < 0 && height < 0) || (gdk_pixbuf_get_width(base) == width && gdk_pixbuf_get_height(base) == height))
    {
        return base;
    }
    else
    {
        GdkPixbuf *scaled;

        scaled = gdk_pixbuf_scale_simple(base, width > 0 ? width : gdk_pixbuf_get_width(base), height > 0 ? height : gdk_pixbuf_get_height(base), GDK_INTERP_BILINEAR);

        g_object_unref(G_OBJECT(base));

        return scaled;
    }
}

static void cb_layout_destroy_button(GtkWidget * widget, gpointer user_data)
{
    g_free(user_data);
}

static void cb_layout_value_changed(GtkWidget * widget, gpointer user_data)
{
    static guint recursive = 0;
    guint i, j, k, l, m = 0;
    guint col, row;
    TitleButtonData *callback_data = (TitleButtonData *) user_data;
    McsPlugin *mcs_plugin = (McsPlugin *) callback_data->data;
    gchar result[STATES];

    if(recursive != 0)
    {
        return;
    }
    if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
    {
        return;
    }
    ++recursive;
    col = callback_data->col;
    row = callback_data->row;

    for(i = TITLE; i < END; i++)
    {
        if((i != row) && (title_radio_buttons[i].active == col))
        {
            for(j = 0; j < STATES; j++)
            {
                if((i == TITLE) && (title_radio_buttons[row].active == STATE_HIDDEN))
                {
                    gboolean in_use;

                    in_use = TRUE;
                    for(l = 0; (l < STATE_HIDDEN) && in_use; l++)
                    {
                        in_use = FALSE;
                        for(k = TITLE; k < END; k++)
                        {
                            if(title_radio_buttons[k].active == l)
                            {
                                in_use = TRUE;
                            }
                        }
                        if(!in_use)
                        {
                            m = l;
                        }
                    }
                    if(!in_use)
                    {
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(title_radio_buttons[TITLE].radio_buttons[m]), TRUE);
                        title_radio_buttons[TITLE].active = m;
                        break;
                    }
                }
                else if((col < STATE_HIDDEN) && title_radio_buttons[row].active == j)
                {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(title_radio_buttons[i].radio_buttons[j]), TRUE);
                    title_radio_buttons[i].active = j;
                    break;
                }
            }
        }
    }
    title_radio_buttons[row].active = col;

    j = 0;

    for(l = 0; l < STATE_HIDDEN; l++)
    {
        for(k = TITLE; k < END; k++)
        {
            if(title_radio_buttons[k].active == l)
            {
                result[j++] = title_buttons[k].code;
            }
        }
    }
    result[j++] = '\0';

    if(current_layout)
    {
        g_free(current_layout);
    }

    current_layout = g_strdup(result);
    mcs_manager_set_string(mcs_plugin->manager, "Xfwm/ButtonLayout", CHANNEL, current_layout);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);
    write_options(mcs_plugin);

    --recursive;
}

static GtkWidget *create_layout_buttons(gchar * layout, gpointer user_data)
{
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *label_row;
    GtkWidget *radio_button;
    GSList *radio_button_group;
    TitleButtonData *callback_data;
    gchar *temp;
    guint i, j, len;
    gboolean visible;

    g_return_val_if_fail(layout != NULL, NULL);
    len = strlen(layout);
    g_return_val_if_fail(len > 0, NULL);

    table = gtk_table_new(8, 9, FALSE);
    gtk_widget_show(table);
    gtk_container_set_border_width(GTK_CONTAINER(table), 5);

    label = gtk_label_new(_("Layout :"));
    gtk_widget_show(label);
    gtk_table_attach(GTK_TABLE(table), label, 0, 9, 0, 1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    for(i = TITLE; i < END; i++)
    {
        temp = g_strdup_printf("<small><i>%s :</i></small> ", title_buttons[i].label);
        label_row = gtk_label_new(temp);
        gtk_widget_show(label_row);
        gtk_table_attach(GTK_TABLE(table), label_row, 0, 1, i + 1, i + 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
        gtk_label_set_use_markup(GTK_LABEL(label_row), TRUE);
        gtk_label_set_justify(GTK_LABEL(label_row), GTK_JUSTIFY_LEFT);
        gtk_misc_set_alignment(GTK_MISC(label_row), 0, 0.5);

        radio_button_group = NULL;
        visible = FALSE;

        for(j = 0; j < STATES - 1; j++)
        {
            radio_button = gtk_radio_button_new(NULL);
            gtk_widget_show(radio_button);
            gtk_table_attach(GTK_TABLE(table), radio_button, j + 1, j + 2, i + 1, i + 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
            gtk_radio_button_set_group(GTK_RADIO_BUTTON(radio_button), radio_button_group);
            radio_button_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button));
            title_radio_buttons[i].radio_buttons[j] = radio_button;

            if((j < len) && (layout[j] == title_buttons[i].code))
            {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);
                visible = TRUE;
                title_radio_buttons[i].active = j;
            }
            callback_data = g_new(TitleButtonData, 1);
            callback_data->row = i;
            callback_data->col = j;
            callback_data->data = user_data;
            g_signal_connect(G_OBJECT(radio_button), "toggled", G_CALLBACK(cb_layout_value_changed), callback_data);
            g_signal_connect_after(G_OBJECT(radio_button), "destroy", G_CALLBACK(cb_layout_destroy_button), callback_data);
        }

        if(i != TITLE)
        {
            radio_button = gtk_radio_button_new_with_mnemonic(NULL, _("Hidden"));
            gtk_widget_show(radio_button);
            gtk_table_attach(GTK_TABLE(table), radio_button, STATES, STATES + 1, i + 1, i + 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
            gtk_radio_button_set_group(GTK_RADIO_BUTTON(radio_button), radio_button_group);
            radio_button_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button));
            title_radio_buttons[i].radio_buttons[STATES - 1] = radio_button;

            if(!visible)
            {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);
                title_radio_buttons[i].active = (STATES - 1);
            }
            callback_data = g_new(TitleButtonData, 1);
            callback_data->row = i;
            callback_data->col = STATES - 1;
            callback_data->data = user_data;
            g_signal_connect(G_OBJECT(radio_button), "toggled", G_CALLBACK(cb_layout_value_changed), callback_data);
            g_signal_connect_after(G_OBJECT(radio_button), "destroy", G_CALLBACK(cb_layout_destroy_button), callback_data);
        }
    }
    return (table);
}

static GtkWidget *create_radio_button_table(RadioTmpl template[], guint size, gchar * display_label, gchar * value, GCallback handler, gpointer user_data)
{
    GtkWidget *table;
    GtkWidget *label;
    GSList *radio_group = NULL;
    guint i;

    table = gtk_table_new(size, 2, FALSE);
    gtk_widget_show(table);
    gtk_container_set_border_width(GTK_CONTAINER(table), 5);

    label = gtk_label_new(display_label);

    gtk_widget_show(label);
    gtk_table_attach(GTK_TABLE(table), label, 0, size, 0, 1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

    for(i = 0; i < size; i++)
    {
        template[i].radio_button = gtk_radio_button_new_with_mnemonic(NULL, _(template[i].label));
        gtk_widget_show(template[i].radio_button);
        gtk_table_attach(GTK_TABLE(table), template[i].radio_button, i, i + 1, 1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), (GtkAttachOptions) (0), 0, 0);
        gtk_radio_button_set_group(GTK_RADIO_BUTTON(template[i].radio_button), radio_group);
        radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(template[i].radio_button));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(template[i].radio_button), strcmp(value, template[i].action) ? FALSE : TRUE);
        g_signal_connect(G_OBJECT(template[i].radio_button), "toggled", G_CALLBACK(handler), user_data);
    }

    return table;
}

static void theme_info_free(ThemeInfo * info)
{
    g_free(info->path);
    g_free(info->name);
    g_free(info);
}

static ThemeInfo *find_theme_info_by_dir(const gchar * theme_dir, GList * theme_list)
{
    GList *list;

    for(list = theme_list; list; list = list->next)
    {
        ThemeInfo *info = list->data;

        if(!strcmp(info->path, theme_dir))
            return info;
    }

    return NULL;
}

static GList *update_theme_dir(const gchar * theme_dir, GList * theme_list)
{
    ThemeInfo *info = NULL;
    gboolean has_decoration = FALSE;
    gboolean has_keybinding = FALSE;
    GList *list = theme_list;

    gchar *tmp;

    if(g_str_has_suffix(theme_dir, ".keys"))
    {
        tmp = g_build_filename(theme_dir, "keythemerc", NULL);
        if(g_file_test(tmp, G_FILE_TEST_IS_REGULAR))
        {
            has_keybinding = TRUE;
        }
        g_free(tmp);
    }
    else
    {
        tmp = g_build_filename(theme_dir, "themerc", NULL);
        if(g_file_test(tmp, G_FILE_TEST_IS_REGULAR))
        {
            has_decoration = TRUE;
        }
        g_free(tmp);
    }

    info = find_theme_info_by_dir(theme_dir, list);

    if(info)
    {
        if(!has_decoration && !has_keybinding)
        {
            list = g_list_remove(list, info);
            theme_info_free(info);
        }
        else if((info->has_keybinding != has_keybinding) || (info->has_decoration != has_decoration))
        {
            info->has_keybinding = has_keybinding;
            info->has_decoration = has_decoration;
        }
    }
    else
    {
        if(has_decoration || has_keybinding)
        {
            info = g_new0(ThemeInfo, 1);
            info->path = g_strdup(theme_dir);
            info->name = g_strdup(strrchr(theme_dir, G_DIR_SEPARATOR) + 1);
            info->has_decoration = has_decoration;
            info->has_keybinding = has_keybinding;

            list = g_list_prepend(list, info);
        }
    }

    return list;
}

static GList *themes_common_list_add_dir(const char *dirname, GList * theme_list)
{
    DIR *dir;
    struct dirent *de;
    GList *list = theme_list;

    g_return_val_if_fail(dirname != NULL, list);

    dir = opendir(dirname);

    if(!dir)
        return list;

    while((de = readdir(dir)))
    {
        char *tmp;

        if(de->d_name[0] == '.')
            continue;

        tmp = g_build_filename(dirname, de->d_name, NULL);
        list = update_theme_dir(tmp, list);
        g_free(tmp);
    }
    closedir(dir);

    return list;
}


static GList *theme_common_init(GList * theme_list)
{
    gchar *dir;
    GList *list = theme_list;

    dir = g_build_filename(g_get_home_dir(), ".themes", G_DIR_SEPARATOR_S, "xfwm4", NULL);
    list = themes_common_list_add_dir(dir, list);
    g_free(dir);

    dir = g_build_filename(DATADIR, "themes", NULL);
    list = themes_common_list_add_dir(dir, list);
    g_free(dir);

    return list;
}

static void decoration_selection_changed(GtkTreeSelection * selection, gpointer data)
{
    GtkTreeModel *model;
    gchar *new_theme;
    GtkTreeIter iter;
    McsPlugin *mcs_plugin;

    if(setting_model)
        return;

    mcs_plugin = (McsPlugin *) data;

    if(gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        gtk_tree_model_get(model, &iter, THEME_NAME_COLUMN, &new_theme, -1);
    }
    else
    {
        new_theme = NULL;
    }

    if(new_theme != NULL)
    {
        if(current_theme && strcmp(current_theme, new_theme))
        {
            g_free(current_theme);
            current_theme = new_theme;
            mcs_manager_set_string(mcs_plugin->manager, "Xfwm/ThemeName", CHANNEL, current_theme);
            mcs_manager_notify(mcs_plugin->manager, CHANNEL);
            write_options(mcs_plugin);
        }
    }
}

static void keybinding_selection_changed(GtkTreeSelection * selection, gpointer data)
{
    GtkTreeModel *model;
    gchar *new_key_theme;
    GtkTreeIter iter;
    McsPlugin *mcs_plugin;

    if(setting_model)
        return;

    mcs_plugin = (McsPlugin *) data;

    if(gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        gtk_tree_model_get(model, &iter, THEME_NAME_COLUMN, &new_key_theme, -1);
    }
    else
    {
        new_key_theme = NULL;
    }

    if(new_key_theme != NULL)
    {
        if(current_key_theme && strcmp(current_key_theme, new_key_theme))
        {
            g_free(current_key_theme);
            current_key_theme = new_key_theme;
            mcs_manager_set_string(mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL, current_key_theme);
            mcs_manager_notify(mcs_plugin->manager, CHANNEL);
            write_options(mcs_plugin);
        }
    }
}

static GList *read_themes(GList * theme_list, GtkWidget * treeview, GtkWidget * swindow, ThemeType type, gchar * current_value)
{
    GList *list;
    GList *new_list = theme_list;
    GtkTreeModel *model;
    GtkTreeView *tree_view;
    gint i = 0;
    gboolean current_value_found = FALSE;
    GtkTreeRowReference *row_ref = NULL;

    new_list = theme_common_init(new_list);
    tree_view = GTK_TREE_VIEW(treeview);
    model = gtk_tree_view_get_model(tree_view);

    setting_model = TRUE;
    gtk_list_store_clear(GTK_LIST_STORE(model));

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swindow), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_widget_set_usize(swindow, -1, -1);

    for(list = new_list; list; list = list->next)
    {
        ThemeInfo *info = list->data;
        GtkTreeIter iter;

        if(((type == DECORATION_THEMES) && !(info->has_decoration)) || ((type == KEYBINDING_THEMES) && !(info->has_keybinding)))
            continue;

        gtk_list_store_prepend(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, THEME_NAME_COLUMN, info->name, -1);

        if(strcmp(current_value, info->name) == 0)
        {
            GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
            row_ref = gtk_tree_row_reference_new(model, path);
            gtk_tree_path_free(path);
            current_value_found = TRUE;
        }

        if(i == MAX_ELEMENTS_BEFORE_SCROLLING)
        {
            GtkRequisition rectangle;
            gtk_widget_size_request(GTK_WIDGET(tree_view), &rectangle);
            gtk_widget_set_usize(swindow, -1, rectangle.height);
            gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        }
        i++;
    }

    if(!current_value_found)
    {
        GtkTreeIter iter;
        GtkTreePath *path;

        gtk_list_store_prepend(GTK_LIST_STORE(model), &iter);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, THEME_NAME_COLUMN, current_value, -1);

        path = gtk_tree_model_get_path(model, &iter);
        row_ref = gtk_tree_row_reference_new(model, path);
        gtk_tree_path_free(path);
    }

    if(row_ref)
    {
        GtkTreePath *path;

        path = gtk_tree_row_reference_get_path(row_ref);
        gtk_tree_view_set_cursor(tree_view, path, NULL, FALSE);
        gtk_tree_view_scroll_to_cell(tree_view, path, NULL, TRUE, 0.5, 0.0);
        gtk_tree_path_free(path);
        gtk_tree_row_reference_free(row_ref);
    }
    setting_model = FALSE;

    return new_list;
}

static gint sort_func(GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b, gpointer user_data)
{
    gchar *a_str = NULL;
    gchar *b_str = NULL;

    gtk_tree_model_get(model, a, 0, &a_str, -1);
    gtk_tree_model_get(model, b, 0, &b_str, -1);

    if(a_str == NULL)
        a_str = g_strdup("");
    if(b_str == NULL)
        b_str = g_strdup("");

    if(!strcmp(a_str, DEFAULT_THEME))
        return -1;
    if(!strcmp(b_str, DEFAULT_THEME))
        return 1;

    return g_utf8_collate(a_str, b_str);
}

static void cb_click_to_focus_changed(GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    click_to_focus = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(itf->click_focus_radio));

    mcs_manager_set_int(mcs_plugin->manager, "Xfwm/ClickToFocus", CHANNEL, click_to_focus ? 1 : 0);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);
    write_options(mcs_plugin);
}

static void cb_focus_new_changed(GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    focus_new = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(itf->focus_new_check));

    mcs_manager_set_int(mcs_plugin->manager, "Xfwm/FocusNewWindow", CHANNEL, focus_new ? 1 : 0);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);
    write_options(mcs_plugin);
}

static void cb_raise_on_focus_changed(GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    focus_raise = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(itf->raise_on_focus_check));
    gtk_widget_set_sensitive(itf->raise_delay_scale, focus_raise);

    mcs_manager_set_int(mcs_plugin->manager, "Xfwm/FocusRaise", CHANNEL, focus_raise ? 1 : 0);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);
    write_options(mcs_plugin);
}

static void cb_raise_delay_changed(GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    raise_delay = (int)gtk_range_get_value(GTK_RANGE(itf->raise_delay_scale));
    mcs_manager_set_int(mcs_plugin->manager, "Xfwm/RaiseDelay", CHANNEL, raise_delay);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);
    write_options(mcs_plugin);
}

static void cb_raise_on_click_changed(GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    raise_on_click = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(itf->click_raise_check));

    mcs_manager_set_int(mcs_plugin->manager, "Xfwm/RaiseOnClick", CHANNEL, raise_on_click ? 1 : 0);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);
    write_options(mcs_plugin);
}

static void cb_snap_to_border_changed(GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    snap_to_border = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(itf->snap_to_border_check));
    gtk_widget_set_sensitive(itf->snap_width_scale, snap_to_border);

    mcs_manager_set_int(mcs_plugin->manager, "Xfwm/SnapToBorder", CHANNEL, snap_to_border ? 1 : 0);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);
    write_options(mcs_plugin);
}

static void cb_snap_width_changed(GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    snap_width = (int)gtk_range_get_value(GTK_RANGE(itf->snap_width_scale));
    mcs_manager_set_int(mcs_plugin->manager, "Xfwm/SnapWidth", CHANNEL, snap_width);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);
    write_options(mcs_plugin);
}

static void cb_wrap_workspaces_changed(GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    wrap_workspaces = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(itf->wrap_workspaces_check));

    mcs_manager_set_int(mcs_plugin->manager, "Xfwm/WrapWorkspaces", CHANNEL, wrap_workspaces ? 1 : 0);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);
    write_options(mcs_plugin);
}

static void cb_box_move_changed(GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    box_move = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(itf->box_move_check));

    mcs_manager_set_int(mcs_plugin->manager, "Xfwm/BoxMove", CHANNEL, box_move ? 1 : 0);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);
    write_options(mcs_plugin);
}

static void cb_box_resize_changed(GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    box_resize = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(itf->box_resize_check));

    mcs_manager_set_int(mcs_plugin->manager, "Xfwm/BoxResize", CHANNEL, box_resize ? 1 : 0);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);
    write_options(mcs_plugin);
}

static void cb_dblclick_action_value_changed(GtkWidget * widget, gpointer user_data)
{
    guint i;
    McsPlugin *mcs_plugin = (McsPlugin *) user_data;

    if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
    {
        return;
    }

    for(i = 0; i < 4; i++)
    {
        if(widget == dbl_click_values[i].radio_button)
        {
            if(dbl_click_action)
            {
                g_free(dbl_click_action);
            }
            dbl_click_action = g_strdup(dbl_click_values[i].action);
            mcs_manager_set_string(mcs_plugin->manager, "Xfwm/DblClickAction", CHANNEL, dbl_click_action);
            mcs_manager_notify(mcs_plugin->manager, CHANNEL);
            write_options(mcs_plugin);
        }
    }
}

static void cb_title_align_value_changed(GtkWidget * widget, gpointer user_data)
{
    guint i;
    McsPlugin *mcs_plugin = (McsPlugin *) user_data;

    if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
    {
        return;
    }

    for(i = 0; i < 3; i++)
    {
        if(widget == title_align_values[i].radio_button)
        {
            if(title_align)
            {
                g_free(title_align);
            }
            title_align = g_strdup(title_align_values[i].action);
            mcs_manager_set_string(mcs_plugin->manager, "Xfwm/TitleAlign", CHANNEL, title_align);
            mcs_manager_notify(mcs_plugin->manager, CHANNEL);
            write_options(mcs_plugin);
        }
    }
}

static void cb_dialog_response(GtkWidget * dialog, gint response_id)
{
    if(response_id == GTK_RESPONSE_HELP)
    {
        g_message("HELP: TBD");
    }
    else
    {
        is_running = FALSE;
        gtk_widget_destroy(dialog);
    }
}

Itf *create_dialog(McsPlugin * mcs_plugin)
{
    Itf *dialog;
    GdkPixbuf *icon;

    dialog = g_new(Itf, 1);

    dialog->mcs_plugin = mcs_plugin;

    dialog->dialog1 = gtk_dialog_new();

    icon = default_icon_at_size(32, 32);
    gtk_window_set_icon(GTK_WINDOW(dialog->dialog1), icon);
    g_object_unref(icon);

    dialog->click_focus_radio_group = NULL;

    dialog->dialog_vbox1 = GTK_DIALOG(dialog->dialog1)->vbox;
    gtk_widget_show(dialog->dialog_vbox1);

    dialog->dialog_header = create_header(icon, _("Window Manager Preferences"));
    gtk_widget_show(dialog->dialog_header);
    gtk_box_pack_start(GTK_BOX(dialog->dialog_vbox1), dialog->dialog_header, TRUE, TRUE, 0);

    dialog->notebook1 = gtk_notebook_new();
    gtk_widget_show(dialog->notebook1);
    gtk_box_pack_start(GTK_BOX(dialog->dialog_vbox1), dialog->notebook1, TRUE, TRUE, 0);

    dialog->hbox1 = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(dialog->hbox1);
    gtk_container_add(GTK_CONTAINER(dialog->notebook1), dialog->hbox1);

    dialog->frame1 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame1);
    gtk_box_pack_start(GTK_BOX(dialog->hbox1), dialog->frame1, TRUE, TRUE, 0);

    dialog->vbox1 = gtk_vbox_new(FALSE, 5);
    gtk_widget_show(dialog->vbox1);
    gtk_container_add(GTK_CONTAINER(dialog->frame1), dialog->vbox1);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->vbox1), 5);

    dialog->hbox2 = gtk_hbox_new(FALSE, 8);
    gtk_widget_show(dialog->hbox2);
    gtk_box_pack_start(GTK_BOX(dialog->vbox1), dialog->hbox2, TRUE, TRUE, 0);

    dialog->scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(dialog->scrolledwindow1);
    gtk_box_pack_start(GTK_BOX(dialog->hbox2), dialog->scrolledwindow1, TRUE, TRUE, 0);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(dialog->scrolledwindow1), GTK_SHADOW_IN);

    dialog->treeview1 = gtk_tree_view_new();
    gtk_widget_show(dialog->treeview1);
    gtk_container_add(GTK_CONTAINER(dialog->scrolledwindow1), dialog->treeview1);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(dialog->treeview1), FALSE);

    dialog->label4 = gtk_label_new(_("Window style"));
    gtk_widget_show(dialog->label4);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame1), dialog->label4);
    gtk_label_set_justify(GTK_LABEL(dialog->label4), GTK_JUSTIFY_LEFT);

    dialog->vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(dialog->vbox2);
    gtk_box_pack_start(GTK_BOX(dialog->hbox1), dialog->vbox2, TRUE, TRUE, 0);

    dialog->frame14 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame14);
    gtk_box_pack_start(GTK_BOX(dialog->vbox2), dialog->frame14, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(dialog->frame14), create_radio_button_table(title_align_values, 3, _("Text alignment inside title bar :"), title_align, G_CALLBACK(cb_title_align_value_changed), mcs_plugin));

    dialog->label48 = gtk_label_new(_("Title Alignment"));
    gtk_widget_show(dialog->label48);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame14), dialog->label48);
    gtk_label_set_justify(GTK_LABEL(dialog->label48), GTK_JUSTIFY_LEFT);

    dialog->frame2 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame2);
    gtk_box_pack_start(GTK_BOX(dialog->vbox2), dialog->frame2, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(dialog->frame2), create_layout_buttons(current_layout, mcs_plugin));

    dialog->label5 = gtk_label_new(_("Button layout"));
    gtk_widget_show(dialog->label5);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame2), dialog->label5);
    gtk_label_set_justify(GTK_LABEL(dialog->label5), GTK_JUSTIFY_LEFT);

    dialog->label1 = gtk_label_new(_("Decoration style"));
    gtk_widget_show(dialog->label1);
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(dialog->notebook1), gtk_notebook_get_nth_page(GTK_NOTEBOOK(dialog->notebook1), 0), dialog->label1);
    gtk_label_set_justify(GTK_LABEL(dialog->label1), GTK_JUSTIFY_LEFT);

    dialog->hbox3 = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(dialog->hbox3);
    gtk_container_add(GTK_CONTAINER(dialog->notebook1), dialog->hbox3);

    dialog->frame3 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame3);
    gtk_box_pack_start(GTK_BOX(dialog->hbox3), dialog->frame3, TRUE, TRUE, 0);

    dialog->vbox3 = gtk_vbox_new(FALSE, 5);
    gtk_widget_show(dialog->vbox3);
    gtk_container_add(GTK_CONTAINER(dialog->frame3), dialog->vbox3);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->vbox3), 5);

    dialog->hbox4 = gtk_hbox_new(FALSE, 8);
    gtk_widget_show(dialog->hbox4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox3), dialog->hbox4, TRUE, TRUE, 0);

    dialog->scrolledwindow2 = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(dialog->scrolledwindow2);
    gtk_box_pack_start(GTK_BOX(dialog->hbox4), dialog->scrolledwindow2, TRUE, TRUE, 0);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(dialog->scrolledwindow2), GTK_SHADOW_IN);

    dialog->treeview2 = gtk_tree_view_new();
    gtk_widget_show(dialog->treeview2);
    gtk_container_add(GTK_CONTAINER(dialog->scrolledwindow2), dialog->treeview2);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(dialog->treeview2), FALSE);

    dialog->label32 = gtk_label_new(_("Keyboard Shortcut"));
    gtk_widget_show(dialog->label32);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame3), dialog->label32);
    gtk_label_set_justify(GTK_LABEL(dialog->label32), GTK_JUSTIFY_LEFT);

    dialog->vbox4 = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(dialog->vbox4);
    gtk_box_pack_start(GTK_BOX(dialog->hbox3), dialog->vbox4, TRUE, TRUE, 0);

    dialog->frame4 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame4);
    gtk_box_pack_start(GTK_BOX(dialog->vbox4), dialog->frame4, TRUE, TRUE, 0);

    dialog->hbox5 = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(dialog->hbox5);
    gtk_container_add(GTK_CONTAINER(dialog->frame4), dialog->hbox5);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->hbox5), 5);

    dialog->click_focus_radio = gtk_radio_button_new_with_mnemonic(NULL, _("Click to focus"));
    gtk_widget_show(dialog->click_focus_radio);
    gtk_box_pack_start(GTK_BOX(dialog->hbox5), dialog->click_focus_radio, TRUE, FALSE, 0);
    gtk_radio_button_set_group(GTK_RADIO_BUTTON(dialog->click_focus_radio), dialog->click_focus_radio_group);
    dialog->click_focus_radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(dialog->click_focus_radio));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->click_focus_radio), click_to_focus);

    dialog->focus_follow_mouse_radio = gtk_radio_button_new_with_mnemonic(NULL, _("Focus follows mouse"));
    gtk_widget_show(dialog->focus_follow_mouse_radio);
    gtk_box_pack_start(GTK_BOX(dialog->hbox5), dialog->focus_follow_mouse_radio, TRUE, FALSE, 0);
    gtk_radio_button_set_group(GTK_RADIO_BUTTON(dialog->focus_follow_mouse_radio), dialog->click_focus_radio_group);
    dialog->click_focus_radio_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(dialog->focus_follow_mouse_radio));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->focus_follow_mouse_radio), !click_to_focus);

    dialog->label33 = gtk_label_new(_("Focus model"));
    gtk_widget_show(dialog->label33);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame4), dialog->label33);
    gtk_label_set_justify(GTK_LABEL(dialog->label33), GTK_JUSTIFY_LEFT);

    dialog->frame5 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame5);
    gtk_box_pack_start(GTK_BOX(dialog->vbox4), dialog->frame5, TRUE, TRUE, 0);

    dialog->focus_new_check = gtk_check_button_new_with_mnemonic(_("Automatically give focus to \nnewly created windows"));
    gtk_widget_show(dialog->focus_new_check);
    gtk_container_add(GTK_CONTAINER(dialog->frame5), dialog->focus_new_check);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->focus_new_check), 5);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->focus_new_check), focus_new);

    dialog->label34 = gtk_label_new(_("New window focus"));
    gtk_widget_show(dialog->label34);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame5), dialog->label34);
    gtk_label_set_justify(GTK_LABEL(dialog->label34), GTK_JUSTIFY_LEFT);

    dialog->frame6 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame6);
    gtk_box_pack_start(GTK_BOX(dialog->vbox4), dialog->frame6, TRUE, TRUE, 0);

    dialog->vbox6 = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(dialog->vbox6);
    gtk_container_add(GTK_CONTAINER(dialog->frame6), dialog->vbox6);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->vbox6), 5);

    dialog->raise_on_focus_check = gtk_check_button_new_with_mnemonic(_("Automatically raise windows \nwhen they receive focus"));
    gtk_widget_show(dialog->raise_on_focus_check);
    gtk_box_pack_start(GTK_BOX(dialog->vbox6), dialog->raise_on_focus_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->raise_on_focus_check), focus_raise);

    dialog->table2 = gtk_table_new(2, 3, FALSE);
    gtk_widget_show(dialog->table2);
    gtk_box_pack_start(GTK_BOX(dialog->vbox6), dialog->table2, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->table2), 5);

    dialog->label37 = gtk_label_new(_("Delay before raising focused window :"));
    gtk_widget_show(dialog->label37);
    gtk_table_attach(GTK_TABLE(dialog->table2), dialog->label37, 0, 3, 0, 1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(dialog->label37), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(dialog->label37), 0, 0.5);

    dialog->label38 = gtk_label_new(_("<small><i>Slow</i></small>"));
    gtk_widget_show(dialog->label38);
    gtk_table_attach(GTK_TABLE(dialog->table2), dialog->label38, 0, 1, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_use_markup(GTK_LABEL(dialog->label38), TRUE);
    gtk_label_set_justify(GTK_LABEL(dialog->label38), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(dialog->label38), 1, 0.5);

    dialog->label39 = gtk_label_new(_("<small><i>Fast</i></small>"));
    gtk_widget_show(dialog->label39);
    gtk_table_attach(GTK_TABLE(dialog->table2), dialog->label39, 2, 3, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_use_markup(GTK_LABEL(dialog->label39), TRUE);
    gtk_label_set_justify(GTK_LABEL(dialog->label39), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(dialog->label39), 0, 0.5);

    dialog->raise_delay_scale = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(raise_delay, 100, 2000, 10, 100, 0)));
    gtk_widget_show(dialog->raise_delay_scale);
    gtk_table_attach(GTK_TABLE(dialog->table2), dialog->raise_delay_scale, 1, 2, 1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_scale_set_draw_value(GTK_SCALE(dialog->raise_delay_scale), FALSE);
    gtk_scale_set_digits(GTK_SCALE(dialog->raise_delay_scale), 0);
    gtk_range_set_update_policy(GTK_RANGE(dialog->raise_delay_scale), GTK_UPDATE_DISCONTINUOUS);
    gtk_range_set_inverted(GTK_RANGE(dialog->raise_delay_scale), TRUE);
    gtk_widget_set_sensitive(dialog->raise_delay_scale, focus_raise);

    dialog->label35 = gtk_label_new(_("Raise on focus"));
    gtk_widget_show(dialog->label35);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame6), dialog->label35);
    gtk_label_set_justify(GTK_LABEL(dialog->label35), GTK_JUSTIFY_LEFT);

    dialog->frame13 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame13);
    gtk_box_pack_start(GTK_BOX(dialog->vbox4), dialog->frame13, TRUE, TRUE, 0);

    dialog->click_raise_check = gtk_check_button_new_with_mnemonic(_("Raise window when clicking inside\napplication window"));
    gtk_widget_show(dialog->click_raise_check);
    gtk_container_add(GTK_CONTAINER(dialog->frame13), dialog->click_raise_check);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->click_raise_check), 5);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->click_raise_check), raise_on_click);

    dialog->label49 = gtk_label_new(_("Raise on click"));
    gtk_widget_show(dialog->label49);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame13), dialog->label49);
    gtk_label_set_justify(GTK_LABEL(dialog->label49), GTK_JUSTIFY_LEFT);

    dialog->label2 = gtk_label_new(_("Keyboard and focus"));
    gtk_widget_show(dialog->label2);
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(dialog->notebook1), gtk_notebook_get_nth_page(GTK_NOTEBOOK(dialog->notebook1), 1), dialog->label2);
    gtk_label_set_justify(GTK_LABEL(dialog->label2), GTK_JUSTIFY_LEFT);

    dialog->vbox5 = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(dialog->vbox5);
    gtk_container_add(GTK_CONTAINER(dialog->notebook1), dialog->vbox5);

    dialog->frame8 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame8);
    gtk_box_pack_start(GTK_BOX(dialog->vbox5), dialog->frame8, TRUE, TRUE, 0);

    dialog->vbox7 = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(dialog->vbox7);
    gtk_container_add(GTK_CONTAINER(dialog->frame8), dialog->vbox7);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->vbox7), 5);

    dialog->snap_to_border_check = gtk_check_button_new_with_mnemonic(_("Snap windows to screen border"));
    gtk_widget_show(dialog->snap_to_border_check);
    gtk_box_pack_start(GTK_BOX(dialog->vbox7), dialog->snap_to_border_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->snap_to_border_check), snap_to_border);

    dialog->table3 = gtk_table_new(2, 3, FALSE);
    gtk_widget_show(dialog->table3);
    gtk_box_pack_start(GTK_BOX(dialog->vbox7), dialog->table3, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->table3), 5);

    dialog->label41 = gtk_label_new(_("Distance :"));
    gtk_widget_show(dialog->label41);
    gtk_table_attach(GTK_TABLE(dialog->table3), dialog->label41, 0, 3, 0, 1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify(GTK_LABEL(dialog->label41), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(dialog->label41), 0, 0.5);

    dialog->label42 = gtk_label_new(_("<small><i>Small</i></small>"));
    gtk_widget_show(dialog->label42);
    gtk_table_attach(GTK_TABLE(dialog->table3), dialog->label42, 0, 1, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_use_markup(GTK_LABEL(dialog->label42), TRUE);
    gtk_label_set_justify(GTK_LABEL(dialog->label42), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(dialog->label42), 1, 0.5);

    dialog->label43 = gtk_label_new(_("<small><i>Wide</i></small>"));
    gtk_widget_show(dialog->label43);
    gtk_table_attach(GTK_TABLE(dialog->table3), dialog->label43, 2, 3, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_use_markup(GTK_LABEL(dialog->label43), TRUE);
    gtk_label_set_justify(GTK_LABEL(dialog->label43), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(dialog->label43), 0, 0.5);

    dialog->snap_width_scale = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(snap_width, 5, 50, 5, 10, 0)));
    gtk_widget_show(dialog->snap_width_scale);
    gtk_table_attach(GTK_TABLE(dialog->table3), dialog->snap_width_scale, 1, 2, 1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_scale_set_draw_value(GTK_SCALE(dialog->snap_width_scale), FALSE);
    gtk_scale_set_digits(GTK_SCALE(dialog->snap_width_scale), 0);
    gtk_range_set_update_policy(GTK_RANGE(dialog->snap_width_scale), GTK_UPDATE_DISCONTINUOUS);
    gtk_widget_set_sensitive(dialog->snap_width_scale, snap_to_border);

    dialog->label40 = gtk_label_new(_("Snap to screen border"));
    gtk_widget_show(dialog->label40);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame8), dialog->label40);
    gtk_label_set_justify(GTK_LABEL(dialog->label40), GTK_JUSTIFY_LEFT);

    dialog->frame9 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame9);
    gtk_box_pack_start(GTK_BOX(dialog->vbox5), dialog->frame9, TRUE, TRUE, 0);

    dialog->wrap_workspaces_check = gtk_check_button_new_with_mnemonic(_("Wrap workspaces when dragging a window off the screen"));
    gtk_widget_show(dialog->wrap_workspaces_check);
    gtk_container_add(GTK_CONTAINER(dialog->frame9), dialog->wrap_workspaces_check);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->wrap_workspaces_check), 5);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->wrap_workspaces_check), wrap_workspaces);

    dialog->label44 = gtk_label_new(_("Wrap workspaces"));
    gtk_widget_show(dialog->label44);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame9), dialog->label44);
    gtk_label_set_justify(GTK_LABEL(dialog->label44), GTK_JUSTIFY_LEFT);

    dialog->frame10 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame10);
    gtk_box_pack_start(GTK_BOX(dialog->vbox5), dialog->frame10, TRUE, TRUE, 0);

    dialog->box_resize_check = gtk_check_button_new_with_mnemonic(_("Display content of windows when resizing"));
    gtk_widget_show(dialog->box_resize_check);
    gtk_container_add(GTK_CONTAINER(dialog->frame10), dialog->box_resize_check);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->box_resize_check), 5);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->box_resize_check), !box_resize);

    dialog->label45 = gtk_label_new(_("Opaque resize"));
    gtk_widget_show(dialog->label45);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame10), dialog->label45);
    gtk_label_set_justify(GTK_LABEL(dialog->label45), GTK_JUSTIFY_LEFT);

    dialog->frame11 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame11);
    gtk_box_pack_start(GTK_BOX(dialog->vbox5), dialog->frame11, TRUE, TRUE, 0);

    dialog->box_move_check = gtk_check_button_new_with_mnemonic(_("Display content of windows when moving"));
    gtk_widget_show(dialog->box_move_check);
    gtk_container_add(GTK_CONTAINER(dialog->frame11), dialog->box_move_check);
    gtk_container_set_border_width(GTK_CONTAINER(dialog->box_move_check), 5);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->box_move_check), !box_move);

    dialog->label46 = gtk_label_new(_("Opaque move"));
    gtk_widget_show(dialog->label46);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame11), dialog->label46);
    gtk_label_set_justify(GTK_LABEL(dialog->label46), GTK_JUSTIFY_LEFT);

    dialog->frame12 = gtk_frame_new(NULL);
    gtk_widget_show(dialog->frame12);
    gtk_box_pack_start(GTK_BOX(dialog->vbox5), dialog->frame12, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(dialog->frame12), create_radio_button_table(dbl_click_values, 4, _("Action to perform when double clicking on title bar :"), dbl_click_action, G_CALLBACK(cb_dblclick_action_value_changed), mcs_plugin));

    dialog->label47 = gtk_label_new(_("Double click action"));
    gtk_widget_show(dialog->label47);
    gtk_frame_set_label_widget(GTK_FRAME(dialog->frame12), dialog->label47);
    gtk_label_set_justify(GTK_LABEL(dialog->label47), GTK_JUSTIFY_LEFT);

    dialog->label3 = gtk_label_new(_("Misc."));
    gtk_widget_show(dialog->label3);
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(dialog->notebook1), gtk_notebook_get_nth_page(GTK_NOTEBOOK(dialog->notebook1), 2), dialog->label3);
    gtk_label_set_justify(GTK_LABEL(dialog->label3), GTK_JUSTIFY_LEFT);

    dialog->dialog_action_area1 = GTK_DIALOG(dialog->dialog1)->action_area;
    gtk_widget_show(dialog->dialog_action_area1);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(dialog->dialog_action_area1), GTK_BUTTONBOX_END);

    dialog->closebutton1 = gtk_button_new_from_stock("gtk-close");
    gtk_widget_show(dialog->closebutton1);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog->dialog1), dialog->closebutton1, GTK_RESPONSE_CLOSE);
    GTK_WIDGET_SET_FLAGS(dialog->closebutton1, GTK_CAN_DEFAULT);

    gtk_widget_grab_focus(dialog->closebutton1);
    gtk_widget_grab_default(dialog->closebutton1);

    return dialog;
}

static void setup_dialog(Itf * itf)
{
    GtkTreeModel *model1, *model2;
    GtkTreeSelection *selection;

    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(itf->treeview1), -1, NULL, gtk_cell_renderer_text_new(), "text", THEME_NAME_COLUMN, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(itf->treeview2), -1, NULL, gtk_cell_renderer_text_new(), "text", THEME_NAME_COLUMN, NULL);

    model1 = (GtkTreeModel *) gtk_list_store_new(N_COLUMNS, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model1), 0, sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model1), 0, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(itf->treeview1), model1);

    model2 = (GtkTreeModel *) gtk_list_store_new(N_COLUMNS, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model2), 0, sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model2), 0, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(itf->treeview2), model2);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(itf->treeview1));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    g_signal_connect(G_OBJECT(selection), "changed", (GCallback) decoration_selection_changed, itf->mcs_plugin);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(itf->treeview2));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
    g_signal_connect(G_OBJECT(selection), "changed", (GCallback) keybinding_selection_changed, itf->mcs_plugin);

    decoration_theme_list = read_themes(decoration_theme_list, itf->treeview1, itf->scrolledwindow1, DECORATION_THEMES, current_theme);
    keybinding_theme_list = read_themes(keybinding_theme_list, itf->treeview2, itf->scrolledwindow2, KEYBINDING_THEMES, current_key_theme);

    g_signal_connect(G_OBJECT(itf->dialog1), "response", G_CALLBACK(cb_dialog_response), itf->mcs_plugin);
    g_signal_connect(G_OBJECT(itf->click_focus_radio), "toggled", G_CALLBACK(cb_click_to_focus_changed), itf);
    g_signal_connect(G_OBJECT(itf->focus_new_check), "toggled", G_CALLBACK(cb_focus_new_changed), itf);
    g_signal_connect(G_OBJECT(itf->raise_on_focus_check), "toggled", G_CALLBACK(cb_raise_on_focus_changed), itf);
    g_signal_connect(G_OBJECT(itf->raise_delay_scale), "value_changed", (GCallback) cb_raise_delay_changed, itf);
    g_signal_connect(G_OBJECT(itf->click_raise_check), "toggled", G_CALLBACK(cb_raise_on_click_changed), itf);
    g_signal_connect(G_OBJECT(itf->snap_to_border_check), "toggled", G_CALLBACK(cb_snap_to_border_changed), itf);
    g_signal_connect(G_OBJECT(itf->snap_width_scale), "value_changed", (GCallback) cb_snap_width_changed, itf);
    g_signal_connect(G_OBJECT(itf->wrap_workspaces_check), "toggled", G_CALLBACK(cb_wrap_workspaces_changed), itf);
    g_signal_connect(G_OBJECT(itf->box_move_check), "toggled", (GCallback) cb_box_move_changed, itf);
    g_signal_connect(G_OBJECT(itf->box_resize_check), "toggled", G_CALLBACK(cb_box_resize_changed), itf);

    gtk_widget_show(itf->dialog1);
}

McsPluginInitResult mcs_plugin_init(McsPlugin * mcs_plugin)
{
    create_channel(mcs_plugin);
    mcs_plugin->plugin_name = g_strdup(PLUGIN_NAME);
    mcs_plugin->caption = g_strdup(_("Window Manager"));
    mcs_plugin->run_dialog = run_dialog;
    mcs_plugin->icon = default_icon_at_size(DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
    mcs_manager_notify(mcs_plugin->manager, CHANNEL);

    return (MCS_PLUGIN_INIT_OK);
}

static void create_channel(McsPlugin * mcs_plugin)
{
    McsSetting *setting;

    const gchar *home = g_getenv("HOME");
    gchar *rcfile = g_strconcat(home, G_DIR_SEPARATOR_S, ".xfce4", G_DIR_SEPARATOR_S, RCFILE, NULL);
    mcs_manager_add_channel_from_file(mcs_plugin->manager, CHANNEL, rcfile);
    g_free(rcfile);

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL);
    if(setting)
    {
        if(current_key_theme)
        {
            g_free(current_key_theme);
        }
        current_key_theme = g_strdup(setting->data.v_string);
    }
    else
    {
        if(current_key_theme)
        {
            g_free(current_key_theme);
        }

        current_key_theme = g_strdup(DEFAULT_KEY_THEME);
        mcs_manager_set_string(mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL, current_key_theme);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/ThemeName", CHANNEL);
    if(setting)
    {
        if(current_theme)
        {
            g_free(current_theme);
        }
        current_theme = g_strdup(setting->data.v_string);
    }
    else
    {
        if(current_theme)
        {
            g_free(current_theme);
        }

        current_theme = g_strdup(DEFAULT_THEME);
        mcs_manager_set_string(mcs_plugin->manager, "Xfwm/ThemeName", CHANNEL, current_theme);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/DblClickAction", CHANNEL);
    if(setting)
    {
        if(title_align)
        {
            g_free(title_align);
        }
        title_align = g_strdup(setting->data.v_string);
    }
    else
    {
        if(title_align)
        {
            g_free(title_align);
        }

        title_align = g_strdup(DEFAULT_ALIGN);
        mcs_manager_set_string(mcs_plugin->manager, "Xfwm/TitleAlign", CHANNEL, title_align);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/ButtonLayout", CHANNEL);
    if(setting)
    {
        if(current_layout)
        {
            g_free(current_layout);
        }
        current_layout = g_strdup(setting->data.v_string);
    }
    else
    {
        if(current_layout)
        {
            g_free(current_layout);
        }

        current_layout = g_strdup(DEFAULT_LAYOUT);
        mcs_manager_set_string(mcs_plugin->manager, "Xfwm/ButtonLayout", CHANNEL, current_layout);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/ClickToFocus", CHANNEL);
    if(setting)
    {
        click_to_focus = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        click_to_focus = TRUE;
        mcs_manager_set_int(mcs_plugin->manager, "Xfwm/ClickToFocus", CHANNEL, click_to_focus ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/FocusNewWindow", CHANNEL);
    if(setting)
    {
        focus_new = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        focus_new = TRUE;
        mcs_manager_set_int(mcs_plugin->manager, "Xfwm/FocusNewWindow", CHANNEL, focus_new ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/FocusRaise", CHANNEL);
    if(setting)
    {
        focus_raise = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        focus_raise = FALSE;
        mcs_manager_set_int(mcs_plugin->manager, "Xfwm/FocusRaise", CHANNEL, focus_raise ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/RaiseDelay", CHANNEL);
    if(setting)
    {
        raise_delay = setting->data.v_int;
    }
    else
    {
        raise_delay = 250;
        mcs_manager_set_int(mcs_plugin->manager, "Xfwm/RaiseDelay", CHANNEL, raise_delay);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/RaiseOnClick", CHANNEL);
    if(setting)
    {
        raise_on_click = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        raise_on_click = TRUE;
        mcs_manager_set_int(mcs_plugin->manager, "Xfwm/RaiseOnClick", CHANNEL, focus_raise ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/SnapToBorder", CHANNEL);
    if(setting)
    {
        snap_to_border = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        snap_to_border = TRUE;
        mcs_manager_set_int(mcs_plugin->manager, "Xfwm/SnapToBorder", CHANNEL, snap_to_border ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/SnapWidth", CHANNEL);
    if(setting)
    {
        snap_width = setting->data.v_int;
    }
    else
    {
        snap_width = 10;
        mcs_manager_set_int(mcs_plugin->manager, "Xfwm/SnapWidth", CHANNEL, snap_width);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/WrapWorkspaces", CHANNEL);
    if(setting)
    {
        wrap_workspaces = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        wrap_workspaces = TRUE;
        mcs_manager_set_int(mcs_plugin->manager, "Xfwm/WrapWorkspaces", CHANNEL, wrap_workspaces ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/BoxMove", CHANNEL);
    if(setting)
    {
        box_move = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        box_move = FALSE;
        mcs_manager_set_int(mcs_plugin->manager, "Xfwm/BoxMove", CHANNEL, box_move ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/BoxResize", CHANNEL);
    if(setting)
    {
        box_resize = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        box_resize = FALSE;
        mcs_manager_set_int(mcs_plugin->manager, "Xfwm/BoxResize", CHANNEL, box_resize ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup(mcs_plugin->manager, "Xfwm/DblClickAction", CHANNEL);
    if(setting)
    {
        if(dbl_click_action)
        {
            g_free(dbl_click_action);
        }
        dbl_click_action = g_strdup(setting->data.v_string);
    }
    else
    {
        if(dbl_click_action)
        {
            g_free(dbl_click_action);
        }

        dbl_click_action = g_strdup(DEFAULT_ACTION);
        mcs_manager_set_string(mcs_plugin->manager, "Xfwm/DblClickAction", CHANNEL, dbl_click_action);
    }
}

static gboolean write_options(McsPlugin * mcs_plugin)
{
    const gchar *home = g_getenv("HOME");
    gchar *rcfile = g_strconcat(home, G_DIR_SEPARATOR_S, ".xfce4", G_DIR_SEPARATOR_S, RCFILE, NULL);
    gboolean result;

    result = mcs_manager_save_channel_to_file(mcs_plugin->manager, CHANNEL, rcfile);
    g_free(rcfile);

    return result;
}

static void run_dialog(McsPlugin * mcs_plugin)
{
    Itf *dialog;

    if(is_running)
        return;

    is_running = TRUE;

    dialog = create_dialog(mcs_plugin);
    setup_dialog(dialog);
}

/* macro defined in manager-plugin.h */
MCS_PLUGIN_CHECK_INIT
