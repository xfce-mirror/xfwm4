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

        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "debug.h"
#include "gtktoxevent.h"

static GtkToXEventFilterStack *filterstack = NULL;
static GtkWidget *gtk_win = NULL;
static GdkWindow *event_win = NULL;

static GtkToXEventFilterStatus default_event_filter(XEvent * xevent, gpointer data)
{
    switch (xevent->type)
    {
        case KeyPress:
            DBG("Unhandled KeyPress event");
            break;
        case KeyRelease:
            DBG("Unhandled KeyRelease event");
            break;
        case ButtonPress:
            DBG("Unhandled ButtonPress event");
            break;
        case ButtonRelease:
            DBG("Unhandled ButtonRelease event");
            break;
        case MotionNotify:
            DBG("Unhandled MotionNotify event");
            break;
        case EnterNotify:
            DBG("Unhandled EnterNotify event");
            break;
        case LeaveNotify:
            DBG("Unhandled LeaveNotify event");
            break;
        case FocusIn:
            DBG("Unhandled FocusIn event");
            break;
        case FocusOut:
            DBG("Unhandled FocusOut event");
            break;
        case KeymapNotify:
            DBG("Unhandled KeymapNotify event");
            break;
        case Expose:
            DBG("Unhandled Expose event");
            break;
        case GraphicsExpose:
            DBG("Unhandled GraphicsExpose event");
            break;
        case NoExpose:
            DBG("Unhandled NoExpose event");
            break;
        case VisibilityNotify:
            DBG("Unhandled VisibilityNotify event");
            break;
        case DestroyNotify:
            DBG("Unhandled DestroyNotify event");
            break;
        case UnmapNotify:
            DBG("Unhandled UnmapNotify event");
            break;
        case MapNotify:
            DBG("Unhandled MapNotify event");
            break;
        case MapRequest:
            DBG("Unhandled MapRequest event");
            break;
        case ReparentNotify:
            DBG("Unhandled ReparentNotify event");
            break;
        case ConfigureNotify:
            DBG("Unhandled ConfigureNotify event");
            break;
        case ConfigureRequest:
            DBG("Unhandled ConfigureRequest event");
            break;
        case GravityNotify:
            DBG("Unhandled GravityNotify event");
            break;
        case ResizeRequest:
            DBG("Unhandled ResizeRequest event");
            break;
        case CirculateNotify:
            DBG("Unhandled CirculateNotify event");
            break;
        case CirculateRequest:
            DBG("Unhandled CirculateRequest event");
            break;
        case PropertyNotify:
            DBG("Unhandled PropertyNotify event");
            break;
        case SelectionClear:
            DBG("Unhandled SelectionClear event");
            break;
        case SelectionRequest:
            DBG("Unhandled SelectionRequest event");
            break;
        case SelectionNotify:
            DBG("Unhandled SelectionNotify event");
            break;
        case ColormapNotify:
            DBG("Unhandled ColormapNotify event");
            break;
        case MappingNotify:
            DBG("Unhandled MappingNotify event");
            break;
        default:
            DBG("Unhandled Unknown event");
            break;
    }
    /* This is supposed to be the default fallback event handler, so we return XEV_FILTER_STOP since we have "treated" the event */
    return XEV_FILTER_STOP;
}

static GdkFilterReturn gdkXEventFilter(GdkXEvent * gdk_xevent, GdkEvent * event, gpointer data)
{
    XEvent *xevent = (XEvent *) gdk_xevent;
    GtkToXEventFilterStatus loop = XEV_FILTER_CONTINUE;
    GtkToXEventFilterStack *filterelt = filterstack;

    g_return_val_if_fail(filterelt != NULL, GDK_FILTER_CONTINUE);

    while((filterelt) && (loop == XEV_FILTER_CONTINUE))
    {
        loop = (*filterelt->filter) (xevent, filterelt->data);
        filterelt = filterelt->next;
    }
    return GDK_FILTER_CONTINUE;
}

GtkToXEventFilterStack *pushEventFilter(GtkToXEventFilter filter, gpointer data)
{
    g_assert(filter != NULL);
    if(filterstack)
    {
        GtkToXEventFilterStack *newfilterstack = (GtkToXEventFilterStack *) g_new(GtkToXEventFilterStack, 1);
        newfilterstack->filter = filter;
        newfilterstack->data = data;
        newfilterstack->next = filterstack;
        filterstack = newfilterstack;
    }
    else
    {
        filterstack = (GtkToXEventFilterStack *) g_new(GtkToXEventFilterStack, 1);
        filterstack->filter = filter;
        filterstack->data = data;
        filterstack->next = NULL;
    }
    return (filterstack);
}

GtkToXEventFilterStack *popEventFilter(void)
{
    GtkToXEventFilterStack *oldfilterstack = filterstack;
    g_return_val_if_fail(filterstack != NULL, NULL);
    filterstack = oldfilterstack->next;
    g_free(oldfilterstack);
    return (filterstack);
}

GtkToXEventFilterStack *initEventFilter(long event_mask, gpointer data, const gchar * widget_name)
{
    XWindowAttributes attribs;

    pushEventFilter(default_event_filter, data);
    event_win = gdk_window_foreign_new(GDK_ROOT_WINDOW());
#if ((GTK_MAJOR_VERSION <= 1) && (GTK_MINOR_VERSION <= 2))
    /* This is a hack for gtk 1.2.x */
    gdk_window_add_filter(GDK_ROOT_PARENT(), gdkXEventFilter, NULL);
#endif
    gdk_window_add_filter(NULL, gdkXEventFilter, NULL);

    gdk_error_trap_push();
    XGetWindowAttributes(GDK_DISPLAY(), GDK_ROOT_WINDOW(), &attribs);
    XSelectInput(GDK_DISPLAY(), GDK_ROOT_WINDOW(), attribs.your_event_mask | event_mask);
    gdk_flush();
    if(gdk_error_trap_pop())
    {
        g_error("Another Window Manager is already running");
    }
    /* Create a GTK window so that we are just like any other GTK application */
    gtk_win = gtk_window_new(GTK_WINDOW_POPUP);
#if ((GTK_MAJOR_VERSION <= 1) && (GTK_MINOR_VERSION <= 2))
    gtk_widget_set_usize(gtk_win, 5, 5);
    gtk_widget_set_uposition(gtk_win, -1000, -1000);
    gtk_widget_set_double_buffered(GTK_WIDGET(gtk_win), FALSE);
#else
    gtk_window_resize(GTK_WINDOW(gtk_win), 5, 5);
    gtk_window_move(GTK_WINDOW(gtk_win), -1000, -1000);
#endif
    if(widget_name)
    {
        gtk_widget_set_name(gtk_win, widget_name);
    }
    gtk_widget_show_now(gtk_win);
    gdk_window_set_user_data(event_win, gtk_win);
    gdk_flush();

    return (filterstack);
}

void closeEventFilter(void)
{
    GtkToXEventFilterStack *filterelt = filterstack;
    while((filterelt = popEventFilter()));
    gdk_window_remove_filter(NULL, gdkXEventFilter, NULL);
    gdk_window_set_user_data(event_win, NULL);
    gtk_widget_destroy(gtk_win);
    gdk_flush();
}

GtkWidget *getDefaultGtkWidget(void)
{
    return (GTK_WIDGET(gtk_win));
}

Window getDefaultXWindow(void)
{
    g_return_val_if_fail(gtk_win != NULL, None);
    return (GDK_WINDOW_XWINDOW(gtk_win->window));
}

GdkWindow *getGdkEventWindow(void)
{
    return (event_win);
}

GdkWindow *getDefaultGdkWindow(void)
{
    g_return_val_if_fail(gtk_win != NULL, NULL);
    return ((GdkWindow *) gtk_win->window);
}
