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
 
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h> 
#include <libxfcegui4/libxfcegui4.h>
#include "inline-tabwin-icon.h"
#include "tabwin.h"

Tabwin *
tabwinCreate (GdkPixbuf *icon, const gchar * class, const gchar * label)
{
    static GdkPixbuf *logo = NULL;
    Tabwin *tabwin;
    GtkWidget *frame;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *header;

    tabwin = g_new (Tabwin, 1);

    if (!logo)
    {
        logo = xfce_inline_icon_at_size (tabwin_icon_data, 32, 32);
        g_object_ref (G_OBJECT (logo));
    }
    tabwin->window = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_container_set_border_width (GTK_CONTAINER (tabwin->window), 0);
    gtk_window_set_position (GTK_WINDOW (tabwin->window),
        GTK_WIN_POS_CENTER_ALWAYS);
    gtk_window_set_default_size (GTK_WINDOW (tabwin->window), 400, -1);
    gtk_widget_set_size_request (tabwin->window, 400, -1);
    gtk_window_set_resizable (GTK_WINDOW (tabwin->window), FALSE);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_container_add (GTK_CONTAINER (tabwin->window), frame);
    gtk_widget_show (frame);

    vbox = gtk_vbox_new (FALSE, 5);
    gtk_widget_show (vbox);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
    gtk_container_add (GTK_CONTAINER (frame), vbox);

    header = xfce_create_header (logo, _("Switch to ..."));
    gtk_widget_show (header);
    gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, TRUE, 0);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
    gtk_widget_show (frame);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
    gtk_widget_show (hbox);
    gtk_container_add (GTK_CONTAINER (frame), hbox);

    tabwin->image = gtk_image_new ();
    gtk_widget_show (tabwin->image);
    gtk_box_pack_start (GTK_BOX (hbox), tabwin->image, FALSE, TRUE, 0);
    
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 2);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 0);
    
    tabwin->class = gtk_label_new ("");
    gtk_label_set_use_markup (GTK_LABEL (tabwin->class), TRUE);
    gtk_misc_set_alignment (GTK_MISC (tabwin->class), 0.0, 0.5);
    gtk_widget_show (tabwin->class);
    gtk_box_pack_start (GTK_BOX (vbox), tabwin->class, FALSE, TRUE, 0);

    tabwin->label = gtk_label_new ("");
    gtk_label_set_use_markup (GTK_LABEL (tabwin->label), FALSE);
    gtk_misc_set_alignment (GTK_MISC (tabwin->label), 0.0, 0.5);
    gtk_widget_show (tabwin->label);
    gtk_box_pack_start (GTK_BOX (vbox), tabwin->label, FALSE, TRUE, 0);

    tabwinSetLabel (tabwin, icon, class, label);

    gtk_widget_show (tabwin->window);

    return tabwin;
}

/* Efficiency is definitely *not* the goal here! */
static gchar *
pretty_string (const gchar *s)
{
    gchar *canonical;
    gchar *markup;
    
    if (s)
    {
        canonical = g_strdup (s);
        g_strcanon (canonical, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", ' ');    
        g_strstrip (canonical);
        *canonical = g_ascii_toupper (*canonical);
    }
    else
    {
        canonical = g_strdup(_("Unknown application!"));
    }
    markup = g_strdup_printf ("<b>%s</b>", canonical);
    free (canonical);
    return markup;
}

void
tabwinSetLabel (Tabwin * tabwin, GdkPixbuf *icon, const gchar * class, const gchar * label)
{
    g_return_if_fail (tabwin != NULL);
    gchar* message;
    
    message = pretty_string (class);
    gtk_label_set_use_markup (GTK_LABEL (tabwin->class), TRUE);
    gtk_label_set_markup (GTK_LABEL (tabwin->class), message);
    g_free (message);
    gtk_label_set_text (GTK_LABEL (tabwin->label), label);

    if (icon)
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (tabwin->image), icon);
    }
    else
    {
        gtk_image_set_from_stock (GTK_IMAGE (tabwin->image), "gtk-missing-image", 
                                  GTK_ICON_SIZE_DIALOG);
    }

    gtk_widget_queue_draw (tabwin->window);
}

void
tabwinDestroy (Tabwin * tabwin)
{
    g_return_if_fail (tabwin != NULL);

    gtk_widget_destroy (tabwin->window);
}
