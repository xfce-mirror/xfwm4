/*  
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
 
        Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef GDK_MULTIHEAD_SAFE
#undef GDK_MULTIHEAD_SAFE
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

#include <libxfce4util/libxfce4util.h> 
#include <xfce-mcs-manager/manager-plugin.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4mcs/mcs-common.h>
#include <libxfce4mcs/mcs-manager.h>

#include "plugin.h"
#include "margins.h"
#include "workspaces.h"

static McsManager *manager = NULL;

static void run_dialog (McsPlugin * mcs_plugin);

/* exported functions */
void
create_channel (McsManager * manager, const char *channel, const char *rcfile)
{
    char *file, *path;

    path = g_build_filename ("xfce4", "mcs_settings", rcfile, NULL);
    file = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, path);
    g_free (path);

    if (!file)
        file = xfce_get_userfile ("settings", rcfile, NULL);

    if (g_file_test (file, G_FILE_TEST_EXISTS))
    {
        mcs_manager_add_channel_from_file (manager, channel, file);
    }
    else
    {
        mcs_manager_add_channel (manager, channel);
    }

    g_free (file);
}

gboolean
save_channel (McsManager * manager, const char *channel, const char *rcfile)
{
    char *homefile, *path;
    gboolean result;

    path = g_build_filename ("xfce4", "mcs_settings", rcfile, NULL);
    homefile = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, path, TRUE);

    result = mcs_manager_save_channel_to_file (manager, channel, homefile);

    g_free (path);
    g_free (homefile);

    return result;
}

/* plugin init */
McsPluginInitResult
mcs_plugin_init (McsPlugin * mcs_plugin)
{
    /* 
       This is required for UTF-8 at least - Please don't remove it
       And it needs to be done here for the label to be properly
       localized....
     */
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    manager = mcs_plugin->manager;

    mcs_plugin->plugin_name = g_strdup (PLUGIN_NAME);
    mcs_plugin->caption = g_strdup (_("Workspaces and Margins"));
    mcs_plugin->run_dialog = run_dialog;
    mcs_plugin->icon = xfce_themed_icon_load ("xfce4-workspaces", 48);

    create_workspaces_channel (mcs_plugin);
    create_margins_channel (mcs_plugin);

    return (MCS_PLUGIN_INIT_OK);
}

/* settings dialog */
static void
run_dialog (McsPlugin * mcs_plugin)
{
    static GtkWidget *dialog = NULL;
    GtkWidget *mainvbox, *notebook, *header, *vbox; 

    if (dialog)
    {
        gtk_window_present (GTK_WINDOW (dialog));
        return;
    }

    dialog =
        gtk_dialog_new_with_buttons (_("Workspaces"), NULL,
                                     GTK_DIALOG_NO_SEPARATOR, GTK_STOCK_CLOSE,
                                     GTK_RESPONSE_OK, NULL);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy),
                      NULL);
    g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_widget_destroy),
                      NULL);

    g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer) & dialog);

    mainvbox = GTK_DIALOG (dialog)->vbox;

    gtk_window_set_icon (GTK_WINDOW (dialog), mcs_plugin->icon);

    header = xfce_create_header (mcs_plugin->icon, _("Workspaces and Margins"));
    gtk_widget_show (header);
    gtk_box_pack_start (GTK_BOX (mainvbox), header, TRUE, TRUE, 0);

    notebook = gtk_notebook_new();
    gtk_container_set_border_width (GTK_CONTAINER (notebook), BORDER);
    gtk_widget_show(notebook);
    gtk_box_pack_start(GTK_BOX (mainvbox), notebook, TRUE, TRUE, 0);
    
    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_widget_show (vbox);
    add_workspaces_page(GTK_BOX(vbox));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, 
	    		     gtk_label_new(_("Workspaces")));

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_widget_show (vbox);
    add_margins_page(GTK_BOX(vbox));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, 
	    		     gtk_label_new(_("Margins")));
    
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_widget_show (dialog);
}
