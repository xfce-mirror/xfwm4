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
        xfwm4    - (c) 2002-2003 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef GDK_MULTIHEAD_SAFE
#undef GDK_MULTIHEAD_SAFE
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfcegui4/xinerama.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "hints.h"

Atom gnome_panel_desktop_area;
Atom motif_wm_hints;
Atom sm_client_id;
Atom win_client_list;
Atom win_desktop_button_proxy;
Atom win_hints;
Atom win_layer;
Atom win_protocols;
Atom win_state;
Atom win_supporting_wm_check;
Atom win_workspace;
Atom win_workspace_count;
Atom wm_change_state;
Atom wm_client_leader;
Atom wm_colormap_windows;
Atom wm_delete_window;
Atom wm_hints;
Atom wm_protocols;
Atom wm_state;
Atom wm_takefocus;
Atom wm_window_role;

/* freedesktop.org protocol */
Atom net_active_window;
Atom net_client_list;
Atom net_client_list_stacking;
Atom net_close_window;
Atom net_current_desktop;
Atom net_desktop_geometry;
Atom net_desktop_viewport;
Atom net_number_of_desktops;
Atom net_startup_id;
Atom net_supported;
Atom net_supporting_wm_check;
Atom net_wm_action_change_desktop;
Atom net_wm_action_close;
Atom net_wm_action_maximize_horz;
Atom net_wm_action_maximize_vert;
Atom net_wm_action_move;
Atom net_wm_action_resize;
Atom net_wm_action_shade;
Atom net_wm_action_stick;
Atom net_wm_allowed_actions;
Atom net_wm_desktop;
Atom net_wm_icon;
Atom net_wm_icon_geometry;
Atom net_wm_icon_name;
Atom net_wm_moveresize;
Atom net_wm_name;
Atom net_wm_state;
Atom net_wm_state_above;
Atom net_wm_state_below;
Atom net_wm_state_fullscreen;
Atom net_wm_state_hidden;
Atom net_wm_state_maximized_horz;
Atom net_wm_state_maximized_vert;
Atom net_wm_state_modal;
Atom net_wm_state_shaded;
Atom net_wm_state_skip_pager;
Atom net_wm_state_skip_taskbar;
Atom net_wm_state_sticky;
Atom net_wm_strut;
Atom net_wm_window_type;
Atom net_wm_window_type_desktop;
Atom net_wm_window_type_dialog;
Atom net_wm_window_type_dock;
Atom net_wm_window_type_menu;
Atom net_wm_window_type_normal;
Atom net_wm_window_type_splashscreen;
Atom net_wm_window_type_toolbar;
Atom net_wm_window_type_utility;
Atom net_workarea;
Atom utf8_string;


void
initICCCMHints (Display * dpy)
{
    TRACE ("entering initICCCMHints");

    sm_client_id = XInternAtom (dpy, "SM_CLIENT_ID", FALSE);
    wm_change_state = XInternAtom (dpy, "WM_CHANGE_STATE", FALSE);
    wm_client_leader = XInternAtom (dpy, "WM_CLIENT_LEADER", FALSE);
    wm_colormap_windows = XInternAtom (dpy, "WM_COLORMAP_WINDOWS", FALSE);
    wm_delete_window = XInternAtom (dpy, "WM_DELETE_WINDOW", FALSE);
    wm_protocols = XInternAtom (dpy, "WM_PROTOCOLS", FALSE);
    wm_state = XInternAtom (dpy, "WM_STATE", FALSE);
    wm_takefocus = XInternAtom (dpy, "WM_TAKE_FOCUS", FALSE);
    wm_window_role = XInternAtom (dpy, "WM_WINDOW_ROLE", FALSE);
}

unsigned long
getWMState (Display * dpy, Window w)
{
    Atom real_type;
    int real_format;
    unsigned long items_read, items_left;
    unsigned long *data = NULL, state = WithdrawnState;

    TRACE ("entering getWmState");

    if ((XGetWindowProperty (dpy, w, wm_state, 0, 3L, FALSE, wm_state,
                &real_type, &real_format, &items_read, &items_left,
                (unsigned char **) &data) == Success) && (items_read))
    {
        state = *data;
        XFree (data);
    }
    return state;
}

void
setWMState (Display * dpy, Window w, unsigned long state)
{
    CARD32 data[2];

    TRACE ("entering setWmState");

    data[0] = state;
    data[1] = None;

    XChangeProperty (dpy, w, wm_state, wm_state, 32, PropModeReplace,
        (unsigned char *) data, 2);
}

void
initMotifHints (Display * dpy)
{
    TRACE ("entering initMotifHints");

    motif_wm_hints = XInternAtom (dpy, "_MOTIF_WM_HINTS", FALSE);
}

PropMwmHints *
getMotifHints (Display * dpy, Window w)
{
    Atom real_type;
    int real_format;
    unsigned long items_read, items_left;
    PropMwmHints *data = NULL;

    TRACE ("entering getMotifHints");

    if ((XGetWindowProperty (dpy, w, motif_wm_hints, 0L, 20L, FALSE,
                motif_wm_hints, &real_type, &real_format, &items_read,
                &items_left, (unsigned char **) &data) == Success)
        && (items_read))
    {
        return data;
    }
    else
    {
        return NULL;
    }
}

unsigned int
getWMProtocols (Display * dpy, Window w)
{
    Atom *protocols = None, *ap;
    int i, n;
    Atom atype;
    int aformat;
    int result = 0;
    unsigned long bytes_remain, nitems;

    TRACE ("entering getWMProtocols");

    if (XGetWMProtocols (dpy, w, &protocols, &n))
    {
        for (i = 0, ap = protocols; i < n; i++, ap++)
        {
            if (*ap == (Atom) wm_takefocus)
            {
                result |= WM_PROTOCOLS_TAKE_FOCUS;
            }
            if (*ap == (Atom) wm_delete_window)
            {
                result |= WM_PROTOCOLS_DELETE_WINDOW;
            }
        }
    }
    else
    {
        if ((XGetWindowProperty (dpy, w, wm_protocols, 0L, 10L, FALSE,
                    wm_protocols, &atype, &aformat, &nitems, &bytes_remain,
                    (unsigned char **) &protocols)) == Success)
        {
            for (i = 0, ap = protocols; i < nitems; i++, ap++)
            {
                if (*ap == (Atom) wm_takefocus)
                {
                    result |= WM_PROTOCOLS_TAKE_FOCUS;
                }
                if (*ap == (Atom) wm_delete_window)
                {
                    result |= WM_PROTOCOLS_DELETE_WINDOW;
                }
            }
        }
    }
    if (protocols)
    {
        XFree (protocols);
    }
    return result;
}


void
initGnomeHints (Display * dpy)
{
    TRACE ("entering initGnomeHints");

    gnome_panel_desktop_area =
        XInternAtom (dpy, "GNOME_PANEL_DESKTOP_AREA", FALSE);
    win_client_list = XInternAtom (dpy, "_WIN_CLIENT_LIST", FALSE);
    win_desktop_button_proxy =
        XInternAtom (dpy, "_WIN_DESKTOP_BUTTON_PROXY", FALSE);
    win_hints = XInternAtom (dpy, "_WIN_HINTS", FALSE);
    win_layer = XInternAtom (dpy, "_WIN_LAYER", FALSE);
    win_protocols = XInternAtom (dpy, "_WIN_PROTOCOLS", FALSE);
    win_state = XInternAtom (dpy, "_WIN_STATE", FALSE);
    win_supporting_wm_check =
        XInternAtom (dpy, "_WIN_SUPPORTING_WM_CHECK", FALSE);
    win_workspace_count = XInternAtom (dpy, "_WIN_WORKSPACE_COUNT", FALSE);
    win_workspace = XInternAtom (dpy, "_WIN_WORKSPACE", FALSE);
}

gboolean
getGnomeHint (Display * dpy, Window w, Atom a, long *value)
{
    Atom real_type;
    int real_format;
    gboolean success = FALSE;
    unsigned long items_read, items_left;
    long *data = NULL;

    TRACE ("entering getGnomeHint");

    *value = 0;

    if ((XGetWindowProperty (dpy, w, a, 0L, 1L, FALSE, XA_CARDINAL,
                &real_type, &real_format, &items_read, &items_left,
                (unsigned char **) &data) == Success) && (items_read))
    {
        *value = *data;
        XFree (data);
        success = TRUE;
    }
    return success;
}

void
setGnomeHint (Display * dpy, Window w, Atom a, long value)
{
    TRACE ("entering setGnomeHint");

    XChangeProperty (dpy, w, a, XA_CARDINAL, 32, PropModeReplace,
        (unsigned char *) &value, 1);
}

void
getGnomeDesktopMargins (Display * dpy, int screen, CARD32 * m)
{
    Atom real_type;
    int real_format;
    unsigned long items_read, items_left;
    CARD32 *data = NULL;

    TRACE ("entering getGnomeDesktopMargins");

    if ((XGetWindowProperty (dpy, RootWindow (dpy, screen),
                gnome_panel_desktop_area, 0L, 4L, FALSE, XA_CARDINAL,
                &real_type, &real_format, &items_read, &items_left,
                (unsigned char **) &data) == Success) && (items_read >= 4))
    {
        m[0] = data[0];
        m[1] = data[1];
        m[2] = data[2];
        m[3] = data[3];
        XFree (data);
    }
    else
    {
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
        m[3] = 0;
    }
}

void
setGnomeProtocols (Display * dpy, int screen, Window w)
{
    Atom atoms[1];

    atoms[0] = win_layer;
    XChangeProperty (dpy, RootWindow (dpy, screen), win_protocols, XA_ATOM,
        32, PropModeReplace, (unsigned char *) atoms, 1);
    setGnomeHint (dpy, w, win_supporting_wm_check, w);
    setGnomeHint (dpy, RootWindow (dpy, screen), win_supporting_wm_check,
        gnome_win);
}

void
initNetHints (Display * dpy)
{
    TRACE ("entering initNetHints");

    net_active_window = XInternAtom (dpy, "_NET_ACTIVE_WINDOW", FALSE);
    net_client_list_stacking =
        XInternAtom (dpy, "_NET_CLIENT_LIST_STACKING", FALSE);
    net_client_list = XInternAtom (dpy, "_NET_CLIENT_LIST", FALSE);
    net_close_window = XInternAtom (dpy, "_NET_CLOSE_WINDOW", FALSE);
    net_current_desktop = XInternAtom (dpy, "_NET_CURRENT_DESKTOP", FALSE);
    net_desktop_geometry = XInternAtom (dpy, "_NET_DESKTOP_GEOMETRY", FALSE);
    net_desktop_viewport = XInternAtom (dpy, "_NET_DESKTOP_VIEWPORT", FALSE);
    net_number_of_desktops =
        XInternAtom (dpy, "_NET_NUMBER_OF_DESKTOPS", FALSE);
    net_startup_id = XInternAtom (dpy, "_NET_STARTUP_ID", FALSE);
    net_supported = XInternAtom (dpy, "_NET_SUPPORTED", FALSE);
    net_supporting_wm_check =
        XInternAtom (dpy, "_NET_SUPPORTING_WM_CHECK", FALSE);
    net_wm_action_change_desktop =
        XInternAtom (dpy, "_NET_WM_ACTION_CHANGE_DESKTOP", FALSE);
    net_wm_action_close = XInternAtom (dpy, "_NET_WM_ACTION_CLOSE", FALSE);
    net_wm_action_maximize_horz =
        XInternAtom (dpy, "_NET_WM_ACTION_MAXIMIZE_HORZ", FALSE);
    net_wm_action_maximize_vert =
        XInternAtom (dpy, "_NET_WM_ACTION_MAXIMIZE_VERT", FALSE);
    net_wm_action_move = XInternAtom (dpy, "_NET_WM_ACTION_MOVE", FALSE);
    net_wm_action_resize = XInternAtom (dpy, "_NET_WM_ACTION_RESIZE", FALSE);
    net_wm_action_shade = XInternAtom (dpy, "_NET_WM_ACTION_SHADE", FALSE);
    net_wm_action_stick = XInternAtom (dpy, "_NET_WM_ACTION_STICK", FALSE);
    net_wm_allowed_actions =
        XInternAtom (dpy, "_NET_WM_ALLOWED_ACTIONS", FALSE);
    net_wm_desktop = XInternAtom (dpy, "_NET_WM_DESKTOP", FALSE);
    net_wm_icon_geometry = XInternAtom (dpy, "_NET_WM_ICON_GEOMETRY", FALSE);
    net_wm_icon_name = XInternAtom (dpy, "_NET_WM_ICON_NAME", FALSE);
    net_wm_icon = XInternAtom (dpy, "_NET_WM_ICON", FALSE);
    net_wm_moveresize = XInternAtom (dpy, "_NET_WM_MOVERESIZE", FALSE);
    net_wm_name = XInternAtom (dpy, "_NET_WM_NAME", FALSE);
    net_wm_state_above = XInternAtom (dpy, "_NET_WM_STATE_ABOVE", FALSE);
    net_wm_state_below = XInternAtom (dpy, "_NET_WM_STATE_BELOW", FALSE);
    net_wm_state_fullscreen =
        XInternAtom (dpy, "_NET_WM_STATE_FULLSCREEN", FALSE);
    net_wm_state_hidden = XInternAtom (dpy, "_NET_WM_STATE_HIDDEN", FALSE);
    net_wm_state_maximized_horz =
        XInternAtom (dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", FALSE);
    net_wm_state_maximized_vert =
        XInternAtom (dpy, "_NET_WM_STATE_MAXIMIZED_VERT", FALSE);
    net_wm_state_modal = XInternAtom (dpy, "_NET_WM_STATE_MODAL", FALSE);
    net_wm_state_shaded = XInternAtom (dpy, "_NET_WM_STATE_SHADED", FALSE);
    net_wm_state_skip_pager =
        XInternAtom (dpy, "_NET_WM_STATE_SKIP_PAGER", FALSE);
    net_wm_state_skip_taskbar =
        XInternAtom (dpy, "_NET_WM_STATE_SKIP_TASKBAR", FALSE);
    net_wm_state_sticky = XInternAtom (dpy, "_NET_WM_STATE_STICKY", FALSE);
    net_wm_state = XInternAtom (dpy, "_NET_WM_STATE", FALSE);
    net_wm_strut = XInternAtom (dpy, "_NET_WM_STRUT", FALSE);
    net_wm_window_type_desktop =
        XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
    net_wm_window_type_dialog =
        XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DIALOG", FALSE);
    net_wm_window_type_dock =
        XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_DOCK", FALSE);
    net_wm_window_type_menu =
        XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_MENU", FALSE);
    net_wm_window_type_normal =
        XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_NORMAL", FALSE);
    net_wm_window_type_splashscreen =
        XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_SPLASHSCREEN", FALSE);
    net_wm_window_type_toolbar =
        XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", FALSE);
    net_wm_window_type_utility =
        XInternAtom (dpy, "_NET_WM_WINDOW_TYPE_UTILITY", FALSE);
    net_wm_window_type = XInternAtom (dpy, "_NET_WM_WINDOW_TYPE", FALSE);
    net_workarea = XInternAtom (dpy, "_NET_WORKAREA", FALSE);
    utf8_string = XInternAtom (dpy, "UTF8_STRING", FALSE);
}

void
set_net_supported_hint (Display * dpy, int screen, Window check_win)
{
    Atom atoms[64];
    unsigned long data[1];
    int i = 0;

    atoms[i++] = net_active_window;
    atoms[i++] = net_client_list;
    atoms[i++] = net_client_list_stacking;
    atoms[i++] = net_close_window;
    atoms[i++] = net_current_desktop;
    atoms[i++] = net_desktop_geometry;
    atoms[i++] = net_desktop_viewport;
    atoms[i++] = net_number_of_desktops;
    atoms[i++] = net_supported;
    atoms[i++] = net_supporting_wm_check;
    atoms[i++] = net_wm_action_change_desktop;
    atoms[i++] = net_wm_action_close;
    atoms[i++] = net_wm_action_maximize_horz;
    atoms[i++] = net_wm_action_maximize_vert;
    atoms[i++] = net_wm_action_move;
    atoms[i++] = net_wm_action_resize;
    atoms[i++] = net_wm_action_shade;
    atoms[i++] = net_wm_action_stick;
    atoms[i++] = net_wm_allowed_actions;
    atoms[i++] = net_wm_desktop;
    atoms[i++] = net_wm_icon;
    atoms[i++] = net_wm_icon_geometry;
    atoms[i++] = net_wm_icon_name;
    atoms[i++] = net_wm_name;
    atoms[i++] = net_wm_state;
    atoms[i++] = net_wm_state_above;
    atoms[i++] = net_wm_state_below;
    atoms[i++] = net_wm_state_fullscreen;
    atoms[i++] = net_wm_state_hidden;
    atoms[i++] = net_wm_state_maximized_horz;
    atoms[i++] = net_wm_state_maximized_vert;
    atoms[i++] = net_wm_state_modal;
    atoms[i++] = net_wm_state_shaded;
    atoms[i++] = net_wm_state_skip_pager;
    atoms[i++] = net_wm_state_skip_taskbar;
    atoms[i++] = net_wm_state_sticky;
    atoms[i++] = net_wm_strut;
    atoms[i++] = net_wm_window_type;
    atoms[i++] = net_wm_window_type_desktop;
    atoms[i++] = net_wm_window_type_dialog;
    atoms[i++] = net_wm_window_type_dock;
    atoms[i++] = net_wm_window_type_menu;
    atoms[i++] = net_wm_window_type_normal;
    atoms[i++] = net_wm_window_type_splashscreen;
    atoms[i++] = net_wm_window_type_toolbar;
    atoms[i++] = net_wm_window_type_utility;
    atoms[i++] = net_workarea;
#ifdef HAVE_LIBSTARTUP_NOTIFICATION

    atoms[i++] = net_startup_id;
#endif

    XChangeProperty (dpy, RootWindow (dpy, screen), net_supported, XA_ATOM,
        32, PropModeReplace, (unsigned char *) atoms, i);
    data[0] = check_win;
    XChangeProperty (dpy, check_win, net_supporting_wm_check, XA_WINDOW, 32,
        PropModeReplace, (unsigned char *) data, 1);
    XChangeProperty (dpy, RootWindow (dpy, screen), net_supporting_wm_check,
        XA_WINDOW, 32, PropModeReplace, (unsigned char *) data, 1);
}

static gboolean
check_type_and_format (Display * dpy, Window w, Atom a, int expected_format,
    Atom expected_type, int n_items, int format, Atom type)
{
    if ((expected_format == format) && (expected_type == type) && (n_items < 0
            || n_items > 0))
    {
        return TRUE;
    }
    return FALSE;
}

gboolean
get_atom_list (Display * dpy, Window w, Atom a, Atom ** atoms_p,
    int *n_atoms_p)
{
    Atom type;
    int format;
    unsigned long n_atoms;
    unsigned long bytes_after;
    Atom *atoms;

    *atoms_p = NULL;
    *n_atoms_p = 0;

    if ((XGetWindowProperty (dpy, w, a, 0, G_MAXLONG, FALSE, XA_ATOM, &type,
                &format, &n_atoms, &bytes_after,
                (unsigned char **) &atoms) != Success) || (type == None))
    {
        return FALSE;
    }

    if (!check_type_and_format (dpy, w, a, 32, XA_ATOM, -1, format, type))
    {
        if (atoms)
        {
            XFree (atoms);
        }
        *atoms_p = NULL;
        *n_atoms_p = 0;
        return FALSE;
    }

    *atoms_p = atoms;
    *n_atoms_p = n_atoms;

    return TRUE;
}

gboolean
get_cardinal_list (Display * dpy, Window w, Atom xatom,
    unsigned long **cardinals_p, int *n_cardinals_p)
{
    Atom type;
    int format;
    unsigned long n_cardinals;
    unsigned long bytes_after;
    unsigned long *cardinals;

    *cardinals_p = NULL;
    *n_cardinals_p = 0;

    if ((XGetWindowProperty (dpy, w, xatom, 0, G_MAXLONG, FALSE, XA_CARDINAL,
                &type, &format, &n_cardinals, &bytes_after,
                (unsigned char **) &cardinals) != Success) || (type == None))
    {
        return FALSE;
    }

    if (!check_type_and_format (dpy, w, xatom, 32, XA_CARDINAL, -1, format,
            type))
    {
        XFree (cardinals);
        return FALSE;
    }

    *cardinals_p = cardinals;
    *n_cardinals_p = n_cardinals;

    return TRUE;
}

void
set_net_workarea (Display * dpy, int screen, int nb_workspaces, CARD32 * m)
{
    CARD32 *data, *ptr;
    int i, j;

    TRACE ("entering set_net_workarea");
    j = (nb_workspaces ? nb_workspaces : 1);
    data = (CARD32 *) malloc (sizeof (CARD32) * j * 4);
    if (!data)
    {
        gdk_beep ();
        return;
    }
    ptr = data;
    for (i = 0; i < j; i++)
    {
        *ptr++ = m[MARGIN_LEFT];
        *ptr++ = m[MARGIN_TOP];
        *ptr++ =
            MyDisplayFullWidth (dpy,
            screen) - (m[MARGIN_LEFT] + m[MARGIN_RIGHT]);
        *ptr++ =
            MyDisplayFullHeight (dpy,
            screen) - (m[MARGIN_TOP] + m[MARGIN_BOTTOM]);
    }
    XChangeProperty (dpy, RootWindow (dpy, screen), net_workarea, XA_CARDINAL,
        32, PropModeReplace, (unsigned char *) data, j * 4);
    free (data);
}

void
init_net_desktop_params (Display * dpy, int screen, int workspace)
{
    unsigned long data[2];
    TRACE ("entering init_net_desktop_params");
    data[0] = MyDisplayFullWidth (dpy, screen);
    data[1] = MyDisplayFullHeight (dpy, screen);
    XChangeProperty (dpy, RootWindow (dpy, screen), net_desktop_geometry,
        XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 2);
    data[0] = 0;
    data[1] = 0;
    XChangeProperty (dpy, RootWindow (dpy, screen), net_desktop_viewport,
        XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 2);
    /* TBD : See why this prevents kdesktop from working properly */
    data[0] = workspace;
    XChangeProperty (dpy, RootWindow (dpy, screen), net_current_desktop,
        XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 1);
}

void
set_utf8_string_hint (Display * dpy, Window w, Atom atom, const char *val)
{
    TRACE ("entering set_utf8_string_hint");

    XChangeProperty (dpy, w, atom, utf8_string, 8, PropModeReplace,
        (unsigned char *) val, strlen (val) + 1);
}

void
getTransientFor (Display * dpy, int screen, Window w, Window * transient_for)
{
    TRACE ("entering getTransientFor");

    if (XGetTransientForHint (dpy, w, transient_for))
    {
        if (*transient_for == None)
        {
            /* Treat transient for "none" same as transient for root */
            *transient_for = RootWindow (dpy, screen);
        }
        else if (*transient_for == w)
        {
            /* Very unlikely to happen, but who knows, maybe a braindead app */
            *transient_for = None;
        }
    }
    else
    {
        *transient_for = None;
    }

    TRACE ("Window (0x%lx) is transient for (0x%lx)", w, *transient_for);
}

gboolean
get_utf8_string (Display * dpy, Window w, Atom xatom, char **str_p)
{
    Atom type;
    int format;
    unsigned long bytes_after;
    unsigned char *str;
    unsigned long n_items;

    TRACE ("entering get_utf8_string");

    *str_p = NULL;
    if ((XGetWindowProperty (dpy, w, xatom, 0, G_MAXLONG, FALSE, utf8_string,
                &type, &format, &n_items, &bytes_after,
                (unsigned char **) &str) != Success) || (type == None))
    {
        TRACE ("no utf8_string value provided");
        return FALSE;
    }

    if (!check_type_and_format (dpy, w, xatom, 8, utf8_string, -1, format,
            type))
    {
        TRACE ("utf8_string value invalid");
        if (str)
        {
            XFree (str);
        }
        return FALSE;
    }

    if (!g_utf8_validate (str, n_items, NULL))
    {
        char *name;

        name = XGetAtomName (dpy, xatom);
        if (name)
        {
            TRACE
                ("Property %s on window 0x%lx contains invalid UTF-8 characters",
                name, w);
            XFree (name);
        }
        XFree (str);

        return FALSE;
    }

    *str_p = str;

    return TRUE;
}

static char *
text_property_to_utf8 (Display * dpy, const XTextProperty * prop)
{
    char **list;
    int count;
    char *retval;

    TRACE ("entering text_property_to_utf8");

    list = NULL;
    if ((count =
            gdk_text_property_to_utf8_list (gdk_x11_xatom_to_atom (prop->
                    encoding), prop->format, prop->value, prop->nitems,
                &list)) == 0)
    {
        TRACE ("gdk_text_property_to_utf8_list returned 0");
        return NULL;
    }
    retval = list[0];
    list[0] = g_strdup ("");
    g_strfreev (list);

    return retval;
}

static char *
get_text_property (Display * dpy, Window w, Atom a)
{
    XTextProperty text;
    char *retval;

    TRACE ("entering get_text_property");
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
        TRACE ("XGetTextProperty() failed");
    }

    return retval;
}


void
getWindowName (Display * dpy, Window w, char **name)
{
    char *str;

    TRACE ("entering getWindowName");

    g_return_if_fail (name != NULL);
    *name = NULL;
    g_return_if_fail (w != None);

    if (get_utf8_string (dpy, w, net_wm_name, &str))
    {
        *name = strdup (str);
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
        *name = strdup ("");
    }
}

gboolean
getWindowRole (Display * dpy, Window window, char **role)
{
    XTextProperty tp;

    TRACE ("entering GetWindowRole");

    g_return_val_if_fail (role != NULL, FALSE);
    *role = NULL;
    g_return_val_if_fail (window != None, FALSE);

    if (XGetTextProperty (dpy, window, &tp, wm_window_role))
    {
        if ((tp.value) && (tp.encoding == XA_STRING) && (tp.format == 8)
            && (tp.nitems != 0))
        {
            *role = strdup ((char *) tp.value);
            XFree (tp.value);
            return TRUE;
        }
    }

    return FALSE;
}

Window
getClientLeader (Display * dpy, Window window)
{
    Window client_leader = None;
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *prop = NULL;

    TRACE ("entering getClientLeader");

    g_return_val_if_fail (window != None, None);

    if (XGetWindowProperty (dpy, window, wm_client_leader, 0L, 1L, FALSE,
            AnyPropertyType, &actual_type, &actual_format, &nitems,
            &bytes_after, &prop) == Success)
    {
        if ((prop) && (actual_type == XA_WINDOW) && (actual_format == 32)
            && (nitems == 1) && (bytes_after == 0))
        {
            client_leader = *((Window *) prop);
        }
        if (prop)
        {
            XFree (prop);
        }
    }
    return client_leader;
}

gboolean
getClientID (Display * dpy, Window window, char **client_id)
{
    Window id;
    XTextProperty tp;

    TRACE ("entering getClientID");

    g_return_val_if_fail (client_id != NULL, FALSE);
    *client_id = NULL;
    g_return_val_if_fail (window != None, FALSE);

    if ((id = getClientLeader (dpy, window)))
    {
        if (XGetTextProperty (dpy, id, &tp, sm_client_id))
        {
            if (tp.encoding == XA_STRING && tp.format == 8 && tp.nitems != 0)
            {
                *client_id = strdup ((char *) tp.value);
                XFree (tp.value);
                return TRUE;
            }
        }
    }

    return FALSE;
}

gboolean
getWindowCommand (Display * dpy, Window window, char ***argv, int *argc)
{
    Window id;

    *argc = 0;
    g_return_val_if_fail (window != None, FALSE);

    if (XGetCommand (dpy, window, argv, argc) && (*argc > 0))
    {
        return TRUE;
    }
    if ((id = getClientLeader (dpy, window)))
    {
        if (XGetCommand (dpy, id, argv, argc) && (*argc > 0))
        {
            return TRUE;
        }
    }
    return FALSE;
}

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
gboolean
getWindowStartupId (Display * dpy, Window window, char **startup_id)
{
    XTextProperty tp;

    TRACE ("entering getWindowStartupId");

    g_return_val_if_fail (startup_id != NULL, FALSE);
    *startup_id = NULL;
    g_return_val_if_fail (window != None, FALSE);

    if (XGetTextProperty (dpy, window, &tp, net_startup_id))
    {
        if ((tp.value) && (tp.encoding == XA_STRING) && (tp.format == 8)
            && (tp.nitems != 0))
        {
            *startup_id = strdup ((char *) tp.value);
            XFree (tp.value);
            return TRUE;
        }
    }

    return FALSE;
}
#endif
