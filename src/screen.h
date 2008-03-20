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
        xfwm4    - (c) 2002-2006 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libxfce4mcs/mcs-client.h>

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <libsn/sn.h>
#endif

#ifndef INC_SCREEN_H
#define INC_SCREEN_H

#include "display.h"
#include "settings.h"
#include "mywindow.h"
#include "mypixmap.h"
#include "client.h"
#include "hints.h"

#ifdef HAVE_COMPOSITOR
struct _gaussian_conv {
    int     size;
    double  *data;
};
typedef struct _gaussian_conv gaussian_conv;
#endif /* HAVE_COMPOSITOR */

struct _ScreenInfo
{
    /* The display this screen belongs to */
    DisplayInfo *display_info;

    /* Window stacking, per screen */
    GList *windows_stack;
    Client *last_raise;
    GList *windows;
    Client *clients;
    unsigned int client_count;
    unsigned long client_serial;
    int key_grabs;
    int pointer_grabs;

    /* Theme pixmaps and other params, per screen */
    XfwmColor title_colors[2];
    XfwmColor title_shadow_colors[2];
    xfwmPixmap buttons[BUTTON_COUNT][6];
    xfwmPixmap corners[4][2];
    xfwmPixmap sides[3][2];
    xfwmPixmap title[5][2];

    /* Per screen graphic contexts */
    GC box_gc;
    GdkGC *black_gc;
    GdkGC *white_gc;

    /* Screen data */
    Colormap cmap;
    GdkScreen *gscr;
    Screen *xscreen;
    int depth;
    int width;
    int height;
    Visual *visual;

    GtkWidget *gtk_win;
    xfwmWindow sidewalk[4];
    Window xfwm4_win;
    Window xroot;

    int gnome_margins[4];
    int margins[4];
    int screen;
    int current_ws;
    int previous_ws;

    /* Workspace definitions */
    int workspace_count;
    gchar **workspace_names;
    int workspace_names_items;
    NetWmDesktopLayout desktop_layout;

    /* Button handler for GTK */
    gulong button_handler_id;

    /* MCS stuff */
    McsClient *mcs_client;
    gboolean mcs_initted;

    /* Per screen parameters */
    XfwmParams *params;

    /* show desktop flag */
    gboolean show_desktop;

#ifdef ENABLE_KDE_SYSTRAY_PROXY
    /* There can be one systray per screen */
    Atom net_system_tray_selection;
    Window systray;
#endif

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    /* Startup notification data, per screen */
    SnMonitorContext *sn_context;
    GSList *startup_sequences;
    guint startup_sequence_timeout;
#endif

#ifdef HAVE_COMPOSITOR
#if HAVE_OVERLAYS
    Window overlay;
    Window root_overlay;
#endif
    GList *cwindows;
    Window output;


    gaussian_conv *gaussianMap;
    gint gaussianSize;
    guchar *shadowCorner;
    guchar *shadowTop;

    Picture rootPicture;
    Picture rootBuffer;
    Picture blackPicture;
    Picture rootTile;
    XserverRegion allDamage;

    guint wins_unredirected;
    gboolean compositor_active;
    gboolean clipChanged;
#endif /* HAVE_COMPOSITOR */
};

ScreenInfo              *myScreenInit                           (DisplayInfo *,
                                                                 GdkScreen *,
                                                                 unsigned long);
ScreenInfo              *myScreenClose                          (ScreenInfo *);
Display                 *myScreenGetXDisplay                    (ScreenInfo *);
GtkWidget               *myScreenGetGtkWidget                   (ScreenInfo *);
GtkWidget               *myScreenGetGtkWidget                   (ScreenInfo *);
GdkWindow               *myScreenGetGdkWindow                   (ScreenInfo *);
gboolean                 myScreenCheckWMAtom                    (ScreenInfo *,
                                                                 Atom atom);
gboolean                 myScreenGrabKeyboard                   (ScreenInfo *,
                                                                 Time);
gboolean                 myScreenGrabPointer                    (ScreenInfo *,
                                                                 unsigned int,
                                                                 Cursor,
                                                                 Time);
unsigned int             myScreenUngrabKeyboard                 (ScreenInfo *);
unsigned int             myScreenUngrabPointer                  (ScreenInfo *);
void                     myScreenGrabKeys                       (ScreenInfo *);
void                     myScreenUngrabKeys                     (ScreenInfo *);
#endif /* INC_SCREEN_H */
