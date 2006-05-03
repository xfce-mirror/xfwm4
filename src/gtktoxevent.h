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

#ifndef __GTKTOXEVENT_H__
#define __GTKTOXEVENT_H__

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

/* this formatting is needed by glib-mkenums */
typedef enum /*< prefix=XEV_FILTER_ >*/ {
    XEV_FILTER_STOP = TRUE,
    XEV_FILTER_CONTINUE = FALSE
}
XfceFilterStatus;

typedef XfceFilterStatus (*XfceFilter) (XEvent * xevent, gpointer data);

typedef struct XfceFilterStack
{
    XfceFilter filter;
    gpointer data;
    struct XfceFilterStack *next;
}
XfceFilterStack;

typedef struct XfceFilterSetup
{
    XfceFilterStack *filterstack;
}
XfceFilterSetup;

XfceFilterStack *xfce_push_event_filter  (XfceFilterSetup *setup,
                                          XfceFilter filter,
                                          gpointer data);
XfceFilterStack * xfce_pop_event_filter  (XfceFilterSetup *setup);
GdkWindow * xfce_add_event_win           (GdkScreen *gscr, 
                                          long event_mask);
XfceFilterSetup * xfce_init_event_filter (gpointer data);
void xfce_close_event_filter             (XfceFilterSetup *setup);

#ifndef XFCE_DISABLE_DEPRECATED

#define GtkToXEventFilterStatus XfceFilterStatus
#define GtkToXEventFilterStack  XfceFilterStack

typedef GtkToXEventFilterStatus (*GtkToXEventFilter) (XEvent * xevent,
                                                      gpointer data);

GtkToXEventFilterStack *pushEventFilter (GtkToXEventFilter filter,
                                         gpointer data);
GtkToXEventFilterStack *popEventFilter  (void);
GtkToXEventFilterStack *initEventFilter (long event_mask, 
                                         gpointer data,
                                         const gchar * widget_name);
void closeEventFilter                   (void);
GtkWidget *getDefaultGtkWidget          (void);
Window getDefaultXWindow                (void);
GdkWindow *getGdkEventWindow            (void);
GdkWindow *getDefaultGdkWindow          (void);

#endif /* XFCE_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* __GTKTOXEVENT_H__ */
