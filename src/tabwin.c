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

#ifndef LISTVIEW_WIN_ICON_SIZE
#define LISTVIEW_WIN_ICON_SIZE 24
#endif

#ifndef WIN_ICON_BORDER
#define WIN_ICON_BORDER 5
#endif

#ifndef WIN_BORDER_WIDTH
#define WIN_BORDER_WIDTH 1
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

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include "icons.h"
#include "focus.h"
#include "tabwin.h"
#include "settings.h"

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
tabwin_expose (GtkWidget *tbw, GdkEventExpose *event, gpointer data)
{
    GdkScreen *screen;
    cairo_t *cr;
    GdkColor *bg_normal = get_color(tbw, GTK_STATE_NORMAL);
    GdkColor *bg_selected = get_color(tbw, GTK_STATE_SELECTED);
    gint border_width = WIN_BORDER_WIDTH;
    gdouble border_alpha = WIN_BORDER_ALPHA;
    gdouble alpha = WIN_ALPHA;
    gint border_radius = WIN_BORDER_RADIUS;
    gdouble degrees = G_PI / 180.0;
    gdouble width = tbw->allocation.width;
    gdouble height = tbw->allocation.height;

    screen = gtk_widget_get_screen(GTK_WIDGET(tbw));
    cr = gdk_cairo_create (tbw->window);
    if (G_UNLIKELY (cr == NULL))
      return FALSE;

    gtk_widget_style_get (GTK_WIDGET (tbw),
                            "border-width", &border_width,
                            "border-alpha", &border_alpha,
                            "alpha", &alpha,
                            "border-radius", &border_radius,
                            NULL);
    cairo_set_line_width (cr, border_width);
    
    if (gdk_screen_is_composited (screen))
    {
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        gdk_cairo_region(cr, event->region);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.0);
        cairo_fill_preserve(cr);
        cairo_clip(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

        /* Draw a filled rounded rectangle with an outline */
        cairo_arc (cr, width - border_radius - 0.5, border_radius + 0.5, border_radius, -90 * degrees, 0 * degrees);
        cairo_arc (cr, width - border_radius - 0.5, height - border_radius - 0.5, border_radius, 0 * degrees, 90 * degrees);
        cairo_arc (cr, border_radius + 0.5, height - border_radius - 0.5, border_radius, 90 * degrees, 180 * degrees);
        cairo_arc (cr, border_radius + 0.5, border_radius + 0.5, border_radius, 180 * degrees, 270 * degrees);
        cairo_close_path(cr);
        cairo_set_source_rgba (cr, bg_normal->red/65535.0, bg_normal->green/65535.0, bg_normal->blue/65535.0, alpha);
        cairo_fill_preserve (cr);
        cairo_set_source_rgba (cr, bg_selected->red/65535.0, bg_selected->green/65535.0, bg_selected->blue/65535.0, border_alpha);
        cairo_stroke (cr);
    }
    else
    {
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_rectangle(cr, 0, 0, width, height);
        gdk_cairo_set_source_color(cr, bg_normal);
        cairo_fill_preserve (cr);
        gdk_cairo_set_source_color(cr, bg_selected);
        cairo_stroke (cr);
    }
    
    cairo_destroy (cr);
    return FALSE;
}

static gboolean
paint_selected (GtkWidget *w, GdkEventExpose *event, gpointer data)
{
    g_return_val_if_fail (GTK_IS_WIDGET(w), FALSE);
    TRACE ("entering paint_selected");

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
tabwinSetLabel (TabwinWidget *tbw, GtkWidget *buttonlabel, gchar *class, gchar *label, int workspace)
{
    gchar *message;
    PangoLayout *layout;

    g_return_if_fail (tbw);
    TRACE ("entering tabwinSetLabel");

    message = pretty_string (class);
    gtk_label_set_text (GTK_LABEL (buttonlabel), message);
    g_free (message);

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
tabwinSetSelected (TabwinWidget *tbw, GtkWidget *w, GtkWidget *l)
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

    if (c != NULL)
    {
        /* We don't update labels here */
        if (c->screen_info->params->cycle_tabwin_mode == OVERFLOW_COLUMN_GRID)
        {
            return;
        }

        classname = g_strdup(c->class.res_class);
        tabwinSetLabel (tbw, l, classname, c->name, c->win_workspace);
        g_free (classname);
    }
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

    if (icon_pixbuf)
    {
        if (FLAG_TEST (c->flags, CLIENT_FLAG_ICONIFIED))
        {
            icon_pixbuf_stated = gdk_pixbuf_copy (icon_pixbuf);
            gdk_pixbuf_saturate_and_pixelate (icon_pixbuf, icon_pixbuf_stated, 0.55, TRUE);
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
            min_height = monitor.height;
    }
    return min_height;
}

static gboolean
cb_window_button_enter (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    TabwinWidget *tbw = user_data;
    Client *c;
    GtkWidget *buttonbox, *buttonlabel;
    gchar *classname;

    TRACE ("entering");

    g_return_val_if_fail (tbw != NULL, FALSE);

    /* keep track of which widget we're hovered over */
    tbw->tabwin->hovered = widget;

    c = g_object_get_data (G_OBJECT (widget), "client-ptr-val");

    /* when hovering over a window icon, display it's label but don't
     * select it */
    if (c != NULL)
    {
        /* we don't update the labels on mouse over for this mode */
        if (c->screen_info->params->cycle_tabwin_mode == OVERFLOW_COLUMN_GRID)
        {
            return FALSE;
        }

        buttonbox = GTK_WIDGET( gtk_container_get_children(GTK_CONTAINER(widget))[0].data );
        buttonlabel = GTK_WIDGET( g_list_nth_data( gtk_container_get_children(GTK_CONTAINER(buttonbox)), 1) );

        classname = g_strdup(c->class.res_class);
        tabwinSetLabel (tbw, buttonlabel, classname, c->name, c->win_workspace);
        g_free (classname);
    }

    return FALSE;
}

static gboolean
cb_window_button_leave (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    TabwinWidget *tbw = user_data;
    Client *c;

    TRACE ("entering");

    g_return_val_if_fail (tbw != NULL, FALSE);

    /* don't do anything if we have the focus */
    if (gtk_widget_is_focus (widget))
    {
        return FALSE;
    }

    tbw->tabwin->hovered = NULL;

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
        tabwinSelectWidget (tbw->tabwin);
    }

    return FALSE;
}

static GtkWidget *
createWindowlist (ScreenInfo *screen_info, TabwinWidget *tbw)
{
    Client *c;
    GList *client_list;
    GtkWidget *windowlist, *icon, *selected, *window_button, *buttonbox, *buttonlabel, *selected_label;
    int packpos, monitor_width, monitor_height, app_label_height;
    Tabwin *t;
    PangoLayout *layout;
    gint icon_size = WIN_ICON_SIZE;

    TRACE ("entering createWindowlist");

    packpos = 0;
    c = NULL;
    selected = NULL;
    t = tbw->tabwin;
    monitor_width = getMinMonitorWidth (screen_info);
    monitor_height = getMinMonitorHeight (screen_info);
    g_return_val_if_fail (screen_info->client_count > 0, NULL);

    gtk_widget_style_get (GTK_WIDGET (tbw), "icon-size", &icon_size, NULL);
    tbw->widgets = NULL;

    if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
    {
        tbw->grid_cols = (monitor_width / (icon_size + 2 * WIN_ICON_BORDER)) * 0.75;
        tbw->grid_rows = screen_info->client_count / tbw->grid_cols + 1;
    }
    else
    {
        icon_size = LISTVIEW_WIN_ICON_SIZE;
        gtk_widget_style_get (GTK_WIDGET (tbw), "listview-icon-size", &icon_size, NULL);
        tbw->grid_rows = (monitor_height / (icon_size + 2 * WIN_ICON_BORDER)) * 0.75;
        tbw->grid_cols = screen_info->client_count / tbw->grid_rows + 1;
    }

    windowlist = gtk_table_new (tbw->grid_rows, tbw->grid_cols, FALSE);

    /* pack the client icons */
    for (client_list = *t->client_list; client_list; client_list = g_list_next (client_list))
    {
        c = (Client *) client_list->data;
        TRACE ("createWindowlist: adding %s", c->name);       

        window_button = gtk_button_new ();
        gtk_button_set_relief (GTK_BUTTON (window_button), GTK_RELIEF_NONE);
        g_object_set_data (G_OBJECT (window_button), "client-ptr-val", c);
        g_signal_connect (window_button, "enter-notify-event", G_CALLBACK (cb_window_button_enter), tbw);
        g_signal_connect (window_button, "leave-notify-event", G_CALLBACK (cb_window_button_leave), tbw);
        gtk_widget_add_events (window_button, GDK_ENTER_NOTIFY_MASK);

        if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
        {
            /* We need to account for changes to the font size in the user's
             * appearance theme and gtkrc settings */
            layout = gtk_widget_create_pango_layout (GTK_WIDGET (tbw), "");
            pango_layout_get_pixel_size (layout, NULL, &app_label_height);

            gtk_widget_set_size_request (GTK_WIDGET (window_button), icon_size+24, icon_size+app_label_height+8);
            buttonbox = gtk_vbox_new (FALSE, 0);
            buttonlabel = gtk_label_new ("");
            gtk_misc_set_alignment (GTK_MISC (buttonlabel), 0.5, 1.0);
        }
        else
        {
            gtk_widget_set_size_request (GTK_WIDGET (window_button), icon_size+256, icon_size+8);
            buttonbox = gtk_hbox_new (FALSE, 6);
            buttonlabel = gtk_label_new (c->name);
            gtk_misc_set_alignment (GTK_MISC (buttonlabel), 0, 0.5);
        }

        gtk_container_add (GTK_CONTAINER (window_button), buttonbox);
        
        icon = createWindowIcon (c, icon_size);
        gtk_box_pack_start (GTK_BOX (buttonbox), icon, FALSE, TRUE, 0);

        gtk_label_set_justify (GTK_LABEL (buttonlabel), GTK_JUSTIFY_CENTER);
        gtk_label_set_ellipsize (GTK_LABEL (buttonlabel), PANGO_ELLIPSIZE_END);
        gtk_box_pack_start (GTK_BOX (buttonbox), buttonlabel, TRUE, TRUE, 0);

        if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
        {
            gtk_table_attach (GTK_TABLE (windowlist), GTK_WIDGET (window_button),
                packpos % tbw->grid_cols, packpos % tbw->grid_cols + 1,
                packpos / tbw->grid_cols, packpos / tbw->grid_cols + 1,
                GTK_FILL, GTK_FILL, 2, 2);
        }
        else
        {
            gtk_table_attach (GTK_TABLE (windowlist), GTK_WIDGET (window_button),
                packpos / tbw->grid_rows, packpos / tbw->grid_rows + 1,
                packpos % tbw->grid_rows, packpos % tbw->grid_rows + 1,
                GTK_FILL, GTK_FILL, 2, 2);
        }

        tbw->widgets = g_list_append (tbw->widgets, window_button);
        packpos++;
        if (c == t->selected->data)
        {
            selected = window_button;
            selected_label = buttonlabel;
            gtk_widget_grab_focus (selected);
        }
    }
    if (selected)
    {
        tabwinSetSelected (tbw, selected, selected_label);
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
                                             g_param_spec_int("listview-icon-size",
                                                              "listview icon size",
                                                               "the size of the application icon in listview",
                                                               16, 48,
                                                               LISTVIEW_WIN_ICON_SIZE,
                                                               G_PARAM_READABLE));
    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_int("border-width",
                                                              "border width",
                                                               "the width of the colored border",
                                                               0, 8,
                                                               WIN_BORDER_WIDTH,
                                                               G_PARAM_READABLE));
    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_double("border-alpha",
                                                              "border alpha",
                                                               "the alpha of the colored border",
                                                               0.0, 1.0,
                                                               WIN_BORDER_ALPHA,
                                                               G_PARAM_READABLE));
    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_double("alpha",
                                                              "alpha",
                                                               "the alpha of the window",
                                                               0.0, 1.0,
                                                               WIN_ALPHA,
                                                               G_PARAM_READABLE));
    gtk_widget_class_install_style_property (widget_class,
                                             g_param_spec_int("border-radius",
                                                              "border radius",
                                                               "the border radius of the window",
                                                               0, 20,
                                                               WIN_BORDER_RADIUS,
                                                               G_PARAM_READABLE));
}

static TabwinWidget *
tabwinCreateWidget (Tabwin *tabwin, ScreenInfo *screen_info, gint monitor_num)
{
    TabwinWidget *tbw;
    GdkScreen *screen;
    GtkWidget *vbox;
    GtkWidget *windowlist;
    GdkRectangle monitor;
    gint border_radius = WIN_BORDER_RADIUS;

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

    /* Check for compositing and set colormap for it */
    screen = gtk_widget_get_screen(GTK_WIDGET(tbw));
    if(gdk_screen_is_composited(screen)) {
        GdkColormap *cmap = gdk_screen_get_rgba_colormap(screen);
        if(cmap)
            gtk_widget_set_colormap(GTK_WIDGET(tbw), cmap);
    }

    gtk_widget_style_get(GTK_WIDGET(tbw), "border-radius", &border_radius, NULL);
    gtk_widget_realize (GTK_WIDGET (tbw));
    gtk_container_set_border_width (GTK_CONTAINER (tbw), border_radius + 4);
    gtk_widget_set_app_paintable(GTK_WIDGET(tbw), TRUE);
    gtk_window_set_position (GTK_WINDOW (tbw), GTK_WIN_POS_NONE);
    gdk_screen_get_monitor_geometry (screen_info->gscr, tbw->monitor_num, &monitor);
    gtk_window_move (GTK_WINDOW(tbw), monitor.x + monitor.width / 2,
                                      monitor.y + monitor.height / 2);

    vbox = gtk_vbox_new (FALSE, 3);
    gtk_container_add (GTK_CONTAINER (tbw), vbox);

    if (screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID)
    {
        tbw->label = gtk_label_new ("");
        gtk_label_set_use_markup (GTK_LABEL (tbw->label), TRUE);
        gtk_label_set_justify (GTK_LABEL (tbw->label), GTK_JUSTIFY_CENTER);
        gtk_label_set_ellipsize (GTK_LABEL (tbw->label), PANGO_ELLIPSIZE_END);
        gtk_box_pack_end (GTK_BOX (vbox), tbw->label, TRUE, TRUE, 0);
    }

    windowlist = createWindowlist (screen_info, tbw);
    tbw->container = windowlist;
    gtk_box_pack_start (GTK_BOX (vbox), windowlist, TRUE, TRUE, 0);

    g_signal_connect_swapped (tbw, "configure-event",
                              GTK_SIGNAL_FUNC (tabwinConfigure), (gpointer) tbw);
    g_signal_connect (tbw, "expose-event", GTK_SIGNAL_FUNC (tabwin_expose), (gpointer) tbw);

    gtk_widget_show_all (GTK_WIDGET (tbw));

    return tbw;
}

static Client *
tabwinChange2Selected (Tabwin *t, GList *selected)
{
    GList *tabwin_list, *widgets;
    GtkWidget *window_button, *buttonbox, *buttonlabel;
    TabwinWidget *tbw;
    Client *c;

    t->selected = selected;
    for (tabwin_list = t->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tbw = (TabwinWidget *) tabwin_list->data;
        for (widgets = tbw->widgets; widgets; widgets = g_list_next (widgets))
        {
            window_button = GTK_WIDGET (widgets->data);
            gtk_button_set_relief (GTK_BUTTON (window_button), GTK_RELIEF_NONE);
            buttonbox = GTK_WIDGET( gtk_container_get_children(GTK_CONTAINER(window_button))[0].data );
            buttonlabel = GTK_WIDGET( g_list_nth_data( gtk_container_get_children(GTK_CONTAINER(buttonbox)), 1) );

            c = g_object_get_data (G_OBJECT (window_button), "client-ptr-val");

            if (c != NULL)
            {
                /* don't clear label if mouse is inside the previously
                 * selected button */
                if (c->screen_info->params->cycle_tabwin_mode == STANDARD_ICON_GRID
                    && window_button != t->hovered)
                {
                    gtk_label_set_text (GTK_LABEL (buttonlabel), "");
                }

                if (c == t->selected->data)
                {
                    gtk_widget_grab_focus (window_button);
                    tabwinSetSelected (tbw, window_button, buttonlabel);
                    gtk_widget_queue_draw (GTK_WIDGET(tbw));
                }
            }
        }
    }
    return tabwinGetSelected (t);
}

Tabwin *
tabwinCreate (GList **client_list, GList *selected, gboolean display_workspace)
{
    ScreenInfo *screen_info;
    Client *c;
    Tabwin *tabwin;
    int num_monitors, i;
    TabwinWidget *win;

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
        win = tabwinCreateWidget (tabwin, screen_info, monitor_index);
        tabwin->tabwin_list  = g_list_append (tabwin->tabwin_list, win);
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
    GtkWidget *window_button, *buttonbox, *buttonlabel;
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
            window_button = GTK_WIDGET (widgets->data);
            buttonbox = GTK_WIDGET( gtk_container_get_children(GTK_CONTAINER(window_button))[0].data );
            buttonlabel = GTK_WIDGET( g_list_nth_data( gtk_container_get_children(GTK_CONTAINER(buttonbox)), 1) );
            if (((Client *) g_object_get_data (G_OBJECT(window_button), "client-ptr-val")) == head->data)
            {
                tabwinSetSelected (tbw, window_button, buttonlabel);
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

    g_return_val_if_fail (t != NULL, NULL);
    TRACE ("entering tabwinSelectNext");

    next = g_list_next(t->selected);
    if (!next)
    {
        next = *t->client_list;
        g_return_val_if_fail (next != NULL, NULL);
    }
    return tabwinChange2Selected (t, next);
}

Client *
tabwinSelectPrev (Tabwin *t)
{
    GList *prev;

    g_return_val_if_fail (t != NULL, NULL);
    TRACE ("entering tabwinSelectPrev");

    prev = g_list_previous (t->selected);
    if (!prev)
    {
        prev = g_list_last (*t->client_list);
        g_return_val_if_fail (prev != NULL, NULL);
    }
    return tabwinChange2Selected (t, prev);
}

Client *
tabwinSelectDelta (Tabwin *t, int row_delta, int col_delta)
{
    GList *selected;
    int pos_current, col_current, row_current, nitems, cols, rows;
    TabwinWidget *tbw;
    Client *c;
    ScreenInfo *screen_info;

    TRACE ("entering tabwinSelectDelta");
    g_return_val_if_fail (t != NULL, NULL);

    tbw = (TabwinWidget *) t->tabwin_list->data;
    pos_current = g_list_position (*t->client_list, t->selected);
    nitems = g_list_length (*t->client_list);

    if (t->selected)
    {
        c = (Client *)t->selected->data;
        screen_info = c->screen_info;
        if (!screen_info)
        {
            return NULL;
        }
    }
    else if (t->client_list)
    {
        c = (Client *)t->client_list;
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
        cols = MIN (nitems, tbw->grid_cols);
        rows = (nitems - 1) / cols + 1;
        col_current = pos_current % cols;
        row_current = pos_current / cols;
    }
    else
    {
        rows = MIN (nitems, tbw->grid_rows);
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
        row_current++;
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

    selected = g_list_nth(*t->client_list, pos_current);

    return tabwinChange2Selected (t, selected);
}

Client*
tabwinSelectWidget (Tabwin *t)
{
    GList *tabwin_list, *widgets, *selected;
    GtkWidget *window_button, *buttonbox, *buttonlabel;
    TabwinWidget *tbw;
    Client *c;

    g_return_val_if_fail (t != NULL, NULL);
    TRACE ("entering tabwinSelectWidget");

    for (tabwin_list = t->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tbw = (TabwinWidget *) tabwin_list->data;
        for (widgets = tbw->widgets; widgets; widgets = g_list_next (widgets))
        {
            window_button = GTK_WIDGET (widgets->data);
            gtk_button_set_relief (GTK_BUTTON (window_button), GTK_RELIEF_NONE);
            buttonbox = GTK_WIDGET( gtk_container_get_children(GTK_CONTAINER (window_button))[0].data );
            buttonlabel = GTK_WIDGET( g_list_nth_data( gtk_container_get_children (GTK_CONTAINER(buttonbox)), 1) );
            gtk_label_set_text (GTK_LABEL (buttonlabel), "");

            if (gtk_widget_is_focus (window_button))
            {
                c = g_object_get_data (G_OBJECT (window_button), "client-ptr-val");
                selected = g_list_find (*t->client_list, c);

                if (selected)
                {
                    t->selected = selected;
                }

                tabwinSetSelected (tbw, window_button, buttonlabel);
                gtk_widget_queue_draw (GTK_WIDGET (tbw));
            }
        }
    }

    return tabwinGetSelected (t);
}

Client*
tabwinSelectHoveredWidget (Tabwin *t)
{
    TRACE ("entering");

    g_return_val_if_fail (t != NULL, NULL);

    if (!t->hovered)
    {
        return NULL;
    }

    gtk_widget_grab_focus (t->hovered);

    return tabwinSelectWidget (t);
}

void
tabwinDestroy (Tabwin *t)
{
    GList *tabwin_list;
    TabwinWidget *tbw;

    g_return_if_fail (t != NULL);
    TRACE ("entering tabwinDestroy");

    for (tabwin_list = t->tabwin_list; tabwin_list; tabwin_list = g_list_next (tabwin_list))
    {
        tbw = (TabwinWidget *) tabwin_list->data;
        g_list_free (tbw->widgets);
        gtk_widget_destroy (GTK_WIDGET (tbw));
    }
    g_list_free (t->tabwin_list);
}

