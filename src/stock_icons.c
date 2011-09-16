/*      $Id$

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2, or (at your option)
        any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., Inc., 51 Franklin Street, Fifth Floor, Boston,
        MA 02110-1301, USA.


        metacity - Copyright (C) 2002 Havoc Pennington
        stock icon code Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
        xfwm4    - (c) 2002-2011 Olivier Fourdan

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include "stock_icons.h"

/*
 * Taken from metacity
 * stock icon code Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
 */

typedef struct
{
    char *stock_id;
    const guint8 *icon_data;
}   StockIcon;


void
initWMStockIcons (void)
{
    GtkIconFactory *factory;
    GtkIconSet *icon_set;
    GdkPixbuf *pixbuf;
    gint i;

    StockIcon items[] =
    {
        { WM_STOCK_MAXIMIZE,   stock_maximize_data   },
        { WM_STOCK_UNMAXIMIZE, stock_unmaximize_data },
        { WM_STOCK_MINIMIZE,   stock_minimize_data   },
        { WM_STOCK_ROLLUP,     stock_rollup_data     },
        { WM_STOCK_ROLLDOWN,   stock_rolldown_data   }
    };

    factory = gtk_icon_factory_new ();
    gtk_icon_factory_add_default (factory);

    for (i = 0; i < (gint) G_N_ELEMENTS (items); i++)
    {
        pixbuf = gdk_pixbuf_new_from_inline (-1, items[i].icon_data, FALSE, NULL);

        icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
        gtk_icon_factory_add (factory, items[i].stock_id, icon_set);
        gtk_icon_set_unref (icon_set);

        g_object_unref (G_OBJECT (pixbuf));
    }
    g_object_unref (G_OBJECT (factory));
}
