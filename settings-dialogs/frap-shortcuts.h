/* vi:set expandtab sw=2 sts=2: */
/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __FRAP_SHORTCUTS_H__
#define __FRAP_SHORTCUTS_H__

#include <X11/Xlib.h>
#include <xfconf/xfconf.h>



typedef void (*FrapShortcutsCallback) (const gchar *shortcut,
                                       gpointer     user_data);



gboolean       frap_shortcuts_conflict_dialog       (const gchar           *owner,
                                                     const gchar           *other,
                                                     const gchar           *shortcut,
                                                     const gchar           *owner_action,
                                                     const gchar           *other_action,
                                                     gboolean               ignore_same_provider);
                                                     
gchar         *frap_shortcuts_get_accelerator_name  (guint                  keyval,
                                                     guint                  modifiers);
void           frap_shortcuts_parse_accelerator     (const gchar           *name,
                                                     guint                 *keyval,
                                                     guint                 *modifiers);

void           frap_shortcuts_set_shortcut_callback (FrapShortcutsCallback  callback,
                                                     gpointer               user_data);
void           frap_shortcuts_add_filter            (GdkFilterFunc          callback,
                                                     gpointer               user_data);
gboolean       frap_shortcuts_grab_shortcut         (const gchar           *shortcut,
                                                     gboolean               ungrab);
gboolean       frap_shortcuts_grab_shortcut_real    (Display               *display,
                                                     Window                 window,
                                                     KeyCode                keycode,
                                                     guint                  modifiers,
                                                     gboolean               ungrab);



#endif /* !__FRAP_SHORTCUTS_H__ */
