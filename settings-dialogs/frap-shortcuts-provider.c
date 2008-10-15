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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <glib-object.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "frap-shortcuts-provider.h"



#define FRAP_SHORTCUTS_PROVIDER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), FRAP_TYPE_SHORTCUTS_PROVIDER, FrapShortcutsProviderPrivate))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_NAME,
};



typedef struct _FrapShortcutsProviderContext FrapShortcutsProviderContext;



static void frap_shortcuts_provider_class_init       (FrapShortcutsProviderClass *klass);
static void frap_shortcuts_provider_init             (FrapShortcutsProvider      *provider);
static void frap_shortcuts_provider_constructed      (GObject                    *object);
static void frap_shortcuts_provider_finalize         (GObject                    *object);
static void frap_shortcuts_provider_get_property     (GObject                    *object,
                                                      guint                       prop_id,
                                                      GValue                     *value,
                                                      GParamSpec                 *pspec);
static void frap_shortcuts_provider_set_property     (GObject                    *object,
                                                      guint                       prop_id,
                                                      const GValue               *value,
                                                      GParamSpec                 *pspec);
static void frap_shortcuts_provider_register         (FrapShortcutsProvider      *provider);
static void frap_shortcuts_provider_property_changed (XfconfChannel              *channel,
                                                      gchar                      *property,
                                                      GValue                     *value,
                                                      FrapShortcutsProvider      *provider);



struct _FrapShortcutsProviderPrivate
{
  XfconfChannel *channel;
  gchar         *name;
  gchar         *default_base_property;
  gchar         *custom_base_property;
};

struct _FrapShortcutsProviderContext
{
  FrapShortcutsProvider *provider;
  GList                 *list;
  const gchar           *base_property;
};



static GObjectClass *frap_shortcuts_provider_parent_class = NULL;



GType
frap_shortcuts_provider_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (FrapShortcutsProviderClass),
        NULL,
        NULL,
        (GClassInitFunc) frap_shortcuts_provider_class_init,
        NULL,
        NULL,
        sizeof (FrapShortcutsProvider),
        0,
        (GInstanceInitFunc) frap_shortcuts_provider_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "FrapShortcutsProvider", &info, 0);
    }

  return type;
}



static void
frap_shortcuts_provider_class_init (FrapShortcutsProviderClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (FrapShortcutsProviderPrivate));

  /* Determine the parent type class */
  frap_shortcuts_provider_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
#if GLIB_CHECK_VERSION (2,14,0)
  gobject_class->constructed = frap_shortcuts_provider_constructed;
#endif
  gobject_class->finalize = frap_shortcuts_provider_finalize; 
  gobject_class->get_property = frap_shortcuts_provider_get_property;
  gobject_class->set_property = frap_shortcuts_provider_set_property;

  g_object_class_install_property (gobject_class, 
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "name",
                                                        "name",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_signal_new ("shortcut-removed", 
                FRAP_TYPE_SHORTCUTS_PROVIDER,
                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__STRING,
                G_TYPE_NONE,
                1,
                G_TYPE_STRING);

  g_signal_new ("shortcut-added", 
                FRAP_TYPE_SHORTCUTS_PROVIDER,
                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__STRING,
                G_TYPE_NONE,
                1,
                G_TYPE_STRING);
}



static void
frap_shortcuts_provider_init (FrapShortcutsProvider *provider)
{
  provider->priv = FRAP_SHORTCUTS_PROVIDER_GET_PRIVATE (provider);

  provider->priv->channel = xfconf_channel_new ("xfce4-keyboard-shortcuts");

  g_signal_connect (provider->priv->channel, "property-changed", 
                    G_CALLBACK (frap_shortcuts_provider_property_changed), provider);
}



static void
frap_shortcuts_provider_constructed (GObject *object)
{
  FrapShortcutsProvider *provider = FRAP_SHORTCUTS_PROVIDER (object);

  frap_shortcuts_provider_register (provider);

  provider->priv->default_base_property = g_strdup_printf ("/%s/default", provider->priv->name);
  provider->priv->custom_base_property = g_strdup_printf ("/%s/custom", provider->priv->name);

  if (!frap_shortcuts_provider_is_custom (provider))
    frap_shortcuts_provider_reset_to_defaults (provider);
}



static void
frap_shortcuts_provider_finalize (GObject *object)
{
  FrapShortcutsProvider *provider = FRAP_SHORTCUTS_PROVIDER (object);

  g_free (provider->priv->name);
  g_free (provider->priv->custom_base_property);
  g_free (provider->priv->default_base_property);

  g_object_unref (provider->priv->channel);

  (*G_OBJECT_CLASS (frap_shortcuts_provider_parent_class)->finalize) (object);
}



static void
frap_shortcuts_provider_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  FrapShortcutsProvider *provider = FRAP_SHORTCUTS_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, provider->priv->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
frap_shortcuts_provider_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  FrapShortcutsProvider *provider = FRAP_SHORTCUTS_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_free (provider->priv->name);
      provider->priv->name = g_strdup (g_value_get_string (value));
      g_object_notify (object, "name");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
frap_shortcuts_provider_register (FrapShortcutsProvider *provider)
{
  gchar       **provider_names;
  const gchar **names;
  gboolean      already_registered = FALSE;
  gint          length;
  gint          i;

  g_return_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider));

  provider_names = xfconf_channel_get_string_list (provider->priv->channel, "/providers");

  for (i = 0; provider_names != NULL && provider_names[i] != NULL; ++i)
    if (G_UNLIKELY (g_str_equal (provider_names[i], frap_shortcuts_provider_get_name (provider))))
      {
        already_registered = TRUE;
        break;
      }

  length = i;

  if (G_UNLIKELY (!already_registered))
    {
      names = g_new0 (const gchar *, length + 1);
      for (i = 0; provider_names != NULL && provider_names[i] != NULL; ++i)
        names[i] = provider_names[i];
      names[i++] = frap_shortcuts_provider_get_name (provider);
      names[i] = NULL;

      xfconf_channel_set_string_list (provider->priv->channel, "/providers", names);

      g_free (names);
    }

  g_strfreev (provider_names);
}



static void 
frap_shortcuts_provider_property_changed (XfconfChannel         *channel,
                                          gchar                 *property,
                                          GValue                *value,
                                          FrapShortcutsProvider *provider)
{
  const gchar *shortcut;
  gchar       *override_property;

  g_return_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider));

  DBG ("property = %s", property);

  if (!g_str_has_prefix (property, provider->priv->custom_base_property))
    return;

  override_property = g_strconcat (provider->priv->custom_base_property, "/override", NULL);

  if (G_UNLIKELY (g_utf8_collate (property, override_property) == 0))
    {
      g_free (override_property);
      return;
    }
  g_free (override_property);

  shortcut = property + strlen (provider->priv->custom_base_property) + strlen ("/");

  if (G_VALUE_TYPE (value) != G_TYPE_INVALID)
    g_signal_emit_by_name (provider, "shortcut-added", shortcut);
  else
    g_signal_emit_by_name (provider, "shortcut-removed", shortcut);
}



FrapShortcutsProvider *
frap_shortcuts_provider_new (const gchar *name)
{
  GObject *object = g_object_new (FRAP_TYPE_SHORTCUTS_PROVIDER, "name", name, NULL);

#if !GLIB_CHECK_VERSION (2,14,0)
  frap_shortcuts_provider_constructed (object);
#endif

  return FRAP_SHORTCUTS_PROVIDER (object);
}



GList *
frap_shortcuts_provider_get_providers (void)
{
  GList         *providers = NULL;
  XfconfChannel *channel;
  gchar        **names;
  gint           i;

  channel = xfconf_channel_get ("xfce4-keyboard-shortcuts");
  names = xfconf_channel_get_string_list (channel, "/providers");

  if (G_LIKELY (names != NULL))
    {
      for (i = 0; names[i] != NULL; ++i)
        providers = g_list_append (providers, frap_shortcuts_provider_new (names[i]));
      g_strfreev (names);
    }
    
  return providers;
}



void
frap_shortcuts_provider_free_providers (GList *providers)
{
  GList *iter;

  for (iter = g_list_first (providers); iter != NULL; iter = g_list_next (iter))
    g_object_unref (iter->data);

  g_list_free (providers);
}



const gchar *
frap_shortcuts_provider_get_name (FrapShortcutsProvider *provider)
{
  g_return_val_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider), NULL);
  return provider->priv->name;
}



gboolean
frap_shortcuts_provider_is_custom (FrapShortcutsProvider *provider)
{
  gchar   *property;
  gboolean override;

  g_return_val_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider), FALSE);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel), FALSE);

  property = g_strconcat (provider->priv->custom_base_property, "/override", NULL);
  override = xfconf_channel_get_bool (provider->priv->channel, property, FALSE);
  g_free (property);

  return override;
}



void
frap_shortcuts_provider_reset_to_defaults (FrapShortcutsProvider *provider)
{
  g_return_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider));
  g_return_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel));

  DBG ("property = %s", provider->priv->custom_base_property);

  xfconf_channel_reset_property (provider->priv->channel, provider->priv->custom_base_property, TRUE);
  frap_shortcuts_provider_clone_defaults (provider);
}



static gboolean
_frap_shortcuts_provider_clone_default (const gchar           *property,
                                        const GValue          *value,
                                        FrapShortcutsProvider *provider)
{
  const gchar *shortcut;
  const gchar *command;
  gchar       *custom_property;

  g_return_val_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider), TRUE);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel), TRUE);

  if (G_UNLIKELY (!G_IS_VALUE (value) || G_VALUE_TYPE (value) != G_TYPE_STRING))
    return FALSE;

  shortcut = property + strlen (provider->priv->default_base_property) + strlen ("/");
  command = g_value_get_string (value);

  DBG ("shortcut = %s, command = %s", shortcut, command);

  custom_property = g_strconcat (provider->priv->custom_base_property, "/", shortcut, NULL);
  xfconf_channel_set_string (provider->priv->channel, custom_property, command);
  g_free (custom_property);

  return FALSE;
}



void
frap_shortcuts_provider_clone_defaults (FrapShortcutsProvider *provider)
{
  GHashTable *properties;
  gchar      *property;

  g_return_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider));
  g_return_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel));

  /* Get default command shortcuts */
  properties = xfconf_channel_get_properties (provider->priv->channel, provider->priv->default_base_property);

  if (G_LIKELY (properties != NULL))
    {
      /* Copy from /commands/default to /commands/custom property by property */
      g_hash_table_foreach (properties, (GHFunc) _frap_shortcuts_provider_clone_default, provider);

      g_hash_table_destroy (properties);
    }

  DBG ("adding override property");

  /* Add the override property */
  property = g_strconcat (provider->priv->custom_base_property, "/override", NULL);
  xfconf_channel_set_bool (provider->priv->channel, property, TRUE);
  g_free (property);
}



static gboolean
_frap_shortcuts_provider_get_shortcut (const gchar                  *property,
                                       const GValue                 *value,
                                       FrapShortcutsProviderContext *context)
{
  FrapShortcut *sc;
  const gchar  *shortcut;
  const gchar  *command;

  g_return_val_if_fail (context != NULL, TRUE);
  g_return_val_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (context->provider), TRUE);

  if (G_VALUE_TYPE (value) != G_TYPE_STRING)
    return FALSE;

  if (!g_str_has_prefix (property, context->provider->priv->custom_base_property))
    return FALSE;
  
  shortcut = property + strlen (context->provider->priv->custom_base_property) + strlen ("/");
  command = g_value_get_string (value);

  if (G_LIKELY (shortcut != NULL && command != NULL && g_utf8_strlen (shortcut, -1) > 0 && g_utf8_strlen (command, -1) > 0))
    {
      sc = g_new0 (FrapShortcut, 1);

      sc->property_name = g_strdup (property);
      sc->shortcut = g_strdup (shortcut);
      sc->command = g_strdup (command);

      context->list = g_list_append (context->list, sc);
    }

  return FALSE;
}



GList *
frap_shortcuts_provider_get_shortcuts (FrapShortcutsProvider *provider)
{
  FrapShortcutsProviderContext context;
  GHashTable                  *properties;

  g_return_val_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider), NULL);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel), NULL);

  properties = xfconf_channel_get_properties (provider->priv->channel, provider->priv->custom_base_property);

  context.provider = provider;
  context.list = NULL;

  if (G_LIKELY (properties != NULL))
    g_hash_table_foreach (properties, (GHFunc) _frap_shortcuts_provider_get_shortcut, &context);

  return context.list;
}



FrapShortcut *
frap_shortcuts_provider_get_shortcut (FrapShortcutsProvider *provider,
                                      const gchar           *shortcut)
{
  FrapShortcut *sc = NULL;
  gchar        *base_property;
  gchar        *property;
  gchar        *command;

  g_return_val_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider), NULL);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel), NULL);

  if (G_LIKELY (frap_shortcuts_provider_is_custom (provider)))
    base_property = provider->priv->custom_base_property;
  else
    base_property = provider->priv->default_base_property;

  property = g_strconcat (base_property, "/", shortcut, NULL);
  command = xfconf_channel_get_string (provider->priv->channel, property, NULL);

  if (G_LIKELY (command != NULL))
    {
      sc = g_new0 (FrapShortcut, 1);
      sc->command = command;
      sc->property_name = g_strdup (property);
      sc->shortcut = g_strdup (shortcut);
    }

  g_free (property);

  return sc;
}



gboolean
frap_shortcuts_provider_has_shortcut (FrapShortcutsProvider *provider,
                                      const gchar           *shortcut)
{
  gchar   *base_property;
  gchar   *property;
  gboolean has_property;

  g_return_val_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider), FALSE);
  g_return_val_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel), FALSE);
  
  if (G_LIKELY (frap_shortcuts_provider_is_custom (provider)))
    base_property = provider->priv->custom_base_property;
  else
    base_property = provider->priv->default_base_property;

  property = g_strconcat (base_property, "/", shortcut, NULL);
  has_property = xfconf_channel_has_property (provider->priv->channel, property);
  g_free (property);

  return has_property;
}



void
frap_shortcuts_provider_set_shortcut (FrapShortcutsProvider *provider,
                                      const gchar           *shortcut,
                                      const gchar           *command)
{
  gchar *property;

  g_return_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider));
  g_return_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel));
  g_return_if_fail (shortcut != NULL && command != NULL);

  /* Only allow custom shortcuts to be changed */
  if (G_UNLIKELY (!frap_shortcuts_provider_is_custom (provider)))
    return;
    
  property = g_strconcat (provider->priv->custom_base_property, "/", shortcut, NULL);

  if (xfconf_channel_has_property (provider->priv->channel, property))
    xfconf_channel_reset_property (provider->priv->channel, property, FALSE);

  xfconf_channel_set_string (provider->priv->channel, property, command);

  g_free (property);
}



void
frap_shortcuts_provider_reset_shortcut (FrapShortcutsProvider *provider,
                                        const gchar           *shortcut)
{
  gchar *property;

  g_return_if_fail (FRAP_IS_SHORTCUTS_PROVIDER (provider));
  g_return_if_fail (XFCONF_IS_CHANNEL (provider->priv->channel));
  g_return_if_fail (shortcut != NULL);

  property = g_strconcat (provider->priv->custom_base_property, "/", shortcut, NULL);

  DBG ("property = %s", property);

  xfconf_channel_reset_property (provider->priv->channel, property, FALSE);
  g_free (property);
}



void
frap_shortcuts_free_shortcuts (GList *shortcuts)
{
  g_list_foreach (shortcuts, (GFunc) frap_shortcuts_free_shortcut, NULL);
}



void
frap_shortcuts_free_shortcut (FrapShortcut *shortcut)
{
  if (G_UNLIKELY (shortcut == NULL))
    return;

  g_free (shortcut->property_name);
  g_free (shortcut->shortcut);
  g_free (shortcut->command);
  g_free (shortcut);
}
