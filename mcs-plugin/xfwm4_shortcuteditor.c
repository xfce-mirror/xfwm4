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

#include <libxfcegui4/libxfcegui4.h>
#include <xfce-mcs-manager/manager-plugin.h>


#include "xfwm4_plugin.h"
#include "xfwm4_shortcuteditor.h"
#include "keys-icon.h"

void
loadtheme_in_treeview (gchar * theme_file, gpointer data)
{
    Itf *itf = (Itf *) data;

    GtkTreeModel *model3, *model4;
    GtkTreeIter iter;
    FILE *fp;
    gchar *lvalue, *rvalue;
    gchar buf[80];
    gboolean key_found = FALSE;

    model3 = gtk_tree_view_get_model (GTK_TREE_VIEW (itf->treeview3));
    model4 = gtk_tree_view_get_model (GTK_TREE_VIEW (itf->treeview4));
    gtk_list_store_clear (GTK_LIST_STORE (model3));
    gtk_list_store_clear (GTK_LIST_STORE (model4));

    fp = fopen (theme_file, "r");

    if (!fp)
        return;

    while (fgets (buf, sizeof (buf), fp))
    {
        lvalue = strtok (buf, "=");
        rvalue = strtok (NULL, "\n");

        if (g_ascii_strcasecmp (lvalue, "close_window_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Close window"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "maximize_window_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Maximize window"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "maximize_vert_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Maximize window vertically"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "maximize_horiz_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Maximize window horizontally"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "hide_window_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Hide window"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "shade_window_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Shade window"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "stick_window_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Stick window"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "cycle_windows_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Cycle windows"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_up_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window up"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_down_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window down"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_left_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window left"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_right_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window right"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "resize_window_up_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Resize window up"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "resize_window_down_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Resize window down"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "resize_window_left_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Resize window left"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "resize_window_right_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Resize window right"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "raise_window_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Raise window"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "lower_window_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Lower window"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "fullscreen_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Toggle fullscreen"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "next_workspace_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Next workspace"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "prev_workspace_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Previous workspace"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "add_workspace_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Add workspace"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "del_workspace_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Delete workspace"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "workspace_1_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Workspace 1"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "workspace_2_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Workspace 2"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "workspace_3_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Workspace 3"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "workspace_4_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Workspace 4"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "workspace_5_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Workspace 5"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "workspace_6_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Workspace 6"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "workspace_7_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Workspace 7"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "workspace_8_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Workspace 8"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "workspace_9_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Workspace 9"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_next_workspace_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window to next workspace"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_prev_workspace_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window to previous workspace"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_workspace_1_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window to workspace 1"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_workspace_2_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window to workspace 2"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_workspace_3_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window to workspace 3"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_workspace_4_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window to workspace 4"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_workspace_5_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window to workspace 5"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_workspace_6_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window to workspace 6"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_workspace_7_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window to workspace 7"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_workspace_8_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window to workspace 8"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "move_window_workspace_9_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model3), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model3), &iter, COLUMN_COMMAND, _("Move window to workspace 9"), COLUMN_SHORTCUT, rvalue, -1);
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_1_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model4), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, "none", COLUMN_SHORTCUT, rvalue, -1);
            key_found = TRUE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_1_exec") == 0 && key_found)
        {
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, rvalue, -1);
            key_found = FALSE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_2_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model4), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, "none", COLUMN_SHORTCUT, rvalue, -1);
            key_found = TRUE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_2_exec") == 0 && key_found)
        {
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, rvalue, -1);
            key_found = FALSE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_3_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model4), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, "none", COLUMN_SHORTCUT, rvalue, -1);
            key_found = TRUE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_3_exec") == 0 && key_found)
        {
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, rvalue, -1);
            key_found = FALSE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_4_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model4), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, "none", COLUMN_SHORTCUT, rvalue, -1);
            key_found = TRUE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_4_exec") == 0 && key_found)
        {
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, rvalue, -1);
            key_found = FALSE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_5_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model4), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, "none", COLUMN_SHORTCUT, rvalue, -1);
            key_found = TRUE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_5_exec") == 0 && key_found)
        {
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, rvalue, -1);
            key_found = FALSE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_6_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model4), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, "none", COLUMN_SHORTCUT, rvalue, -1);
            key_found = TRUE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_6_exec") == 0 && key_found)
        {
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, rvalue, -1);
            key_found = FALSE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_7_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model4), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, "none", COLUMN_SHORTCUT, rvalue, -1);
            key_found = TRUE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_7_exec") == 0 && key_found)
        {
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, rvalue, -1);
            key_found = FALSE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_8_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model4), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, "none", COLUMN_SHORTCUT, rvalue, -1);
            key_found = TRUE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_8_exec") == 0 && key_found)
        {
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, rvalue, -1);
            key_found = FALSE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_9_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model4), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, "none", COLUMN_SHORTCUT, rvalue, -1);
            key_found = TRUE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_9_exec") == 0 && key_found)
        {
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, rvalue, -1);
            key_found = FALSE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_10_key") == 0)
        {
            gtk_list_store_append (GTK_LIST_STORE (model4), &iter);
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, "none", COLUMN_SHORTCUT, rvalue, -1);
            key_found = TRUE;
        }
        else if (g_ascii_strcasecmp (lvalue, "shortcut_10_exec") == 0 && key_found)
        {
            gtk_list_store_set (GTK_LIST_STORE (model4), &iter, COLUMN_COMMAND, rvalue, -1);
            key_found = FALSE;
        }
    }

    fclose (fp);
}

static gboolean
savetree4_foreach_func (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, gpointer data)
{
    FILE *file = (FILE *) data;
    gchar *line = NULL;
    gchar *command = NULL;
    gchar *shortcut = NULL;

    static guint index = 0;

    if (index == 0 || index == 10)
        index = 1;
    else
        index++;

    gtk_tree_model_get (model, iter, COLUMN_COMMAND, &command, COLUMN_SHORTCUT, &shortcut, -1);

    if (strcmp (command, "none") == 0)
        line = g_strdup_printf ("shortcut_%d_key=%s\n", index, shortcut);
    else
        line = g_strdup_printf ("shortcut_%d_key=%s\nshortcut_%d_exec=%s\n", index, shortcut, index, command);

    fputs (line, file);

    g_free (shortcut);
    g_free (command);
    g_free (line);

    return FALSE;
}

static gboolean
savetree3_foreach_func (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, gpointer data)
{
    FILE *file = (FILE *) data;
    gchar *line = NULL;
    gchar *command = NULL;
    gchar xfwm4_command[50] = "";
    gchar *shortcut = NULL;

    gtk_tree_model_get (model, iter, COLUMN_COMMAND, &command, COLUMN_SHORTCUT, &shortcut, -1);

    if (strlen (shortcut) == 0)
    {
        g_free (shortcut);
        shortcut = g_strdup ("none");
    }

    if (strcmp (command, _("Close window")) == 0)
    {
        strcpy (xfwm4_command, "close_window_key");
    }
    else if (strcmp (command, _("Maximize window")) == 0)
    {
        strcpy (xfwm4_command, "maximize_window_key");
    }
    else if (strcmp (command, _("Maximize window vertically")) == 0)
    {
        strcpy (xfwm4_command, "maximize_vert_key");
    }
    else if (strcmp (command, _("Maximize window horizontally")) == 0)
    {
        strcpy (xfwm4_command, "maximize_horiz_key");
    }
    else if (strcmp (command, _("Hide window")) == 0)
    {
        strcpy (xfwm4_command, "hide_window_key");
    }
    else if (strcmp (command, _("Shade window")) == 0)
    {
        strcpy (xfwm4_command, "shade_window_key");
    }
    else if (strcmp (command, _("Stick window")) == 0)
    {
        strcpy (xfwm4_command, "stick_window_key");
    }
    else if (strcmp (command, _("Cycle windows")) == 0)
    {
        strcpy (xfwm4_command, "cycle_windows_key");
    }
    else if (strcmp (command, _("Move window up")) == 0)
    {
        strcpy (xfwm4_command, "move_window_up_key");
    }
    else if (strcmp (command, _("Move window down")) == 0)
    {
        strcpy (xfwm4_command, "move_window_down_key");
    }
    else if (strcmp (command, _("Move window left")) == 0)
    {
        strcpy (xfwm4_command, "move_window_left_key");
    }
    else if (strcmp (command, _("Move window right")) == 0)
    {
        strcpy (xfwm4_command, "move_window_right_key");
    }
    else if (strcmp (command, _("Resize window up")) == 0)
    {
        strcpy (xfwm4_command, "resize_window_up_key");
    }
    else if (strcmp (command, _("Resize window down")) == 0)
    {
        strcpy (xfwm4_command, "resize_window_down_key");
    }
    else if (strcmp (command, _("Resize window left")) == 0)
    {
        strcpy (xfwm4_command, "resize_window_left_key");
    }
    else if (strcmp (command, _("Resize window right")) == 0)
    {
        strcpy (xfwm4_command, "resize_window_right_key");
    }
    else if (strcmp (command, _("Raise window")) == 0)
    {
        strcpy (xfwm4_command, "raise_window_key");
    }
    else if (strcmp (command, _("Lower window")) == 0)
    {
        strcpy (xfwm4_command, "lower_window_key");
    }
    else if (strcmp (command, _("Toggle fullscreen")) == 0)
    {
        strcpy (xfwm4_command, "fullscreen_key");
    }
    else if (strcmp (command, _("Next workspace")) == 0)
    {
        strcpy (xfwm4_command, "next_workspace_key");
    }
    else if (strcmp (command, _("Previous workspace")) == 0)
    {
        strcpy (xfwm4_command, "prev_workspace_key");
    }
    else if (strcmp (command, _("Add workspace")) == 0)
    {
        strcpy (xfwm4_command, "add_workspace_key");
    }
    else if (strcmp (command, _("Delete workspace")) == 0)
    {
        strcpy (xfwm4_command, "del_workspace_key");
    }
    else if (strcmp (command, _("Workspace 1")) == 0)
    {
        strcpy (xfwm4_command, "workspace_1_key");
    }
    else if (strcmp (command, _("Workspace 2")) == 0)
    {
        strcpy (xfwm4_command, "workspace_2_key");
    }
    else if (strcmp (command, _("Workspace 3")) == 0)
    {
        strcpy (xfwm4_command, "workspace_3_key");
    }
    else if (strcmp (command, _("Workspace 4")) == 0)
    {
        strcpy (xfwm4_command, "workspace_4_key");
    }
    else if (strcmp (command, _("Workspace 5")) == 0)
    {
        strcpy (xfwm4_command, "workspace_5_key");
    }
    else if (strcmp (command, _("Workspace 6")) == 0)
    {
        strcpy (xfwm4_command, "workspace_6_key");
    }
    else if (strcmp (command, _("Workspace 7")) == 0)
    {
        strcpy (xfwm4_command, "workspace_7_key");
    }
    else if (strcmp (command, _("Workspace 8")) == 0)
    {
        strcpy (xfwm4_command, "workspace_8_key");
    }
    else if (strcmp (command, _("Workspace 9")) == 0)
    {
        strcpy (xfwm4_command, "workspace_9_key");
    }
    else if (strcmp (command, _("Move window to next workspace")) == 0)
    {
        strcpy (xfwm4_command, "move_window_next_workspace_key");
    }
    else if (strcmp (command, _("Move window to previous workspace")) == 0)
    {
        strcpy (xfwm4_command, "move_window_prev_workspace_key");
    }
    else if (strcmp (command, _("Move window to workspace 1")) == 0)
    {
        strcpy (xfwm4_command, "move_window_workspace_1_key");
    }
    else if (strcmp (command, _("Move window to workspace 2")) == 0)
    {
        strcpy (xfwm4_command, "move_window_workspace_2_key");
    }
    else if (strcmp (command, _("Move window to workspace 3")) == 0)
    {
        strcpy (xfwm4_command, "move_window_workspace_3_key");
    }
    else if (strcmp (command, _("Move window to workspace 4")) == 0)
    {
        strcpy (xfwm4_command, "move_window_workspace_4_key");
    }
    else if (strcmp (command, _("Move window to workspace 5")) == 0)
    {
        strcpy (xfwm4_command, "move_window_workspace_5_key");
    }
    else if (strcmp (command, _("Move window to workspace 6")) == 0)
    {
        strcpy (xfwm4_command, "move_window_workspace_6_key");
    }
    else if (strcmp (command, _("Move window to workspace 7")) == 0)
    {
        strcpy (xfwm4_command, "move_window_workspace_7_key");
    }
    else if (strcmp (command, _("Move window to workspace 8")) == 0)
    {
        strcpy (xfwm4_command, "move_window_workspace_8_key");
    }
    else if (strcmp (command, _("Move window to workspace 9")) == 0)
    {
        strcpy (xfwm4_command, "move_window_workspace_9_key");
    }

    line = g_strdup_printf ("%s=%s\n", xfwm4_command, shortcut);

    fputs (line, file);

    g_free (command);
    g_free (shortcut);
    g_free (line);

    return FALSE;
}

void
savetreeview_in_theme (gchar * theme_file, gpointer data)
{
    Itf *itf = (Itf *) data;

    FILE *file;
    GtkTreeModel *model3, *model4;
    gchar *filename = NULL;

    model3 = gtk_tree_view_get_model (GTK_TREE_VIEW (itf->treeview3));
    model4 = gtk_tree_view_get_model (GTK_TREE_VIEW (itf->treeview4));

    if (g_str_has_prefix (theme_file, "/usr"))
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
        hometheme_dir = g_build_filename (xfce_get_homedir (), G_DIR_SEPARATOR_S, ".themes", theme_dir, NULL);

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
    gtk_tree_model_foreach (model4, &savetree4_foreach_func, file);

    fclose (file);

    if (!g_str_has_prefix (theme_file, "/usr"))
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
shortcut_tree_foreach_func (GtkTreeModel * model, GtkTreePath * path, GtkTreeIter * iter, gpointer data)
{
    shortcut_tree_foreach_struct *stfs = (shortcut_tree_foreach_struct *) data;
    gchar *shortcut_key = stfs->key;
    gchar *current_shortcut = NULL;

    gboolean current_modifiers[3] = { FALSE, FALSE, FALSE };
    gboolean key_found = FALSE;

    gtk_tree_model_get (model, iter, COLUMN_SHORTCUT, &current_shortcut, -1);

    if (g_strrstr (current_shortcut, "Control") != NULL)
        current_modifiers[0] = TRUE;
    if (g_strrstr (current_shortcut, "Shift") != NULL)
        current_modifiers[1] = TRUE;
    if (g_strrstr (current_shortcut, "Alt") != NULL)
        current_modifiers[2] = TRUE;
    if (g_strrstr (current_shortcut, shortcut_key) != NULL)
        key_found = TRUE;

    if ((stfs->modifier_control == current_modifiers[0]) &&
        (stfs->modifier_shift == current_modifiers[1]) && (stfs->modifier_alt == current_modifiers[2]) && key_found)
    {
        stfs->found = TRUE;
        stfs->path = gtk_tree_path_to_string (path);
    }

    g_free (current_shortcut);
    return stfs->found;
}

static gboolean
command_exists (const gchar * command)
{
    gchar *cmd_buf = NULL;
    gchar *cmd_tok = NULL;
    gboolean result = FALSE;

    cmd_buf = g_strdup (command);
    cmd_tok = strtok (cmd_buf, " ");

    if (g_find_program_in_path (cmd_buf) != NULL)
        result = TRUE;

    g_free (cmd_buf);

    return result;
}

static void
cb_browse_command (GtkWidget *widget, GtkEntry *entry_command)
{
    GtkWidget *filesel_dialog;

    filesel_dialog = xfce_file_chooser_new (_("Select command"), NULL,
					    XFCE_FILE_CHOOSER_ACTION_OPEN,
					    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					    GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

    if(gtk_dialog_run (GTK_DIALOG (filesel_dialog)) == GTK_RESPONSE_ACCEPT)
    {
        gchar *filename;

	filename = xfce_file_chooser_get_filename (XFCE_FILE_CHOOSER (filesel_dialog));
        gtk_entry_set_text (entry_command,
			    filename);

	g_free (filename);
    }

    gtk_widget_destroy (GTK_WIDGET (filesel_dialog));
}

static gboolean
cb_compose_dialog_key_release (GtkWidget * widget, GdkEventKey * event, gpointer data)
{
    Itf *itf = (Itf *) data;

    gchar shortcut_string[80] = "";
    GtkTreeModel *model3, *model4;
    GtkTreeModel *model, *model_old;
    GtkTreeIter iter3, iter4;
    GtkTreeIter iter;
    GtkTreeSelection *selection3, *selection4;
    shortcut_tree_foreach_struct stfs;
    ThemeInfo *ti;

    selection3 = gtk_tree_view_get_selection (GTK_TREE_VIEW (itf->treeview3));
    selection4 = gtk_tree_view_get_selection (GTK_TREE_VIEW (itf->treeview4));

    stfs.modifier_control = FALSE;
    stfs.modifier_shift = FALSE;
    stfs.modifier_alt = FALSE;

    if (event->state & GDK_CONTROL_MASK)
    {
        stfs.modifier_control = TRUE;
        strcat (shortcut_string, "Control+");
    }

    if (event->state & GDK_SHIFT_MASK)
    {
        stfs.modifier_shift = TRUE;
        strcat (shortcut_string, "Shift+");
    }

    if (event->state & GDK_MOD1_MASK)
    {
        stfs.modifier_alt = TRUE;
        strcat (shortcut_string, "Alt+");
    }

    strcat (shortcut_string, gdk_keyval_name (event->keyval));

    /* Release keyboard */
    gdk_keyboard_ungrab (GDK_CURRENT_TIME);

    /* Apply change */
    gtk_tree_selection_get_selected (selection3, &model3, &iter3);
    gtk_tree_selection_get_selected (selection4, &model4, &iter4);

    if (gtk_widget_is_focus (itf->treeview3))
    {
        iter = iter3;
        model = model3;
    }
    else
    {
        iter = iter4;
        model = model4;
    }

    /* check if shorcut already exists */
    stfs.key = gdk_keyval_name (event->keyval);

    stfs.found = FALSE;
    gtk_tree_model_foreach (model3, &shortcut_tree_foreach_func, &stfs);
    model_old = model3;

    if (!stfs.found)
    {
        gtk_tree_model_foreach (model4, &shortcut_tree_foreach_func, &stfs);
        model_old = model4;
    }

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
        gtk_tree_model_get_iter (model_old, &iter_old, path_old);
        g_free (stfs.path);
        gtk_tree_path_free (path_old);

        if (model_old == model4)
            gtk_list_store_set (GTK_LIST_STORE (model_old), &iter_old, COLUMN_SHORTCUT, "none", -1);
        else
            gtk_list_store_set (GTK_LIST_STORE (model_old), &iter_old, COLUMN_SHORTCUT, "", -1);

    }

    gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_SHORTCUT, shortcut_string, -1);

    /* save changes */
    printf("%s\n", current_key_theme);
    ti = find_theme_info_by_name (current_key_theme, keybinding_theme_list);

    if (ti)
    {
        gchar *theme_file = g_build_filename (ti->path, G_DIR_SEPARATOR_S, KEY_SUFFIX, G_DIR_SEPARATOR_S, KEYTHEMERC, NULL);
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
cb_activate_treeview3 (GtkWidget * treeview, GtkTreePath * path, GtkTreeViewColumn * column, gpointer data)
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
    gchar *dialog_text = NULL;

    /* Get shortcut name */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_get_selected (selection, &model, &iter);
    gtk_tree_model_get (model, &iter, COLUMN_COMMAND, &shortcut_name, -1);

    dialog_text = g_strdup_printf ("<i>%s</i>\n<b>%s</b>", _("Compose shortcut for :"), shortcut_name);

    /* Create dialog */
    dialog = gtk_dialog_new_with_buttons (_("Compose shortcut"), NULL, GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

    hbox = gtk_hbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (hbox),10);
    gtk_widget_show (hbox);

    icon = xfce_inline_icon_at_size (keys_icon_data, 48, 48);
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
    gtk_button_box_set_layout (GTK_BUTTON_BOX (GTK_DIALOG (dialog)->action_area), GTK_BUTTONBOX_SPREAD);

    /* Connect signals */
    g_signal_connect (G_OBJECT (dialog), "key-release-event", G_CALLBACK (cb_compose_dialog_key_release), itf);

    /* Take control on the keyboard */
    if (gdk_keyboard_grab (gtk_widget_get_root_window (label), TRUE, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
    {
        g_warning (_("Cannot grab the keyboard"));
        g_free (dialog_text);
        g_free (shortcut_name);
        return;
    }

    /* Show dialog */
    gtk_dialog_run (GTK_DIALOG (dialog));

    /* Release keyboard if not yet done */
    gdk_keyboard_ungrab (GDK_CURRENT_TIME);

    gtk_widget_destroy (dialog);
    g_free (dialog_text);
    g_free (shortcut_name);
}

void
cb_activate_treeview4 (GtkWidget * treeview, GtkTreePath * path, GtkTreeViewColumn * column, gpointer data)
{
    Itf *itf = (Itf *) data;
    gboolean need_shortcut = TRUE;

    if (column == gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), 0))
    {
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GtkTreeIter iter;

        gchar *shortcut = NULL;
        gchar *command = NULL;

        GtkWidget *dialog;
        GtkWidget *label;
        GtkWidget *entry;
        GtkWidget *button;
        GtkWidget *hbox;
        GtkWidget *hbox_entry;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        gtk_tree_selection_get_selected (selection, &model, &iter);

        /* Get shortcut name */
        gtk_tree_model_get (model, &iter, COLUMN_SHORTCUT, &shortcut, -1);

        if (strcmp (shortcut, "none") != 0)
            need_shortcut = FALSE;

        /* Get the command */
        gtk_tree_model_get (model, &iter, COLUMN_COMMAND, &command, -1);

        /* Create dialog */
        dialog = gtk_dialog_new_with_buttons (_("Choose command"), NULL,
            GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        label = gtk_label_new (_("Command :"));
        entry = gtk_entry_new_with_max_length (255);
        hbox_entry = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox_entry), entry, FALSE, FALSE, 0);
        button = gtk_button_new_with_label ("...");
	g_signal_connect ( (gpointer) button, "clicked", G_CALLBACK (cb_browse_command), entry);
        gtk_box_pack_start (GTK_BOX (hbox_entry), button, FALSE, FALSE, 0);

        hbox = gtk_hbox_new (FALSE, 10);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), hbox_entry, FALSE, TRUE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);

        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, TRUE, 0);
        gtk_widget_show_all (dialog);

        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
        {
            if (strcmp (gtk_entry_get_text (GTK_ENTRY (entry)), "none") == 0)
            {
                gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_COMMAND, "none", COLUMN_SHORTCUT, "none", -1);

                need_shortcut = FALSE;
            }
            else if (command_exists (gtk_entry_get_text (GTK_ENTRY (entry))))
	    {
                gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_COMMAND, gtk_entry_get_text (GTK_ENTRY (entry)), -1);
	    }
            else
            {
                GtkWidget *dialog_warning = gtk_message_dialog_new (GTK_WINDOW (dialog),
                    GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_WARNING,
                    GTK_BUTTONS_OK,
                    _("The command doesn't exist or the file is not executable !"));
                need_shortcut = FALSE;

                gtk_dialog_run (GTK_DIALOG (dialog_warning));
                gtk_widget_destroy (dialog_warning);
            }

        }
	else
  	    need_shortcut = FALSE;

        if (!need_shortcut)
        {
            ThemeInfo *ti;

            /* save changes */
            ti = find_theme_info_by_name (current_key_theme, keybinding_theme_list);

            if (ti)
            {
                gchar *theme_file = g_build_filename (ti->path, G_DIR_SEPARATOR_S, KEY_SUFFIX, G_DIR_SEPARATOR_S, KEYTHEMERC, NULL);
                savetreeview_in_theme (theme_file, itf);

                g_free (theme_file);
            }
            else
                g_warning ("Cannot find the keytheme !");
        }


        gtk_widget_destroy (dialog);
        g_free (shortcut);
        g_free (command);
    }

    if (need_shortcut)
    {
        GtkTreeSelection *selection;
        GtkTreeModel *model;
        GtkTreeIter iter;
        gchar *shortcut_name = NULL;
        gchar *shortcut = NULL;
        GtkWidget *dialog;
	GtkWidget *hbox;
	GdkPixbuf *icon = NULL;
	GtkWidget *image;
        GtkWidget *label;
        gchar *dialog_text = NULL;


        /* Get shortcut name */
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        gtk_tree_selection_get_selected (selection, &model, &iter);
        gtk_tree_model_get (model, &iter, COLUMN_COMMAND, &shortcut_name, -1);
        gtk_tree_model_get (model, &iter, COLUMN_SHORTCUT, &shortcut, -1);

        dialog_text = g_strdup_printf ("<i>%s</i>\n<b>%s</b>", _("Compose shortcut for command :"), shortcut_name);

        /* Create dialog */
        dialog = gtk_dialog_new_with_buttons (_("Compose shortcut"), NULL, GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

	hbox = gtk_hbox_new (FALSE, 10);
	gtk_container_set_border_width (GTK_CONTAINER (hbox),10);
	gtk_widget_show (hbox);

	icon = xfce_inline_icon_at_size (keys_icon_data, 48, 48);
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
        gtk_button_box_set_layout (GTK_BUTTON_BOX (GTK_DIALOG (dialog)->action_area), GTK_BUTTONBOX_SPREAD);

        /* Connect signals */
        g_signal_connect (G_OBJECT (dialog), "key-release-event", G_CALLBACK (cb_compose_dialog_key_release), itf);

        /* Take control on the keyboard */
        if (gdk_keyboard_grab (gtk_widget_get_root_window (label), TRUE, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
        {
            g_warning (_("Cannot grab the keyboard"));
            g_free (dialog_text);
            g_free (shortcut_name);
            return;
        }

        /* Show dialog */
        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_CANCEL)
        {
            if (strcmp (shortcut, "none") == 0)
                gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_COMMAND, "none", -1);
        }

        /* Release keyboard if not yet done */
        gdk_keyboard_ungrab (GDK_CURRENT_TIME);

        gtk_widget_destroy (dialog);
        g_free (dialog_text);
        g_free (shortcut_name);
        g_free (shortcut);
    }
}

