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
 */


#include <config.h>
#include <string.h>

#include <glib.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <xfconf/xfconf.h>
#include "xfwm4-workspace-dialog_glade.h"

static gboolean version = FALSE;

static GtkWidget *
workspace_dialog_new_from_xml (GladeXML *gxml)
{
    GtkWidget *dialog;
    GtkWidget *vbox;
    XfconfChannel *xfwm4_channel = xfconf_channel_new("xfwm4");

    GtkWidget *workspace_count_spinbutton = glade_xml_get_widget (gxml, "workspace_count_spinbutton");

    /* Bind easy properties */
    xfconf_g_property_bind (xfwm4_channel, 
                            "/general/workspace_count",
                            G_TYPE_INT,
                            (GObject *)workspace_count_spinbutton, "value"); 

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

    dialog = workspace_dialog_new_from_xml (gxml);

    gtk_dialog_run(GTK_DIALOG(dialog));

    xfconf_shutdown();

    return 0;
}
