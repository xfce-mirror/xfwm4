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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "keyboard.h"
#include "debug.h"

void parseKeyString(Display * dpy, MyKey * key, char *str)
{
    char *k;

    DBG("entering parseKeyString\n");
    DBG("key string=%s\n", str);

    key->keycode = 0;
    key->modifier = 0;

    if(!g_strcasecmp((gchar *) str, "none"))
        return;

    k = strrchr(str, '+');
    if(k)
    {
        key->keycode = XKeysymToKeycode(dpy, XStringToKeysym(k + 1));
        if(strstr(str, "Shift"))
            key->modifier = key->modifier | ShiftMask;
        if(strstr(str, "Control"))
            key->modifier = key->modifier | ControlMask;
        if(strstr(str, "Mod1"))
            key->modifier = key->modifier | Mod1Mask;
        if(strstr(str, "Mod2"))
            key->modifier = key->modifier | Mod2Mask;
        if(strstr(str, "Mod3"))
            key->modifier = key->modifier | Mod3Mask;
        if(strstr(str, "Mod4"))
            key->modifier = key->modifier | Mod4Mask;
        if(strstr(str, "Mod5"))
            key->modifier = key->modifier | Mod5Mask;
    }
}

void grabKey(Display * dpy, MyKey * key, Window w)
{
    DBG("entering grabKey\n");

    if(key->keycode)
    {
        XGrabKey(dpy, key->keycode, key->modifier, w, False, GrabModeAsync, GrabModeAsync);
        XGrabKey(dpy, key->keycode, key->modifier | LockMask, w, False, GrabModeAsync, GrabModeAsync);
    }
}

void ungrabKeys(Display * dpy, Window w)
{
    DBG("entering ungrabKeys\n");

    XUngrabKey(dpy, AnyKey, AnyModifier, w);
}
