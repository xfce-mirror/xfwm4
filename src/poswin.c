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
          based on a patch from Joshua Blanton <jblanton@irg.cs.ohiou.edu>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h> 
#include <libxfcegui4/libxfcegui4.h>

#include "client.h"
#include "frame.h"
#include "poswin.h"

Poswin *
poswinCreate (GdkScreen *gscr)
{
    Poswin *poswin;
    GtkWidget *frame;

    poswin = g_new (Poswin, 1);

    poswin->window = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_screen (GTK_WINDOW (poswin->window), gscr);
    gtk_container_set_border_width (GTK_CONTAINER (poswin->window), 0);
    gtk_window_set_resizable (GTK_WINDOW (poswin->window), TRUE);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_container_add (GTK_CONTAINER (poswin->window), frame);
    gtk_widget_show (frame);

    poswin->label = gtk_label_new ("");
    gtk_misc_set_alignment (GTK_MISC (poswin->label), 0.5, 0.5);
    gtk_misc_set_padding (GTK_MISC (poswin->label), 3, 3);
    gtk_widget_show (poswin->label);
    gtk_container_add(GTK_CONTAINER(frame), poswin->label);
    gtk_widget_show_all (frame);

    return poswin;
}

void
poswinSetPosition (Poswin * poswin, Client *c)
{
    /* 32 is enough for (NNNNNxNNNNN) @ (-NNNNN,-NNNNN) */
    gchar label[32];
    gint x, y, px, py, pw, ph;
    gint wsize, hsize;
    
    g_return_if_fail (poswin != NULL);
    g_return_if_fail (c != NULL);
    g_return_if_fail (c->size->width_inc != 0);
    g_return_if_fail (c->size->height_inc != 0);
    
    x = frameX (c);
    y = frameY (c);
    wsize = (c->width - c->size->base_width) / c->size->width_inc;
    hsize = (c->height - c->size->base_height) / c->size->height_inc;

#ifdef SHOW_POSITION
    g_snprintf (label, 32, "(%ix%i) @ (%i,%i)", wsize, hsize, x, y);
#else
    g_snprintf (label, 32, "(%ix%i)", wsize, hsize);
#endif
    gtk_label_set_text (GTK_LABEL (poswin->label), label);
    gtk_widget_queue_draw (poswin->window);
    gtk_window_get_size (GTK_WINDOW (poswin->window), &pw, &ph);
    px = x + (frameWidth (c) - pw) / 2;
    py = y + (frameHeight (c) - ph) / 2;
    if (GTK_WIDGET_REALIZED (poswin->window))
    { 
        gdk_window_move_resize (poswin->window->window, px, py, pw, ph);
    }
    else
    {
        gtk_window_move   (GTK_WINDOW (poswin->window), px, py);
    }
}

void
poswinDestroy (Poswin * poswin)
{
    g_return_if_fail (poswin != NULL);

    gtk_widget_destroy (poswin->window);
}

void
poswinShow (Poswin * poswin)
{
    g_return_if_fail (poswin != NULL);
    
    gtk_widget_show_now (poswin->window);
}

void
poswinHide(Poswin * poswin)
{
    g_return_if_fail (poswin != NULL);
    
    gtk_widget_hide (poswin->window);
}
