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
 
 
        shortcut editor - Copyright (C) 2004 Jean-Francois Wauthy
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <errno.h>

#include <gdk/gdkkeysyms.h>
#include <libxfcegui4/libxfcegui4.h>
#include <xfce-mcs-manager/manager-plugin.h>


#include "xfwm4_plugin.h"
#include "xfwm4_shortcuteditor.h"

typedef struct _shortcut_tree_foreach_struct shortcut_tree_foreach_struct;
struct _shortcut_tree_foreach_struct
{
    gchar *shortcut;
    gchar *path;
    GtkTreeSelection *selection;
    gboolean found;
};

/**************/
/* Popup menu */
/**************/
void
cb_popup_del_menu (GtkWidget * widget, gpointer data)
{
    Itf *itf = (Itf *) data;

    if (xfce_confirm (_("Do you really want to remove this keybinding theme ?"), GTK_STOCK_YES,
            NULL))
    {
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GtkTreeIter iter;
        ThemeInfo *ti;

        gchar *theme_name = NULL;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (itf->treeview2));

        gtk_tree_selection_get_selected (selection, &model, &iter);
        gtk_tree_model_get (model, &iter, THEME_NAME_COLUMN, &theme_name, -1);

        ti = xfwm4_plugin_find_theme_info_by_name (theme_name, keybinding_theme_list);

        if (ti)
        {
            gchar *theme_file = NULL;

            theme_file = g_build_filename (ti->path, KEY_SUFFIX, KEYTHEMERC, NULL);
            if (unlink (theme_file) != 0)
                g_warning ("Unable to remove the theme file !");
            g_free (theme_file);
        }
        else
            g_warning ("Cannot find the keytheme !");

        /* refresh list */
        while (keybinding_theme_list)
        {
            xfwm4_plugin_theme_info_free ((ThemeInfo *) keybinding_theme_list->data);
            keybinding_theme_list = g_list_next (keybinding_theme_list);
        }
        g_list_free (keybinding_theme_list);

        g_free (xfwm4_plugin_current_key_theme);
        xfwm4_plugin_current_key_theme = g_strdup ("Default");
        keybinding_theme_list = NULL;
        keybinding_theme_list =
            xfwm4_plugin_read_themes (keybinding_theme_list, itf->treeview2, itf->scrolledwindow2,
            KEYBINDING_THEMES, xfwm4_plugin_current_key_theme);
        gtk_widget_set_sensitive (itf->treeview3, FALSE);
        loadtheme_in_treeview (xfwm4_plugin_find_theme_info_by_name ("Default",
                keybinding_theme_list), itf);

        /* tell it to the mcs manager */
        mcs_manager_set_string (itf->mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL2,
            xfwm4_plugin_current_key_theme);
        mcs_manager_notify (itf->mcs_plugin->manager, CHANNEL2);
        xfwm4_plugin_write_options (itf->mcs_plugin);

        g_free (theme_name);
    }
}

void
cb_popup_add_menu (GtkWidget * widget, gpointer data)
{
    Itf *itf = (Itf *) data;
    gchar buf[80];
    GtkWidget *dialog;
    GtkWidget *hbox;
    GtkWidget *label;
    GtkWidget *entry;
    GtkWidget *header_image;
    GtkWidget *header;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    ThemeInfo *ti;

    gchar *new_theme_path = NULL;
    gchar *new_theme_file = NULL;
    gchar *theme_name = NULL;
    gchar *theme_file = NULL;

    FILE *new_theme;
    FILE *default_theme;

    dialog = gtk_dialog_new_with_buttons (_("Add keybinding theme"), GTK_WINDOW (itf->xfwm4_dialog),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    header_image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_LARGE_TOOLBAR);
    header = xfce_create_header_with_image (header_image, _("Add keybinding theme"));
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), header, FALSE, FALSE, 0);

    hbox = gtk_hbox_new (FALSE, BORDER);
    label = gtk_label_new (_("Enter a name for the theme:"));
    entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
    gtk_widget_show_all (dialog);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(itf->treeview2));
    gtk_tree_selection_get_selected (selection, &model, &iter);
    gtk_tree_model_get (model, &iter, THEME_NAME_COLUMN, &theme_name, -1);
    ti = xfwm4_plugin_find_theme_info_by_name (theme_name, keybinding_theme_list);
    g_free (theme_name);

    if (ti)
    {
        theme_file = g_build_filename (ti->path, G_DIR_SEPARATOR_S, KEY_SUFFIX, G_DIR_SEPARATOR_S, KEYTHEMERC, NULL);
    }
    else
    {
        theme_file = g_build_filename (DATADIR, "themes", "Default", KEY_SUFFIX, KEYTHEMERC, NULL);
    }
    
    while (TRUE)
    {
        gint response = GTK_RESPONSE_CANCEL;

        response = gtk_dialog_run (GTK_DIALOG (dialog));

        if (response == GTK_RESPONSE_OK)
        {
            if (xfwm4_plugin_find_theme_info_by_name (gtk_entry_get_text (GTK_ENTRY (entry)),
                    keybinding_theme_list))
            {
                xfce_err (_("A keybinding theme with the same name already exists"));
                continue;
            }

            if (strlen (gtk_entry_get_text (GTK_ENTRY (entry))) == 0)
            {
                xfce_err (_("You have to provide a name for the keybinding theme"));
                continue;
            }

            /* create theme (copy default) */
            new_theme_path =
                g_strdup_printf ("%s/xfwm4/%s", gtk_entry_get_text (GTK_ENTRY (entry)), KEYTHEMERC);
            new_theme_file =
                xfce_resource_save_location (XFCE_RESOURCE_THEMES, new_theme_path, TRUE);

            new_theme = fopen (new_theme_file, "w+");
            if (!new_theme)
            {
                g_warning ("unable to create the new theme file");
                break;
            }


            default_theme = fopen (theme_file, "r");
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

            /* refresh list */
            while (keybinding_theme_list)
            {
                xfwm4_plugin_theme_info_free ((ThemeInfo *) keybinding_theme_list->data);
                keybinding_theme_list = g_list_next (keybinding_theme_list);
            }
            g_list_free (keybinding_theme_list);

            g_free (xfwm4_plugin_current_key_theme);
            xfwm4_plugin_current_key_theme = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
            keybinding_theme_list = NULL;
            keybinding_theme_list =
                xfwm4_plugin_read_themes (keybinding_theme_list, itf->treeview2,
                itf->scrolledwindow2, KEYBINDING_THEMES, xfwm4_plugin_current_key_theme);
            gtk_widget_set_sensitive (itf->treeview3, TRUE);
            loadtheme_in_treeview (xfwm4_plugin_find_theme_info_by_name (gtk_entry_get_text
                    (GTK_ENTRY (entry)), keybinding_theme_list), itf);
            /* tell it to the mcs manager */
            mcs_manager_set_string (itf->mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL2,
                xfwm4_plugin_current_key_theme);
            mcs_manager_notify (itf->mcs_plugin->manager, CHANNEL2);

            xfwm4_plugin_write_options (itf->mcs_plugin);
        }

        break;
    }

    gtk_widget_destroy (dialog);
    g_free (new_theme_path);
    g_free (new_theme_file);
    g_free (theme_file);
}

gboolean
cb_popup_menu (GtkTreeView * treeview, GdkEventButton * event, gpointer data)
{
    Itf *itf = (Itf *) data;

    /* Right click draws the context menu */
    if ((event->button == 3) && (event->type == GDK_BUTTON_PRESS))
    {
        GtkTreePath *path;
        GdkScreen *screen;

        if (gtk_tree_view_get_path_at_pos (treeview, event->x, event->y, &path, NULL, NULL, NULL))
        {
            GtkTreeSelection *selection;
            GtkTreeModel *model;
            GtkTreeIter iter;

            gchar *theme_name = NULL;
            ThemeInfo *ti;

            selection = gtk_tree_view_get_selection (treeview);
            model = gtk_tree_view_get_model (treeview);
            gtk_tree_model_get_iter (model, &iter, path);

            gtk_tree_model_get (model, &iter, THEME_NAME_COLUMN, &theme_name, -1);

            ti = xfwm4_plugin_find_theme_info_by_name (theme_name, keybinding_theme_list);

            if (ti)
            {
                gtk_tree_selection_unselect_all (selection);
                gtk_tree_selection_select_path (selection, path);

                gtk_widget_set_sensitive (itf->popup_del_menuitem, ti->user_writable);
            }
            else
                g_warning ("Cannot find the keytheme !");

            g_free (theme_name);
        }
        else
        {
            gtk_widget_set_sensitive (itf->popup_del_menuitem, FALSE);
        }


        screen = xfce_gdk_display_locate_monitor_with_pointer (NULL, NULL);
        gtk_menu_set_screen (GTK_MENU (itf->popup_menu),
            screen ? screen : gdk_screen_get_default ());
        gtk_menu_popup (GTK_MENU (itf->popup_menu), NULL, NULL, NULL, NULL, event->button,
            gtk_get_current_event_time ());
        return TRUE;
    }

    return FALSE;
}

/*******************************/
/* Load theme in the treeview  */
/*******************************/
void
loadtheme_in_treeview (ThemeInfo * ti, gpointer data)
{
    const gchar *shortcut_options_list[] = { 
        "close_window_key",
        "maximize_window_key",
        "maximize_vert_key",
        "maximize_horiz_key",
        "hide_window_key",
        "shade_window_key",
        "stick_window_key",
        "cycle_windows_key",
        "move_window_up_key",
        "move_window_down_key",
        "move_window_left_key",
        "move_window_right_key",
        "resize_window_up_key",
        "resize_window_down_key",
        "resize_window_left_key",
        "resize_window_right_key",
        "move_resize_cancel_key",
        "raise_window_key",
        "lower_window_key",
        "fullscreen_key",
        "up_workspace_key",
        "down_workspace_key",
        "left_workspace_key",
        "right_workspace_key",
        "next_workspace_key",
        "prev_workspace_key",
        "add_workspace_key",
        "del_workspace_key",
        "move_window_next_workspace_key",
        "move_window_prev_workspace_key",
        "move_window_up_workspace_key",
        "move_window_down_workspace_key",
        "move_window_left_workspace_key",
        "move_window_right_workspace_key",
        "show_desktop_key",
        "cancel_key",
        "popup_menu_key",
        NULL
    };

    const gchar *shortcut_name_list[] = { 
        N_("Close window"),
        N_("Maximize window"),
        N_("Maximize window vertically"),
        N_("Maximize window horizontally"),
        N_("Hide window"),
        N_("Shade window"),
        N_("Stick window"),
        N_("Cycle windows"),
        N_("Move window up"),
        N_("Move window down"),
        N_("Move window left"),
        N_("Move window right"),
        N_("Resize window up"),
        N_("Resize window down"),
        N_("Resize window left"),
        N_("Resize window right"),
        N_("Cancel move/resize window"),
        N_("Raise window"),
        N_("Lower window"),
        N_("Toggle fullscreen"),
        N_("Upper workspace"),
        N_("Bottom workspace"),
        N_("Left workspace"),
        N_("Right workspace"),
        N_("Next workspace"),
        N_("Previous workspace"),
        N_("Add workspace"),
        N_("Delete workspace"),
        N_("Move window to next workspace"),
        N_("Move window to previous workspace"),
        N_("Move window to upper workspace"),
        N_("Move window to bottom workspace"),
        N_("Move window to left workspace"),
        N_("Move window to right workspace"),
        N_("Show desktop"),
        N_("Cancel window action"),
        N_("Window operations menu"),
        NULL
    };

    Itf *itf = (Itf *) data;

    XfceRc *default_rc = NULL;
    XfceRc *user_rc = NULL;

    GtkTreeModel *model3;
    GtkTreeIter iter;

    gchar *user_theme_file = NULL;
    gchar *default_theme_file = NULL;

    gchar **shortcuts_list = NULL;
    gchar **shortcut = NULL;

    model3 = gtk_tree_view_get_model (GTK_TREE_VIEW (itf->treeview3));
    gtk_list_store_clear (GTK_LIST_STORE (model3));

    user_theme_file = g_build_filename (ti->path, KEY_SUFFIX, KEYTHEMERC, NULL);
    default_theme_file = g_build_filename (DATADIR, "themes", "Default",
        KEY_SUFFIX, KEYTHEMERC, NULL);

    if (g_ascii_strcasecmp (ti->name, "Default") == 0)
    {
        g_free (user_theme_file);
        user_theme_file = g_strdup (default_theme_file);
        gtk_widget_set_sensitive (itf->treeview3, FALSE);
        gtk_widget_set_sensitive (itf->del_button, FALSE);
    }
    else
    {
        gtk_widget_set_sensitive (itf->treeview3, TRUE);
        gtk_widget_set_sensitive (itf->del_button, TRUE);
    }

    default_rc = xfce_rc_simple_open (default_theme_file, TRUE);
    user_rc = xfce_rc_simple_open (user_theme_file, TRUE);

    shortcuts_list = xfce_rc_get_entries (default_rc, NULL);

    g_free (user_theme_file);
    g_free (default_theme_file);

    shortcut = shortcuts_list;

    while (*shortcut)
    {
        const gchar *entry_value;
        const gchar *fallback_value;
        int i;
        gboolean found = FALSE;


        fallback_value = xfce_rc_read_entry (default_rc, *shortcut, "none");
        entry_value = xfce_rc_read_entry (user_rc, *shortcut, fallback_value);

        if (g_str_has_prefix (*shortcut, "shortcut_") || g_str_has_suffix (*shortcut, "_exec"))
        {
            shortcut++;
            continue;
        }

        for (i = 0; !found && shortcut_options_list[i]; i++)
        {
            if (g_ascii_strcasecmp (*shortcut, shortcut_options_list[i]) == 0)
            {
                gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
                gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND,
                    _(shortcut_name_list[i]), COLUMN_SHORTCUT, entry_value, COLUMN_NAME, *shortcut,
                    -1);
                found = TRUE;
            }
        }


        for (i = 0; !found && i <= 12; i++)
        {
            gchar *option;

            option = g_strdup_printf ("workspace_%d_key", i);
            if (g_ascii_strcasecmp (*shortcut, option) == 0)
            {
                gchar *text;

                text = g_strdup_printf (_("Workspace %d"), i);
                gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
                gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, text,
                    COLUMN_SHORTCUT, entry_value, COLUMN_NAME, *shortcut, -1);
                g_free (text);

                found = TRUE;
            }

            g_free (option);
        }

        for (i = 0; !found && i <= 12; i++)
        {
            gchar *option;

            option = g_strdup_printf ("move_window_workspace_%d_key", i);
            if (g_ascii_strcasecmp (*shortcut, option) == 0)
            {
                gchar *text;

                text = g_strdup_printf (_("Move window to workspace %d"), i);
                gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
                gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, text,
                    COLUMN_SHORTCUT, entry_value, COLUMN_NAME, *shortcut, -1);
                g_free (text);

                found = TRUE;
            }

            g_free (option);
        }

        shortcut++;
    }

    g_strfreev (shortcuts_list);

    xfce_rc_close (default_rc);
    xfce_rc_close (user_rc);
}

static gboolean
savetree3_foreach_func (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, gpointer data)
{
    FILE *file = (FILE *) data;
    gchar *line = NULL;
    gchar *name = NULL;
    gchar *shortcut = NULL;

    gtk_tree_model_get (model, iter, COLUMN_NAME, &name, COLUMN_SHORTCUT, &shortcut, -1);

    if (strlen (shortcut) == 0)
    {
        g_free (shortcut);
        shortcut = g_strdup ("none");
    }

    line = g_strdup_printf ("%s=%s\n", name, shortcut);

    fputs (line, file);

    g_free (name);
    g_free (shortcut);
    g_free (line);

    return FALSE;
}

void
savetreeview_in_theme (gchar * theme_file, gpointer data)
{
    Itf *itf = (Itf *) data;

    FILE *file;
    GtkTreeModel *model3;
    gchar *filename = NULL;

    model3 = gtk_tree_view_get_model (GTK_TREE_VIEW (itf->treeview3));

    if (!g_str_has_prefix (theme_file, xfce_get_homedir ()))
    {
        gchar *hometheme_dir = NULL;
        gchar *theme_dir = NULL;
        int nbr_slash = 0;
        int pos = strlen (theme_file) - 1;

        while (nbr_slash < 3 && pos > 0)
        {
            if (theme_file[pos] == '/')
                nbr_slash++;

            pos--;
        }

        theme_dir = g_strndup (&theme_file[pos + 1], strlen (theme_file) - pos - 11);
        hometheme_dir =
            g_build_filename (xfce_get_homedir (), G_DIR_SEPARATOR_S, ".themes", theme_dir, NULL);

        if (!xfce_mkdirhier (hometheme_dir, 0755, NULL))
        {
            xfce_err (_("Cannot open the theme directory !"));
            g_free (hometheme_dir);
            g_free (theme_dir);
            return;
        }

        filename = g_build_filename (hometheme_dir, G_DIR_SEPARATOR_S, "keythemerc", NULL);

        g_free (hometheme_dir);
        g_free (theme_dir);
    }
    else
        filename = g_strdup_printf ("%s.tmp", theme_file);

    file = fopen (filename, "w");

    if (!file)
    {
        perror ("fopen(filename)");
        xfce_err (_("Cannot open %s : \n%s"), filename, strerror (errno));
        g_free (filename);
        return;
    }

    gtk_tree_model_foreach (model3, &savetree3_foreach_func, file);

    fclose (file);

    if (g_str_has_prefix (theme_file, xfce_get_homedir ()))
    {
        if (unlink (theme_file))
        {
            perror ("unlink(theme_file)");
            xfce_err (_("Cannot write in %s : \n%s"), theme_file, strerror (errno));
            g_free (filename);
        }
        if (link (filename, theme_file))
        {
            perror ("link(filename, theme_file)");
            g_free (filename);
        }
        if (unlink (filename))
        {
            perror ("unlink(filename)");
            xfce_err (_("Cannot write in %s : \n%s"), filename, strerror (errno));
            g_free (filename);
        }
    }

    g_free (filename);
}

static gboolean
shortcut_tree_foreach_func (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter,
    gpointer data)
{
    shortcut_tree_foreach_struct *stfs = (shortcut_tree_foreach_struct *) data;
    gchar *shortcut_key = stfs->shortcut;
    gchar *current_shortcut = NULL;

    gtk_tree_model_get (model, iter, COLUMN_SHORTCUT, &current_shortcut, -1);

    if (!gtk_tree_selection_path_is_selected (stfs->selection, path)
        && (strcmp (shortcut_key, current_shortcut) == 0))
    {
        stfs->found = TRUE;
        stfs->path = gtk_tree_path_to_string (path);
    }

    g_free (current_shortcut);
    return stfs->found;
}

static gboolean
is_modifier (guint keycode)
{
    XModifierKeymap *keymap;
    gboolean result = FALSE;
    gint n;

    keymap = XGetModifierMapping (gdk_display);
    for (n = 0; n < keymap->max_keypermod * 8; ++n)
    {
        if (keycode == keymap->modifiermap[n])
        {
            result = TRUE;
            break;
        }
    }

    XFreeModifiermap (keymap);

    return result;
}

static gboolean
cb_compose_shortcut (GtkWidget * widget, GdkEventKey * event, gpointer data)
{
    Itf *itf = (Itf *) data;

    GdkModifierType consumed_modifiers = 0;
    gchar shortcut_string[80] = "";
    ThemeInfo *ti;
    GtkTreeModel *model;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    shortcut_tree_foreach_struct stfs;
    guint keyval;
    guint modifiers;
    gchar *accelerator;
    gchar **shortcuts;
    gchar **current_shortcut;

    if (is_modifier (event->hardware_keycode))
        return TRUE;

    gdk_keymap_translate_keyboard_state (gdk_keymap_get_default (),
        event->hardware_keycode, event->state, event->group, NULL, NULL, NULL, &consumed_modifiers);

    keyval = gdk_keyval_to_lower (event->keyval);

    switch (keyval)
    {
        case GDK_ISO_Left_Tab:
            keyval = GDK_Tab;
            break;
        case GDK_ISO_Level3_Latch:
        case GDK_ISO_Level3_Lock:
        case GDK_ISO_Level3_Shift:
        case GDK_Scroll_Lock:
            return TRUE;
            break;
        default:
            break;
    }

    /* Release keyboard */
    gdk_keyboard_ungrab (GDK_CURRENT_TIME);

    /*
     * gtk_accelerator_get_default_mod_mask () limits the number of modifiers.
     * That's a good thing because we don't want the "fake" modifiers such as
     * Scroll/Num/Caps Lock to be taken into account.
     *
     * Unfortunately, at this time of writing, gdk doesn't know about all of the
     * modifiers, like META, SUPER or HYPER, but just SHIFT, ALT and CONTROL...
     *
     * It means that ppl won't be able to use the "Windows" key as a modifier...
     * Too bad, that's life, you may gently ask the gtk+ maintainers about it.
     *
     * Things may change in the future? See this thread:
     *
     * http://mail.gnome.org/archives/gtk-devel-list/2005-September/msg00024.html
     *
     * (Olivier)
     */

    modifiers = event->state & (~consumed_modifiers | GDK_MODIFIER_MASK);
    modifiers = modifiers & gtk_accelerator_get_default_mod_mask ();
    accelerator = gtk_accelerator_name (keyval, modifiers);
    shortcuts = g_strsplit_set (accelerator, "<>", 0);

    current_shortcut = shortcuts;
    while (*current_shortcut)
    {
        if (strlen (*current_shortcut))
        {
            strcat (shortcut_string, *current_shortcut);
            strcat (shortcut_string, "+");
        }
        current_shortcut++;
    }

    shortcut_string[strlen (shortcut_string) - 1] = '\0';       /* replace the trailing '+' */
    g_free (accelerator);
    g_strfreev (shortcuts);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (itf->treeview3));

    /* Apply change */
    gtk_tree_selection_get_selected (selection, &model, &iter);

    /* check if shorcut already exists */
    stfs.shortcut = shortcut_string;

    stfs.found = FALSE;
    stfs.selection = selection;
    gtk_tree_model_foreach (model, &shortcut_tree_foreach_func, &stfs);

    if (stfs.found)
    {
        GtkTreePath *path_old;
        GtkTreeIter iter_old;
        GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (widget),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_YES_NO,
            _("Shortcut already in use !\nAre you sure you want to use it ?"));

        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_NO)
        {
            gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_OK);
            return TRUE;
        }

        path_old = gtk_tree_path_new_from_string (stfs.path);
        gtk_tree_model_get_iter (model, &iter_old, path_old);
        g_free (stfs.path);
        gtk_tree_path_free (path_old);

        gtk_list_store_set (GTK_LIST_STORE (model), &iter_old, COLUMN_SHORTCUT, "None", -1);
    }

    gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_SHORTCUT, shortcut_string, -1);

    /* save changes */
    ti = xfwm4_plugin_find_theme_info_by_name (xfwm4_plugin_current_key_theme,
        keybinding_theme_list);

    if (ti)
    {
        gchar *theme_file =
            g_build_filename (ti->path, G_DIR_SEPARATOR_S, KEY_SUFFIX, G_DIR_SEPARATOR_S,
            KEYTHEMERC, NULL);
        savetreeview_in_theme (theme_file, itf);

        g_free (theme_file);
    }
    else
        g_warning ("Cannot find the keytheme !");

    /* End operations */
    gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_OK);
    return TRUE;
}

void
cb_activate_treeview3 (GtkWidget * treeview, GtkTreePath * path, GtkTreeViewColumn * column,
    gpointer data)
{
    Itf *itf = (Itf *) data;

    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *shortcut_name = NULL;
    GtkWidget *dialog;
    GtkWidget *hbox;
    GdkPixbuf *icon = NULL;
    GtkWidget *image;
    GtkWidget *label;
    GtkWidget *button;
    gchar *dialog_text = NULL;
    gint response;

    /* Get shortcut name */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_get_selected (selection, &model, &iter);
    gtk_tree_model_get (model, &iter, COLUMN_COMMAND, &shortcut_name, -1);

    dialog_text =
        g_strdup_printf ("<i>%s</i>\n<b>%s</b>", _("Compose shortcut for :"), shortcut_name);

    /* Create dialog */
    dialog =
        gtk_dialog_new_with_buttons (_("Compose shortcut"), NULL, GTK_DIALOG_MODAL, NULL);

    button = xfce_create_mixed_button (GTK_STOCK_CANCEL, _("Cancel"));
    gtk_widget_show (button);
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_CANCEL);

    button = xfce_create_mixed_button (GTK_STOCK_CLEAR, _("No shortcut"));
    gtk_widget_show (button);
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_NO);

    hbox = gtk_hbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
    gtk_widget_show (hbox);

    icon = xfce_themed_icon_load ("xfce4-keys", 48);
    if (icon)
    {
        image = gtk_image_new_from_pixbuf (icon);
        gtk_widget_show (image);
        gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);
    }

    label = gtk_label_new (dialog_text);
    gtk_label_set_markup (GTK_LABEL (label), dialog_text);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);


    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, TRUE, 0);

    /* Center cancel button */
    gtk_button_box_set_layout (GTK_BUTTON_BOX (GTK_DIALOG (dialog)->action_area),
        GTK_BUTTONBOX_SPREAD);

    /* Connect signals */
    g_signal_connect (G_OBJECT (dialog), "key-release-event", G_CALLBACK (cb_compose_shortcut), itf);

    /* Take control on the keyboard */
    if (gdk_keyboard_grab (gtk_widget_get_root_window (dialog), TRUE,
            GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
    {
        g_warning ("Cannot grab the keyboard");
        g_free (dialog_text);
        g_free (shortcut_name);
        return;
    }

    /* Show dialog */
    response = gtk_dialog_run (GTK_DIALOG (dialog));

    if (response == GTK_RESPONSE_NO)
    {
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GtkTreeIter iter;
        ThemeInfo *ti;

        /* No shortcut, set it to none */
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (itf->treeview3));
        gtk_tree_selection_get_selected (selection, &model, &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_SHORTCUT, "None", -1);

        /* save changes */
        ti = xfwm4_plugin_find_theme_info_by_name (xfwm4_plugin_current_key_theme,
            keybinding_theme_list);

        if (ti)
        {
            gchar *theme_file =
                g_build_filename (ti->path, G_DIR_SEPARATOR_S, KEY_SUFFIX, G_DIR_SEPARATOR_S,
                KEYTHEMERC, NULL);
            savetreeview_in_theme (theme_file, itf);
            g_free (theme_file);
        }
        else
            g_warning ("Cannot find the keytheme !");
    }

    /* Release keyboard if not yet done */
    gdk_keyboard_ungrab (GDK_CURRENT_TIME);

    /* 
       Tell it to the mcs manager, set the channel to raw mode
       so that the client gets notified even if the key theme
       name has not changed
     */
    mcs_manager_set_raw_channel (itf->mcs_plugin->manager, CHANNEL2, TRUE);
    mcs_manager_set_string (itf->mcs_plugin->manager, "Xfwm/KeyThemeName", CHANNEL2,
        xfwm4_plugin_current_key_theme);
    mcs_manager_notify (itf->mcs_plugin->manager, CHANNEL2);
    mcs_manager_set_raw_channel (itf->mcs_plugin->manager, CHANNEL2, FALSE);
    xfwm4_plugin_write_options (itf->mcs_plugin);

    gtk_widget_destroy (dialog);
    g_free (dialog_text);
    g_free (shortcut_name);
}
