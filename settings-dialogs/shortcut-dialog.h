/* $Id$ */
/* vi:set sw=2 sts=2 ts=2 et ai: */
/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or (at 
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 * MA  02111-1307  USA
 */

#ifndef __SHORTCUT_DIALOG_H__
#define __SHORTCUT_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _ShortcutDialogClass ShortcutDialogClass;
typedef struct _ShortcutDialog      ShortcutDialog;

#define TYPE_SHORTCUT_DIALOG            (shortcut_dialog_get_type ())
#define SHORTCUT_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SHORTCUT_DIALOG, ShortcutDialog))
#define SHORTCUT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SHORTCUT_DIALOG, ShortcutDialogClass))
#define IS_SHORTCUT_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SHORTCUT_DIALOG))
#define IS_SHORTCUT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_SHORTCUT_DIALOG))
#define SHORTCUT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_SHORTCUT_DIALOG, ShortcutDialogClass))

GType        shortcut_dialog_get_type     (void) G_GNUC_CONST;

GtkWidget   *shortcut_dialog_new          (const gchar    *action);
gint         shortcut_dialog_run          (ShortcutDialog *dialog,
                                           GtkWidget      *parent);
const gchar *shortcut_dialog_get_shortcut (ShortcutDialog *dialog);

G_END_DECLS;

#endif /* !__SHORTCUT_DIALOG_H__ */
