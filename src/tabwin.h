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

        xfwm4    - (c) 2003 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifndef INC_TABWIN_H__
#define INC_TABWIN_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include "debug.h"

typedef struct _Tabwin Tabwin;
struct _Tabwin
{
    GtkWidget *window;
    GtkWidget *label;
};

Tabwin *tabwinCreate(gchar * label);
void tabwinSetLabel(Tabwin * tabwin, gchar * label);
void tabwinDestroy(Tabwin * tabwin);

#endif /* INC_TABWIN_H__ */
