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

#ifndef __GTKTOXEVENT_H__
#define __GTKTOXEVENT_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <X11/Xlib.h>

typedef enum 
{
  XEV_FILTER_STOP = TRUE,
  XEV_FILTER_CONTINUE = FALSE
} GtkToXEventFilterStatus;

typedef GtkToXEventFilterStatus (*GtkToXEventFilter) (XEvent *xevent, gpointer data);

typedef struct GtkToXEventFilterStack
{
  GtkToXEventFilter            filter;
  gpointer                     data;
  struct GtkToXEventFilterStack *next;
} GtkToXEventFilterStack;

GtkToXEventFilterStack *pushEventFilter(GtkToXEventFilter filter, gpointer data);
GtkToXEventFilterStack *popEventFilter(void);
GtkToXEventFilterStack *initEventFilter(long event_mask, gpointer data, const gchar *widget_name);
void closeEventFilter(void);
GtkWidget *getDefaultGtkWidget(void);
Window getDefaultXWindow(void);
GdkWindow *getGdkEventWindow(void);
GdkWindow *getDefaultGdkWindow(void);

#endif /* __GTKTOXEVENT_H__ */
