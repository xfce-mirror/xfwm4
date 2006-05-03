/*
 * xfce4    - (c) 2002 Olivier Fourdan
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

#ifndef __GTKSTYLE_H__
#define __GTKSTYLE_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <pango/pango-font.h>

gchar *get_style (GtkWidget *widget, const gchar *name, const gchar *state);
GdkGC *get_style_gc (GtkWidget *widget, const gchar *state, const gchar *style);
PangoFontDescription *get_font_desc (GtkWidget *widget);
PangoContext *pango_get_context (GtkWidget *widget);

#endif
