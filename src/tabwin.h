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
 
        xfwm4    - (c) 2002-2005 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifndef INC_TABWIN_H
#define INC_TABWIN_H

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

typedef struct _Tabwin Tabwin;
struct _Tabwin
{
    /* The below must be freed when destroying */
    GtkWidget *window;
    GList *head;
    /* these don't have to be */
    GtkWidget *label;
    GtkWidget *container;
    GList *current;
    gulong selected_callback;
    int grid_cols;
    int grid_rows;
};

Tabwin *tabwinCreate (GdkScreen *, Client *c, unsigned int cycle_range);
void tabwinSelectNext (Tabwin *);
void tabwinSelectPrev (Tabwin *);
void tabwinRemoveClient(Tabwin *t, Client *c);
void tabwinDestroy (Tabwin *);
void tabwinAppendClient(Tabwin *t, Client *c, int cycle_range);

#endif /* INC_TABWIN_H */
