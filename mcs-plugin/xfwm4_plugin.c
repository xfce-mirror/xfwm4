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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <errno.h>
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
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <xfce-mcs-manager/manager-plugin.h>
#include "xfwm4-icon.h"

#include "xfwm4_plugin.h"
#include "xfwm4_shortcuteditor.h"

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

typedef struct _MenuTmpl MenuTmpl;
struct _MenuTmpl
{
    gchar *label;
    gchar *action;
};

TitleButton title_buttons[] = {
    {N_("Title"), '|'},
    {N_("Menu"), 'O'},
    {N_("Stick"), 'T'},
    {N_("Shade"), 'S'},
    {N_("Hide"), 'H'},
    {N_("Maximize"), 'M'},
    {N_("Close"), 'C'}
};

MenuTmpl dbl_click_values[] = {
    {N_("Shade window"), "shade"},
    {N_("Hide window"), "hide"},
    {N_("Maximize window"), "maximize"},
    {N_("Nothing"), "none"},
    {NULL, NULL}
};

MenuTmpl title_align_values[] = {
    {N_("Left"), "left"},
    {N_("Center"), "center"},
    {N_("Right"), "right"},
    {NULL, NULL}
};

enum
{
    THEME_NAME_COLUMN,
    N_COLUMNS
};

static void create_channel (McsPlugin * mcs_plugin);
static gboolean write_options (McsPlugin * mcs_plugin);
static void run_dialog (McsPlugin * mcs_plugin);

static gboolean setting_model = FALSE;
static gboolean is_running = FALSE;
static gchar *current_theme = NULL;
gchar *current_key_theme = NULL;
static gchar *current_layout = NULL;
static gchar *current_font = NULL;
static gchar *dbl_click_action = NULL;
static gchar *title_align = NULL;
static gboolean click_to_focus = TRUE;
static gboolean focus_new = TRUE;
static gboolean focus_raise = FALSE;
static gboolean raise_on_click = TRUE;
static gboolean snap_to_border = TRUE;
static gboolean snap_to_windows = FALSE;
static gboolean wrap_workspaces = FALSE;
static gboolean wrap_windows = TRUE;
static gboolean box_move = FALSE;
static gboolean box_resize = FALSE;
static int raise_delay;
static int snap_width;
static int wrap_resistance;
static TitleRadioButton title_radio_buttons[END];

static GList *decoration_theme_list = NULL;
GList *keybinding_theme_list = NULL;

static void
sensitive_cb (GtkWidget * widget, gpointer user_data)
{
    gtk_widget_set_sensitive (widget, (gboolean) GPOINTER_TO_INT (user_data));
}

static void
cb_layout_destroy_button (GtkWidget * widget, gpointer user_data)
{
    g_free (user_data);
}

static void
cb_layout_value_changed (GtkWidget * widget, gpointer user_data)
{
    static guint recursive = 0;
    guint i, j, k, l, m = 0;
    guint col, row;
    TitleButtonData *callback_data = (TitleButtonData *) user_data;
    McsPlugin *mcs_plugin = (McsPlugin *) callback_data->data;
    gchar result[STATES];

    if (recursive != 0)
    {
        return;
    }
    if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
        return;
    }
    ++recursive;
    col = callback_data->col;
    row = callback_data->row;

    for (i = TITLE; i < END; i++)
    {
        if ((i != row) && (title_radio_buttons[i].active == col))
        {
            for (j = 0; j < STATES; j++)
            {
                if ((i == TITLE) && (title_radio_buttons[row].active == STATE_HIDDEN))
                {
                    gboolean in_use;

                    in_use = TRUE;
                    for (l = 0; (l < STATE_HIDDEN) && in_use; l++)
                    {
                        in_use = FALSE;
                        for (k = TITLE; k < END; k++)
                        {
                            if (title_radio_buttons[k].active == l)
                            {
                                in_use = TRUE;
                            }
                        }
                        if (!in_use)
                        {
                            m = l;
                        }
                    }
                    if (!in_use)
                    {
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (title_radio_buttons[TITLE].radio_buttons[m]), TRUE);
                        title_radio_buttons[TITLE].active = m;
                        break;
                    }
                }
                else if ((col < STATE_HIDDEN) && title_radio_buttons[row].active == j)
                {
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (title_radio_buttons[i].radio_buttons[j]), TRUE);
                    title_radio_buttons[i].active = j;
                    break;
                }
            }
        }
    }
    title_radio_buttons[row].active = col;

    j = 0;

    for (l = 0; l < STATE_HIDDEN; l++)
    {
        for (k = TITLE; k < END; k++)
        {
            if (title_radio_buttons[k].active == l)
            {
                result[j++] = title_buttons[k].code;
            }
        }
    }
    result[j++] = '\0';

    if (current_layout)
    {
        g_free (current_layout);
    }

    current_layout = g_strdup (result);
    mcs_manager_set_string (mcs_plugin->manager, "Xfwm/ButtonLayout", CHANNEL, current_layout);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);

    --recursive;
}

static GtkWidget *
create_layout_buttons (gchar * layout, gpointer user_data)
{
    GtkWidget *table;
    GtkWidget *label_row;
    GtkWidget *radio_button;
    GSList *radio_button_group;
    TitleButtonData *callback_data;
    gchar *temp;
    guint i, j, len;
    gboolean visible;

    g_return_val_if_fail (layout != NULL, NULL);
    len = strlen (layout);
    g_return_val_if_fail (len > 0, NULL);

    table = gtk_table_new (8, 9, FALSE);
    gtk_widget_show (table);
    gtk_container_set_border_width (GTK_CONTAINER (table), BORDER);

    /*    label = gtk_label_new(_("Layout :"));
       gtk_widget_show(label);
       gtk_table_attach(GTK_TABLE(table), label, 0, 9, 0, 1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
       gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
       gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
     */
    for (i = TITLE; i < END; i++)
    {
        temp = g_strdup_printf ("<small><i>%s :</i></small> ", _(title_buttons[i].label));
        label_row = gtk_label_new (temp);
        gtk_widget_show (label_row);
        gtk_table_attach (GTK_TABLE (table), label_row, 0, 1, i + 1, i + 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
        gtk_label_set_use_markup (GTK_LABEL (label_row), TRUE);
        gtk_label_set_justify (GTK_LABEL (label_row), GTK_JUSTIFY_LEFT);
        gtk_misc_set_alignment (GTK_MISC (label_row), 0, 0.5);

        radio_button_group = NULL;
        visible = FALSE;

        for (j = 0; j < STATES - 1; j++)
        {
            radio_button = gtk_radio_button_new (NULL);
            gtk_widget_show (radio_button);
            gtk_table_attach (GTK_TABLE (table), radio_button, j + 1, j + 2, i + 1, i + 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
            gtk_radio_button_set_group (GTK_RADIO_BUTTON (radio_button), radio_button_group);
            radio_button_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_button));
            title_radio_buttons[i].radio_buttons[j] = radio_button;

            if ((j < len) && (layout[j] == title_buttons[i].code))
            {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), TRUE);
                visible = TRUE;
                title_radio_buttons[i].active = j;
            }
            callback_data = g_new (TitleButtonData, 1);
            callback_data->row = i;
            callback_data->col = j;
            callback_data->data = user_data;
            g_signal_connect (G_OBJECT (radio_button), "toggled", G_CALLBACK (cb_layout_value_changed), callback_data);
            g_signal_connect_after (G_OBJECT (radio_button), "destroy", G_CALLBACK (cb_layout_destroy_button), callback_data);
        }

        if (i != TITLE)
        {
            radio_button = gtk_radio_button_new_with_mnemonic (NULL, _("Hidden"));
            gtk_widget_show (radio_button);
            gtk_table_attach (GTK_TABLE (table), radio_button, STATES, STATES + 1, i + 1, i + 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
            gtk_radio_button_set_group (GTK_RADIO_BUTTON (radio_button), radio_button_group);
            radio_button_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio_button));
            title_radio_buttons[i].radio_buttons[STATES - 1] = radio_button;

            if (!visible)
            {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_button), TRUE);
                title_radio_buttons[i].active = (STATES - 1);
            }
            callback_data = g_new (TitleButtonData, 1);
            callback_data->row = i;
            callback_data->col = STATES - 1;
            callback_data->data = user_data;
            g_signal_connect (G_OBJECT (radio_button), "toggled", G_CALLBACK (cb_layout_value_changed), callback_data);
            g_signal_connect_after (G_OBJECT (radio_button), "destroy", G_CALLBACK (cb_layout_destroy_button), callback_data);
        }
    }
    return (table);
}

static GtkWidget *
create_option_menu_box (MenuTmpl template[], guint size, gchar * display_label, gchar * value, GCallback handler, gpointer user_data)
{
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *menu;
    GtkWidget *omenu;
    GtkWidget *item;
    guint n;

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    omenu = gtk_option_menu_new ();
    gtk_box_pack_start (GTK_BOX (hbox), omenu, TRUE, TRUE, 0);
    gtk_widget_show (omenu);

    menu = gtk_menu_new ();
    gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
    gtk_widget_show (menu);

    for (n = 0; n < size; n++)
    {
        item = gtk_menu_item_new_with_mnemonic (_(template[n].label));
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        if (strcmp (value, template[n].action) == 0)
            gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), n);

        g_object_set_data (G_OBJECT (item), "user-data", template[n].action);

        g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (handler), user_data);
    }

    return (vbox);
}

static void
theme_info_free (ThemeInfo * info)
{
    g_free (info->path);
    g_free (info->name);
    g_free (info);
}

ThemeInfo *
find_theme_info_by_name (const gchar * theme_name, GList * theme_list)
{
    GList *list;

    for (list = theme_list; list; list = list->next)
    {
        ThemeInfo *info = list->data;

        if (!strcmp (info->name, theme_name))
            return info;
    }

    return NULL;
}

static gboolean
parserc (const gchar * filename, gboolean * set_layout, gboolean * set_align, gboolean * set_font)
{
    gchar buf[80];
    gchar *lvalue, *rvalue;
    FILE *fp;

    *set_layout = FALSE;
    *set_align = FALSE;
    *set_font = FALSE;

    fp = fopen (filename, "r");
    if (!fp)
    {
        return FALSE;
    }
    while (fgets (buf, sizeof (buf), fp))
    {
        lvalue = strtok (buf, "=");
        rvalue = strtok (NULL, "\n");
        if ((lvalue) && (rvalue))
        {
            if (!g_ascii_strcasecmp (lvalue, "button_layout"))
            {
                *set_layout = TRUE;
            }
            else if (!g_ascii_strcasecmp (lvalue, "title_alignment"))
            {
                *set_align = TRUE;
            }
            else if (!g_ascii_strcasecmp (lvalue, "title_font"))
            {
                *set_font = TRUE;
            }
        }

    }
    fclose (fp);
    return TRUE;
}

static GList *
update_theme_dir (const gchar * theme_dir, GList * theme_list)
{
    ThemeInfo *info = NULL;
    gchar *theme_name;
    GList *list = theme_list;
    gboolean has_decoration = FALSE;
    gboolean has_keybinding = FALSE;
    gboolean set_layout = FALSE;
    gboolean set_align = FALSE;
    gboolean set_font = FALSE;

    gchar *tmp;

    tmp = g_build_filename (theme_dir, G_DIR_SEPARATOR_S, KEY_SUFFIX, G_DIR_SEPARATOR_S, KEYTHEMERC, NULL);
    if (g_file_test (tmp, G_FILE_TEST_IS_REGULAR) && parserc (tmp, &set_layout, &set_align, &set_font))
    {
        has_keybinding = TRUE;
    }
    g_free (tmp);

    tmp = g_build_filename (theme_dir, G_DIR_SEPARATOR_S, SUFFIX, G_DIR_SEPARATOR_S, THEMERC, NULL);
    if (g_file_test (tmp, G_FILE_TEST_IS_REGULAR) && parserc (tmp, &set_layout, &set_align, &set_font))
    {
        has_decoration = TRUE;
    }
    g_free (tmp);

    theme_name = g_strdup (strrchr (theme_dir, G_DIR_SEPARATOR) + 1);
    info = find_theme_info_by_name (theme_name, list);

    if (info)
    {
        if (!has_decoration && !has_keybinding)
        {
            list = g_list_remove (list, info);
            theme_info_free (info);
        }
        else if ((info->has_keybinding != has_keybinding)
            || (info->has_decoration != has_decoration) || (info->set_layout != set_layout) || (info->set_align != set_align) || (info->set_font != set_font))
        {
            info->has_keybinding = has_keybinding;
            info->has_decoration = has_decoration;
            info->set_layout = set_layout;
            info->set_align = set_align;
            info->set_font = set_font;
        }
    }
    else
    {
        if (has_decoration || has_keybinding)
        {
            info = g_new0 (ThemeInfo, 1);
            info->path = g_strdup (theme_dir);
            info->name = g_strdup (theme_name);
            info->has_decoration = has_decoration;
            info->has_keybinding = has_keybinding;
            info->set_layout = set_layout;
            info->set_align = set_align;
            info->set_font = set_font;

            list = g_list_prepend (list, info);
        }
    }

    g_free (theme_name);
    return list;
}

static GList *
themes_common_list_add_dir (const char *dirname, GList * theme_list)
{
#ifdef HAVE_OPENDIR
    struct dirent *de;
    gchar *tmp;
    DIR *dir;

    g_return_val_if_fail (dirname != NULL, theme_list);

    if ((dir = opendir (dirname)) != NULL)
    {
        while ((de = readdir (dir)))
        {
            if (de->d_name[0] == '.')
                continue;

            tmp = g_build_filename (dirname, de->d_name, NULL);
            theme_list = update_theme_dir (tmp, theme_list);
            g_free (tmp);
        }

        closedir (dir);
    }
#endif

    return (theme_list);
}


static GList *
theme_common_init (GList * theme_list)
{
    gchar *dir;
    GList *list = theme_list;

    dir = xfce_get_homefile (".themes", NULL);
    list = themes_common_list_add_dir (dir, list);
    g_free (dir);

    dir = g_build_filename (DATADIR, "themes", NULL);
    list = themes_common_list_add_dir (dir, list);
    g_free (dir);

    return list;
}

static gboolean
dialog_update_from_theme (Itf * itf, const gchar * theme_name, GList * theme_list)
{
    ThemeInfo *info = NULL;

    g_return_val_if_fail (theme_name != NULL, FALSE);
    g_return_val_if_fail (theme_list != NULL, FALSE);

    info = find_theme_info_by_name (theme_name, theme_list);
    if (info)
    {
        gtk_container_foreach (GTK_CONTAINER (itf->frame_layout), sensitive_cb, GINT_TO_POINTER ((gint) ! (info->set_layout)));
        gtk_container_foreach (GTK_CONTAINER (itf->frame_align), sensitive_cb, GINT_TO_POINTER ((gint) ! (info->set_align)));
        gtk_widget_set_sensitive (itf->font_button, !(info->set_font));
        return TRUE;
    }
    return FALSE;
}

static void
decoration_selection_changed (GtkTreeSelection * selection, gpointer data)
{
    GtkTreeModel *model;
    gchar *new_theme;
    GtkTreeIter iter;
    Itf *itf;
    McsPlugin *mcs_plugin;

    g_return_if_fail (data != NULL);

    if (setting_model)
    {
        return;
    }

    itf = (Itf *) data;
    mcs_plugin = itf->mcs_plugin;

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, THEME_NAME_COLUMN, &new_theme, -1);
    }
    else
    {
        new_theme = NULL;
    }

    if (new_theme != NULL)
    {
        if (current_theme && strcmp (current_theme, new_theme))
        {
            g_free (current_theme);
            current_theme = new_theme;
            dialog_update_from_theme (itf, current_theme, decoration_theme_list);
            mcs_manager_set_string (mcs_plugin->manager, "Xfwm/ThemeName", CHANNEL, current_theme);
            mcs_manager_notify (mcs_plugin->manager, CHANNEL);
            write_options (mcs_plugin);
        }
    }
}

static void
keybinding_selection_changed (GtkTreeSelection * selection, gpointer data)
{
    GtkTreeModel *model;
    gchar *new_key_theme;
    GtkTreeIter iter;
    Itf *itf;
    McsPlugin *mcs_plugin;

    g_return_if_fail (data != NULL);

    if (setting_model)
    {
        return;
    }

    itf = (Itf *) data;
    mcs_plugin = itf->mcs_plugin;

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, THEME_NAME_COLUMN, &new_key_theme, -1);
    }
    else
    {
        new_key_theme = NULL;
    }

    if (new_key_theme != NULL)
    {
        if (current_key_theme && strcmp (current_key_theme, new_key_theme))
        {
            g_free (current_key_theme);
            current_key_theme = new_key_theme;
            mcs_manager_set_string (mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL, current_key_theme);
            mcs_manager_notify (mcs_plugin->manager, CHANNEL);
            write_options (mcs_plugin);
        }
    }
}

static GList *
read_themes (GList * theme_list, GtkWidget * treeview, GtkWidget * swindow, ThemeType type, gchar * current_value)
{
    GList *list;
    GList *new_list = theme_list;
    GtkTreeModel *model;
    GtkTreeView *tree_view;
    gint i = 0;
    gboolean current_value_found = FALSE;
    GtkTreeRowReference *row_ref = NULL;

    new_list = theme_common_init (new_list);
    tree_view = GTK_TREE_VIEW (treeview);
    model = gtk_tree_view_get_model (tree_view);

    setting_model = TRUE;
    gtk_list_store_clear (GTK_LIST_STORE (model));

    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_widget_set_size_request (swindow, -1, -1);

    for (list = new_list; list; list = list->next)
    {
        ThemeInfo *info = list->data;
        GtkTreeIter iter;

        if (((type == DECORATION_THEMES) && !(info->has_decoration)) || ((type == KEYBINDING_THEMES) && !(info->has_keybinding)))
            continue;

        gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter, THEME_NAME_COLUMN, info->name, -1);

        if (strcmp (current_value, info->name) == 0)
        {
            GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
            row_ref = gtk_tree_row_reference_new (model, path);
            gtk_tree_path_free (path);
            current_value_found = TRUE;
        }

        if (i == MAX_ELEMENTS_BEFORE_SCROLLING)
        {
            GtkRequisition rectangle;
            gtk_widget_size_request (GTK_WIDGET (tree_view), &rectangle);
            gtk_widget_set_size_request (swindow, -1, rectangle.height);
            gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        }
        i++;
    }

    if (!current_value_found)
    {
        GtkTreeIter iter;
        GtkTreePath *path;

        gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter, THEME_NAME_COLUMN, current_value, -1);

        path = gtk_tree_model_get_path (model, &iter);
        row_ref = gtk_tree_row_reference_new (model, path);
        gtk_tree_path_free (path);
    }

    if (row_ref)
    {
        GtkTreePath *path;

        path = gtk_tree_row_reference_get_path (row_ref);
        gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
        gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.5, 0.0);
        gtk_tree_path_free (path);
        gtk_tree_row_reference_free (row_ref);
    }
    setting_model = FALSE;

    return new_list;
}

static gint
sort_func (GtkTreeModel * model, GtkTreeIter * a, GtkTreeIter * b, gpointer user_data)
{
    gchar *a_str = NULL;
    gchar *b_str = NULL;

    gtk_tree_model_get (model, a, 0, &a_str, -1);
    gtk_tree_model_get (model, b, 0, &b_str, -1);

    if (a_str == NULL)
        a_str = g_strdup ("");
    if (b_str == NULL)
        b_str = g_strdup ("");

    if (!strcmp (a_str, DEFAULT_THEME))
        return -1;
    if (!strcmp (b_str, DEFAULT_THEME))
        return 1;

    return g_utf8_collate (a_str, b_str);
}

static void
cb_click_to_focus_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    click_to_focus = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->click_focus_radio));

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/ClickToFocus", CHANNEL, click_to_focus ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_focus_new_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    focus_new = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->focus_new_check));

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/FocusNewWindow", CHANNEL, focus_new ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_raise_on_focus_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    focus_raise = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->raise_on_focus_check));
    gtk_widget_set_sensitive (itf->raise_delay_scale, focus_raise);

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/FocusRaise", CHANNEL, focus_raise ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_raise_delay_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    raise_delay = (int) gtk_range_get_value (GTK_RANGE (itf->raise_delay_scale));
    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/RaiseDelay", CHANNEL, raise_delay);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_raise_on_click_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    raise_on_click = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->click_raise_check));

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/RaiseOnClick", CHANNEL, raise_on_click ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_snap_to_border_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    snap_to_border = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->snap_to_border_check));
    gtk_widget_set_sensitive (itf->snap_width_scale, snap_to_windows || snap_to_border);

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapToBorder", CHANNEL, snap_to_border ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_snap_to_windows_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    snap_to_windows = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->snap_to_windows_check));
    gtk_widget_set_sensitive (itf->snap_width_scale, snap_to_windows || snap_to_border);

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapToWindows", CHANNEL, snap_to_windows ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_snap_width_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    snap_width = (int) gtk_range_get_value (GTK_RANGE (itf->snap_width_scale));
    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapWidth", CHANNEL, snap_width);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_wrap_resistance_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    wrap_resistance = (int) gtk_range_get_value (GTK_RANGE (itf->wrap_resistance_scale));
    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapResistance", CHANNEL, wrap_resistance);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_wrap_workspaces_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    wrap_workspaces = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->wrap_workspaces_check));
    gtk_widget_set_sensitive (itf->wrap_resistance_scale, wrap_workspaces || wrap_windows);

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapWorkspaces", CHANNEL, wrap_workspaces ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_wrap_windows_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    wrap_windows = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->wrap_windows_check));
    gtk_widget_set_sensitive (itf->wrap_resistance_scale, wrap_workspaces || wrap_windows);

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapWindows", CHANNEL, wrap_windows ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_box_move_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    box_move = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->box_move_check));

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/BoxMove", CHANNEL, box_move ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_box_resize_changed (GtkWidget * dialog, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    box_resize = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (itf->box_resize_check));

    mcs_manager_set_int (mcs_plugin->manager, "Xfwm/BoxResize", CHANNEL, box_resize ? 1 : 0);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_dblclick_action_value_changed (GtkWidget * widget, gpointer user_data)
{
    McsPlugin *mcs_plugin = (McsPlugin *) user_data;
    const gchar *action;

    action = (const gchar *) g_object_get_data (G_OBJECT (widget), "user-data");

    if (dbl_click_action)
        g_free (dbl_click_action);

    dbl_click_action = g_strdup (action);
    mcs_manager_set_string (mcs_plugin->manager, "Xfwm/DblClickAction", CHANNEL, dbl_click_action);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
cb_title_align_value_changed (GtkWidget * widget, gpointer user_data)
{
    McsPlugin *mcs_plugin = (McsPlugin *) user_data;
    const gchar *action;

    action = (const gchar *) g_object_get_data (G_OBJECT (widget), "user-data");

    if (title_align)
        g_free (title_align);

    title_align = g_strdup (action);
    mcs_manager_set_string (mcs_plugin->manager, "Xfwm/TitleAlign", CHANNEL, title_align);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);
    write_options (mcs_plugin);
}

static void
font_selection_ok (GtkWidget * w, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;
    gchar *new_font = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG (itf->font_selection));
    McsPlugin *mcs_plugin = itf->mcs_plugin;

    if (new_font != NULL)
    {
        if (current_font && strcmp (current_font, new_font))
        {
            g_free (current_font);
            current_font = new_font;
            gtk_button_set_label (GTK_BUTTON (itf->font_button), current_font);
            mcs_manager_set_string (mcs_plugin->manager, "Xfwm/TitleFont", CHANNEL, current_font);
            mcs_manager_notify (mcs_plugin->manager, CHANNEL);
            write_options (mcs_plugin);
        }
    }

    gtk_widget_destroy (GTK_WIDGET (itf->font_selection));
    itf->font_selection = NULL;
}

static void
show_font_selection (GtkWidget * widget, gpointer user_data)
{
    Itf *itf = (Itf *) user_data;

    if (!(itf->font_selection))
    {
        itf->font_selection = gtk_font_selection_dialog_new (_("Font Selection Dialog"));

        gtk_window_set_position (GTK_WINDOW (itf->font_selection), GTK_WIN_POS_MOUSE);
        gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG (itf->font_selection), current_font);
        g_signal_connect (itf->font_selection, "destroy", G_CALLBACK (gtk_widget_destroyed), &itf->font_selection);

        g_signal_connect (GTK_FONT_SELECTION_DIALOG (itf->font_selection)->ok_button, "clicked", G_CALLBACK (font_selection_ok), user_data);
        g_signal_connect_swapped (GTK_FONT_SELECTION_DIALOG
            (itf->font_selection)->cancel_button, "clicked", G_CALLBACK (gtk_widget_destroy), itf->font_selection);

        gtk_widget_show (itf->font_selection);
    }
    else
    {
        gtk_widget_destroy (itf->font_selection);
        itf->font_selection = NULL;
    }
}

static void
cb_dialog_response (GtkWidget * dialog, gint response_id)
{
    if (response_id == GTK_RESPONSE_HELP)
    {
        GError *error = NULL;
        xfce_exec ("xfhelp4 xfwm4.html", FALSE, FALSE, &error);
        if (error)
        {
            char *msg = g_strcompress (error->message);
            xfce_warn ("%s", msg);
            g_free (msg);
            g_error_free (error);
        }
    }
    else
    {
        is_running = FALSE;
        gtk_widget_destroy (dialog);
    }
}

static void
cb_shortcuttheme_changed (GtkTreeSelection * selection, Itf * itf)
{
    ThemeInfo *ti;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *theme_name;

    if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
        gtk_tree_model_get (model, &iter, THEME_NAME_COLUMN, &theme_name, -1);

        ti = find_theme_info_by_name (theme_name, keybinding_theme_list);

        if (ti)
        {
            gchar *theme_file = g_build_filename (ti->path, G_DIR_SEPARATOR_S, KEY_SUFFIX, G_DIR_SEPARATOR_S, KEYTHEMERC, NULL);
            loadtheme_in_treeview (theme_file, itf);

            g_free (theme_file);
        }

        g_free (theme_name);
    }
}

Itf *
create_dialog (McsPlugin * mcs_plugin)
{
    Itf *dialog;
    GdkPixbuf *icon;
    GtkWidget *frame;
    GtkWidget *hbox;
    GtkWidget *label;
    GtkWidget *notebook;
    GtkWidget *table2;
    GtkWidget *table3;
    GtkWidget *table4;
    GtkWidget *vbox1;
    GtkWidget *vbox2;
    GtkWidget *vbox3;
    GtkWidget *vbox4;
    GtkWidget *vbox5;
    GtkWidget *vbox6;
    GtkWidget *vbox7;
    GtkWidget *vbox8;

    GtkCellRenderer *renderer;
    GtkListStore *model;

    dialog = g_new (Itf, 1);

    dialog->mcs_plugin = mcs_plugin;

    dialog->xfwm4_dialog = gtk_dialog_new ();
    gtk_window_set_title (GTK_WINDOW (dialog->xfwm4_dialog), _("Window Manager"));
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog->xfwm4_dialog), FALSE);

    dialog->font_selection = NULL;

    icon = xfce_inline_icon_at_size (xfwm4_icon_data, 32, 32);
    gtk_window_set_icon (GTK_WINDOW (dialog->xfwm4_dialog), icon);

    dialog->click_focus_radio_group = NULL;

    dialog->dialog_vbox = GTK_DIALOG (dialog->xfwm4_dialog)->vbox;
    gtk_widget_show (dialog->dialog_vbox);

    dialog->dialog_header = xfce_create_header (icon, _("Window Manager Preferences"));
    gtk_widget_show (dialog->dialog_header);
    gtk_box_pack_start (GTK_BOX (dialog->dialog_vbox), dialog->dialog_header, FALSE, TRUE, 0);
    g_object_unref (icon);

    notebook = gtk_notebook_new ();
    gtk_container_set_border_width (GTK_CONTAINER (notebook), BORDER + 1);
    gtk_widget_show (notebook);
    gtk_box_pack_start (GTK_BOX (dialog->dialog_vbox), notebook, TRUE, TRUE, 0);

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), BORDER);
    gtk_widget_show (hbox);
    gtk_container_add (GTK_CONTAINER (notebook), hbox);

    dialog->scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (dialog->scrolledwindow1);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->scrolledwindow1), BORDER);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->scrolledwindow1, TRUE, TRUE, 0);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (dialog->scrolledwindow1), GTK_SHADOW_IN);

    dialog->treeview1 = gtk_tree_view_new ();
    gtk_widget_show (dialog->treeview1);
    gtk_container_add (GTK_CONTAINER (dialog->scrolledwindow1), dialog->treeview1);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->treeview1), FALSE);

    vbox1 = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox1);
    gtk_box_pack_start (GTK_BOX (hbox), vbox1, TRUE, TRUE, 0);

    frame = xfce_framebox_new (_("Title font"), TRUE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox1), frame, TRUE, FALSE, 0);

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), hbox);

    dialog->font_button = gtk_button_new ();
    gtk_button_set_label (GTK_BUTTON (dialog->font_button), current_font);
    gtk_widget_show (dialog->font_button);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->font_button, TRUE, TRUE, 0);

    dialog->frame_align = xfce_framebox_new (_("Title Alignment"), TRUE);
    gtk_widget_show (dialog->frame_align);
    gtk_box_pack_start (GTK_BOX (vbox1), dialog->frame_align, TRUE, TRUE, 0);

    xfce_framebox_add (XFCE_FRAMEBOX (dialog->frame_align),
        create_option_menu_box (title_align_values, 3,
            /*XXX*/ _("Text alignment inside title bar :"), title_align, G_CALLBACK (cb_title_align_value_changed), mcs_plugin));

    dialog->frame_layout = xfce_framebox_new (_("Button layout"), TRUE);
    gtk_widget_show (dialog->frame_layout);
    gtk_box_pack_start (GTK_BOX (vbox1), dialog->frame_layout, TRUE, TRUE, 0);

    xfce_framebox_add (XFCE_FRAMEBOX (dialog->frame_layout), create_layout_buttons (current_layout, mcs_plugin));

    label = gtk_label_new (_("Style"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), BORDER);
    gtk_widget_show (hbox);
    gtk_container_add (GTK_CONTAINER (notebook), hbox);

    dialog->scrolledwindow2 = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->scrolledwindow2), BORDER);
    gtk_widget_show (dialog->scrolledwindow2);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (dialog->scrolledwindow2), GTK_SHADOW_IN);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->scrolledwindow2, FALSE, TRUE, 0);

    dialog->treeview2 = gtk_tree_view_new ();
    gtk_widget_show (dialog->treeview2);
    gtk_container_add (GTK_CONTAINER (dialog->scrolledwindow2), dialog->treeview2);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->treeview2), FALSE);

    vbox8 = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox8), BORDER);
    gtk_box_pack_start (GTK_BOX (hbox), vbox8, TRUE, TRUE, 0);
    gtk_widget_show (vbox8);

    frame = xfce_framebox_new (_("Windows shortcuts"), FALSE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox8), frame, TRUE, TRUE, 0);

    dialog->scrolledwindow3 = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (dialog->scrolledwindow3), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->scrolledwindow3), BORDER);
    gtk_widget_show (dialog->scrolledwindow3);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), dialog->scrolledwindow3);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (dialog->scrolledwindow3), GTK_SHADOW_IN);

    model = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
    dialog->treeview3 = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    gtk_widget_show (dialog->treeview3);
    gtk_container_add (GTK_CONTAINER (dialog->scrolledwindow3), dialog->treeview3);
    /* gtk_widget_set_size_request (dialog->treeview3, 250, -1); */

    /* command column */
    renderer = gtk_cell_renderer_text_new ();
    g_object_set_data (G_OBJECT (renderer), "column", (gint *) COLUMN_COMMAND);

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (dialog->treeview3), -1, _("Command"), renderer, "text", COLUMN_COMMAND, NULL);
    /* shortcut column */
    renderer = gtk_cell_renderer_text_new ();
    g_object_set_data (G_OBJECT (renderer), "column", (gint *) COLUMN_SHORTCUT);

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (dialog->treeview3), -1, _("Shortcut"), renderer, "text", COLUMN_SHORTCUT, NULL);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->treeview3), TRUE);

    frame = xfce_framebox_new (_("Command shortcuts"), FALSE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox8), frame, TRUE, TRUE, 0);

    dialog->scrolledwindow4 = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (dialog->scrolledwindow4), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->scrolledwindow4), BORDER);
    gtk_widget_show (dialog->scrolledwindow4);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), dialog->scrolledwindow4);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (dialog->scrolledwindow4), GTK_SHADOW_IN);

    model = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
    dialog->treeview4 = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
    gtk_widget_show (dialog->treeview4);
    gtk_container_add (GTK_CONTAINER (dialog->scrolledwindow4), dialog->treeview4);

    /* command column */
    renderer = gtk_cell_renderer_text_new ();
    g_object_set_data (G_OBJECT (renderer), "column", (gint *) COLUMN_COMMAND);

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (dialog->treeview4), -1, _("Command"), renderer, "text", COLUMN_COMMAND, NULL);
    /* shortcut column */
    renderer = gtk_cell_renderer_text_new ();
    g_object_set_data (G_OBJECT (renderer), "column", (gint *) COLUMN_SHORTCUT);

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (dialog->treeview4), -1, _("Shortcut"), renderer, "text", COLUMN_SHORTCUT, NULL);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dialog->treeview4), TRUE);

    label = gtk_label_new (_("Keyboard"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    vbox2 = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox2);
    gtk_container_set_border_width (GTK_CONTAINER (vbox2), BORDER);
    gtk_widget_show (vbox2);
    gtk_container_add (GTK_CONTAINER (notebook), vbox2);

    frame = xfce_framebox_new (_("Focus model"), TRUE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox2), frame, TRUE, TRUE, 0);

    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), hbox);

    dialog->click_focus_radio = gtk_radio_button_new_with_mnemonic (NULL, _("Click to focus"));
    gtk_widget_show (dialog->click_focus_radio);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->click_focus_radio, TRUE, FALSE, 0);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (dialog->click_focus_radio), dialog->click_focus_radio_group);
    dialog->click_focus_radio_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (dialog->click_focus_radio));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->click_focus_radio), click_to_focus);

    dialog->focus_follow_mouse_radio = gtk_radio_button_new_with_mnemonic (NULL, _("Focus follows mouse"));
    gtk_widget_show (dialog->focus_follow_mouse_radio);
    gtk_box_pack_start (GTK_BOX (hbox), dialog->focus_follow_mouse_radio, TRUE, FALSE, 0);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (dialog->focus_follow_mouse_radio), dialog->click_focus_radio_group);
    dialog->click_focus_radio_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (dialog->focus_follow_mouse_radio));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->focus_follow_mouse_radio), !click_to_focus);

    frame = xfce_framebox_new (_("New window focus"), TRUE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox2), frame, TRUE, TRUE, 0);

    dialog->focus_new_check = gtk_check_button_new_with_mnemonic (_("Automatically give focus to \nnewly created windows"));
    gtk_widget_show (dialog->focus_new_check);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), dialog->focus_new_check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->focus_new_check), focus_new);

    frame = xfce_framebox_new (_("Raise on focus"), TRUE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox2), frame, TRUE, TRUE, 0);

    vbox4 = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox4);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox4);

    dialog->raise_on_focus_check = gtk_check_button_new_with_mnemonic (_("Automatically raise windows \nwhen they receive focus"));
    gtk_widget_show (dialog->raise_on_focus_check);
    gtk_box_pack_start (GTK_BOX (vbox4), dialog->raise_on_focus_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->raise_on_focus_check), focus_raise);

    table2 = gtk_table_new (2, 3, FALSE);
    gtk_widget_show (table2);
    gtk_box_pack_start (GTK_BOX (vbox4), table2, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (table2), BORDER);

    label = gtk_label_new (_("Delay before raising focused window :"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table2), label, 0, 3, 0, 1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = xfce_create_small_label (_("Slow"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table2), label, 0, 1, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

    label = xfce_create_small_label (_("Fast"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table2), label, 2, 3, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    dialog->raise_delay_scale = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (raise_delay, 100, 2000, 10, 100, 0)));
    gtk_widget_show (dialog->raise_delay_scale);
    gtk_table_attach (GTK_TABLE (table2), dialog->raise_delay_scale, 1, 2, 1,
        2, (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_scale_set_draw_value (GTK_SCALE (dialog->raise_delay_scale), FALSE);
    gtk_scale_set_digits (GTK_SCALE (dialog->raise_delay_scale), 0);
    gtk_range_set_update_policy (GTK_RANGE (dialog->raise_delay_scale), GTK_UPDATE_DISCONTINUOUS);
    gtk_range_set_inverted (GTK_RANGE (dialog->raise_delay_scale), TRUE);
    gtk_widget_set_sensitive (dialog->raise_delay_scale, focus_raise);

    frame = xfce_framebox_new (_("Raise on click"), TRUE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox2), frame, TRUE, TRUE, 0);

    dialog->click_raise_check = gtk_check_button_new_with_mnemonic (_("Raise window when clicking inside\napplication window"));
    gtk_widget_show (dialog->click_raise_check);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), dialog->click_raise_check);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->click_raise_check), raise_on_click);

    label = gtk_label_new (_("Focus"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 2), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    vbox3 = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox3), BORDER);
    gtk_widget_show (vbox3);
    gtk_container_add (GTK_CONTAINER (notebook), vbox3);

    frame = xfce_framebox_new (_("Windows snapping"), TRUE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox3), frame, TRUE, TRUE, 0);

    vbox5 = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox5);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox5);

    dialog->snap_to_border_check = gtk_check_button_new_with_mnemonic (_("Snap windows to screen border"));
    gtk_widget_show (dialog->snap_to_border_check);
    gtk_box_pack_start (GTK_BOX (vbox5), dialog->snap_to_border_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->snap_to_border_check), snap_to_border);

    dialog->snap_to_windows_check = gtk_check_button_new_with_mnemonic (_("Snap windows to other windows"));
    gtk_widget_show (dialog->snap_to_windows_check);
    gtk_box_pack_start (GTK_BOX (vbox5), dialog->snap_to_windows_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->snap_to_windows_check), snap_to_windows);

    table3 = gtk_table_new (2, 3, FALSE);
    gtk_widget_show (table3);
    gtk_box_pack_start (GTK_BOX (vbox5), table3, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (table3), BORDER);

    label = gtk_label_new (_("Distance :"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table3), label, 0, 3, 0, 1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = xfce_create_small_label (_("Small"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table3), label, 0, 1, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

    label = xfce_create_small_label (_("Wide"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table3), label, 2, 3, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    dialog->snap_width_scale = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (snap_width, 5, 50, 5, 10, 0)));
    gtk_widget_show (dialog->snap_width_scale);
    gtk_table_attach (GTK_TABLE (table3), dialog->snap_width_scale, 1, 2, 1,
        2, (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_scale_set_draw_value (GTK_SCALE (dialog->snap_width_scale), FALSE);
    gtk_scale_set_digits (GTK_SCALE (dialog->snap_width_scale), 0);
    gtk_range_set_update_policy (GTK_RANGE (dialog->snap_width_scale), GTK_UPDATE_DISCONTINUOUS);
    gtk_widget_set_sensitive (dialog->snap_width_scale, snap_to_border || snap_to_windows);

    frame = xfce_framebox_new (_("Wrap workspaces"), TRUE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox3), frame, TRUE, TRUE, 0);

    vbox6 = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox6);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox6);

    dialog->wrap_workspaces_check = gtk_check_button_new_with_mnemonic (_("Wrap workspaces when the pointer reaches a screen edge"));
    gtk_widget_show (dialog->wrap_workspaces_check);
    gtk_box_pack_start (GTK_BOX (vbox6), dialog->wrap_workspaces_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->wrap_workspaces_check), wrap_workspaces);

    dialog->wrap_windows_check = gtk_check_button_new_with_mnemonic (_("Wrap workspaces when dragging a window off the screen"));
    gtk_widget_show (dialog->wrap_windows_check);
    gtk_box_pack_start (GTK_BOX (vbox6), dialog->wrap_windows_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->wrap_windows_check), wrap_windows);

    table4 = gtk_table_new (2, 3, FALSE);
    gtk_widget_show (table4);
    gtk_box_pack_start (GTK_BOX (vbox6), table4, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (table4), BORDER);

    label = gtk_label_new (_("Edge Resistance :"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table4), label, 0, 3, 0, 1, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    label = xfce_create_small_label (_("Small"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table4), label, 0, 1, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

    label = xfce_create_small_label (_("Wide"));
    gtk_widget_show (label);
    gtk_table_attach (GTK_TABLE (table4), label, 2, 3, 1, 2, (GtkAttachOptions) (GTK_FILL), (GtkAttachOptions) (0), 0, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

    dialog->wrap_resistance_scale = gtk_hscale_new (GTK_ADJUSTMENT (gtk_adjustment_new (wrap_resistance, 1, 50, 5, 10, 0)));
    gtk_widget_show (dialog->wrap_resistance_scale);
    gtk_table_attach (GTK_TABLE (table4), dialog->wrap_resistance_scale, 1, 2,
        1, 2, (GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL), (GtkAttachOptions) (GTK_FILL), 0, 0);
    gtk_scale_set_draw_value (GTK_SCALE (dialog->wrap_resistance_scale), FALSE);
    gtk_scale_set_digits (GTK_SCALE (dialog->wrap_resistance_scale), 0);
    gtk_range_set_update_policy (GTK_RANGE (dialog->wrap_resistance_scale), GTK_UPDATE_DISCONTINUOUS);
    gtk_widget_set_sensitive (dialog->wrap_resistance_scale, wrap_workspaces || wrap_windows);

    frame = xfce_framebox_new (_("Opaque move and resize"), TRUE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox3), frame, TRUE, TRUE, 0);

    vbox7 = gtk_vbox_new (FALSE, BORDER);
    gtk_widget_show (vbox7);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox7);

    dialog->box_resize_check = gtk_check_button_new_with_mnemonic (_("Display content of windows when resizing"));
    gtk_widget_show (dialog->box_resize_check);
    gtk_box_pack_start (GTK_BOX (vbox7), dialog->box_resize_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->box_resize_check), !box_resize);

    dialog->box_move_check = gtk_check_button_new_with_mnemonic (_("Display content of windows when moving"));
    gtk_widget_show (dialog->box_move_check);
    gtk_box_pack_start (GTK_BOX (vbox7), dialog->box_move_check, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->box_move_check), !box_move);

    frame = xfce_framebox_new (_("Double click action"), TRUE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (vbox3), frame, TRUE, TRUE, 0);

    xfce_framebox_add (XFCE_FRAMEBOX (frame),
        create_option_menu_box (dbl_click_values, 4,
            _("Action to perform when double clicking on title bar :"), dbl_click_action, G_CALLBACK (cb_dblclick_action_value_changed), mcs_plugin));

    label = gtk_label_new (_("Advanced"));
    gtk_widget_show (label);
    gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 3), label);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

    dialog->dialog_action_area1 = GTK_DIALOG (dialog->xfwm4_dialog)->action_area;
    gtk_widget_show (dialog->dialog_action_area1);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog->dialog_action_area1), GTK_BUTTONBOX_END);

    dialog->closebutton1 = gtk_button_new_from_stock ("gtk-close");
    gtk_widget_show (dialog->closebutton1);
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog->xfwm4_dialog), dialog->closebutton1, GTK_RESPONSE_CLOSE);
    GTK_WIDGET_SET_FLAGS (dialog->closebutton1, GTK_CAN_DEFAULT);

    dialog->helpbutton1 = gtk_button_new_from_stock ("gtk-help");
    gtk_widget_show (dialog->helpbutton1);
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog->xfwm4_dialog), dialog->helpbutton1, GTK_RESPONSE_HELP);

    gtk_widget_grab_focus (dialog->closebutton1);
    gtk_widget_grab_default (dialog->closebutton1);

    return dialog;
}

static void
setup_dialog (Itf * itf)
{
    GtkTreeModel *model1, *model2;
    GtkTreeSelection *selection;
    ThemeInfo *ti;

    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (itf->treeview1), -1, NULL, gtk_cell_renderer_text_new (), "text", THEME_NAME_COLUMN, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (itf->treeview2), -1, NULL, gtk_cell_renderer_text_new (), "text", THEME_NAME_COLUMN, NULL);

    model1 = (GtkTreeModel *) gtk_list_store_new (N_COLUMNS, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model1), 0, sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model1), 0, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (itf->treeview1), model1);

    model2 = (GtkTreeModel *) gtk_list_store_new (N_COLUMNS, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model2), 0, sort_func, NULL, NULL);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model2), 0, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (itf->treeview2), model2);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (itf->treeview1));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
    g_signal_connect (G_OBJECT (selection), "changed", (GCallback) decoration_selection_changed, itf);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (itf->treeview2));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
    g_signal_connect (G_OBJECT (selection), "changed", (GCallback) keybinding_selection_changed, itf);


    g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (itf->treeview2))), "changed", G_CALLBACK (cb_shortcuttheme_changed), itf);
    g_signal_connect (G_OBJECT (itf->treeview3), "row-activated", G_CALLBACK (cb_activate_treeview3), itf);
    g_signal_connect (G_OBJECT (itf->treeview4), "row-activated", G_CALLBACK (cb_activate_treeview4), itf);


    decoration_theme_list = read_themes (decoration_theme_list, itf->treeview1, itf->scrolledwindow1, DECORATION_THEMES, current_theme);
    keybinding_theme_list = read_themes (keybinding_theme_list, itf->treeview2, itf->scrolledwindow2, KEYBINDING_THEMES, current_key_theme);
    dialog_update_from_theme (itf, current_theme, decoration_theme_list);

    /* load the theme */
    ti = find_theme_info_by_name (current_key_theme, keybinding_theme_list);

    if (ti)
    {
        gchar *theme_file = g_build_filename (ti->path, G_DIR_SEPARATOR_S, KEY_SUFFIX, G_DIR_SEPARATOR_S, KEYTHEMERC, NULL);
        loadtheme_in_treeview (theme_file, itf);

        g_free (theme_file);
    }
    else
        g_warning ("Cannot find the keytheme !");

    g_signal_connect (G_OBJECT (itf->xfwm4_dialog), "response", G_CALLBACK (cb_dialog_response), itf->mcs_plugin);
    g_signal_connect (G_OBJECT (itf->font_button), "clicked", G_CALLBACK (show_font_selection), itf);
    g_signal_connect (G_OBJECT (itf->click_focus_radio), "toggled", G_CALLBACK (cb_click_to_focus_changed), itf);
    g_signal_connect (G_OBJECT (itf->focus_new_check), "toggled", G_CALLBACK (cb_focus_new_changed), itf);
    g_signal_connect (G_OBJECT (itf->raise_on_focus_check), "toggled", G_CALLBACK (cb_raise_on_focus_changed), itf);
    g_signal_connect (G_OBJECT (itf->raise_delay_scale), "value_changed", (GCallback) cb_raise_delay_changed, itf);
    g_signal_connect (G_OBJECT (itf->click_raise_check), "toggled", G_CALLBACK (cb_raise_on_click_changed), itf);
    g_signal_connect (G_OBJECT (itf->snap_to_border_check), "toggled", G_CALLBACK (cb_snap_to_border_changed), itf);
    g_signal_connect (G_OBJECT (itf->snap_to_windows_check), "toggled", G_CALLBACK (cb_snap_to_windows_changed), itf);
    g_signal_connect (G_OBJECT (itf->snap_width_scale), "value_changed", (GCallback) cb_snap_width_changed, itf);
    g_signal_connect (G_OBJECT (itf->wrap_workspaces_check), "toggled", G_CALLBACK (cb_wrap_workspaces_changed), itf);
    g_signal_connect (G_OBJECT (itf->wrap_windows_check), "toggled", G_CALLBACK (cb_wrap_windows_changed), itf);
    g_signal_connect (G_OBJECT (itf->wrap_resistance_scale), "value_changed", (GCallback) cb_wrap_resistance_changed, itf);
    g_signal_connect (G_OBJECT (itf->box_move_check), "toggled", (GCallback) cb_box_move_changed, itf);
    g_signal_connect (G_OBJECT (itf->box_resize_check), "toggled", G_CALLBACK (cb_box_resize_changed), itf);

    gtk_window_set_position (GTK_WINDOW (itf->xfwm4_dialog), GTK_WIN_POS_CENTER);
    gtk_widget_show (itf->xfwm4_dialog);
}

McsPluginInitResult
mcs_plugin_init (McsPlugin * mcs_plugin)
{
#if 0
#ifdef ENABLE_NLS
    /* 
       This is required for UTF-8 at least - Please don't remove it
       And it needs to be done here for the label to be properly
       localized....
     */
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
    textdomain (GETTEXT_PACKAGE);
#endif
#else
    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
#endif

    create_channel (mcs_plugin);
    mcs_plugin->plugin_name = g_strdup (PLUGIN_NAME);
    mcs_plugin->caption = g_strdup (_("Window Manager"));
    mcs_plugin->run_dialog = run_dialog;
    mcs_plugin->icon = xfce_inline_icon_at_size (xfwm4_icon_data, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);
    mcs_manager_notify (mcs_plugin->manager, CHANNEL);

    return (MCS_PLUGIN_INIT_OK);
}

static void
create_channel (McsPlugin * mcs_plugin)
{
    McsSetting *setting;

    const gchar *home = g_get_home_dir ();
    gchar *rcfile;

    rcfile = g_strconcat (home, G_DIR_SEPARATOR_S, ".xfce4", G_DIR_SEPARATOR_S, RCDIR, G_DIR_SEPARATOR_S, RCFILE, NULL);
    mcs_manager_add_channel_from_file (mcs_plugin->manager, CHANNEL, rcfile);
    g_free (rcfile);

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL);
    if (setting)
    {
        if (current_key_theme)
        {
            g_free (current_key_theme);
        }
        current_key_theme = g_strdup (setting->data.v_string);
    }
    else
    {
        if (current_key_theme)
        {
            g_free (current_key_theme);
        }

        current_key_theme = g_strdup (DEFAULT_KEY_THEME);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL, current_key_theme);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/ThemeName", CHANNEL);
    if (setting)
    {
        if (current_theme)
        {
            g_free (current_theme);
        }
        current_theme = g_strdup (setting->data.v_string);
    }
    else
    {
        if (current_theme)
        {
            g_free (current_theme);
        }

        current_theme = g_strdup (DEFAULT_THEME);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/ThemeName", CHANNEL, current_theme);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/TitleFont", CHANNEL);
    if (setting)
    {
        if (current_font)
        {
            g_free (current_font);
        }
        current_font = g_strdup (setting->data.v_string);
    }
    else
    {
        if (current_font)
        {
            g_free (current_font);
        }

        current_font = g_strdup (DEFAULT_FONT);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/TitleFont", CHANNEL, current_font);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/TitleAlign", CHANNEL);
    if (setting)
    {
        if (title_align)
        {
            g_free (title_align);
        }
        title_align = g_strdup (setting->data.v_string);
    }
    else
    {
        if (title_align)
        {
            g_free (title_align);
        }

        title_align = g_strdup (DEFAULT_ALIGN);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/TitleAlign", CHANNEL, title_align);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/ButtonLayout", CHANNEL);
    if (setting)
    {
        if (current_layout)
        {
            g_free (current_layout);
        }
        current_layout = g_strdup (setting->data.v_string);
    }
    else
    {
        if (current_layout)
        {
            g_free (current_layout);
        }

        current_layout = g_strdup (DEFAULT_LAYOUT);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/ButtonLayout", CHANNEL, current_layout);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/ClickToFocus", CHANNEL);
    if (setting)
    {
        click_to_focus = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        click_to_focus = TRUE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/ClickToFocus", CHANNEL, click_to_focus ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/FocusNewWindow", CHANNEL);
    if (setting)
    {
        focus_new = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        focus_new = TRUE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/FocusNewWindow", CHANNEL, focus_new ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/FocusRaise", CHANNEL);
    if (setting)
    {
        focus_raise = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        focus_raise = FALSE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/FocusRaise", CHANNEL, focus_raise ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/RaiseDelay", CHANNEL);
    if (setting)
    {
        raise_delay = setting->data.v_int;
    }
    else
    {
        raise_delay = 250;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/RaiseDelay", CHANNEL, raise_delay);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/RaiseOnClick", CHANNEL);
    if (setting)
    {
        raise_on_click = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        raise_on_click = TRUE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/RaiseOnClick", CHANNEL, raise_on_click ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/SnapToBorder", CHANNEL);
    if (setting)
    {
        snap_to_border = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        snap_to_border = TRUE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapToBorder", CHANNEL, snap_to_border ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/SnapToWindows", CHANNEL);
    if (setting)
    {
        snap_to_windows = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        snap_to_windows = FALSE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapToWindows", CHANNEL, snap_to_windows ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/SnapWidth", CHANNEL);
    if (setting)
    {
        snap_width = setting->data.v_int;
    }
    else
    {
        snap_width = 10;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/SnapWidth", CHANNEL, snap_width);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/WrapResistance", CHANNEL);
    if (setting)
    {
        wrap_resistance = setting->data.v_int;
    }
    else
    {
        wrap_resistance = 10;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapResistance", CHANNEL, wrap_resistance);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/WrapWorkspaces", CHANNEL);
    if (setting)
    {
        wrap_workspaces = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        wrap_workspaces = FALSE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapWorkspaces", CHANNEL, wrap_workspaces ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/WrapWindows", CHANNEL);
    if (setting)
    {
        wrap_windows = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        wrap_windows = TRUE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/WrapWindows", CHANNEL, wrap_windows ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/BoxMove", CHANNEL);
    if (setting)
    {
        box_move = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        box_move = FALSE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/BoxMove", CHANNEL, box_move ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/BoxResize", CHANNEL);
    if (setting)
    {
        box_resize = (setting->data.v_int ? TRUE : FALSE);
    }
    else
    {
        box_resize = FALSE;
        mcs_manager_set_int (mcs_plugin->manager, "Xfwm/BoxResize", CHANNEL, box_resize ? 1 : 0);
    }

    setting = mcs_manager_setting_lookup (mcs_plugin->manager, "Xfwm/DblClickAction", CHANNEL);
    if (setting)
    {
        if (dbl_click_action)
        {
            g_free (dbl_click_action);
        }
        dbl_click_action = g_strdup (setting->data.v_string);
    }
    else
    {
        if (dbl_click_action)
        {
            g_free (dbl_click_action);
        }

        dbl_click_action = g_strdup (DEFAULT_ACTION);
        mcs_manager_set_string (mcs_plugin->manager, "Xfwm/DblClickAction", CHANNEL, dbl_click_action);
    }
}

static gboolean
write_options (McsPlugin * mcs_plugin)
{
    gchar *rcfile;
    gboolean result;

    rcfile = xfce_get_userfile (RCDIR, RCFILE, NULL);
    result = mcs_manager_save_channel_to_file (mcs_plugin->manager, CHANNEL, rcfile);
    g_free (rcfile);

    return result;
}

static void
run_dialog (McsPlugin * mcs_plugin)
{
    Itf *dialog;

    if (is_running)
        return;

    is_running = TRUE;

    dialog = create_dialog (mcs_plugin);
    setup_dialog (dialog);
}

/* macro defined in manager-plugin.h */
MCS_PLUGIN_CHECK_INIT
