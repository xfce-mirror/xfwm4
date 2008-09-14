/*
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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
 */


#include <config.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <dbus/dbus-glib.h>
#include <libwnck/libwnck.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <xfconf/xfconf.h>
#include "xfwm4-workspace-dialog_glade.h"

#define WORKSPACES_CHANNEL         "xfwm4"

#define WORKSPACE_NAMES_PROP       "/general/workspace_names"

static gboolean version = FALSE;


enum
{
    COL_NUMBER = 0,
    COL_NAME,
    N_COLS,
};

static void
workspace_names_update_xfconf(gint workspace,
                              const gchar *new_name)
{
    WnckScreen *screen = wnck_screen_get_default();
    XfconfChannel *channel;
    gchar **names;
    gboolean do_update_xfconf = TRUE;

    channel = xfconf_channel_new(WORKSPACES_CHANNEL);
    names = xfconf_channel_get_string_list(channel, WORKSPACE_NAMES_PROP);

    if(!names) {
        /* the property doesn't exist; let's build one from scratch */
        gint i, n_workspaces = wnck_screen_get_workspace_count(screen);

        names = g_new(gchar *, n_workspaces + 1);
        for(i = 0; i < n_workspaces; ++i) {
            if(G_LIKELY(i != workspace))
                names[i] = g_strdup_printf(_("Workspace %d"), i + 1);
            else
                names[i] = g_strdup(new_name);
        }
        names[n_workspaces] = NULL;
    } else {
        gint i, prop_len = g_strv_length(names);
        gint n_workspaces = wnck_screen_get_workspace_count(screen);

        if(prop_len < n_workspaces) {
            /* the property exists, but it's smaller than the current
             * actual number of workspaces */
            names = g_realloc(names, sizeof(gchar *) * (n_workspaces + 1));
            for(i = prop_len; i < n_workspaces; ++i) {
                if(i != workspace)
                    names[i] = g_strdup_printf(_("Workspace %d"), i + 1);
                else
                    names[i] = g_strdup(new_name);
            }
            names[n_workspaces] = NULL;
        } else {
            /* here we may have a |names| array longer than the actual
             * number of workspaces, but that's fine.  the user might
             * want to re-add a workspace or whatever, and may appreciate
             * that we remember the old name. */
            if(strcmp(names[workspace], new_name)) {
                g_free(names[workspace]);
                names[workspace] = g_strdup(new_name);
            } else {
                /* nothing's actually changed, so don't update the xfconf
                 * property.  this saves us some trouble later. */
                do_update_xfconf = FALSE;
            }
        }
    }

    if(do_update_xfconf)
        xfconf_channel_set_string_list(channel, WORKSPACE_NAMES_PROP, names);

    g_strfreev(names);
    g_object_unref(G_OBJECT(channel));
}

static void
treeview_ws_names_row_activated(GtkTreeView *treeview,
                                GtkTreePath *path,
                                GtkTreeViewColumn *column,
                                gpointer user_data)
{
    GtkWidget *dialog = user_data, *entry;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreeIter iter;
    gint ws_num = 1;
    gchar *subtitle, *old_name = NULL;

    if(!gtk_tree_model_get_iter(model, &iter, path))
        return;

    gtk_tree_model_get(model, &iter,
                       COL_NUMBER, &ws_num,
                       -1);

    subtitle = g_strdup_printf(_("Change the name of workspace %d"), ws_num);
    xfce_titled_dialog_set_subtitle(XFCE_TITLED_DIALOG(dialog), subtitle);
    g_free(subtitle);

    entry = g_object_get_data(G_OBJECT(dialog), "name-entry");
    gtk_tree_model_get(model, &iter,
                       COL_NAME, &old_name,
                       -1);
    gtk_entry_set_text(GTK_ENTRY(entry), old_name);
    gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);

    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *new_name;

        new_name = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
        if(!*new_name) {
            g_free(new_name);
            new_name = g_strdup_printf(_("Workspace %d"), ws_num);
        }

        /* only update it if the name's actually different */
        if(strcmp(old_name, new_name)) {
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               COL_NAME, &new_name,
                               -1);
            workspace_names_update_xfconf(ws_num - 1, new_name);
        }

        g_free(new_name);
    }

    g_free(old_name);
    gtk_widget_hide(dialog);
}


static void
xfconf_workspace_names_changed(XfconfChannel *channel,
                               const gchar *property,
                               const GValue *value,
                               gpointer user_data)
{
    GtkTreeView *treeview = user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    WnckScreen *screen = wnck_screen_get_default();
    gint i, n_workspaces;
    GPtrArray *names;
    GtkTreePath *path;
    GtkTreeIter iter;

    if(G_VALUE_TYPE(value) !=  dbus_g_type_get_collection("GPtrArray",
                                                          G_TYPE_VALUE))
    {
        g_warning("(workspace names) Expected boxed GPtrArray property, got %s",
                  G_VALUE_TYPE_NAME(value));
        return;
    }

    names = g_value_get_boxed(value);
    if(!names)
        return;

    wnck_screen_force_update(screen);
    n_workspaces = wnck_screen_get_workspace_count(screen);
    for(i = 0; i < n_workspaces && i < names->len; ++i) {
        GValue *val = g_ptr_array_index(names, i);
        const gchar *new_name;

        if(!G_VALUE_HOLDS_STRING(val)) {
            g_warning("(workspace names) Expected string but got %s for item %d",
                      G_VALUE_TYPE_NAME(val), i);
            continue;
        }

        new_name = g_value_get_string(val);

        path = gtk_tree_path_new_from_indices(i, -1);
        if(gtk_tree_model_get_iter(model, &iter, path)) {
            gchar *old_name = NULL;

            gtk_tree_model_get(model, &iter,
                               COL_NAME, &old_name,
                               -1);
            /* only update the names that have actually changed */
            if(strcmp(old_name, new_name)) {
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   COL_NAME, new_name,
                                   -1);
            }
            g_free(old_name);
        } else {
            /* must be a new workspace */
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               COL_NUMBER, i + 1,
                               COL_NAME, new_name,
                               -1);
        }

        gtk_tree_path_free(path);
    }

    /* if workspaces got destroyed, we need to remove them from the treeview */
    path = gtk_tree_path_new_from_indices(n_workspaces, -1);
    while(gtk_tree_model_get_iter(model, &iter, path))
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    gtk_tree_path_free(path);
}

static void
workspace_dialog_setup_names_treeview(GladeXML *gxml,
                                      XfconfChannel *channel)
{
    GtkWidget *treeview, *dialog;
    GtkListStore *ls;
    GtkCellRenderer *render;
    GtkTreeViewColumn *col;
    WnckScreen *screen;
    gint n_workspaces, i;
    GtkTreeIter iter;

    dialog = glade_xml_get_widget(gxml, "change_name_dialog");
    g_object_set_data(G_OBJECT(dialog), "name-entry",
                      glade_xml_get_widget(gxml, "entry_name"));
    g_signal_connect(G_OBJECT(dialog), "delete-event",
                     G_CALLBACK(gtk_true), NULL);

    treeview = glade_xml_get_widget(gxml, "treeview_ws_names");
    g_signal_connect(G_OBJECT(treeview), "row-activated",
                     G_CALLBACK(treeview_ws_names_row_activated), dialog);

    ls = gtk_list_store_new(N_COLS, G_TYPE_INT, G_TYPE_STRING);

    render = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("", render,
                                                   "text", COL_NUMBER,
                                                   NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

    render = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(render),
                 "ellipsize", PANGO_ELLIPSIZE_END,
                 "ellipsize-set", TRUE,
                 NULL);
    col = gtk_tree_view_column_new_with_attributes(_("Workspace Name"),
                                                   render,
                                                   "text", COL_NAME,
                                                   NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

    screen = wnck_screen_get_default();
    wnck_screen_force_update(screen);

    n_workspaces = wnck_screen_get_workspace_count(screen);
    for(i = 0; i < n_workspaces; ++i) {
        WnckWorkspace *space = wnck_screen_get_workspace(screen, i);
        const char *name = wnck_workspace_get_name(space);

        gtk_list_store_append(ls, &iter);
        gtk_list_store_set(ls, &iter,
                           COL_NUMBER, i + 1,
                           COL_NAME, name,
                           -1);
    }

    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(ls));

    g_signal_connect(G_OBJECT(channel),
                     "property-changed::" WORKSPACE_NAMES_PROP,
                     G_CALLBACK(xfconf_workspace_names_changed), treeview);
}

static GtkWidget *
workspace_dialog_new_from_xml (GladeXML *gxml,
                               XfconfChannel *channel)
{
    GtkWidget *dialog;
    GtkWidget *vbox;

    GtkWidget *workspace_count_spinbutton = glade_xml_get_widget (gxml, "workspace_count_spinbutton");

    /* Bind easy properties */
    xfconf_g_property_bind (channel, 
                            "/general/workspace_count",
                            G_TYPE_INT,
                            (GObject *)workspace_count_spinbutton, "value");

    workspace_dialog_setup_names_treeview(gxml, channel);

    vbox = glade_xml_get_widget (gxml, "main-vbox");
    dialog = glade_xml_get_widget (gxml, "main-dialog");

    gtk_widget_show_all(vbox);

    return dialog;
}


static GOptionEntry entries[] =
{
    {    "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
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
    XfconfChannel *channel;
    GError *cli_error = NULL;

    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

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

    if(!xfconf_init (&cli_error)) {
        g_critical ("Failed to contact xfconfd: %s", cli_error->message);
        g_error_free (cli_error);
        return 1;
    }

    gxml = glade_xml_new_from_buffer (workspace_dialog_glade,
                                      workspace_dialog_glade_length,
                                      NULL, NULL);

    channel = xfconf_channel_new(WORKSPACES_CHANNEL);

    dialog = workspace_dialog_new_from_xml (gxml, channel);

    while(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_HELP) {
        /* FIXME: launch help */
    }

    g_object_unref(G_OBJECT(channel));
    xfconf_shutdown();

    return 0;
}
