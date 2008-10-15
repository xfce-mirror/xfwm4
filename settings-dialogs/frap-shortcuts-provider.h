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

#ifndef __FRAP_SHORTCUTS_PROVIDER_H__
#define __FRAP_SHORTCUTS_PROVIDER_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _FrapShortcut                 FrapShortcut;

typedef struct _FrapShortcutsProviderPrivate FrapShortcutsProviderPrivate;
typedef struct _FrapShortcutsProviderClass   FrapShortcutsProviderClass;
typedef struct _FrapShortcutsProvider        FrapShortcutsProvider;

#define FRAP_TYPE_SHORTCUTS_PROVIDER            (frap_shortcuts_provider_get_type ())
#define FRAP_SHORTCUTS_PROVIDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FRAP_TYPE_SHORTCUTS_PROVIDER, FrapShortcutsProvider))
#define FRAP_SHORTCUTS_PROVIDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FRAP_TYPE_SHORTCUTS_PROVIDER, FrapShortcutsProviderClass))
#define FRAP_IS_SHORTCUTS_PROVIDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FRAP_TYPE_SHORTCUTS_PROVIDER))
#define FRAP_IS_SHORTCUTS_PROVIDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FRAP_TYPE_SHORTCUTS_PROVIDER)
#define FRAP_SHORTCUTS_PROVIDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FRAP_TYPE_SHORTCUTS_PROVIDER, FrapShortcutsProviderClass))

GType frap_shortcuts_provider_get_type (void) G_GNUC_CONST;

FrapShortcutsProvider  *frap_shortcuts_provider_new               (const gchar           *name) G_GNUC_MALLOC;
GList                  *frap_shortcuts_provider_get_providers     (void) G_GNUC_MALLOC;
void                    frap_shortcuts_provider_free_providers    (GList                 *providers);
const gchar            *frap_shortcuts_provider_get_name          (FrapShortcutsProvider *provider);
gboolean                frap_shortcuts_provider_is_custom         (FrapShortcutsProvider *provider);
void                    frap_shortcuts_provider_reset_to_defaults (FrapShortcutsProvider *provider);
void                    frap_shortcuts_provider_clone_defaults    (FrapShortcutsProvider *provider);
GList                  *frap_shortcuts_provider_get_shortcuts     (FrapShortcutsProvider *provider);
FrapShortcut           *frap_shortcuts_provider_get_shortcut      (FrapShortcutsProvider *provider,
                                                                   const gchar           *shortcut);
gboolean                frap_shortcuts_provider_has_shortcut      (FrapShortcutsProvider *provider,
                                                                   const gchar           *shortcut);
void                    frap_shortcuts_provider_set_shortcut      (FrapShortcutsProvider *provider,
                                                                   const gchar           *shortcut,
                                                                   const gchar           *command);
void                    frap_shortcuts_provider_reset_shortcut    (FrapShortcutsProvider *provider,
                                                                   const gchar           *shortcut);

void                    frap_shortcuts_free_shortcuts             (GList                 *shortcuts);
void                    frap_shortcuts_free_shortcut              (FrapShortcut          *shortcut);



struct _FrapShortcutsProviderClass
{
  GObjectClass __parent__;
};

struct _FrapShortcutsProvider
{
  GObject __parent__;

  FrapShortcutsProviderPrivate *priv;
};

struct _FrapShortcut
{
  gchar *property_name;
  gchar *shortcut;
  gchar *command;
};

G_END_DECLS;

#endif /* !__FRAP_SHORTCUTS_PROVIDER_H__ */
