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

        sawfish  - (c) 1999 John Harper
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifndef __GTKSTYLE_H__
#define __GTKSTYLE_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <pango/pango-font.h>

gchar *get_style(GtkWidget *, gchar *, gchar *);
GdkGC *get_style_gc(GtkWidget *, gchar *, gchar *);
PangoFontDescription *get_font_desc(GtkWidget *);
PangoContext *pango_get_context(GtkWidget *);

#endif
