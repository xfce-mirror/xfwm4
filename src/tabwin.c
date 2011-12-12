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
        Foundation, Inc., Inc., 51 Franklin Street, Fifth Floor, Boston,
        MA 02110-1301, USA.


        xfwm4    - (c) 2002-2011 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef WIN_ICON_SIZE
#define WIN_ICON_SIZE 48
#endif

#ifndef WIN_ICON_BORDER
#define WIN_ICON_BORDER 5
#endif

#ifndef WIN_COLOR_BORDER
#define WIN_COLOR_BORDER 3
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include "icons.h"
#include "focus.h"
#include "tabwin.h"

static void tabwin_widget_class_init (TabwinWidgetClass *klass);

static GType
tabwin_widget_get_type (void)
{
    static GType type = G_TYPE_INVALID;

    if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
        static const GTypeInfo info =
        {
            sizeof (TabwinWidgetClass),
            NULL,
            NULL,
            (GClassInitFunc) tabwin_widget_class_init,
            NULL,
            NULL,
            sizeof (TabwinWidget),
            0,
            NULL,
            NULL,
        };

        type = g_type_register_static (GTK_TYPE_WINDOW, "Xfwm4TabwinWidget", &info, 0);
    }

    return type;
}

static GdkColor *
get_color (GtkWidget *win, GtkStateType state_type)
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
paint_selected (GtkWidget *w, GdkEventExpose *event, gpointer data)
{
    g_return_val_if_fail (GTK_IS_WIDGET(w), FALSE);
    TRACE ("entering paint_selected");

    gtk_draw_flat_box (w->style, w->window,
        GTK_STATE_SELECTED,
        GTK_SHADOW_NONE,
        w->allocation.x - WIN_ICON_BORDER,
        w->allocation.y - WIN_ICON_BORDER,
        w->allocation.width + 2 * WIN_ICON_BORDER,
        w->allocation.height + 2 * WIN_ICON_BORDER);
    gtk_draw_focus (w->style, w->window,
        w->allocation.x - WIN_ICON_BORDER,
        w->allocation.y - WIN_ICON_BORDER,
        w->allocation.width + 2 * WIN_ICON_BORDER,
        w->allocation.height + 2 * WIN_ICON_BORDER);
    return FALSE;
}

/* Efficiency is definitely *not* the goal here! */
static gchar *
pretty_string (const gchar *s)
{
    gchar *canonical;

    if (s)
    {
        canonical = g_ascii_strup (s, -1);
        g_strcanon (canonical, "[]()0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ", ' ');
        g_strstrip (canonical);
    }
    else
    {
        canonical = g_strdup ("...");
    }

    return canonical;
}

static void
tabwinSetLabel (TabwinWidget *tbw, gchar *class, gchar *label, int workspace)
{
    gchar *markup;
    gchar *message;
    PangoLayout *layout;

    g_return_if_fail (tbw);
    TRACE ("entering tabwinSetLabel");

    message = pretty_string (class);
    markup = g_strconcat ("<span size=\"larger\" weight=\"bold\">", message, "</span>", NULL);
    g_free (message);

    gtk_label_set_markup (GTK_LABEL (tbw->class), markup);
    g_free (markup);

    if (tbw->tabwin->display_workspace)
    {
        message = g_strdup_printf ("[%i] - %s", workspace + 1, label);
    }
    else
    {
        message = g_strdup_printf ("%s", label);
    }

    gtk_label_set_text (GTK_LABEL (tbw->label), message);
    /* Need to update the layout after setting the text */
    layout = gtk_label_get_layout (GTK_LABEL(tbw->label));
    pango_layout_set_auto_dir (layout, FALSE);
    /* the layout belong to the gtk_label and must not be freed */

    g_free (message);
}

static void
tabwinSetSelected (TabwinWidget *tbw, GtkWidget *w)
{
    Client *c;
    gchar *classname;

    g_return_if_fail (tbw);
    g_return_if_fail (GTK_IS_WIDGET(w));
    TRACE ("entering tabwinSetSelected");

    if (tbw->selected && tbw->selected_callback)
    {
        g_signal_handler_disconnect (tbw->selected, tbw->selected_callback);
    }
    tbw->selected = w;
    tbw->selected_callback = g_signal_connect (G_OBJECT (tbw->selected),
                                               "expose-event",
                                               G_CALLBACK (paint_selected),
                                               NULL);
    c = g_object_get_data (G_OBJECT (tbw->selected), "client-ptr-val");

    if (FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
    {
        classname = g_strdup_printf ("[ %s ]", c->class.res_class);
    }
    else
    {
        classname = g_strdup(c->class.res_class);
    }
    tabwinSetLabel (tbw, classname, c->name, c->win_workspace);
    g_free (classname);
}

static GtkWidget *
createWindowIcon (Client *c, gint icon_size)
{
    GdkPixbuf *icon_pixbuf;
    GdkPixbuf *icon_pixbuf_stated;
    GtkWidget *icon;

    g_return_val_if_fail (c, NULL);
    TRACE ("entering createWindowIcon");

    icon_pixbuf = getAppIcon (c->screen_info->display_info, c->window, icon_size, icon_size);
    icon_pixbuf_stated = NULL;
    icon = gtk_image_new ();
    g_object_set_data (G_OBJECT (icon), "client-ptr-val", c);

    if (icon_pixbuf)
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
        {
            icon_pixbuf_stated = gdk_pixbuf_copy (icon_pixbuf);
            gdk_pixbuf_saturate_and_pixelate (icon_pixbuf, icon_pixbuf_stated, 0.25, TRUE);
            gtk_image_set_from_pixbuf (GTK_IMAGE (icon), icon_pixbuf_stated);
            g_object_unref(icon_pixbuf_stated);
        }
        else
        {
            gtk_image_set_from_pixbuf (GTK_IMAGE (icon), icon_pixbuf);
        }
        g_object_unref(icon_pixbuf);
    }
    else
    {
        gtk_image_set_from_stock (GTK_IMAGE (icon), "gtk-missing-image", GTK_ICON_SIZE_DIALOG);
    }

    return icon;
}

static GtkWidget *
createWindowlist (ScreenInfo *screen_info, TabwinWidget *tbw)
{
    Client *c;
    GList *client_list;
    GdkRectangle monitor;
    GtkWidget *windowlist, *icon, *selected;
    int packpos;
    Tabwin *t;
    gint icon_size = WIN_ICON_SIZE;

    TRACE ("entering createWindowlist");

    packpos = 0;
    c = NULL;
    selected = NULL;
    t = tbw->tabwin;
    g_return_val_if_fail (screen_info->client_count > 0, NULL);

    gdk_screen_get_monitor_geometry (screen_info->gscr, tbw->monitor_num, &monitor);

    gtk_widget_style_get (GTK_WIDGET (tbw), "icon-size", &icon_size, NULL);
    tbw->grid_cols = (monitor.width / (icon_size + 2 * WIN_ICON_BORDER)) * 0.75;
    tbw->grid_rows = screen_info->client_count / tbw->grid_cols + 1;
    tbw->widgets = NULL;
    windowlist = gtk_table_new (tbw->grid_rows, tbw->grid_cols, FALSE);

    /* pack the client icons */
    for (client_list = *t->client_list; client_list; client_list = g_list_next (client_list))
    {
        c = (Client *) client_list->data;
        TRACE ("createWindowlist: adding %s", c->name);
        icon = createWindowIcon (c, icon_size);
        gtk_table_attach (GTK_TABLE (windowlist), GTK_WIDGET (icon),
            packpos % tbw->grid_cols, packpos % tbw->grid_cols + 1,
            packpos / tbw->grid_cols, packpos / tbw->grid_cols + 1,
            GTK_FILL, GTK_FILL, 7, 7);
        tbw->widgets = g_list_append (tbw->widgets, icon);
        packpos++;
        if (c == t->selected->data)
        {
            selected = icon;
        }
    }
    if (selected)
    {
        tabwinSetSelected (tbw, selected);
    }

    return windowlist;
}

static gboolean
tabwinConfigure (TabwinWidget *tbw, GdkEventConfigure *event)
{
    GtkWindow *window;
    GdkRectangle monitor;
    GdkScreen *screen;
    gint x, y;

    g_return_val_if_fail (tbw != NULL, FALSE);
    TRACE ("entering tabwinConfigure");

    if ((tbw->width == event->width) && (tbw->height == event->height))
    {
        return FALSE;
    }

    window = GTK_WINDOW(tbw);
    screen = gtk_window_get_screen(window);
    gdk_screen_get_monitor_geometry (screen, tbw->monitor_num, &monitor);
    x = monitor.x + (monitor.width - event->width) / 2;
    y = monitor.y + (monitor.height - event->height) / 2;
    gtk_window_move (window, x, y);

    tbw->width = event->width;
    tbw->height = event->height;

    return FALSE;
}

static void
tabwin_widget_class_init (TabwinWidgetClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_int("icon-size",
                                                              "icon size",
                                                               "the size of the application icon",
                                                               24, 128,
                                                               WIN_ICON_SIZE,
                                                               G_PARAM_READABLE));
    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_int("border-width",
                                                              "border width",
                                                               "the width of the colored border",
                                                               0, 8,
                                                               WIN_COLOR_BORDER,
                                                               G_PARAM_READABLE));
}

static TabwinWidget *
tabwinCreateWidget (Tabwin *tabwin, ScreenInfo *screen_info, gint monitor_num)
{
    TabwinWidget *tbw;
    GtkWidget *frame;
    GtkWidget *colorbox1, *colorbox2;
    GtkWidget *vbox;
    GtkWidget *windowlist;
    GdkColor *color;
    GdkRectangle monitor;
    gint border_width = WIN_COLOR_BORDER;

    TRACE ("entering tabwinCreateWidget for monitor %i", monitor_num);

    tbw = g_object_new (tabwin_widget_get_type(), "type", GTK_WINDOW_POPUP, NULL);

    tbw->width = -1;
    tbw->height = -1;
    tbw->monitor_num = monitor_num;
    tbw->tabwin = tabwin;
    tbw->selected = NULL;
    tbw->selected_callback = 0;

    gtk_window_set_screen (GTK_WINDOW (tbw), screen_info->gscr);
    gtk_widget_set_name (GTK_WIDGET (tbw), "xfwm4-tabwin");
    gtk_widget_realize (GTK_WIDGET (tbw));
    gtk_container_set_border_width (GTK_CONTAINER (tbw), 0);
    gtk_window_set_position (GTK_WINDOW (tbw), GTK_WIN_POS_NONE);
    gdk_screen_get_monitor_geometry (screen_info->gscr, tbw->monitor_num, &monitor);
    gtk_window_move (GTK_WINDOW(tbw), monitor.x + monitor.width / 2,
                                      monitor.y + monitor.height / 2);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_container_add (GTK_CONTAINER (tbw), frame);

    colorbox1 = gtk_event_box_new ();
    gtk_container_add (GTK_CONTAINER (frame), colorbox1);

    gtk_widget_style_get (GTK_WIDGET (tbw), "border-width", &border_width, NULL);
    colorbox2 = gtk_event_box_new ();
    gtk_container_set_border_width (GTK_CONTAINER (colorbox2), border_width);
    gtk_container_add (GTK_CONTAINER (colorbox1), colorbox2);

    vbox = gtk_vbox_new (FALSE, 5);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
    gtk_container_add (GTK_CONTAINER (colorbox2), vbox);

    tbw->class = gtk_label_new ("");
    gtk_label_set_use_markup (GTK_LABEL (tbw->class), TRUE);
    gtk_label_set_justify (GTK_LABEL (tbw->class), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start (GTK_BOX (vbox), tbw->class, TRUE, TRUE, 0);

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
    gtk_container_set_border_width (GTK_CONTAINER (frame), 0);
    gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

    tbw->label = gtk_label_new ("");
    gtk_label_set_use_markup (GTK_LABEL (tbw->label), FALSE);
    gtk_label_set_justify (GTK_LABEL (tbw->label), GTK_JUSTIFY_CENTER);
    gtk_label_set_use_markup (GTK_LABEL (tbw->class), TRUE);
    gtk_box_pack_start (GTK_BOX (vbox), tbw->label, TRUE, TRUE, 0);
    gtk_widget_set_size_request (GTK_WIDGET (tbw->label), 240, -1);

    windowlist = createWindowlist (screen_info, tbw);
    tbw->container = windowlist;
    gtk_container_add (GTK_CONTAINER (frame), windowlist);

    color = get_color (GTK_WIDGET (tbw), GTK_STATE_SELECTED);
    if (color)
    {
        gtk_widget_modify_bg (colorbox1, GTK_STATE_NORMAL, color);
    }

#if 0
    if (GTK_CHECK_VERSION (2, 6, 2))
    {
        gtk_label_set_ellipsize (GTK_LABEL (tbw->label), PANGO_ELLIPSIZE_END);
        gtk_label_set_ellipsize (GTK_LABEL (tbw->class), PANGO_ELLIPSIZE_END);
    }
#endif

    g_signal_connect_swapped (tbw, "configure-event",
                              GTK_SIGNAL_FUNC (tabwinConfigure), (gpointer) tbw);

    gtk_widget_show_all (GTK_WIDGET (tbw));

    return tbw;
}

Tabwin *
tabwinCreate (GList **client_list, GList *selected, gboolean display_workspace)
{
    ScreenInfo *screen_info;
    Client *c;
    Tabwin *tabwin;
    int num_monitors, i;

    g_return_val_if_fail (selected, NULL);
    g_return_val_if_fail (client_list, NULL);
    g_return_val_if_fail (*client_list, NULL);

    TRACE ("entering tabwinCreate");
    c = (Client *) selected->data;
    tabwin = g_new0 (Tabwin, 1);
    screen_info = c->screen_info;
    tabwin->display_workspace = display_workspace;
    tabwin->client_list = client_list;
    tabwin->selected = selected;
    tabwin->tabwin_list = NULL;
    num_monitors = myScreenGetNumMonitors (screen_info);
    for (i = 0; i < num_monitors; i++)
    {
        gint monitor_index;

        monitor_index = myScreenGetMonitorIndex(screen_info, i);
        tabwin->tabwin_list  = g_list_append (tabwin->tabwin_list, tabwinCreateWidget (tabwin, screen_info, monitor_index));
    }

    return tabwin;
}

Client *
tabwinGetSelected (Tabwin *t)
{
    g_return_val_if_fail (t != NULL, NULL);
    TRACE ("entering tabwinGetSelected");

    if (t->selected)
    {
        return (Client *) t->selected->data;
    }

    return NULL;
}

Client *
tabwinRemoveClient (Tabwin *t, Client *c)
{
    GList *client_list, *tabwin_list, *widgets;
    GtkWidget *icon;
    TabwinWidget *tbw;

    g_return_val_if_fail (t != NULL, NULL);
    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering tabwinRemoveClient");

    if (!*t->client_list)
    {
        return NULL;
    }

    /* First, remove the client from our own client list */
    for (client_list = *t->client_list; client_list; client_list = g_list_next (client_list))
    {
        if (client_list->data == c)
        {
            if (client_list == t->selected)
            {
                tabwinSelectNext (t);
            }
            *t->client_list = g_list_delete_link (*t->client_list, client_list);
            break;
        }
    }

    /* Second, remove the icon from all boxes */
    for (tabwin_list = t->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tbw = (TabwinWidget *) tabwin_list->data;
        for (widgets = tbw->widgets; widgets; widgets = g_list_next (widgets))
        {
            icon = GTK_WIDGET (widgets->data);
            if (((Client *) g_object_get_data (G_OBJECT(icon), "client-ptr-val")) == c)
            {
                gtk_container_remove (GTK_CONTAINER (tbw->container), icon);
                tbw->widgets = g_list_delete_link (tbw->widgets, widgets);
            }
        }
    }

    return tabwinGetSelected (t);
}

Client *
tabwinSelectHead (Tabwin *t)
{
    GList *head;
    GList *tabwin_list, *widgets;
    GtkWidget *icon;
    TabwinWidget *tbw;

    g_return_val_if_fail (t != NULL, NULL);
    TRACE ("entering tabwinSelectFirst");

    head = *t->client_list;
    if (!head)
    {
        return NULL;
    }
    t->selected = head;
    for (tabwin_list = t->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tbw = (TabwinWidget *) tabwin_list->data;
        for (widgets = tbw->widgets; widgets; widgets = g_list_next (widgets))
        {
            icon = GTK_WIDGET (widgets->data);
            if (((Client *) g_object_get_data (G_OBJECT(icon), "client-ptr-val")) == head->data)
                {
                    tabwinSetSelected (tbw, icon);
                    gtk_widget_queue_draw (GTK_WIDGET(tbw));
                }
            }
    }

    return tabwinGetSelected (t);
}

Client *
tabwinSelectNext (Tabwin *t)
{
    GList *next;
    GList *tabwin_list, *widgets;
    GtkWidget *icon;
    TabwinWidget *tbw;

    g_return_val_if_fail (t != NULL, NULL);
    TRACE ("entering tabwinSelectNext");

    next = g_list_next(t->selected);
    if (!next)
    {
        next = *t->client_list;
        g_return_val_if_fail (next != NULL, NULL);
    }
    t->selected = next;
    for (tabwin_list = t->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tbw = (TabwinWidget *) tabwin_list->data;
        for (widgets = tbw->widgets; widgets; widgets = g_list_next (widgets))
        {
            icon = GTK_WIDGET (widgets->data);
            if (((Client *) g_object_get_data (G_OBJECT(icon), "client-ptr-val")) == next->data)
                {
                    tabwinSetSelected (tbw, icon);
                    gtk_widget_queue_draw (GTK_WIDGET(tbw));
                }
            }
    }

    return tabwinGetSelected (t);
}

Client *
tabwinSelectPrev (Tabwin *t)
{
    GList *prev;
    GList *tabwin_list, *widgets;
    GtkWidget *icon;
    TabwinWidget *tbw;

    g_return_val_if_fail (t != NULL, NULL);
    TRACE ("entering tabwinSelectPrev");

    prev = g_list_previous (t->selected);
    if (!prev)
    {
        prev = g_list_last (*t->client_list);
        g_return_val_if_fail (prev != NULL, NULL);
    }
    t->selected = prev;
    for (tabwin_list = t->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tbw = (TabwinWidget *) tabwin_list->data;
        for (widgets = tbw->widgets; widgets; widgets = g_list_next (widgets))
        {
            icon = GTK_WIDGET (widgets->data);
            if (((Client *) g_object_get_data (G_OBJECT(icon), "client-ptr-val")) == prev->data)
                {
                    tabwinSetSelected (tbw, icon);
                    gtk_widget_queue_draw (GTK_WIDGET(tbw));
                }
            }
    }

    return tabwinGetSelected (t);
}

void
tabwinDestroy (Tabwin *t)
{
    GList *tabwin_list;
    TabwinWidget *tbw;

    g_return_if_fail (t != NULL);
    TRACE ("entering tabwinDestroy");

    g_return_if_fail (t != NULL);
    for (tabwin_list = t->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tbw = (TabwinWidget *) tabwin_list->data;
        g_list_free (tbw->widgets);
        gtk_widget_destroy (GTK_WIDGET (tbw));
    }
    g_list_free (t->tabwin_list);
}

