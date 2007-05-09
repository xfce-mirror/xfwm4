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

        shortcut editor - Copyright (C) 2004 Jean-Francois Wauthy
*/

#ifndef INC_XFWM4_SHORTCUTEDITOR_H
#define INC_XFWM4_SHORTCUTEDITOR_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

void cb_popup_add_menu (GtkWidget *, gpointer);
void cb_popup_del_menu (GtkWidget *, gpointer);
gboolean cb_popup_menu (GtkTreeView *, GdkEventButton *, gpointer);
void loadtheme_in_treeview (ThemeInfo *, gpointer);
void savetreeview_in_theme (gchar *, gpointer);

void cb_activate_treeview3 (GtkWidget *, GtkTreePath *,
                            GtkTreeViewColumn *, gpointer);
#endif /* INC_XFWM4_SHORTCUTEDITOR_H */
