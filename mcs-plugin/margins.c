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
 
        Copyright (C) 2002-2005 Jasper Huijsmans (huysmans@users.sourceforge.net)
                                Olivier Fourdan (fourdan@xfce.org)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef GDK_MULTIHEAD_SAFE
#undef GDK_MULTIHEAD_SAFE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h> 
#include <libxfce4mcs/mcs-common.h>
#include <libxfce4mcs/mcs-manager.h>
#include <libxfcegui4/libxfcegui4.h>
#include <xfce-mcs-manager/manager-plugin.h>

#include "plugin.h"
#include "monitor-icon.h"
#include "margins.h"

static void save_margins_channel (void);

static McsManager *mcs_manager;

static int margins[4];

static char *options[] = {
    "Xfwm/LeftMargin",
    "Xfwm/RightMargin",
    "Xfwm/TopMargin",
    "Xfwm/BottomMargin"
};

static void set_margin (int side, int margin);

/* create margins channel */
void
create_margins_channel (McsPlugin * mcs_plugin)
{
    McsSetting *setting;
    int i, n;

    mcs_manager = mcs_plugin->manager;
    
    ws_create_channel (mcs_plugin->manager, CHANNEL2, RCFILE2);

    for (i = 0; i < 4; i++)
    {
        margins[i] = 0;

        setting =
            mcs_manager_setting_lookup (mcs_plugin->manager, options[i],
                                        CHANNEL2);

        n = (setting) ? setting->data.v_int : 0;

        set_margin (i, n);
    }

    save_margins_channel ();
}

/* write channel to file */
static void
save_margins_channel (void)
{
    ws_save_channel (mcs_manager, CHANNEL2, RCFILE2);
}

/* setting a margin */
static void
set_margin (int side, int margin)
{
    mcs_manager_set_int (mcs_manager, options[side], CHANNEL2, margin);

    margins[side] = margin;

    mcs_manager_notify (mcs_manager, CHANNEL2);

    save_margins_channel ();
}

static void
margin_changed (GtkSpinButton * spin, gpointer p)
{
    int i = GPOINTER_TO_INT (p);
    int n = gtk_spin_button_get_value_as_int (spin);

    set_margin (i, n);
}

void
add_margins_page (GtkBox *box)
{
    GtkWidget *hbox, *frame, *label, *vbox, *spin, *image;
    GtkSizeGroup *sg;
    GdkPixbuf *monitor;
    int wmax, hmax, i;
    
    wmax = gdk_screen_width () / 4;
    hmax = gdk_screen_height () / 4;

    frame = xfce_framebox_new (_("Workspace Margins"), FALSE);
    gtk_widget_show (frame);
    gtk_box_pack_start (GTK_BOX (box), frame, TRUE, TRUE, BORDER);
    
    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_widget_show (vbox);
    xfce_framebox_add (XFCE_FRAMEBOX (frame), vbox);

    label =
        gtk_label_new (_("Margins are areas on the edges of the screen where no window will be placed"));
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_misc_set_padding (GTK_MISC (label), BORDER, 0);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, BORDER);
    
    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, BORDER);

    monitor = xfce_inline_icon_at_size (monitor_icon_data, -1, -1);
    image = gtk_image_new_from_pixbuf (monitor);
    gtk_widget_show (image);
    gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
    g_object_unref(monitor);

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER);
    gtk_widget_show (vbox);
    gtk_box_pack_end (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

    sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    /* left */
    i = 0;
    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new (_("Left :"));
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_size_group_add_widget (sg, label);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

    spin = gtk_spin_button_new_with_range (0, wmax, 1);
    gtk_widget_show (spin);
    gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, TRUE, 0);

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), margins[i]);
    g_signal_connect (G_OBJECT (spin), "value-changed",
                      G_CALLBACK (margin_changed), GINT_TO_POINTER (i));

    /* right */
    i++;
    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new (_("Right :"));
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_size_group_add_widget (sg, label);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

    spin = gtk_spin_button_new_with_range (0, wmax, 1);
    gtk_widget_show (spin);
    gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, TRUE, 0);

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), margins[i]);
    g_signal_connect (G_OBJECT (spin), "value-changed",
                      G_CALLBACK (margin_changed), GINT_TO_POINTER (i));

    /* top */
    i++;
    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new (_("Top :"));
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_size_group_add_widget (sg, label);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

    spin = gtk_spin_button_new_with_range (0, hmax, 1);
    gtk_widget_show (spin);
    gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, TRUE, 0);

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), margins[i]);
    g_signal_connect (G_OBJECT (spin), "value-changed",
                      G_CALLBACK (margin_changed), GINT_TO_POINTER (i));

    /* bottom */
    i++;
    hbox = gtk_hbox_new (FALSE, BORDER);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new (_("Bottom :"));
    gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
    gtk_size_group_add_widget (sg, label);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

    spin = gtk_spin_button_new_with_range (0, hmax, 1);
    gtk_widget_show (spin);
    gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, TRUE, 0);

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), margins[i]);
    g_signal_connect (G_OBJECT (spin), "value-changed",
                      G_CALLBACK (margin_changed), GINT_TO_POINTER (i));
}


