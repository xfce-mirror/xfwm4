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


        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2011 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/X.h>
#include <X11/Xlib.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include "display.h"
#include "screen.h"
#include "events.h"
#include "event_filter.h"
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
    KeyPressMask|\
    KeyReleaseMask|\
    FocusChangeMask|\
    PropertyChangeMask|\
    ColormapChangeMask

#ifdef HAVE_COMPOSITOR
#define MAIN_EVENT_MASK BASE_EVENT_MASK|ExposureMask
#else /* HAVE_COMPOSITOR */
#define MAIN_EVENT_MASK BASE_EVENT_MASK
#endif /* HAVE_COMPOSITOR */

enum {
    COMPOSITOR_MODE_OFF = 0,
    COMPOSITOR_MODE_AUTO,
    COMPOSITOR_MODE_MANUAL
};

#ifndef DEBUG
/* For what, IEEE Std 1003.1-2001, Section 12.2, Utility Syntax Guidelines.*/
static char revision[]="@(#)$ " PACKAGE " version " VERSION " revision " REVISION " $";
#endif

static DisplayInfo *main_display_info = NULL;
static gint compositor = COMPOSITOR_MODE_MANUAL;

static void
handleSignal (int sig)
{
    if (main_display_info)
    {
        switch (sig)
        {
            case SIGINT:
                /* Walk thru */
            case SIGTERM:
                gtk_main_quit ();
                main_display_info->quit = TRUE;
                break;
            case SIGHUP:
                /* Walk thru */
            case SIGUSR1:
                main_display_info->reload = TRUE;
                break;
            default:
                break;
        }
    }
}

static void
setupHandler (gboolean install)
{
    struct sigaction act;

    if (install)
        act.sa_handler = handleSignal;
    else
        act.sa_handler = SIG_DFL;

    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction (SIGINT,  &act, NULL);
    sigaction (SIGTERM, &act, NULL);
    sigaction (SIGHUP,  &act, NULL);
    sigaction (SIGUSR1, &act, NULL);
}

static void
cleanUp (void)
{
    GSList *screens;

    TRACE ("entering cleanUp");

    setupHandler (FALSE);

    g_return_if_fail (main_display_info);

    eventFilterClose (main_display_info->xfilter);
    for (screens = main_display_info->screens; screens; screens = g_slist_next (screens))
    {
        ScreenInfo *screen_info_n = (ScreenInfo *) screens->data;
        myScreenClose (screen_info_n);
        g_free (screen_info_n);
    }
    sn_close_display ();
    sessionFreeWindowStates ();

    myDisplayClose (main_display_info);
    g_free (main_display_info);
    main_display_info = NULL;
    xfconf_shutdown();
}

static void
ensure_basedir_spec (void)
{
    char *new, *old, path[PATH_MAX];
    GError *error;
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

    error = NULL;
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
            int c;

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
                int c;

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

    g_print ("\t- XSync support:                                ");
#ifdef HAVE_XSYNC
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

    g_print ("\t- KDE systray proxy (deprecated):               ");
#ifdef ENABLE_KDE_SYSTRAY_PROXY
    g_print ("Yes\n");
#else
    g_print ("No\n");
#endif
}

#ifdef HAVE_COMPOSITOR
static gint
get_default_compositor (DisplayInfo *display_info)
{
    /*
     * Don't even check for the render speed if there is no compositor.
     */
    if (!display_info->enable_compositor)
    {
        return 0;
    }

    /*
     * Check if the XServer is black listed.
     */
    if (!compositorTestServer (display_info))
    {
        g_warning ("The XServer currently in use on this system is not suitable for the compositor");
        return 0;
    }

#if 0
    /*
     * Test if the XRender implementation is fast enough for the
     * compositor.
     */
    if (!myDisplayTestXrender (display_info, 0.025))
    {
        g_warning ("The XRender implementation currently in use on this system is too slow for the compositor");
        return 0;
    }
#endif
    return 2;
}
#endif /* HAVE_COMPOSITOR */

#ifdef HAVE_COMPOSITOR
static gboolean
compositor_callback (const gchar  *name,
                     const gchar  *value,
                     gpointer      user_data,
                     GError      **error)
{
    gboolean succeed = TRUE;

    g_return_val_if_fail (value != NULL, FALSE);

    if (strcmp (value, "off") == 0)
    {
        compositor = COMPOSITOR_MODE_OFF;
    }
    else if (strcmp (value, "auto") == 0)
    {
        compositor = COMPOSITOR_MODE_AUTO;
    }
    else if (strcmp (value, "on") == 0)
    {
        compositor = COMPOSITOR_MODE_MANUAL;
    }
    else
    {
        g_set_error (error, 0, 0, "Unrecognized compositor option \"%s\"", value);
        succeed = FALSE;
    }

    return succeed;
}
#endif /* HAVE_COMPOSITOR */

static int
initialize (gint compositor_mode, gboolean replace_wm)
{
    gint i, nscreens;

    TRACE ("entering initialize");

    DBG ("xfwm4 starting, using GTK+-%d.%d.%d", gtk_major_version,
         gtk_minor_version, gtk_micro_version);

    ensure_basedir_spec ();

    initMenuEventWin ();
    clientClearFocus (NULL);
    main_display_info = myDisplayInit (gdk_display_get_default ());

#ifdef HAVE_COMPOSITOR
    if (compositor_mode < COMPOSITOR_MODE_OFF)
    {
        compositor_mode = get_default_compositor (main_display_info);
    }

    /* Disabling compositor from command line */
    if (!compositor_mode)
    {
        main_display_info->enable_compositor = FALSE;
    }
    compositorSetCompositeMode (main_display_info, (compositor_mode == COMPOSITOR_MODE_MANUAL));
#else /* HAVE_COMPOSITOR */
    main_display_info->enable_compositor = FALSE;
#endif /* HAVE_COMPOSITOR */

    initModifiers (main_display_info->dpy);

    setupHandler (TRUE);

    nscreens = gdk_display_get_n_screens (main_display_info->gdisplay);
    for(i = 0; i < nscreens; i++)
    {
        ScreenInfo *screen_info;
        GdkScreen *gscr;

        gscr = gdk_display_get_screen (main_display_info->gdisplay, i);
        screen_info = myScreenInit (main_display_info, gscr, MAIN_EVENT_MASK, replace_wm);

        if (!screen_info)
        {
            continue;
        }

        if (!initSettings (screen_info))
        {
            return -2;
        }

        if (compositor_mode == COMPOSITOR_MODE_AUTO)
        {
            compositorManageScreen (screen_info);
        }
        else if (compositor_mode == COMPOSITOR_MODE_MANUAL)
        {
            gboolean xfwm4_compositor;

            xfwm4_compositor = TRUE;
            if (screen_info->params->use_compositing)
            {
                /* Enable compositor if "use compositing" is enabled */
                xfwm4_compositor = compositorManageScreen (screen_info);
            }
            /*
               The user may want to use the manual compositing, but the installed
               system may not support it, so we need to double check, to see if
               initialization of the compositor was successful.
             */
            if (xfwm4_compositor)
            {
                /*
                   Acquire selection on XFWM4_COMPOSITING_MANAGER to advertise our own
                   compositing manager (used by WM tweaks to determine whether or not
                   show the "compositor" tab.
                 */
                setAtomIdManagerOwner (main_display_info, XFWM4_COMPOSITING_MANAGER,
                                       screen_info->xroot, screen_info->xfwm4_win);
            }
        }

        sn_init_display (screen_info);
        myDisplayAddScreen (main_display_info, screen_info);
        screen_info->current_ws = getNetCurrentDesktop (main_display_info, screen_info->xroot);
        setUTF8StringHint (main_display_info, screen_info->xfwm4_win, NET_WM_NAME, "Xfwm4");
        setNetSupportedHint (main_display_info, screen_info->xroot, screen_info->xfwm4_win);
        setNetDesktopInfo (main_display_info, screen_info->xroot, screen_info->current_ws,
                                   screen_info->width,
                                   screen_info->height);
        workspaceUpdateArea (screen_info);
        XSetInputFocus (main_display_info->dpy, screen_info->xfwm4_win, RevertToPointerRoot, CurrentTime);

        clientFrameAll (screen_info);

        initPerScreenCallbacks (screen_info);

        XDefineCursor (main_display_info->dpy, screen_info->xroot, myDisplayGetCursorRoot(main_display_info));
    }

    /* No screen to manage, give up */
    if (!main_display_info->nb_screens)
    {
        return -1;
    }
    main_display_info->xfilter = eventFilterInit ((gpointer) main_display_info);
    eventFilterPush (main_display_info->xfilter, xfwm4_event_filter, (gpointer) main_display_info);
    initPerDisplayCallbacks (main_display_info);

    return sessionStart (main_display_info);
}

int
main (int argc, char **argv)
{
    gboolean daemon_mode = FALSE;
    gboolean version = FALSE;
    gboolean replace_wm = FALSE;
    int status;
    GOptionContext *context;
    GError *error = NULL;
#ifndef HAVE_COMPOSITOR
    gchar *compositor_foo = NULL;
#endif
    GOptionEntry option_entries[] =
    {
#ifdef HAVE_DAEMON
        { "daemon", '\0', 0, G_OPTION_ARG_NONE, &daemon_mode, N_("Fork to the background"), NULL },
#else
        { "daemon", '\0', 0, G_OPTION_ARG_NONE, &daemon_mode, N_("Fork to the background (not supported)"), NULL },
#endif
#ifdef HAVE_COMPOSITOR
        { "compositor", '\0', 0, G_OPTION_ARG_CALLBACK, compositor_callback, N_("Set the compositor mode"), "on|off|auto" },
#else
        { "compositor", '\0', 0, G_OPTION_ARG_STRING, &compositor_foo, N_("Set the compositor mode (not supported)"), "on|off|auto" },
#endif
        { "replace", '\0', 0, G_OPTION_ARG_NONE, &replace_wm, N_("Replace the existing window manager"), NULL },
        { "version", 'V', 0, G_OPTION_ARG_NONE, &version, N_("Print version information and exit"), NULL },
        { NULL }
    };

    DBG ("xfwm4 starting");

    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    context = g_option_context_new (_("[ARGUMENTS...]"));
    g_option_context_add_main_entries (context, option_entries, GETTEXT_PACKAGE);
    g_option_context_add_group (context, gtk_get_option_group (FALSE));
    g_option_context_add_group (context, xfce_sm_client_get_option_group (argc, argv));
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
          g_print ("%s: %s.\n", PACKAGE_NAME, error->message);
          g_print (_("Type \"%s --help\" for usage."), G_LOG_DOMAIN);
          g_print ("\n");
          g_error_free (error);

          return EXIT_FAILURE;
    }
    g_option_context_free (context);

    gtk_init (&argc, &argv);

    if (G_UNLIKELY (version))
    {
         print_version ();
         return EXIT_SUCCESS;
    }

    status = initialize (compositor, replace_wm);
    /*
       status  < 0   =>   Error, cancel execution
       status == 0   =>   Run w/out session manager
       status == 1   =>   Connected to session manager
     */
    switch (status)
    {
        case -1:
            g_warning ("Could not find a screen to manage, exiting");
            exit (1);
            break;
        case -2:
            g_warning ("Missing data from default files");
            exit (1);
            break;
        case 0:
        case 1:
            if (daemon_mode)
            {
#ifdef HAVE_DAEMON
                if (daemon (TRUE, TRUE) < 0)
                {
                        g_warning ("Failed to enter daemon mode: %s", g_strerror(errno));
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
