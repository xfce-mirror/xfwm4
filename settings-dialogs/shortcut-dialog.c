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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "shortcut-dialog.h"



static void     shortcut_dialog_class_init       (ShortcutDialogClass *klass);
static void     shortcut_dialog_init             (ShortcutDialog      *dialog);
static void     shortcut_dialog_dispose          (GObject             *object);
static void     shortcut_dialog_finalize         (GObject             *object);
static void     shortcut_dialog_create_contents  (ShortcutDialog      *dialog,
                                                  const gchar         *action);
static gboolean shortcut_dialog_key_released     (ShortcutDialog      *dialog,
                                                  GdkEventKey         *event);
static gboolean get_modmap_masks                 (Display             *display,
                                                  gint                *caps_lock_mask,
                                                  gint                *num_lock_mask,
                                                  gint                *scroll_lock_mask);
static gint     get_ignore_mask                  (Display             *display);



struct _ShortcutDialogClass
{
  GtkDialogClass __parent__;

  gboolean (*validate_shortcut) (ShortcutDialog *dialog,
                                 const gchar    *shortcut,
                                 gpointer        user_data);

  gint validate_shortcut_signal;
};

struct _ShortcutDialog
{
  GtkDialog __parent__;

  gchar    *shortcut;
};



static GObjectClass *shortcut_dialog_parent_class = NULL;



GType
shortcut_dialog_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info = 
        {
          sizeof (ShortcutDialogClass),
          NULL,
          NULL,
          (GClassInitFunc) shortcut_dialog_class_init,
          NULL,
          NULL,
          sizeof (ShortcutDialog),
          0,
          (GInstanceInitFunc) shortcut_dialog_init,
          NULL,
        };

      type = g_type_register_static (GTK_TYPE_DIALOG, "ShortcutDialog", &info, 0);
    }
  
  return type;
}



/**
 * Taken from GTK+ (_gtk_marshal_BOOLEAN__SRING). Credits go out to the
 * GTK+ devs for this.
 */
void
marshal_BOOLEAN__STRING (GClosure     *closure,
                         GValue       *return_value G_GNUC_UNUSED,
                         guint         n_param_values,
                         const GValue *param_values,
                         gpointer      invocation_hint G_GNUC_UNUSED,
                         gpointer      marshal_data)
{
  typedef gboolean (*GMarshalFunc_BOOLEAN__STRING) (gpointer     data1,
                                                    gpointer     arg_1,
                                                    gpointer     data2);
  register GMarshalFunc_BOOLEAN__STRING callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;
  gboolean v_return;

  g_return_if_fail (return_value != NULL);
  g_return_if_fail (n_param_values == 2);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }

  callback = (GMarshalFunc_BOOLEAN__STRING) (marshal_data ? marshal_data : cc->callback);

  #define g_marshal_value_peek_string(v) (char*) g_value_get_string (v)
  v_return = callback (data1, g_marshal_value_peek_string (param_values + 1), data2);

  g_value_set_boolean (return_value, v_return);
}




static void
shortcut_dialog_class_init (ShortcutDialogClass *klass)
{
  GObjectClass *gobject_class;

  /* Determine parent type class */
  shortcut_dialog_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = shortcut_dialog_dispose;
  gobject_class->finalize = shortcut_dialog_finalize;

  klass->validate_shortcut = NULL; 

  /* Create 'validate-shortcut' signal */
  klass->validate_shortcut_signal = g_signal_new ("validate-shortcut",
                                                  G_TYPE_FROM_CLASS (klass),
                                                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                                  G_STRUCT_OFFSET (ShortcutDialogClass, validate_shortcut),
                                                  NULL,
                                                  NULL,
                                                  marshal_BOOLEAN__STRING,
                                                  G_TYPE_BOOLEAN,
                                                  1,
                                                  G_TYPE_STRING);
}



static void
shortcut_dialog_init (ShortcutDialog *dialog)
{
  dialog->shortcut = NULL;
}



static void
shortcut_dialog_dispose (GObject *object)
{
  (*G_OBJECT_CLASS (shortcut_dialog_parent_class)->dispose) (object);
}



static void
shortcut_dialog_finalize (GObject *object)
{
  ShortcutDialog *dialog = SHORTCUT_DIALOG (object);

  g_free (dialog->shortcut);

  (*G_OBJECT_CLASS (shortcut_dialog_parent_class)->finalize) (object);
}



GtkWidget*
shortcut_dialog_new (const gchar *action)
{
  ShortcutDialog *dialog;
  
  dialog = SHORTCUT_DIALOG (g_object_new (TYPE_SHORTCUT_DIALOG, NULL));

  shortcut_dialog_create_contents (dialog, action);

  return GTK_WIDGET (dialog);
}



static void
shortcut_dialog_create_contents (ShortcutDialog *dialog,
                                 const gchar    *action)
{
  GtkWidget *button;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *image;
  gchar     *text;

  /* Set dialog title */
  gtk_window_set_title (GTK_WINDOW (dialog), _("Set shortcut"));

  /* Configure dialog */
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

  /* Create cancel button */
  button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_CANCEL);
  gtk_widget_show (button);

  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_icon_name ("input-keyboard", GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);
  gtk_widget_show (image);

  text = g_markup_printf_escaped ("%s\n<b>%s</b>", _("Set shortcut for action:"), action);

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), text);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  g_free (text);

  /* Connect to key release signal for determining the new shortcut */
  g_signal_connect_swapped (dialog, "key-release-event", G_CALLBACK (shortcut_dialog_key_released), dialog);
}



gint
shortcut_dialog_run (ShortcutDialog *dialog,
                     GtkWidget      *parent)
{
  gint response = GTK_RESPONSE_CANCEL;

  g_return_val_if_fail (IS_SHORTCUT_DIALOG (dialog), GTK_RESPONSE_CANCEL);

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (gtk_widget_get_toplevel (parent)));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  /* Take control on the keyboard */
  if (G_LIKELY (gdk_keyboard_grab (gtk_widget_get_root_window (parent), TRUE, GDK_CURRENT_TIME) == GDK_GRAB_SUCCESS))
    {
      /* Run the dialog and wait for the user to enter a valid shortcut */
      response = gtk_dialog_run (GTK_DIALOG (dialog));

      /* Clear shortcut if requested by the user */
      if (G_UNLIKELY (response == GTK_RESPONSE_NO))
        {
          g_free (dialog->shortcut);
          dialog->shortcut = g_strdup ("");
        }

      /* Release keyboard */
      gdk_keyboard_ungrab (GDK_CURRENT_TIME);
    }
  else
    g_warning ("%s", _("Could not grab the keyboard."));

  /* Return the response ID */
  return response;
}



static gboolean 
shortcut_dialog_key_released (ShortcutDialog *dialog,
                              GdkEventKey    *event)
{
  gboolean event_handled = FALSE;
  gboolean shortcut_accepted = FALSE;
  gchar   *shortcut;
  gint     modifiers;
  gint     ignore_mask;

  /* Determine mask of ignored modifiers */
  ignore_mask = get_ignore_mask (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));

  /* Strip ignored modifiers from the event mask */
  modifiers = event->state & ~ignore_mask;

  /* Get GTK+ accelerator string */
  shortcut = gtk_accelerator_name (event->keyval, modifiers);

  /* Let 'validate-shortcut' listeners decide whether this shortcut is ok or not */
  g_signal_emit_by_name (dialog, "validate-shortcut", shortcut, &shortcut_accepted);

  /* Check if the shortcut was accepted */
  if (G_LIKELY (shortcut_accepted))
    {
      /* Replace old shortcut */
      g_free (dialog->shortcut);
      dialog->shortcut = shortcut;

      /* Release keyboard */
      gdk_keyboard_ungrab (GDK_CURRENT_TIME);

      /* Exit dialog with positive response */
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
    }
  else
    {
      /* Free shortcut string */
      g_free (shortcut);
    }

  return event_handled;
}



const gchar*
shortcut_dialog_get_shortcut (ShortcutDialog *dialog)
{
  g_return_val_if_fail (IS_SHORTCUT_DIALOG (dialog), NULL);
  return dialog->shortcut;
}



/* The following code appears again in xfce4-settings-helper/keyboard-shortcuts-helper.c: */

/* Modifiers to be ignored (0x2000 is an Xkb modifier) */
#define IGNORED_MODIFIERS (0x2000 | GDK_LOCK_MASK | GDK_HYPER_MASK | GDK_SUPER_MASK | GDK_META_MASK)

/* Modifiers to be used */
#define USED_MODIFIERS (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD2_MASK | \
                        GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK)



static gboolean
get_modmap_masks (Display *display,
                  gint    *caps_lock_mask,
                  gint    *num_lock_mask,
                  gint    *scroll_lock_mask)
{
  XModifierKeymap *modmap;
  const KeySym    *keysyms;
  KeyCode          keycode;
  KeySym          *keymap;
  gint             keysyms_per_keycode = 0;
  gint             min_keycode = 0;
  gint             max_keycode = 0;
  gint             mask;
  gint             i;
  gint             j;

  /* Clear the masks */
  *caps_lock_mask = 0;
  *num_lock_mask = 0;
  *scroll_lock_mask = 0;

  gdk_error_trap_push ();

  /* Determine minimum and maximum number of keycodes */
  XDisplayKeycodes (display, &min_keycode, &max_keycode);

  /* Determine symbols for all keycodes */
  keymap = XGetKeyboardMapping (display, min_keycode, max_keycode - min_keycode + 1, &keysyms_per_keycode);

  if (G_UNLIKELY (keymap == NULL))
    return FALSE;

  /* Determine modifier mappign */
  modmap = XGetModifierMapping (display);

  if (G_UNLIKELY (modmap == NULL))
    {
      XFree (keymap);
      return FALSE;
    }

  /* Iterate over all modifier keycodes */
  for (i = 0; i < 8 * modmap->max_keypermod; ++i)
    {
      /* Get the current keycode */
      keycode = modmap->modifiermap[i];

      /* Ignore invalid codes */
      if (keycode == 0 || keycode < min_keycode || keycode > max_keycode)
        continue;

      /* Determine all keysyms for the current keycode */
      keysyms = keymap + (keycode - min_keycode) * keysyms_per_keycode;

      /* Create modifier mask for the current modifier */
      mask = 1 << (i / modmap->max_keypermod);

      /* Iterate over all keysyms of the current keycode and modify the resulting 
       * masks according to which keysyms belong to which X modifiers */
      for (j = 0; j < keysyms_per_keycode; ++j)
        {
          if (keysyms[j] == GDK_Caps_Lock) 
            *caps_lock_mask |= mask;
          else if (keysyms[j] == GDK_Num_Lock)
            *num_lock_mask |= mask;
          else if (keysyms[j] == GDK_Scroll_Lock)
            *scroll_lock_mask |= mask;
        }
    }
  
  /* Free mappings */
  XFreeModifiermap (modmap);
  XFree (keymap);

  gdk_flush ();
  gdk_error_trap_pop ();

  return TRUE;
}



static gint
get_ignore_mask (Display *display)
{
  gint caps_lock_mask;
  gint num_lock_mask;
  gint scroll_lock_mask;
  gint ignore_mask;

  ignore_mask = IGNORED_MODIFIERS & GDK_MODIFIER_MASK;

  if (G_LIKELY (get_modmap_masks (display, &caps_lock_mask, &num_lock_mask, &scroll_lock_mask)))
    ignore_mask |= caps_lock_mask | num_lock_mask | scroll_lock_mask;

  return ignore_mask;
}

/* End of the duplicated code */
