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

        xfwm4    - (c) 2003 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfcegui4/libxfcegui4.h>
#include "inline-tabwin-icon.h"
#include "tabwin.h"
#include "debug.h"

#define _(x) x

Tabwin *tabwinCreate(gchar *label)
{
    static GdkPixbuf *icon = NULL;
    Tabwin *tabwin;
    GtkWidget *frame;
    GtkWidget *vbox;
    GtkWidget *header;

    tabwin = g_new(Tabwin, 1);

    if (!icon)
    {
        icon = inline_icon_at_size(tabwin_icon_data, 32, 32);
	g_object_ref (G_OBJECT(icon));
    }

    tabwin->window = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_container_set_border_width (GTK_CONTAINER (tabwin->window), 0);
    gtk_window_set_position (GTK_WINDOW (tabwin->window), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_default_size (GTK_WINDOW (tabwin->window), 320, -1);
    gtk_widget_set_size_request (tabwin->window, 320, -1);
    gtk_window_set_resizable (GTK_WINDOW (tabwin->window), FALSE);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_OUT);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_container_add (GTK_CONTAINER (tabwin->window), frame);
    gtk_widget_show(frame);

    vbox = gtk_vbox_new(FALSE, 5);
    gtk_widget_show(vbox);
    gtk_container_set_border_width(GTK_CONTAINER (vbox), 5);
    gtk_container_add (GTK_CONTAINER (frame), vbox);

    header = create_header(icon, _("Switch to ..."));
    gtk_widget_show(header);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, TRUE, 0);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
    gtk_widget_show(frame);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(vbox);
    gtk_container_set_border_width(GTK_CONTAINER (vbox), 5);
    gtk_container_add (GTK_CONTAINER (frame), vbox);

    tabwin->label = gtk_label_new("");
    if (label)
    {
        tabwinSetLabel(tabwin, label);
    }
    gtk_misc_set_alignment(GTK_MISC(tabwin->label), 0.5, 0.5);
    gtk_widget_show(tabwin->label);
    gtk_box_pack_start(GTK_BOX(vbox), tabwin->label, FALSE, TRUE, 0);
    gtk_widget_show(tabwin->window);
    
    return tabwin;    
}

void tabwinSetLabel(Tabwin *tabwin, gchar *label)
{
    g_return_if_fail (tabwin != NULL);
    
    if (label)
    {
        gtk_label_set_text(GTK_LABEL(tabwin->label), label);
    }
    else
    {
        gtk_label_set_text(GTK_LABEL(tabwin->label), "");
    }
}

void tabwinDestroy(Tabwin *tabwin)
{
    g_return_if_fail (tabwin != NULL);
    
    gtk_widget_destroy (tabwin->window);    
}

