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

        xfwm4    - (c) 2002-2006 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN_ICON_SIZE
#define WIN_ICON_SIZE 48
#endif

#ifndef WIN_ICON_BORDER
#define WIN_ICON_BORDER 5
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include "inline-tabwin-icon.h"
#include "icons.h"
#include "focus.h"
#include "tabwin.h"


static GdkColor *
get_color (GtkWidget * win, GtkStateType state_type)
{
    GtkStyle *style;

    g_return_val_if_fail (win != NULL, NULL);
    g_return_val_if_fail (GTK_IS_WIDGET (win), NULL);
    g_return_val_if_fail (GTK_WIDGET_REALIZED (win), NULL);

    style = gtk_rc_get_style (win);
    if (!style)
    {
        style = gtk_widget_get_style (win);
    }
    if (!style)
    {
        style = gtk_widget_get_default_style ();
    }

    return (&style->bg[state_type]);
}

static gboolean
paint_selected (GtkWidget * w, GdkEventExpose * event, gpointer data)
{
    gtk_paint_box (w->style, w->window,
        GTK_STATE_SELECTED,
        GTK_SHADOW_IN,
        NULL, w, "box",
        w->allocation.x - WIN_ICON_BORDER,
        w->allocation.y - WIN_ICON_BORDER,
        w->allocation.width + 2 * WIN_ICON_BORDER,
        w->allocation.height + 2 * WIN_ICON_BORDER);
    return FALSE;
}

/* Efficiency is definitely *not* the goal here! */
static gchar *
pretty_string (const gchar * s)
{
    gchar *canonical;

    if (s)
    {
        canonical = g_strdup (s);
        g_strcanon (canonical, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", ' ');
        g_strstrip (canonical);
        *canonical = g_ascii_toupper (*canonical);
    }
    else
    {
        canonical = g_strdup ("...");
    }

    return canonical;
}

static void
tabwinSetLabel (Tabwin * t, gchar * class, gchar * label, int workspace)
{
    gchar *markup;
    gchar *message;

    message = pretty_string (class);
    gtk_label_set_use_markup (GTK_LABEL (t->class), TRUE);
    markup = g_strconcat ("<span size=\"larger\" weight=\"bold\">", message, "</span>", NULL);
    g_free (message);

    gtk_label_set_markup (GTK_LABEL (t->class), markup);
    g_free (markup);

    if (t->display_workspace)
    {
        message = g_strdup_printf ("[%i] - %s", workspace + 1, label);
    }
    else
    {
        message = g_strdup_printf ("%s", label);
    }
    gtk_label_set_text (GTK_LABEL (t->label), message);
    g_free (message);
}

static void
tabwinSetSelected (Tabwin * t, GtkWidget * w)
{
    Client *c;

    if (t->selected_callback)
    {
        g_signal_handler_disconnect (t->current->data, t->selected_callback);
    }
    t->selected_callback = g_signal_connect (G_OBJECT (w), "expose-event", G_CALLBACK (paint_selected), NULL);
    c = g_object_get_data (G_OBJECT (w), "client-ptr-val");

    tabwinSetLabel (t, c->class.res_class, c->name, c->win_workspace);
}

static GtkWidget *
createWindowIcon (Client * c)
{
    GdkPixbuf *icon_pixbuf;
    GtkWidget *icon;

    icon_pixbuf = getAppIcon (c->screen_info->display_info, c->window, WIN_ICON_SIZE, WIN_ICON_SIZE);
    icon = gtk_image_new ();
    g_object_set_data (G_OBJECT (icon), "client-ptr-val", c);

    if (icon_pixbuf)
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (icon), icon_pixbuf);
        g_object_unref(icon_pixbuf);
    }
    else
    {
        gtk_image_set_from_stock (GTK_IMAGE (icon), "gtk-missing-image", GTK_ICON_SIZE_DIALOG);
    }

    return icon;
}

static GtkWidget *
createWindowlist (GdkScreen * scr, Client * current, Client * new, unsigned int cycle_range, Tabwin * t)
{
    ScreenInfo *screen_info;
    Client *c2;
    GdkRectangle monitor_sz;
    GtkWidget *windowlist, *icon;
    GList *next;
    unsigned int grid_cols;
    unsigned int n_clients;
    unsigned int grid_rows;
    int i, packpos;
    int msx, msy;
    gint monitor;

    g_return_val_if_fail (current != NULL, NULL);

    i = 0;
    packpos = 0;
    c2 = NULL;
    next = NULL;

    /* calculate the wrapping */
    screen_info = current->screen_info;
    n_clients = screen_info->client_count;

    g_return_val_if_fail (n_clients > 0, NULL);

    getMouseXY (screen_info, screen_info->xroot, &msx, &msy);
    monitor = find_monitor_at_point (scr, msx, msy);
    gdk_screen_get_monitor_geometry (scr, monitor, &monitor_sz);

    /* add the width of the border on each side */
    grid_cols = (monitor_sz.width / (WIN_ICON_SIZE + 2 * WIN_ICON_BORDER)) * 0.75;
    grid_rows = n_clients / grid_cols + 1;
    windowlist = gtk_table_new (grid_rows, grid_cols, FALSE);

    t->grid_cols = grid_cols;
    t->grid_rows = grid_rows;
    /* pack the client icons */
    for (c2 = current, i = 0; c2 && i < n_clients; i++, c2 = c2->next)
    {
        if (!clientSelectMask (c2, cycle_range, WINDOW_REGULAR_FOCUSABLE))
            continue;
        icon = createWindowIcon (c2);
        gtk_table_attach (GTK_TABLE (windowlist), GTK_WIDGET (icon),
            packpos % grid_cols, packpos % grid_cols + 1,
            packpos / grid_cols, packpos / grid_cols + 1,
            GTK_FILL, GTK_FILL, 7, 7);
        packpos++;
        t->head = g_list_append (t->head, icon);
        if (c2 == new)
        {
            next = g_list_last (t->head);
        }
    }
    if (next)
    {
        tabwinSetSelected (t, next->data);
    }
    t->current = next;
    return windowlist;
}

Tabwin *
tabwinCreate (GdkScreen * scr, Client * current, Client * new, unsigned int cycle_range, gboolean display_workspace)
{
    Tabwin *tabwin;
    GtkWidget *frame;
    GtkWidget *colorbox1, *colorbox2;
    GtkWidget *vbox;
    GtkWidget *windowlist;
    GdkColor *color;

    tabwin = g_new0 (Tabwin, 1);

    tabwin->window = gtk_window_new (GTK_WINDOW_POPUP);

    tabwin->display_workspace = display_workspace;
    gtk_window_set_screen (GTK_WINDOW (tabwin->window), scr);
    gtk_widget_set_name (GTK_WIDGET (tabwin->window), "xfwm4-tabwin");
    gtk_widget_realize (GTK_WIDGET (tabwin->window));
    gtk_container_set_border_width (GTK_CONTAINER (tabwin->window), 0);
    gtk_window_set_position (GTK_WINDOW (tabwin->window), GTK_WIN_POS_CENTER_ALWAYS);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_container_add (GTK_CONTAINER (tabwin->window), frame);

    colorbox1 = gtk_event_box_new ();
    gtk_container_add (GTK_CONTAINER (frame), colorbox1);

    colorbox2 = gtk_event_box_new ();
    gtk_container_set_border_width (GTK_CONTAINER (colorbox2), 3);
    gtk_container_add (GTK_CONTAINER (colorbox1), colorbox2);

    vbox = gtk_vbox_new (FALSE, 5);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
    gtk_container_add (GTK_CONTAINER (colorbox2), vbox);

    tabwin->class = gtk_label_new (NULL);
    gtk_label_set_use_markup (GTK_LABEL (tabwin->class), TRUE);
    gtk_label_set_justify (GTK_LABEL (tabwin->class), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX (vbox), tabwin->class, TRUE, TRUE, 0);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

    tabwin->label = gtk_label_new ("");
    gtk_label_set_use_markup (GTK_LABEL (tabwin->label), FALSE);
    gtk_label_set_justify (GTK_LABEL (tabwin->label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX (vbox), tabwin->label, TRUE, TRUE, 0);
    gtk_widget_set_size_request (GTK_WIDGET (tabwin->label), 240, -1);

    windowlist = createWindowlist (scr, current, new, cycle_range, tabwin);
    tabwin->container = windowlist;
    gtk_container_add (GTK_CONTAINER (frame), windowlist);

    color = get_color (tabwin->window, GTK_STATE_SELECTED);
    if (color)
    {
        gtk_widget_modify_bg (colorbox1, GTK_STATE_NORMAL, color);
    }

#if 0
    if (GTK_CHECK_VERSION (2, 6, 2))
    {
        gtk_label_set_ellipsize (GTK_LABEL (tabwin->label), PANGO_ELLIPSIZE_END);
        gtk_label_set_ellipsize (GTK_LABEL (tabwin->class), PANGO_ELLIPSIZE_END);
    }
#endif

    gtk_widget_show_all (GTK_WIDGET (tabwin->window));
    return tabwin;
}

Client *
tabwinGetSelected (Tabwin * t)
{
    g_return_val_if_fail (t != NULL, NULL);

    if ((t->current) && (t->current->data))
    {
        return (Client *) g_object_get_data (G_OBJECT (t->current->data), "client-ptr-val");
    }

    return NULL;
}

Client *
tabwinRemoveClient (Tabwin * t, Client * c)
{
    GList *tmp;

    g_return_val_if_fail (t != NULL, NULL);

    tmp = t->head;
    if (!tmp)
    {
        return NULL;
    }

    do
    {
        if (((Client *) g_object_get_data (tmp->data, "client-ptr-val")) == c)
        {
            if (tmp == t->current)
            {
                tabwinSelectNext (t);
            }
            gtk_container_remove (GTK_CONTAINER (t->container), tmp->data);
            t->head = g_list_delete_link (t->head, tmp);

            /* No need to look any further */
            return tabwinGetSelected (t);
        }
        /* since this is a circular list, hitting head signals end, not hitting NULL */
    }
    while ((tmp) && (tmp = g_list_next (tmp)));

    return tabwinGetSelected (t);
}

Client *
tabwinSelectNext (Tabwin * t)
{
    GList *next;

    g_return_val_if_fail (t != NULL, NULL);

    next = g_list_next(t->current);
    if (!next)
    {
        next = t->head;
        g_return_val_if_fail (next != NULL, NULL);
    }

    tabwinSetSelected (t, next->data);
    t->current = next;
    gtk_widget_queue_draw (t->window);

    return tabwinGetSelected (t);
}

Client *
tabwinSelectPrev (Tabwin * t)
{
    GList *prev;

    g_return_val_if_fail (t != NULL, NULL);

    prev = g_list_previous (t->current);
    if (!prev)
    {
        prev = g_list_last (t->head);
        g_return_val_if_fail (prev != NULL, NULL);
    }

    tabwinSetSelected (t, prev->data);
    t->current = prev;
    gtk_widget_queue_draw (t->window);

    return tabwinGetSelected (t);
}

Client *
tabwinGetHead (Tabwin * t)
{
    g_return_val_if_fail (t != NULL, NULL);

    if ((t->head) && (t->head->data))
    {
        return (Client *) g_object_get_data (G_OBJECT (t->head->data), "client-ptr-val");
    }

    return NULL;
}

void
tabwinDestroy (Tabwin * t)
{
    g_return_if_fail (t != NULL);

    if (t->head)
    {
        g_list_free (t->head);
    }
    gtk_widget_destroy (t->window);
}
