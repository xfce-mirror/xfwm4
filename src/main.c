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
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h> 
#include <libxfcegui4/libxfcegui4.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include "main.h"
#include "events.h"
#include "frame.h"
#include "settings.h"
#include "client.h"
#include "menu.h"
#include "focus.h"
#include "keyboard.h"
#include "workspaces.h"
#include "session.h"
#include "startup_notification.h"
#include "spinning_cursor.h"

#define MAIN_EVENT_MASK\
    SubstructureNotifyMask|\
    StructureNotifyMask|\
    SubstructureRedirectMask|\
    ButtonPressMask|\
    ButtonReleaseMask|\
    FocusChangeMask|\
    PropertyChangeMask|\
    ColormapNotify

MainData *md = NULL;

static int
handleXError (Display * dpy, XErrorEvent * err)
{
#if DEBUG            
    char buf[64];
#endif
    switch (err->error_code)
    {
        case BadAccess:
            if (err->resourceid == md->xroot)
            {
                g_message ("%s: Another window manager is running\n",
                    md->progname);
                exit (1);
            }
            break;
        default:
#if DEBUG            
            XGetErrorText (dpy, err->error_code, buf, 63);
            g_print ("XError: %s\n", buf);                                                  
            g_print ("==>  XID Ox%lx, Request %d, Error %d <==\n", 
                      err->resourceid, err->request_code, err->error_code); 
#endif
            break;
    }
    return 0;
}

static void
cleanUp ()
{
    int i;

    TRACE ("entering cleanUp");

    clientUnframeAll ();
    sn_close_display ();
    unloadSettings ();
    XFreeCursor (md->dpy, md->root_cursor);
    XFreeCursor (md->dpy, md->move_cursor);
    XFreeCursor (md->dpy, md->busy_cursor);
    sessionFreeWindowStates ();
    for (i = 0; i < 7; i++)
    {
        XFreeCursor (md->dpy, md->resize_cursor[i]);
    }
    for (i = 0; i < NB_KEY_SHORTCUTS; i++)
    {
        if (params.shortcut_exec[i])
        {
            g_free (params.shortcut_exec[i]);
            params.shortcut_exec[i] = NULL;
        }
    }
    g_free (params.workspace_names);
    params.workspace_names = NULL;
    removeTmpEventWin (md->sidewalk[0]);
    removeTmpEventWin (md->sidewalk[1]);
    XSetInputFocus (md->dpy, md->xroot, RevertToPointerRoot, GDK_CURRENT_TIME);
    xfce_close_event_filter (md->gtox_data);
    g_free (md);
}

static char *build_session_filename(SessionClient *client_session)
{
    gchar *filename;
    gchar *path;
    gchar *file;
    
    path = (gchar *)xfce_get_userdir();
    if (!g_file_test(path, G_FILE_TEST_IS_DIR) && mkdir(path, 0755) < 0) {
            g_warning("Unable to create xfce user dir %s: %s",
                       path, g_strerror(errno));
            return NULL;
    }

    path = xfce_get_userfile("sessions", NULL);
    if (!g_file_test(path, G_FILE_TEST_IS_DIR) && mkdir(path, 0755) < 0) 
    {
        g_warning("Unable to create session dir %s: %s", 
                  path, g_strerror(errno));
        g_free (path);
        return NULL;
    }
    
    file = g_strdup_printf("xfwm4-%s", client_session->given_client_id);
    filename = g_build_filename (path, file, NULL);
    g_free (file);
    g_free (path);
    
    return filename;
}

static void
load_saved_session (SessionClient *client_session)
{
    gchar *filename;
    
    filename = build_session_filename(client_session);
    if (filename)
    {
        sessionLoadWindowStates (filename);
        g_free (filename);
    }
}

static void
save_phase_2 (gpointer data)
{
    SessionClient *client_session = (SessionClient *) data;
    gchar *filename;

    filename = build_session_filename(client_session);
    if (filename)
    {
        sessionSaveWindowStates (filename);
        g_free (filename);
    }
}

static void
session_die (gpointer client_data)
{
    gtk_main_quit ();
    md->quit = TRUE;
}

static void
handleSignal (int sig)
{
    TRACE ("entering handleSignal");

    switch (sig)
    {
        case SIGINT:
        case SIGTERM:
            gtk_main_quit ();
            md->quit = TRUE;
            break;
        case SIGHUP:
        case SIGUSR1:
            md->reload = TRUE;
            break;
        case SIGSEGV:
            cleanUp ();
            g_error (_("%s: Segmentation fault"), g_get_prgname ());
            break;
        default:
            break;
    }
}

static int
initialize (int argc, char **argv)
{
    PangoLayout *layout;
    struct sigaction act;
    int dummy;
    long ws;
    GdkWindow *groot;
    SessionClient *client_session;

    TRACE ("entering initialize");

    md = g_new0 (MainData, 1);
    
    md->quit = FALSE;
    md->reload = FALSE;
    md->progname = argv[0];

    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    gtk_set_locale ();
    gtk_init (&argc, &argv);

    md->gscr = gdk_screen_get_default ();
    if (!md->gscr)
    {
        g_error (_("Cannot get default screen\n"));
    }
    md->gdisplay = gdk_screen_get_display (md->gscr);
    gtk_widget_push_colormap(gdk_screen_get_rgb_colormap (md->gscr));
    md->dpy = gdk_x11_display_get_xdisplay (md->gdisplay);
    md->xscreen = gdk_x11_screen_get_xscreen (md->gscr);
    groot = gdk_screen_get_root_window (md->gscr);
    md->xroot = (Window) gdk_x11_drawable_get_xid (groot);
    md->screen = gdk_screen_get_number (md->gscr);
    md->cmap = GDK_COLORMAP_XCOLORMAP(gdk_screen_get_rgb_colormap (md->gscr));
    
    DBG ("xfwm4 starting, using GTK+-%d.%d.%d", gtk_major_version, 
         gtk_minor_version, gtk_micro_version);

    xfce_setenv ("DISPLAY", gdk_display_get_name (md->gdisplay), TRUE);

    md->depth = DefaultDepth (md->dpy, md->screen);
    sn_init_display (md->dpy, md->screen);
    md->current_ws = 0;

    XSetErrorHandler (handleXError);
    md->shape = XShapeQueryExtension (md->dpy, &md->shape_event, &dummy);

    /* Create the side windows to detect edge movement */
    md->sidewalk[0] = setTmpEventWin (0, 0, 
                                  1, gdk_screen_get_height (md->gscr), 
                                  LeaveWindowMask | PointerMotionMask);

    md->sidewalk[1] = setTmpEventWin (gdk_screen_get_width (md->gscr) - 1, 0, 
                                  1, gdk_screen_get_height (md->gscr), 
                                  LeaveWindowMask | PointerMotionMask);

    md->margins[TOP] = md->gnome_margins[TOP] = 0;
    md->margins[LEFT] = md->gnome_margins[LEFT] = 0;
    md->margins[RIGHT] = md->gnome_margins[RIGHT] = 0;
    md->margins[BOTTOM] = md->gnome_margins[BOTTOM] = 0;

    initICCCMHints (md->dpy);
    initMotifHints (md->dpy);
    initGnomeHints (md->dpy);
    initNetHints (md->dpy);
    initKDEHints (md->dpy);
    initSystrayHints (md->dpy, md->screen);
    
    initModifiers (md->dpy);

    md->root_cursor = XCreateFontCursor (md->dpy, XC_left_ptr);
    md->move_cursor = XCreateFontCursor (md->dpy, XC_fleur);
    md->busy_cursor = cursorCreateSpinning (md->dpy, md->xroot);
    md->resize_cursor[CORNER_TOP_LEFT] =
        XCreateFontCursor (md->dpy, XC_top_left_corner);
    md->resize_cursor[CORNER_TOP_RIGHT] =
        XCreateFontCursor (md->dpy, XC_top_right_corner);
    md->resize_cursor[CORNER_BOTTOM_LEFT] =
        XCreateFontCursor (md->dpy, XC_bottom_left_corner);
    md->resize_cursor[CORNER_BOTTOM_RIGHT] =
        XCreateFontCursor (md->dpy, XC_bottom_right_corner);
    md->resize_cursor[4 + SIDE_LEFT] = XCreateFontCursor (md->dpy, XC_left_side);
    md->resize_cursor[4 + SIDE_RIGHT] = XCreateFontCursor (md->dpy, XC_right_side);
    md->resize_cursor[4 + SIDE_BOTTOM] = XCreateFontCursor (md->dpy, XC_bottom_side);

    XDefineCursor (md->dpy, md->xroot, md->root_cursor);

    md->gtox_data = xfce_init_event_filter (md->gscr, MAIN_EVENT_MASK, NULL, "xfwm");
    if (!md->gtox_data)
    {
        return -1;
    }
    xfce_push_event_filter (md->gtox_data, xfwm4_event_filter, NULL);

    md->gnome_win = xfce_get_default_XID (md->gtox_data);
    DBG ("Our event window is 0x%lx", md->gnome_win);

    if (!initSettings ())
    {
        return -2;
    }

    md->systray = getSystrayWindow (md->dpy);
    setGnomeProtocols (md->dpy, md->screen, md->gnome_win);
    setHint (md->dpy, md->xroot, win_supporting_wm_check, md->gnome_win);
    setHint (md->dpy, md->xroot, win_desktop_button_proxy, md->gnome_win);
    setHint (md->dpy, md->gnome_win, win_desktop_button_proxy, md->gnome_win);
    getHint (md->dpy, md->xroot, win_workspace, &ws);
    md->current_ws = (int) ws;
    getGnomeDesktopMargins (md->dpy, md->screen, md->gnome_margins);
    set_utf8_string_hint (md->dpy, md->gnome_win, net_wm_name, "Xfwm4");
    setNetSupportedHint (md->dpy, md->screen, md->gnome_win);
    workspaceUpdateArea (md->margins, md->gnome_margins);
    initNetDesktopParams (md->dpy, md->screen, 
                               md->current_ws,
                               gdk_screen_get_width (md->gscr), 
                               gdk_screen_get_height (md->gscr));
    setNetWorkarea (md->dpy, md->screen, params.workspace_count, 
                         gdk_screen_get_width (md->gscr), 
                         gdk_screen_get_height (md->gscr), 
                         md->margins);
    XSetInputFocus (md->dpy, md->gnome_win, RevertToPointerRoot, GDK_CURRENT_TIME);
    initGtkCallbacks ();
    
    /* The first time the first Gtk application on a display uses pango,
     * pango grabs the XServer while it creates the font cache window.
     * Therefore, force the cache window to be created now instead of
     * trying to do it while we have another grab and deadlocking the server.
     */
    layout = gtk_widget_create_pango_layout (xfce_get_default_gtk_widget (md->gtox_data), "-");
    pango_layout_get_pixel_extents (layout, NULL, NULL);
    g_object_unref (G_OBJECT (layout));

    clientClearFocus ();
    clientFrameAll ();

    act.sa_handler = handleSignal;
    act.sa_flags = 0;
    sigaction (SIGINT, &act, NULL);
    sigaction (SIGTERM, &act, NULL);
    sigaction (SIGHUP, &act, NULL);
    sigaction (SIGUSR1, &act, NULL);
    sigaction (SIGSEGV, &act, NULL);

    client_session =
        client_session_new (argc, argv, NULL, SESSION_RESTART_IF_RUNNING, 20);
    client_session->data = (gpointer) client_session;
    client_session->save_phase_2 = save_phase_2;
    client_session->die = session_die;

    if (session_init (client_session))
    {
        load_saved_session (client_session);
    }

    return 0;
}

int
main (int argc, char **argv)
{
    int i;
    gboolean daemon_mode = FALSE;
    int status;

    if (argc > 1 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-V")))
    {
        g_print ("\tThis is %s version %s for Xfce %s\n", 
                      PACKAGE, VERSION,  xfce_version_string());
        g_print ("\tbuilt with GTK+-%d.%d.%d, ", 
                      GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
        g_print ("linked with GTK+-%d.%d.%d.\n", 
                      gtk_major_version, gtk_minor_version, gtk_micro_version);
        exit (0);
    }

    for (i = 1; i < argc; i++)
    {
        if (!strcmp (argv[i], "--daemon"))
        {
            daemon_mode = TRUE;
        }
    }

    status = initialize (argc, argv);
    switch (status)
    {
        case -1:
            g_warning (_("%s: Another Window Manager is already running"),
                g_get_prgname ());
            exit (1);
            break;
        case -2:
            g_warning (_("%s: Missing data from default files"),
                g_get_prgname ());
            exit (1);
            break;
        case 0:
            if (daemon_mode)
            {
#ifdef HAVE_DAEMON
                                if (daemon(TRUE, TRUE) < 0) {
                                        g_warning(_("%s: Failed to enter daemon mode: %s"),
                                                        g_get_prgname(), g_strerror(errno));
                                        exit(EXIT_FAILURE);
                                }
#else /* !HAVE_DAEMON */
                switch (fork ())
                {
                    case -1:
                        g_warning (_("%s: Failed to create new process: %s"),
                                                                g_get_prgname(), g_strerror(errno));
                        exit (1);
                        break;
                    case 0:    /* child */
#ifdef HAVE_SETSID
                                                /* detach from terminal session */
                                                (void)setsid();
#endif /* !HAVE_SETSID */
                        break;
                    default:   /* parent */
                        _exit (0);
                        break;
                }
#endif /* !HAVE_DAEMON */
                        }

                        /* enter GTK main loop */
            gtk_main ();
            break;
        default:
            g_warning (_("%s: Unknown error occured"), g_get_prgname ());
            exit (1);
            break;
    }
    cleanUp ();
    DBG ("xfwm4 terminated");
    return 0;
}
