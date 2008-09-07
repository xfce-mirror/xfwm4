/*
 * Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * 
 * Based on the mcs-plugin written by Olivier Fourdan.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <dbus/dbus-glib.h>

#include <libxfcegui4/libxfcegui4.h>
#include <xfconf/xfconf.h>

#include "xfwm4-dialog_glade.h"
#include "shortcut-dialog.h"



enum
{
  SHORTCUTS_NAME_COLUMN = 0,
  SHORTCUTS_SHORTCUT_COLUMN,
  SHORTCUTS_INTERNAL_NAME_COLUMN,
  SHORTCUTS_DEFAULT_SHORTCUT_COLUMN,
};

typedef struct
{
  XfconfChannel *channel;
  GtkListStore *store;
  GHashTable *table;
  GtkTreeIter *iter;
  const GValue *value;
  gboolean ignore_internal_conflicts;
  const gchar *shortcut;
} ShortcutContext;

typedef struct
{
  const gchar *name;
  const gchar *internal_name;
  const gchar *shortcut;
} ShortcutTmpl;

typedef struct
{
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

static ShortcutTmpl shortcuts_defaults[] = {
  {N_("Window operations menu"), "popup_menu_key", "<Mod4>m"},
  {N_("Up"), "up_key", "<Mod4>o"},
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
static gchar *xfwm4_themerc_filename = NULL;



static void cb_xfwm4_theme_treeselection_changed (GtkTreeSelection *selection, XfconfChannel *channel);
static void cb_xfwm4_dbl_click_changed (GtkComboBox *combo, XfconfChannel *channel);
static void cb_xfwm4_title_align_changed (GtkComboBox *combo, XfconfChannel *channel);
static void cb_xfwm4_title_button_alignment_changed (GtkComboBox *combo, GtkButton *button);
static void cb_xfwm4_channel_double_click_action_changed (XfconfChannel *channel, const gchar *property, const GValue *value, GtkComboBox *combo);
static void cb_xfwm4_channel_title_alignment_changed (XfconfChannel *channel, const gchar *property, const GValue *value, GtkComboBox *combo);
static void cb_xfwm4_channel_button_layout_changed (XfconfChannel *channel, const gchar *property, const GValue *value, GtkContainer *hidden_box);
static gboolean str_starts_with (const gchar *str, const gchar *start);
static void save_button_layout (GtkContainer *container, XfconfChannel *channel);
static void cb_xfwm4_active_frame_drag_data (GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *, guint, guint, GtkWidget *);
static gboolean cb_xfwm4_active_frame_drag_motion (GtkWidget *, GdkDragContext *, gint, gint, guint, GtkWidget *);
static void cb_xfwm4_active_frame_drag_leave (GtkWidget *, GdkDragContext *, guint, GtkWidget *);
static void cb_xfwm4_hidden_frame_drag_data (GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *, guint, guint, GtkWidget *);
static void cb_xfwm4_title_button_data_get (GtkWidget *, GdkDragContext *, GtkSelectionData *, guint, guint, const gchar *);
static void cb_xfwm4_title_button_drag_begin (GtkWidget *, GdkDragContext *, gpointer);
static void cb_xfwm4_title_button_drag_end (GtkWidget *, GdkDragContext *, gpointer);
static gboolean cb_xfwm4_signal_blocker (GtkWidget *, gpointer);
static void cb_xfwm4_shortcuts_property_changed (XfconfChannel *channel, const gchar *property, const GValue *value, GtkListStore *store);
static void cb_xfwm4_shortcuts_row_activated (GtkWidget *treeview, GtkTreePath *path, GtkTreeViewColumn *column, XfconfChannel *channel);
static void cb_xfwm4_shortcuts_reset (GtkWidget *button, GtkListStore *store);

static GdkPixbuf *xfwm4_create_icon_from_widget (GtkWidget *widget);

static void xfwm4_create_indicator (GtkWidget *box, gint x, gint y, gint width, gint height);
static void xfwm4_delete_indicator (GtkWidget *box);

static const gchar *get_shortcut_name (const gchar *internal_name);
static gboolean read_shortcut_property (const GValue *value, const gchar **type, const gchar **action);
static void load_shortcuts (GtkListStore *store, XfconfChannel *channel);
static void load_shortcut_property (const gchar *property, const GValue *value, ShortcutContext *context);



void
check_xfwm4_themes (GtkListStore *list_store, GtkTreeView *tree_view, XfconfChannel *channel)
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

  active_theme_name = xfconf_channel_get_string (channel, "/general/theme", "Default");

  xfce_resource_push_path (XFCE_RESOURCE_THEMES, DATADIR G_DIR_SEPARATOR_S "themes");
  xfwm4_theme_dirs = xfce_resource_dirs (XFCE_RESOURCE_THEMES);
  xfce_resource_pop_path (XFCE_RESOURCE_THEMES);

  /*
   * Iterate over all base directories 
   */
  for (i = 0; xfwm4_theme_dirs[i] != NULL; ++i)
    {
      /*
       * Open directory handle 
       */
      dir = g_dir_open (xfwm4_theme_dirs[i], 0, NULL);

      /*
       * Try next base directory if this one cannot be read 
       */
      if (G_UNLIKELY (dir == NULL))
	continue;

      /*
       * Iterate over filenames in the directory 
       */
      while ((file = g_dir_read_name (dir)) != NULL)
	{
	  /*
	   * Build the theme style filename 
	   */
	  xfwm4rc_filename = g_build_filename (xfwm4_theme_dirs[i], file, "xfwm4", "themerc", NULL);

	  /*
	   * Check if the gtkrc file exists and the theme is not already 
	   * in the list 
	   */
	  if (g_file_test (xfwm4rc_filename, G_FILE_TEST_EXISTS) && g_slist_find_custom (check_list, file, (GCompareFunc) g_utf8_collate) == NULL)
	    {

	      /*
	       * Insert the theme in the check list 
	       */
	      check_list = g_slist_prepend (check_list, g_strdup (file));

	      /*
	       * Append ui theme to the list store 
	       */
	      gtk_list_store_append (list_store, &iter);
	      gtk_list_store_set (list_store, &iter, 0, file, -1);

	      /*
	       * Check if this is the active theme, if so, select it 
	       */
	      if (G_UNLIKELY (g_utf8_collate (file, active_theme_name) == 0))
		{
		  if (xfwm4_themerc_filename)
		    g_free (xfwm4_themerc_filename);
		  xfwm4_themerc_filename = g_strdup (xfwm4rc_filename);

		  tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
		  gtk_tree_selection_select_path (gtk_tree_view_get_selection (tree_view), tree_path);
		  gtk_tree_path_free (tree_path);
		}
	    }
	  /*
	   * Free xfwm4rc filename 
	   */
	  g_free (xfwm4rc_filename);
	}
      /*
       * Close directory handle 
       */
      g_dir_close (dir);
    }
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
  GtkTreeSelection *theme_selection;
  GtkTreeSelection *shortcuts_selection;
  GtkTargetEntry target_entry[2];
  GList *list_iter,
   * children;
  GValue value = { 0, };
  XfconfChannel *xfwm4_channel = xfconf_channel_new ("xfwm4");
  XfconfChannel *shortcuts_channel = xfconf_channel_new ("xfce4-keyboard-shortcuts");

  /*
   * Style tab 
   */
  GtkWidget *theme_name_treeview = glade_xml_get_widget (gxml, "theme_name_treeview");
  GtkWidget *title_font_button = glade_xml_get_widget (gxml, "title_font_button");
  GtkWidget *title_align_combo = glade_xml_get_widget (gxml, "title_align_combo");
  GtkWidget *active_frame = glade_xml_get_widget (gxml, "active-frame");
  GtkWidget *active_box = glade_xml_get_widget (gxml, "active-box");
  GtkWidget *hidden_frame = glade_xml_get_widget (gxml, "hidden-frame");
  GtkWidget *hidden_box = glade_xml_get_widget (gxml, "hidden-box");

  /*
   * Keyboard tab 
   */
  GtkWidget *shortcuts_treeview = glade_xml_get_widget (gxml, "shortcuts_treeview");
  GtkWidget *shortcuts_reset_button = glade_xml_get_widget (gxml, "shortcuts_reset_button");

  /*
   * Focus tab 
   */
  GtkWidget *focus_delay_scale = (GtkWidget *) gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "focus_delay_scale")));
  GtkWidget *focus_new_check = glade_xml_get_widget (gxml, "focus_new_check");
  GtkWidget *raise_on_focus_check = glade_xml_get_widget (gxml, "raise_on_focus_check");
  GtkWidget *raise_on_click_check = glade_xml_get_widget (gxml, "raise_on_click_check");
  GtkWidget *click_to_focus_mode = glade_xml_get_widget (gxml, "click_to_focus_mode");

  /*
   * Advanced tab 
   */
  GtkWidget *box_move_check = glade_xml_get_widget (gxml, "box_move_check");
  GtkWidget *box_resize_check = glade_xml_get_widget (gxml, "box_resize_check");
  GtkWidget *wrap_workspaces_check = glade_xml_get_widget (gxml, "wrap_workspaces_check");
  GtkWidget *wrap_windows_check = glade_xml_get_widget (gxml, "wrap_windows_check");
  GtkWidget *snap_to_border_check = glade_xml_get_widget (gxml, "snap_to_border_check");
  GtkWidget *snap_to_window_check = glade_xml_get_widget (gxml, "snap_to_window_check");
  GtkWidget *double_click_action_combo = glade_xml_get_widget (gxml, "double_click_action_combo");
  GtkWidget *snap_width_scale = (GtkWidget *) gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "snap_width_scale")));
  GtkWidget *wrap_resistance_scale = (GtkWidget *) gtk_range_get_adjustment (GTK_RANGE (glade_xml_get_widget (gxml, "wrap_resistance_scale")));

  /*
   * Double click action 
   */
  list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  tmpl_iter = dbl_click_values;
  while (tmpl_iter->name)
    {
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter, 0, tmpl_iter->name, 1, tmpl_iter->value, -1);
      tmpl_iter++;
    }

  gtk_cell_layout_clear (GTK_CELL_LAYOUT (double_click_action_combo));
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (double_click_action_combo), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (double_click_action_combo), renderer, "text", 0);

  gtk_combo_box_set_model (GTK_COMBO_BOX (double_click_action_combo), GTK_TREE_MODEL (list_store));

  xfconf_channel_get_property (xfwm4_channel, "/general/double_click_action", &value);
  cb_xfwm4_channel_double_click_action_changed (xfwm4_channel, "/general/double_click_action", &value, GTK_COMBO_BOX (double_click_action_combo));
  g_value_unset (&value);

  g_signal_connect (G_OBJECT (double_click_action_combo), "changed", G_CALLBACK (cb_xfwm4_dbl_click_changed), xfwm4_channel);

  /*
   * Title alignment 
   */
  list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  tmpl_iter = title_align_values;
  while (tmpl_iter->name)
    {
      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter, 0, tmpl_iter->name, 1, tmpl_iter->value, -1);
      tmpl_iter++;
    }

  gtk_cell_layout_clear (GTK_CELL_LAYOUT (title_align_combo));
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (title_align_combo), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (title_align_combo), renderer, "text", 0);

  gtk_combo_box_set_model (GTK_COMBO_BOX (title_align_combo), GTK_TREE_MODEL (list_store));

  xfconf_channel_get_property (xfwm4_channel, "/general/title_alignment", &value);
  cb_xfwm4_channel_title_alignment_changed (xfwm4_channel, "/general/title_alignment", &value, GTK_COMBO_BOX (title_align_combo));
  g_value_unset (&value);

  g_signal_connect (G_OBJECT (title_align_combo), "changed", G_CALLBACK (cb_xfwm4_title_align_changed), xfwm4_channel);

  /*
   * Button layout 
   */
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

  children = list_iter = gtk_container_get_children (GTK_CONTAINER (active_box));

  while (list_iter)
    {
      GtkWidget *button;
      const gchar *name;

      button = GTK_WIDGET (list_iter->data);
      name = gtk_widget_get_name (button);

      if (name[strlen (name) - 1] == '|')
	{
	  g_signal_connect (G_OBJECT (title_align_combo), "changed", G_CALLBACK (cb_xfwm4_title_button_alignment_changed), button);
	  cb_xfwm4_title_button_alignment_changed (GTK_COMBO_BOX (title_align_combo), GTK_BUTTON (button));
	}

      g_object_set_data (G_OBJECT (button), "key_char", (gpointer) & name[strlen (name) - 1]);
      gtk_drag_source_set (button, GDK_BUTTON1_MASK, &target_entry[1], 1, GDK_ACTION_MOVE);
      g_signal_connect (G_OBJECT (button), "drag_data_get", G_CALLBACK (cb_xfwm4_title_button_data_get), target_entry[1].target);
      g_signal_connect (G_OBJECT (button), "drag_begin", G_CALLBACK (cb_xfwm4_title_button_drag_begin), NULL);
      g_signal_connect (G_OBJECT (button), "drag_end", G_CALLBACK (cb_xfwm4_title_button_drag_end), NULL);
      g_signal_connect (G_OBJECT (button), "button_press_event", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);
      g_signal_connect (G_OBJECT (button), "enter_notify_event", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);
      g_signal_connect (G_OBJECT (button), "focus", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);

      list_iter = g_list_next (list_iter);
    }

  g_list_free (children);

  children = list_iter = gtk_container_get_children (GTK_CONTAINER (hidden_box));

  while (list_iter)
    {
      GtkWidget *button;
      const gchar *name;

      button = GTK_WIDGET (list_iter->data);
      name = gtk_widget_get_name (button);

      g_object_set_data (G_OBJECT (button), "key_char", (gpointer) & name[strlen (name) - 1]);
      gtk_drag_source_set (button, GDK_BUTTON1_MASK, &target_entry[0], 1, GDK_ACTION_MOVE);
      g_signal_connect (G_OBJECT (button), "drag_data_get", G_CALLBACK (cb_xfwm4_title_button_data_get), target_entry[0].target);
      g_signal_connect (G_OBJECT (button), "drag_begin", G_CALLBACK (cb_xfwm4_title_button_drag_begin), NULL);
      g_signal_connect (G_OBJECT (button), "drag_end", G_CALLBACK (cb_xfwm4_title_button_drag_end), NULL);
      g_signal_connect (G_OBJECT (button), "button_press_event", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);
      g_signal_connect (G_OBJECT (button), "enter_notify_event", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);
      g_signal_connect (G_OBJECT (button), "focus", G_CALLBACK (cb_xfwm4_signal_blocker), NULL);

      list_iter = g_list_next (list_iter);
    }

  g_list_free (children);

  xfconf_channel_get_property (xfwm4_channel, "/general/button_layout", &value);
  cb_xfwm4_channel_button_layout_changed (xfwm4_channel, "/general/button_layout", &value, GTK_CONTAINER (hidden_box));
  g_value_unset (&value);


  /*
   * theme name 
   */
  list_store = gtk_list_store_new (1, G_TYPE_STRING);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_set_model (GTK_TREE_VIEW (theme_name_treeview), GTK_TREE_MODEL (list_store));
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (theme_name_treeview), 0, _("Theme"), renderer, "text", 0, NULL);

  theme_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (theme_name_treeview));
  gtk_tree_selection_set_mode (theme_selection, GTK_SELECTION_SINGLE);

  check_xfwm4_themes (list_store, GTK_TREE_VIEW (theme_name_treeview), xfwm4_channel);

  /*
   * shortcut contents 
   */
  list_store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_set_model (GTK_TREE_VIEW (shortcuts_treeview), GTK_TREE_MODEL (list_store));
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (shortcuts_treeview), SHORTCUTS_NAME_COLUMN, _("Action"), renderer, "text", SHORTCUTS_NAME_COLUMN, NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (shortcuts_treeview), SHORTCUTS_SHORTCUT_COLUMN, _("Shortcut"), renderer, "text", SHORTCUTS_SHORTCUT_COLUMN, NULL);

  shortcuts_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (shortcuts_treeview));
  gtk_tree_selection_set_mode (shortcuts_selection, GTK_SELECTION_SINGLE);

  /* Load shortcuts */
  load_shortcuts (list_store, shortcuts_channel);


  /*
   * Bind easy properties 
   */
  /*
   * Style tab 
   */
  xfconf_g_property_bind (xfwm4_channel, "/general/title_font", G_TYPE_STRING, (GObject *) title_font_button, "font-name");
  g_signal_connect (G_OBJECT (xfwm4_channel), "property-changed::/general/title_alignment", G_CALLBACK (cb_xfwm4_channel_title_alignment_changed), title_align_combo);
  g_signal_connect (G_OBJECT (xfwm4_channel), "property-changed::/general/button_layout", G_CALLBACK (cb_xfwm4_channel_button_layout_changed), hidden_box);
  /*
   * Focus tab 
   */
  xfconf_g_property_bind (xfwm4_channel, "/general/focus_delay", G_TYPE_INT, (GObject *) focus_delay_scale, "value");
  xfconf_g_property_bind (xfwm4_channel, "/general/click_to_focus", G_TYPE_BOOLEAN, (GObject *) click_to_focus_mode, "active");
  xfconf_g_property_bind (xfwm4_channel, "/general/raise_on_click", G_TYPE_BOOLEAN, (GObject *) raise_on_click_check, "active");
  xfconf_g_property_bind (xfwm4_channel, "/general/raise_on_focus", G_TYPE_BOOLEAN, (GObject *) raise_on_focus_check, "active");
  xfconf_g_property_bind (xfwm4_channel, "/general/focus_new", G_TYPE_BOOLEAN, (GObject *) focus_new_check, "active");

  /*
   * Keyboard tab 
   */
  g_signal_connect (G_OBJECT (shortcuts_channel), "property-changed", G_CALLBACK (cb_xfwm4_shortcuts_property_changed), list_store);
  g_signal_connect (G_OBJECT (shortcuts_treeview), "row-activated", G_CALLBACK (cb_xfwm4_shortcuts_row_activated), shortcuts_channel);
  g_signal_connect (G_OBJECT (shortcuts_reset_button), "clicked", G_CALLBACK (cb_xfwm4_shortcuts_reset), list_store);

  /*
   * Advanced tab 
   */
  xfconf_g_property_bind (xfwm4_channel, "/general/snap_width", G_TYPE_INT, (GObject *) snap_width_scale, "value");
  xfconf_g_property_bind (xfwm4_channel, "/general/wrap_resistance", G_TYPE_INT, (GObject *) wrap_resistance_scale, "value");
  xfconf_g_property_bind (xfwm4_channel, "/general/box_move", G_TYPE_BOOLEAN, (GObject *) box_move_check, "active");
  xfconf_g_property_bind (xfwm4_channel, "/general/box_resize", G_TYPE_BOOLEAN, (GObject *) box_resize_check, "active");
  xfconf_g_property_bind (xfwm4_channel, "/general/wrap_workspaces", G_TYPE_BOOLEAN, (GObject *) wrap_workspaces_check, "active");
  xfconf_g_property_bind (xfwm4_channel, "/general/wrap_windows", G_TYPE_BOOLEAN, (GObject *) wrap_windows_check, "active");
  xfconf_g_property_bind (xfwm4_channel, "/general/snap_to_border", G_TYPE_BOOLEAN, (GObject *) snap_to_border_check, "active");
  xfconf_g_property_bind (xfwm4_channel, "/general/snap_to_windows", G_TYPE_BOOLEAN, (GObject *) snap_to_window_check, "active");
  g_signal_connect (G_OBJECT (xfwm4_channel), "property-changed::/general/double_click_action", G_CALLBACK (cb_xfwm4_channel_double_click_action_changed), double_click_action_combo);

  g_signal_connect (G_OBJECT (theme_selection), "changed", G_CALLBACK (cb_xfwm4_theme_treeselection_changed), xfwm4_channel);

  vbox = glade_xml_get_widget (gxml, "main-vbox");
  dialog = glade_xml_get_widget (gxml, "main-dialog");

  gtk_widget_show_all (vbox);

  return dialog;
}


static GOptionEntry entries[] = {
  {"version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
   N_("Version information"),
   NULL},
  {NULL}
};


int
main (int argc, gchar **argv)
{
  GladeXML *gxml;
  GtkWidget *dialog;
  GError *cli_error = NULL;

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  if (!gtk_init_with_args (&argc, &argv, _("."), entries, PACKAGE, &cli_error))
    {
      if (cli_error != NULL)
	{
	  g_print (_("%s: %s\nTry %s --help to see a full list of available command line options.\n"), PACKAGE, cli_error->message, PACKAGE_NAME);
	  g_error_free (cli_error);
	  return 1;
	}
    }

  if (version)
    {
      g_print ("%s\n", PACKAGE_STRING);
      return 0;
    }

  xfconf_init (NULL);

  gxml = glade_xml_new_from_buffer (xfwm4_dialog_glade, xfwm4_dialog_glade_length, NULL, NULL);

  dialog = xfwm4_dialog_new_from_xml (gxml);

  gtk_dialog_run (GTK_DIALOG (dialog));

  xfconf_shutdown ();

  return 0;
}

static void
cb_xfwm4_theme_treeselection_changed (GtkTreeSelection *selection, XfconfChannel *channel)
{
  GtkTreeModel *model = NULL;
  GList *list = gtk_tree_selection_get_selected_rows (selection, &model);
  GtkTreeIter iter;
  GValue value = { 0, };

  /*
   * valid failure 
   */
  if (g_list_length (list) == 0)
    return;

  /*
   * everything else is invalid 
   */
  g_return_if_fail (g_list_length (list) == 1);

  gtk_tree_model_get_iter (model, &iter, list->data);
  gtk_tree_model_get_value (model, &iter, 0, &value);

  xfconf_channel_set_property (channel, "/general/theme", &value);

  g_value_unset (&value);

  g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
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
cb_xfwm4_channel_double_click_action_changed (XfconfChannel *channel, const gchar *property, const GValue *value, GtkComboBox *combo)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  const gchar *str_value;
  GValue value2 = { 0, };

  model = gtk_combo_box_get_model (combo);

  str_value = g_value_get_string (value);

  gtk_tree_model_get_iter_first (model, &iter);
  do
    {
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
cb_xfwm4_channel_title_alignment_changed (XfconfChannel *channel, const gchar *property, const GValue *value, GtkComboBox *combo)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  const gchar *str_value;
  GValue value2 = { 0, };

  model = gtk_combo_box_get_model (combo);

  str_value = g_value_get_string (value);

  gtk_tree_model_get_iter_first (model, &iter);
  do
    {
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
  GList *children,
   * list_iter;
  gchar *str_value = NULL;
  gchar *tmp_string = "";

  children = list_iter = gtk_container_get_children (container);

  while (list_iter)
    {
      GtkWidget *button;

      button = GTK_WIDGET (list_iter->data);

      tmp_string = g_strconcat (tmp_string, (const gchar *) g_object_get_data (G_OBJECT (button), "key_char"), NULL);
      g_free (str_value);
      str_value = tmp_string;

      list_iter = g_list_next (list_iter);
    }

  g_list_free (children);

  g_signal_handlers_block_by_func (channel, G_CALLBACK (cb_xfwm4_channel_button_layout_changed), NULL);
  xfconf_channel_set_string (channel, "/general/button_layout", str_value);
  g_signal_handlers_unblock_by_func (channel, G_CALLBACK (cb_xfwm4_channel_button_layout_changed), NULL);

  g_free (str_value);
}

static gboolean
str_starts_with (const gchar *str, const gchar *start)
{
  while (*start)
    {
      if (*str++ != *start++)
	return FALSE;
    }

  return TRUE;
}

static void
cb_xfwm4_channel_button_layout_changed (XfconfChannel *channel, const gchar *property, const GValue *value, GtkContainer *hidden_box)
{
  GList *children,
   * list_iter;
  const gchar *str_value;
  GtkContainer *active_box;

  str_value = g_value_get_string (value);

  active_box = g_object_get_data (G_OBJECT (hidden_box), "active_box");

  gtk_widget_set_app_paintable (GTK_WIDGET (active_box), FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (hidden_box), FALSE);

  /*
   * move all the buttons to the hidden list, except the title 
   */
  children = list_iter = gtk_container_get_children (active_box);

  while (list_iter)
    {
      GtkWidget *button;
      const gchar *key_char;

      button = GTK_WIDGET (list_iter->data);

      key_char = (const gchar *) g_object_get_data (G_OBJECT (button), "key_char");

      if (key_char[0] != '|')
	{
	  gtk_container_remove (active_box, button);
	  gtk_box_pack_start (GTK_BOX (hidden_box), button, FALSE, FALSE, 0);
	}

      list_iter = g_list_next (list_iter);
    }

  g_list_free (children);

  /*
   * move the buttons to the active list, in the correct order 
   */
  children = g_list_concat (gtk_container_get_children (active_box), gtk_container_get_children (hidden_box));

  while (*str_value)
    {
      list_iter = children;
      while (list_iter)
	{
	  GtkWidget *button;
	  const gchar *key_char;

	  button = GTK_WIDGET (list_iter->data);

	  key_char = (const gchar *) g_object_get_data (G_OBJECT (button), "key_char");

	  if (str_starts_with (str_value, key_char))
	    {
	      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (button)), button);
	      gtk_box_pack_start (GTK_BOX (active_box), button, key_char[0] == '|', key_char[0] == '|', 0);
	      break;
	    }

	  list_iter = g_list_next (list_iter);
	}

      str_value++;
    }

  g_list_free (children);

  gtk_widget_set_app_paintable (GTK_WIDGET (active_box), TRUE);
  gtk_widget_set_app_paintable (GTK_WIDGET (hidden_box), TRUE);
}

static void
cb_xfwm4_active_frame_drag_data (GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, GtkSelectionData *data, guint info, guint time, GtkWidget *active_box)
{
  GtkWidget *source = gtk_drag_get_source_widget (drag_context);
  GtkWidget *parent = gtk_widget_get_parent (source);

  gtk_widget_ref (source);
  gtk_container_remove (GTK_CONTAINER (parent), source);
  gtk_box_pack_start (GTK_BOX (active_box), source, info == 3, info == 3, 0);
  gtk_widget_unref (source);

  guint button = 0;
  gint xoffset = widget->allocation.x;
  GtkWidget *item;

  GList *iter,
   * children = iter = gtk_container_get_children (GTK_CONTAINER (active_box));

  gint i = 0;

  while (iter)
    {
      item = GTK_WIDGET (iter->data);

      if (GTK_WIDGET_VISIBLE (item))
	{
	  button++;
	  if (x < (item->allocation.width / 2 + item->allocation.x - xoffset))
	    {
	      break;
	    }
	}
      i++;
      iter = g_list_next (iter);
    }

  g_list_free (children);

  gtk_box_reorder_child (GTK_BOX (active_box), source, i);

  save_button_layout (GTK_CONTAINER (active_box), g_object_get_data (G_OBJECT (active_box), "xfwm4_channel"));
}

static gboolean
cb_xfwm4_active_frame_drag_motion (GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, guint time, GtkWidget *active_box)
{
  guint button = 0;
  gint ix,
    iy;
  gint xoffset = widget->allocation.x;
  GtkWidget *item;
  GdkWindow *indicator;

  GList *iter,
   * children = iter = gtk_container_get_children (GTK_CONTAINER (active_box));

  ix = active_box->allocation.x + gtk_container_get_border_width (GTK_CONTAINER (active_box));

  while (iter)
    {
      item = GTK_WIDGET (iter->data);

      if (GTK_WIDGET_VISIBLE (item))
	{
	  button++;
	  if (x < (item->allocation.width / 2 + item->allocation.x - xoffset))
	    {
	      ix = item->allocation.x;
	      break;
	    }
	  ix = item->allocation.x + item->allocation.width;
	}
      iter = g_list_next (iter);
    }

  g_list_free (children);

  ix -= INDICATOR_SIZE / 2 + 1;
  iy = active_box->allocation.y - INDICATOR_SIZE / 2 + gtk_container_get_border_width (GTK_CONTAINER (active_box));

  indicator = g_object_get_data (G_OBJECT (active_box), "indicator_window");
  if (!indicator)
    {
      xfwm4_create_indicator (active_box, ix, iy, INDICATOR_SIZE, active_box->allocation.height + INDICATOR_SIZE - gtk_container_get_border_width (GTK_CONTAINER (active_box)) *2);
    }
  else
    {
      gdk_window_move (indicator, ix, iy);
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
  GtkWidget *source = gtk_drag_get_source_widget (drag_context);
  GtkWidget *parent = gtk_widget_get_parent (source);

  /*
   * if the item was dragged back to the location it already was 
   */
  if (parent == hidden_box)
    {
      return;
    }

  gtk_widget_ref (source);
  gtk_container_remove (GTK_CONTAINER (parent), source);
  gtk_box_pack_start (GTK_BOX (hidden_box), source, FALSE, FALSE, 0);
  gtk_widget_unref (source);

  save_button_layout (g_object_get_data (G_OBJECT (hidden_box), "active_box"), g_object_get_data (G_OBJECT (parent), "xfwm4_channel"));
}

static void
cb_xfwm4_title_button_data_get (GtkWidget *widget, GdkDragContext *drag_context, GtkSelectionData *data, guint info, guint time, const gchar *atom_name)
{
  gtk_widget_hide (widget);
  gtk_selection_data_set (data, gdk_atom_intern (atom_name, FALSE), 8, NULL, 0);
}

static void
cb_xfwm4_title_button_drag_begin (GtkWidget *widget, GdkDragContext *drag_context, gpointer user_data)
{
  GdkPixbuf *pixbuf = xfwm4_create_icon_from_widget (widget);

  gtk_drag_source_set_icon_pixbuf (widget, pixbuf);
  g_object_unref (G_OBJECT (pixbuf));
  gtk_widget_hide (widget);
}

static void
cb_xfwm4_title_button_drag_end (GtkWidget *widget, GdkDragContext *drag_context, gpointer user_data)
{
  gtk_widget_show (widget);
}

static gboolean
cb_xfwm4_signal_blocker (GtkWidget *widget, gpointer user_data)
{
  return TRUE;
}

static GdkPixbuf *
xfwm4_create_icon_from_widget (GtkWidget *widget)
{
  GdkWindow *drawable = GDK_DRAWABLE (gtk_widget_get_parent_window (widget));

  return gdk_pixbuf_get_from_drawable (NULL, drawable, NULL, widget->allocation.x, widget->allocation.y, 0, 0, widget->allocation.width, widget->allocation.height);
}

static void
xfwm4_create_indicator (GtkWidget *widget, gint x, gint y, gint width, gint height)
{
  gint attr_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_COLORMAP | GDK_WA_VISUAL;
  GdkWindowAttr attributes = {
    NULL,
    0,
    x, y,
    width, height,
    GDK_INPUT_OUTPUT,
    gtk_widget_get_visual (widget),
    gtk_widget_get_colormap (widget),
    GDK_WINDOW_CHILD,
    NULL, NULL, NULL, FALSE
  };

  GdkWindow *indicator = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes,
					 attr_mask);

  gdk_window_set_user_data (indicator, widget);
  g_object_set_data (G_OBJECT (widget), "indicator_window", indicator);

  GdkPoint points[9];

  points[0].x = 0;
  points[0].y = 0;
  points[1].x = width;
  points[1].y = 0;
  points[2].x = width / 2 + 1;
  points[2].y = width / 2;
  points[3].x = width / 2 + 1;
  points[3].y = height - 1 - width / 2;
  points[4].x = width;
  points[4].y = height;
  points[5].x = 0;
  points[5].y = height - 1;
  points[6].x = width / 2;
  points[6].y = height - 1 - width / 2;
  points[7].x = width / 2;
  points[7].y = width / 2;
  points[8].x = 0;
  points[8].y = 0;
  GdkRegion *shape = gdk_region_polygon (points, 9, GDK_WINDING_RULE);

  gdk_window_shape_combine_region (indicator, shape, 0, 0);

  gdk_window_show (indicator);
  gdk_window_raise (indicator);
}

static void
xfwm4_delete_indicator (GtkWidget *widget)
{
  GdkWindow *indicator = g_object_get_data (G_OBJECT (widget), "indicator_window");

  if (indicator)
    {
      gdk_window_destroy (indicator);
      g_object_set_data (G_OBJECT (widget), "indicator_window", NULL);
    }
}



static gboolean
request_shortcut_confirmation (XfconfChannel *channel, const gchar *property, const gchar *internal_name, gboolean ignore_internal_conflicts)
{
  GValue value = { 0 };
  const gchar *type;
  const gchar *action;
  const gchar *name;
  gboolean shortcut_accepted = TRUE;
  gchar *escaped_shortcut;
  gchar *primary_text;
  gchar *secondary_text;
  gchar *other_name;
  gchar *option_accept;
  gchar *option_reject;
  gint response;

  if (G_LIKELY (xfconf_channel_get_property (channel, property, &value) && read_shortcut_property (&value, &type, &action)))
    {
      if (G_LIKELY (g_utf8_collate (type, "xfwm4") != 0 || !ignore_internal_conflicts))
        {
          if (G_LIKELY (g_utf8_collate (type, "xfwm4") != 0 || g_utf8_collate (action, internal_name) != 0))
            {
              /* TODO: Maybe add another parameter (via context) to this function to switch between resolving
               * and ignoring xfwm4-only conflicts (for the use of "Reset to defaults") */

              name = get_shortcut_name (internal_name);
          
              /* Build primary error message and insert the shortcut */
              escaped_shortcut = g_markup_escape_text (property+1, -1);
              primary_text = g_strdup_printf (_("%s shortcut conflict"), escaped_shortcut);
              g_free (escaped_shortcut);
          
              /* Generate error description based on the type of the confilcting shortcut */
              if (g_utf8_collate (type, "xfwm4") == 0)
                {
                  other_name = g_strdup (get_shortcut_name (action));
                  secondary_text = g_strdup_printf (_("This shortcut is already being used by the window manager action <b>%s</b>. "
                                                      "Which action do you want to use?"),
                                                      other_name);
                }
              else
                {
                  other_name = g_markup_escape_text (action, -1);
                  secondary_text = g_strdup_printf (_("The shortcut is already being used for the command <b>%s</b>. "
                                                      "Which action do you want to use?"), 
                                                    other_name);
                }

              option_accept = g_markup_printf_escaped (_("Use %s"), name);
              option_reject = g_markup_printf_escaped (_("Keep %s"), other_name);
          
              /* Ask the user what to do */
              response = xfce_message_dialog (NULL, _("Shortcut conflict"), GTK_STOCK_DIALOG_ERROR,
                                              primary_text, secondary_text,
                                              XFCE_CUSTOM_BUTTON, option_accept, GTK_RESPONSE_ACCEPT,
                                              XFCE_CUSTOM_BUTTON, option_reject, GTK_RESPONSE_REJECT,
                                              NULL);

              shortcut_accepted = (response == GTK_RESPONSE_ACCEPT);

              /* Free strings */
              g_free (option_accept);
              g_free (option_reject);
              g_free (primary_text);
              g_free (secondary_text);
              g_free (other_name);
            }
        }
    }
  else
    {
      xfce_err (_("The shortcut '%s' is already being used for something else."), property+1);
      shortcut_accepted = FALSE;
    }

  return shortcut_accepted;
}



static gboolean
validate_shortcut (ShortcutDialog *dialog, const gchar *shortcut, ShortcutContext *context)
{
  gboolean shortcut_accepted = TRUE;
  gchar *current_shortcut;
  gchar *internal_name;
  gchar *property;

  /* Ignore raw 'Return' and 'space' since they may have been used to activate the shortcut row */
  if (G_UNLIKELY (g_utf8_collate (shortcut, "Return") == 0 || g_utf8_collate (shortcut, "space") == 0))
    return FALSE;

  /* Ignore empty shortcuts */
  if (G_UNLIKELY (g_utf8_strlen (shortcut, -1) == 0))
    return FALSE;

  /* Build property name */
  property = g_strdup_printf ("/%s", shortcut);

  if (G_LIKELY (context->iter != NULL))
    {
      /* Get the shortcut of the row we're currently editing */
      gtk_tree_model_get (GTK_TREE_MODEL (context->store), context->iter,
                          SHORTCUTS_SHORTCUT_COLUMN, &current_shortcut,
                          SHORTCUTS_INTERNAL_NAME_COLUMN, &internal_name, -1);

      /* Let the user handle conflicts if there are any */
      if (G_UNLIKELY (xfconf_channel_has_property (context->channel, property)))
        shortcut_accepted = request_shortcut_confirmation (context->channel, property, internal_name, FALSE);

      /* Free strings */
      g_free (internal_name);
      g_free (current_shortcut);
    }
  else
    {
      /* Let the user handle conflicts if there are any */
      if (G_UNLIKELY (xfconf_channel_has_property (context->channel, property)))
        shortcut_accepted = request_shortcut_confirmation (context->channel, property, _("This action"), FALSE);
    }

  /* Free strings */
  g_free (property);

  return shortcut_accepted;
}



static gboolean
update_shortcut (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, ShortcutContext *context)
{
  const gchar *type;
  const gchar *action;
  gboolean exit_loop = FALSE;
  gchar *internal_name;
  gchar *shortcut;

  gtk_tree_model_get (model, iter, SHORTCUTS_SHORTCUT_COLUMN, &shortcut, SHORTCUTS_INTERNAL_NAME_COLUMN, &internal_name, -1);

  if (G_LIKELY (shortcut != NULL))
    {
      if (G_UNLIKELY (g_utf8_collate (shortcut, context->shortcut) == 0))
        {
          if (G_LIKELY (context->value != NULL))
            {
              if (G_LIKELY (read_shortcut_property (context->value, &type, &action)))
                {
                  if (G_UNLIKELY (g_utf8_collate (type, "xfwm4") == 0 && g_utf8_collate (action, internal_name) == 0))
                    gtk_list_store_set (GTK_LIST_STORE (model), iter, SHORTCUTS_SHORTCUT_COLUMN, context->shortcut, -1);
                  else
                    gtk_list_store_set (GTK_LIST_STORE (model), iter, SHORTCUTS_SHORTCUT_COLUMN, NULL, -1);
                }
            }
          else
            gtk_list_store_set (GTK_LIST_STORE (model), iter, SHORTCUTS_SHORTCUT_COLUMN, NULL, -1);
    
          exit_loop = TRUE;
        }
    }
  else
    {
      if (G_LIKELY (context->value != NULL))
        {
          if (G_LIKELY (read_shortcut_property (context->value, &type, &action)))
            {
              if (G_UNLIKELY (g_utf8_collate (type, "xfwm4") == 0 && g_utf8_collate (action, internal_name) == 0))
                {
                  gtk_list_store_set (GTK_LIST_STORE (model), iter, SHORTCUTS_SHORTCUT_COLUMN, context->shortcut, -1);
                  exit_loop = TRUE;
                }
            }
        }
    }

  /* Free strings */
  g_free (shortcut);
  g_free (internal_name);

  return exit_loop;
}



static void 
cb_xfwm4_shortcuts_property_changed (XfconfChannel *channel, const gchar *property, const GValue *value, GtkListStore *store)
{
  ShortcutContext context;

  context.value = value;
  context.shortcut = property + 1;

  gtk_tree_model_foreach (GTK_TREE_MODEL (store), (GtkTreeModelForeachFunc) update_shortcut, &context);
}



static void 
cb_xfwm4_shortcuts_row_activated (GtkWidget *treeview, GtkTreePath *path, GtkTreeViewColumn *column, XfconfChannel *channel)
{
  ShortcutContext context;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkWidget *dialog;
  const gchar *new_shortcut;
  gchar *name;
  gchar *current_shortcut;
  gchar *internal_name;
  gchar *old_property;
  gchar *new_property;
  gint response;

  /* Get tree view model */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));

  /* Convert tree path to tree iter */
  if (G_LIKELY (gtk_tree_model_get_iter (model, &iter, path)))
    {
      /* Read current shortcut from the activated row */
      gtk_tree_model_get (model, &iter, 
                          SHORTCUTS_NAME_COLUMN, &name,
                          SHORTCUTS_SHORTCUT_COLUMN, &current_shortcut, 
                          SHORTCUTS_INTERNAL_NAME_COLUMN, &internal_name, -1);

      /* Prepare callback context */
      context.channel = channel;
      context.iter = &iter;
      context.store = GTK_LIST_STORE (model);
      context.ignore_internal_conflicts = FALSE;
      
      /* Request a new shortcut from the user */
      dialog = shortcut_dialog_new (name);
      g_signal_connect (dialog, "validate-shortcut", G_CALLBACK (validate_shortcut), &context);
      response = shortcut_dialog_run (SHORTCUT_DIALOG (dialog), treeview);

      if (G_LIKELY (response == GTK_RESPONSE_OK))
        {
          /* Build property name */
          old_property = g_strdup_printf ("/%s", current_shortcut);

          /* Remove old property shortcut from the settings */
          xfconf_channel_remove_property (channel, old_property);

          /* Get the shortcut entered by the user */
          new_shortcut = shortcut_dialog_get_shortcut (SHORTCUT_DIALOG (dialog));

          /* Build property name */
          new_property = g_strdup_printf ("/%s", new_shortcut);

          /* Save the new shortcut */
          xfconf_channel_set_array (channel, new_property, G_TYPE_STRING, "xfwm4", G_TYPE_STRING, internal_name, G_TYPE_INVALID);

          /* Free strings */
          g_free (new_property);
          g_free (old_property);
        }

      /* Destroy shortcut dialog */
      gtk_widget_destroy (dialog);

      /* Free strings */
      g_free (name);
      g_free (current_shortcut);
      g_free (internal_name);
    }
}



static gboolean 
read_shortcut_property (const GValue *value, const gchar **type, const gchar **action)
{
  const GPtrArray *array;
  const GValue *type_value;
  const GValue *action_value;

  /* Make sure we only load shortcuts from string arrays */
  if (G_UNLIKELY (G_VALUE_TYPE (value) != dbus_g_type_get_collection ("GPtrArray", G_TYPE_VALUE)))
    return FALSE;

  /* Get the pointer array */
  array = g_value_get_boxed (value);

  /* Make sure the array has exactly two members */
  if (G_UNLIKELY (array->len != 2))
    return FALSE;

  /* Get the array member values */
  type_value = g_ptr_array_index (array, 0);
  action_value = g_ptr_array_index (array, 1);

  /* Make sure both are string values */
  if (G_UNLIKELY (G_VALUE_TYPE (type_value) != G_TYPE_STRING || G_VALUE_TYPE (action_value) != G_TYPE_STRING))
    return FALSE;

  /* Read shortcut type and action */
  *type = g_value_get_string (type_value);
  *action = g_value_get_string (action_value);

  return TRUE;
}



static void
load_shortcuts (GtkListStore *store, XfconfChannel *channel)
{
  ShortcutContext context;
  GtkTreeIter iter;
  GHashTable *properties;
  GHashTable *table;
  gchar *iter_str;
  int i;

  /* Create hash table for internal name to GtkTreeIter mapping */
  table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  /* Iterate over all xfwm4 features available for shortcuts */
  for (i = 0; shortcuts_defaults[i].internal_name != NULL; ++i)
    {
      /* Create a new row for the current feature */
      gtk_list_store_append (store, &iter);
      
      /* Get string representation of the iter */
      iter_str = gtk_tree_model_get_string_from_iter (GTK_TREE_MODEL (store), &iter);

      /* Map internal name of the current feature to this tree iter */
      g_hash_table_insert (table, g_strdup (shortcuts_defaults[i].internal_name), iter_str);

      /* Store feature information in the list store */
      gtk_list_store_set (store, &iter, 
                          SHORTCUTS_NAME_COLUMN, shortcuts_defaults[i].name,
                          SHORTCUTS_INTERNAL_NAME_COLUMN, shortcuts_defaults[i].internal_name,
                          SHORTCUTS_DEFAULT_SHORTCUT_COLUMN, shortcuts_defaults[i].shortcut, -1);
    }

  /* Prepare load context */
  context.channel = channel;
  context.store = store;
  context.table = table;

  /* Get all shortcut properties */
  if (G_LIKELY ((properties = xfconf_channel_get_properties (channel, NULL)) != NULL))
    {
      g_hash_table_foreach (properties, (GHFunc) load_shortcut_property, &context);
      g_hash_table_destroy (properties);
    }

  /* Destroy internal name to tree iter mapping */
  g_hash_table_destroy (table);
}



static void 
load_shortcut_property (const gchar *property, const GValue *value, ShortcutContext *context)
{
  GtkTreeIter iter;
  const gchar *type;
  const gchar *action;
  const gchar *iter_str;

  /* Read shortcut type and action */
  if (G_LIKELY (read_shortcut_property (value, &type, &action)))
    {
      /* Only load shortcuts with type 'xfwm4' */
      if (g_utf8_collate (type, "xfwm4") == 0)
        {
          /* Search for the correct tree iter in the internal name to iter mapping */
          iter_str = g_hash_table_lookup (context->table, action);

          /* Check whether the feature actually provides a tree iter */
          if (G_LIKELY (iter_str != NULL && gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (context->store), &iter, iter_str)))
            {
              /* Insert the shortcut value */
              gtk_list_store_set (context->store, &iter, SHORTCUTS_SHORTCUT_COLUMN, property+1, -1);
            }
        }
    }
}



static const gchar *
get_shortcut_name (const gchar *internal_name)
{
  int i;

  for (i = 0; shortcuts_defaults[i].internal_name != NULL; ++i)
    if (g_utf8_collate (shortcuts_defaults[i].internal_name, internal_name) == 0)
      return shortcuts_defaults[i].name;

  return NULL;
}



static gboolean
remove_shortcut (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, XfconfChannel *channel)
{
  GValue value = { 0 };
  const gchar *type;
  const gchar *action;
  gchar *shortcut;
  gchar *internal_name;
  gchar *property;

  /* Get shortcut string from the list store */
  gtk_tree_model_get (model, iter,
                      SHORTCUTS_SHORTCUT_COLUMN, &shortcut, 
                      SHORTCUTS_INTERNAL_NAME_COLUMN, &internal_name, -1);

  /* Build property name */
  property = g_strdup_printf ("/%s", shortcut);

  /* Remove property if it exists and the shortcut is really being used for the current feature */
  if (G_LIKELY (xfconf_channel_get_property (channel, property, &value) && read_shortcut_property (&value, &type, &action)))
    if (G_LIKELY (g_utf8_collate (type, "xfwm4") == 0 && g_utf8_collate (action, internal_name) == 0))
      xfconf_channel_remove_property (channel, property);

  /* Free property value data */
  if (G_IS_VALUE (&value))
    g_value_unset (&value);

  /* Free strings */
  g_free (internal_name);
  g_free (shortcut);
  g_free (property);

  return FALSE;
}



static gboolean
reset_shortcut (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, ShortcutContext *context)
{
  gchar *name;
  gchar *internal_name;
  gchar *default_shortcut;
  gchar *property;

  /* Get default shortcut for this feature */
  gtk_tree_model_get (model, iter, 
                      SHORTCUTS_NAME_COLUMN, &name,
                      SHORTCUTS_INTERNAL_NAME_COLUMN, &internal_name, 
                      SHORTCUTS_DEFAULT_SHORTCUT_COLUMN, &default_shortcut, -1);

  /* Build property name */
  property = g_strdup_printf ("/%s", default_shortcut);

  /* Check whether the shortcut is already being used */
  if (G_LIKELY (!xfconf_channel_has_property (context->channel, property)))
    {
      /* Remove the current shortcut for this feature and replace it with the default */
      remove_shortcut (model, path, iter, context->channel);
      xfconf_channel_set_array (context->channel, property, G_TYPE_STRING, "xfwm4", G_TYPE_STRING, internal_name, G_TYPE_INVALID);
    }
  else
    {
      /* Only reset the shortcut if its not used by something else or if the user wants to replace the existing binding */
      if (request_shortcut_confirmation (context->channel, property, internal_name, context->ignore_internal_conflicts))
        xfconf_channel_set_array (context->channel, property, G_TYPE_STRING, "xfwm4", G_TYPE_STRING, internal_name, G_TYPE_INVALID);
    }

  /* Free strings */
  g_free (name);
  g_free (property);

  return FALSE;
}



static void 
cb_xfwm4_shortcuts_reset (GtkWidget *button, GtkListStore *store)
{
  ShortcutContext context;
  XfconfChannel *channel;

  channel = xfconf_channel_new ("xfce4-keyboard-shortcuts");

  context.channel = channel;
  context.ignore_internal_conflicts = TRUE;

  gtk_tree_model_foreach (GTK_TREE_MODEL (store), (GtkTreeModelForeachFunc) reset_shortcut, &context);

  g_object_unref (channel);
}
