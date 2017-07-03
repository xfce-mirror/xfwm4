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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xinerama.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_RENDER
#include <X11/extensions/Xrender.h>
#endif

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif

#include <common/xfwm-common.h>

#include "display.h"
#include "screen.h"
#include "misc.h"
#include "mywindow.h"
#include "compositor.h"
#include "ui_style.h"

#ifndef WM_EXITING_TIMEOUT
#define WM_EXITING_TIMEOUT 15 /*seconds */
#endif

gboolean
myScreenCheckWMAtom (ScreenInfo *screen_info, Atom atom)
{
    gchar selection[32];
    Atom wm_sn_atom;

    TRACE ("entering myScreenCheckWMAtom");
    g_snprintf (selection, sizeof (selection), "WM_S%d", screen_info->screen);
    wm_sn_atom = XInternAtom (myScreenGetXDisplay (screen_info), selection, FALSE);

    return (atom == wm_sn_atom);
}

static gboolean
myScreenSetWMAtom (ScreenInfo *screen_info, gboolean replace_wm)
{
    const char *wm_name;
    gchar selection[32];
    gchar *display_name;
    gulong wait, timeout;
    DisplayInfo *display_info;
    XSetWindowAttributes attrs;
    Window current_wm;
    XEvent event;
    Atom wm_sn_atom;


    g_return_val_if_fail (screen_info, FALSE);
    g_return_val_if_fail (screen_info->display_info, FALSE);

    TRACE ("entering myScreenReplaceWM");

    display_info = screen_info->display_info;
    g_snprintf (selection, sizeof (selection), "WM_S%d", screen_info->screen);
    wm_sn_atom = XInternAtom (display_info->dpy, selection, FALSE);
    display_name = xfwm_make_display_name (screen_info->gscr);
    wm_name = gdk_x11_screen_get_window_manager_name (screen_info->gscr);

    XSync (display_info->dpy, FALSE);
    current_wm = XGetSelectionOwner (display_info->dpy, wm_sn_atom);
    if (current_wm)
    {
        if (!replace_wm)
        {
            g_message ("Another Window Manager (%s) is already running on screen %s", wm_name, display_name);
            g_message ("To replace the current window manager, try \"--replace\"");
            g_free (display_name);

            return FALSE;
        }
        gdk_error_trap_push ();
        attrs.event_mask = StructureNotifyMask;
        XChangeWindowAttributes (display_info->dpy, current_wm, CWEventMask, &attrs);
        gdk_flush ();
        if (gdk_error_trap_pop ())
        {
            current_wm = None;
        }
    }

    if (!setXAtomManagerOwner (display_info, wm_sn_atom, screen_info->xroot, screen_info->xfwm4_win))
    {
        g_warning ("Cannot acquire window manager selection on screen %s", display_name);
        g_free (display_name);

        return FALSE;
    }

    /* Waiting for previous window manager to exit */
    if (current_wm)
    {
        g_print ("Waiting for current window manager (%s) on screen %s to exit:", wm_name, display_name);
        wait = 0;
        timeout = WM_EXITING_TIMEOUT * G_USEC_PER_SEC;
        while (wait < timeout)
        {
            if (XCheckWindowEvent (display_info->dpy, current_wm, StructureNotifyMask, &event) && (event.type == DestroyNotify))
            {
                break;
            }
            g_usleep(G_USEC_PER_SEC / 10);
            wait += G_USEC_PER_SEC / 10;
            if (wait % G_USEC_PER_SEC == 0)
            {
              g_print (".");
            }
        }

        if (wait >= timeout)
        {
            g_print(" Failed\n");
            g_warning("Previous window manager (%s) on screen %s is not exiting", wm_name, display_name);
            g_free (display_name);

            return FALSE;
        }
        g_print(" Done\n");
    }
    g_free (display_name);

    return TRUE;
}

ScreenInfo *
myScreenInit (DisplayInfo *display_info, GdkScreen *gscr, unsigned long event_mask, gboolean replace_wm)
{
#ifdef ENABLE_KDE_SYSTRAY_PROXY
    gchar selection[32];
#endif
    ScreenInfo *screen_info;
    GdkWindow *event_win;
    PangoLayout *layout;
    long desktop_visible;
    int i, j;

    g_return_val_if_fail (display_info, NULL);
    g_return_val_if_fail (GDK_IS_SCREEN (gscr), NULL);
    TRACE ("entering myScreenInit");

    screen_info = g_new0 (ScreenInfo, 1);
    screen_info->params = g_new0 (XfwmParams, 1);

    screen_info->display_info = display_info;
    screen_info->gscr = gscr;
    desktop_visible = 0;
    layout = NULL;

    /* Create a GTK window so that we are just like any other GTK application */
    screen_info->gtk_win = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_set_screen (GTK_WINDOW (screen_info->gtk_win), gscr);
    gtk_window_resize (GTK_WINDOW (screen_info->gtk_win), 5, 5);
    gtk_window_move (GTK_WINDOW (screen_info->gtk_win), -1000, -1000);
    gtk_widget_set_name (screen_info->gtk_win, "xfwm");
    gtk_widget_show_now (screen_info->gtk_win);

    /*
     * The first time the first Gtk application on a display uses pango,
     * pango grabs the XServer while it creates the font cache window.
     * Therefore, force the cache window to be created now instead of
     * trying to do it while we have another grab and deadlocking the server.
     */
    layout = gtk_widget_create_pango_layout (screen_info->gtk_win, "-");
    pango_layout_get_pixel_extents (layout, NULL, NULL);
    g_object_unref (G_OBJECT (layout));

    screen_info->xscreen = gdk_x11_screen_get_xscreen (gscr);
    screen_info->xroot = gdk_x11_window_get_xid (gdk_screen_get_root_window (gscr));
    screen_info->screen = gdk_x11_screen_get_screen_number (gscr);
    screen_info->cmap = DefaultColormapOfScreen (screen_info->xscreen);
    screen_info->depth = DefaultDepth (display_info->dpy, screen_info->screen);
    screen_info->visual = DefaultVisual (display_info->dpy, screen_info->screen);
    screen_info->shape_win = (Window) None;
    myScreenComputeSize (screen_info);

    screen_info->xfwm4_win = gdk_x11_window_get_xid (gtk_widget_get_window (screen_info->gtk_win));
    if (!myScreenSetWMAtom (screen_info, replace_wm))
    {
        gtk_widget_destroy (screen_info->gtk_win);
        g_free (screen_info);
        return NULL;
    }

    event_win = eventFilterAddWin (gscr, event_mask);
    if (!event_win)
    {
        gtk_widget_destroy (screen_info->gtk_win);
        g_free (screen_info);
        return NULL;
    }
    gdk_window_set_user_data (event_win, screen_info->gtk_win);

    screen_info->current_ws = 0;
    screen_info->previous_ws = 0;
    screen_info->current_ws = 0;
    screen_info->previous_ws = 0;

    screen_info->margins[STRUTS_TOP] = screen_info->gnome_margins[STRUTS_TOP] = 0;
    screen_info->margins[STRUTS_LEFT] = screen_info->gnome_margins[STRUTS_LEFT] = 0;
    screen_info->margins[STRUTS_RIGHT] = screen_info->gnome_margins[STRUTS_RIGHT] = 0;
    screen_info->margins[STRUTS_BOTTOM] = screen_info->gnome_margins[STRUTS_BOTTOM] = 0;

    screen_info->workspace_count = 0;
    screen_info->workspace_names = NULL;
    screen_info->workspace_names_items = 0;

    screen_info->windows_stack = NULL;
    screen_info->last_raise = NULL;
    screen_info->windows = NULL;
    screen_info->clients = NULL;
    screen_info->client_count = 0;
    screen_info->client_serial = 0L;
    screen_info->button_handler_id = 0L;

    screen_info->key_grabs = 0;
    screen_info->pointer_grabs = 0;

    getHint (display_info, screen_info->xroot, NET_SHOWING_DESKTOP, &desktop_visible);
    screen_info->show_desktop = (desktop_visible != 0);

    /* Create the side windows to detect edge movement */

    /*left*/
    xfwmWindowTemp (screen_info,
                    NULL, 0,
                    screen_info->xroot,
                    &screen_info->sidewalk[0],
                    0, 0,
                    1, screen_info->height,
                    EnterWindowMask,
                    TRUE);

    /*right*/
    xfwmWindowTemp (screen_info,
                    NULL, 0,
                    screen_info->xroot,
                    &screen_info->sidewalk[1],
                    screen_info->width - 1, 0,
                    1, screen_info->height,
                    EnterWindowMask,
                    TRUE);

    /*top*/
    xfwmWindowTemp (screen_info,
                    NULL, 0,
                    screen_info->xroot,
                    &screen_info->sidewalk[2],
                    0, 0,
                    screen_info->width, 1,
                    EnterWindowMask,
                    TRUE);

    /*bottom*/
    xfwmWindowTemp (screen_info,
                    NULL, 0,
                    screen_info->xroot,
                    &screen_info->sidewalk[3],
                    0, screen_info->height - 1,
                    screen_info->width, 1,
                    EnterWindowMask,
                    TRUE);

#ifdef ENABLE_KDE_SYSTRAY_PROXY
    g_snprintf (selection, sizeof (selection), "_NET_SYSTEM_TRAY_S%d", screen_info->screen);
    screen_info->net_system_tray_selection = XInternAtom (display_info->dpy, selection, FALSE);
    screen_info->systray = getSystrayWindow (display_info, screen_info->net_system_tray_selection);
#endif

    screen_info->font_height = 0;
    screen_info->box_gc = None;
    screen_info->title_colors[ACTIVE].gc = NULL;
    screen_info->title_colors[ACTIVE].allocated = FALSE;
    screen_info->title_colors[INACTIVE].gc = NULL;
    screen_info->title_colors[INACTIVE].allocated = FALSE;
    screen_info->title_shadow_colors[ACTIVE].gc = NULL;
    screen_info->title_shadow_colors[ACTIVE].allocated = FALSE;
    screen_info->title_shadow_colors[INACTIVE].gc = NULL;
    screen_info->title_shadow_colors[INACTIVE].allocated = FALSE;

    for (i = 0; i < SIDE_COUNT; i++)
    {
        xfwmPixmapInit (screen_info, &screen_info->sides[i][ACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->sides[i][INACTIVE]);
    }
    for (i = 0; i < CORNER_COUNT; i++)
    {
        xfwmPixmapInit (screen_info, &screen_info->corners[i][ACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->corners[i][INACTIVE]);
    }
    for (i = 0; i < BUTTON_COUNT; i++)
    {
        for (j = 0; j < STATE_COUNT; j++)
        {
            xfwmPixmapInit (screen_info, &screen_info->buttons[i][j]);
        }
    }
    for (i = 0; i < TITLE_COUNT; i++)
    {
        xfwmPixmapInit (screen_info, &screen_info->title[i][ACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->title[i][INACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->top[i][ACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->top[i][INACTIVE]);
    }

    screen_info->monitors_index = NULL;
    myScreenInvalidateMonitorCache (screen_info);
    myScreenRebuildMonitorIndex (screen_info);

    return (screen_info);
}

ScreenInfo *
myScreenClose (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;

    g_return_val_if_fail (screen_info, NULL);
    TRACE ("entering myScreenClose");

    display_info = screen_info->display_info;

    clientUnframeAll (screen_info);
    compositorUnmanageScreen (screen_info);
    closeSettings (screen_info);

    if (screen_info->workspace_names)
    {
        g_strfreev (screen_info->workspace_names);
    }
    screen_info->workspace_names = NULL;
    screen_info->workspace_names_items = 0;

    if (screen_info->shape_win != None)
    {
        XDestroyWindow (display_info->dpy, screen_info->shape_win);
        screen_info->shape_win = (Window) None;
    }

    xfwmWindowDelete (&screen_info->sidewalk[0]);
    xfwmWindowDelete (&screen_info->sidewalk[1]);
    xfwmWindowDelete (&screen_info->sidewalk[2]);
    xfwmWindowDelete (&screen_info->sidewalk[3]);
    XSetInputFocus (display_info->dpy, screen_info->xroot, RevertToPointerRoot, CurrentTime);

    g_free (screen_info->params);
    screen_info->params = NULL;

    gtk_widget_destroy (screen_info->gtk_win);
    screen_info->gtk_win = NULL;

    g_list_free (screen_info->windows_stack);
    screen_info->windows_stack = NULL;

    g_list_free (screen_info->windows);
    screen_info->windows = NULL;

    if (screen_info->monitors_index)
    {
        g_array_free (screen_info->monitors_index, TRUE);
        screen_info->monitors_index = NULL;
    }

    return (screen_info);
}

Display *
myScreenGetXDisplay (ScreenInfo *screen_info)
{
    DisplayInfo *display_info;

    g_return_val_if_fail (screen_info, NULL);
    g_return_val_if_fail (screen_info->display_info, NULL);
    TRACE ("entering myScreenGetXDisplay");

    display_info = screen_info->display_info;
    return display_info->dpy;
}

GtkWidget *
myScreenGetGtkWidget (ScreenInfo *screen_info)
{
    g_return_val_if_fail (screen_info, NULL);
    TRACE ("entering myScreenGetGtkWidget");

    return screen_info->gtk_win;
}

GdkWindow *
myScreenGetGdkWindow (ScreenInfo *screen_info)
{
    g_return_val_if_fail (screen_info, NULL);
    TRACE ("entering myScreenGetGdkWindow");

    return gtk_widget_get_window (screen_info->gtk_win);
}

gboolean
myScreenGrabKeyboard (ScreenInfo *screen_info, guint32 timestamp)
{
    gboolean grab;

    g_return_val_if_fail (screen_info, FALSE);

    TRACE ("entering myScreenGrabKeyboard");

    grab = TRUE;
    if (screen_info->key_grabs == 0)
    {
        grab = (XGrabKeyboard (myScreenGetXDisplay (screen_info),
                               screen_info->xroot,
                               TRUE,
                               GrabModeAsync, GrabModeAsync,
                               (Time) timestamp) == GrabSuccess);
    }
    screen_info->key_grabs++;
    TRACE ("global key grabs %i", screen_info->key_grabs);

    return grab;
}

gboolean
myScreenGrabPointer (ScreenInfo *screen_info, gboolean owner_events, unsigned int event_mask, Cursor cursor, guint32 timestamp)
{
    gboolean grab;

    g_return_val_if_fail (screen_info, FALSE);
    TRACE ("entering myScreenGrabPointer");

    grab = TRUE;
    if (screen_info->pointer_grabs == 0)
    {
        grab = (XGrabPointer (myScreenGetXDisplay (screen_info),
                              screen_info->xroot,
                              owner_events, event_mask,
                              GrabModeAsync, GrabModeAsync,
                              screen_info->xroot,
                              cursor,
                              (Time) timestamp) == GrabSuccess);
    }
    screen_info->pointer_grabs++;
    TRACE ("global pointer grabs %i", screen_info->pointer_grabs);

    return grab;
}

gboolean
myScreenChangeGrabPointer (ScreenInfo *screen_info, unsigned int event_mask, Cursor cursor, guint32 timestamp)
{
    gboolean grab;

    g_return_val_if_fail (screen_info, FALSE);
    TRACE ("entering myScreenChangeGrabPointer");

    grab = FALSE;
    if (screen_info->pointer_grabs > 0)
    {
        grab = (XChangeActivePointerGrab (myScreenGetXDisplay (screen_info),
                                          event_mask, cursor, (Time) timestamp) == GrabSuccess);
    }

    return grab;
}

unsigned int
myScreenUngrabKeyboard (ScreenInfo *screen_info, guint32 timestamp)
{
    g_return_val_if_fail (screen_info, 0);
    TRACE ("entering myScreenUngrabKeyboard");

    screen_info->key_grabs--;
    if (screen_info->key_grabs < 0)
    {
        screen_info->key_grabs = 0;
    }
    if (screen_info->key_grabs == 0)
    {
        XUngrabKeyboard (myScreenGetXDisplay (screen_info), (Time) timestamp);
    }
    TRACE ("global key grabs %i", screen_info->key_grabs);

    return screen_info->key_grabs;
}

unsigned int
myScreenUngrabPointer (ScreenInfo *screen_info, guint32 timestamp)
{
    g_return_val_if_fail (screen_info, 0);
    TRACE ("entering myScreenUngrabPointer");

    screen_info->pointer_grabs--;
    if (screen_info->pointer_grabs < 0)
    {
        screen_info->pointer_grabs = 0;
    }
    if (screen_info->pointer_grabs == 0)
    {
        XUngrabPointer (myScreenGetXDisplay (screen_info), (Time) timestamp);
    }
    TRACE ("global pointer grabs %i", screen_info->pointer_grabs);

    return screen_info->pointer_grabs;
}

void
myScreenGrabKeys (ScreenInfo *screen_info)
{
    Display *dpy;
    int i;

    TRACE ("entering myScreenUnrabKeys");
    g_return_if_fail (screen_info != NULL);

    dpy = myScreenGetXDisplay (screen_info);

    for (i = FIRST_KEY; i < KEY_COUNT; i++)
    {
        grabKey (dpy, &screen_info->params->keys[i], screen_info->xroot);
    }
}

void
myScreenUngrabKeys (ScreenInfo *screen_info)
{
    Display *dpy;

    TRACE ("entering myScreenUnrabKeys");
    g_return_if_fail (screen_info != NULL);

    dpy = myScreenGetXDisplay (screen_info);
    ungrabKeys (dpy, screen_info->xroot);
}

int
myScreenGetKeyPressed (ScreenInfo *screen_info, XKeyEvent * ev)
{
    int key, state;

    TRACE ("entering myScreenGetKeyPressed");

    state = ev->state & MODIFIER_MASK;
    for (key = 0; key < KEY_COUNT; key++)
    {
        if ((screen_info->params->keys[key].keycode == ev->keycode)
            && (screen_info->params->keys[key].modifier == state))
        {
            break;
        }
    }

    return key;
}

int
myScreenGetModifierPressed (ScreenInfo *screen_info)
{
    int rx, ry;

    return (int) getMouseXY (screen_info, screen_info->xroot, &rx, &ry);
}

Client *
myScreenGetClientFromWindow (ScreenInfo *screen_info, Window w, unsigned short mode)
{
    Client *c;
    guint i;

    g_return_val_if_fail (w != None, NULL);
    TRACE ("entering myScreenGetClientFromWindow");
    TRACE ("looking for (0x%lx)", w);

    for (c = screen_info->clients, i = 0; i < screen_info->client_count; c = c->next, i++)
    {
        if (clientGetFromWindow (c, w, mode))
        {
            return (c);
        }
    }
    TRACE ("no client found");

    return NULL;
}

gboolean
myScreenComputeSize (ScreenInfo *screen_info)
{
    gint num_monitors, i;
    gint width, height;
    GdkRectangle monitor;
    gboolean changed;

    g_return_val_if_fail (screen_info != NULL, FALSE);
    g_return_val_if_fail (GDK_IS_SCREEN (screen_info->gscr), FALSE);
    TRACE ("entering myScreenComputeSize");

    width = 0;
    height = 0;
    num_monitors = xfwm_get_n_monitors (screen_info->gscr);

    if (num_monitors == 0)
    {
        return FALSE;
    }

    for (i = 0; i < num_monitors; i++)
    {
        xfwm_get_monitor_geometry (screen_info->gscr, i, &monitor);
        width = MAX (monitor.x + monitor.width, width);
        height = MAX (monitor.y + monitor.height, height);
    }

    screen_info->logical_width = WidthOfScreen (screen_info->xscreen);
    screen_info->logical_height = HeightOfScreen (screen_info->xscreen);
    if ((width != screen_info->logical_width) || (height != screen_info->logical_height))
    {
        g_warning ("output size (%dx%d) and logical screen size (%dx%d) do not match",
                   width, height, screen_info->logical_width, screen_info->logical_height);
    }

    /* Keep the smallest size between what we computed and the
     * reported size for the screen.
     */
    if (width == 0 || width > screen_info->logical_width)
    {
        width = screen_info->logical_width;
    }

    if (height == 0 || height > screen_info->logical_height)
    {
        height = screen_info->logical_height;
    }

    changed = ((screen_info->width != width) | (screen_info->height != height));
    screen_info->width = width;
    screen_info->height = height;
    TRACE ("myScreenComputeSize(): width=%i, height=%i", width, height);

    return changed;
}

gint
myScreenGetNumMonitors (ScreenInfo *screen_info)
{
    g_return_val_if_fail (screen_info != NULL, 0);
    g_return_val_if_fail (screen_info->monitors_index != NULL, 0);
    TRACE ("entering myScreenGetNMonitors");

    return (screen_info->monitors_index->len);
}

gint
myScreenGetMonitorIndex (ScreenInfo *screen_info, gint idx)
{
    g_return_val_if_fail (screen_info != NULL, 0);
    g_return_val_if_fail (screen_info->monitors_index != NULL, 0);
    TRACE ("entering myScreenGetMonitorIndex");

    return (g_array_index (screen_info->monitors_index, gint, idx));
}

gboolean
myScreenRebuildMonitorIndex (ScreenInfo *screen_info)
{
    gint i, j, num_monitors, previous_num_monitors;
    GdkRectangle monitor, previous;
    gboolean cloned;

    g_return_val_if_fail (screen_info != NULL, FALSE);
    TRACE ("entering myScreenRebuildMonitorIndex");

    previous_num_monitors = screen_info->num_monitors;
    screen_info->num_monitors = 0;

    if (screen_info->monitors_index)
    {
        g_array_free (screen_info->monitors_index, TRUE);
    }
    screen_info->monitors_index = g_array_new (FALSE, TRUE, sizeof (guint));

    /* GDK already sorts monitors for us, for "cloned" monitors, sort
     * the bigger ones first (giving preference to taller monitors
     * over wider monitors)
     */
    num_monitors = xfwm_get_n_monitors (screen_info->gscr);
    for (i = 0; i < num_monitors; i++)
    {
        xfwm_get_monitor_geometry (screen_info->gscr, i, &monitor);
        cloned = FALSE;
        for (j = 0; j < (gint) screen_info->monitors_index->len; j++)
        {
            xfwm_get_monitor_geometry (screen_info->gscr, j, &previous);
            if ((previous.x == monitor.x) && (previous.y == monitor.y))
            {
                cloned = TRUE;
            }
        }
        if (!cloned)
        {
            screen_info->num_monitors++;
            g_array_append_val (screen_info->monitors_index , i);
        }
    }

    TRACE ("Physical monitor reported.: %i", num_monitors);
    TRACE ("Logical views found.......: %i", screen_info->num_monitors);

    return (screen_info->num_monitors != previous_num_monitors);
}

void
myScreenInvalidateMonitorCache (ScreenInfo *screen_info)
{
    g_return_if_fail (screen_info != NULL);
    TRACE ("entering myScreenInvalidateMonitorCache");

    screen_info->cache_monitor.x = -1;
    screen_info->cache_monitor.y = -1;
    screen_info->cache_monitor.width = 0;
    screen_info->cache_monitor.height = 0;
}

/*
   gdk_screen_get_monitor_at_point () doesn't give accurate results
   when the point is off screen, use my own implementation from xfce 3
 */
void
myScreenFindMonitorAtPoint (ScreenInfo *screen_info, gint x, gint y, GdkRectangle *rect)
{
    gint dx, dy, center_x, center_y, num_monitors, i;
    guint32 distsquare, min_distsquare;
    GdkRectangle monitor, nearest_monitor = { G_MAXINT, G_MAXINT, 0, 0 };

    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (rect != NULL);
    g_return_if_fail (GDK_IS_SCREEN (screen_info->gscr));
    TRACE ("entering myScreenFindMonitorAtPoint");

    /* Cache system */
    if ((x >= screen_info->cache_monitor.x) && (x < screen_info->cache_monitor.x + screen_info->cache_monitor.width) &&
        (y >= screen_info->cache_monitor.y) && (y < screen_info->cache_monitor.y + screen_info->cache_monitor.height))
    {
        *rect = screen_info->cache_monitor;
        return;
    }

    min_distsquare = G_MAXUINT32;
    num_monitors = myScreenGetNumMonitors (screen_info);

    for (i = 0; i < num_monitors; i++)
    {
        gint monitor_index;

        monitor_index = myScreenGetMonitorIndex (screen_info, i);
        xfwm_get_monitor_geometry (screen_info->gscr, monitor_index, &monitor);

        if ((x >= monitor.x) && (x < (monitor.x + monitor.width)) &&
            (y >= monitor.y) && (y < (monitor.y + monitor.height)))
        {
            screen_info->cache_monitor = monitor;
            *rect = screen_info->cache_monitor;
            return;
        }

        center_x = monitor.x + (monitor.width / 2);
        center_y = monitor.y + (monitor.height / 2);

        dx = x - center_x;
        dy = y - center_y;

        distsquare = (dx * dx) + (dy * dy);

        if (distsquare < min_distsquare)
        {
            min_distsquare = distsquare;
            nearest_monitor = monitor;
        }
    }

    screen_info->cache_monitor = nearest_monitor;
    *rect = screen_info->cache_monitor;
}

void
myScreenGetXineramaMonitorGeometry (ScreenInfo *screen_info, gint monitor_num, GdkRectangle *rect)
{
    XineramaScreenInfo *infos;
    int n;

    g_return_if_fail (screen_info != NULL);
    g_return_if_fail (rect != NULL);
    g_return_if_fail (XineramaIsActive (myScreenGetXDisplay (screen_info)));

    infos = XineramaQueryScreens (myScreenGetXDisplay (screen_info), &n);
    if (infos == NULL || n <= 0 || monitor_num > n)
    {
        g_warning ("Cannot query Xinerama!");
        XFree (infos);

        /* Using screen size as fallback, safer than some random values */
        rect->x = 0;
        rect->y = 0;
        rect->width = screen_info->width;
        rect->height = screen_info->height;

        return;
    }

    rect->x = infos[monitor_num].x_org;
    rect->y = infos[monitor_num].y_org;
    rect->width = infos[monitor_num].width;
    rect->height = infos[monitor_num].height;

    XFree (infos);
}

gboolean
myScreenUpdateFontHeight (ScreenInfo *screen_info)
{
    PangoFontDescription *desc;
    PangoContext *context;
    PangoFontMetrics *metrics;
    GtkWidget *widget;

    g_return_val_if_fail (screen_info != NULL, FALSE);
    TRACE ("entering myScreenUpdateFontHeight");

    widget = myScreenGetGtkWidget (screen_info);
    context = getUIPangoContext (widget);
    desc = getUIPangoFontDesc (widget);

    if (desc && context)
    {
        metrics = pango_context_get_metrics (context, desc, NULL);
        screen_info->font_height =
                 PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
                               pango_font_metrics_get_descent (metrics));
        pango_font_metrics_unref (metrics);

        return TRUE;
    }

    return FALSE;
}

