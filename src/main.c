/*
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

#include "display.h"
#include "screen.h"
#include "events.h"
#include "frame.h"
#include "settings.h"
#include "client.h"
#include "menu.h"
#include "focus.h"
#include "keyboard.h"
#include "workspaces.h"
#include "mywindow.h"
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

static DisplayInfo *display_info = NULL;
gboolean xfwm4_quit           = FALSE;
gboolean xfwm4_reload         = FALSE;

static void
cleanUp (void)
{
    GSList *screens;
    
    g_return_if_fail (display_info);
    
    TRACE ("entering cleanUp");

    xfce_close_event_filter (display_info->xfilter);
    for (screens = display_info->screens; screens; screens = g_slist_next (screens))
    {
        ScreenInfo *screen_info_n = (ScreenInfo *) screens->data;
        myScreenClose (screen_info_n);
        g_free (screen_info_n);
    }
    sn_close_display ();
    sessionFreeWindowStates ();
    
    myDisplayClose (display_info);
    g_free (display_info);
    display_info = NULL;
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
        sessionSaveWindowStates (display_info, filename);
        g_free (filename);
    }
}

static void
session_die (gpointer client_data)
{
    gtk_main_quit ();
    xfwm4_quit = TRUE;
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
            xfwm4_quit = TRUE;
            break;
        case SIGHUP:
        case SIGUSR1:
            xfwm4_reload = TRUE;
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
    struct sigaction act;
    long ws;
    SessionClient *client_session;
    gint i, nscreens;
    
    TRACE ("entering initialize");
    
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    gtk_set_locale ();
    gtk_init (&argc, &argv);

    DBG ("xfwm4 starting, using GTK+-%d.%d.%d", gtk_major_version, 
         gtk_minor_version, gtk_micro_version);

    initMenuEventWin ();
    clientClearFocus ();

    display_info = myDisplayInit (gdk_display_get_default ());

    initModifiers (display_info->dpy);

    act.sa_handler = handleSignal;
    act.sa_flags = 0;
    sigaction (SIGINT,  &act, NULL);
    sigaction (SIGTERM, &act, NULL);
    sigaction (SIGHUP,  &act, NULL);
    sigaction (SIGUSR1, &act, NULL);
    sigaction (SIGSEGV, &act, NULL);

    nscreens = gdk_display_get_n_screens(display_info->gdisplay);
    for(i = 0; i < nscreens; i++) 
    {
        ScreenInfo *screen_info = NULL;
        GdkScreen *gscr;
        
        gscr = gdk_display_get_screen(display_info->gdisplay, i);
        screen_info = myScreenInit (display_info, gscr, MAIN_EVENT_MASK);

        if (!screen_info)
        {
            continue;
        }
	
        if (!initSettings (screen_info))
        {
            return -2;
        }
	
        sn_init_display (screen_info);
        myDisplayAddScreen (display_info, screen_info);
        setGnomeProtocols (display_info->dpy, screen_info->screen, screen_info->gnome_win);
        setHint (display_info->dpy, screen_info->xroot, win_supporting_wm_check, screen_info->gnome_win);
        setHint (display_info->dpy, screen_info->xroot, win_desktop_button_proxy, screen_info->gnome_win);
        setHint (display_info->dpy, screen_info->gnome_win, win_desktop_button_proxy, screen_info->gnome_win);
        getHint (display_info->dpy, screen_info->xroot, win_workspace, &ws);
        screen_info->current_ws = (int) ws;
        getGnomeDesktopMargins (display_info->dpy, screen_info->screen, screen_info->gnome_margins);
        set_utf8_string_hint (display_info->dpy, screen_info->gnome_win, net_wm_name, "Xfwm4");
        setNetSupportedHint (display_info->dpy, screen_info->screen, screen_info->gnome_win);
        initNetDesktopInfo (display_info->dpy, screen_info->screen, 
                                   screen_info->current_ws,
                                   gdk_screen_get_width (screen_info->gscr), 
                                   gdk_screen_get_height (screen_info->gscr));
        workspaceUpdateArea (screen_info);
        XSetInputFocus (display_info->dpy, screen_info->gnome_win, RevertToPointerRoot, GDK_CURRENT_TIME);

        clientFrameAll (screen_info);
        
        initGtkCallbacks (screen_info);
    }
    /* No screen to manage, give up */
    if (!display_info->nb_screens)
    {
        return -1;
    }
    display_info->xfilter = xfce_init_event_filter ((gpointer) display_info);
    xfce_push_event_filter (display_info->xfilter, xfwm4_event_filter, (gpointer) display_info);

    client_session = client_session_new (argc, argv, (gpointer) display_info, 
                                         SESSION_RESTART_IF_RUNNING, 20);
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
                      PACKAGE, VERSION, xfce_version_string());
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
                if (daemon(TRUE, TRUE) < 0) 
                {
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
