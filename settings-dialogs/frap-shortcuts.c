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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include <X11/Xlib.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <xfconf/xfconf.h>

#include "frap-shortcuts.h"



typedef struct
{
  const gchar      *owner_name;
  const gchar      *other_name;
  const gchar      *message;
  const gchar      *owner_button_text;
  const gchar      *other_button_text;
} FrapShortcutsConflictMessage;



static FrapShortcutsConflictMessage conflict_messages[] = {
  { "xfwm4", "xfwm4", 
    N_("This shortcut is already being used by <b>another window manager action</b>. Which action do you want to use?"), 
    N_("Use %s"), N_("Keep the other one") },
  { "xfwm4", "commands",
    N_("The shortcut is already being used for the command <b>%s</b>. Which action do you want to use?"), 
    N_("Use %s"), N_("Keep %s") },
  { "commands","commands",
    N_("The shortcut is already being used for the command <b>%s</b>. Which action do you want to use?"),
    N_("Use %s"), N_("Keep %s") },
  { "commands", "xfwm4",
    N_("The shortcut is already being used by a <b>window manager action</b>. Which action do you want to use?"),
    N_("Use %s"), N_("Keep the window manager action") },
  { 0, 0, NULL, NULL, NULL },
};



gint
frap_shortcuts_conflict_dialog (const gchar *owner,
                                const gchar *other,
                                const gchar *shortcut,
                                const gchar *owner_action,
                                const gchar *other_action,
                                gboolean     ignore_same_provider)
{
  gchar   *title;
  gchar   *primary_text;
  gchar   *secondary_text;
  gchar   *owner_action_name;
  gchar   *other_action_name;
  gchar   *owner_button_text;
  gchar   *other_button_text;
  gchar   *escaped_shortcut;
  gboolean handled = FALSE;
  gint     response = GTK_RESPONSE_ACCEPT;
  gint     i;

  if (g_utf8_collate (owner, other) == 0 && ignore_same_provider)
    return GTK_RESPONSE_ACCEPT;

  if (g_utf8_collate (owner, other) == 0 && g_utf8_collate (owner_action, other_action) == 0)
    return GTK_RESPONSE_ACCEPT;

  title = g_strdup_printf (_("Conflicting actions for %s"), shortcut);

  for (i = 0; conflict_messages[i].message != NULL; ++i)
    if (g_utf8_collate (conflict_messages[i].owner_name, owner) == 0 &&
        g_utf8_collate (conflict_messages[i].other_name, other) == 0)
      {
        escaped_shortcut = g_markup_escape_text (shortcut, -1);
        primary_text = g_strdup_printf (_("Conflicting actions for %s"), escaped_shortcut);
        g_free (escaped_shortcut);

        owner_action_name = owner_action == NULL ? NULL : g_markup_escape_text (owner_action, -1);
        other_action_name = other_action == NULL ? NULL : g_markup_escape_text (other_action, -1);

        secondary_text = g_strdup_printf (conflict_messages[i].message, other_action_name);

        owner_button_text = g_markup_printf_escaped (conflict_messages[i].owner_button_text, owner_action_name);
        other_button_text = g_markup_printf_escaped (conflict_messages[i].other_button_text, other_action_name);

        response = xfce_message_dialog (NULL, title, GTK_STOCK_DIALOG_QUESTION,
                                        primary_text, secondary_text,
                                        XFCE_CUSTOM_BUTTON, owner_button_text, GTK_RESPONSE_ACCEPT,
                                        XFCE_CUSTOM_BUTTON, other_button_text, GTK_RESPONSE_REJECT,
                                        NULL);

        g_free (other_button_text);
        g_free (owner_button_text);
        g_free (secondary_text);
        g_free (other_action_name);
        g_free (owner_action_name);
        g_free (primary_text);
        g_free (title);

        handled = TRUE;
        break;
      }

  if (G_UNLIKELY (!handled))
    {
      xfce_message_dialog (NULL, title, GTK_STOCK_DIALOG_ERROR,
                           primary_text, _("The shortcut is already being used for something else."),
                           GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
      response = GTK_RESPONSE_REJECT;
    }

  return response;
}
