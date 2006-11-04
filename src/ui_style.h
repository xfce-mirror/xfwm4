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

#ifndef __UI_STYLE_H__
#define __UI_STYLE_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <pango/pango-font.h>

gchar                   *getUIStyle                             (GtkWidget *,
                                                                 const gchar *,
                                                                 const gchar *);
GdkGC                   *getUIStyle_gc                          (GtkWidget *,
                                                                 const gchar *,
                                                                 const gchar *);
PangoFontDescription    *getUIPangoFontDesc                     (GtkWidget *);
PangoContext            *getUIPangoContext                      (GtkWidget *);

#endif /* __UI_STYLE_H__ */
