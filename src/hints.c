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
 
        oroborus - (c) 2001 Ken Lynch
        Metacity - (c) 2001 Havoc Pennington
        xfwm4    - (c) 2002-2005 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libxfce4util/libxfce4util.h> 
#include "display.h"
#include "hints.h"

static gboolean
check_type_and_format (int expected_format, Atom expected_type, int n_items, int format, Atom type)
{
    if ((expected_format == format) && (expected_type == type) && (n_items < 0 || n_items > 0))
    {
        return TRUE;
    }
    return FALSE;
}

unsigned long
getWMState (DisplayInfo *display_info, Window w)
{
    Atom real_type;
    int real_format;
    unsigned long items_read, items_left;
    unsigned long *data = NULL, state = WithdrawnState;

    TRACE ("entering getWmState");

    if ((XGetWindowProperty (display_info->dpy, w, display_info->atoms[WM_STATE], 
                             0, 3L, FALSE, display_info->atoms[WM_STATE],
                             &real_type, &real_format, &items_read, &items_left,
                             (unsigned char **) &data) == Success) && (items_read))
    {
        state = *data;
        if (data)
        {
            XFree (data);
        }
    }
    return state;
}

void
setWMState (DisplayInfo *display_info, Window w, unsigned long state)
{
    CARD32 data[2];

    TRACE ("entering setWmState");

    data[0] = state;
    data[1] = None;

    XChangeProperty (display_info->dpy, w, display_info->atoms[WM_STATE], 
                     display_info->atoms[WM_STATE], 32, PropModeReplace,
                     (unsigned char *) data, 2);
}

PropMwmHints *
getMotifHints (DisplayInfo *display_info, Window w)
{
    Atom real_type;
    int real_format;
    unsigned long items_read, items_left;
    long *data = NULL;
    PropMwmHints *result = NULL;

    TRACE ("entering getMotifHints");

    if ((XGetWindowProperty (display_info->dpy, w, display_info->atoms[MOTIF_WM_HINTS], 0L, MWM_HINTS_ELEMENTS, 
                FALSE, display_info->atoms[MOTIF_WM_HINTS], &real_type, &real_format, &items_read,
                &items_left, (unsigned char **) &data) == Success))
    {
        if (items_read >= MWM_HINTS_ELEMENTS)
        {    
            result = g_new0(PropMwmHints, 1);
            memcpy (result, data, sizeof (PropMwmHints));
        }
        if (data)        
        {
            XFree (data);
        }
    }
    return result;
}

unsigned int
getWMProtocols (DisplayInfo *display_info, Window w)
{
    Atom *protocols = None, *ap;
    int i, n;
    Atom atype;
    int aformat;
    unsigned int result = 0;
    unsigned long bytes_remain, nitems;

    TRACE ("entering getWMProtocols");

    if (XGetWMProtocols (display_info->dpy, w, &protocols, &n))
    {
        for (i = 0, ap = protocols; i < n; i++, ap++)
        {
            if (*ap == display_info->atoms[WM_TAKE_FOCUS])
            {
                result |= WM_PROTOCOLS_TAKE_FOCUS;
            }
            if (*ap == display_info->atoms[WM_DELETE_WINDOW])
            {
                result |= WM_PROTOCOLS_DELETE_WINDOW;
            }
            /* KDE extension */
            if (*ap == display_info->atoms[NET_WM_CONTEXT_HELP])
            {
                result |= WM_PROTOCOLS_CONTEXT_HELP;
            }
        }
    }
    else
    {
        if ((XGetWindowProperty (display_info->dpy, w, display_info->atoms[WM_PROTOCOLS], 0L, 10L, FALSE,
                    display_info->atoms[WM_PROTOCOLS], &atype, &aformat, &nitems, &bytes_remain,
                    (unsigned char **) &protocols)) == Success)
        {
            for (i = 0, ap = protocols; i < nitems; i++, ap++)
            {
                if (*ap == display_info->atoms[WM_TAKE_FOCUS])
                {
                    result |= WM_PROTOCOLS_TAKE_FOCUS;
                }
                if (*ap == display_info->atoms[WM_DELETE_WINDOW])
                {
                    result |= WM_PROTOCOLS_DELETE_WINDOW;
                }
                /* KDE extension */
                if (*ap == display_info->atoms[NET_WM_CONTEXT_HELP])
                {
                    result |= WM_PROTOCOLS_CONTEXT_HELP;
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

gboolean
getHint (DisplayInfo *display_info, Window w, int atom_id, long *value)
{
    Atom real_type;
    int real_format;
    gboolean success = FALSE;
    unsigned long items_read, items_left;
    long *data = NULL;

    g_return_val_if_fail (((atom_id > 0) && (atom_id < NB_ATOMS)), FALSE);
    TRACE ("entering getHint");

    *value = 0;

    if ((XGetWindowProperty (display_info->dpy, w, display_info->atoms[atom_id], 0L, 1L, 
                             FALSE, XA_CARDINAL, &real_type, &real_format, &items_read, &items_left,
                             (unsigned char **) &data) == Success) && (items_read))
    {
        *value = *data;
        if (data)        
        {
            XFree (data);
        }
        success = TRUE;
    }
    return success;
}

void
setHint (DisplayInfo *display_info, Window w, int atom_id, long value)
{
    g_return_if_fail ((atom_id > 0) && (atom_id < NB_ATOMS));
    TRACE ("entering setHint");

    XChangeProperty (display_info->dpy, w, display_info->atoms[atom_id], XA_CARDINAL, 
                     32, PropModeReplace, (unsigned char *) &value, 1);
}

void
getDesktopLayout (DisplayInfo *display_info, Window root, int ws_count, NetWmDesktopLayout * layout)
{
    Atom real_type;
    int real_format;
    gboolean success = FALSE;
    unsigned long items_read, items_left;
    unsigned long orientation, cols, rows, start;
    unsigned long *data = NULL;

    TRACE ("entering getDesktopLayout");

    if ((XGetWindowProperty (display_info->dpy, root, display_info->atoms[NET_DESKTOP_LAYOUT],
                0L, 4L, FALSE, XA_CARDINAL,
                &real_type, &real_format, &items_read, &items_left,
                (unsigned char **) &data) == Success) && (items_read >= 3))
    {
        do
        {
            orientation = data[0];
            cols = data[1];
            rows = data[2];
            start = (items_read >= 4) ? data[3] : NET_WM_TOPLEFT;

            if (orientation > NET_WM_ORIENTATION_VERT) 
            {
                break;
            }
            
            if (start > NET_WM_BOTTOMLEFT)
            {
                break;
            }
            
            if ((rows == 0) && (cols == 0))
            {
                break;
            }

            if (rows == 0)
            {
                rows = (ws_count - 1) / cols + 1;
            }

            if (cols == 0)
            {
                cols = (ws_count - 1) / rows + 1;
            }

            layout->orientation = orientation;
            layout->cols = cols;
            layout->rows = rows;
            layout->start = start;
            success = TRUE;
        } while (0);

        XFree (data);
    }

    if (!success)
    {
        /* Assume HORZ, TOPLEFT, one row by default */
        layout->orientation = NET_WM_ORIENTATION_HORZ;
        layout->cols = ws_count;
        layout->rows = 1;
        layout->start = NET_WM_TOPLEFT;
    }
}

void
getGnomeDesktopMargins (DisplayInfo *display_info, Window root, int * m)
{
    Atom real_type;
    int real_format;
    unsigned long items_read, items_left;
    unsigned long *data = NULL;

    TRACE ("entering getGnomeDesktopMargins");

    if ((XGetWindowProperty (display_info->dpy, root,
                display_info->atoms[GNOME_PANEL_DESKTOP_AREA], 0L, 4L, FALSE, XA_CARDINAL,
                &real_type, &real_format, &items_read, &items_left,
                (unsigned char **) &data) == Success) && (items_read >= 4))
    {
        m[0] = (int) data[0];
        m[1] = (int) data[1];
        m[2] = (int) data[2];
        m[3] = (int) data[3];
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
setGnomeProtocols (DisplayInfo *display_info, Window root, Window w)
{
    Atom atoms[1];

    atoms[0] = display_info->atoms[WIN_LAYER];
    XChangeProperty (display_info->dpy, root, display_info->atoms[WIN_PROTOCOLS], XA_ATOM,
                     32, PropModeReplace, (unsigned char *) atoms, 1);
    setHint (display_info, w, WIN_SUPPORTING_WM_CHECK, w);
    setHint (display_info, root, WIN_SUPPORTING_WM_CHECK, w);
}

void
setNetSupportedHint (DisplayInfo *display_info, Window root, Window check_win)
{
    Atom atoms[64];
    unsigned long data[1];
    int i = 0;

    atoms[i++] = display_info->atoms[NET_ACTIVE_WINDOW];
    atoms[i++] = display_info->atoms[NET_CLIENT_LIST];
    atoms[i++] = display_info->atoms[NET_CLIENT_LIST_STACKING];
    atoms[i++] = display_info->atoms[NET_CLOSE_WINDOW];
    atoms[i++] = display_info->atoms[NET_CURRENT_DESKTOP];
    atoms[i++] = display_info->atoms[NET_DESKTOP_GEOMETRY];
    atoms[i++] = display_info->atoms[NET_DESKTOP_LAYOUT];
    atoms[i++] = display_info->atoms[NET_DESKTOP_NAMES];
    atoms[i++] = display_info->atoms[NET_DESKTOP_VIEWPORT];
    atoms[i++] = display_info->atoms[NET_FRAME_EXTENTS];
    atoms[i++] = display_info->atoms[NET_NUMBER_OF_DESKTOPS];
    atoms[i++] = display_info->atoms[NET_REQUEST_FRAME_EXTENTS];
    atoms[i++] = display_info->atoms[NET_SHOWING_DESKTOP];
    atoms[i++] = display_info->atoms[NET_SUPPORTED];
    atoms[i++] = display_info->atoms[NET_SUPPORTING_WM_CHECK];
    atoms[i++] = display_info->atoms[NET_WM_ACTION_CHANGE_DESKTOP];
    atoms[i++] = display_info->atoms[NET_WM_ACTION_CLOSE];
    atoms[i++] = display_info->atoms[NET_WM_ACTION_MAXIMIZE_HORZ];
    atoms[i++] = display_info->atoms[NET_WM_ACTION_MAXIMIZE_VERT];
    atoms[i++] = display_info->atoms[NET_WM_ACTION_MOVE];
    atoms[i++] = display_info->atoms[NET_WM_ACTION_RESIZE];
    atoms[i++] = display_info->atoms[NET_WM_ACTION_SHADE];
    atoms[i++] = display_info->atoms[NET_WM_ACTION_STICK];
    atoms[i++] = display_info->atoms[NET_WM_ALLOWED_ACTIONS];
    atoms[i++] = display_info->atoms[NET_WM_DESKTOP];
    atoms[i++] = display_info->atoms[NET_WM_ICON];
    atoms[i++] = display_info->atoms[NET_WM_ICON_GEOMETRY];
    atoms[i++] = display_info->atoms[NET_WM_ICON_NAME];
    atoms[i++] = display_info->atoms[NET_WM_NAME];
    atoms[i++] = display_info->atoms[NET_WM_STATE];
    atoms[i++] = display_info->atoms[NET_WM_STATE_ABOVE];
    atoms[i++] = display_info->atoms[NET_WM_STATE_BELOW];
    atoms[i++] = display_info->atoms[NET_WM_STATE_DEMANDS_ATTENTION];
    atoms[i++] = display_info->atoms[NET_WM_STATE_FULLSCREEN];
    atoms[i++] = display_info->atoms[NET_WM_STATE_HIDDEN];
    atoms[i++] = display_info->atoms[NET_WM_STATE_MAXIMIZED_HORZ];
    atoms[i++] = display_info->atoms[NET_WM_STATE_MAXIMIZED_VERT];
    atoms[i++] = display_info->atoms[NET_WM_STATE_MODAL];
    atoms[i++] = display_info->atoms[NET_WM_STATE_SHADED];
    atoms[i++] = display_info->atoms[NET_WM_STATE_SKIP_PAGER];
    atoms[i++] = display_info->atoms[NET_WM_STATE_SKIP_TASKBAR];
    atoms[i++] = display_info->atoms[NET_WM_STATE_STICKY];
    atoms[i++] = display_info->atoms[NET_WM_STRUT];
    atoms[i++] = display_info->atoms[NET_WM_STRUT_PARTIAL];
    atoms[i++] = display_info->atoms[NET_WM_USER_TIME];
    atoms[i++] = display_info->atoms[NET_WM_WINDOW_TYPE];
    atoms[i++] = display_info->atoms[NET_WM_WINDOW_TYPE_DESKTOP];
    atoms[i++] = display_info->atoms[NET_WM_WINDOW_TYPE_DIALOG];
    atoms[i++] = display_info->atoms[NET_WM_WINDOW_TYPE_DOCK];
    atoms[i++] = display_info->atoms[NET_WM_WINDOW_TYPE_MENU];
    atoms[i++] = display_info->atoms[NET_WM_WINDOW_TYPE_NORMAL];
    atoms[i++] = display_info->atoms[NET_WM_WINDOW_TYPE_SPLASH];
    atoms[i++] = display_info->atoms[NET_WM_WINDOW_TYPE_TOOLBAR];
    atoms[i++] = display_info->atoms[NET_WM_WINDOW_TYPE_UTILITY];
    atoms[i++] = display_info->atoms[NET_WORKAREA];
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    atoms[i++] = display_info->atoms[NET_STARTUP_ID];
#endif

    data[0] = check_win;
    XChangeProperty (display_info->dpy, root, display_info->atoms[NET_SUPPORTED], 
                     XA_ATOM, 32, PropModeReplace, (unsigned char *) atoms, i);
    XChangeProperty (display_info->dpy, check_win, display_info->atoms[NET_SUPPORTING_WM_CHECK], 
                     XA_WINDOW, 32, PropModeReplace, (unsigned char *) data, 1);
    XChangeProperty (display_info->dpy, root, display_info->atoms[NET_SUPPORTING_WM_CHECK],
                     XA_WINDOW, 32, PropModeReplace, (unsigned char *) data, 1);
}

gboolean
getAtomList (DisplayInfo *display_info, Window w, int atom_id, Atom ** atoms_p, int *n_atoms_p)
{
    Atom type;
    int format;
    unsigned long n_atoms;
    unsigned long bytes_after;
    Atom *atoms;

    *atoms_p = NULL;
    *n_atoms_p = 0;

    g_return_val_if_fail (((atom_id > 0) && (atom_id < NB_ATOMS)), FALSE);
    TRACE ("entering getAtomList()");

    if ((XGetWindowProperty (display_info->dpy, w, display_info->atoms[atom_id], 
                             0, G_MAXLONG, FALSE, XA_ATOM, &type, &format, &n_atoms, 
                             &bytes_after, (unsigned char **) &atoms) != Success) || (type == None))
    {
        return FALSE;
    }

    if (!check_type_and_format (32, XA_ATOM, -1, format, type))
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
getCardinalList (DisplayInfo *display_info, Window w, int atom_id, unsigned long **cardinals_p, int *n_cardinals_p)
{
    Atom type;
    int format;
    unsigned long n_cardinals;
    unsigned long bytes_after;
    unsigned long *cardinals;

    *cardinals_p = NULL;
    *n_cardinals_p = 0;

    g_return_val_if_fail (((atom_id > 0) && (atom_id < NB_ATOMS)), FALSE);
    TRACE ("entering getCardinalList()");

    if ((XGetWindowProperty (display_info->dpy, w, display_info->atoms[atom_id], 
                             0, G_MAXLONG, FALSE, XA_CARDINAL,
                             &type, &format, &n_cardinals, &bytes_after,
                             (unsigned char **) &cardinals) != Success) || (type == None))
    {
        return FALSE;
    }

    if (!check_type_and_format (32, XA_CARDINAL, -1, format, type))
    {
        XFree (cardinals);
        return FALSE;
    }

    *cardinals_p = cardinals;
    *n_cardinals_p = n_cardinals;

    return TRUE;
}

void
setNetWorkarea (DisplayInfo *display_info, Window root, int nb_workspaces, int width, int height, int * m)
{
    unsigned long *data, *ptr;
    int i, j;

    TRACE ("entering setNetWorkarea");
    j = (nb_workspaces ? nb_workspaces : 1);
    data = (unsigned long *) g_new0 (unsigned long, j * 4);
    ptr = data;
    for (i = 0; i < j; i++)
    {
        *ptr++ = (unsigned long) m[LEFT];
        *ptr++ = (unsigned long) m[TOP];
        *ptr++ = (unsigned long) (width  - (m[LEFT] + m[RIGHT]));
        *ptr++ = (unsigned long) (height - (m[TOP] + m[BOTTOM]));
    }
    XChangeProperty (display_info->dpy, root, display_info->atoms[NET_WORKAREA], 
                     XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, j * 4);
    g_free (data);
}

void
setNetFrameExtents (DisplayInfo *display_info, Window w, int top, int left, int right, int bottom)
{
    unsigned long data[4] = { 0, 0, 0, 0 };

    TRACE ("entering setNetFrameExtents");
    data[0] = (unsigned long) left;
    data[1] = (unsigned long) right;
    data[2] = (unsigned long) top;
    data[3] = (unsigned long) bottom;
    XChangeProperty (display_info->dpy, w, display_info->atoms[NET_FRAME_EXTENTS], 
                     XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 4);
}

void
initNetDesktopInfo (DisplayInfo *display_info, Window root, int workspace, int width, int height)
{
    unsigned long data[2];
    TRACE ("entering initNetDesktopInfo");
    data[0] = width;
    data[1] = height;
    XChangeProperty (display_info->dpy, root, display_info->atoms[NET_DESKTOP_GEOMETRY],
                     XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 2);
    data[0] = 0;
    data[1] = 0;
    XChangeProperty (display_info->dpy, root, display_info->atoms[NET_DESKTOP_VIEWPORT],
                     XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 2);
    data[0] = workspace;
    XChangeProperty (display_info->dpy, root, display_info->atoms[NET_CURRENT_DESKTOP],
                     XA_CARDINAL, 32, PropModeReplace, (unsigned char *) data, 1);
}

void
setUTF8StringHint (DisplayInfo *display_info, Window w, int atom_id, const char *val)
{
    g_return_if_fail ((atom_id > 0) && (atom_id < NB_ATOMS));
    TRACE ("entering set_utf8_string_hint");

    XChangeProperty (display_info->dpy, w, display_info->atoms[atom_id], 
                     display_info->atoms[UTF8_STRING], 8, PropModeReplace,
                     (unsigned char *) val, strlen (val) + 1);
}

void
getTransientFor (DisplayInfo *display_info, Window root, Window w, Window * transient_for)
{
    TRACE ("entering getTransientFor");

    if (XGetTransientForHint (display_info->dpy, w, transient_for))
    {
        if (*transient_for == None)
        {
            /* Treat transient for "none" same as transient for root */
            *transient_for = root;
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
getUTF8String (DisplayInfo *display_info, Window w, int atom_id, char **str_p, int *length)
{
    Atom type;
    int format;
    unsigned long bytes_after;
    unsigned char *str;
    unsigned long n_items;

    g_return_val_if_fail (((atom_id > 0) && (atom_id < NB_ATOMS)), FALSE);
    TRACE ("entering getUTF8String");

    *str_p = NULL;
    if ((XGetWindowProperty (display_info->dpy, w, display_info->atoms[atom_id], 
                             0, G_MAXLONG, FALSE, display_info->atoms[UTF8_STRING], &type, 
                             &format, &n_items, &bytes_after, (unsigned char **) &str) != Success) || (type == None))
    {
        TRACE ("no utf8_string value provided");
        return FALSE;
    }

    if (!check_type_and_format (8, display_info->atoms[UTF8_STRING], MAX_TITLE_LENGTH, format, type))
    {
        TRACE ("utf8_string value invalid");
        if (str)
        {
            XFree (str);
        }
        return FALSE;
    }

    if (!g_utf8_validate (str, MAX_TITLE_LENGTH, NULL))
    {
        char *name;

        name = XGetAtomName (display_info->dpy, display_info->atoms[atom_id]);
        if (name)
        {
            TRACE ("Property %s on window 0x%lx contains invalid UTF-8 characters", name, w);
            XFree (name);
        }
        XFree (str);

        return FALSE;
    }

    if (str)
    {
        xfce_utf8_remove_controls((gchar *) str, MAX_TITLE_LENGTH, NULL);
    }
    
    *str_p = str;
    *length = n_items;

    return TRUE;
}

static gchar *
internal_utf8_strndup (const gchar *src, gssize max_len)
{
    const gchar *s = src;

    if (max_len <= 0)
	return g_strdup (src);

    while (max_len && *s)
    {
	s = g_utf8_next_char (s);
	max_len--;
    }

    return g_strndup (src, s - src);
}

static char *
text_property_to_utf8 (DisplayInfo *display_info, const XTextProperty * prop)
{
    char **list;
    int count;
    char *retval;

    TRACE ("entering text_property_to_utf8");

    list = NULL;
    count = gdk_text_property_to_utf8_list (gdk_x11_xatom_to_atom (prop->encoding), 
                                            prop->format, prop->value, prop->nitems, &list);
    if (count == 0)
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
get_text_property (DisplayInfo *display_info, Window w, Atom a)
{
    XTextProperty text;
    char *retval;

    TRACE ("entering get_text_property");
    text.nitems = 0;
    if (XGetTextProperty (display_info->dpy, w, &text, a))
    {
        retval = text_property_to_utf8 (display_info, &text);
        if (retval)
        {
            xfce_utf8_remove_controls((gchar *) retval, MAX_TITLE_LENGTH, NULL);
        }
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
getWindowName (DisplayInfo *display_info, Window w, char **name)
{
    char *str;
    int len;

    TRACE ("entering getWindowName");

    g_return_if_fail (name != NULL);
    *name = NULL;
    g_return_if_fail (w != None);

    if (getUTF8String (display_info, w, NET_WM_NAME, &str, &len))
    {
        *name = internal_utf8_strndup (str, MAX_TITLE_LENGTH);
        XFree (str);
        return;
    }
    str = get_text_property (display_info, w, XA_WM_NAME);
    if (str)
    {
        *name = internal_utf8_strndup (str, MAX_TITLE_LENGTH);
        XFree (str);
    }
    else
    {
        *name = strdup ("");
    }
}

gboolean
checkKdeSystrayWindow(DisplayInfo *display_info, Window window)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    Window trayIconForWindow;

    TRACE ("entering GetWindowRole");
    g_return_val_if_fail (window != None, FALSE);
    
    XGetWindowProperty(display_info->dpy, window, display_info->atoms[KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR], 
                       0L, sizeof(Window), FALSE, XA_WINDOW, &actual_type, &actual_format, 
                       &nitems, &bytes_after, (unsigned char **)&trayIconForWindow);

    if ((actual_format == None) || 
        (actual_type != XA_WINDOW) || 
        (trayIconForWindow == None))
    {
        return FALSE;
    }
    return TRUE;
}

void
sendSystrayReqDock(DisplayInfo *display_info, Window window, Window systray)
{
    XClientMessageEvent xev;

    TRACE ("entering sendSystrayReqDock");
    g_return_if_fail (window != None);
    g_return_if_fail (systray != None);

    xev.type = ClientMessage;
    xev.window = systray;
    xev.message_type = display_info->atoms[NET_SYSTEM_TRAY_OPCODE];
    xev.format = 32;
    xev.data.l[0] = CurrentTime;
    xev.data.l[1] = 0; /* SYSTEM_TRAY_REQUEST_DOCK */
    xev.data.l[2] = window;
    xev.data.l[3] = 0; /* Nada */
    xev.data.l[4] = 0; /* Niet */

    XSendEvent (display_info->dpy, systray, FALSE, NoEventMask, (XEvent *) & xev);
}

Window
getSystrayWindow (DisplayInfo *display_info, Atom net_system_tray_selection)
{
    Window systray_win = None;

    TRACE ("entering getSystrayWindow");

    systray_win = XGetSelectionOwner (display_info->dpy, net_system_tray_selection);
    if (systray_win)
    {
        XSelectInput (display_info->dpy, systray_win, StructureNotifyMask);
    }
    TRACE ("New systray window:  0x%lx", systray_win);
    return systray_win;
}

gboolean
getWindowRole (DisplayInfo *display_info, Window window, char **role)
{
    XTextProperty tp;

    TRACE ("entering GetWindowRole");

    g_return_val_if_fail (role != NULL, FALSE);
    *role = NULL;
    g_return_val_if_fail (window != None, FALSE);

    if (XGetTextProperty (display_info->dpy, window, &tp, display_info->atoms[WM_WINDOW_ROLE]))
    {
        if (tp.value)
        {
            if ((tp.encoding == XA_STRING) && (tp.format == 8) && (tp.nitems != 0))
            {
                *role = strdup ((char *) tp.value);
                XFree (tp.value);
                return TRUE;
            }
            XFree (tp.value);
        }
    }

    return FALSE;
}

Window
getClientLeader (DisplayInfo *display_info, Window window)
{
    Window client_leader = None;
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *prop = NULL;

    TRACE ("entering getClientLeader");

    g_return_val_if_fail (window != None, None);

    if (XGetWindowProperty (display_info->dpy, window, display_info->atoms[WM_CLIENT_LEADER], 
                            0L, 1L, FALSE, AnyPropertyType, &actual_type, &actual_format, &nitems,
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
getNetWMUserTime (DisplayInfo *display_info, Window window, Time *time)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *data = NULL;

    TRACE ("entering getNetWMUserTime");

    g_return_val_if_fail (window != None, None);

    if (XGetWindowProperty (display_info->dpy, window, display_info->atoms[NET_WM_USER_TIME], 
                            0L, 1L, FALSE, XA_CARDINAL, &actual_type, &actual_format, &nitems,
                            &bytes_after, &data) == Success)
    {
        if ((data) && (actual_type == XA_CARDINAL)
            && (nitems == 1) && (bytes_after == 0))
        {
            *time = *((long *) data);
            XFree (data);
            return TRUE;
        }
    }
    *time = 0L;
    return FALSE;
}

gboolean
getClientID (DisplayInfo *display_info, Window window, char **client_id)
{
    Window id;
    XTextProperty tp;

    TRACE ("entering getClientID");

    g_return_val_if_fail (client_id != NULL, FALSE);
    *client_id = NULL;
    g_return_val_if_fail (window != None, FALSE);

    if ((id = getClientLeader (display_info, window)))
    {
        if (XGetTextProperty (display_info->dpy, id, &tp, display_info->atoms[SM_CLIENT_ID]))
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
getWindowCommand (DisplayInfo *display_info, Window window, char ***argv, int *argc)
{
    Window id;

    *argc = 0;
    g_return_val_if_fail (window != None, FALSE);

    if (XGetCommand (display_info->dpy, window, argv, argc) && (*argc > 0))
    {
        return TRUE;
    }
    if ((id = getClientLeader (display_info, window)))
    {
        if (XGetCommand (display_info->dpy, id, argv, argc) && (*argc > 0))
        {
            return TRUE;
        }
    }
    return FALSE;
}

gboolean
getKDEIcon (DisplayInfo *display_info, Window window, Pixmap * pixmap, Pixmap * mask)
{
    Atom type;
    int format;
    unsigned long nitems;
    unsigned long bytes_after;
    Pixmap *icons;

    *pixmap = None;
    *mask = None;

    icons = NULL;
    if (XGetWindowProperty (display_info->dpy, window, display_info->atoms[KWM_WIN_ICON], 
                            0L, G_MAXLONG, FALSE, display_info->atoms[KWM_WIN_ICON], &type, 
                            &format, &nitems, &bytes_after, (unsigned char **)&icons) != Success)
    {
        return FALSE;
    }

    if (type != display_info->atoms[KWM_WIN_ICON])
    {
        if (icons)
        {
            XFree (icons);
        }
        return FALSE;
    }

    *pixmap = icons[0];
    *mask = icons[1];

    XFree (icons);

    return TRUE;
}

gboolean
getRGBIconData (DisplayInfo *display_info, Window window, unsigned long **data, unsigned long *nitems)
{
    Atom type;
    int format;
    unsigned long bytes_after;

    if (XGetWindowProperty (display_info->dpy, window, display_info->atoms[NET_WM_ICON], 
                            0L, G_MAXLONG, FALSE, XA_CARDINAL, &type, &format, nitems,
                            &bytes_after, (unsigned char **)data) != Success)
    {
        *data = NULL;
        return FALSE;
    }
    
    if (type != XA_CARDINAL)
    {
        if (*data)
        {
            XFree (*data);
        }
        *data = NULL;
        return FALSE;
    }

    return TRUE;
}

gboolean
getOpacity (DisplayInfo *display_info, Window window, guint *opacity)
{
    long val = 0;
    
    g_return_val_if_fail (window != None, FALSE);
    g_return_val_if_fail (opacity != NULL, FALSE);
    TRACE ("entering getOpacity");    

    if (getHint (display_info, window, NET_WM_WINDOW_OPACITY, &val))
    {
        *opacity = (guint) val;
        return TRUE;
    }

    return FALSE;
}

gboolean
setCompositingManagerOwner (DisplayInfo *display_info, Window root, Window w)
{
    XClientMessageEvent ev;
    Time server_time;
    int status;
    
    server_time = myDisplayGetCurrentTime (display_info);
    status = XSetSelectionOwner (display_info->dpy, 
                                 display_info->atoms[COMPOSITING_MANAGER],
                                 w, server_time);

    if ((status == BadAtom) || (status == BadWindow))
    {
        return FALSE;
    }

    if (XGetSelectionOwner (display_info->dpy, display_info->atoms[COMPOSITING_MANAGER]) == w)
    {
        ev.type = ClientMessage;
        ev.message_type = display_info->atoms[COMPOSITING_MANAGER];
        ev.format = 32;
        ev.data.l[0] = server_time;
        ev.data.l[1] = display_info->atoms[COMPOSITING_MANAGER];
        ev.data.l[2] = w;
        ev.data.l[3] = 0;
        ev.data.l[4] = 0;
        ev.window = root;

        XSendEvent (display_info->dpy, root, FALSE, StructureNotifyMask, (XEvent *) &ev);

        return TRUE;
    }

    return FALSE;
}

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
gboolean
getWindowStartupId (DisplayInfo *display_info, Window w, char **startup_id)
{
    char *str;
    int len;

    TRACE ("entering getWindowStartupId");

    g_return_val_if_fail (startup_id != NULL, FALSE);
    *startup_id = NULL;
    g_return_val_if_fail (w != None, FALSE);

    if (getUTF8String (display_info, w, NET_STARTUP_ID, &str, &len))
    {
        *startup_id = strdup (str);
        XFree (str);
        return TRUE;
    }

    str = get_text_property (display_info, w, NET_STARTUP_ID);
    if (str)
    {
        *startup_id = strdup (str);
        XFree (str);
        return TRUE;
    }

    return FALSE;
}
#endif
