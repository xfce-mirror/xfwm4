/*
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; You may only use version 2 of the License,
	you have no option to use any other version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002 Olivier Fourdan

 */

#ifndef __PARSERC_H__
#define __PARSERC_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include "settings.h"

typedef struct _Settings Settings;

struct _Settings
{
    gchar *option;
    gchar *value;
    gboolean required;
};

gboolean parseRc(gchar *, gchar *, Settings rc[]);
gboolean checkRc(Settings rc[]);
gchar *getValue(gchar *, Settings rc[]);
void freeRc(Settings rc[]);

#endif /* __PARSERC_H__ */
