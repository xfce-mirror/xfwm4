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
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libxfcegui4/libxfcegui4.h>
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
    
    /* There can be one systray per screen */
    Atom net_system_tray_selection;
    
    /* Window stacking, per screen */
    GList *windows_stack;
    Client *last_raise;
    GList *windows;
    Client *clients;
    unsigned int client_count;
    unsigned long client_serial;
    
    /* Theme pixmaps and other params, per screen */
    XfwmColor title_colors[2];
    XfwmPixmap buttons[BUTTON_COUNT][6];
    XfwmPixmap corners[4][2];
    XfwmPixmap sides[3][2];
    XfwmPixmap title[5][2];

    /* Per screen graphic contexts */
    GC box_gc;
    GdkGC *black_gc;
    GdkGC *white_gc;
    
    /* Screen data */
    Colormap cmap;
    GdkScreen *gscr;
    Screen *xscreen;
    int depth;

    GtkWidget *gtk_win;
    xfwmWindow sidewalk[4];
    Window gnome_win;
    Window xroot;
    Window systray;
    
    int gnome_margins[4];
    int margins[4];
    int screen;
    int current_ws;
    int previous_ws;

    /* Workspace definitions */
    int workspace_count;
    gchar *workspace_names;
    int workspace_names_length;
    NetWmDesktopLayout desktop_layout;

    /* Button handler for GTK */
    gulong button_handler_id;

    /* MCS stuff */
    McsClient *mcs_client;
    gboolean mcs_initted;
    
    /* Per screen parameters */
    XfwmParams *params;

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    /* Startup notification data, per screen */
    SnMonitorContext *sn_context;
    GSList *startup_sequences;
    guint startup_sequence_timeout;
#endif

#ifdef HAVE_COMPOSITOR
    gboolean compositor_active;

    GList *cwindows;

    gaussian_conv *gaussianMap;
    gint gsize;
    guchar *shadowCorner;
    guchar *shadowTop;

    
    Picture rootPicture;
    Picture rootBuffer;
    Picture blackPicture;
    Picture rootTile;
    XserverRegion allDamage;
    gboolean clipChanged;
#endif /* HAVE_COMPOSITOR */
};

ScreenInfo *     myScreenInit         (DisplayInfo *, 
                                       GdkScreen *, 
                                       unsigned long);
ScreenInfo *     myScreenClose        (ScreenInfo *);
Display *        myScreenGetXDisplay  (ScreenInfo *);
GtkWidget *      myScreenGetGtkWidget (ScreenInfo *);
GtkWidget *      myScreenGetGtkWidget (ScreenInfo *);
GdkWindow *      myScreenGetGdkWindow (ScreenInfo *);

#endif /* INC_SCREEN_H */
