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
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <X11/Xlib.h>
#include "gtktoxevent.h"
#include "main.h"
#include "events.h"
#include "frame.h"
#include "settings.h"
#include "client.h"
#include "menu.h"
#include "workspaces.h"
#include "debug.h"

char *progname;
Display *dpy;
Window root, gnome_win;
Colormap cmap;
int screen;
int depth;
CARD32 margins[4];
CARD32 gnome_margins[4];
int quit = False, reload = False;
int shape, shape_event;
Cursor resize_cursor[7], move_cursor, root_cursor;

int handleXError(Display * dpy, XErrorEvent * err)
{
    switch (err->error_code)
    {
        case BadAccess:
            if(err->resourceid == root)
            {
                fprintf(stderr, "%s: Another window manager is running\n", progname);
                exit(1);
            }
            break;
        default:
            DBG("X error ignored\n");
            break;
    }
    return 0;
}

void cleanUp()
{
    int i;

    DBG("entering cleanUp\n");

    clientUnframeAll();
    unloadSettings();
    XFreeCursor(dpy, root_cursor);
    XFreeCursor(dpy, move_cursor);
    for(i = 0; i < 7; i++)
    {
        XFreeCursor(dpy, resize_cursor[i]);
    }
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    closeEventFilter();
}

void handleSignal(int sig)
{
    DBG("entering handleSignal\n");

    switch (sig)
    {
        case SIGINT:
        case SIGTERM:
            gtk_main_quit();
            quit = True;
            break;
        case SIGHUP:
            reload = True;
            break;
        case SIGSEGV:
            fprintf(stderr, "%s: Segmentation fault\n", progname);
            cleanUp();
            exit(1);
            break;
        default:
            break;
    }
}

void initialize(int argc, char **argv)
{
    struct sigaction act;
    int dummy;
    long ws;

    DBG("entering initialize\n");

    gtk_init(&argc, &argv);
    progname = argv[0];

    dpy = GDK_DISPLAY();
    root = GDK_ROOT_WINDOW();
    screen = XDefaultScreen(dpy);
    depth = DefaultDepth(dpy, screen);
    cmap = DefaultColormap(dpy, screen);

    margins[MARGIN_TOP] = gnome_margins[MARGIN_TOP] = 0;
    margins[MARGIN_LEFT] = gnome_margins[MARGIN_LEFT] = 0;
    margins[MARGIN_RIGHT] = gnome_margins[MARGIN_RIGHT] = 0;
    margins[MARGIN_BOTTOM] = gnome_margins[MARGIN_BOTTOM] = 0;

    XSetErrorHandler(handleXError);
    shape = XShapeQueryExtension(dpy, &shape_event, &dummy);

    initICCCMHints(dpy);
    initMotifHints(dpy);
    initGnomeHints(dpy);
    initNetHints(dpy);

    root_cursor = XCreateFontCursor(dpy, XC_left_ptr);
    move_cursor = XCreateFontCursor(dpy, XC_fleur);
    resize_cursor[CORNER_TOP_LEFT] = XCreateFontCursor(dpy, XC_top_left_corner);
    resize_cursor[CORNER_TOP_RIGHT] = XCreateFontCursor(dpy, XC_top_right_corner);
    resize_cursor[CORNER_BOTTOM_LEFT] = XCreateFontCursor(dpy, XC_bottom_left_corner);
    resize_cursor[CORNER_BOTTOM_RIGHT] = XCreateFontCursor(dpy, XC_bottom_right_corner);
    resize_cursor[4 + SIDE_LEFT] = XCreateFontCursor(dpy, XC_left_side);
    resize_cursor[4 + SIDE_RIGHT] = XCreateFontCursor(dpy, XC_right_side);
    resize_cursor[4 + SIDE_BOTTOM] = XCreateFontCursor(dpy, XC_bottom_side);

    XDefineCursor(dpy, root, root_cursor);

    initEventFilter(SubstructureNotifyMask | StructureNotifyMask | SubstructureRedirectMask | ButtonPressMask | ButtonReleaseMask | FocusChangeMask | PropertyChangeMask | ColormapNotify, NULL, "xfwm");
    pushEventFilter(xfwm4_event_filter, NULL);

    gnome_win = getDefaultXWindow();

    initSettings();
    loadSettings();

    act.sa_handler = handleSignal;
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGHUP, &act, NULL);

    setGnomeProtocols(dpy, root, gnome_win);
    setGnomeHint(dpy, root, win_supporting_wm_check, gnome_win);
    setGnomeHint(dpy, root, win_desktop_button_proxy, gnome_win);
    setGnomeHint(dpy, gnome_win, win_desktop_button_proxy, gnome_win);
    getGnomeHint(dpy, root, win_workspace, &ws);
    workspace = (int)ws;
    getGnomeDesktopMargins(dpy, gnome_margins);
    set_utf8_string_hint(dpy, gnome_win, net_wm_name, "Xfwm4");
    set_net_supported_hint(dpy, root, gnome_win);
    workspaceUpdateArea(margins, gnome_margins);
    init_net_desktop_params(dpy, root, workspace);
    set_net_workarea(dpy, root, workspace_count, margins);
    XSetInputFocus(dpy, gnome_win, RevertToNone, CurrentTime);
    initGtkCallbacks();
    clientFrameAll();
}

int main(int argc, char **argv)
{
    initialize(argc, argv);
    gtk_main();
    cleanUp();
    DBG("Window manager exits cleanly\n");
    return 0;
}
