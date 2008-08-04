/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Based on the mcs-plugin written by olivier fourdan
 */


#include <config.h>
#include <string.h>

#include <glib.h>

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libxfcegui4/libxfcegui4.h>
#include <xfconf/xfconf.h>
#include "xfwm4-dialog_glade.h"

#ifndef KEYTHEMERC
#define KEYTHEMERC "keythemerc"
#endif

typedef enum {
    THEME_TYPE_XFWM4,
    THEME_TYPE_KEYTHEME,
} ThemeType;

typedef struct {
    const gchar *name;
    const gchar *key;
    gchar *value;
} KeyThemeTmpl;

typedef struct {
   gchar *name;
   gchar *value;
} MenuTmpl;

static const MenuTmpl dbl_click_values[] = {
    {N_("Shade window"), "shade"},
    {N_("Hide window"), "hide"},
    {N_("Maximize window"), "maximize"},
    {N_("Fill window"), "fill"},
    {N_("Nothing"), "none"},
    {NULL, NULL}
};

static const MenuTmpl title_align_values[] = {
    {N_("Left"), "left"},
    {N_("Center"), "center"},
    {N_("Right"), "right"},
    {NULL, NULL}
};

static KeyThemeTmpl key_theme_values[] = {
    {N_("Window operations menu"), "popup_menu_key", NULL},
    {N_("Up"), "up_key", NULL},
    {N_("Down"), "down_key", NULL},
    {N_("Left"), "left_key", NULL},
    {N_("Right"), "right_key", NULL},
    {N_("Cancel"), "cancel_key", NULL},
    {N_("Cycle windows"), "cycle_windows_key", NULL},
    {N_("Close window"), "close_window_key", NULL},
    {N_("Maximize window horizontally"), "maximize_horiz_key", NULL},
    {N_("Maximize window vertically"), "maximize_vert_key", NULL},
    {N_("Maximize window"), "maximize_window_key", NULL},
    {N_("Hide window"), "hide_window_key", NULL},
    {N_("Move window"), "move_window_key", NULL},
    {N_("Resize window"), "resize_window_key", NULL},
    {N_("Shade window"), "shade_window_key", NULL},
    {N_("Stick window"), "stick_window_key", NULL},
    {N_("Raise window"), "raise_window_key", NULL},
    {N_("Lower window"), "lower_window_key", NULL},
    {N_("Fill window"), "fill_window_key", NULL},
    {N_("Fill window horizontally"), "fill_horiz_key", NULL},
    {N_("Fill window vertically"), "fill_vert_key", NULL},
    {N_("Toggle above"), "above_key", NULL},
    {N_("Toggle fullscreen"), "fullscreen_key", NULL},
    {N_("Move window to upper workspace"), "move_window_up_workspace_key", NULL},
    {N_("Move window to bottom workspace"), "move_window_down_workspace_key", NULL},
    {N_("Move window to left workspace"), "move_window_left_workspace_key", NULL},
    {N_("Move window to right workspace"), "move_window_right_workspace_key", NULL},
    {N_("Move window to previous workspace"), "move_window_prev_workspace_key", NULL},
    {N_("Move window to next workspace"), "move_window_next_workspace_key", NULL},
    {N_("Show desktop"), "show_desktop_key", NULL},
    {N_("Upper workspace"), "up_workspace_key", NULL},
    {N_("Bottom workspace"), "down_workspace_key", NULL},
    {N_("Left workspace"), "left_workspace_key", NULL},
    {N_("Right workspace"), "right_workspace_key", NULL},
    {N_("Previous workspace"), "prev_workspace_key", NULL},
    {N_("Next workspace"), "next_workspace_key", NULL},
    {N_("Add workspace"), "add_workspace_key", NULL},
    {N_("Add adjacent workspace"), "add_adjacent_workspace_key", NULL},
    {N_("Delete last workspace"), "del_workspace_key", NULL},
    {N_("Delete active workspace"), "del_active_workspace_key", NULL},
    {NULL, NULL, NULL}
};

#define INDICATOR_SIZE 9



static gboolean version = FALSE;
static GtkWidget *add_keytheme_dialog = NULL;
static GtkWidget *add_keytheme_name_entry = NULL;
static gchar *xfwm4_keythemerc_filename = NULL;
static gchar *xfwm4_themerc_filename = NULL;

static void
cb_xfwm4_theme_treeselection_changed (GtkTreeSelection *selection, XfconfChannel *channel);
static void
cb_xfwm4_keytheme_treeselection_changed (GtkTreeSelection *selection, XfconfChannel *channel);

static void
cb_xfwm4_dbl_click_changed (GtkComboBox *combo, XfconfChannel *channel);

static void
cb_xfwm4_title_align_changed (GtkComboBox *combo, XfconfChannel *channel);

static void
cb_xfwm4_title_button_alignment_changed (GtkComboBox *combo, GtkButton *button);

static void
cb_xfwm4_keytheme_contents_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data);

static void
cb_xfwm4_channel_keytheme_changed (XfconfChannel *channel, const gchar *property, const GValue *value, GtkTreeView *treeview);

static void
cb_xfwm4_channel_double_click_action_changed(XfconfChannel *channel,
                                             const gchar *property,
                                             const GValue *value,
                                             GtkComboBox *combo);

static void
cb_xfwm4_channel_title_alignment_changed(XfconfChannel *channel,
                                         const gchar *property,
                                         const GValue *value,
                                         GtkComboBox *combo);

static void
cb_xfwm4_channel_button_layout_changed(XfconfChannel *channel,
                                       const gchar *property,
                                       const GValue *value,
                                       GtkContainer *hidden_box);

static gboolean
str_starts_with(const gchar *str, const gchar *start);

static void
save_button_layout (GtkContainer *container, XfconfChannel *channel);

static void
cb_xfwm4_active_frame_drag_data (GtkWidget*, GdkDragContext*, gint, gint, GtkSelectionData*, guint, guint, GtkWidget*);
static gboolean
cb_xfwm4_active_frame_drag_motion (GtkWidget*, GdkDragContext*, gint, gint, guint, GtkWidget*);
static void
cb_xfwm4_active_frame_drag_leave (GtkWidget*, GdkDragContext*, guint, GtkWidget*);
static void
cb_xfwm4_hidden_frame_drag_data (GtkWidget*, GdkDragContext*, gint, gint, GtkSelectionData*, guint, guint, GtkWidget*);
static void
cb_xfwm4_title_button_data_get (GtkWidget*, GdkDragContext*, GtkSelectionData*, guint, guint, const gchar*);
static void
cb_xfwm4_title_button_drag_begin (GtkWidget*, GdkDragContext*, gpointer);
static void
cb_xfwm4_title_button_drag_end (GtkWidget*, GdkDragContext*, gpointer);
static gboolean
cb_xfwm4_signal_blocker (GtkWidget*, gpointer);
static void
cb_xfwm4_add_keytheme_button_clicked (GtkWidget *, gpointer);
static void
cb_xfwm4_del_keytheme_button_clicked (GtkWidget *, gpointer);

static void
load_key_theme (const gchar* user_theme_name);

static GdkPixbuf*
xfwm4_create_icon_from_widget(GtkWidget *widget);

static void
xfwm4_create_indicator(GtkWidget *box, gint x, gint y, gint width, gint height);
static void
xfwm4_delete_indicator(GtkWidget *box);

void
check_keytheme_contents (GtkListStore *list_store, XfconfChannel *channel, GtkTreeView *treeview)
{
    GtkTreeIter iter;
    const KeyThemeTmpl *shortcuts_iter;
    gchar *active_theme_name = xfconf_channel_get_string (channel, "/general/keytheme", "Default");
    load_key_theme (active_theme_name);
    shortcuts_iter = key_theme_values;
    gtk_list_store_clear(list_store);
    while (shortcuts_iter->name)
    {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, shortcuts_iter->name, 1, shortcuts_iter->value, 2, shortcuts_iter->key, -1);
        shortcuts_iter++;
    }
    if (!strcmp (active_theme_name, "Default"))
        gtk_widget_set_sensitive (GTK_WIDGET(treeview), FALSE);
    else
        gtk_widget_set_sensitive (GTK_WIDGET(treeview), TRUE);
}

void
check_xfwm4_themes (GtkListStore *list_store, GtkTreeView *tree_view, XfconfChannel *channel, ThemeType type)
{
    GSList *check_list = NULL;
    gint i;
    const gchar *file;
    gchar *xfwm4rc_filename = NULL;
    gchar **xfwm4_theme_dirs;
    GDir *dir;
    gchar *active_theme_name = NULL;
    GtkTreeIter iter;
    GtkTreePath *tree_path = NULL;

    switch (type)
    {
        case THEME_TYPE_XFWM4:
            active_theme_name = xfconf_channel_get_string (channel, "/general/theme", "Default");
            break;
        case THEME_TYPE_KEYTHEME:
            active_theme_name = xfconf_channel_get_string (channel, "/general/keytheme", "Default");
            break;
    }

    xfce_resource_push_path (XFCE_RESOURCE_THEMES, DATADIR G_DIR_SEPARATOR_S "themes");
    xfwm4_theme_dirs = xfce_resource_dirs (XFCE_RESOURCE_THEMES);
    xfce_resource_pop_path (XFCE_RESOURCE_THEMES);

    /* Iterate over all base directories */
    for (i = 0; xfwm4_theme_dirs[i] != NULL; ++i)
    {
        /* Open directory handle */
        dir = g_dir_open (xfwm4_theme_dirs[i], 0, NULL);

        /* Try next base directory if this one cannot be read */
        if (G_UNLIKELY (dir == NULL))
            continue;

        /* Iterate over filenames in the directory */
        while ((file = g_dir_read_name (dir)) != NULL)
        {
            /* Build the theme style filename */
            switch (type)
            {
                case THEME_TYPE_XFWM4:
                    xfwm4rc_filename = g_build_filename (xfwm4_theme_dirs[i], file, "xfwm4", "themerc", NULL);
                    break;
                case THEME_TYPE_KEYTHEME:
                    xfwm4rc_filename = g_build_filename (xfwm4_theme_dirs[i], file, "xfwm4", "keythemerc", NULL);
                    break;
            }

            /* Check if the gtkrc file exists and the theme is not already in the list */
            if (g_file_test (xfwm4rc_filename, G_FILE_TEST_EXISTS)
                && g_slist_find_custom (check_list, file, (GCompareFunc) g_utf8_collate) == NULL)
            {

                /* Insert the theme in the check list */
                check_list = g_slist_prepend (check_list, g_strdup (file));

                /* Append ui theme to the list store */
                gtk_list_store_append (list_store, &iter);
                gtk_list_store_set (list_store, &iter,
                                    0, file,
                                    -1);

                /* Check if this is the active theme, if so, select it */
                if (G_UNLIKELY (g_utf8_collate (file, active_theme_name) == 0))
                {
                    switch (type)
                    {
                        case THEME_TYPE_KEYTHEME:
                            if (xfwm4_keythemerc_filename)
                                g_free (xfwm4_keythemerc_filename);
                            xfwm4_keythemerc_filename = g_strdup (xfwm4rc_filename);
                            break;
                        case THEME_TYPE_XFWM4:
                            if (xfwm4_themerc_filename)
                                g_free (xfwm4_themerc_filename);
                            xfwm4_themerc_filename = g_strdup (xfwm4rc_filename);
                            break;
                    }

                    tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
                    gtk_tree_selection_select_path (gtk_tree_view_get_selection (tree_view), tree_path);
                    gtk_tree_path_free (tree_path);
                }
            }
            /* Free xfwm4rc filename */
            g_free (xfwm4rc_filename);
        }
        /* Close directory handle */
        g_dir_close (dir);
    }
}

static void
load_key_theme (const gchar* user_theme_name)
{
    gchar *user_theme_file = NULL;
    gchar *default_theme_file = g_build_filename (DATADIR, "themes", "Default", "xfwm4", "keythemerc", NULL);
    gchar **shortcuts_keys, **shortcut = NULL;
    XfceRc *default_rc = NULL;
    XfceRc *user_rc = NULL;
    gchar **theme_dirs, **xfwm4_theme_dirs;
    gint i = 0;

    xfce_resource_push_path (XFCE_RESOURCE_THEMES, DATADIR G_DIR_SEPARATOR_S "themes");
    xfwm4_theme_dirs = xfce_resource_dirs (XFCE_RESOURCE_THEMES);
    xfce_resource_pop_path (XFCE_RESOURCE_THEMES);

    theme_dirs = xfwm4_theme_dirs;

    while (*theme_dirs)
    {
        user_theme_file = g_build_filename (*theme_dirs, user_theme_name, "xfwm4", "keythemerc", NULL);       
        if (!g_file_test (user_theme_file, G_FILE_TEST_EXISTS))
        {
            g_free (user_theme_file);
            user_theme_file = NULL;
        }
        else
            break;
        theme_dirs++;
    }

    

    default_rc = xfce_rc_simple_open (default_theme_file, TRUE);
    if (user_theme_file)
        user_rc = xfce_rc_simple_open (user_theme_file, TRUE);

    shortcuts_keys = xfce_rc_get_entries (default_rc, NULL);

    shortcut = shortcuts_keys;

    while (*shortcut)
    {
        gboolean found = FALSE;
        const gchar *entry_value;
        const gchar *fallback_value;
        fallback_value = xfce_rc_read_entry (default_rc, *shortcut, "none");
        entry_value = xfce_rc_read_entry (user_rc, *shortcut, fallback_value);

        for (i = 0; !found && key_theme_values[i].name; i++)
        {
            if (g_ascii_strcasecmp (*shortcut, key_theme_values[i].key) == 0)
            {
                key_theme_values[i].value = g_strdup(entry_value);
            }
        }
        shortcut++;
    }

    g_strfreev (shortcuts_keys);

    xfce_rc_close (default_rc);
    xfce_rc_close (user_rc);
}

GtkWidget *
xfwm4_dialog_new_from_xml (GladeXML *gxml)
{
    GtkWidget *dialog;
    GtkWidget *vbox;
    GtkTreeIter iter;
    GtkListStore *list_store;
    GtkCellRenderer *renderer;
    const MenuTmpl *tmpl_iter;
    GtkTreeSelection *theme_selection, *keytheme_selection, *keytheme_contents_selection;
    GtkTargetEntry target_entry[2];
    GList *list_iter, *children;
    GValue value = { 0, };
    XfconfChannel *xfwm4_channel = xfconf_channel_new("xfwm4");

    /* Style tab */
    GtkWidget *theme_name_treeview = glade_xml_get_widget (gxml, "theme_name_treeview");
    GtkWidget *title_font_button = glade_xml_get_widget (gxml, "title_font_button");
    GtkWidget *title_align_combo = glade_xml_get_widget (gxml, "title_align_combo");
    GtkWidget *active_frame = glade_xml_get_widget (gxml, "active-frame");
    GtkWidget *active_box = glade_xml_get_widget (gxml, "active-box");
    GtkWidget *hidden_frame = glade_xml_get_widget (gxml, "hidden-frame");
    GtkWidget *hidden_box = glade_xml_get_widget (gxml, "hidden-box");

    /* Keyboard tab */
    GtkWidget *add_keytheme_button = glade_xml_get_widget (gxml, "add_keytheme_button");
    GtkWidget *del_keytheme_button = glade_xml_get_widget (gxml, "del_keytheme_button");
    GtkWidget *keytheme_name_treeview = glade_xml_get_widget (gxml, "keytheme_name_treeview");
    GtkWidget *keytheme_contents_treeview = glade_xml_get_widget (gxml, "keytheme_contents_treeview");

    /* Focus tab */
    GtkWidget *focus_delay_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "focus_delay_scale")));
    GtkWidget *focus_new_check = glade_xml_get_widget (gxml, "focus_new_check");
    GtkWidget *raise_on_focus_check = glade_xml_get_widget (gxml, "raise_on_focus_check");
    GtkWidget *raise_on_click_check = glade_xml_get_widget (gxml, "raise_on_click_check");
    GtkWidget *click_to_focus_mode = glade_xml_get_widget (gxml, "click_to_focus_mode");

    /* Advanced tab */
    GtkWidget *box_move_check = glade_xml_get_widget (gxml, "box_move_check");
    GtkWidget *box_resize_check = glade_xml_get_widget (gxml, "box_resize_check");
    GtkWidget *snap_to_border_check = glade_xml_get_widget (gxml, "snap_to_border_check");
    GtkWidget *snap_to_window_check = glade_xml_get_widget (gxml, "snap_to_window_check");
    GtkWidget *double_click_action_combo = glade_xml_get_widget (gxml, "double_click_action_combo");
    GtkWidget *snap_width_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "snap_width_scale")));
    GtkWidget *wrap_resistance_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "wrap_resistance_scale")));

    /* Double click action */
    list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    tmpl_iter = dbl_click_values;
    while (tmpl_iter->name)
    {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, tmpl_iter->name, 1, tmpl_iter->value, -1);
        tmpl_iter++;
    }

    gtk_cell_layout_clear (GTK_CELL_LAYOUT (double_click_action_combo));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (double_click_action_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (double_click_action_combo), renderer, "text", 0);

    gtk_combo_box_set_model (GTK_COMBO_BOX (double_click_action_combo), GTK_TREE_MODEL(list_store));

    xfconf_channel_get_property (xfwm4_channel, "/general/double_click_action", &value);
    cb_xfwm4_channel_double_click_action_changed (xfwm4_channel, "/general/double_click_action", &value, GTK_COMBO_BOX (double_click_action_combo));
    g_value_unset (&value);

    g_signal_connect (G_OBJECT (double_click_action_combo), "changed", G_CALLBACK (cb_xfwm4_dbl_click_changed), xfwm4_channel);
    g_signal_connect (G_OBJECT (add_keytheme_button), "clicked", G_CALLBACK (cb_xfwm4_add_keytheme_button_clicked), NULL);
    g_signal_connect (G_OBJECT (del_keytheme_button), "clicked", G_CALLBACK (cb_xfwm4_del_keytheme_button_clicked), NULL);

    /* Title alignment */
    list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    tmpl_iter = title_align_values;
    while (tmpl_iter->name)
    {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, tmpl_iter->name, 1, tmpl_iter->value, -1);
        tmpl_iter++;
    }

    gtk_cell_layout_clear (GTK_CELL_LAYOUT (title_align_combo));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (title_align_combo), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (title_align_combo), renderer, "text", 0);

    gtk_combo_box_set_model (GTK_COMBO_BOX (title_align_combo), GTK_TREE_MODEL(list_store));

    xfconf_channel_get_property (xfwm4_channel, "/general/title_alignment", &value);
    cb_xfwm4_channel_title_alignment_changed (xfwm4_channel, "/general/title_alignment", &value, GTK_COMBO_BOX (title_align_combo));
    g_value_unset (&value);

    g_signal_connect (G_OBJECT (title_align_combo), "changed", G_CALLBACK (cb_xfwm4_title_align_changed), xfwm4_channel);

    /* Button layout */
    target_entry[0].target = "_xfwm4_button_layout";
    target_entry[0].flags = GTK_TARGET_SAME_APP;
    target_entry[0].info = 2;
    target_entry[1].target = "_xfwm4_active_layout";
    target_entry[1].flags = GTK_TARGET_SAME_APP;
    target_entry[1].info = 3;

    gtk_drag_dest_set (active_frame, GTK_DEST_DEFAULT_ALL, target_entry, 2, GDK_ACTION_MOVE);
    g_signal_connect (active_frame, "drag-data-received", G_CALLBACK (cb_xfwm4_active_frame_drag_data), active_box);
    g_signal_connect (active_frame, "drag-motion", G_CALLBACK (cb_xfwm4_active_frame_drag_motion), active_box);
    g_signal_connect (active_frame, "drag-leave", G_CALLBACK (cb_xfwm4_active_frame_drag_leave), active_box);

    g_object_set_data (G_OBJECT (active_box), "xfwm4_channel", xfwm4_channel);

    gtk_drag_dest_set (hidden_frame, GTK_DEST_DEFAULT_ALL, target_entry, 1, GDK_ACTION_MOVE);
    g_signal_connect (hidden_frame, "drag-data-received", G_CALLBACK (cb_xfwm4_hidden_frame_drag_data), hidden_box);

    g_object_set_data (G_OBJECT (hidden_box), "active_box", active_box);

    children = list_iter = gtk_container_get_children(GTK_CONTAINER(active_box));

    while(list_iter)
    {
        GtkWidget *button;
        const gchar *name;

        button = GTK_WIDGET(list_iter->data);
        name = gtk_widget_get_name(button);

        if (name[strlen(name)-1] == '|')
        {
            g_signal_connect (G_OBJECT (title_align_combo), "changed", G_CALLBACK (cb_xfwm4_title_button_alignment_changed), button);
            cb_xfwm4_title_button_alignment_changed (GTK_COMBO_BOX (title_align_combo), GTK_BUTTON (button));
        }

        g_object_set_data (G_OBJECT (button), "key_char", (gpointer)&name[strlen(name)-1]);
        gtk_drag_source_set(button, GDK_BUTTON1_MASK, &target_entry[1], 1, GDK_ACTION_MOVE);
        g_signal_connect (G_OBJECT (button), "drag_data_get", G_CALLBACK (cb_xfwm4_title_button_data_get), target_entry[1].target);
        g_signal_connect (G_OBJECT (button), "drag_begin", G_CALLBACK (cb_xfwm4_title_button_drag_begin), NULL);
        g_signal_connect (G_OBJECT (button), "drag_end", G_CALLBACK (cb_xfwm4_title_button_drag_end), NULL);
        g_signal_connect (G_OBJECT (button), "button_press_event", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);
        g_signal_connect (G_OBJECT (button), "enter_notify_event", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);
        g_signal_connect (G_OBJECT (button), "focus", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);

        list_iter = g_list_next(list_iter);
    }

    g_list_free(children);

    children = list_iter = gtk_container_get_children(GTK_CONTAINER(hidden_box));

    while(list_iter)
    {
        GtkWidget *button;
        const gchar *name;

        button = GTK_WIDGET(list_iter->data);
        name = gtk_widget_get_name(button);

        g_object_set_data (G_OBJECT (button), "key_char", (gpointer)&name[strlen(name)-1]);
        gtk_drag_source_set(button, GDK_BUTTON1_MASK, &target_entry[0], 1, GDK_ACTION_MOVE);
        g_signal_connect (G_OBJECT (button), "drag_data_get", G_CALLBACK (cb_xfwm4_title_button_data_get), target_entry[0].target);
        g_signal_connect (G_OBJECT (button), "drag_begin", G_CALLBACK (cb_xfwm4_title_button_drag_begin), NULL);
        g_signal_connect (G_OBJECT (button), "drag_end", G_CALLBACK (cb_xfwm4_title_button_drag_end), NULL);
        g_signal_connect (G_OBJECT (button), "button_press_event", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);
        g_signal_connect (G_OBJECT (button), "enter_notify_event", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);
        g_signal_connect (G_OBJECT (button), "focus", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);

        list_iter = g_list_next(list_iter);
    }

    g_list_free(children);

    xfconf_channel_get_property (xfwm4_channel, "/general/button_layout", &value);
    cb_xfwm4_channel_button_layout_changed (xfwm4_channel, "/general/button_layout", &value, GTK_CONTAINER (hidden_box));
    g_value_unset (&value);


    /* theme name */
    list_store = gtk_list_store_new(1, G_TYPE_STRING);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_set_model (GTK_TREE_VIEW (theme_name_treeview), GTK_TREE_MODEL (list_store));
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (theme_name_treeview), 0, "", renderer, "text", 0, NULL);

    theme_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (theme_name_treeview));
    gtk_tree_selection_set_mode (theme_selection, GTK_SELECTION_SINGLE);

    check_xfwm4_themes (list_store, GTK_TREE_VIEW (theme_name_treeview), xfwm4_channel, THEME_TYPE_XFWM4);

    /* keytheme name */
    list_store = gtk_list_store_new(1, G_TYPE_STRING);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_set_model (GTK_TREE_VIEW (keytheme_name_treeview), GTK_TREE_MODEL (list_store));
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (keytheme_name_treeview), 0, "", renderer, "text", 0, NULL);

    keytheme_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (keytheme_name_treeview));
    gtk_tree_selection_set_mode (keytheme_selection, GTK_SELECTION_SINGLE);

    check_xfwm4_themes (list_store, GTK_TREE_VIEW (keytheme_name_treeview), xfwm4_channel, THEME_TYPE_KEYTHEME);

    /* keytheme contents */
    list_store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    check_keytheme_contents(list_store, xfwm4_channel, GTK_TREE_VIEW(keytheme_contents_treeview));

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_set_model (GTK_TREE_VIEW (keytheme_contents_treeview), GTK_TREE_MODEL (list_store));
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (keytheme_contents_treeview), 0, _("Action"), renderer, "text", 0, NULL);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (keytheme_contents_treeview), 1, _("Button"), renderer, "text", 1, NULL);

    keytheme_contents_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (keytheme_contents_treeview));
    gtk_tree_selection_set_mode (keytheme_contents_selection, GTK_SELECTION_SINGLE);


    /* Bind easy properties */
    /* Style tab */
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/title_font",
                            G_TYPE_STRING,
                            (GObject *)title_font_button, "font-name"); 
    g_signal_connect (G_OBJECT(xfwm4_channel),
                      "property-changed::/general/title_alignment",
                      G_CALLBACK(cb_xfwm4_channel_title_alignment_changed),
                      title_align_combo);
    g_signal_connect (G_OBJECT(xfwm4_channel), 
                      "property-changed::/general/button_layout",
                      G_CALLBACK(cb_xfwm4_channel_button_layout_changed),
                      hidden_box);
    /* Focus tab */
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/focus_delay",
                            G_TYPE_INT,
                            (GObject *)focus_delay_scale, "value");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/click_to_focus",
                            G_TYPE_BOOLEAN,
                            (GObject *)click_to_focus_mode, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/raise_on_click",
                            G_TYPE_BOOLEAN,
                            (GObject *)raise_on_click_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/raise_on_focus",
                            G_TYPE_BOOLEAN,
                            (GObject *)raise_on_focus_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/focus_new",
                            G_TYPE_BOOLEAN,
                            (GObject *)focus_new_check, "active");

    /* Keyboard tab */
    g_signal_connect (G_OBJECT(xfwm4_channel),
                      "property-changed::/general/keytheme",
                      G_CALLBACK (cb_xfwm4_channel_keytheme_changed),
                      keytheme_contents_treeview);
    g_signal_connect (G_OBJECT (keytheme_contents_treeview),
                      "row-activated",
                      G_CALLBACK (cb_xfwm4_keytheme_contents_row_activated),
                      NULL);

    /* Advanced tab */
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/snap_width",
                            G_TYPE_INT,
                            (GObject *)snap_width_scale, "value");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/wrap_resistance",
                            G_TYPE_INT,
                            (GObject *)wrap_resistance_scale, "value");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/box_move",
                            G_TYPE_BOOLEAN,
                            (GObject *)box_move_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/box_resize",
                            G_TYPE_BOOLEAN,
                            (GObject *)box_resize_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/snap_to_border",
                            G_TYPE_BOOLEAN,
                            (GObject *)snap_to_border_check, "active");
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/snap_to_windows",
                            G_TYPE_BOOLEAN,
                            (GObject *)snap_to_window_check, "active");
    g_signal_connect (G_OBJECT(xfwm4_channel),
                      "property-changed::/general/double_click_action",
                      G_CALLBACK(cb_xfwm4_channel_double_click_action_changed),
                      double_click_action_combo);

    g_signal_connect (G_OBJECT(theme_selection), "changed", G_CALLBACK (cb_xfwm4_theme_treeselection_changed), xfwm4_channel);
    g_signal_connect (G_OBJECT(keytheme_selection), "changed", G_CALLBACK (cb_xfwm4_keytheme_treeselection_changed), xfwm4_channel);

    vbox = glade_xml_get_widget (gxml, "main-vbox");
    dialog = glade_xml_get_widget (gxml, "main-dialog");
    add_keytheme_dialog = glade_xml_get_widget (gxml, "add-keytheme-dialog");
    add_keytheme_name_entry = glade_xml_get_widget (gxml, "add_keytheme_name_entry");

    gtk_widget_show_all(vbox);

    return dialog;
}


static GOptionEntry entries[] =
{
    {    "version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        N_("Version information"),
        NULL
    },
    { NULL }
};


int 
main(int argc, gchar **argv)
{
    GladeXML *gxml;
    GtkWidget *dialog;
    GError *cli_error = NULL;

    #ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    #endif

    if(!gtk_init_with_args(&argc, &argv, _("."), entries, PACKAGE, &cli_error))
    {
        if (cli_error != NULL)
        {
            g_print (_("%s: %s\nTry %s --help to see a full list of available command line options.\n"), PACKAGE, cli_error->message, PACKAGE_NAME);
            g_error_free (cli_error);
            return 1;
        }
    }

    if(version)
    {
        g_print("%s\n", PACKAGE_STRING);
        return 0;
    }

    xfconf_init(NULL);

    gxml = glade_xml_new_from_buffer (xfwm4_dialog_glade,
                                      xfwm4_dialog_glade_length,
                                      NULL, NULL);

    dialog = xfwm4_dialog_new_from_xml (gxml);

    gtk_dialog_run(GTK_DIALOG(dialog));

    xfconf_shutdown();

    return 0;
}

static void
cb_xfwm4_theme_treeselection_changed (GtkTreeSelection *selection, XfconfChannel *channel)
{
    GtkTreeModel *model = NULL;
    GList *list = gtk_tree_selection_get_selected_rows (selection, &model);
    GtkTreeIter iter;
    GValue value = {0,};

    /* valid failure */
    if ( g_list_length (list) == 0)
        return;

    /* everything else is invalid */
    g_return_if_fail (g_list_length (list) == 1);

    gtk_tree_model_get_iter (model, &iter, list->data);
    gtk_tree_model_get_value (model, &iter, 0, &value);
    
    xfconf_channel_set_property (channel, "/general/theme", &value);

    g_value_unset (&value);

    g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (list);
}

static void
cb_xfwm4_keytheme_treeselection_changed (GtkTreeSelection *selection, XfconfChannel *channel)
{
    GtkTreeModel *model = NULL;
    GList *list = gtk_tree_selection_get_selected_rows (selection, &model);
    GtkTreeIter iter;
    GValue value = {0,};

    /* valid failure */
    if ( g_list_length (list) == 0)
        return;

    /* everything else is invalid */
    g_return_if_fail (g_list_length (list) == 1);

    gtk_tree_model_get_iter (model, &iter, list->data);
    gtk_tree_model_get_value (model, &iter, 0, &value);
    
    xfconf_channel_set_property (channel, "/general/keytheme", &value);

    g_value_unset (&value);

    g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
    g_list_free (list);
}

static void
cb_xfwm4_dbl_click_changed (GtkComboBox *combo, XfconfChannel *channel)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    GValue value = { 0, };

    model = gtk_combo_box_get_model (combo);

    gtk_combo_box_get_active_iter (combo, &iter);
    gtk_tree_model_get_value (model, &iter, 1, &value);

    g_signal_handlers_block_by_func (channel, G_CALLBACK (cb_xfwm4_channel_double_click_action_changed), combo);
    xfconf_channel_set_property (channel, "/general/double_click_action", &value);
    g_signal_handlers_unblock_by_func (channel, G_CALLBACK (cb_xfwm4_channel_double_click_action_changed), combo);

    g_value_unset (&value);
}

static void
cb_xfwm4_title_align_changed (GtkComboBox *combo, XfconfChannel *channel)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    GValue value = { 0, };

    model = gtk_combo_box_get_model (combo);

    gtk_combo_box_get_active_iter (combo, &iter);
    gtk_tree_model_get_value (model, &iter, 1, &value);

    g_signal_handlers_block_by_func (channel, G_CALLBACK (cb_xfwm4_channel_title_alignment_changed), combo);
    xfconf_channel_set_property (channel, "/general/title_alignment", &value);
    g_signal_handlers_unblock_by_func (channel, G_CALLBACK (cb_xfwm4_channel_title_alignment_changed), combo);

    g_value_unset (&value);
}

static void
cb_xfwm4_title_button_alignment_changed (GtkComboBox *combo, GtkButton *button)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    GValue value = { 0, };
    const gchar *str_value;
    float align = 0.5f;

    model = gtk_combo_box_get_model (combo);

    gtk_combo_box_get_active_iter (combo, &iter);
    gtk_tree_model_get_value (model, &iter, 1, &value);

    str_value = g_value_get_string (&value);

    if (strcmp (str_value, "left") == 0)
    {
      align = 0.0f;
    }
    else if (strcmp (str_value, "right") == 0)
    {
      align = 1.0f;
    }

    g_value_unset (&value);

    gtk_button_set_alignment (button, align, 0.5f);
}

static void
cb_xfwm4_channel_double_click_action_changed(XfconfChannel *channel,
                                             const gchar *property,
                                             const GValue *value,
                                             GtkComboBox *combo)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    const gchar *str_value;
    GValue value2 = { 0, };

    model = gtk_combo_box_get_model (combo);

    str_value = g_value_get_string (value);

    gtk_tree_model_get_iter_first (model, &iter);
    do {
        gtk_tree_model_get_value (model, &iter, 1, &value2);
        if (strcmp (g_value_get_string (&value2), str_value) == 0)
        {
            g_value_unset (&value2);
            g_signal_handlers_block_by_func (combo, G_CALLBACK (cb_xfwm4_dbl_click_changed), channel);
            gtk_combo_box_set_active_iter (combo, &iter);
            g_signal_handlers_unblock_by_func (combo, G_CALLBACK (cb_xfwm4_dbl_click_changed), channel);
            break;
        }
        g_value_unset (&value2);
    }
    while (gtk_tree_model_iter_next (model, &iter));
}

static void
cb_xfwm4_channel_title_alignment_changed(XfconfChannel *channel,
                                         const gchar *property,
                                         const GValue *value,
                                         GtkComboBox *combo)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    const gchar *str_value;
    GValue value2 = { 0, };

    model = gtk_combo_box_get_model (combo);

    str_value = g_value_get_string (value);

    gtk_tree_model_get_iter_first (model, &iter);
    do {
        gtk_tree_model_get_value (model, &iter, 1, &value2);
        if (strcmp (g_value_get_string (&value2), str_value) == 0)
        {
            g_value_unset (&value2);
            g_signal_handlers_block_by_func (combo, G_CALLBACK (cb_xfwm4_title_align_changed), channel);
            gtk_combo_box_set_active_iter (combo, &iter);
            g_signal_handlers_unblock_by_func (combo, G_CALLBACK (cb_xfwm4_title_align_changed), channel);
            break;
        }
        g_value_unset (&value2);
    }
    while (gtk_tree_model_iter_next (model, &iter));
}

static void
save_button_layout (GtkContainer *container, XfconfChannel *channel)
{
    GList *children, *list_iter;
    gchar *str_value = NULL;
    gchar *tmp_string = "";

    children = list_iter = gtk_container_get_children(container);

    while(list_iter)
    {
        GtkWidget *button;

        button = GTK_WIDGET(list_iter->data);

        tmp_string = g_strconcat(tmp_string, (const gchar*)g_object_get_data (G_OBJECT (button), "key_char"), NULL);
        g_free (str_value);
        str_value = tmp_string;

        list_iter = g_list_next(list_iter);
    }

    g_list_free(children);

    g_signal_handlers_block_by_func (channel, G_CALLBACK (cb_xfwm4_channel_button_layout_changed), NULL);
    xfconf_channel_set_string (channel, "/general/button_layout", str_value);
    g_signal_handlers_unblock_by_func (channel, G_CALLBACK (cb_xfwm4_channel_button_layout_changed), NULL);

    g_free (str_value);
}

static gboolean
str_starts_with(const gchar *str, const gchar *start)
{
    while (*start)
    {
        if (*str++ != *start++)
            return FALSE;
    }

    return TRUE;
}

static void
cb_xfwm4_channel_button_layout_changed(XfconfChannel *channel,
                                       const gchar *property,
                                       const GValue *value,
                                       GtkContainer *hidden_box)
{
    GList *children, *list_iter;
    const gchar *str_value;
    GtkContainer *active_box;

    str_value = g_value_get_string (value);

    active_box = g_object_get_data (G_OBJECT (hidden_box), "active_box");

    gtk_widget_set_app_paintable (GTK_WIDGET (active_box), FALSE);
    gtk_widget_set_app_paintable (GTK_WIDGET (hidden_box), FALSE);

    /* move all the buttons to the hidden list, except the title */
    children = list_iter = gtk_container_get_children(active_box);

    while (list_iter)
    {
        GtkWidget *button;
        const gchar *key_char;

        button = GTK_WIDGET(list_iter->data);

        key_char = (const gchar*)g_object_get_data (G_OBJECT (button), "key_char");

        if (key_char[0] != '|')
        {
            gtk_container_remove(active_box, button);
            gtk_box_pack_start (GTK_BOX (hidden_box), button, FALSE, FALSE, 0);
        }

        list_iter = g_list_next(list_iter);
    }

    g_list_free(children);

    /* move the buttons to the active list, in the correct order */
    children = g_list_concat (gtk_container_get_children (active_box), gtk_container_get_children (hidden_box));

    while (*str_value)
    {
        list_iter = children;
        while (list_iter)
        {
            GtkWidget *button;
            const gchar *key_char;

            button = GTK_WIDGET(list_iter->data);

            key_char = (const gchar*)g_object_get_data (G_OBJECT (button), "key_char");

            if (str_starts_with(str_value, key_char))
            {
                gtk_container_remove(GTK_CONTAINER (gtk_widget_get_parent (button)), button);
                gtk_box_pack_start (GTK_BOX (active_box), button, key_char[0] == '|', key_char[0] == '|', 0);
                break;
            }

            list_iter = g_list_next(list_iter);
        }

        str_value++;
    }

    g_list_free(children);

    gtk_widget_set_app_paintable (GTK_WIDGET (active_box), TRUE);
    gtk_widget_set_app_paintable (GTK_WIDGET (hidden_box), TRUE);
}

static void
cb_xfwm4_active_frame_drag_data (GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, GtkSelectionData *data, guint info, guint time, GtkWidget *active_box)
{
    GtkWidget *source = gtk_drag_get_source_widget(drag_context);
    GtkWidget *parent = gtk_widget_get_parent(source);

    gtk_widget_ref(source);
    gtk_container_remove(GTK_CONTAINER(parent), source);
    gtk_box_pack_start(GTK_BOX(active_box), source, info == 3, info == 3, 0);
    gtk_widget_unref(source);

    guint button = 0;
    gint xoffset = widget->allocation.x;
    GtkWidget *item;

    GList *iter, *children = iter = gtk_container_get_children(GTK_CONTAINER(active_box));

    gint i = 0;

    while(iter)
    {
        item = GTK_WIDGET(iter->data);

        if(GTK_WIDGET_VISIBLE(item))
        {
            button++;
            if(x < (item->allocation.width/2 + item->allocation.x - xoffset))
            {
                break;
            }
        }
        i++;
        iter = g_list_next(iter);
    }

    g_list_free(children);

    gtk_box_reorder_child(GTK_BOX(active_box), source, i);

    save_button_layout (GTK_CONTAINER (active_box), g_object_get_data (G_OBJECT (active_box), "xfwm4_channel"));
}

static gboolean
cb_xfwm4_active_frame_drag_motion (GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, guint time, GtkWidget *active_box)
{
    guint button = 0;
    gint ix, iy;
    gint xoffset = widget->allocation.x;
    GtkWidget *item;
    GdkWindow *indicator;

    GList *iter, *children = iter = gtk_container_get_children(GTK_CONTAINER(active_box));

    ix = active_box->allocation.x + gtk_container_get_border_width(GTK_CONTAINER(active_box));

    while(iter)
    {
        item = GTK_WIDGET(iter->data);

        if(GTK_WIDGET_VISIBLE(item))
        {
            button++;
            if(x < (item->allocation.width/2 + item->allocation.x - xoffset))
            {
                ix = item->allocation.x;
                break;
            }
            ix = item->allocation.x + item->allocation.width;
        }
        iter = g_list_next(iter);
    }

    g_list_free(children);

    ix -= INDICATOR_SIZE/2 + 1;
    iy = active_box->allocation.y - INDICATOR_SIZE/2 + gtk_container_get_border_width(GTK_CONTAINER(active_box));

    indicator = g_object_get_data (G_OBJECT (active_box), "indicator_window");
    if(!indicator)
    {
        xfwm4_create_indicator(active_box, ix, iy, INDICATOR_SIZE, active_box->allocation.height + INDICATOR_SIZE - gtk_container_get_border_width(GTK_CONTAINER(active_box))*2);
    }
    else
    {
        gdk_window_move(indicator, ix, iy);
    }

    return FALSE;
}

static void
cb_xfwm4_active_frame_drag_leave (GtkWidget *widget, GdkDragContext *drag_context, guint time, GtkWidget *active_box)
{
    xfwm4_delete_indicator (active_box);
}

static void
cb_xfwm4_hidden_frame_drag_data (GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, GtkSelectionData *data, guint info, guint time, GtkWidget *hidden_box)
{
    GtkWidget *source = gtk_drag_get_source_widget(drag_context);
    GtkWidget *parent = gtk_widget_get_parent(source);

    /* if the item was dragged back to the location it already was */
    if(parent == hidden_box)
    {
        return;
    }

    gtk_widget_ref(source);
    gtk_container_remove(GTK_CONTAINER(parent), source);
    gtk_box_pack_start(GTK_BOX(hidden_box), source, FALSE, FALSE, 0);
    gtk_widget_unref(source);

    save_button_layout (g_object_get_data (G_OBJECT (hidden_box), "active_box"), g_object_get_data (G_OBJECT (parent), "xfwm4_channel"));
}

static void
cb_xfwm4_title_button_data_get (GtkWidget *widget, GdkDragContext *drag_context, GtkSelectionData *data, guint info, guint time, const gchar *atom_name)
{
    gtk_widget_hide(widget);
    gtk_selection_data_set(data, gdk_atom_intern(atom_name, FALSE), 8, NULL, 0);
}

static void
cb_xfwm4_title_button_drag_begin (GtkWidget *widget, GdkDragContext *drag_context, gpointer user_data)
{
    GdkPixbuf *pixbuf = xfwm4_create_icon_from_widget(widget);

    gtk_drag_source_set_icon_pixbuf(widget, pixbuf);
    g_object_unref(G_OBJECT(pixbuf));
    gtk_widget_hide(widget);
}

static void
cb_xfwm4_title_button_drag_end (GtkWidget *widget, GdkDragContext *drag_context, gpointer user_data)
{
    gtk_widget_show(widget);
}

static gboolean
cb_xfwm4_signal_blocker (GtkWidget *widget, gpointer user_data)
{
    return TRUE;
}

static GdkPixbuf*
xfwm4_create_icon_from_widget(GtkWidget *widget)
{
    GdkWindow *drawable = GDK_DRAWABLE(gtk_widget_get_parent_window(widget));
    return gdk_pixbuf_get_from_drawable(NULL, drawable, NULL, widget->allocation.x, widget->allocation.y, 0, 0, widget->allocation.width, widget->allocation.height);
}

static void
xfwm4_create_indicator(GtkWidget *widget, gint x, gint y, gint width, gint height)
{
    gint attr_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_COLORMAP | GDK_WA_VISUAL;
    GdkWindowAttr attributes = {
        NULL,
        0,
        x, y,
        width, height,
        GDK_INPUT_OUTPUT,
        gtk_widget_get_visual(widget),
        gtk_widget_get_colormap(widget),
        GDK_WINDOW_CHILD, 
        NULL, NULL, NULL, FALSE
    };

    GdkWindow *indicator = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attr_mask);
    gdk_window_set_user_data(indicator, widget);
    g_object_set_data (G_OBJECT (widget), "indicator_window", indicator);

    GdkPoint points[9];
    points[0].x = 0;
    points[0].y = 0;
    points[1].x = width;
    points[1].y = 0;
    points[2].x = width/2+1;
    points[2].y = width/2;
    points[3].x = width/2+1;
    points[3].y = height-1-width/2;
    points[4].x = width;
    points[4].y = height;
    points[5].x = 0;
    points[5].y = height-1;
    points[6].x = width/2;
    points[6].y = height-1-width/2;
    points[7].x = width/2;
    points[7].y = width/2;
    points[8].x = 0;
    points[8].y = 0;
    GdkRegion *shape = gdk_region_polygon(points, 9, GDK_WINDING_RULE);

    gdk_window_shape_combine_region(indicator, shape, 0, 0);

    gdk_window_show(indicator);
    gdk_window_raise(indicator);
}

static void
xfwm4_delete_indicator(GtkWidget *widget)
{
    GdkWindow *indicator = g_object_get_data (G_OBJECT (widget), "indicator_window");
    if(indicator)
    {
        gdk_window_destroy(indicator);
        g_object_set_data (G_OBJECT (widget), "indicator_window", NULL);
    }
}

static void
cb_xfwm4_add_keytheme_button_clicked (GtkWidget *widget, gpointer user_data)
{

    FILE *new_theme;
    FILE *default_theme;
    gchar buf[80];
    gchar *new_theme_path = NULL;
    gchar *new_theme_file = NULL;

    while (TRUE)
    {
        gint result = gtk_dialog_run (GTK_DIALOG (add_keytheme_dialog));
        gtk_widget_hide (add_keytheme_dialog);
        if (result == GTK_RESPONSE_OK)
        {
            if (FALSE)
            {
                xfce_err (_("A keybinding theme with the same name already exists"));
                continue;
            }

            if (strlen (gtk_entry_get_text (GTK_ENTRY (add_keytheme_name_entry))) == 0)
            {
                xfce_err (_("You have to provide a name for the keybinding theme"));
                continue;
            }

            /* create theme (copy default) */
            new_theme_path =
                g_strdup_printf ("%s/xfwm4/%s", gtk_entry_get_text (GTK_ENTRY (add_keytheme_name_entry)), KEYTHEMERC);
            new_theme_file =
                xfce_resource_save_location (XFCE_RESOURCE_THEMES, new_theme_path, TRUE);

            new_theme = fopen (new_theme_file, "w+");
            if (!new_theme)
            {
                g_warning ("unable to create the new theme file");
                break;
            }

            default_theme = fopen (xfwm4_keythemerc_filename, "r");
            if (!default_theme)
            {
                g_warning ("unable to open the default theme file");
                fclose (new_theme);
                break;
            }

            while (fgets (buf, sizeof (buf), default_theme))
            {
                fputs (buf, new_theme);
            }

            fclose (new_theme);
            fclose (default_theme);
        }
        break;
    }

    g_free (new_theme_path);
    g_free (new_theme_file);
}

static void
cb_xfwm4_del_keytheme_button_clicked (GtkWidget *widget, gpointer user_data)
{
    if (xfce_confirm (_("Do you really want to remove this keybinding theme ?"), GTK_STOCK_YES,
            NULL))
    {
    }
}

static void
cb_xfwm4_channel_keytheme_changed (XfconfChannel *channel, const gchar *property, const GValue *value, GtkTreeView *treeview)
{
    GtkListStore *list_store = GTK_LIST_STORE (gtk_tree_view_get_model(treeview));
    check_keytheme_contents(list_store, channel, treeview);
}

static void
cb_xfwm4_keytheme_contents_row_activated (GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    GtkTreeIter iter;
    GValue value = {0,};
    GtkTreeModel *tree_model = gtk_tree_view_get_model (tree_view);

    gtk_tree_model_get_iter (tree_model, &iter, path);
    gtk_tree_model_get_value (tree_model, &iter, 0, &value);
    g_print ("shortcut for '%s' : \n", g_value_get_string (&value));
}
