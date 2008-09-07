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

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include <gtk/gtk.h>

#include <dbus/dbus-glib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <xfconf/xfconf.h>

#include "frap-shortcuts.h"



typedef struct
{
  FrapShortcutsFunc callback;
  gpointer          user_data;
} FrapShortcutsCallbackContext;

typedef struct
{
  FrapShortcutsType owner_type;
  FrapShortcutsType other_type;
  const gchar      *message;
  const gchar      *owner_button_text;
  const gchar      *other_button_text;
} FrapShortcutsConflictMessage;



static guint           frap_shortcuts_x11_get_ignore_mask            (void);
static guint           frap_shortcuts_x11_get_use_mask               (void);
static guint           frap_shortcuts_gdk_remove_duplicate_modifiers (guint      modifiers);
static guint           frap_shortcuts_x11_add_gdk_modifiers          (guint      modifiers);
static guint           frap_shortcuts_gdk_add_x11_modifiers          (guint      modifiers);
#if 0
static void            frap_shortcuts_print_gdk_modifiers            (guint      modifiers);
#endif
static GdkFilterReturn frap_shortcuts_default_filter                 (GdkXEvent *gdk_xevent,
                                                                      GdkEvent  *event,
                                                                      gpointer   user_data);
static void            frap_shortcuts_handle_key_press               (XKeyEvent *xevent);



static FrapShortcutsCallbackContext frap_shortcuts_callback_context = { 0 };



static FrapShortcutsConflictMessage conflict_messages[] = {
  { FRAP_SHORTCUTS_XFWM4, FRAP_SHORTCUTS_XFWM4, 
    N_("This shortcut is already being used by the window manager action <b>%s</b>. Which action do you want to use?"), 
    N_("Use %s"), N_("Keep %s") },
  { FRAP_SHORTCUTS_XFWM4, FRAP_SHORTCUTS_EXECUTE,
    N_("The shortcut is already being used for the command <b>%s</b>. Which action do you want to use?"), 
    N_("Use %s"), N_("Keep %s") },
  { FRAP_SHORTCUTS_EXECUTE, FRAP_SHORTCUTS_EXECUTE,
    N_("The shortcut is already being used for the command <b>%s</b>. Which action do you want to use?"),
    N_("Use %s"), N_("Keep %s") },
  { FRAP_SHORTCUTS_EXECUTE, FRAP_SHORTCUTS_XFWM4,
    N_("The shortcut is already being used by a <b>window manager action</b>. Which action do you want to use?"),
    N_("Use %s"), N_("Keep window manager action") },
  { 0, 0, NULL, NULL, NULL },
};



XfconfChannel *
frap_shortcuts_get_channel (void)
{
  return xfconf_channel_new ("xfce4-keyboard-shortcuts");
}



gchar *
frap_shortcuts_get_property_name (const gchar *shortcut)
{
  return g_strdup_printf ("/%s", shortcut);
}



const gchar *
frap_shortcuts_get_type_name (FrapShortcutsType type)
{
  return type == FRAP_SHORTCUTS_XFWM4 ? "xfwm4" : "execute";
}



gboolean
frap_shortcuts_parse_value (const GValue      *value, 
                            FrapShortcutsType *type,
                            gchar            **action)
{
  const GPtrArray *array;
  const GValue    *type_value;
  const GValue    *action_value;

  g_return_val_if_fail (value != NULL, FALSE);

  if (G_VALUE_TYPE (value) != dbus_g_type_get_collection ("GPtrArray", G_TYPE_VALUE))
    return FALSE;

  array = g_value_get_boxed (value);

  if (array->len != 2)
    return FALSE;

  type_value = g_ptr_array_index (array, 0);
  action_value = g_ptr_array_index (array, 1);

  if (G_VALUE_TYPE (type_value) != G_TYPE_STRING || G_VALUE_TYPE (action_value) != G_TYPE_STRING)
    return FALSE;

  if (type != NULL)
    {
      if (g_utf8_collate (g_value_get_string (type_value), "execute") == 0)
        *type = FRAP_SHORTCUTS_EXECUTE;
      else if (g_utf8_collate (g_value_get_string (type_value), "xfwm4") == 0)
        *type = FRAP_SHORTCUTS_XFWM4;
      else
        *type = FRAP_SHORTCUTS_INVALID;
    }

  if (action != NULL)
    *action = g_strdup (g_value_get_string (action_value));

  return TRUE;
}



gboolean
frap_shortcuts_has_shortcut (XfconfChannel *channel,
                             const gchar   *shortcut)
{
  gboolean exists = FALSE;
  gchar   *property;

  g_return_val_if_fail (XFCONF_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (shortcut != NULL, FALSE);

  if (g_utf8_strlen (shortcut, -1) <= 0)
    return FALSE;

  property = frap_shortcuts_get_property_name (shortcut);
  exists = xfconf_channel_has_property (channel, property);
  g_free (property);

  return exists;
}



gboolean
frap_shortcuts_parse_shortcut (XfconfChannel     *channel,
                               const gchar       *shortcut,
                               FrapShortcutsType *type,
                               gchar            **action)
{
  GValue   value = { 0 };
  gboolean success = FALSE;
  gchar   *property;

  g_return_val_if_fail (XFCONF_IS_CHANNEL (channel), FALSE);
  g_return_val_if_fail (shortcut != NULL, FALSE);
  
  property = frap_shortcuts_get_property_name (shortcut);

  if (xfconf_channel_has_property (channel, property) && xfconf_channel_get_property (channel, property, &value))
    {
      if (frap_shortcuts_parse_value (&value, type, action))
        success = TRUE;

      if (G_IS_VALUE (&value))
        g_value_unset (&value);
    }

  g_free (property);

  return success;
}



gint
frap_shortcuts_conflict_dialog (XfconfChannel    *channel,
                                const gchar      *property,
                                const gchar      *action,
                                FrapShortcutsType type,
                                gboolean          ignore_same_type)
{
  FrapShortcutsType other_type;
  GValue            value = { 0 };
  gchar            *primary_text;
  gchar            *secondary_text;
  gchar            *owner_name;
  gchar            *other_name;
  gchar            *owner_button_text;
  gchar            *other_button_text;
  gchar            *other_action;
  gchar            *escaped_shortcut;
  gint              response = GTK_RESPONSE_ACCEPT;
  gint              i;

  if (xfconf_channel_get_property (channel, property, &value) && frap_shortcuts_parse_value (&value, &other_type, &other_action))
    {
      if ((type != other_type || action == NULL || g_utf8_collate (action, other_action) != 0) && !(type == other_type && ignore_same_type))
        {
          for (i = 0; conflict_messages[i].message != NULL; ++i)
            if (conflict_messages[i].owner_type == type && conflict_messages[i].other_type == other_type)
              {
                /* Build primary error message */
                escaped_shortcut = g_markup_escape_text (property + 1, -1);
                primary_text = g_strdup_printf (_("%s shortcut conflict"), escaped_shortcut);
                g_free (escaped_shortcut);

                owner_name = action == NULL ? NULL : g_markup_escape_text (action, -1);
                other_name = g_markup_escape_text (other_action, -1);

                secondary_text = g_strdup_printf (conflict_messages[i].message, other_name);

                owner_button_text = owner_name == NULL ? _("Use this action") : g_markup_printf_escaped (conflict_messages[i].owner_button_text, owner_name); 
                other_button_text = g_markup_printf_escaped (conflict_messages[i].other_button_text, other_name);

                response = xfce_message_dialog (NULL, _("Shortcut conflict"), GTK_STOCK_DIALOG_ERROR,
                                                primary_text, secondary_text,
                                                XFCE_CUSTOM_BUTTON, owner_button_text, GTK_RESPONSE_ACCEPT,
                                                XFCE_CUSTOM_BUTTON, other_button_text, GTK_RESPONSE_REJECT, 
                                                NULL);

                g_free (owner_button_text);
                g_free (other_button_text);
                g_free (secondary_text);
                g_free (primary_text);
                g_free (owner_name);
                g_free (other_name);

                break;
              }
        }

      g_free (other_action);
    }
  else
    {
      xfce_err (_("The shortcut '%s' is already being used for something else."), property + 1);
      response = GTK_RESPONSE_REJECT;
    }

  return response;
}



void
frap_shortcuts_set_shortcut (XfconfChannel    *channel,
                             const gchar      *shortcut,
                             const gchar      *action,
                             FrapShortcutsType type)
{
  const gchar *typename;
  gchar       *property;

  g_return_if_fail (XFCONF_IS_CHANNEL (channel));
  g_return_if_fail (shortcut != NULL);
  g_return_if_fail (action != NULL);

  if (g_utf8_strlen (shortcut, -1) <= 0 || g_utf8_strlen (action, -1) <= 0)
    return;

  property = frap_shortcuts_get_property_name (shortcut);
  typename = frap_shortcuts_get_type_name (type);

  xfconf_channel_set_array (channel, property, G_TYPE_STRING, typename, G_TYPE_STRING, action, G_TYPE_INVALID);

  g_free (property);
}



void
frap_shortcuts_remove_shortcut (XfconfChannel *channel,
                                const gchar   *shortcut)
{
  gchar *property;

  g_return_if_fail (XFCONF_IS_CHANNEL (channel));
  g_return_if_fail (shortcut != NULL);

  if (g_utf8_strlen (shortcut, -1) <= 0)
    return;

  property = frap_shortcuts_get_property_name (shortcut);

  if (xfconf_channel_has_property (channel, property))
    xfconf_channel_remove_property (channel, property);

  g_free (property);
}



gchar *
frap_shortcuts_get_accelerator_name (guint keyval,
                                     guint modifiers)
{
  modifiers = frap_shortcuts_x11_add_gdk_modifiers (modifiers);
  modifiers = frap_shortcuts_gdk_remove_duplicate_modifiers (modifiers);

  return gtk_accelerator_name (keyval, modifiers);
}



void
frap_shortcuts_parse_accelerator (const gchar *name,
                                  guint       *keyval,
                                  guint       *modifiers)
{
  gtk_accelerator_parse (name, keyval, modifiers);
}



void
frap_shortcuts_set_shortcut_callback (FrapShortcutsFunc callback,
                                      gpointer          user_data)
{
  frap_shortcuts_callback_context.callback = callback;
  frap_shortcuts_callback_context.user_data = user_data;

  frap_shortcuts_add_filter (frap_shortcuts_default_filter, NULL);
}



void
frap_shortcuts_add_filter (GdkFilterFunc callback,
                           gpointer      user_data)
{
  GdkDisplay *display;

  display = gdk_display_get_default ();

  /* Flush events before adding the event filter */
  XAllowEvents (GDK_DISPLAY_XDISPLAY (display), AsyncBoth, CurrentTime);

  /* Add event filter to the root window of each screen */
#if 0
  for (i = 0; i < screens; ++i)
    {
      screen = gdk_display_get_screen (display, i);
      gdk_window_add_filter (gdk_screen_get_root_window (screen), callback, helper);
    }
#else
  gdk_window_add_filter (NULL, callback, user_data);
#endif
}



gboolean
frap_shortcuts_grab_shortcut (const gchar *shortcut,
                              gboolean     ungrab)
{
  GdkDisplay *display;
  GdkScreen  *screen;
  KeyCode     keycode;
  guint       keyval;
  guint       modifiers;
  guint       bits[32];
  guint       current_mask;
  guint       ignore_mask;
  guint       use_mask;
  guint       n_bits;
  guint       screens;
  guint       i;
  guint       j;
  guint       k;

  display = gdk_display_get_default ();
  screens = gdk_display_get_n_screens (display);

  /* Parse the shortcut */
  frap_shortcuts_parse_accelerator (shortcut, &keyval, &modifiers);

  g_debug ("grab_shortcut: shortcut = %s, keyval = 0x%x, modifiers = 0x%x, ungrab = %i", shortcut, keyval, modifiers, ungrab);

  /* Determine mask containing ignored modifier bits */
  ignore_mask = frap_shortcuts_x11_get_ignore_mask ();
  use_mask = frap_shortcuts_x11_get_use_mask ();

  /* Mask used modifiers */
  modifiers = frap_shortcuts_gdk_add_x11_modifiers (modifiers);
  modifiers &= use_mask;

  /* Determine keycode */
  keycode = XKeysymToKeycode (GDK_DISPLAY_XDISPLAY (display), keyval);

  /* Store indices of all 1 bits of the ignore mask in an array */
  for (i = 0, n_bits = 0; i < 32; ++i, ignore_mask >>= 1)
    if ((ignore_mask & 0x1) == 0x1)
      bits[n_bits++] = i;

  for (i = 0; i < (1 << n_bits); ++i)
    {
      /* Map bits in the counter to those in the mask and thereby retrieve all ignored bit
       * mask combinations */
      for (current_mask = 0, j = 0; j < n_bits; ++j)
        if ((i & (1 << j)) != 0)
          current_mask |= (1 << bits[j]);

      /* Grab key on all screens */
      for (k = 0; k < screens; ++k)
        {
          /* Get current screen and root X window */
          screen = gdk_display_get_screen (display, k);
          
          /* Really grab or ungrab the key now */
          frap_shortcuts_grab_shortcut_real (GDK_DISPLAY_XDISPLAY (display), 
                                             GDK_WINDOW_XWINDOW (gdk_screen_get_root_window (screen)),
                                             keycode,
                                             current_mask | modifiers,
                                             ungrab);
        }
    }

  return TRUE;
}



gboolean
frap_shortcuts_grab_shortcut_real (Display *display,
                                   Window   window,
                                   KeyCode  keycode,
                                   guint    modifiers,
                                   gboolean ungrab)
{
  gdk_error_trap_push ();

  if (ungrab)
    XUngrabKey (display, keycode, modifiers, window);
  else
    XGrabKey (display, keycode, modifiers, window, FALSE, GrabModeAsync, GrabModeAsync);

  gdk_flush ();

  return gdk_error_trap_pop () == 0;
}



static guint 
frap_shortcuts_x11_get_ignore_mask (void)
{
  XModifierKeymap *modmap;
  const KeySym    *keysyms;
  Display         *display;
  KeyCode          keycode;
  KeySym          *keymap;
  guint            ignored_modifiers = 0;
  gint             keysyms_per_keycode = 0;
  gint             min_keycode = 0;
  gint             max_keycode = 0;
  gint             mask;
  gint             i;
  gint             j;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  gdk_error_trap_push ();

  XDisplayKeycodes (display, &min_keycode, &max_keycode);

  keymap = XGetKeyboardMapping (display, min_keycode, max_keycode - min_keycode + 1, &keysyms_per_keycode);

  if (keymap == NULL)
    return ignored_modifiers;

  modmap = XGetModifierMapping (display);

  if (modmap == NULL)
    {
      XFree (keymap);
      return ignored_modifiers;
    }

  for (i = 0; i < 8 * modmap->max_keypermod; ++i)
    {
      keycode = modmap->modifiermap[i];

      if (keycode == 0 || keycode < min_keycode || keycode > max_keycode)
        continue;

      keysyms = keymap + (keycode - min_keycode) * keysyms_per_keycode;

      mask = 1 << (i / modmap->max_keypermod);

      for (j = 0; j < keysyms_per_keycode; ++j)
        {
          if (keysyms[j] == GDK_Caps_Lock ||
              keysyms[j] == GDK_Num_Lock ||
              keysyms[j] == GDK_Scroll_Lock)
            {
              ignored_modifiers |= mask;
            }
        }
    }

  XFreeModifiermap (modmap);
  XFree (keymap);

  gdk_flush ();
  gdk_error_trap_pop ();

  return ignored_modifiers | 0x2000 | GDK_LOCK_MASK | GDK_HYPER_MASK | GDK_SUPER_MASK | GDK_META_MASK;
}



static guint 
frap_shortcuts_x11_get_use_mask (void)
{
  return GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK;
}



static guint
frap_shortcuts_gdk_remove_duplicate_modifiers (guint modifiers)
{
  XModifierKeymap *modmap;
  const KeySym    *keysyms;
  Display         *display;
  KeyCode          keycode;
  KeySym          *keymap;
  guint            modifiers_result = modifiers;
  gint             keysyms_per_keycode = 0;
  gint             min_keycode = 0;
  gint             max_keycode = 0;
  gint             mask;
  gint             i;
  gint             j;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  gdk_error_trap_push ();

  XDisplayKeycodes (display, &min_keycode, &max_keycode);

  keymap = XGetKeyboardMapping (display, min_keycode, max_keycode - min_keycode + 1, &keysyms_per_keycode);

  if (keymap == NULL)
    return modifiers_result;

  modmap = XGetModifierMapping (display);

  if (modmap == NULL)
    {
      XFree (keymap);
      return modifiers_result;
    }

  for (i = 0; i < 8 * modmap->max_keypermod; ++i)
    {
      keycode = modmap->modifiermap[i];

      if (keycode == 0 || keycode < min_keycode || keycode > max_keycode)
        continue;

      keysyms = keymap + (keycode - min_keycode) * keysyms_per_keycode;

      mask = 1 << (i / modmap->max_keypermod);

      for (j = 0; j < keysyms_per_keycode; ++j)
        {
          if (keysyms[j] == GDK_Caps_Lock ||
              keysyms[j] == GDK_Num_Lock ||
              keysyms[j] == GDK_Scroll_Lock ||
#if 0
              keysyms[j] == GDK_Hyper_L ||
              keysyms[j] == GDK_Hyper_R ||
#endif
              keysyms[j] == GDK_Meta_L ||
              keysyms[j] == GDK_Meta_R ||
              keysyms[j] == GDK_Super_L ||
              keysyms[j] == GDK_Super_R)
            {
              if ((modifiers & mask) == mask)
                modifiers_result &= ~mask;
            }
        }
    }

  XFreeModifiermap (modmap);
  XFree (keymap);

  gdk_flush ();
  gdk_error_trap_pop ();

  return modifiers_result;
}



static guint
frap_shortcuts_x11_add_gdk_modifiers (guint modifiers)
{
  XModifierKeymap *modmap;
  const KeySym    *keysyms;
  Display         *display;
  KeyCode          keycode;
  KeySym          *keymap;
  guint            modifiers_result = modifiers;
  gint             keysyms_per_keycode = 0;
  gint             min_keycode = 0;
  gint             max_keycode = 0;
  gint             mask;
  gint             i;
  gint             j;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  gdk_error_trap_push ();

  XDisplayKeycodes (display, &min_keycode, &max_keycode);

  keymap = XGetKeyboardMapping (display, min_keycode, max_keycode - min_keycode + 1, &keysyms_per_keycode);

  if (keymap == NULL)
    return modifiers_result;

  modmap = XGetModifierMapping (display);

  if (modmap == NULL)
    {
      XFree (keymap);
      return modifiers_result;
    }

  for (i = 0; i < 8 * modmap->max_keypermod; ++i)
    {
      keycode = modmap->modifiermap[i];

      if (keycode == 0 || keycode < min_keycode || keycode > max_keycode)
        continue;

      keysyms = keymap + (keycode - min_keycode) * keysyms_per_keycode;

      mask = 1 << (i / modmap->max_keypermod);

      for (j = 0; j < keysyms_per_keycode; ++j)
        {
          switch (keysyms[j])
            {
            case GDK_Super_L:
            case GDK_Super_R:
              if ((modifiers & mask) == mask)
                modifiers_result |= GDK_SUPER_MASK;
              break;

#if 0
            case GDK_Hyper_L:
            case GDK_Hyper_R:
              if ((modifiers & mask) == mask)
                modifiers_result |= GDK_HYPER_MASK;
              break;
#endif

            case GDK_Meta_L:
            case GDK_Meta_R:
              if ((modifiers & mask) == mask)
                modifiers_result |= GDK_META_MASK;
              break;

            default:
              break;
            }
        }
    }

  XFreeModifiermap (modmap);
  XFree (keymap);

  gdk_flush ();
  gdk_error_trap_pop ();

  return modifiers_result;
}



static guint
frap_shortcuts_gdk_add_x11_modifiers (guint modifiers)
{
  XModifierKeymap *modmap;
  const KeySym    *keysyms;
  Display         *display;
  KeyCode          keycode;
  KeySym          *keymap;
  guint            modifiers_result = modifiers;
  gint             keysyms_per_keycode = 0;
  gint             min_keycode = 0;
  gint             max_keycode = 0;
  gint             mask;
  gint             i;
  gint             j;

  display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

  gdk_error_trap_push ();

  XDisplayKeycodes (display, &min_keycode, &max_keycode);

  keymap = XGetKeyboardMapping (display, min_keycode, max_keycode - min_keycode + 1, &keysyms_per_keycode);

  if (keymap == NULL)
    return modifiers_result;

  modmap = XGetModifierMapping (display);

  if (modmap == NULL)
    {
      XFree (keymap);
      return modifiers_result;
    }

  for (i = 0; i < 8 * modmap->max_keypermod; ++i)
    {
      keycode = modmap->modifiermap[i];

      if (keycode == 0 || keycode < min_keycode || keycode > max_keycode)
        continue;

      keysyms = keymap + (keycode - min_keycode) * keysyms_per_keycode;

      mask = 1 << (i / modmap->max_keypermod);

      for (j = 0; j < keysyms_per_keycode; ++j)
        {
          switch (keysyms[j])
            {
            case GDK_Super_L:
            case GDK_Super_R:
              if ((modifiers & GDK_SUPER_MASK) == GDK_SUPER_MASK)
                modifiers_result |= mask;
              break;

#if 0
            case GDK_Hyper_L:
            case GDK_Hyper_R:
              if ((modifiers & GDK_HYPER_MASK) == GDK_HYPER_MASK)
                modifiers_result |= mask;
              break;
#endif

            case GDK_Meta_L:
            case GDK_Meta_R:
              if ((modifiers & GDK_META_MASK) == GDK_META_MASK)
                modifiers_result |= mask;
              break;

            default:
              break;
            }
        }
    }

  XFreeModifiermap (modmap);
  XFree (keymap);

  gdk_flush ();
  gdk_error_trap_pop ();

  return modifiers_result;
}



#if 0
static void
frap_shortcuts_print_gdk_modifiers (guint modifiers)
{
  GList *strings = NULL;
  gint   i;

  if ((modifiers & GDK_SHIFT_MASK) == GDK_SHIFT_MASK)
    strings = g_list_append (strings, " GDK_SHIFT_MASK");

  if ((modifiers & GDK_LOCK_MASK) == GDK_LOCK_MASK)
    strings = g_list_append (strings, " GDK_LOCK_MASK");

  if ((modifiers & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
    strings = g_list_append (strings, "GDK_CONTROL_MASK");

  if ((modifiers & GDK_MOD1_MASK) == GDK_MOD1_MASK)
    strings = g_list_append (strings, "GDK_MOD1_MASK");

  if ((modifiers & GDK_MOD2_MASK) == GDK_MOD2_MASK)
    strings = g_list_append (strings, "GDK_MOD2_MASK");

  if ((modifiers & GDK_MOD3_MASK) == GDK_MOD3_MASK)
    strings = g_list_append (strings, "GDK_MOD3_MASK");

  if ((modifiers & GDK_MOD4_MASK) == GDK_MOD4_MASK)
    strings = g_list_append (strings, "GDK_MOD4_MASK");

  if ((modifiers & GDK_MOD5_MASK) == GDK_MOD5_MASK)
    strings = g_list_append (strings, "GDK_MOD5_MASK");

  if ((modifiers & GDK_SUPER_MASK) == GDK_SUPER_MASK)
    strings = g_list_append (strings, "GDK_SUPER_MASK");

  if ((modifiers & GDK_HYPER_MASK) == GDK_HYPER_MASK)
    strings = g_list_append (strings, "GDK_HYPER_MASK");

  if ((modifiers & GDK_META_MASK) == GDK_META_MASK)
    strings = g_list_append (strings, "GDK_META_MASK");

  g_print ("MODIFIERS: ");

  for (i = 0; i < g_list_length (strings); ++i)
    if (i < g_list_length (strings)-1)
      g_print ("%s | ", (const gchar *) g_list_nth (strings, i)->data);
    else
      g_print ("%s\n", (const gchar *) g_list_nth (strings, i)->data);

  if (g_list_length (strings) == 0)
    g_print ("\n");

  g_list_free (strings);
}
#endif



static GdkFilterReturn 
frap_shortcuts_default_filter (GdkXEvent *gdk_xevent,
                               GdkEvent  *event,
                               gpointer   user_data)
{
  XEvent *xevent = (XEvent *) gdk_xevent;

  switch (xevent->type)
    {
    case KeyPress:
      frap_shortcuts_handle_key_press ((XKeyEvent *) xevent);
      break;
    default:
      break;
    }

  return GDK_FILTER_CONTINUE;
}



static void
frap_shortcuts_handle_key_press (XKeyEvent *xevent)
{
  GdkDisplay *display;
  KeySym      keysym;
  gchar      *shortcut;

  display = gdk_display_get_default ();

  /* FIXME: The first keysym might not be sufficient, maybe try them all and fire up the
   * callback for each of them */
  keysym = XKeycodeToKeysym (GDK_DISPLAY_XDISPLAY (display), xevent->keycode, 0);

  shortcut = frap_shortcuts_get_accelerator_name (keysym, xevent->state);
  
  if (frap_shortcuts_callback_context.callback != NULL)
    frap_shortcuts_callback_context.callback (shortcut, frap_shortcuts_callback_context.user_data);

  g_free (shortcut);
}
