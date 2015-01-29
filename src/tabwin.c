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


        xfwm4    - (c) 2002-2015 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef WIN_ICON_SIZE
#define WIN_ICON_SIZE 48
#endif

#ifndef WIN_PREVIEW_SIZE
#define WIN_PREVIEW_SIZE (5 * WIN_ICON_SIZE)
#endif

#ifndef LISTVIEW_WIN_ICON_SIZE
#define LISTVIEW_WIN_ICON_SIZE (WIN_ICON_SIZE / 2)
#endif

#ifndef WIN_ICON_BORDER
#define WIN_ICON_BORDER 5
#endif

#ifndef WIN_BORDER_WIDTH
#define WIN_BORDER_WIDTH 1
#endif

#ifndef WIN_MAX_RATIO
#define WIN_MAX_RATIO 0.80
#endif

#ifndef WIN_ALPHA
#define WIN_ALPHA 0.85
#endif

#ifndef WIN_BORDER_ALPHA
#define WIN_BORDER_ALPHA 0.5
#endif

#ifndef WIN_BORDER_RADIUS
#define WIN_BORDER_RADIUS 10
#endif

#include <math.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include "icons.h"
#include "focus.h"
#include "tabwin.h"
#include "settings.h"
#include "compositor.h"

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
tabwin_expose (GtkWidget *tabwin_widget, GdkEventExpose *event, gpointer data)
{
    GdkScreen *screen;
    cairo_t *cr;
    GdkColor *bg_normal = get_color (tabwin_widget, GTK_STATE_NORMAL);
    GdkColor *bg_selected = get_color (tabwin_widget, GTK_STATE_SELECTED);
    gint border_width = WIN_BORDER_WIDTH;
    gdouble border_alpha = WIN_BORDER_ALPHA;
    gdouble alpha = WIN_ALPHA;
    gint border_radius = WIN_BORDER_RADIUS;
    gdouble degrees = G_PI / 180.0;
    gdouble width = tabwin_widget->allocation.width;
    gdouble height = tabwin_widget->allocation.height;

    screen = gtk_widget_get_screen (GTK_WIDGET(tabwin_widget));
    cr = gdk_cairo_create (tabwin_widget->window);
    if (G_UNLIKELY (cr == NULL))
      return FALSE;

    gtk_widget_style_get (GTK_WIDGET (tabwin_widget),
                            "border-width", &border_width,
                            "border-alpha", &border_alpha,
                            "alpha", &alpha,
                            "border-radius", &border_radius,
                            NULL);
    cairo_set_line_width (cr, border_width);

    if (gdk_screen_is_composited (screen))
    {
        cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
        gdk_cairo_region (cr, event->region);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
        cairo_fill_preserve (cr);
        cairo_clip (cr);
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

        /* Draw a filled rounded rectangle with an outline */
        cairo_arc (cr, width - border_radius - 0.5,
                       border_radius + 0.5,
                       border_radius,
                       -90 * degrees,
                         0 * degrees);
        cairo_arc (cr, width - border_radius - 0.5,
                       height - border_radius - 0.5,
                       border_radius,
                        0 * degrees,
                       90 * degrees);
        cairo_arc (cr, border_radius + 0.5,
                       height - border_radius - 0.5,
                       border_radius,
                        90 * degrees,
                       180 * degrees);
        cairo_arc (cr, border_radius + 0.5,
                       border_radius + 0.5,
                       border_radius,
                       180 * degrees,
                       270 * degrees);
        cairo_close_path(cr);
        cairo_set_source_rgba (cr, bg_normal->red / 65535.0,
                                   bg_normal->green / 65535.0,
                                   bg_normal->blue / 65535.0,
                                   alpha);
        cairo_fill_preserve (cr);
        cairo_set_source_rgba (cr, bg_selected->red / 65535.0,
                                   bg_selected->green / 65535.0,
                                   bg_selected->blue / 65535.0,
                                   border_alpha);
        cairo_stroke (cr);
    }
    else
    {
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_rectangle (cr, 0, 0, width, height);
        gdk_cairo_set_source_color (cr, bg_normal);
        cairo_fill_preserve (cr);
        gdk_cairo_set_source_color (cr, bg_selected);
        cairo_stroke (cr);
    }

    cairo_destroy (cr);
    return FALSE;
}

static gboolean
paint_selected (GtkWidget *w, GdkEventExpose *event, gpointer user_data)
{
    TabwinWidget *tabwin_widget = user_data;

    g_return_val_if_fail (GTK_IS_WIDGET(w), FALSE);
    TRACE ("entering paint_selected");

    if (w == tabwin_widget->hovered)
    {
        gtk_widget_set_state (w, GTK_STATE_ACTIVE);
    }
    else
    {
        gtk_widget_set_state (w, GTK_STATE_SELECTED);
    }

    gtk_button_set_relief (GTK_BUTTON (w), GTK_RELIEF_NORMAL);

    return FALSE;
}

/* Efficiency is definitely *not* the goal here! */
static gchar *
pretty_string (const gchar *s)
{
    gchar *canonical;

    if (s)
    {
        canonical = g_strdup_printf ("%s",s);
        g_strcanon (canonical, "[]()0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", ' ');
        g_strstrip (canonical);
    }
    else
    {
        canonical = g_strdup ("...");
    }

    return canonical;
}

static void
tabwinSetLabel (TabwinWidget *tabwin_widget, GtkWidget *buttonlabel, gchar *class, gchar *label, int workspace)
{
    gchar *message;
    PangoLayout *layout;

    g_return_if_fail (tabwin_widget);
    TRACE ("entering tabwinSetLabel");

    message = pretty_string (class);
    gtk_label_set_text (GTK_LABEL (buttonlabel), message);
    g_free (message);

    if (tabwin_widget->tabwin->display_workspace)
    {
        message = g_strdup_printf ("[%i] - %s", workspace + 1, label);
    }
    else
    {
        message = g_strdup_printf ("%s", label);
    }

    gtk_label_set_text (GTK_LABEL (tabwin_widget->label), message);
    /* Need to update the layout after setting the text */
    layout = gtk_label_get_layout (GTK_LABEL(tabwin_widget->label));
    pango_layout_set_auto_dir (layout, FALSE);
    /* the layout belong to the gtk_label and must not be freed */

    g_free (message);
}

static void
tabwinSetSelected (TabwinWidget *tabwin_widget, GtkWidget *w, GtkWidget *l)
{
    Client *c;
    gchar *classname;

    g_return_if_fail (tabwin_widget);
    g_return_if_fail (GTK_IS_WIDGET(w));
    TRACE ("entering tabwinSetSelected");

    if (tabwin_widget->selected && tabwin_widget->selected_callback)
    {
        g_signal_handler_disconnect (tabwin_widget->selected, tabwin_widget->selected_callback);
    }
    tabwin_widget->selected = w;
    tabwin_widget->selected_callback = g_signal_connect (G_OBJECT (tabwin_widget->selected),
                                               "expose-event",
                                               G_CALLBACK (paint_selected),
                                               tabwin_widget);

    c = g_object_get_data (G_OBJECT (tabwin_widget->selected), "client-ptr-val");

    if (c != NULL)
    {
        /* We don't update labels here */
        if (c->screen_info->params->cycle_tabwin_mode == OVERFLOW_COLUMN_GRID)
        {
            return;
        }

        classname = g_strdup(c->class.res_class);
        tabwinSetLabel (tabwin_widget, l, classname, c->name, c->win_workspace);
        g_free (classname);
    }
}

static Client *
tabwinSelectWidget (Tabwin *tabwin)
{
    GList *tabwin_list, *widgets, *selected;
    GtkWidget *window_button, *buttonbox, *buttonlabel;
    TabwinWidget *tabwin_widget;
    Client *c;

    g_return_val_if_fail (tabwin != NULL, NULL);
    TRACE ("entering tabwinSelectWidget");

    for (tabwin_list = tabwin->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tabwin_widget = (TabwinWidget *) tabwin_list->data;
        for (widgets = tabwin_widget->widgets; widgets; widgets = g_list_next (widgets))
        {
            window_button = GTK_WIDGET (widgets->data);
            gtk_button_set_relief (GTK_BUTTON (window_button), GTK_RELIEF_NONE);
            buttonbox = GTK_WIDGET (gtk_container_get_children (GTK_CONTAINER (window_button))[0].data );
            buttonlabel = GTK_WIDGET (g_list_nth_data (gtk_container_get_children (GTK_CONTAINER(buttonbox)), 1) );
            gtk_label_set_text (GTK_LABEL (buttonlabel), "");

            if (gtk_widget_is_focus (window_button))
            {
                c = g_object_get_data (G_OBJECT (window_button), "client-ptr-val");
                selected = g_list_find (*tabwin->client_list, c);

                if (selected)
                {
                    tabwin->selected = selected;
                }

                tabwinSetSelected (tabwin_widget, window_button, buttonlabel);
                gtk_widget_queue_draw (GTK_WIDGET (tabwin_widget));
            }
        }
    }

    return tabwinGetSelected (tabwin);
}

static GtkWidget *
createWindowIcon (GdkPixbuf *icon_pixbuf)
{
    GtkWidget *icon;

    TRACE ("entering createWindowIcon");

    icon = gtk_image_new ();
    if (icon_pixbuf)
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (icon), icon_pixbuf);
    }
    else
    {
        gtk_image_set_from_stock (GTK_IMAGE (icon), "gtk-missing-image", GTK_ICON_SIZE_DIALOG);
    }

    return icon;
}

static int
getMinMonitorWidth (ScreenInfo *screen_info)
{
    int i, min_width, num_monitors = myScreenGetNumMonitors (screen_info);
    for (min_width = i = 0; i < num_monitors; i++)
    {
        GdkRectangle monitor;
        gdk_screen_get_monitor_geometry (screen_info->gscr, i, &monitor);
        if (min_width == 0 || monitor.width < min_width)
            min_width = monitor.width;
    }
    return min_width;
}

static int
getMinMonitorHeight (ScreenInfo *screen_info)
{
    int i, min_height, num_monitors = myScreenGetNumMonitors (screen_info);
    for (min_height = i = 0; i < num_monitors; i++)
    {
        GdkRectangle monitor;
        gdk_screen_get_monitor_geometry (screen_info->gscr, i, &monitor);
        if (min_height == 0 || monitor.height < min_height)
        {
            min_height = monitor.height;
        }
    }
    return min_height;
}

static gboolean
cb_window_button_enter (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    TabwinWidget *tabwin_widget = user_data;
    Client *c;
    GtkWidget *buttonbox, *buttonlabel;
    gchar *classname;

    TRACE ("entering");

    g_return_val_if_fail (tabwin_widget != NULL, FALSE);

    /* keep track of which widget we're hovered over */
    tabwin_widget->hovered = widget;

    c = g_object_get_data (G_OBJECT (widget), "client-ptr-val");

    /* when hovering over a window icon, display it's label but don't
     * select it */
    if (c != NULL)
    {
        if (gtk_widget_is_focus (widget))
        {
            gtk_widget_set_state (widget, GTK_STATE_ACTIVE);
        }

        /* we don't update the labels on mouse over for this mode */
        if (c->screen_info->params->cycle_tabwin_mode == OVERFLOW_COLUMN_GRID)
        {
            return FALSE;
        }

        buttonbox = GTK_WIDGET (gtk_container_get_children (GTK_CONTAINER (widget))[0].data);
        buttonlabel = GTK_WIDGET (g_list_nth_data( gtk_container_get_children (GTK_CONTAINER (buttonbox)), 1));

        classname = g_strdup (c->class.res_class);
        tabwinSetLabel (tabwin_widget, buttonlabel, classname, c->name, c->win_workspace);
        g_free (classname);
    }

    return FALSE;
}

static gboolean
cb_window_button_leave (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    TabwinWidget *tabwin_widget = user_data;
    Client *c;

    TRACE ("entering");

    g_return_val_if_fail (tabwin_widget != NULL, FALSE);

    tabwin_widget->hovered = NULL;

    /* don't do anything if we have the focus */
    if (gtk_widget_is_focus (widget))
    {
        gtk_widget_set_state (widget, GTK_STATE_SELECTED);
        return FALSE;
    }

    c = g_object_get_data (G_OBJECT (widget), "client-ptr-val");

    /* when hovering over a window icon, display it's label but don't
     * select it */
    if (c != NULL)
    {
        /* we don't update the labels for this mode */
        if (c->screen_info->params->cycle_tabwin_mode == OVERFLOW_COLUMN_GRID)
        {
            return FALSE;
        }

        /* reselect the selected widget, it will clear everything else out */
        tabwinSelectWidget (tabwin_widget->tabwin);
    }

    return FALSE;
}

static GtkWidget *
createWindowlist (ScreenInfo *screen_info, TabwinWidget *tabwin_widget)
{
    Client *c;
    GList *client_list;
    GList *icon_list;
    GtkWidget *windowlist;
    GtkWidget *icon;
    GtkWidget *selected;
    GtkWidget *window_button;
    GtkWidget *buttonbox;
    GtkWidget *buttonlabel;
    GtkWidget *selected_label;
    GdkPixbuf *icon_pixbuf;
    gint packpos;
    gint label_width;
    gint size_request;
    Tabwin *tabwin;

    TRACE ("entering createWindowlist");
    g_return_val_if_fail (tabwin_widget != NULL, NULL);
    tabwin = tabwin_widget->tabwin;
    g_return_val_if_fail (tabwin->client_count > 0, NULL);

    packpos = 0;
    c = NULL;
    selected = NULL;
    tabwin_widget->widgets = NULL;
    size_request = tabwin->icon_size + tabwin->label_height + 2 * WIN_ICON_BORDER;

    windowlist = gtk_table_new (tabwin->grid_rows, tabwin->grid_cols, FALSE);

    /* pack the client icons */
    icon_list = tabwin->icon_list;
    for (client_list = *tabwin->client_list; client_list; client_list = g_list_next (client_list))
    {
        c = (Client *) client_list->data;
        TRACE ("createWindowlist: adding %s", c->name);
        icon_pixbuf = (GdkPixbuf *) icon_list->data;
        icon_list = g_list_next (icon_list);

        window_button = gtk_button_new ();
        gtk_button_set_relief (GTK_BUTTON (window_button), GTK_RELIEF_NONE);
        g_object_set_data (G_OBJECT (window_button), "client-ptr-val", c);
        g_signal_connect (window_button, "enter-notify-event",
                          G_CALLBACK (cb_window_button_enter), tabwin_widget);
        g_signal_connect (window_button, "leave-notify-event",
                          G_CALLBACK (cb_window_button_leave), tabwin_widget);
        gtk_widget_add_events (window_button, GDK_ENTER_NOTIFY_MASK);

        icon = createWindowIcon (icon_pixbuf);
        if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
        {
            gtk_widget_set_size_request (GTK_WIDGET (window_button), size_request, size_request);
            buttonbox = gtk_vbox_new (FALSE, 0);
            buttonlabel = gtk_label_new ("");
            gtk_misc_set_alignment (GTK_MISC (buttonlabel), 0.5, 1.0);

            gtk_misc_set_alignment (GTK_MISC (icon), 0.5, 1.0);
            gtk_box_pack_start (GTK_BOX (buttonbox), icon, TRUE, TRUE, 0);
        }
        else
        {
            label_width = tabwin->monitor_width / (tabwin->grid_cols + 1);

            if (tabwin->icon_size < tabwin->label_height)
            {
                gtk_widget_set_size_request (GTK_WIDGET (window_button),
                                             label_width, tabwin->label_height + 8);
            }
            else
            {
                gtk_widget_set_size_request (GTK_WIDGET (window_button),
                                             label_width, tabwin->icon_size + 8);
            }
            buttonbox = gtk_hbox_new (FALSE, 6);
            buttonlabel = gtk_label_new (c->name);
            gtk_misc_set_alignment (GTK_MISC (buttonlabel), 0, 0.5);

            gtk_misc_set_alignment (GTK_MISC (icon), 0.5, 0.5);
            gtk_box_pack_start (GTK_BOX (buttonbox), icon, FALSE, FALSE, 0);
        }
        gtk_container_add (GTK_CONTAINER (window_button), buttonbox);

        gtk_label_set_justify (GTK_LABEL (buttonlabel), GTK_JUSTIFY_CENTER);
        gtk_label_set_ellipsize (GTK_LABEL (buttonlabel), PANGO_ELLIPSIZE_END);
        gtk_box_pack_start (GTK_BOX (buttonbox), buttonlabel, TRUE, TRUE, 0);

        if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
        {
            gtk_table_attach (GTK_TABLE (windowlist), GTK_WIDGET (window_button),
                packpos % tabwin->grid_cols, packpos % tabwin->grid_cols + 1,
                packpos / tabwin->grid_cols, packpos / tabwin->grid_cols + 1,
                GTK_FILL, GTK_FILL, 2, 2);
        }
        else
        {
            gtk_table_attach (GTK_TABLE (windowlist), GTK_WIDGET (window_button),
                packpos / tabwin->grid_rows, packpos / tabwin->grid_rows + 1,
                packpos % tabwin->grid_rows, packpos % tabwin->grid_rows + 1,
                GTK_FILL, GTK_FILL, 2, 2);
        }

        tabwin_widget->widgets = g_list_append (tabwin_widget->widgets, window_button);
        packpos++;
        if (c == tabwin->selected->data)
        {
            selected = window_button;
            selected_label = buttonlabel;
            gtk_widget_grab_focus (selected);
        }
    }
    if (selected)
    {
        tabwinSetSelected (tabwin_widget, selected, selected_label);
    }
    return windowlist;
}

static gboolean
tabwinConfigure (TabwinWidget *tabwin_widget, GdkEventConfigure *event)
{
    GtkWindow *window;
    GdkRectangle monitor;
    GdkScreen *screen;
    gint x, y;

    g_return_val_if_fail (tabwin_widget != NULL, FALSE);
    TRACE ("entering tabwinConfigure");

    if ((tabwin_widget->width == event->width) && (tabwin_widget->height == event->height))
    {
        return FALSE;
    }

    window = GTK_WINDOW(tabwin_widget);
    screen = gtk_window_get_screen(window);
    gdk_screen_get_monitor_geometry (screen, tabwin_widget->monitor_num, &monitor);
    x = monitor.x + (monitor.width - event->width) / 2;
    y = monitor.y + (monitor.height - event->height) / 2;
    gtk_window_move (window, x, y);

    tabwin_widget->width = event->width;
    tabwin_widget->height = event->height;

    return FALSE;
}

static void
tabwin_widget_class_init (TabwinWidgetClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_int ("preview-size",
                                                               "preview size",
                                                                "the size of the application preview",
                                                                24, 256,
                                                                WIN_PREVIEW_SIZE,
                                                                G_PARAM_READABLE));
    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_int ("icon-size",
                                                               "icon size",
                                                                "the size of the application icon",
                                                                24, 128,
                                                                WIN_ICON_SIZE,
                                                                G_PARAM_READABLE));
    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_int ("listview-icon-size",
                                                               "listview icon size",
                                                               "the size of the application icon in listview",
                                                               16, 48,
                                                               LISTVIEW_WIN_ICON_SIZE,
                                                               G_PARAM_READABLE));
    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_int ("border-width",
                                                               "border width",
                                                               "the width of the colored border",
                                                               0, 8,
                                                               WIN_BORDER_WIDTH,
                                                               G_PARAM_READABLE));
    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_int ("border-radius",
                                                               "border radius",
                                                               "the border radius of the window",
                                                               0, 20,
                                                               WIN_BORDER_RADIUS,
                                                               G_PARAM_READABLE));
    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_double ("border-alpha",
                                                                  "border alpha",
                                                                  "the alpha of the colored border",
                                                                  0.0, 1.0,
                                                                  WIN_BORDER_ALPHA,
                                                                  G_PARAM_READABLE));
    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_double ("alpha",
                                                                  "alpha",
                                                                  "the alpha of the window",
                                                                  0.0, 1.0,
                                                                  WIN_ALPHA,
                                                                  G_PARAM_READABLE));
}

static void
computeTabwinData (ScreenInfo *screen_info, TabwinWidget *tabwin_widget)
{
    Tabwin *tabwin;
    GdkPixbuf *icon_pixbuf;
    PangoLayout *layout;
    GList *client_list;
    gint size_request;
    gboolean preview;

    TRACE ("entering computeTabwinData");
    g_return_if_fail (GTK_IS_WIDGET(tabwin_widget));
    tabwin = tabwin_widget->tabwin;
    g_return_if_fail (tabwin->client_count > 0);

    tabwin->monitor_width = getMinMonitorWidth (screen_info);
    tabwin->monitor_height = getMinMonitorHeight (screen_info);
    tabwin->label_height = 30;
    preview = screen_info->params->cycle_preview &
              compositorWindowPixmapAvailable (screen_info);

    /* We need to account for changes to the font size in the user's
     * appearance theme and gtkrc settings */
    layout = gtk_widget_create_pango_layout (GTK_WIDGET (tabwin_widget), "");
    pango_layout_get_pixel_size (layout, NULL, &tabwin->label_height);
    g_object_unref (layout);

    if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
    {
        if (preview)
        {
            tabwin->icon_size = WIN_PREVIEW_SIZE;
            gtk_widget_style_get (GTK_WIDGET (tabwin_widget),
                                  "preview-size", &tabwin->icon_size, NULL);
        }
        else
        {
            tabwin->icon_size = WIN_ICON_SIZE;
            gtk_widget_style_get (GTK_WIDGET (tabwin_widget),
                                  "icon-size", &tabwin->icon_size, NULL);
        }
        size_request = tabwin->icon_size + tabwin->label_height + 2 * WIN_ICON_BORDER;
        tabwin->grid_cols = (int) (floor ((double) tabwin->monitor_width * WIN_MAX_RATIO /
                                          (double) size_request));
        tabwin->grid_rows = (int) (ceil ((double) tabwin->client_count /
                                         (double) tabwin->grid_cols));

        /* If we run out of space, halve the icon size to make more room. */
        while ((size_request) * tabwin->grid_rows + tabwin->label_height >
               ((double) tabwin->monitor_height) * WIN_MAX_RATIO)
        {
            /* Disable preview otherwise it'd be too slow */
            if (preview)
            {
                preview = FALSE;
                /* switch back to regular icon size */
                tabwin->icon_size = WIN_ICON_SIZE;
                gtk_widget_style_get (GTK_WIDGET (tabwin_widget),
                                      "icon-size", &tabwin->icon_size, NULL);
            }
            else
            {
                tabwin->icon_size = tabwin->icon_size / 2;
            }
            size_request = tabwin->icon_size + tabwin->label_height + 2 * WIN_ICON_BORDER;

            /* Recalculate with new icon size */
            tabwin->grid_cols = (int) (floor ((double) tabwin->monitor_width * WIN_MAX_RATIO /
                                              (double) size_request));
            tabwin->grid_rows = (int) (ceil ((double) tabwin->client_count /
                                             (double) tabwin->grid_cols));

            /* Shrinking the icon too much makes it hard to see */
            if (tabwin->icon_size < 8)
            {
                tabwin->icon_size = 8;
                break;
            }
        }
    }
    else
    {
        tabwin->icon_size = LISTVIEW_WIN_ICON_SIZE;
        gtk_widget_style_get (GTK_WIDGET (tabwin_widget),
                              "listview-icon-size", &tabwin->icon_size, NULL);
        tabwin->grid_rows = (int) (floor ((double) tabwin->monitor_height * WIN_MAX_RATIO /
                                          (double) (tabwin->icon_size + 2 * WIN_ICON_BORDER)));
        tabwin->grid_cols = (int) (ceil ((double) tabwin->client_count /
                                         (double) tabwin->grid_rows));
    }

    /* pack the client icons */
    for (client_list = *tabwin->client_list; client_list; client_list = g_list_next (client_list))
    {
        Client *c = (Client *) client_list->data;

        if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
        {
            if (preview)
            {
                icon_pixbuf = getClientIcon (c, tabwin->icon_size, tabwin->icon_size);
            }
            else
            {
                icon_pixbuf = getAppIcon (c->screen_info, c->window,
                                          tabwin->icon_size, tabwin->icon_size);
            }
        }
        else
        {
            /* No preview in list mode */
            icon_pixbuf = getAppIcon (c->screen_info, c->window,
                                      tabwin->icon_size, tabwin->icon_size);
        }
        tabwin->icon_list = g_list_append(tabwin->icon_list, icon_pixbuf);
    }
}

static TabwinWidget *
tabwinCreateWidget (Tabwin *tabwin, ScreenInfo *screen_info, gint monitor_num)
{
    TabwinWidget *tabwin_widget;
    GdkScreen *screen;
    GtkWidget *vbox;
    GtkWidget *windowlist;
    GdkRectangle monitor;
    gint border_radius = WIN_BORDER_RADIUS;

    TRACE ("entering tabwinCreateWidget for monitor %i", monitor_num);

    tabwin_widget = g_object_new (tabwin_widget_get_type(), "type", GTK_WINDOW_POPUP, NULL);

    tabwin_widget->monitor_num = monitor_num;
    tabwin_widget->tabwin = tabwin;
    tabwin_widget->selected = NULL;
    tabwin_widget->selected_callback = 0;
    tabwin_widget->width = -1;
    tabwin_widget->height = -1;

    gtk_window_set_screen (GTK_WINDOW (tabwin_widget), screen_info->gscr);
    gtk_widget_set_name (GTK_WIDGET (tabwin_widget), "xfwm4-tabwin");

    /* Check for compositing and set colormap for it */
    screen = gtk_widget_get_screen (GTK_WIDGET (tabwin_widget));
    if(gdk_screen_is_composited (screen)) {
        GdkColormap *cmap = gdk_screen_get_rgba_colormap (screen);
        if(cmap)
            gtk_widget_set_colormap (GTK_WIDGET (tabwin_widget), cmap);
    }

    if (tabwin->icon_list == NULL)
    {
        computeTabwinData (screen_info, tabwin_widget);
    }
    gtk_widget_style_get (GTK_WIDGET(tabwin_widget), "border-radius", &border_radius, NULL);
    gtk_widget_realize (GTK_WIDGET (tabwin_widget));
    gtk_container_set_border_width (GTK_CONTAINER (tabwin_widget), border_radius + 4);
    gtk_widget_set_app_paintable (GTK_WIDGET (tabwin_widget), TRUE);
    gtk_window_set_position (GTK_WINDOW (tabwin_widget), GTK_WIN_POS_NONE);
    gdk_screen_get_monitor_geometry (screen_info->gscr, tabwin_widget->monitor_num, &monitor);
    gtk_window_move (GTK_WINDOW (tabwin_widget), monitor.x + monitor.width / 2,
                                      monitor.y + monitor.height / 2);

    vbox = gtk_vbox_new (FALSE, 3);
    gtk_container_add (GTK_CONTAINER (tabwin_widget), vbox);

    if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
    {
        tabwin_widget->label = gtk_label_new ("");
        gtk_label_set_use_markup (GTK_LABEL (tabwin_widget->label), TRUE);
        gtk_label_set_justify (GTK_LABEL (tabwin_widget->label), GTK_JUSTIFY_CENTER);
        gtk_label_set_ellipsize (GTK_LABEL (tabwin_widget->label), PANGO_ELLIPSIZE_END);
        gtk_box_pack_end (GTK_BOX (vbox), tabwin_widget->label, TRUE, TRUE, 0);
    }

    windowlist = createWindowlist (screen_info, tabwin_widget);
    tabwin_widget->container = windowlist;
    gtk_box_pack_start (GTK_BOX (vbox), windowlist, TRUE, TRUE, 0);

    g_signal_connect_swapped (tabwin_widget, "configure-event",
                              GTK_SIGNAL_FUNC (tabwinConfigure),
                              (gpointer) tabwin_widget);

    g_signal_connect (tabwin_widget, "expose-event",
                      GTK_SIGNAL_FUNC (tabwin_expose),
                      (gpointer) tabwin_widget);

    gtk_widget_show_all (GTK_WIDGET (tabwin_widget));

    return tabwin_widget;
}

static Client *
tabwinChange2Selected (Tabwin *tabwin, GList *selected)
{
    GList *tabwin_list, *widgets;
    GtkWidget *window_button, *buttonbox, *buttonlabel;
    TabwinWidget *tabwin_widget;
    Client *c;

    tabwin->selected = selected;
    for (tabwin_list = tabwin->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tabwin_widget = (TabwinWidget *) tabwin_list->data;
        for (widgets = tabwin_widget->widgets; widgets; widgets = g_list_next (widgets))
        {
            window_button = GTK_WIDGET (widgets->data);
            gtk_button_set_relief (GTK_BUTTON (window_button), GTK_RELIEF_NONE);
            buttonbox = GTK_WIDGET (gtk_container_get_children (GTK_CONTAINER (window_button))[0].data );
            buttonlabel = GTK_WIDGET (g_list_nth_data (gtk_container_get_children (GTK_CONTAINER (buttonbox)), 1) );

            c = g_object_get_data (G_OBJECT (window_button), "client-ptr-val");

            if (c != NULL)
            {
                /* don't clear label if mouse is inside the previously
                 * selected button */
                if (c->screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID
                    && window_button != tabwin_widget->hovered)
                {
                    gtk_label_set_text (GTK_LABEL (buttonlabel), "");
                }

                if (c == tabwin->selected->data)
                {
                    gtk_widget_grab_focus (window_button);
                    tabwinSetSelected (tabwin_widget, window_button, buttonlabel);
                    gtk_widget_queue_draw (GTK_WIDGET (tabwin_widget));
                }
                else if (window_button == tabwin_widget->hovered)
                {
                    gtk_widget_set_state (window_button, GTK_STATE_PRELIGHT);
                }
                else
                {
                    gtk_widget_set_state (window_button, GTK_STATE_NORMAL);
                }
            }
        }
    }
    return tabwinGetSelected (tabwin);
}

Tabwin *
tabwinCreate (GList **client_list, GList *selected, gboolean display_workspace)
{
    ScreenInfo *screen_info;
    Client *c;
    Tabwin *tabwin;
    TabwinWidget *win;
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
    tabwin->client_count = g_list_length (*client_list);
    tabwin->selected = selected;
    tabwin->tabwin_list = NULL;
    tabwin->icon_list = NULL;

    num_monitors = myScreenGetNumMonitors (screen_info);
    for (i = 0; i < num_monitors; i++)
    {
        gint monitor_index;

        monitor_index = myScreenGetMonitorIndex (screen_info, i);
        win = tabwinCreateWidget (tabwin, screen_info, monitor_index);
        tabwin->tabwin_list  = g_list_append (tabwin->tabwin_list, win);
    }

    return tabwin;
}

Client *
tabwinGetSelected (Tabwin *tabwin)
{
    g_return_val_if_fail (tabwin != NULL, NULL);
    TRACE ("entering tabwinGetSelected");

    if (tabwin->selected)
    {
        return (Client *) tabwin->selected->data;
    }

    return NULL;
}

Client *
tabwinRemoveClient (Tabwin *tabwin, Client *c)
{
    GList *client_list, *tabwin_list, *widgets;
    GtkWidget *icon;
    TabwinWidget *tabwin_widget;

    g_return_val_if_fail (tabwin != NULL, NULL);
    g_return_val_if_fail (c != NULL, NULL);
    TRACE ("entering tabwinRemoveClient");

    if (!*tabwin->client_list)
    {
        return NULL;
    }

    /* First, remove the client from our own client list */
    for (client_list = *tabwin->client_list; client_list; client_list = g_list_next (client_list))
    {
        if (client_list->data == c)
        {
            if (client_list == tabwin->selected)
            {
                tabwinSelectNext (tabwin);
            }
            *tabwin->client_list = g_list_delete_link (*tabwin->client_list, client_list);
            break;
        }
    }

    /* Second, remove the icon from all boxes */
    for (tabwin_list = tabwin->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tabwin_widget = (TabwinWidget *) tabwin_list->data;
        for (widgets = tabwin_widget->widgets; widgets; widgets = g_list_next (widgets))
        {
            icon = GTK_WIDGET (widgets->data);
            if (((Client *) g_object_get_data (G_OBJECT(icon), "client-ptr-val")) == c)
            {
                gtk_container_remove (GTK_CONTAINER (tabwin_widget->container), icon);
                tabwin_widget->widgets = g_list_delete_link (tabwin_widget->widgets, widgets);
            }
        }
    }

    return tabwinGetSelected (tabwin);
}

Client *
tabwinSelectHead (Tabwin *tabwin)
{
    GList *head;
    GList *tabwin_list, *widgets;
    GtkWidget *window_button, *buttonbox, *buttonlabel;
    TabwinWidget *tabwin_widget;

    g_return_val_if_fail (tabwin != NULL, NULL);
    TRACE ("entering tabwinSelectFirst");

    head = *tabwin->client_list;
    if (!head)
    {
        return NULL;
    }
    tabwin->selected = head;
    for (tabwin_list = tabwin->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tabwin_widget = (TabwinWidget *) tabwin_list->data;
        for (widgets = tabwin_widget->widgets; widgets; widgets = g_list_next (widgets))
        {
            window_button = GTK_WIDGET (widgets->data);
            buttonbox = GTK_WIDGET (gtk_container_get_children (GTK_CONTAINER (window_button))[0].data );
            buttonlabel = GTK_WIDGET (g_list_nth_data (gtk_container_get_children (GTK_CONTAINER (buttonbox)), 1) );
            if (((Client *) g_object_get_data (G_OBJECT (window_button), "client-ptr-val")) == head->data)
            {
                tabwinSetSelected (tabwin_widget, window_button, buttonlabel);
                gtk_widget_queue_draw (GTK_WIDGET (tabwin_widget));
            }
        }
    }

    return tabwinGetSelected (tabwin);
}

Client *
tabwinSelectNext (Tabwin *tabwin)
{
    GList *next;

    g_return_val_if_fail (tabwin != NULL, NULL);
    TRACE ("entering tabwinSelectNext");

    next = g_list_next (tabwin->selected);
    if (!next)
    {
        next = *tabwin->client_list;
        g_return_val_if_fail (next != NULL, NULL);
    }
    return tabwinChange2Selected (tabwin, next);
}

Client *
tabwinSelectPrev (Tabwin *tabwin)
{
    GList *prev;

    g_return_val_if_fail (tabwin != NULL, NULL);
    TRACE ("entering tabwinSelectPrev");

    prev = g_list_previous (tabwin->selected);
    if (!prev)
    {
        prev = g_list_last (*tabwin->client_list);
        g_return_val_if_fail (prev != NULL, NULL);
    }
    return tabwinChange2Selected (tabwin, prev);
}

Client *
tabwinSelectDelta (Tabwin *tabwin, int row_delta, int col_delta)
{
    GList *selected;
    int pos_current, col_current, row_current, nitems, cols, rows;
    Client *c;
    ScreenInfo *screen_info;

    TRACE ("entering tabwinSelectDelta");
    g_return_val_if_fail (tabwin != NULL, NULL);

    pos_current = g_list_position (*tabwin->client_list, tabwin->selected);
    nitems = g_list_length (*tabwin->client_list);

    if (tabwin->selected)
    {
        c = (Client *) tabwin->selected->data;
        screen_info = c->screen_info;
        if (!screen_info)
        {
            return NULL;
        }
    }
    else if (tabwin->client_list)
    {
        c = (Client *) tabwin->client_list;
        screen_info = c->screen_info;
        if (!screen_info)
        {
            return NULL;
        }
    }
    else
    {
        /* There's no items? */
        return NULL;
    }

    if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
    {
        cols = MIN (nitems, tabwin->grid_cols);
        rows = (nitems - 1) / cols + 1;
        col_current = pos_current % cols;
        row_current = pos_current / cols;
    }
    else
    {
        rows = MIN (nitems, tabwin->grid_rows);
        cols = (nitems - 1) / rows + 1;
        row_current = pos_current % rows;
        col_current = pos_current / rows;
    }

    /* Wrap column */
    col_current += col_delta;
    if (col_current < 0)
    {
        col_current = cols - 1;
        row_current--;
        if (row_current < 0)
        {
            row_current = rows - 1;
        }
    }
    else if (col_current >= cols)
    {
        col_current = 0;
        if (rows > 1)
        {
            row_current++;
        }
        else
        {
            /* If there's only 1 row then col needs to wrap back to
             * the head of the grid */
            col_current = 0;
        }
    }

    /* Wrap row */
    row_current += row_delta;
    if (row_current < 0)
    {
        row_current = rows - 1;
        col_current--;
        if (col_current < 0)
        {
            col_current = cols - 1;
        }
    }
    else if (row_current >= rows)
    {
        row_current = 0;
        col_current++;
        if (col_current >= cols)
        {
            if (rows != 1)
            {
                col_current = cols - 1;
            }
            else
            {
                /* If there's only 1 row then col needs to wrap back to
                 * the head of the grid */
                col_current = 0;
            }
        }
    }

    /* So here we are at the new (wrapped) position in the rectangle */
    if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
    {
        pos_current = col_current + row_current * cols;
    }
    else
    {
        pos_current = row_current + col_current * rows;
    }

    if (pos_current >= nitems)   /* If that position does not exist */
    {
        if (col_delta)            /* Let horizontal prevail */
        {
            if (col_delta > 0)
            {
                if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
                {
                    /* In this case we're going past the tail, reset to the head
                     * of the grid */
                    col_current = 0;
                    row_current = 0;
                }
                else
                {
                    col_current = 0;
                    row_current++;
                    if (row_current >= rows)
                    {
                        row_current = 0;
                    }
                }
            }
            else
            {
                if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
                {
                    col_current = (nitems - 1) % cols;
                }
                else
                {
                    col_current = cols - 1;
                    row_current = (nitems - 1) % rows;
                }
            }
        }
        else
        {
            if (row_delta > 0)
            {
                row_current = 0;
                col_current++;
                if (col_current >= cols)
                {
                    col_current = 0;
                }
            }
            else
            {
                if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
                {
                    row_current = row_current - 1;
                }
                else
                {
                    row_current = (nitems - 1) % rows;
                    col_current = cols - 1;
                }
            }

        }

        if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
        {
            pos_current = col_current + row_current * cols;
        }
        else
        {
            pos_current = row_current + col_current * rows;
        }
    }

    selected = g_list_nth (*tabwin->client_list, pos_current);

    return tabwinChange2Selected (tabwin, selected);
}

Client*
tabwinSelectHovered (Tabwin *tabwin)
{
    GList *tabwin_list, *selected;
    TabwinWidget *tabwin_widget;
    Client *c;

    g_return_val_if_fail (tabwin != NULL, NULL);
    TRACE ("entering tabwinSelectHovered");

    for (tabwin_list = tabwin->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tabwin_widget = (TabwinWidget *) tabwin_list->data;
        if (tabwin_widget->hovered)
        {
            gtk_widget_grab_focus (tabwin_widget->hovered);
            c = g_object_get_data (G_OBJECT (tabwin_widget->hovered), "client-ptr-val");
            selected = g_list_find (*tabwin->client_list, c);
            if (selected)
            {
                tabwin->selected = selected;
            }
            return c;
        }
    }

    return tabwinSelectWidget (tabwin);
}

void
tabwinDestroy (Tabwin *tabwin)
{
    GList *tabwin_list;
    TabwinWidget *tabwin_widget;

    g_return_if_fail (tabwin != NULL);
    TRACE ("entering tabwinDestroy");

    for (tabwin_list = tabwin->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tabwin_widget = (TabwinWidget *) tabwin_list->data;
        g_list_free (tabwin_widget->widgets);
        gtk_widget_destroy (GTK_WIDGET (tabwin_widget));
    }
    g_list_free_full (tabwin->icon_list, g_object_unref);
    g_list_free (tabwin->tabwin_list);
}
