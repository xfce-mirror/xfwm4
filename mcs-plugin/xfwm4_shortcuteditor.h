/*  xfce4
 *  
 *  Copyright (C) 2003 Jasper Huijsmans (huysmans@users.sourceforge.net)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef __XFWM4_SHORTCUTEDITOR_H
#define __XFWM4_SHORTCUTEDITOR_H

extern void loadtheme_in_treeview (gchar *, gpointer);
extern void savetreeview_in_theme (gchar *, gpointer);

extern void cb_activate_treeview3 (GtkWidget *, GtkTreePath *,
				   GtkTreeViewColumn *, gpointer);
extern void cb_activate_treeview4 (GtkWidget *, GtkTreePath *,
				   GtkTreeViewColumn *, gpointer);
#endif
