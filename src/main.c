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
#include "compositor.h"
#include "spinning_cursor.h"

#define BASE_EVENT_MASK \
    SubstructureNotifyMask|\
    StructureNotifyMask|\
    SubstructureRedirectMask|\
    ButtonPressMask|\
    ButtonReleaseMask|\
    FocusChangeMask|\
    PropertyChangeMask|\
    ColormapNotify

#ifdef HAVE_COMPOSITOR
#define MAIN_EVENT_MASK BASE_EVENT_MASK|ExposureMask
#else /* HAVE_COMPOSITOR */
#define MAIN_EVENT_MASK BASE_EVENT_MASK
#endif /* HAVE_COMPOSITOR */

#ifndef DEBUG
/* For what, IEEE Std 1003.1-2001, Section 12.2, Utility Syntax Guidelines.*/
static char revision[]="@(#)$ " PACKAGE " version " VERSION " revision " REVISION " $";
#endif

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

static char *
build_session_filename(SessionClient *client_session)
{
    gchar *filename, *path, *file;
    GError *error = NULL;

    path = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "sessions", FALSE);
    
    if (!xfce_mkdirhier(path, 0700, &error)) 
    {
        g_warning("Unable to create session dir %s: %s", path, error->message);
        g_error_free (error);
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
            g_error (_("%s: Segmentation fault"), PACKAGE);
            break;
        default:
            break;
    }
}

static void
ensure_basedir_spec (void)
{
    char *new, *old, path[PATH_MAX];
    GError *error = NULL;
    GDir *gdir;
    const char *name;

    /* test if new directory is there */
    
    new = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, 
                                       "xfce4" G_DIR_SEPARATOR_S "xfwm4", 
                                       FALSE);
    
    if (g_file_test (new, G_FILE_TEST_IS_DIR))
    {
        g_free (new);
        return;
    }

    if (!xfce_mkdirhier(new, 0700, &error)) 
    {
        g_warning("Unable to create config dir %s: %s", new, error->message);
        g_error_free (error);
        g_free (new);
        return;
    }

    g_free (new);

    /* copy xfwm4rc */
    
    old = xfce_get_userfile ("xfwm4rc", NULL);

    if (g_file_test (old, G_FILE_TEST_EXISTS))
    {
        FILE *r, *w;

        g_strlcpy (path, "xfce4/xfwm4/xfwm4rc", PATH_MAX);
        new = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, path, FALSE);

        r = fopen (old, "r");
        w = fopen (new, "w");

        g_free (new);
        
        if (w && r)
        {
            char c;
            
            while ((c = getc (r)) != EOF)
            {
                putc (c, w);
            }
        }

        if (r)
        {
            fclose (r);
        }

        if (w)
        {
            fclose (w);
        }
    }
    
    g_free (old);

    /* copy saved session data */
    
    new = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "sessions", FALSE);

    if (!xfce_mkdirhier(new, 0700, &error)) 
    {
        g_warning("Unable to create session dir %s: %s", new, error->message);
        g_error_free (error);
        g_free (new);
        return;
    }

    old = xfce_get_userfile ("sessions", NULL);
    gdir = g_dir_open (old, 0, NULL);

    if (gdir)
    {
        while ((name = g_dir_read_name (gdir)) != NULL)
        {
            FILE *r, *w;

            g_snprintf (path, PATH_MAX, "%s/%s", old, name);
            r = fopen (path, "r");

            g_snprintf (path, PATH_MAX, "%s/%s", new, name);
            w = fopen (path, "w");

            if (w && r)
            {
                char c;
                
                while ((c = getc (r)) != EOF)
                    putc (c, w);
            }

            if (r)
                fclose (r);
            if (w)
                fclose (w);
        }

        g_dir_close (gdir);
    }

    g_free (old);
    g_free (new);
}

static void
print_usage (void)
{
    g_print ("%s [--sm-client-id=ID] [--display=DISPLAY] "
#ifdef HAVE_COMPOSITOR
             "[--compositor=off|on|auto] "
#endif
             "[--daemon] [--version|-V] [--help|-H]\n", PACKAGE);
    exit (0);
}

static void
print_version (void)
{
    g_print ("\tThis is %s version %s (revision %s) for Xfce %s\n", 
                    PACKAGE, VERSION, REVISION, xfce_version_string());
    g_print ("\tReleased under the terms of the GNU General Public License.\n"); 
    g_print ("\tCompiled against GTK+-%d.%d.%d, ", 
                    GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
    g_print ("using GTK+-%d.%d.%d.\n", 
                    gtk_major_version, gtk_minor_version, gtk_micro_version);
    g_print ("\n");
    g_print ("\tBuild configuration and supported features:\n");

    g_print ("\t- Startup notification support:                 ");
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif    

    g_print ("\t- Render support:                               ");
#ifdef HAVE_RENDER
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif    

    g_print ("\t- Xrandr support:                               ");
#ifdef HAVE_RANDR
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif    

    g_print ("\t- Embedded compositor:                          ");
#ifdef HAVE_COMPOSITOR
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif    
    exit (0);
}

static gint
parse_compositor (const gchar *s)
{
    gchar *rvalue;
    gboolean retval = 2;

    rvalue = strrchr (s, '=');
    if (rvalue)
    {
        rvalue++;
        if (!strcmp (rvalue, "off"))
        {
            retval=0;
        }
        else if (!strcmp (rvalue, "auto"))
        {
            retval=1;
        }
        else if (!strcmp (rvalue, "on"))
        {
            retval=2;
        }
        else
        {
            g_warning ("Unrecognized compositor option \"%s\"", rvalue);
        }
    }

    return retval;
}

static int
initialize (int argc, char **argv, gint compositor_mode)
{
    struct sigaction act;
    long ws;
    SessionClient *client_session;
    gint i, nscreens;
    
    TRACE ("entering initialize");
    
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    gtk_init (&argc, &argv);

    DBG ("xfwm4 starting, using GTK+-%d.%d.%d", gtk_major_version, 
         gtk_minor_version, gtk_micro_version);

    ensure_basedir_spec ();
    
    initMenuEventWin ();
    clientClearFocus ();

    display_info = myDisplayInit (gdk_display_get_default ());
    
    /* Disabling compositor from command line */
    if (!compositor_mode)
    {
        display_info->enable_compositor = FALSE;
    }
    
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
        
        if (compositor_mode)
        {
            if (compositorManageScreen (screen_info, (compositor_mode == 2)))
            {
                setCompositingManagerOwner (display_info,  screen_info->xroot, screen_info->xfwm4_win);
            }
        }

        sn_init_display (screen_info);
        myDisplayAddScreen (display_info, screen_info);
        setGnomeProtocols (display_info, screen_info->xroot, screen_info->xfwm4_win);
        setHint (display_info, screen_info->xroot, WIN_SUPPORTING_WM_CHECK, screen_info->xfwm4_win);
        setHint (display_info, screen_info->xroot, WIN_DESKTOP_BUTTON_PROXY, screen_info->xfwm4_win);
        setHint (display_info, screen_info->xfwm4_win, WIN_DESKTOP_BUTTON_PROXY, screen_info->xfwm4_win);
        getHint (display_info, screen_info->xroot, WIN_WORKSPACE, &ws);
        screen_info->current_ws = (int) ws;
        getGnomeDesktopMargins (display_info, screen_info->xroot, screen_info->gnome_margins);
        setUTF8StringHint (display_info, screen_info->xfwm4_win, NET_WM_NAME, "Xfwm4");
        setNetSupportedHint (display_info, screen_info->xroot, screen_info->xfwm4_win);
        initNetDesktopInfo (display_info, screen_info->xroot, screen_info->current_ws,
                                   gdk_screen_get_width (screen_info->gscr), 
                                   gdk_screen_get_height (screen_info->gscr));
        workspaceUpdateArea (screen_info);
        XSetInputFocus (display_info->dpy, screen_info->xfwm4_win, RevertToPointerRoot, CurrentTime);

        clientFrameAll (screen_info);
        
        initGtkCallbacks (screen_info);

        XDefineCursor (display_info->dpy, screen_info->xroot, myDisplayGetCursorRoot(display_info));
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
        return 1;
    }

    return 0;
}

int
main (int argc, char **argv)
{
    int i;
    gboolean daemon_mode = FALSE;
    gint compositor = 2;
    int status;

    DBG ("xfwm4 starting");
    for (i = 1; i < argc; i++)
    {
        if (!strcmp (argv[i], "--daemon"))
        {
            daemon_mode = TRUE;
        }
        else if (!strncmp (argv[i], "--compositor=", strlen ("--compositor=")))
        {
            compositor = parse_compositor (argv[i]);
        }
        else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-V"))
        {
            print_version ();
        }
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-H"))
        {
            print_usage ();
        }
    }

    status = initialize (argc, argv, compositor);
    /* 
       status  < 0   =>   Error, cancel execution
       status == 0   =>   Run w/out session manager
       status == 1   =>   Connected to session manager
     */
    switch (status)
    {
        case -1:
            g_warning ("Another Window Manager is already running");
            exit (1);
            break;
        case -2:
            g_warning ("Missing data from default files");
            exit (1);
            break;
        case 0:
            if (daemon_mode)
            {
#ifdef HAVE_DAEMON
                if (daemon(TRUE, TRUE) < 0) 
                {
                        g_warning("Failed to enter daemon mode: %s", g_strerror(errno));
                        exit(EXIT_FAILURE);
                }
#else /* !HAVE_DAEMON */
                switch (fork ())
                {
                    case -1:
                        g_warning ("Failed to create new process: %s", g_strerror(errno));
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
            /* Walk through */
        case 1:
            /* enter GTK main loop */
            gtk_main ();
            break;
        default:
            g_warning ("Unknown error occured");
            exit (1);
            break;
    }
    cleanUp ();
    DBG ("xfwm4 terminated");
    return 0;
}
