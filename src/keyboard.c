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
        xfwm4    - (c) 2002-2003 Olivier Fourdan
 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <X11/Xlib.h>
#include <libxfce4util/debug.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "keyboard.h"

unsigned int KeyMask;
unsigned int ButtonMask;
unsigned int ButtonKeyMask;
unsigned int AltMask;
unsigned int MetaMask;
unsigned int NumLockMask;
unsigned int ScrollLockMask;
unsigned int CapsLockMask;
unsigned int SuperMask;
unsigned int HyperMask;

void parseKeyString(Display * dpy, MyKey * key, char *str)
{
    char *k;

    DBG("entering parseKeyString\n");
    DBG("key string=%s\n", str);

    key->keycode = 0;
    key->modifier = 0;

    if(!g_ascii_strcasecmp((gchar *) str, "none"))
        return;

    k = strrchr(str, '+');
    if(k)
    {
        gchar *tmp;

        tmp = g_ascii_strdown(str, -1);

        key->keycode = XKeysymToKeycode(dpy, XStringToKeysym(k + 1));
        if(strstr(tmp, "shift"))
        {
            key->modifier = key->modifier | ShiftMask;
        }
        if(strstr(tmp, "control"))
        {
            key->modifier = key->modifier | ControlMask;
        }
        if(strstr(tmp, "alt") || strstr(tmp, "mod1"))
        {
            key->modifier = key->modifier | AltMask;
        }
        if(strstr(tmp, "meta") || strstr(tmp, "mod2"))
        {
            key->modifier = key->modifier | MetaMask;
        }
        if(strstr(tmp, "hyper"))
        {
            key->modifier = key->modifier | HyperMask;
        }
        if(strstr(tmp, "super"))
        {
            key->modifier = key->modifier | SuperMask;
        }

        g_free(tmp);
    }
}

void grabKey(Display * dpy, MyKey * key, Window w)
{
    DBG("entering grabKey\n");

    if(key->keycode)
    {
        if((key->modifier == AnyModifier) || (key->modifier == 0))
        {
            XGrabKey(dpy, key->keycode, key->modifier, w, False, GrabModeAsync, GrabModeAsync);
        }
        else
        {
            /* Here we grab all combinations of well known modifiers */
            XGrabKey(dpy, key->keycode, key->modifier, w, False, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, key->keycode, key->modifier | ScrollLockMask, w, False, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, key->keycode, key->modifier | NumLockMask, w, False, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, key->keycode, key->modifier | CapsLockMask, w, False, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, key->keycode, key->modifier | ScrollLockMask | NumLockMask, w, False, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, key->keycode, key->modifier | ScrollLockMask | CapsLockMask, w, False, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, key->keycode, key->modifier | CapsLockMask | NumLockMask, w, False, GrabModeAsync, GrabModeAsync);
            XGrabKey(dpy, key->keycode, key->modifier | ScrollLockMask | CapsLockMask | NumLockMask, w, False, GrabModeAsync, GrabModeAsync);
        }
    }
}

void ungrabKeys(Display * dpy, Window w)
{
    DBG("entering ungrabKeys\n");

    XUngrabKey(dpy, AnyKey, AnyModifier, w);
}

void initModifiers(Display * dpy)
{
    XModifierKeymap *xmk = XGetModifierMapping(dpy);
    int m, k;

    AltMask = MetaMask = NumLockMask = ScrollLockMask = CapsLockMask = SuperMask = HyperMask = 0;
    if(xmk)
    {
        KeyCode *c = xmk->modifiermap;
        KeyCode numLockKeyCode;
        KeyCode scrollLockKeyCode;
        KeyCode capsLockKeyCode;
        KeyCode altKeyCode;
        KeyCode metaKeyCode;
        KeyCode superKeyCode;
        KeyCode hyperKeyCode;

        numLockKeyCode = XKeysymToKeycode(dpy, XK_Num_Lock);
        scrollLockKeyCode = XKeysymToKeycode(dpy, XK_Scroll_Lock);
        capsLockKeyCode = XKeysymToKeycode(dpy, XK_Caps_Lock);
        altKeyCode = XKeysymToKeycode(dpy, XK_Alt_L);
        metaKeyCode = XKeysymToKeycode(dpy, XK_Meta_L);
        superKeyCode = XKeysymToKeycode(dpy, XK_Super_L);
        hyperKeyCode = XKeysymToKeycode(dpy, XK_Hyper_L);

        if(!altKeyCode)
        {
            altKeyCode = XKeysymToKeycode(dpy, XK_Alt_R);
        }
        if(!metaKeyCode)
        {
            metaKeyCode = XKeysymToKeycode(dpy, XK_Meta_R);
        }
        if(!superKeyCode)
        {
            superKeyCode = XKeysymToKeycode(dpy, XK_Super_R);
        }
        if(!hyperKeyCode)
        {
            hyperKeyCode = XKeysymToKeycode(dpy, XK_Hyper_R);
        }


        for(m = 0; m < 8; m++)
        {
            for(k = 0; k < xmk->max_keypermod; k++, c++)
            {
                if(*c == NoSymbol)
                {
                    continue;
                }
                if(*c == numLockKeyCode)
                {
                    NumLockMask = (1 << m);
                }
                if(*c == scrollLockKeyCode)
                {
                    ScrollLockMask = (1 << m);
                }
                if(*c == capsLockKeyCode)
                {
                    CapsLockMask = (1 << m);
                }
                if(*c == altKeyCode)
                {
                    AltMask = (1 << m);
                }
                if(*c == metaKeyCode)
                {
                    MetaMask = (1 << m);
                }
                if(*c == superKeyCode)
                {
                    SuperMask = (1 << m);
                }
                if(*c == hyperKeyCode)
                {
                    HyperMask = (1 << m);
                }
            }
        }
        XFreeModifiermap(xmk);
    }
    if(MetaMask == AltMask)
    {
        MetaMask = 0;
    }
    if((AltMask != 0) && (MetaMask == Mod1Mask))
    {
        MetaMask = AltMask;
        AltMask = Mod1Mask;
    }

    if((AltMask == 0) && (MetaMask != 0))
    {
        if(MetaMask != Mod1Mask)
        {
            AltMask = Mod1Mask;
        }
        else
        {
            AltMask = MetaMask;
            MetaMask = 0;
        }
    }

    if(AltMask == 0)
    {
        AltMask = Mod1Mask;
    }
    KeyMask = ControlMask | ShiftMask | AltMask | MetaMask | SuperMask | HyperMask;

    ButtonMask = Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask;

    ButtonKeyMask = KeyMask | ButtonMask;
}
