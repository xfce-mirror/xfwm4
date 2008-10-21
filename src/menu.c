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

        Metacity - (c) 2001 Havoc Pennington
        xfwm4    - (c) 2002-2006 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <string.h>
#include <libxfce4util/libxfce4util.h>

#include "event_filter.h"
#include "misc.h"
#include "menu.h"

static GtkWidget *menu_open = NULL;
static MenuItem menuitems[] = {
    {MENU_OP_MAXIMIZE, "gtk-zoom-100", N_("Ma_ximize")},
    {MENU_OP_UNMAXIMIZE, "gtk-zoom-out", N_("Un_maximize")},
    {MENU_OP_MINIMIZE, "gtk-undo", N_("_Hide")},
    {MENU_OP_MINIMIZE_ALL, "gtk-clear", N_("Hide _all others")},
    {MENU_OP_UNMINIMIZE, "gtk-add", N_("S_how")},
    {MENU_OP_SHADE, "gtk-goto-top", N_("_Shade")},
    {MENU_OP_UNSHADE, "gtk-goto-bottom", N_("Un_shade")},
    {MENU_OP_STICK, "gtk-add", N_("S_tick")},
    {MENU_OP_UNSTICK, "gtk-remove", N_("Uns_tick")},
    {MENU_OP_CONTEXT_HELP, "gtk-help", N_("Context _help")},
    {MENU_OP_ABOVE, NULL, N_("Always on top")},
    {MENU_OP_NORMAL, "gtk-apply", N_("Always on top")},
    {MENU_OP_WORKSPACES, NULL, N_("Send to...")},
    {0, NULL, NULL},
    {MENU_OP_DELETE, "gtk-close", N_("_Close")},
#if 0
    {0,, NULL NULL},
    {MENU_OP_DESTROY, "gtk-delete", N_("Destroy")},
    {0, NULL, NULL},
#endif
    {MENU_OP_QUIT, "gtk-quit", N_("_Quit")},
    {MENU_OP_RESTART, "gtk-refresh", N_("Restart")},
};

static eventFilterStatus
menu_filter (XEvent * xevent, gpointer data)
{
    switch (xevent->type)
    {
        case KeyPress:
        case KeyRelease:
        case ButtonPress:
        case ButtonRelease:
        case MotionNotify:
        case EnterNotify:
        case LeaveNotify:
            return EVENT_FILTER_STOP;
            break;
        default:
            break;
    }
    return EVENT_FILTER_CONTINUE;
}


static void
popup_position_func (GtkMenu * menu, gint * x, gint * y, gboolean * push_in,
    gpointer user_data)
{
    GtkRequisition req;
    GdkPoint *pos;
    gint monitor;
    GdkScreen *screen;

    pos = user_data;

    gtk_widget_size_request (GTK_WIDGET (menu), &req);

    if (pos->x >= 0)
    {
        *x = pos->x;
        *x = CLAMP (*x, 0, MAX (0, gdk_screen_width () - req.width));
    }
    else
    {
        *x = (gdk_screen_width () - req.width) / 2;
    }
    if (pos->x >= 0)
    {
        *y = pos->y;
        *y = CLAMP (*y, 0, MAX (0, gdk_screen_height () - req.height));
    }
    else
    {
        *y = (gdk_screen_height () - req.height) / 2;
    }

    screen = gtk_widget_get_screen (GTK_WIDGET(menu));
    monitor = find_monitor_at_point (screen, *x, *y);
    gtk_menu_set_monitor (GTK_MENU (menu), monitor);

    g_free (user_data);
}

static gboolean
activate_cb (GtkWidget * menuitem, gpointer data)
{
    MenuData *menudata;

    TRACE ("entering activate_cb");
    g_return_val_if_fail (GTK_IS_WIDGET (menuitem), FALSE);

    menu_open = NULL;

    menudata = data;

    TRACE ("deactivating menu_filter");
    eventFilterPop (menudata->menu->filter_setup);
    (*menudata->menu->func) (menudata->menu,
                             menudata->op,
                             menudata->menu->xid,
                             menudata->menu->data,
                             menudata->data);
    return (FALSE);
}

static gboolean
menu_closed (GtkMenu * widget, gpointer data)
{
    Menu *menu;

    TRACE ("entering menu_closed");
    menu = data;
    menu_open = NULL;
    TRACE ("deactivating menu_filter");
    eventFilterPop (menu->filter_setup);
    (*menu->func) (menu, 0, menu->xid, menu->data, NULL);
    return (FALSE);
}

static GtkWidget *
menu_workspace (Menu * menu, MenuOp insensitive, gint ws, gint nws, gchar **wsn, gint wsn_items)
{
    GtkWidget *menu_widget;
    GtkWidget *menuitem;
    MenuData *menudata;
    gchar *name;
    gint i;

    menu_widget = gtk_menu_new ();
    gtk_menu_set_screen (GTK_MENU (menu->menu), menu->screen);

    for (i = 0; i < nws; i++)
    {
        if ((i < wsn_items) && wsn[i])
        {
            name = g_strdup_printf (_("Workspace %i (%s)"), i+ 1, wsn[i]);
        }
        else
        {
            name = g_strdup_printf (_("Workspace %i"), i + 1);
        }
        menuitem = gtk_check_menu_item_new_with_label (name);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), (i == ws));
        gtk_widget_show (menuitem);
        if (insensitive & MENU_OP_WORKSPACES)
        {
            gtk_widget_set_sensitive (menuitem, FALSE);
        }
        g_free (name);

        menudata = g_new (MenuData, 1);
        menudata->menu = menu;
        menudata->op = MENU_OP_WORKSPACES;
        menudata->data = GINT_TO_POINTER (i);
        menu_item_connect (menuitem, menudata);

        gtk_menu_shell_append (GTK_MENU_SHELL (menu_widget), menuitem);
    }

    return (menu_widget);
}

Menu *
menu_default (GdkScreen *gscr, Window xid, MenuOp ops, MenuOp insensitive, MenuFunc func,
    gint ws, gint nws, gchar **wsn, gint wsn_items, eventFilterSetup *filter_setup, gpointer data)
{
    GtkWidget *menuitem;
    GtkWidget *image;
    GtkWidget *ws_menu;
    MenuData *menudata;
    Menu *menu;
    const gchar *label;
    int i;

    TRACE ("entering menu_new");
    menu = g_new (Menu, 1);
    menu->func = func;
    menu->filter_setup = filter_setup;
    menu->data = data;
    menu->ops = ops;
    menu->insensitive = insensitive;
    menu->screen = gscr;
    menu->xid = xid;
    menu->menu = gtk_menu_new ();
    gtk_menu_set_screen (GTK_MENU (menu->menu), menu->screen);

    i = 0;
    while (i < (int) (sizeof (menuitems) / sizeof (MenuItem)))
    {
        if ((ops & menuitems[i].op) || (menuitems[i].op == MENU_OP_SEPARATOR))
        {
            label = _(menuitems[i].label);
            ws_menu = NULL;
            switch (menuitems[i].op)
            {
                case MENU_OP_SEPARATOR:
                    menuitem = gtk_separator_menu_item_new ();
                    break;
                case MENU_OP_WORKSPACES:
                    menuitem = gtk_menu_item_new_with_mnemonic (label);
                    if (insensitive & menuitems[i].op)
                    {
                        gtk_widget_set_sensitive (menuitem, FALSE);
                    }
                    ws_menu = menu_workspace (menu, insensitive, ws, nws, wsn, wsn_items);
                    gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), ws_menu);
                    break;
                default:
                    if (menuitems[i].image_name)
                    {
                        menuitem = gtk_image_menu_item_new_with_mnemonic (label);
                        image =
                            gtk_image_new_from_stock (menuitems[i].image_name,
                            GTK_ICON_SIZE_MENU);
                        gtk_widget_show (image);
                        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
                    }
                    else
                    {
                        menuitem = gtk_menu_item_new_with_mnemonic (label);
                    }
                    if (insensitive & menuitems[i].op)
                    {
                        gtk_widget_set_sensitive (menuitem, FALSE);
                    }
                    menudata = g_new (MenuData, 1);
                    menudata->menu = menu;
                    menudata->op = menuitems[i].op;
                    menudata->data = data;
                    menu_item_connect (menuitem, menudata);
                    break;
            }
            gtk_menu_shell_append (GTK_MENU_SHELL (menu->menu), menuitem);
            gtk_widget_show (menuitem);
        }
        ++i;
    }
    menu_connect (menu);

    return (menu);
}

Menu *
menu_connect (Menu * menu)
{
    TRACE ("entering menu_connect");
    g_return_val_if_fail (menu != NULL, NULL);
    g_return_val_if_fail (GTK_IS_MENU (menu->menu), NULL);
    g_signal_connect (GTK_OBJECT (menu->menu), "selection_done",
        GTK_SIGNAL_FUNC (menu_closed), menu);
    return (menu);
}

static void
closure_notify (gpointer data, GClosure * closure)
{
    TRACE ("entering closure_notify");
    if (data)
    {
        TRACE ("freeing data");
        g_free (data);
    }
}

GtkWidget *
menu_item_connect (GtkWidget * item, MenuData * item_data)
{
    TRACE ("entering menu_item_connect");
    g_return_val_if_fail (item != NULL, NULL);
    g_return_val_if_fail (GTK_IS_MENU_ITEM (item), NULL);
    g_signal_connect_closure (GTK_OBJECT (item), "activate",
        g_cclosure_new (GTK_SIGNAL_FUNC (activate_cb), item_data,
            (GClosureNotify) closure_notify), FALSE);
    return (item);
}

gboolean
menu_is_opened (void)
{
    TRACE ("entering menu_is_opened");
    return (menu_open != NULL);
}

gboolean
menu_check_and_close (void)
{
    TRACE ("entering menu_check_or_close");
    if (menu_open)
    {
        TRACE ("menu open, emitting deactivate signal");
        g_signal_emit_by_name (GTK_OBJECT (menu_open), "deactivate");
        menu_open = NULL;
        return (TRUE);
    }
    return (FALSE);
}

static gboolean
grab_available (GdkWindow *win, guint32 timestamp)
{
    GdkEventMask mask;
    GdkGrabStatus g1;
    GdkGrabStatus g2;
    gboolean grab_failed;
    gint i;

    TRACE ("entering grab_available");

    mask = GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
           GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
           GDK_POINTER_MOTION_MASK;
    g1 = gdk_pointer_grab (win, TRUE, mask, NULL, NULL, timestamp);
    g2 = gdk_keyboard_grab (win, TRUE, timestamp);
    grab_failed = FALSE;

    i = 0;
    while ((i++ < 100) && (grab_failed = ((g1 != GDK_GRAB_SUCCESS)
                || (g2 != GDK_GRAB_SUCCESS))))
    {
        TRACE ("grab not available yet, waiting... (%i)", i);
        g_usleep (100);
        if (g1 != GDK_GRAB_SUCCESS)
        {
            g1 = gdk_pointer_grab (win, TRUE, mask, NULL, NULL, timestamp);
        }
        if (g2 != GDK_GRAB_SUCCESS)
        {
            g2 = gdk_keyboard_grab (win, TRUE, timestamp);
        }
    }

    if (g1 == GDK_GRAB_SUCCESS)
    {
        gdk_pointer_ungrab (timestamp);
    }
    if (g2 == GDK_GRAB_SUCCESS)
    {
        gdk_keyboard_ungrab (timestamp);
    }

    return (!grab_failed);
}

gboolean
menu_popup (Menu * menu, int root_x, int root_y, int button,
    guint32 timestamp)
{
    GdkPoint *pt;

    TRACE ("entering menu_popup");

    g_return_val_if_fail (menu != NULL, FALSE);
    g_return_val_if_fail (GTK_IS_MENU (menu->menu), FALSE);

    pt = g_new (GdkPoint, 1);
    pt->x = root_x;
    pt->y = root_y;

    if (!menu_check_and_close ())
    {
        if (!grab_available (gdk_screen_get_root_window (menu->screen), timestamp))
        {
            g_free (pt);
            TRACE ("Cannot get grab on pointer/keyboard, cancel.");
            return FALSE;
        }
        TRACE ("opening new menu");
        menu_open = menu->menu;
        eventFilterPush (menu->filter_setup, menu_filter, NULL);
        gtk_menu_popup (GTK_MENU (menu->menu), NULL, NULL,
            popup_position_func, pt, 0, timestamp);

        if (!GTK_MENU_SHELL (GTK_MENU (menu->menu))->have_xgrab)
        {
            gdk_beep ();
            g_message (_("%s: GtkMenu failed to grab the pointer\n"), g_get_prgname ());
            gtk_menu_popdown (GTK_MENU (menu->menu));
            menu_open = NULL;
            eventFilterPop (menu->filter_setup);
            return FALSE;
        }
    }
    return TRUE;
}

void
menu_free (Menu * menu)
{
    TRACE ("entering menu_free");

    g_return_if_fail (menu != NULL);
    g_return_if_fail (menu->menu != NULL);
    g_return_if_fail (GTK_IS_MENU (menu->menu));

    TRACE ("freeing menu");

    gtk_widget_destroy (menu->menu);
    g_free (menu);
}
