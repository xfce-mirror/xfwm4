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
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
        oroborus - (c) 2001 Ken Lynch
        xfwm4    - (c) 2002-2004 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libxfce4util/libxfce4util.h> 
#include "keyboard.h"

unsigned int KeyMask;
unsigned int ButtonMask;
unsigned int ButtonKeyMask;
unsigned int AltMask;
unsigned int MetaMask;
unsigned int NumLockMask;
unsigned int ScrollLockMask;
unsigned int SuperMask;
unsigned int HyperMask;

void
parseKeyString (Display * dpy, MyKey * key, char *str)
{
    char *k;

    g_return_if_fail (key != NULL);

    TRACE ("entering parseKeyString");
    TRACE ("key string=%s", str);

    key->keycode = 0;
    key->modifier = 0;

    g_return_if_fail (str != NULL);

    if (!g_ascii_strcasecmp (str, "none"))
    {
        return;
    }

    k = strrchr (str, '+');
    if (k)
    {
        /* There is a modifier */
        gchar *tmp;

        tmp = g_ascii_strdown ((gchar *) str, -1);

        key->keycode = XKeysymToKeycode (dpy, XStringToKeysym (++k));
        if (strstr (tmp, "shift"))
        {
            key->modifier = key->modifier | ShiftMask;
        }
        if (strstr (tmp, "control"))
        {
            key->modifier = key->modifier | ControlMask;
        }
        if (strstr (tmp, "alt"))
        {
            key->modifier = key->modifier | AltMask;
        }
        if (strstr (tmp, "meta"))
        {
            key->modifier = key->modifier | MetaMask;
        }
        if (strstr (tmp, "super"))
        {
            key->modifier = key->modifier | SuperMask;
        }
        if (strstr (tmp, "hyper"))
        {
            key->modifier = key->modifier | HyperMask;
        }
        if (strstr (tmp, "mod1"))
        {
            key->modifier = key->modifier | Mod1Mask;
        }
        if (strstr (tmp, "mod2"))
        {
            key->modifier = key->modifier | Mod2Mask;
        }
        if (strstr (tmp, "mod3"))
        {
            key->modifier = key->modifier | Mod3Mask;
        }
        if (strstr (tmp, "mod4"))
        {
            key->modifier = key->modifier | Mod4Mask;
        }
        if (strstr (tmp, "mod5"))
        {
            key->modifier = key->modifier | Mod5Mask;
        }
        g_free (tmp);
    }
    else
    {
        key->keycode = XKeysymToKeycode (dpy, XStringToKeysym (str));
        key->modifier = 0;
    }
}

void
grabKey (Display * dpy, MyKey * key, Window w)
{
    TRACE ("entering grabKey");

    if (key->keycode)
    {
        if (key->modifier == 0)
        {
            XGrabKey (dpy, key->keycode, AnyModifier, w, FALSE,
                GrabModeAsync, GrabModeAsync);
        }
        else
        {
            /* Here we grab all combinations of well known modifiers */
            XGrabKey (dpy, key->keycode, key->modifier, w, FALSE,
                GrabModeAsync, GrabModeAsync);
            XGrabKey (dpy, key->keycode, key->modifier | ScrollLockMask, w,
                FALSE, GrabModeAsync, GrabModeAsync);
            XGrabKey (dpy, key->keycode, key->modifier | NumLockMask, w,
                FALSE, GrabModeAsync, GrabModeAsync);
            XGrabKey (dpy, key->keycode, key->modifier | LockMask, w,
                FALSE, GrabModeAsync, GrabModeAsync);
            XGrabKey (dpy, key->keycode,
                key->modifier | ScrollLockMask | NumLockMask, w, FALSE,
                GrabModeAsync, GrabModeAsync);
            XGrabKey (dpy, key->keycode,
                key->modifier | ScrollLockMask | LockMask, w, FALSE,
                GrabModeAsync, GrabModeAsync);
            XGrabKey (dpy, key->keycode,
                key->modifier | LockMask | NumLockMask, w, FALSE,
                GrabModeAsync, GrabModeAsync);
            XGrabKey (dpy, key->keycode,
                key->modifier | ScrollLockMask | LockMask | NumLockMask,
                w, FALSE, GrabModeAsync, GrabModeAsync);
        }
    }
}

void
ungrabKeys (Display * dpy, Window w)
{
    TRACE ("entering ungrabKeys");

    XUngrabKey (dpy, AnyKey, AnyModifier, w);
}

void
grabButton (Display * dpy, int button, int modifier, Window w)
{
    TRACE ("entering grabButton");

    if (modifier == AnyModifier)
    {
        XGrabButton (dpy, button, AnyModifier, w, FALSE,
            ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync, 
            None, None);
    }
    else
    {
        /* Here we grab all combinations of well known modifiers */
        XGrabButton (dpy, button, modifier, 
            w, FALSE,
            ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync, 
            None, None);
        XGrabButton (dpy, button, modifier | ScrollLockMask, 
            w, FALSE,
            ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync, 
            None, None);
        XGrabButton (dpy, button, modifier | NumLockMask, 
            w, FALSE,
            ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync, 
            None, None);
        XGrabButton (dpy, button, modifier | LockMask, w, FALSE,
            ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync, 
            None, None);
        XGrabButton (dpy, button, modifier | ScrollLockMask | NumLockMask, 
            w, FALSE,
            ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync, 
            None, None);
        XGrabButton (dpy, button, modifier | ScrollLockMask | LockMask, 
            w, FALSE,
            ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync, 
            None, None);
        XGrabButton (dpy, button, modifier | LockMask | NumLockMask, 
            w, FALSE,
            ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync, 
            None, None);
        XGrabButton (dpy, button, 
            modifier | ScrollLockMask | LockMask | NumLockMask, 
            w, FALSE,
            ButtonPressMask|ButtonReleaseMask, GrabModeSync, GrabModeAsync, 
            None, None);
    }
}

void
ungrabButton (Display * dpy, int button, int modifier, Window w)
{
    TRACE ("entering ungrabKeys");

    TRACE ("entering grabButton");

    if (modifier == AnyModifier)
    {
        XUngrabButton (dpy, button, AnyModifier, w);
    }
    else
    {
        /* Here we ungrab all combinations of well known modifiers */
        XUngrabButton (dpy, button, modifier, w);
        XUngrabButton (dpy, button, modifier | ScrollLockMask, w);
        XUngrabButton (dpy, button, modifier | NumLockMask, w);
        XUngrabButton (dpy, button, modifier | LockMask, w);
        XUngrabButton (dpy, button, modifier | ScrollLockMask | NumLockMask, w);
        XUngrabButton (dpy, button, modifier | ScrollLockMask | LockMask, w);
        XUngrabButton (dpy, button, modifier | LockMask | NumLockMask, w);
        XUngrabButton (dpy, button, modifier | ScrollLockMask | LockMask | NumLockMask, w);
    }
}

void
initModifiers (Display * dpy)
{
    XModifierKeymap *modmap;
    KeySym *keymap;
    int i;
    int keysyms_per_keycode;
    int min_keycode;
    int max_keycode;

    AltMask = 0;
    MetaMask = 0;
    NumLockMask = 0;
    ScrollLockMask = 0;
    SuperMask = 0;
    HyperMask = 0;
    keysyms_per_keycode = 0;
    min_keycode = 0;
    max_keycode = 0;

    XDisplayKeycodes (dpy, &min_keycode, &max_keycode);
    modmap = XGetModifierMapping (dpy);
    keymap = XGetKeyboardMapping (dpy, min_keycode, max_keycode - min_keycode + 1, &keysyms_per_keycode);  

    for (i = 3 * modmap->max_keypermod; i < 8 * modmap->max_keypermod; i++)
    {
        unsigned int keycode = modmap->modifiermap[i];

        if ((keycode >= min_keycode) && (keycode <= max_keycode))
        {
            int j;
            KeySym *syms = keymap + (keycode - min_keycode) * keysyms_per_keycode;

            for (j = 0; j < keysyms_per_keycode; j++)
            {
                if (syms[j] == XK_Num_Lock)
                {
                    NumLockMask |= (1 << ( i / modmap->max_keypermod));
                }
                else if (syms[j] == XK_Scroll_Lock)
                {
                    ScrollLockMask |= (1 << ( i / modmap->max_keypermod));
                }
                else if ((syms[j] == XK_Super_L) || (syms[j] == XK_Super_R))
                {
                    SuperMask |= (1 << ( i / modmap->max_keypermod));
                }
                else if ((syms[j] == XK_Hyper_L) || (syms[j] == XK_Hyper_R))
                {
                    HyperMask |= (1 << ( i / modmap->max_keypermod));
                }              
                else if ((syms[j] == XK_Meta_L) || (syms[j] == XK_Meta_R))
                {
                    MetaMask |= (1 << ( i / modmap->max_keypermod));
                }
                else if ((syms[j] == XK_Alt_L) || (syms[j] == XK_Alt_R))
                {
                    AltMask |= (1 << ( i / modmap->max_keypermod));
                }
            }
        }
    }
    KeyMask =
        ControlMask | ShiftMask | AltMask | MetaMask | SuperMask | HyperMask;

    ButtonMask =
        Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask;

    ButtonKeyMask = KeyMask | ButtonMask;
}
