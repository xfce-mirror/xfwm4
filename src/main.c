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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <X11/Xlib.h>
#include <libxfce4util/debug.h>
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <libxfcegui4/libxfcegui4.h>

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

char *progname;
Display *dpy;
Window root, gnome_win, systray, sidewalk[2];
Colormap cmap;
Screen *xscreen;
int screen;
int depth;
int workspace;
gboolean use_xinerama;
int xinerama_heads;
int margins[4];
int gnome_margins[4];
int quit = FALSE, reload = FALSE;
int shape, shape_event;
Cursor resize_cursor[7], move_cursor, busy_cursor, root_cursor;
SessionClient *client_session;

static int
handleXError (Display * dpy, XErrorEvent * err)
{
#if DEBUG            
    char buf[64];
#endif
    switch (err->error_code)
    {
        case BadAccess:
            if (err->resourceid == root)
            {
                g_message ("%s: Another window manager is running\n",
                    progname);
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
    XFreeCursor (dpy, root_cursor);
    XFreeCursor (dpy, move_cursor);
    XFreeCursor (dpy, busy_cursor);
    xineramaFree ();
    sessionFreeWindowStates ();
    for (i = 0; i < 7; i++)
    {
        XFreeCursor (dpy, resize_cursor[i]);
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
    removeTmpEventWin (sidewalk[0]);
    removeTmpEventWin (sidewalk[1]);
    XSetInputFocus (dpy, root, RevertToPointerRoot, CurrentTime);
    closeEventFilter ();
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
    quit = TRUE;
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
            quit = TRUE;
            break;
        case SIGHUP:
        case SIGUSR1:
            reload = TRUE;
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

    TRACE ("entering initialize");

    progname = argv[0];

#if 0
#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
    textdomain (GETTEXT_PACKAGE);
#endif
#else
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
#endif

    gtk_set_locale ();
    gtk_init (&argc, &argv);
    gdk_rgb_init();

    gtk_widget_push_visual(gdk_rgb_get_visual ());
    gtk_widget_push_colormap(gdk_rgb_get_cmap ());
    
    DBG ("xfwm4 starting, using GTK+-%d.%d.%d", gtk_major_version, 
         gtk_minor_version, gtk_micro_version);

    dpy = GDK_DISPLAY ();
    root = GDK_ROOT_WINDOW ();
    xscreen = DefaultScreenOfDisplay(dpy);
    screen = XDefaultScreen (dpy);
    depth = DefaultDepth (dpy, screen);
    cmap = GDK_COLORMAP_XCOLORMAP(gdk_rgb_get_cmap ());
    sn_init_display (dpy, screen);
    workspace = 0;

    XSetErrorHandler (handleXError);
    shape = XShapeQueryExtension (dpy, &shape_event, &dummy);
    use_xinerama = xineramaInit (dpy);
    xinerama_heads = xineramaGetHeads ();

    client_session =
        client_session_new (argc, argv, NULL, SESSION_RESTART_IF_RUNNING, 20);
    client_session->data = (gpointer) client_session;
    client_session->save_phase_2 = save_phase_2;
    client_session->die = session_die;

    if (session_init (client_session))
    {
        load_saved_session (client_session);
    }

    /* Create the side windows to detect edge movement */
    sidewalk[0] = setTmpEventWin (0, 0, 
                                  1, MyDisplayFullHeight (dpy, screen), 
                                  LeaveWindowMask | PointerMotionMask);

    sidewalk[1] = setTmpEventWin (MyDisplayFullWidth (dpy, screen) - 1, 0, 
                                  1, MyDisplayFullHeight (dpy, screen), 
                                  LeaveWindowMask | PointerMotionMask);

    margins[TOP] = gnome_margins[TOP] = 0;
    margins[LEFT] = gnome_margins[LEFT] = 0;
    margins[RIGHT] = gnome_margins[RIGHT] = 0;
    margins[BOTTOM] = gnome_margins[BOTTOM] = 0;

    initICCCMHints (dpy);
    initMotifHints (dpy);
    initGnomeHints (dpy);
    initNetHints (dpy);
    initKDEHints (dpy);
    initSystrayHints (dpy, screen);
    
    initModifiers (dpy);

    root_cursor = XCreateFontCursor (dpy, XC_left_ptr);
    move_cursor = XCreateFontCursor (dpy, XC_fleur);
    busy_cursor = cursorCreateSpinning (dpy, root);
    resize_cursor[CORNER_TOP_LEFT] =
        XCreateFontCursor (dpy, XC_top_left_corner);
    resize_cursor[CORNER_TOP_RIGHT] =
        XCreateFontCursor (dpy, XC_top_right_corner);
    resize_cursor[CORNER_BOTTOM_LEFT] =
        XCreateFontCursor (dpy, XC_bottom_left_corner);
    resize_cursor[CORNER_BOTTOM_RIGHT] =
        XCreateFontCursor (dpy, XC_bottom_right_corner);
    resize_cursor[4 + SIDE_LEFT] = XCreateFontCursor (dpy, XC_left_side);
    resize_cursor[4 + SIDE_RIGHT] = XCreateFontCursor (dpy, XC_right_side);
    resize_cursor[4 + SIDE_BOTTOM] = XCreateFontCursor (dpy, XC_bottom_side);

    XDefineCursor (dpy, root, root_cursor);

    if (!initEventFilter (MAIN_EVENT_MASK, NULL, "xfwm"))
    {
        return -1;
    }
    pushEventFilter (xfwm4_event_filter, NULL);

    gnome_win = getDefaultXWindow ();
    DBG ("Our event window is 0x%lx", gnome_win);

    if (!initSettings ())
    {
        return -2;
    }

    systray = getSystrayWindow (dpy);
    setGnomeProtocols (dpy, screen, gnome_win);
    setHint (dpy, root, win_supporting_wm_check, gnome_win);
    setHint (dpy, root, win_desktop_button_proxy, gnome_win);
    setHint (dpy, gnome_win, win_desktop_button_proxy, gnome_win);
    getHint (dpy, root, win_workspace, &ws);
    workspace = (int) ws;
    getGnomeDesktopMargins (dpy, screen, gnome_margins);
    set_utf8_string_hint (dpy, gnome_win, net_wm_name, "Xfwm4");
    setNetSupportedHint (dpy, screen, gnome_win);
    workspaceUpdateArea (margins, gnome_margins);
    initNetDesktopParams (dpy, screen, workspace);
    setNetWorkarea (dpy, screen, params.workspace_count, margins);
    XSetInputFocus (dpy, gnome_win, RevertToNone, CurrentTime);
    initGtkCallbacks ();
    
    /* The first time the first Gtk application on a display uses pango,
     * pango grabs the XServer while it creates the font cache window.
     * Therefore, force the cache window to be created now instead of
     * trying to do it while we have another grab and deadlocking the server.
     */
    layout = gtk_widget_create_pango_layout (getDefaultGtkWidget (), "-");
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
