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
 
        Metacity - (c) 2001 Havoc Pennington
        xfwm4    - (c) 2002-2003 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef GDK_MULTIHEAD_SAFE
#undef GDK_MULTIHEAD_SAFE
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <unistd.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfcegui4/libxfcegui4.h>
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

static GtkToXEventFilterStatus
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
            return XEV_FILTER_STOP;
            break;
        default:
            return XEV_FILTER_CONTINUE;
            break;
    }
    return XEV_FILTER_STOP;
}


static void
popup_position_func (GtkMenu * menu, gint * x, gint * y, gboolean * push_in,
    gpointer user_data)
{
    GtkRequisition req;
    GdkPoint *pos;

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
    g_free (user_data);
}

static gboolean
activate_cb (GtkWidget * menuitem, gpointer data)
{
    MenuData *md;

    TRACE ("entering activate_cb");
    g_return_val_if_fail (GTK_IS_WIDGET (menuitem), FALSE);

    menu_open = NULL;

    md = data;

    TRACE ("deactivating menu_filter");
    popEventFilter ();
    (*md->menu->func) (md->menu, md->op, md->client_xwindow, md->menu->data,
        md->data);
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
    popEventFilter ();
    (*menu->func) (menu, 0, None, menu->data, NULL);
    return (FALSE);
}

static GtkWidget *
menu_workspace (Menu * menu, MenuOp insensitive, gint ws, gint nws)
{
    gint i;
    GtkWidget *menu_widget;
    GtkWidget *mi;
    MenuData *md;
    gchar *name;

    menu_widget = gtk_menu_new ();

    for (i = 0; i < nws; i++)
    {
        name = g_strdup_printf (_("Workspace %i"), i + 1);
        mi = gtk_check_menu_item_new_with_label (name);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mi), (i == ws));
        gtk_widget_show (mi);
        if (insensitive & MENU_OP_WORKSPACES)
        {
            gtk_widget_set_sensitive (mi, FALSE);
        }
        g_free (name);

        md = g_new (MenuData, 1);
        md->menu = menu;
        md->op = MENU_OP_WORKSPACES;
        md->client_xwindow = None;
        md->data = GINT_TO_POINTER (i);
        menu_item_connect (mi, md);

        gtk_menu_shell_append (GTK_MENU_SHELL (menu_widget), mi);
    }

    return (menu_widget);
}

Menu *
menu_default (MenuOp ops, MenuOp insensitive, MenuFunc func, gint ws,
    gint nws, gpointer data)
{
    int i;
    Menu *menu;

    TRACE ("entering menu_new");
    menu = g_new (Menu, 1);
    menu->func = func;
    menu->data = data;
    menu->ops = ops;
    menu->insensitive = insensitive;
    menu->menu = gtk_menu_new ();

    i = 0;
    while (i < (int) (sizeof (menuitems) / sizeof (MenuItem)))
    {
        if ((ops & menuitems[i].op) || (menuitems[i].op == MENU_OP_SEPARATOR))
        {
            GtkWidget *mi;
            GtkWidget *image;
            GtkWidget *ws_menu;
            MenuData *md;
            const gchar *label;

            label = _(menuitems[i].label);
            ws_menu = NULL;
            switch (menuitems[i].op)
            {
                case MENU_OP_SEPARATOR:
                    mi = gtk_separator_menu_item_new ();
                    break;
                case MENU_OP_WORKSPACES:
                    mi = gtk_menu_item_new_with_mnemonic (label);
                    if (insensitive & menuitems[i].op)
                    {
                        gtk_widget_set_sensitive (mi, FALSE);
                    }
                    ws_menu = menu_workspace (menu, insensitive, ws, nws);
                    gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), ws_menu);

                    md = g_new (MenuData, 1);
                    md->menu = menu;
                    md->op = menuitems[i].op;
                    md->client_xwindow = None;
                    md->data = NULL;
                    break;
                default:
                    if (menuitems[i].image_name)
                    {
                        mi = gtk_image_menu_item_new_with_mnemonic (label);
                        image =
                            gtk_image_new_from_stock (menuitems[i].image_name,
                            GTK_ICON_SIZE_MENU);
                        gtk_widget_show (image);
                        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM
                            (mi), image);

                    }
                    else
                    {
                        mi = gtk_menu_item_new_with_mnemonic (label);
                    }
                    if (insensitive & menuitems[i].op)
                    {
                        gtk_widget_set_sensitive (mi, FALSE);
                    }
                    md = g_new (MenuData, 1);
                    md->menu = menu;
                    md->op = menuitems[i].op;
                    md->client_xwindow = None;
                    md->data = NULL;
                    menu_item_connect (mi, md);
                    break;
            }
            gtk_menu_shell_append (GTK_MENU_SHELL (menu->menu), mi);
            gtk_widget_show (mi);
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
grab_available (guint32 timestamp)
{
    GdkEventMask mask =
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
        GDK_POINTER_MOTION_MASK;
    GdkGrabStatus g1;
    GdkGrabStatus g2;
    gboolean grab_failed = FALSE;
    gint i = 0;

    TRACE ("entering grab_available");

    g1 = gdk_pointer_grab (getGdkEventWindow (), TRUE, mask, NULL, NULL,
        timestamp);
    g2 = gdk_keyboard_grab (getGdkEventWindow (), TRUE, timestamp);

    while ((i++ < 100) && (grab_failed = ((g1 != GDK_GRAB_SUCCESS)
                || (g2 != GDK_GRAB_SUCCESS))))
    {
        TRACE ("grab not available yet, waiting... (%i)", i);
        usleep (100);
        if (g1 != GDK_GRAB_SUCCESS)
        {
            g1 = gdk_pointer_grab (getGdkEventWindow (), TRUE, mask, NULL,
                NULL, timestamp);
        }
        if (g2 != GDK_GRAB_SUCCESS)
        {
            g2 = gdk_keyboard_grab (getGdkEventWindow (), TRUE, timestamp);
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
        if (!grab_available (timestamp))
        {
            g_free (pt);
            TRACE ("Cannot get grab on pointer/keyboard, cancel.");
            return FALSE;
        }
        TRACE ("opening new menu");
        menu_open = menu->menu;
        pushEventFilter (menu_filter, NULL);
        gtk_menu_popup (GTK_MENU (menu->menu), NULL, NULL,
            popup_position_func, pt, 0, timestamp);

        if (!GTK_MENU_SHELL (GTK_MENU (menu->menu))->have_xgrab)
        {
            gdk_beep ();
            g_message (_("%s: GtkMenu failed to grab the pointer\n"),
                g_get_prgname ());
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
