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

        xfwm4    - (c) 2002-2008 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_RENDER
#include <X11/extensions/Xrender.h>
#endif

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif

#include "display.h"
#include "screen.h"
#include "misc.h"
#include "mywindow.h"
#include "compositor.h"

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
    gchar selection[32];
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

    current_wm = XGetSelectionOwner (display_info->dpy, wm_sn_atom);
    if (current_wm)
    {
        if (!replace_wm)
        {
            g_message ("To replace the current window manager, try \"--replace\"");
            return FALSE;
        }
        gdk_error_trap_push ();
        attrs.event_mask = StructureNotifyMask;
        XChangeWindowAttributes (display_info->dpy, current_wm, CWEventMask, &attrs);
        if (gdk_error_trap_pop ())
        {
            current_wm = None;
        }
    }

    if (!setXAtomManagerOwner (display_info, wm_sn_atom, screen_info->xroot, screen_info->xfwm4_win))
    {
        g_warning ("Cannot acquire window manager selection on screen %d", screen_info->screen);
        return FALSE;
    }

    /* Waiting for previous window manager to exit */
    if (current_wm)
    {
        wait = 0;
        timeout = 10 * G_USEC_PER_SEC;
        while (wait < timeout)
        {
            if (XCheckWindowEvent (display_info->dpy, current_wm, StructureNotifyMask, &event) && (event.type == DestroyNotify))
            {
                break;
            }
            g_usleep(G_USEC_PER_SEC / 10);
            wait += G_USEC_PER_SEC / 10;
        }

        if (wait >= timeout)
        {
            g_warning("Previous window manager on screen %d is not exiting", screen_info->screen);
            return FALSE;
        }
    }
    return TRUE;
}

ScreenInfo *
myScreenInit (DisplayInfo *display_info, GdkScreen *gscr, unsigned long event_mask, gboolean replace_wm)
{
    gchar selection[32];
    ScreenInfo *screen_info;
    GdkWindow *event_win;
    PangoLayout *layout;
    Atom wm_sn_atom;
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
    screen_info->xroot = (Window) GDK_DRAWABLE_XID(gdk_screen_get_root_window (gscr));
    screen_info->screen = gdk_screen_get_number (gscr);
    screen_info->cmap = GDK_COLORMAP_XCOLORMAP(gdk_screen_get_rgb_colormap (gscr));
    screen_info->depth = DefaultDepth (display_info->dpy, screen_info->screen);
    screen_info->width = WidthOfScreen (screen_info->xscreen);
    screen_info->height = HeightOfScreen (screen_info->xscreen);
    screen_info->visual = DefaultVisual (display_info->dpy, screen_info->screen);

    screen_info->xfwm4_win = GDK_WINDOW_XWINDOW (screen_info->gtk_win->window);
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

    screen_info->workspace_count = -1;
    screen_info->workspace_names = NULL;
    screen_info->workspace_names_items = 0;

    screen_info->mcs_client = NULL;
    screen_info->mcs_initted = FALSE;

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

    screen_info->box_gc = None;
    screen_info->black_gc = NULL;
    screen_info->white_gc = NULL;
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

    return screen_info->gtk_win->window;
}

gboolean
myScreenGrabKeyboard (ScreenInfo *screen_info, Time time)
{
    gboolean grab;

    g_return_val_if_fail (screen_info, FALSE);

    TRACE ("entering myScreenGrabKeyboard");

    grab = TRUE;
    if (screen_info->key_grabs == 0)
    {
        grab = (XGrabKeyboard (myScreenGetXDisplay (screen_info),
                               screen_info->xfwm4_win,
                               TRUE,
                               GrabModeAsync, GrabModeAsync,
                               time) == GrabSuccess);
    }
    screen_info->key_grabs++;
    TRACE ("global key grabs %i", screen_info->key_grabs);

    return grab;
}

gboolean
myScreenGrabPointer (ScreenInfo *screen_info, unsigned int event_mask, Cursor cursor, Time time)
{
    gboolean grab;

    g_return_val_if_fail (screen_info, FALSE);
    TRACE ("entering myScreenGrabPointer");

    grab = TRUE;
    if (screen_info->pointer_grabs == 0)
    {
        grab = (XGrabPointer (myScreenGetXDisplay (screen_info),
                              screen_info->xfwm4_win,
                              TRUE, event_mask,
                              GrabModeAsync, GrabModeAsync,
                              screen_info->xroot,
                              cursor,
                              time) == GrabSuccess);
    }
    screen_info->pointer_grabs++;
    TRACE ("global pointer grabs %i", screen_info->pointer_grabs);

    return grab;
}

unsigned int
myScreenUngrabKeyboard (ScreenInfo *screen_info)
{
    g_return_val_if_fail (screen_info, 0);
    TRACE ("entering myScreenUngrabKeyboard");

    screen_info->key_grabs = screen_info->key_grabs - 1;
    if (screen_info->key_grabs < 0)
    {
        screen_info->key_grabs = 0;
    }
    if (screen_info->key_grabs == 0)
    {
        XUngrabKeyboard (myScreenGetXDisplay (screen_info), CurrentTime);
    }
    TRACE ("global key grabs %i", screen_info->key_grabs);

    return screen_info->key_grabs;
}

unsigned int
myScreenUngrabPointer (ScreenInfo *screen_info)
{
    g_return_val_if_fail (screen_info, 0);
    TRACE ("entering myScreenUngrabPointer");

    screen_info->pointer_grabs = screen_info->pointer_grabs - 1;
    if (screen_info->pointer_grabs < 0)
    {
        screen_info->pointer_grabs = 0;
    }
    if (screen_info->pointer_grabs == 0)
    {
        XUngrabPointer (myScreenGetXDisplay (screen_info), CurrentTime);
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

    /*
       Ugly hack: KEY_CANCEL is used just for cancelling window ops
       it should not be grabbed all the time (especially when you
       realize that it's mapped to Esc by default, damn, I can't
       switch back to command mode in vi anymore, I'm stuck in
       edition mode ;)

       That's why we start at FIRST_KEY which is defined after
       KEY_CANCEL...
     */
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

Client *
myScreenGetClientFromWindow (ScreenInfo *screen_info, Window w, unsigned short mode)
{
    Client *c;
    int i;

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
