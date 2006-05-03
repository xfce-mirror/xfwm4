/*
 * xfce4    - (c) 2002-2004 Olivier Fourdan
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include <libxfce4util/libxfce4util.h>
#include "gtktoxevent.h"

/* static var for backward compat */
static XfceFilterSetup *p_filter_setup = NULL;
static GtkWidget *p_filter_gtk_win     = NULL;
static GdkWindow *p_filter_event_win   = NULL;

static XfceFilterStatus
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
    /* This is supposed to be the default fallback event handler, so we return XEV_FILTER_STOP since we have "treated" the event */
    return XEV_FILTER_STOP;
}

static GdkFilterReturn
anXEventFilter (GdkXEvent * gdk_xevent, GdkEvent * event, gpointer data)
{
    XEvent *xevent = (XEvent *) gdk_xevent;
    XfceFilterStatus loop = XEV_FILTER_CONTINUE;
    XfceFilterSetup *setup = (XfceFilterSetup *) data;
    XfceFilterStack *filterelt;

    g_return_val_if_fail (setup != NULL, GDK_FILTER_CONTINUE);
    filterelt = setup->filterstack;
    g_return_val_if_fail (filterelt != NULL, GDK_FILTER_CONTINUE);

    while ((filterelt) && (loop == XEV_FILTER_CONTINUE))
    {
        XfceFilterStack *filterelt_next = filterelt->next;
        loop = (*filterelt->filter) (xevent, filterelt->data);
        filterelt = filterelt_next;
    }
    return GDK_FILTER_CONTINUE;
}

XfceFilterStack *
xfce_push_event_filter (XfceFilterSetup *setup, XfceFilter filter, gpointer data)
{
    g_assert (filter != NULL);
    if (setup->filterstack)
    {
        XfceFilterStack *newfilterstack =
            (XfceFilterStack *) g_new (XfceFilterStack, 1);
        newfilterstack->filter = filter;
        newfilterstack->data = data;
        newfilterstack->next = setup->filterstack;
        setup->filterstack = newfilterstack;
    }
    else
    {
        setup->filterstack =
            (XfceFilterStack *) g_new (XfceFilterStack, 1);
        setup->filterstack->filter = filter;
        setup->filterstack->data = data;
        setup->filterstack->next = NULL;
    }
    return (setup->filterstack);
}

XfceFilterStack *
xfce_pop_event_filter (XfceFilterSetup *setup)
{
    XfceFilterStack *oldfilterstack = setup->filterstack;
    g_return_val_if_fail (setup->filterstack != NULL, NULL);
    setup->filterstack = oldfilterstack->next;
    g_free (oldfilterstack);
    return (setup->filterstack);
}

GdkWindow *
xfce_add_event_win (GdkScreen *gscr, long event_mask)
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
        TRACE ("xfce_add_event_win error code: %i", error);
        return (NULL);
    }
    
    return event_win;
}

XfceFilterSetup *
xfce_init_event_filter (gpointer data)
{
    XfceFilterSetup *setup;
    
    setup = g_new0 (XfceFilterSetup, 1);
    setup->filterstack = NULL;
    xfce_push_event_filter (setup, default_event_filter, data);
    gdk_window_add_filter (NULL, anXEventFilter, (gpointer) setup);

    return (setup);
}

void
xfce_close_event_filter (XfceFilterSetup *setup)
{
    XfceFilterStack *filterelt = setup->filterstack;
    while ((filterelt = xfce_pop_event_filter (setup)));
    gdk_window_remove_filter (NULL, anXEventFilter, NULL);
    setup->filterstack = NULL;
}

/* Old backward comapt/deprecated functions */

XfceFilterStack *
pushEventFilter (XfceFilter filter, gpointer data)
{
    g_assert (filter != NULL);
    return xfce_push_event_filter (p_filter_setup, filter, data);
}

XfceFilterStack *
popEventFilter (void)
{
    return xfce_pop_event_filter (p_filter_setup);
}

XfceFilterStack *
initEventFilter (long event_mask, gpointer data, const gchar * widget_name)
{
    p_filter_setup = xfce_init_event_filter (data);
    if (!p_filter_setup)
    {
        return NULL;
    }
    p_filter_event_win = xfce_add_event_win (gdk_screen_get_default (), event_mask);
    
    if (!p_filter_event_win)
    {
        xfce_close_event_filter (p_filter_setup);
        p_filter_setup = NULL;
        return NULL;
    }
    
    p_filter_gtk_win = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_window_resize (GTK_WINDOW (p_filter_gtk_win), 5, 5);
    gtk_window_move (GTK_WINDOW (p_filter_gtk_win), -1000, -1000);
    if (widget_name)
    {
        gtk_widget_set_name (p_filter_gtk_win, widget_name);
    }
    gtk_widget_show_now (p_filter_gtk_win);
    gdk_window_set_user_data (p_filter_event_win, p_filter_gtk_win);
    gdk_flush ();

    return p_filter_setup->filterstack;
}

void
closeEventFilter (void)
{
    xfce_close_event_filter (p_filter_setup);
    g_free (p_filter_setup);
    p_filter_setup = NULL;
    if (p_filter_event_win)
    {
        p_filter_event_win = NULL;
    }
    if (p_filter_gtk_win)
    {
        gtk_widget_destroy (p_filter_gtk_win);
        p_filter_gtk_win = NULL;
    }
}

GtkWidget *
getDefaultGtkWidget (void)
{
    return (GTK_WIDGET (p_filter_gtk_win));
}

Window
getDefaultXWindow (void)
{
    return GDK_WINDOW_XWINDOW (p_filter_gtk_win->window);
}

GdkWindow *
getGdkEventWindow (void)
{
    return (GdkWindow *) p_filter_event_win;
}

GdkWindow *
getDefaultGdkWindow (void)
{
    return (GdkWindow *) p_filter_gtk_win->window;
}
