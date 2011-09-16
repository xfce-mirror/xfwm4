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

/* icons */
#include "inline-stock-maximize.h"
#include "inline-stock-unmaximize.h"
#include "inline-stock-rollup.h"
#include "inline-stock-rolldown.h"
#include "inline-stock-minimize.h"

#ifndef INC_STOCK_ICONS_H
#define INC_STOCK_ICONS_H

#define WM_STOCK_MAXIMIZE   "wm-maximize"
#define WM_STOCK_UNMAXIMIZE "wm-unmaximize"
#define WM_STOCK_MINIMIZE   "wm-minimize"
#define WM_STOCK_ROLLUP     "wm-rollup"
#define WM_STOCK_ROLLDOWN   "wm-rolldown"

void                     initWMStockIcons                        (void);

#endif /* INC_STOCK_ICONS_H */
