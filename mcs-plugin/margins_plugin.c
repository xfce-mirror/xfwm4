/*  xfce4
 *  
 *  Copyright (C) 2002 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#include <libxfce4mcs/mcs-common.h>
#include <libxfce4mcs/mcs-manager.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <xfce-mcs-manager/manager-plugin.h>
#include <libxfcegui4/libxfcegui4.h>
#include "margins-icon.h"
#include "monitor-icon.h"

#define RCFILE "margins.xml"
#define PLUGIN_NAME "xfwm_margins"
#define MARGINS_CHANNEL "margins"

#define DEFAULT_ICON_SIZE 48
#define BORDER 5

static McsManager *manager = NULL;

static int margins[4];

static char *options[] = {
                             "Xfwm/LeftMargin",
                             "Xfwm/RightMargin",
                             "Xfwm/TopMargin",
                             "Xfwm/BottomMargin"
                         };

static void run_dialog(McsPlugin * mcs_plugin);
static void create_margins_channel(McsPlugin * mcs_plugin);
static void set_margin(int side, int margin);

static void create_channel(McsManager * manager, const char *channel, const char *rcfile)
{
    char *homefile, *sysfile;

    homefile = xfce_get_userfile("settings", rcfile, NULL);
    sysfile = g_build_filename(DATADIR, "xfce4", "settings", rcfile, NULL);

    if(g_file_test(homefile, G_FILE_TEST_EXISTS))
    {
        mcs_manager_add_channel_from_file(manager, channel, homefile);
    }
    else if(g_file_test(sysfile, G_FILE_TEST_EXISTS))
    {
        mcs_manager_add_channel_from_file(manager, channel, sysfile);
    }
    else
    {
        mcs_manager_add_channel(manager, channel);
    }

    g_free(homefile);
    g_free(sysfile);
}

static gboolean save_channel(McsManager * manager, const char *channel, const char *rcfile)
{
    char *homefile;
    gboolean result;

    homefile = xfce_get_userfile("settings", rcfile, NULL);
    result = mcs_manager_save_channel_to_file(manager, channel, homefile);

    g_free(homefile);

    return result;
}

McsPluginInitResult mcs_plugin_init(McsPlugin * mcs_plugin)
{
#ifdef ENABLE_NLS
    /* 
       This is required for UTF-8 at least - Please don't remove it
       And it needs to be done here for the label to be properly
       localized....
     */
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    manager = mcs_plugin->manager;

    mcs_plugin->plugin_name = g_strdup(PLUGIN_NAME);
    mcs_plugin->caption = g_strdup(_("Workspace Margins"));
    mcs_plugin->run_dialog = run_dialog;
    mcs_plugin->icon = inline_icon_at_size(margins_icon_data, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);

    create_margins_channel(mcs_plugin);

    return (MCS_PLUGIN_INIT_OK);
}

/* create margins channel */
static void create_margins_channel(McsPlugin * mcs_plugin)
{
    McsSetting *setting;
    int i, n;

    create_channel(mcs_plugin->manager, MARGINS_CHANNEL, RCFILE);

    for(i = 0; i < 4; i++)
    {
        margins[i] = 0;

        setting = mcs_manager_setting_lookup(mcs_plugin->manager, options[i], MARGINS_CHANNEL);

        n = (setting) ? setting->data.v_int : 0;

        set_margin(i, n);
    }
}

/* write channel to file */
static void save_margins_channel(void)
{
    save_channel(manager, MARGINS_CHANNEL, RCFILE);
}

/* setting a margin */
static void set_margin(int side, int margin)
{
    mcs_manager_set_int(manager, options[side], MARGINS_CHANNEL, margin);

    margins[side] = margin;

    mcs_manager_notify(manager, MARGINS_CHANNEL);

    save_margins_channel();
}

/* useful widgets */
static void margin_changed(GtkSpinButton * spin, gpointer p)
{
    int i = GPOINTER_TO_INT(p);
    int n = gtk_spin_button_get_value_as_int(spin);

    set_margin(i, n);
}

static void run_dialog(McsPlugin * mcs_plugin)
{
    static GtkWidget *dialog = NULL;
    GtkWidget *mainvbox, *frame, *header, *vbox, *hbox, *label, *spin, *image;
    GtkSizeGroup *sg;
    GdkPixbuf *icon;
    GdkPixbuf *monitor;
    int wmax, hmax, i;

    if(dialog)
    {
        gtk_window_present(GTK_WINDOW(dialog));
        return;
    }

    wmax = gdk_screen_width() / 4;
    hmax = gdk_screen_height() / 4;

    dialog = gtk_dialog_new_with_buttons(_("Adjust workspace margins"), NULL, GTK_DIALOG_NO_SEPARATOR, GTK_STOCK_CLOSE, GTK_RESPONSE_OK, NULL);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    g_signal_connect(dialog, "delete-event", G_CALLBACK(gtk_widget_destroy), NULL);

    g_object_add_weak_pointer(G_OBJECT(dialog), (gpointer) & dialog);

    mainvbox = GTK_DIALOG(dialog)->vbox;

    icon = inline_icon_at_size(margins_icon_data, 32, 32);
    gtk_window_set_icon(GTK_WINDOW(dialog), icon);

    header = create_header(icon, _("Workspace Margins"));
    gtk_widget_show(header);
    gtk_box_pack_start(GTK_BOX(mainvbox), header, TRUE, TRUE, 0);
    g_object_unref(icon);

    label = gtk_label_new(_("Margins are areas on the edges of the screen where no window will be placed"));
    gtk_label_set_line_wrap (GTK_LABEL(label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_misc_set_padding(GTK_MISC(label), BORDER, 4);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(mainvbox), label, FALSE, FALSE, 0);

    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(mainvbox), vbox, TRUE, TRUE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

    frame = gtk_frame_new(_("Workspace Margins"));
    gtk_widget_show(frame);
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 5);

    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), BORDER);
    gtk_widget_show(hbox);
    gtk_container_add(GTK_CONTAINER(frame), hbox);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);

    monitor = inline_icon_at_size(monitor_icon_data, -1, -1);
    image = gtk_image_new_from_pixbuf(monitor);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);

    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER);
    gtk_widget_show(vbox);
    gtk_box_pack_end(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    /* left */
    i = 0;
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new(_("Left :"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_size_group_add_widget(sg, label);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);

    spin = gtk_spin_button_new_with_range(0, wmax, 1);
    gtk_widget_show(spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, TRUE, 0);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), margins[i]);
    g_signal_connect(G_OBJECT(spin), "value-changed", G_CALLBACK(margin_changed), GINT_TO_POINTER(i));

    /* right */
    i++;
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new(_("Right :"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_size_group_add_widget(sg, label);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);

    spin = gtk_spin_button_new_with_range(0, wmax, 1);
    gtk_widget_show(spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, TRUE, 0);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), margins[i]);
    g_signal_connect(G_OBJECT(spin), "value-changed", G_CALLBACK(margin_changed), GINT_TO_POINTER(i));

    /* top */
    i++;
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new(_("Top :"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_size_group_add_widget(sg, label);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);

    spin = gtk_spin_button_new_with_range(0, hmax, 1);
    gtk_widget_show(spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, TRUE, 0);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), margins[i]);
    g_signal_connect(G_OBJECT(spin), "value-changed", G_CALLBACK(margin_changed), GINT_TO_POINTER(i));

    /* bottom */
    i++;
    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new(_("Bottom :"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_size_group_add_widget(sg, label);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0);

    spin = gtk_spin_button_new_with_range(0, hmax, 1);
    gtk_widget_show(spin);
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, TRUE, 0);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), margins[i]);
    g_signal_connect(G_OBJECT(spin), "value-changed", G_CALLBACK(margin_changed), GINT_TO_POINTER(i));

    gtk_widget_show(dialog);
}
