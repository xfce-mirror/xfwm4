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

#ifndef __FRAP_SHORTCUTS_DIALOG_H__
#define __FRAP_SHORTCUTS_DIALOG_H__

#include <gtk/gtk.h>

#include "frap-shortcuts.h"

G_BEGIN_DECLS;

typedef struct _FrapShortcutsDialogClass FrapShortcutsDialogClass;
typedef struct _FrapShortcutsDialog      FrapShortcutsDialog;

#define FRAP_TYPE_SHORTCUTS_DIALOG            (frap_shortcuts_dialog_get_type ())
#define FRAP_SHORTCUTS_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_SHORTCUTS_DIALOG, FrapShortcutsDialog))
#define FRAP_SHORTCUTS_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_SHORTCUTS_DIALOG, FrapShortcutsDialogClass))
#define FRAP_IS_SHORTCUTS_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_SHORTCUTS_DIALOG))
#define FRAP_IS_SHORTCUTS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FRAP_TYPE_SHORTCUTS_DIALOG))
#define FRAP_SHORTCUTS_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_SHORTCUTS_DIALOG, FrapShortcutsDialogClass))

GType        frap_shortcuts_dialog_get_type     (void) G_GNUC_CONST;

GtkWidget   *frap_shortcuts_dialog_new          (FrapShortcutsType    type,
                                                 const gchar         *action);
gint         frap_shortcuts_dialog_run          (FrapShortcutsDialog *dialog);
const gchar *frap_shortcuts_dialog_get_shortcut (FrapShortcutsDialog *dialog);

G_END_DECLS;

#endif /* !__FRAP_SHORTCUTS_DIALOG_H__ */
