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
 
#ifndef __EVENT_FILTER_H__
#define __EVENT_FILTER_H__

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <X11/Xlib.h>

/* this formatting is needed by glib-mkenums */
typedef enum {
    XFWM_FILTER_STOP = TRUE,
    XFWM_FILTER_CONTINUE = FALSE
}
XfwmFilterStatus;

typedef XfwmFilterStatus (*XfwmFilter) (XEvent * xevent, gpointer data);

typedef struct XfwmFilterStack
{
    XfwmFilter filter;
    gpointer data;
    struct XfwmFilterStack *next;
}
XfwmFilterStack;

typedef struct XfwmFilterSetup
{
    XfwmFilterStack *filterstack;
}
XfwmFilterSetup;

XfwmFilterStack *pushXfwmFilter  (XfwmFilterSetup *setup,
                                  XfwmFilter filter,
                                  gpointer data);
XfwmFilterStack * popXfwmFilter  (XfwmFilterSetup *setup);
GdkWindow * addEventWin          (GdkScreen *gscr, 
                                  long event_mask);
XfwmFilterSetup * initXfwmFilter (gpointer data);
void closeXfwmFilter             (XfwmFilterSetup *setup);

#endif /* __EVENT_FILTER_H__ */
