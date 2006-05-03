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
 
        xfwm4    - (c) 2002-2006 Olivier Fourdan
 
 */
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include <libxfce4util/libxfce4util.h>
#include "event_filter.h"

/* static var for backward compat */
static XfwmFilterSetup *p_filter_setup = NULL;
static GtkWidget *p_filter_gtk_win     = NULL;
static GdkWindow *p_filter_event_win   = NULL;

static XfwmFilterStatus
default_event_filter (XEvent * xevent, gpointer data)
{
    switch (xevent->type)
    {
        case KeyPress:
            TRACE ("Unhandled KeyPress event");
            break;
        case KeyRelease:
            TRACE ("Unhandled KeyRelease event");
            break;
        case ButtonPress:
            TRACE ("Unhandled ButtonPress event");
            break;
        case ButtonRelease:
            TRACE ("Unhandled ButtonRelease event");
            break;
        case MotionNotify:
            TRACE ("Unhandled MotionNotify event");
            break;
        case EnterNotify:
            TRACE ("Unhandled EnterNotify event");
            break;
        case LeaveNotify:
            TRACE ("Unhandled LeaveNotify event");
            break;
        case FocusIn:
            TRACE ("Unhandled FocusIn event");
            break;
        case FocusOut:
            TRACE ("Unhandled FocusOut event");
            break;
        case KeymapNotify:
            TRACE ("Unhandled KeymapNotify event");
            break;
        case Expose:
            TRACE ("Unhandled Expose event");
            break;
        case GraphicsExpose:
            TRACE ("Unhandled GraphicsExpose event");
            break;
        case NoExpose:
            TRACE ("Unhandled NoExpose event");
            break;
        case VisibilityNotify:
            TRACE ("Unhandled VisibilityNotify event");
            break;
        case DestroyNotify:
            TRACE ("Unhandled DestroyNotify event");
            break;
        case UnmapNotify:
            TRACE ("Unhandled UnmapNotify event");
            break;
        case MapNotify:
            TRACE ("Unhandled MapNotify event");
            break;
        case MapRequest:
            TRACE ("Unhandled MapRequest event");
            break;
        case ReparentNotify:
            TRACE ("Unhandled ReparentNotify event");
            break;
        case ConfigureNotify:
            TRACE ("Unhandled ConfigureNotify event");
            break;
        case ConfigureRequest:
            TRACE ("Unhandled ConfigureRequest event");
            break;
        case GravityNotify:
            TRACE ("Unhandled GravityNotify event");
            break;
        case ResizeRequest:
            TRACE ("Unhandled ResizeRequest event");
            break;
        case CirculateNotify:
            TRACE ("Unhandled CirculateNotify event");
            break;
        case CirculateRequest:
            TRACE ("Unhandled CirculateRequest event");
            break;
        case PropertyNotify:
            TRACE ("Unhandled PropertyNotify event");
            break;
        case SelectionClear:
            TRACE ("Unhandled SelectionClear event");
            break;
        case SelectionRequest:
            TRACE ("Unhandled SelectionRequest event");
            break;
        case SelectionNotify:
            TRACE ("Unhandled SelectionNotify event");
            break;
        case ColormapNotify:
            TRACE ("Unhandled ColormapNotify event");
            break;
        case MappingNotify:
            TRACE ("Unhandled MappingNotify event");
            break;
        default:
            TRACE ("Unhandled Unknown event");
            break;
    }
    /* This is supposed to be the default fallback event handler, so we return XFWM_FILTER_STOP since we have "treated" the event */
    return XFWM_FILTER_STOP;
}

static GdkFilterReturn
anXXfwmFilter (GdkXEvent * gdk_xevent, GdkEvent * event, gpointer data)
{
    XEvent *xevent = (XEvent *) gdk_xevent;
    XfwmFilterStatus loop = XFWM_FILTER_CONTINUE;
    XfwmFilterSetup *setup = (XfwmFilterSetup *) data;
    XfwmFilterStack *filterelt;

    g_return_val_if_fail (setup != NULL, GDK_FILTER_CONTINUE);
    filterelt = setup->filterstack;
    g_return_val_if_fail (filterelt != NULL, GDK_FILTER_CONTINUE);

    while ((filterelt) && (loop == XFWM_FILTER_CONTINUE))
    {
        XfwmFilterStack *filterelt_next = filterelt->next;
        loop = (*filterelt->filter) (xevent, filterelt->data);
        filterelt = filterelt_next;
    }
    return GDK_FILTER_CONTINUE;
}

XfwmFilterStack *
pushXfwmFilter (XfwmFilterSetup *setup, XfwmFilter filter, gpointer data)
{
    g_assert (filter != NULL);
    if (setup->filterstack)
    {
        XfwmFilterStack *newfilterstack =
            (XfwmFilterStack *) g_new (XfwmFilterStack, 1);
        newfilterstack->filter = filter;
        newfilterstack->data = data;
        newfilterstack->next = setup->filterstack;
        setup->filterstack = newfilterstack;
    }
    else
    {
        setup->filterstack =
            (XfwmFilterStack *) g_new (XfwmFilterStack, 1);
        setup->filterstack->filter = filter;
        setup->filterstack->data = data;
        setup->filterstack->next = NULL;
    }
    return (setup->filterstack);
}

XfwmFilterStack *
popXfwmFilter (XfwmFilterSetup *setup)
{
    XfwmFilterStack *oldfilterstack = setup->filterstack;
    g_return_val_if_fail (setup->filterstack != NULL, NULL);
    setup->filterstack = oldfilterstack->next;
    g_free (oldfilterstack);
    return (setup->filterstack);
}

GdkWindow *
addEventWin (GdkScreen *gscr, long event_mask)
{
    XWindowAttributes attribs;
    Display *dpy;
    Window xroot;
    GdkWindow *event_win;
    guint error;
    
    g_return_val_if_fail (gscr, NULL);
    g_return_val_if_fail (GDK_IS_SCREEN (gscr), NULL);
    
    event_win = gdk_screen_get_root_window (gscr);
    xroot = (Window) GDK_DRAWABLE_XID(event_win);
    dpy = GDK_DRAWABLE_XDISPLAY(event_win);

    gdk_error_trap_push ();
    gdk_x11_grab_server ();

    XGetWindowAttributes (dpy, xroot, &attribs);
    XSelectInput (dpy, xroot, attribs.your_event_mask | event_mask);
    
    gdk_x11_ungrab_server ();
    gdk_flush ();
    
    error = gdk_error_trap_pop ();
    if (error)
    {
        TRACE ("addEventWin error code: %i", error);
        return (NULL);
    }
    
    return event_win;
}

XfwmFilterSetup *
initXfwmFilter (gpointer data)
{
    XfwmFilterSetup *setup;
    
    setup = g_new0 (XfwmFilterSetup, 1);
    setup->filterstack = NULL;
    pushXfwmFilter (setup, default_event_filter, data);
    gdk_window_add_filter (NULL, anXXfwmFilter, (gpointer) setup);

    return (setup);
}

void
closeXfwmFilter (XfwmFilterSetup *setup)
{
    XfwmFilterStack *filterelt = setup->filterstack;
    while ((filterelt = popXfwmFilter (setup)));
    gdk_window_remove_filter (NULL, anXXfwmFilter, NULL);
    setup->filterstack = NULL;
}
