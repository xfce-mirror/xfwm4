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
 
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libxfcegui4/libxfcegui4.h>

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif

#include "display.h"
#include "screen.h"
#include "mywindow.h"
#include "mywindow.h"
#include "compositor.h"

ScreenInfo *
myScreenInit (DisplayInfo *display_info, GdkScreen *gscr, unsigned long event_mask)
{
    ScreenInfo *screen_info = NULL;
    GdkWindow *event_win;
    PangoLayout *layout = NULL;
    int i;
    
    g_return_val_if_fail (display_info, NULL);
    g_return_val_if_fail (GDK_IS_SCREEN (gscr), NULL);

    screen_info = g_new0 (ScreenInfo, 1);
    screen_info->params = g_new0 (XfwmParams, 1);
    
    screen_info->display_info = display_info;
    screen_info->gscr = gscr;

    /* Create a GTK window so that we are just like any other GTK application */
    screen_info->gtk_win = gtk_window_new (GTK_WINDOW_POPUP);
    /* gtk_widget_set_colormap(screen_info->gtk_win, gdk_screen_get_rgb_colormap (gscr)); */
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

    event_win = xfce_add_event_win (gscr, event_mask);
    if (!event_win)
    {
        gtk_widget_destroy (screen_info->gtk_win);
        g_free (screen_info);
        return NULL;
    }
    gdk_window_set_user_data (event_win, screen_info->gtk_win);

    screen_info->xscreen = gdk_x11_screen_get_xscreen (gscr);
    screen_info->xroot = (Window) GDK_DRAWABLE_XID(gdk_screen_get_root_window (gscr));
    screen_info->screen = gdk_screen_get_number (gscr);
    screen_info->cmap = GDK_COLORMAP_XCOLORMAP(gdk_screen_get_rgb_colormap (gscr));
    screen_info->depth = DefaultDepth (display_info->dpy, screen_info->screen);
    screen_info->current_ws = 0;
    screen_info->previous_ws = 0;

    screen_info->margins[TOP] = screen_info->gnome_margins[TOP] = 0;
    screen_info->margins[LEFT] = screen_info->gnome_margins[LEFT] = 0;
    screen_info->margins[RIGHT] = screen_info->gnome_margins[RIGHT] = 0;
    screen_info->margins[BOTTOM] = screen_info->gnome_margins[BOTTOM] = 0;

    screen_info->workspace_count = -1;
    screen_info->workspace_names = NULL;
    screen_info->workspace_names_length = 0;

    screen_info->mcs_client = NULL;
    screen_info->mcs_initted = FALSE;

    screen_info->windows_stack = NULL;
    screen_info->last_raise = NULL;
    screen_info->windows = NULL;
    screen_info->clients = NULL;
    screen_info->client_count = 0;
    screen_info->client_serial = 0L;
    screen_info->button_handler_id = 0L;
    
    /* Create the side windows to detect edge movement */
    xfwmWindowTemp (display_info->dpy, screen_info->xroot, &screen_info->sidewalk[0], 
                                  0, 0, 
                                  1, gdk_screen_get_height (gscr), 
                                  LeaveWindowMask | PointerMotionMask);

    xfwmWindowTemp (display_info->dpy, screen_info->xroot, &screen_info->sidewalk[1], 
                                  gdk_screen_get_width (gscr) - 1, 0, 
                                  1, gdk_screen_get_height (gscr), 
                                  LeaveWindowMask | PointerMotionMask);

    xfwmWindowTemp (display_info->dpy, screen_info->xroot, &screen_info->sidewalk[2], 
                                  0, 0, 
                                  gdk_screen_get_width (gscr), 1,
                                  LeaveWindowMask | PointerMotionMask);

    xfwmWindowTemp (display_info->dpy, screen_info->xroot, &screen_info->sidewalk[3], 
                                  0, gdk_screen_get_height (gscr) - 1, 
                                  gdk_screen_get_width (gscr), 1,
                                  LeaveWindowMask | PointerMotionMask);

    screen_info->gnome_win = GDK_WINDOW_XWINDOW (screen_info->gtk_win->window);

    screen_info->net_system_tray_selection = 
        initSystrayHints (display_info->dpy, screen_info->screen);
    screen_info->systray = getSystrayWindow (display_info->dpy, screen_info->net_system_tray_selection);
    
    screen_info->box_gc = None;
    screen_info->black_gc = NULL;
    screen_info->white_gc = NULL;
    screen_info->title_colors[ACTIVE].gc = NULL;
    screen_info->title_colors[ACTIVE].allocated = FALSE;
    screen_info->title_colors[INACTIVE].gc = NULL;
    screen_info->title_colors[INACTIVE].allocated = FALSE;

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        xfwmPixmapInit (screen_info, &screen_info->buttons[i][ACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->buttons[i][INACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->buttons[i][PRESSED]);
        xfwmPixmapInit (screen_info, &screen_info->buttons[i][T_ACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->buttons[i][T_INACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->buttons[i][T_PRESSED]);
    }
    for (i = 0; i < 4; i++)
    {
        xfwmPixmapInit (screen_info, &screen_info->corners[i][ACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->corners[i][INACTIVE]);
    }
    for (i = 0; i < 3; i++)
    {
        xfwmPixmapInit (screen_info, &screen_info->sides[i][ACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->sides[i][INACTIVE]);
    }
    for (i = 0; i < 5; i++)
    {
        xfwmPixmapInit (screen_info, &screen_info->title[i][ACTIVE]);
        xfwmPixmapInit (screen_info, &screen_info->title[i][INACTIVE]);
    }

    return (screen_info);
}

ScreenInfo *
myScreenClose (ScreenInfo *screen_info)
{
    DisplayInfo *display_info = NULL;
    
    g_return_val_if_fail (screen_info, NULL);
    
    display_info = screen_info->display_info;
    
    clientUnframeAll (screen_info);
    compositorUnmanageScreen (screen_info);
    closeSettings (screen_info);

    g_free (screen_info->workspace_names);
    screen_info->workspace_names = NULL;
    screen_info->workspace_names_length = 0;

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
    DisplayInfo *display_info = NULL;
    
    g_return_val_if_fail (screen_info, NULL);
    g_return_val_if_fail (screen_info->display_info, NULL);
    
    display_info = screen_info->display_info;
    return display_info->dpy;
}

GtkWidget *
myScreenGetGtkWidget (ScreenInfo *screen_info)
{
    g_return_val_if_fail (screen_info, NULL);
    
    return screen_info->gtk_win;
}

GdkWindow *
myScreenGetGdkWindow (ScreenInfo *screen_info)
{
    g_return_val_if_fail (screen_info, NULL);
    
    return screen_info->gtk_win->window;
}
