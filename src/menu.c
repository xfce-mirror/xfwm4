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
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <unistd.h>
#include "menu.h"
#include "gtktoxevent.h"
#include "debug.h"

static GtkWidget *menu_open = NULL;
static MenuItem menuitems[] = {
    {MENU_OP_MAXIMIZE, NULL, "Ma_ximize"},
    {MENU_OP_UNMAXIMIZE, NULL, "Un_maximize"},
    {MENU_OP_MINIMIZE, NULL, "_Hide"},
    {MENU_OP_MINIMIZE_ALL, NULL, "Hide _all others"},
    {MENU_OP_UNMINIMIZE, NULL, "S_how"},
    {MENU_OP_SHADE, NULL, "_Shade"},
    {MENU_OP_UNSHADE, NULL, "Un_shade"},
    {MENU_OP_STICK, NULL, "S_tick"},
    {MENU_OP_UNSTICK, NULL, "Uns_tick"},
    {MENU_OP_MOVE, NULL, "_Move"},
    {MENU_OP_RESIZE, NULL, "_Resize"},
    {MENU_OP_SWITCH, NULL, "S_witch"},
    {MENU_OP_DELETE, NULL, "_Close"},
    {0, NULL, NULL},
    {MENU_OP_DESTROY, NULL, "Destroy"},
#if 0
    {0, NULL, NULL},
    {MENU_OP_WORKSPACES, NULL, "Wor_kspace"},
    {0, NULL, NULL},
#endif
    {MENU_OP_QUIT, NULL, "_Quit"},
    {MENU_OP_RESTART, NULL, "Restart"},
};

static GtkToXEventFilterStatus menu_filter(XEvent * xevent, gpointer data)
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
popup_position_func(GtkMenu * menu, gint * x, gint * y, gboolean * push_in, gpointer user_data)
{
    GtkRequisition req;
    GdkPoint *pos;

    pos = user_data;

    gtk_widget_size_request(GTK_WIDGET(menu), &req);

    if(pos->x >= 0)
    {
        *x = pos->x;
        *x = CLAMP(*x, 0, MAX(0, gdk_screen_width() - req.width));
    }
    else
    {
        *x = (gdk_screen_width() - req.width) / 2;
    }
    if(pos->x >= 0)
    {
        *y = pos->y;
        *y = CLAMP(*y, 0, MAX(0, gdk_screen_height() - req.height));
    }
    else
    {
        *y = (gdk_screen_height() - req.height) / 2;
    }
    g_free(user_data);
}

static gboolean activate_cb(GtkWidget * menuitem, gpointer data)
{
    MenuData *md;

    DBG("entering activate_cb\n");
    g_return_val_if_fail(GTK_IS_WIDGET(menuitem), FALSE);

    menu_open = NULL;

    md = data;

    DBG("deactivating menu_filter\n");
    popEventFilter();
    (*md->menu->func) (md->menu, md->op, md->client_xwindow, md->menu->data, md->data);
    return (FALSE);
}

static gboolean menu_closed(GtkMenu * widget, gpointer data)
{
    Menu *menu;

    DBG("entering menu_closed\n");
    menu = data;
    menu_open = NULL;
    DBG("deactivating menu_filter\n");
    popEventFilter();
    (*menu->func) (menu, 0, None, menu->data, NULL);
    return (FALSE);
}

Menu *menu_default(MenuOp ops, MenuOp insensitive, MenuFunc func, gpointer data)
{
    int i;
    Menu *menu;

    DBG("entering menu_new\n");
    menu = g_new(Menu, 1);
    menu->func = func;
    menu->data = data;
    menu->ops = ops;
    menu->insensitive = insensitive;
    menu->menu = gtk_menu_new();

    i = 0;
    while(i < (int)(sizeof(menuitems) / sizeof(MenuItem)))
    {
        if(ops & menuitems[i].op || menuitems[i].op == 0)
        {
            GtkWidget *mi;
            MenuData *md;

            if(menuitems[i].op == 0)
            {
                mi = gtk_separator_menu_item_new();
            }
            else
            {
                mi = gtk_menu_item_new_with_mnemonic(menuitems[i].label);
                if(insensitive & menuitems[i].op)
                {
                    gtk_widget_set_sensitive(mi, FALSE);
                }
                md = g_new(MenuData, 1);
                md->menu = menu;
                md->op = menuitems[i].op;
                md->client_xwindow = None;
                md->data = NULL;
                menu_item_connect(mi, md);
            }
            gtk_menu_shell_append(GTK_MENU_SHELL(menu->menu), mi);
            gtk_widget_show(mi);
        }
        ++i;
    }
    menu_connect(menu);

    return (menu);
}

Menu *menu_connect(Menu * menu)
{
    DBG("entering menu_connect\n");
    g_return_val_if_fail(menu != NULL, NULL);
    g_return_val_if_fail(GTK_IS_MENU(menu->menu), NULL);
    g_signal_connect(GTK_OBJECT(menu->menu), "selection_done", GTK_SIGNAL_FUNC(menu_closed), menu);
    return (menu);
}

static void closure_notify(gpointer data, GClosure * closure)
{
    DBG("entering closure_notify\n");
    if(data)
    {
        DBG("freeing data\n");
        g_free(data);
    }
}

GtkWidget *menu_item_connect(GtkWidget * item, MenuData * item_data)
{
    DBG("entering menu_item_connect\n");
    g_return_val_if_fail(item != NULL, NULL);
    g_return_val_if_fail(GTK_IS_MENU_ITEM(item), NULL);
    g_signal_connect_closure(GTK_OBJECT(item), "activate", g_cclosure_new(GTK_SIGNAL_FUNC(activate_cb), item_data, (GClosureNotify) closure_notify), FALSE);
    return (item);
}

gboolean menu_is_opened(void)
{
    DBG("entering menu_is_opened\n");
    return (menu_open != NULL);
}

gboolean menu_check_and_close(void)
{
    DBG("entering menu_check_or_close\n");
    if(menu_open)
    {
        DBG("menu open, emitting deactivate signal\n");
        g_signal_emit_by_name(GTK_OBJECT(menu_open), "deactivate");
        menu_open = NULL;
        return (TRUE);
    }
    return (FALSE);
}

static gboolean grab_available(guint32 timestamp)
{
    GdkEventMask mask = GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_POINTER_MOTION_MASK;
    GdkGrabStatus g1;
    GdkGrabStatus g2;
    gboolean grab_failed = FALSE;
    gint i = 0;

    DBG("entering grab_available\n");

    g1 = gdk_pointer_grab(getGdkEventWindow(), TRUE, mask, NULL, NULL, timestamp);
    g2 = gdk_keyboard_grab(getGdkEventWindow(), TRUE, timestamp);

    while((i++ < 100) && (grab_failed = ((g1 != GDK_GRAB_SUCCESS) || (g2 != GDK_GRAB_SUCCESS))))
    {
        DBG("grab not available yet, waiting... (%i)\n", i);
        usleep(100);
        if(g1 != GDK_GRAB_SUCCESS)
        {
            g1 = gdk_pointer_grab(getGdkEventWindow(), TRUE, mask, NULL, NULL, timestamp);
        }
        if(g2 != GDK_GRAB_SUCCESS)
        {
            g2 = gdk_keyboard_grab(getGdkEventWindow(), TRUE, timestamp);
        }
    }

    if(g1 == GDK_GRAB_SUCCESS)
    {
        gdk_pointer_ungrab(timestamp);
    }
    if(g2 == GDK_GRAB_SUCCESS)
    {
        gdk_keyboard_ungrab(timestamp);
    }

    return (!grab_failed);
}

gboolean menu_popup(Menu * menu, int root_x, int root_y, int button, guint32 timestamp)
{
    GdkPoint *pt;
    DBG("entering menu_popup\n");

    g_return_val_if_fail(menu != NULL, FALSE);
    g_return_val_if_fail(GTK_IS_MENU(menu->menu), FALSE);

    pt = g_new(GdkPoint, 1);
    pt->x = root_x;
    pt->y = root_y;

    if(!menu_check_and_close())
    {
        if(!grab_available(timestamp))
        {
            g_free(pt);
            DBG("Cannot get grab on pointer/keyboard, cancel.\n");
            return FALSE;
        }
        DBG("opening new menu\n");
        menu_open = menu->menu;
        pushEventFilter(menu_filter, NULL);
        gtk_menu_popup(GTK_MENU(menu->menu), NULL, NULL, popup_position_func, pt, button, timestamp);
        if(!GTK_MENU_SHELL(GTK_MENU(menu->menu))->have_xgrab)
        {
            gdk_beep();
            g_message("GtkMenu failed to grab the pointer\n");
        }
    }
    return TRUE;
}

void menu_free(Menu * menu)
{
    DBG("entering menu_free\n");
    g_return_if_fail(menu != NULL);
    g_return_if_fail(menu->menu != NULL);
    g_return_if_fail(GTK_IS_MENU(menu->menu));
    DBG("freeing menu\n");
    gtk_widget_destroy(menu->menu);
    g_free(menu);
}
