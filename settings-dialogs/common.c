/* vi:set sw=2 sts=2 ts=2 et ai tw=100: */
/*-
 * Copyright (c) 2017 Viktor Odintsev <zakhams@gmail.com>
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
 * Foundation, Inc., Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <gtk/gtk.h>



void
xfwm_widget_reparent (GtkWidget *widget,
                      GtkWidget *new_parent)
{
  GtkWidget *old_parent;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_CONTAINER (new_parent));

  old_parent = gtk_widget_get_parent (widget);

  g_return_if_fail (old_parent != NULL);

  if (old_parent != new_parent)
    {
      g_object_ref (widget);
      gtk_container_remove (GTK_CONTAINER (old_parent), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      g_object_unref (widget);
    }
}

