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

        oroborus - (c) 2001 Ken Lynch
        Metacity - (c) 2001 Havoc Pennington
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "hints.h"
#include "debug.h"

Atom wm_state;
Atom wm_change_state;
Atom wm_delete_window;
Atom wm_protocols;
Atom motif_wm_hints;
Atom win_hints;
Atom win_state;
Atom win_client_list;
Atom win_layer;
Atom win_workspace;
Atom win_workspace_count;
Atom win_desktop_button_proxy;
Atom win_supporting_wm_check;
Atom gnome_panel_desktop_area;

/* freedesktop.org protocol */

Atom utf8_string;
Atom net_wm_name;
Atom net_close_window;
Atom net_wm_state;
Atom motif_wm_hints;
Atom net_wm_state_shaded;
Atom net_wm_state_sticky;
Atom net_wm_state_maximized_horz;
Atom net_wm_state_maximized_vert;
Atom net_wm_desktop;
Atom net_number_of_desktops;
Atom net_current_desktop;
Atom net_desktop_viewport;
Atom net_desktop_geometry;
Atom net_workarea;
Atom net_supporting_wm_check;
Atom net_supported;
Atom net_wm_window_type;
Atom net_wm_window_type_desktop;
Atom net_wm_window_type_dock;
Atom net_wm_window_type_toolbar;
Atom net_wm_window_type_menu;
Atom net_wm_window_type_dialog;
Atom net_wm_window_type_normal;
Atom net_wm_state_modal;
Atom net_client_list;
Atom net_client_list_stacking;
Atom net_wm_state_skip_taskbar;
Atom net_wm_state_skip_pager;
Atom net_wm_icon_name;
Atom net_wm_icon;
Atom net_wm_icon_geometry;
Atom net_wm_moveresize;
Atom net_active_window;
Atom net_wm_strut;
Atom net_wm_state_hidden;
Atom net_wm_window_type_utility;
Atom net_wm_window_type_splashscreen;


void initICCCMHints(Display * dpy)
{
    DBG("entering initICCCMHints\n");

    wm_state = XInternAtom(dpy, "WM_STATE", False);
    wm_change_state = XInternAtom(dpy, "WM_CHANGE_STATE", False);
    wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
}

unsigned long getWMState(Display * dpy, Window w)
{
    Atom real_type;
    int real_format;
    unsigned long items_read, items_left;
    unsigned long *data = NULL, state = WithdrawnState;

    DBG("entering getWmState\n");

    if((XGetWindowProperty(dpy, w, wm_state, 0L, 2L, False, wm_state, &real_type, &real_format, &items_read, &items_left, (unsigned char **)&data) == Success) && (items_read))
    {
        state = *data;
	XFree(data);
    }
    return (state);
}

void setWMState(Display * dpy, Window w, unsigned long state)
{
    CARD32 data[2];

    DBG("entering setWmState\n");

    data[0] = state;
    data[1] = None;

    XChangeProperty(dpy, w, wm_state, wm_state, 32, PropModeReplace, (unsigned char *)data, 2);
}

void initMotifHints(Display * dpy)
{
    DBG("entering initMotifHints\n");

    motif_wm_hints = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
}

PropMwmHints *getMotifHints(Display * dpy, Window w)
{
    Atom real_type;
    int real_format;
    unsigned long items_read, items_left;
    PropMwmHints *data = NULL;

    DBG("entering getMotifHints\n");

    if((XGetWindowProperty(dpy, w, motif_wm_hints, 0L, 20L, False, motif_wm_hints, &real_type, &real_format, &items_read, &items_left, (unsigned char **)&data) == Success) && (items_read))
    {
        return (data);
    }
    else
    {
        return (NULL);
    }
}

void initGnomeHints(Display * dpy)
{
    DBG("entering initGnomeHints\n");

    win_hints                       = XInternAtom(dpy, "_WIN_HINTS", False);
    win_state                       = XInternAtom(dpy, "_WIN_STATE", False);
    win_client_list                 = XInternAtom(dpy, "_WIN_CLIENT_LIST", False);
    win_layer                       = XInternAtom(dpy, "_WIN_LAYER", False);
    win_workspace                   = XInternAtom(dpy, "_WIN_WORKSPACE", False);
    win_workspace_count             = XInternAtom(dpy, "_WIN_WORKSPACE_COUNT", False);
    win_desktop_button_proxy        = XInternAtom(dpy, "_WIN_DESKTOP_BUTTON_PROXY", False);
    win_supporting_wm_check         = XInternAtom(dpy, "_WIN_SUPPORTING_WM_CHECK", False);
    gnome_panel_desktop_area        = XInternAtom(dpy, "GNOME_PANEL_DESKTOP_AREA", False);
}

int getGnomeHint(Display * dpy, Window w, Atom a, long *value)
{
    Atom real_type;
    int real_format, success = False;
    unsigned long items_read, items_left;
    long *data = NULL;

    DBG("entering getGnomeHint\n");

    *value = 0;
    
    if((XGetWindowProperty (dpy, w, a, 0L, 1L, False, XA_CARDINAL, &real_type, &real_format, &items_read, &items_left, (unsigned char **)&data) == Success) && (items_read))
    {
        *value = *data;
	XFree(data);
        success = True;
    }
    return (success);
}

void setGnomeHint(Display * dpy, Window w, Atom a, long value)
{
    DBG("entering setGnomeHint\n");

    XChangeProperty(dpy, w, a, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&value, 1);
}

void getGnomeDesktopMargins(Display * dpy, CARD32 * margins)
{
    Atom real_type;
    int real_format;
    unsigned long items_read, items_left;
    CARD32 *data = NULL;

    DBG("entering getGnomeDesktopMargins\n");

    if((XGetWindowProperty (dpy, XDefaultRootWindow(dpy), gnome_panel_desktop_area, 0L, 4L, False, XA_CARDINAL, &real_type, &real_format, &items_read, &items_left, (unsigned char **)&data) == Success) && (items_read >= 4))
    {
        margins[0] = data[0];
        margins[1] = data[1];
        margins[2] = data[2];
        margins[3] = data[3];
        XFree(data);
    }
    else
    {
        margins[0] = 0;
        margins[1] = 0;
        margins[2] = 0;
        margins[3] = 0;
    }
}

void initNetHints(Display * dpy)
{
    DBG("entering initNetHints\n");

    utf8_string                     = XInternAtom(dpy, "UTF8_STRING", False);
    net_wm_name                     = XInternAtom(dpy, "_NET_WM_NAME", False);
    net_close_window                = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
    net_wm_state                    = XInternAtom(dpy, "_NET_WM_STATE", False);
    net_wm_state_shaded             = XInternAtom(dpy, "_NET_WM_STATE_SHADED", False);
    net_wm_state_sticky             = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
    net_wm_state_maximized_horz     = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    net_wm_state_maximized_vert     = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    net_wm_desktop                  = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    net_number_of_desktops          = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
    net_current_desktop             = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
    net_desktop_viewport            = XInternAtom(dpy, "_NET_DESKTOP_VIEWPORT", False);
    net_desktop_geometry            = XInternAtom(dpy, "_NET_DESKTOP_GEOMETRY", False);
    net_workarea                    = XInternAtom(dpy, "_NET_WORKAREA", False);
    net_supporting_wm_check         = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    net_supported                   = XInternAtom(dpy, "_NET_SUPPORTED", False);
    net_wm_window_type              = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    net_wm_window_type_desktop      = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    net_wm_window_type_dock         = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    net_wm_window_type_toolbar      = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
    net_wm_window_type_menu         = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
    net_wm_window_type_dialog       = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    net_wm_window_type_normal       = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    net_wm_state_modal              = XInternAtom(dpy, "_NET_WM_STATE_MODAL", False);
    net_client_list                 = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    net_client_list_stacking        = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
    net_wm_state_skip_taskbar       = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    net_wm_state_skip_pager         = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
    net_wm_icon_name                = XInternAtom(dpy, "_NET_WM_ICON_NAME", False);
    net_wm_icon                     = XInternAtom(dpy, "_NET_WM_ICON", False);
    net_wm_icon_geometry            = XInternAtom(dpy, "_NET_WM_ICON_GEOMETRY", False);
    net_wm_moveresize               = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
    net_active_window               = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    net_wm_strut                    = XInternAtom(dpy, "_NET_WM_STRUT", False);
    net_wm_state_hidden             = XInternAtom(dpy, "_NET_WM_STATE_HIDDEN", False);
    net_wm_window_type_utility      = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    net_wm_window_type_splashscreen = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASHSCREEN", False);
}

int getNetHint(Display * dpy, Window w, Atom a, long *value)
{
    Atom real_type;
    int real_format, success = False;
    unsigned long items_read, items_left;
    long *data = NULL;

    DBG("entering getNetHint\n");

    *value = 0;
    
    if((XGetWindowProperty (dpy, w, a, 0L, 1L, False, XA_CARDINAL, &real_type, &real_format, &items_read, &items_left, (unsigned char **)&data) == Success) && (items_read))
    {
        *value = *data;
	XFree(data);
        success = True;
    }
    return (success);
}

void set_net_supported_hint (Display * dpy, Window root_win, Window check_win)
{
    Atom atoms[30];
    unsigned long data[1];
    int i = 0;

    atoms[i++] = net_wm_name;
    atoms[i++] = net_close_window;
    atoms[i++] = net_wm_state;
    atoms[i++] = net_wm_state_shaded;
    atoms[i++] = net_wm_state_sticky;
    atoms[i++] = net_wm_state_maximized_vert;
    atoms[i++] = net_wm_state_maximized_horz;
    atoms[i++] = net_wm_desktop;
    atoms[i++] = net_number_of_desktops;
    atoms[i++] = net_current_desktop;
    atoms[i++] = net_desktop_geometry;
    atoms[i++] = net_desktop_viewport;
    atoms[i++] = net_workarea;
    atoms[i++] = net_wm_window_type;
    atoms[i++] = net_wm_window_type_desktop;
    atoms[i++] = net_wm_window_type_dock;
    atoms[i++] = net_wm_window_type_toolbar;
    atoms[i++] = net_wm_window_type_menu;
    atoms[i++] = net_wm_window_type_dialog;
    atoms[i++] = net_wm_window_type_normal;
    atoms[i++] = net_wm_state_modal;
    atoms[i++] = net_client_list_stacking;
    atoms[i++] = net_client_list;
    atoms[i++] = net_wm_state_skip_taskbar;
    atoms[i++] = net_wm_state_skip_pager;
    atoms[i++] = net_wm_strut;
    /* Not supported (yet)
    atoms[i++] = net_wm_moveresize;
     */
    atoms[i++] = net_wm_state_hidden;
    atoms[i++] = net_wm_window_type_utility;
    atoms[i++] = net_wm_window_type_splashscreen;

    XChangeProperty (dpy, check_win, net_supported, XA_ATOM, 32, PropModeReplace, (unsigned char *) atoms, i);
    data[0] = check_win;
    XChangeProperty (dpy, root_win, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *) data, 1);
}

static int check_type_and_format (Display *dpy, Window w, Atom a, int expected_format, Atom expected_type, int n_items,  int format, Atom type)
{
    if ((expected_format == format) && (expected_type == type) && (n_items < 0 || n_items > 0))
    {
	return (True);  
    }
    return (False);
}

int get_atom_list (Display *dpy, Window  w, Atom a, Atom **atoms_p, int *n_atoms_p)
{
    Atom type;
    int format;
    unsigned long n_atoms;
    unsigned long bytes_after;
    Atom *atoms;

    *atoms_p = NULL;
    *n_atoms_p = 0;

    if (XGetWindowProperty (dpy, w, a, 0L, 1L, False, XA_ATOM, &type, &format, &n_atoms, &bytes_after, (guchar **)&atoms) != Success || type == None)
    {
	return (False);
    }

    if (!check_type_and_format (dpy, w, a, 32, XA_ATOM, -1, format, type))
    {
	if (atoms)
	{
            XFree (atoms);
	}
	*atoms_p = NULL;
	*n_atoms_p = 0;
	return (False);
    }

    *atoms_p = atoms;
    *n_atoms_p = n_atoms;

    return (True);
}

int get_cardinal_list (Display *dpy, Window w, Atom xatom, unsigned long **cardinals_p, int *n_cardinals_p)
{
    Atom type;
    int format;
    unsigned long n_cardinals;
    unsigned long bytes_after;
    unsigned long *cardinals;

    *cardinals_p = NULL;
    *n_cardinals_p = 0;

    if ((XGetWindowProperty (dpy, w, xatom, 0, 32, False, XA_CARDINAL, &type, &format, &n_cardinals, &bytes_after, (unsigned char **)&cardinals) != Success) || (type == None))
    {
	return False;
    }

    if (!check_type_and_format (dpy, w, xatom, 32, XA_CARDINAL, -1, format, type))
    {
	XFree (cardinals);
	return False;
    }

    *cardinals_p = cardinals;
    *n_cardinals_p = n_cardinals;

    return True;
}

void set_net_workarea (Display * dpy, Window w, CARD32 *margins)
{
    unsigned long data[4];
    data[0] = margins[MARGIN_LEFT];
    data[1] = margins[MARGIN_TOP];
    data[2] = XDisplayWidth(dpy, screen) - (margins[MARGIN_LEFT] + margins[MARGIN_RIGHT]);
    data[3] = XDisplayHeight(dpy, screen) - (margins[MARGIN_TOP] + margins[MARGIN_BOTTOM]);
    XChangeProperty (dpy, w, net_workarea, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 4);
}

void init_net_desktop_params (Display * dpy, Window w, int workspace)
{
    unsigned long data[2];
    data[0] = XDisplayWidth(dpy, screen);
    data[1] = XDisplayHeight(dpy, screen);
    XChangeProperty (dpy, w, net_desktop_geometry, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 2);
    data[0] = 0;
    data[1] = 0;
    XChangeProperty (dpy, w, net_desktop_viewport, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 2);
    /* TBD : See why this prevents kdesktop from working properly */   
    data[0] = workspace;
    XChangeProperty (dpy, w, net_current_desktop, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 1);
}

void
set_utf8_string_hint (Display *dpy, Window w, Atom atom, const char *val)
{
    XChangeProperty (dpy, w, atom, utf8_string,  8, PropModeReplace, (unsigned char *) val, strlen (val) + 1);
}

void getTransientFor(Display * dpy, Window w, Window *transient_for)
{
    DBG("entering getTransientFor\n");

    if (!XGetTransientForHint (dpy, w, transient_for))
    {
	*transient_for = None;
    }

    DBG("Window (%#lx) is transient for (%#lx)\n", w, *transient_for);
}

int get_utf8_string (Display *dpy, Window w, Atom xatom, char **str_p)
{
    Atom type;
    int format;
    unsigned long bytes_after;
    unsigned char *str;
    unsigned long n_items;

    DBG("entering get_utf8_string\n");

    *str_p = NULL;
    if ((XGetWindowProperty (dpy, w, xatom, 0, 32, False, utf8_string, &type, &format, &n_items, &bytes_after, (unsigned char **)&str) != Success) || (type == None))
    {
        DBG("no utf8_string value provided\n");
	return False;
    }

    if (!check_type_and_format (dpy, w, xatom, 8, utf8_string, -1, format, type))
    {
        DBG("utf8_string value invalid\n");
	if (str)
	{
            XFree (str);
	}
	return False;
    }

    if (!g_utf8_validate (str, n_items, NULL))
    {
	char *name;

	name = XGetAtomName (dpy, xatom);
	if (name)
	{
	    g_message("Property %s on window (%lx) contained invalid UTF-8\n", name, w);
	    XFree (name);
	}
	XFree (str);

	return False;
    }

    *str_p = str;

    return True;
}

static char* text_property_to_utf8 (Display *dpy, const XTextProperty *prop)
{
    char **list;
    int count;
    char *retval;

    DBG("entering text_property_to_utf8\n");

    list = NULL;
    if ((count = gdk_text_property_to_utf8_list (gdk_x11_xatom_to_atom (prop->encoding), prop->format, prop->value, prop->nitems, &list)) == 0)
    {
        DBG("gdk_text_property_to_utf8_list returned 0\n");
	return NULL;
    }
    retval = list[0];
    list[0] = g_strdup ("");
    g_strfreev (list);

    return retval;
}

static char* get_text_property (Display *dpy, Window w, Atom a)
{
    XTextProperty text;
    char *retval;

    DBG("entering get_text_property\n");
    text.nitems = 0;
    if (XGetTextProperty (dpy, w, &text, a))
    {
	retval = text_property_to_utf8 (dpy, &text);
	if ((text.value) && (text.nitems > 0))
	{
            XFree (text.value);
	}
    }
    else
    {
	retval = NULL;
	DBG("XGetTextProperty() failed\n");
    }

    return retval;
}


void getWindowName(Display * dpy, Window w, char **name)
{
    char *str;

    DBG("entering getWindowName\n");
    *name = NULL;
    if (get_utf8_string (dpy, w, net_wm_name, &str))
    {
        *name = strdup(str);
	XFree (str);
	return;
    }
    str = get_text_property (dpy, w, XA_WM_NAME);
    if (str)
    {
        *name = strdup (str);
	XFree (str);
    }    
    else
    {
        *name = strdup("");
    }

    return;
}
